#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

KERNEL=${1:-./fiwix-arm-generic.bin}
DISK=${2:-arch/arm/fixture/linux-root-disk.img}
QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-60}

tests/arm-ext2-check.sh "$DISK"

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

fail_log()
{
	cat "$log" >&2
	exit 1
}

for transport in legacy modern; do
	log=$temporary/arm-linux-$transport.log
	run_disk=$temporary/arm-linux-$transport.img
	cp "$DISK" "$run_disk"
	set --
	if test "$transport" = modern; then
		set -- -global virtio-mmio.force-legacy=false
	fi
	if ! timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m 256M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		-append 'earlycon console=ttyAMA0 root=/dev/vda ro rootfstype=ext2 init=/sbin/init' \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" \
		>"$log" 2>&1; then
		fail_log
	fi
	grep -q '^Fiwix ARM Linux zImage header gate passed' "$log" ||
		fail_log
	grep -q '^Fiwix ARM Linux ext2 root handoff' "$log" || fail_log
	grep -q '^Linux version ' "$log" || fail_log
	grep -q 'VFS: Mounted root (ext2 filesystem) readonly' "$log" ||
		fail_log
	grep -q '^Fiwix ARM Linux root PID 1 passed' "$log" || fail_log
	if grep -Eiq 'Kernel panic|fatal .* trap|Fiwix ARM Linux zImage load failed' \
		"$log"; then
		fail_log
	fi
	tests/arm-ext2-check.sh "$run_disk"
done

echo "Fiwix ARM Linux ext2 root handoff passed"
