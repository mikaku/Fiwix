#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-300}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
ARM_TCC=${ARM_TCC:?set ARM_TCC to the completed ARM tcc-mes}
ARM_TCC_SOURCE=${ARM_TCC_SOURCE:?set ARM_TCC_SOURCE to the pinned TinyCC source tree}
KERNEL=${1:-./fiwix-arm-generic.bin}
INIT=${2:-arch/arm/fixture/tcc-selfhost-init.elf}
TCC_SHA256=0caa6ca807e45ac14f432487f6f31e9282a0b7b2bf72e93f980600361d769ced
TCC_TREE_SHA256=a99e237bd52a171202c536740a1a5492a1caa0201019158ca391509db593f7e1
BOOT0_SHA256=d9aaa2f6cb4626db9476ebc01995ad610315c1d04765952dae5c640b3a877ce4
BOOT0_BYTES=313964
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
tcc_prefix=$(CDPATH='' cd -- "$(dirname "$ARM_TCC")/.." && pwd)
manifest=$root/tests/fixtures/arm-tcc-0.9.26-sources.SHA256SUM
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
test -f "$ARM_TCC"
test -f "$manifest"
test -f "$tcc_prefix/include/mes/stdio.h"
test -f "$tcc_prefix/lib/mes/crt1.o"
test -f "$tcc_prefix/lib/mes/libc.a"
test -f "$tcc_prefix/lib/mes/tcc/libtcc1.a"

check_hash "$ARM_TCC" "$TCC_SHA256"
test "$(tree_hash "$tcc_prefix" include/mes lib/mes)" = "$TCC_TREE_SHA256"
(CDPATH='' cd -- "$ARM_TCC_SOURCE" && "$SHA256SUM" -c "$manifest")

rootfs=$temporary/rootfs
disk=$temporary/arm-tcc-selfhost.img
mkdir -p "$rootfs/bin" "$rootfs/dev" "$rootfs/mes" "$rootfs/sbin" \
	"$rootfs/tcc"
install -m 755 "$INIT" "$rootfs/sbin/init"
install -m 755 "$ARM_TCC" "$rootfs/bin/tcc-mes"
cp -R "$tcc_prefix/include/mes" "$rootfs/mes/include"
cp -R "$tcc_prefix/lib/mes" "$rootfs/mes/lib"
while read -r _ source_file; do
	install -m 644 "$ARM_TCC_SOURCE/$source_file" \
		"$rootfs/tcc/$source_file"
done < "$manifest"
printf '%s\n' 'Fiwix ARM ext2 writable gate init' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 32768
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$temporary/debugfs.commands"
"$DEBUGFS" -w -f "$temporary/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix ARM virtio sector gate' |
	dd of="$disk" conv=notrunc status=none
"$root/tests/arm-ext2-check.sh" "$disk"

run_selfhost()
{
	transport=$1
	shift
	run_disk=$temporary/arm-tcc-selfhost-$transport.img
	log=$temporary/arm-tcc-selfhost-$transport.log
	output=$temporary/tcc-boot0-$transport
	cp "$disk" "$run_disk"
	if ! timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m 256M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" >"$log" 2>&1; then
		cat "$log" >&2
		exit 1
	fi
	if ! grep -q '^Fiwix ARM TinyCC self-host process tree entered' "$log" ||
		! grep -q '^tcc version 0.9.26 (ARM Linux)' "$log" ||
		! grep -q '^Fiwix ARM TinyCC self-host process tree completed' "$log" ||
		grep -Eiq 'undefined|unhandled|panic|failed|returned' "$log"; then
		cat "$log" >&2
		exit 1
	fi
	"$DEBUGFS" -R 'cat /tcc/tcc-boot0' "$run_disk" >"$output" 2>/dev/null
	test "$(wc -c < "$output")" -eq "$BOOT0_BYTES"
	check_hash "$output" "$BOOT0_SHA256"
	"$root/tests/arm-ext2-check.sh" "$run_disk"
}

run_selfhost legacy
run_selfhost modern -global virtio-mmio.force-legacy=false

printf '%s\n' 'Fiwix ARM TinyCC self-host boot passed'
