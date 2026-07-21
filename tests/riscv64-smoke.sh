#!/bin/sh

set -eu

KERNEL=${1:-./fiwix}
QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-10}
READELF=${READELF:-riscv64-linux-gnu-readelf}

output=$(mktemp)
trap 'rm -f "$output"' EXIT HUP INT TERM

"$READELF" -h "$KERNEL" | grep -q 'Class:.*ELF64'
"$READELF" -h "$KERNEL" | grep -q 'Machine:.*RISC-V'
"$READELF" -h "$KERNEL" | grep -q 'Entry point address:.*0x80000000'

timeout "$TIMEOUT" "$QEMU" \
	-machine virt -m 256M -smp 1 -nographic -bios none \
	-kernel "$KERNEL" -no-reboot >"$output" 2>&1

grep -q '^Fiwix riscv64 milestone 1' "$output"
grep -q '^firmware-free machine-mode entry passed' "$output"
grep -q '^Fiwix riscv64 S-mode entry passed' "$output"
grep -q '^Fiwix riscv64 context-switch gate passed: 6 switches' "$output"
grep -q '^Fiwix riscv64 timer gate passed: 3 ticks' "$output"
if grep -q 'fatal .* trap' "$output"; then
	cat "$output" >&2
	exit 1
fi

cat "$output"
