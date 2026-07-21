#!/bin/sh

set -eu

QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-10}
kernel=${1:-./fiwix-generic}
temporary=$(mktemp)
trap 'rm -f "$temporary"' EXIT HUP INT TERM

if timeout "$TIMEOUT" "$QEMU" -machine virt -m 256M -smp 1 -nographic \
	-bios none -kernel "$kernel" -no-reboot > "$temporary" 2>&1; then
	status=0
else
	status=$?
fi

if test "$status" -ne 0; then
	cat "$temporary" >&2
	exit "$status"
fi
grep -q 'Fiwix riscv64 generic memory/timer init passed' "$temporary"

echo "Fiwix riscv64 generic boot smoke passed"
