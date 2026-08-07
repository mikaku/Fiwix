#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-120}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
ARM_TCC=${ARM_TCC:?set ARM_TCC to the completed ARM tcc-mes}
LINUX_IMAGE=${LINUX_IMAGE:?set LINUX_IMAGE to the ARM Linux zImage}
KERNEL=${1:-./fiwix-arm-generic.bin}
INIT=${2:-arch/arm/fixture/linux-tcc-init.elf}
TCC_SHA256=0caa6ca807e45ac14f432487f6f31e9282a0b7b2bf72e93f980600361d769ced
TCC_TREE_SHA256=a99e237bd52a171202c536740a1a5492a1caa0201019158ca391509db593f7e1
SOURCE_SHA256=b96609b7b81e8cf360e0a2c50d433b0c53a964d199069d974cdd4828782b852d
ELF_SHA256=11524da4977a66afe344c7697078bb0d64c08a9267770efc9eb7bfc7a315d451
ELF_BYTES=48348
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
tcc_prefix=$(CDPATH='' cd -- "$(dirname "$ARM_TCC")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

find_tool()
{
	name=$1
	explicit=$2
	if test -n "$explicit"; then
		printf '%s\n' "$explicit"
		return
	fi
	if command -v "$name" >/dev/null 2>&1; then
		command -v "$name"
		return
	fi
	for candidate in "/usr/sbin/$name" "/sbin/$name"; do
		if test -x "$candidate"; then
			printf '%s\n' "$candidate"
			return
		fi
	done
	return 1
}

tree_hash()
{
	tree_root=$1
	shift
	tar -C "$tree_root" --sort=name --mtime='@0' \
		--owner=0 --group=0 --numeric-owner --format=gnu \
		-cf - "$@" |
		"$SHA256SUM" | awk '{print $1}'
}

check_hash()
{
	file=$1
	expected=$2
	actual=$("$SHA256SUM" "$file")
	actual=${actual%% *}
	test "$actual" = "$expected" || {
		printf 'unexpected checksum for %s: %s\n' "$file" "$actual" >&2
		exit 1
	}
}

MKE2FS=$(find_tool mke2fs "$MKE2FS")
DEBUGFS=$(find_tool debugfs "$DEBUGFS")
command -v "$QEMU" >/dev/null
command -v "$SHA256SUM" >/dev/null
test -f "$KERNEL"
test -f "$INIT"
test -f "$LINUX_IMAGE"
test -f "$ARM_TCC"
test -f "$tcc_prefix/include/mes/stdio.h"
test -f "$tcc_prefix/lib/mes/crt1.o"
test -f "$tcc_prefix/lib/mes/libc.a"
test -f "$tcc_prefix/lib/mes/tcc/libtcc1.a"

check_hash "$ARM_TCC" "$TCC_SHA256"
check_hash "$root/tests/fixtures/arm-tcc-hello.c" "$SOURCE_SHA256"
test "$(tree_hash "$tcc_prefix" include/mes lib/mes)" = "$TCC_TREE_SHA256"

rootfs=$temporary/rootfs
disk=$temporary/arm-linux-tcc.img
mkdir -p "$rootfs/bin" "$rootfs/dev" "$rootfs/mes" "$rootfs/sbin"
install -m 755 "$INIT" "$rootfs/sbin/init"
install -m 755 "$ARM_TCC" "$rootfs/bin/tcc-mes"
install -m 644 "$LINUX_IMAGE" "$rootfs/linux"
install -m 644 "$root/tests/fixtures/arm-tcc-hello.c" "$rootfs/hello.c"
cp -R "$tcc_prefix/include/mes" "$rootfs/mes/include"
cp -R "$tcc_prefix/lib/mes" "$rootfs/mes/lib"
printf '%s\n' 'Fiwix ARM ext2 writable gate init' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 32768
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$temporary/debugfs.commands"
"$DEBUGFS" -w -f "$temporary/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix ARM virtio sector gate' |
	dd of="$disk" conv=notrunc status=none
"$root/tests/arm-ext2-check.sh" "$disk"

run_tcc()
{
	transport=$1
	shift
	run_disk=$temporary/arm-linux-tcc-$transport.img
	log=$temporary/arm-linux-tcc-$transport.log
	output=$temporary/arm-linux-tcc-$transport
	cp "$disk" "$run_disk"
	if ! timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m 256M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		-append 'earlycon console=ttyAMA0 root=/dev/vda rw rootfstype=ext2 init=/sbin/init' \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" >"$log" 2>&1; then
		cat "$log" >&2
		exit 1
	fi
	if ! grep -q '^Fiwix ARM Linux ext2 root handoff' "$log" ||
		! grep -q '^Linux version ' "$log" ||
		! grep -q 'VFS: Mounted root (ext2 filesystem)' "$log" ||
		! grep -q '^Fiwix ARM Linux TinyCC process tree entered' "$log" ||
		! grep -q '^Fiwix ARM Linux TinyCC linked ELF entered' "$log" ||
		! grep -q '^Fiwix ARM TinyCC boundary' "$log" ||
		! grep -q '^Fiwix ARM Linux TinyCC process tree completed' "$log" ||
		grep -Eiq 'undefined|unhandled|Kernel panic|process tree failed|exec returned' \
			"$log"; then
		cat "$log" >&2
		exit 1
	fi
	"$DEBUGFS" -R 'cat /hello' "$run_disk" >"$output" 2>/dev/null
	test "$(wc -c < "$output")" -eq "$ELF_BYTES"
	check_hash "$output" "$ELF_SHA256"
	"$root/tests/arm-ext2-check.sh" "$run_disk"
}

run_tcc legacy
run_tcc modern -global virtio-mmio.force-legacy=false

printf '%s\n' 'Fiwix ARM Linux TinyCC compile/link/run boot passed'
