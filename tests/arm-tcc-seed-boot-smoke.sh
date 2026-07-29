#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-14400}
QEMU_MEMORY=${QEMU_MEMORY:-1G}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
HANDOFF_LINUX=${HANDOFF_LINUX:-0}
ARM_MES=${ARM_MES:?set ARM_MES to the completed ARM mes-m2}
ARM_NYACC=${ARM_NYACC:?set ARM_NYACC to the pinned NYACC source tree}
ARM_M1=${ARM_M1:?set ARM_M1 to the completed ARM M1}
ARM_HEX2=${ARM_HEX2:?set ARM_HEX2 to the completed ARM hex2}
ARM_TCC_SOURCE=${ARM_TCC_SOURCE:?set ARM_TCC_SOURCE to the pinned TinyCC source tree}
LINUX_IMAGE=${LINUX_IMAGE:-}
KERNEL=${1:-./fiwix-arm-generic.bin}
INIT=${2:-arch/arm/fixture/tcc-seed-init.elf}
MES_SHA256=8aa74fb3cecbcf4bb7bea9f9e7764f4f6549227cf68501da14d6a83302eb068c
M1_SHA256=b0f0e941e1c1a268ee6dd208b5519d79511b1c8f19ba4a096795de2492716309
HEX2_SHA256=c0c0580cf21ceb49cce565b694cc81d1be1df861b0f0e9e5d2763189afdfa1d8
MESCC_TREE_SHA256=e59166c175a898d9d1d19d00fbfe6e85a53db2a91038a3119b57d3c7b2e67344
NYACC_TREE_SHA256=b2e0d321a7349ee3b7e708c962fd4b26821b8bc784eb307cfb000218034634d5
TCC_SEED_SHA256=${TCC_SEED_SHA256:-}
TCC_SEED_BYTES=${TCC_SEED_BYTES:-}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
mes_root=$(CDPATH='' cd -- "$(dirname "$ARM_MES")/.." && pwd)
nyacc_root=$(CDPATH='' cd -- "$ARM_NYACC" && pwd)
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

case "$HANDOFF_LINUX" in
	0)
		start_marker='Fiwix ARM MesCC TinyCC seed rebuild entered'
		compile_marker='Fiwix ARM MesCC TinyCC source compile completed'
		complete_marker='Fiwix ARM MesCC TinyCC seed rebuild completed'
		;;
	1)
		start_marker='Fiwix ARM Linux MesCC TinyCC seed rebuild entered'
		compile_marker='Fiwix ARM Linux MesCC TinyCC source compile completed'
		complete_marker='Fiwix ARM Linux MesCC TinyCC seed rebuild completed'
		test -f "$LINUX_IMAGE"
		;;
	*)
		echo "HANDOFF_LINUX must be 0 or 1" >&2
		exit 2
		;;
esac

MKE2FS=$(find_tool mke2fs "$MKE2FS")
DEBUGFS=$(find_tool debugfs "$DEBUGFS")
command -v "$QEMU" >/dev/null
command -v "$SHA256SUM" >/dev/null
test -f "$KERNEL"
test -f "$INIT"
test -f "$ARM_MES"
test -f "$ARM_M1"
test -f "$ARM_HEX2"
test -f "$mes_root/mescc.scm"
test -f "$nyacc_root/module/nyacc/lang/c99/parser.scm"
test -f "$manifest"

check_hash "$ARM_MES" "$MES_SHA256"
check_hash "$ARM_M1" "$M1_SHA256"
check_hash "$ARM_HEX2" "$HEX2_SHA256"
test "$(tree_hash "$mes_root" include lib mes/module module mescc.scm)" = \
	"$MESCC_TREE_SHA256"
test "$(tree_hash "$nyacc_root" module)" = "$NYACC_TREE_SHA256"
(CDPATH='' cd -- "$ARM_TCC_SOURCE" && "$SHA256SUM" -c "$manifest")

rootfs=$temporary/rootfs
disk=$temporary/arm-tcc-seed.img
log=$temporary/arm-tcc-seed.log
output=$temporary/tcc-mes
mkdir -p "$rootfs/bin" "$rootfs/dev" "$rootfs/mes/bin" \
	"$rootfs/mes/mes/module" "$rootfs/mes/module" "$rootfs/nyacc/module" \
	"$rootfs/sbin" "$rootfs/tcc"
install -m 755 "$INIT" "$rootfs/sbin/init"
install -m 755 "$ARM_M1" "$rootfs/bin/M1"
install -m 755 "$ARM_HEX2" "$rootfs/bin/hex2"
install -m 755 "$ARM_MES" "$rootfs/mes/bin/mes-m2"
install -m 644 "$mes_root/mescc.scm" "$rootfs/mes/mescc.scm"
cp -R "$mes_root/include" "$rootfs/mes/"
cp -R "$mes_root/lib" "$rootfs/mes/"
cp -R "$mes_root/mes/module/." "$rootfs/mes/mes/module/"
cp -R "$mes_root/module/." "$rootfs/mes/module/"
cp -R "$nyacc_root/module/." "$rootfs/nyacc/module/"
while read -r _ source_file; do
	install -m 644 "$ARM_TCC_SOURCE/$source_file" \
		"$rootfs/tcc/$source_file"
done < "$manifest"
if test "$HANDOFF_LINUX" -eq 1; then
	install -m 644 "$LINUX_IMAGE" "$rootfs/linux"
fi
printf '%s\n' 'Fiwix ARM ext2 writable gate init' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 65536
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$temporary/debugfs.commands"
"$DEBUGFS" -w -f "$temporary/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix ARM virtio sector gate' |
	dd of="$disk" conv=notrunc status=none
"$root/tests/arm-ext2-check.sh" "$disk"

set --
if test "$HANDOFF_LINUX" -eq 1; then
	set -- -append \
		'earlycon console=ttyAMA0 root=/dev/vda rw rootfstype=ext2 init=/sbin/init'
fi
if ! timeout "$TIMEOUT" "$QEMU" \
	-M virt,virtualization=on,secure=off -cpu cortex-a15 \
	-m "$QEMU_MEMORY" -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
	-drive file="$disk",format=raw,if=none,id=drive0 \
	-device virtio-blk-device,drive=drive0 \
	-global virtio-mmio.force-legacy=false "$@" >"$log" 2>&1; then
	cat "$log" >&2
	exit 1
fi
if ! grep -q "^$start_marker\$" "$log" ||
	! grep -q "^$compile_marker\$" "$log" ||
	! grep -q '^tcc version 0.9.26 (ARM Linux)' "$log" ||
	! grep -q "^$complete_marker\$" "$log" ||
	grep -Eiq 'undefined|unhandled|panic|failed|exec returned|out of memory' \
		"$log"; then
	cat "$log" >&2
	exit 1
fi
"$DEBUGFS" -R 'cat /tcc-mes' "$disk" >"$output" 2>/dev/null
actual_bytes=$(wc -c < "$output")
actual_hash=$("$SHA256SUM" "$output")
actual_hash=${actual_hash%% *}
if test -n "$TCC_SEED_BYTES"; then
	test "$actual_bytes" -eq "$TCC_SEED_BYTES"
fi
if test -n "$TCC_SEED_SHA256"; then
	test "$actual_hash" = "$TCC_SEED_SHA256"
fi
"$root/tests/arm-ext2-check.sh" "$disk"

printf 'Fiwix ARM MesCC TinyCC seed artifact: %s bytes, SHA-256 %s\n' \
	"$actual_bytes" "$actual_hash"
if test "$HANDOFF_LINUX" -eq 1; then
	printf '%s\n' 'Fiwix ARM Linux cumulative MesCC TinyCC seed gate passed'
else
	printf '%s\n' 'Fiwix ARM 770 MiB MesCC TinyCC seed rebuild passed'
fi
