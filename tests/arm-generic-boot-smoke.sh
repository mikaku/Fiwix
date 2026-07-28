#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-20}
KERNEL=${1:-./fiwix-arm-generic.bin}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

timeout "$TIMEOUT" "$QEMU" \
	-M virt,virtualization=on,secure=off -cpu cortex-a15 \
	-m 128M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
	>"$temporary/arm-generic.log" 2>&1

grep -q '^Fiwix ARM generic kernel entry' "$temporary/arm-generic.log"
grep -q '^Fiwix ARM generic memory and process init passed' \
	"$temporary/arm-generic.log"
if grep -Eiq 'undefined|unhandled|panic|fail|returned' \
	"$temporary/arm-generic.log"; then
	cat "$temporary/arm-generic.log" >&2
	exit 1
fi

printf '%s\n' 'Fiwix ARM generic boot smoke passed'
