#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-1800}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
ARM_MES=${ARM_MES:?set ARM_MES to the completed ARM mes-m2}
ARM_NYACC=${ARM_NYACC:?set ARM_NYACC to the pinned NYACC source tree}
KERNEL=${1:-./fiwix-arm-generic.bin}
INIT=${2:-arch/arm/fixture/mescc-init.elf}
MES_SHA256=8aa74fb3cecbcf4bb7bea9f9e7764f4f6549227cf68501da14d6a83302eb068c
MESCC_TREE_SHA256=e59166c175a898d9d1d19d00fbfe6e85a53db2a91038a3119b57d3c7b2e67344
NYACC_TREE_SHA256=b2e0d321a7349ee3b7e708c962fd4b26821b8bc784eb307cfb000218034634d5
SOURCE_SHA256=55b241b486f18e4723544a510b07e5054b73e4e7a14e135ca5cc6436df7de835
EXPECTED_SHA256=4186c398787fecd6d436a0fafdfee8a53e0aa9136fdab515395a5e107799c90e
EXPECTED_BYTES=290
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
mes_root=$(CDPATH='' cd -- "$(dirname "$ARM_MES")/.." && pwd)
nyacc_root=$(CDPATH='' cd -- "$ARM_NYACC" && pwd)
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
test -f "$ARM_MES"
test -f "$mes_root/mescc.scm"
test -f "$nyacc_root/module/nyacc/lang/c99/parser.scm"

check_hash "$ARM_MES" "$MES_SHA256"
check_hash "$root/tests/fixtures/arm-mescc-hello.c" "$SOURCE_SHA256"
test "$(tree_hash "$mes_root" include lib mes/module module mescc.scm)" = \
	"$MESCC_TREE_SHA256"
test "$(tree_hash "$nyacc_root" module)" = "$NYACC_TREE_SHA256"

rootfs=$temporary/rootfs
disk=$temporary/arm-mescc.img
mkdir -p "$rootfs/dev" "$rootfs/sbin" "$rootfs/mes/bin" \
	"$rootfs/mes/mes/module" "$rootfs/mes/module" "$rootfs/nyacc/module"
install -m 755 "$INIT" "$rootfs/sbin/init"
install -m 755 "$ARM_MES" "$rootfs/mes/bin/mes-m2"
install -m 644 "$mes_root/mescc.scm" "$rootfs/mes/mescc.scm"
install -m 644 "$root/tests/fixtures/arm-mescc-hello.c" "$rootfs/hello.c"
cp -R "$mes_root/include" "$rootfs/mes/"
cp -R "$mes_root/lib" "$rootfs/mes/"
cp -R "$mes_root/mes/module/." "$rootfs/mes/mes/module/"
cp -R "$mes_root/module/." "$rootfs/mes/module/"
cp -R "$nyacc_root/module/." "$rootfs/nyacc/module/"
printf '%s\n' 'Fiwix ARM ext2 writable gate init' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 32768
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$temporary/debugfs.commands"
"$DEBUGFS" -w -f "$temporary/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix ARM virtio sector gate' |
	dd of="$disk" conv=notrunc status=none
"$root/tests/arm-ext2-check.sh" "$disk"

run_mescc()
{
	transport=$1
	shift
	run_disk=$temporary/arm-mescc-$transport.img
	log=$temporary/arm-mescc-$transport.log
	output=$temporary/arm-mescc-$transport.s
	cp "$disk" "$run_disk"
	if ! timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m 256M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" >"$log" 2>&1; then
		cat "$log" >&2
		exit 1
	fi
	if ! grep -q '^Fiwix ARM MesCC process tree entered' "$log" ||
		! grep -q '^Fiwix ARM MesCC process tree completed' "$log" ||
		grep -Eiq 'undefined|unhandled|panic|failed|returned' "$log"; then
		cat "$log" >&2
		exit 1
	fi
	"$DEBUGFS" -R 'cat /hello.s' "$run_disk" >"$output" 2>/dev/null
	test "$(wc -c < "$output")" -eq "$EXPECTED_BYTES"
	check_hash "$output" "$EXPECTED_SHA256"
	"$root/tests/arm-ext2-check.sh" "$run_disk"
}

run_mescc legacy
run_mescc modern -global virtio-mmio.force-legacy=false

printf '%s\n' 'Fiwix ARM MesCC compile boot passed'
