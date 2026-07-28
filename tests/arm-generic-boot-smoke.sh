#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-20}
QEMU_MEMORIES=${QEMU_MEMORIES:-"128M 256M"}
KERNEL=${1:-./fiwix-arm-generic.bin}
DISK=${2:-arch/arm/fixture/disk.img}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

if ! command -v "$QEMU" >/dev/null 2>&1; then
	printf 'ARM system emulator not found: %s\n' "$QEMU" >&2
	exit 1
fi
test -f "$KERNEL"
test -f "$DISK"
tests/arm-ext2-check.sh "$DISK"

fail_log()
{
	cat "$log" >&2
	exit 1
}

for memory in $QEMU_MEMORIES; do
	for transport in legacy modern; do
		log=$temporary/arm-generic-$memory-$transport.log
		run_disk=$temporary/arm-generic-$memory-$transport.img
		cp "$DISK" "$run_disk"
		set --
		version=1
		if test "$transport" = modern; then
			set -- -global virtio-mmio.force-legacy=false
			version=2
		fi
		if ! timeout "$TIMEOUT" "$QEMU" \
			-M virt,virtualization=on,secure=off -cpu cortex-a15 \
			-m "$memory" -smp 1 -nographic -kernel "$KERNEL" \
			-no-reboot \
			-drive file="$run_disk",format=raw,if=none,id=drive0 \
			-device virtio-blk-device,drive=drive0 "$@" \
			>"$log" 2>&1; then
			fail_log
		fi

		grep -q '^Fiwix ARM generic kernel entry' "$log" || fail_log
		grep -q '^Fiwix ARM PL011 system console passed' "$log" ||
			fail_log
		grep -q '^Fiwix ARM firmware DTB discovery passed' "$log" ||
			fail_log
		grep -q "^Fiwix ARM virtio-mmio v$version block passed" "$log" ||
			fail_log
		grep -q '^Fiwix ARM writable ext2 root passed' "$log" ||
			fail_log
		grep -q \
			'^Fiwix ARM generic console, timer, memory, and process init passed' \
			"$log" || fail_log
		if grep -Eiq 'undefined|unhandled|panic|fail|returned' "$log"; then
			fail_log
		fi
		tests/arm-ext2-check.sh "$run_disk"
	done
done

printf '%s\n' 'Fiwix ARM generic boot smoke passed'
