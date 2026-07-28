#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-20}
QEMU_MEMORIES=${QEMU_MEMORIES:-"128M 256M"}
KERNEL=${1:-./fiwix-arm-generic.bin}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

for memory in $QEMU_MEMORIES; do
	log=$temporary/arm-generic-$memory.log
	timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m "$memory" -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		>"$log" 2>&1

	grep -q '^Fiwix ARM generic kernel entry' "$log"
	grep -q '^Fiwix ARM PL011 system console passed' "$log"
	grep -q '^Fiwix ARM firmware DTB discovery passed' "$log"
	grep -q \
		'^Fiwix ARM generic console, timer, memory, and process init passed' \
		"$log"
	if grep -Eiq 'undefined|unhandled|panic|fail|returned' "$log"; then
		cat "$log" >&2
		exit 1
	fi
done

printf '%s\n' 'Fiwix ARM generic boot smoke passed'
