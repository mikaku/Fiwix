#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-60}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
ARM_MES=${ARM_MES:?set ARM_MES to the completed ARM mes-m2}
KERNEL=${1:-./fiwix-arm-generic.bin}
INIT=${2:-arch/arm/fixture/mes-init.elf}
MES_SHA256=8aa74fb3cecbcf4bb7bea9f9e7764f4f6549227cf68501da14d6a83302eb068c
mes_root=$(CDPATH='' cd -- "$(dirname "$ARM_MES")/.." && pwd)
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

MKE2FS=$(find_tool mke2fs "$MKE2FS")
DEBUGFS=$(find_tool debugfs "$DEBUGFS")
command -v "$QEMU" >/dev/null
command -v "$SHA256SUM" >/dev/null
test -f "$KERNEL"
test -f "$INIT"
test -f "$ARM_MES"
test -f "$mes_root/mes/module/mes/boot-5.scm"
test -d "$mes_root/module"

actual=$("$SHA256SUM" "$ARM_MES")
actual=${actual%% *}
test "$actual" = "$MES_SHA256" || {
	printf 'unexpected checksum for %s: %s\n' "$ARM_MES" "$actual" >&2
	exit 1
}

rootfs=$temporary/rootfs
disk=$temporary/arm-mes.img
mkdir -p "$rootfs/dev" "$rootfs/sbin" "$rootfs/mes/module" "$rootfs/module"
install -m 755 "$INIT" "$rootfs/sbin/init"
install -m 755 "$ARM_MES" "$rootfs/mes-m2"
cp -R "$mes_root/mes/module/." "$rootfs/mes/module/"
cp -R "$mes_root/module/." "$rootfs/module/"
printf '%s\n' 'Fiwix ARM ext2 writable gate init' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 16384
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$temporary/debugfs.commands"
"$DEBUGFS" -w -f "$temporary/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix ARM virtio sector gate' |
	dd of="$disk" conv=notrunc status=none
"$(dirname "$0")/arm-ext2-check.sh" "$disk"

run_mes()
{
	transport=$1
	shift
	run_disk=$temporary/arm-mes-$transport.img
	log=$temporary/arm-mes-$transport.log
	cp "$disk" "$run_disk"
	if ! timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m 256M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" >"$log" 2>&1; then
		cat "$log" >&2
		exit 1
	fi
	if ! grep -q '^Fiwix ARM Mes process tree entered' "$log" ||
		! grep -q '^Fiwix ARM Mes Scheme boundary passed' "$log" ||
		! grep -q '^Fiwix ARM Mes process tree completed' "$log" ||
		grep -Eiq 'undefined|unhandled|panic|failed|returned' "$log"; then
		cat "$log" >&2
		exit 1
	fi
	"$(dirname "$0")/arm-ext2-check.sh" "$run_disk"
}

run_mes legacy
run_mes modern -global virtio-mmio.force-legacy=false

printf '%s\n' 'Fiwix ARM Mes Scheme boot passed'
