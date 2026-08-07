#!/bin/sh

set -eu

KERNEL=${1:-./fiwix}
QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-20}
DISK=${DISK:-arch/riscv64/fixture/disk.img}

tests/riscv64-ext2-check.sh "$DISK"

output=$(mktemp)
trap 'rm -f "$output"' EXIT HUP INT TERM

status=0
timeout "$TIMEOUT" "$QEMU" \
	-machine virt -m 256M -smp 1 -nographic -bios none \
	-kernel "$KERNEL" -no-reboot \
	-append 'earlycon=uart8250,mmio,0x10000000 console=ttyS0,115200' \
	-drive file="$DISK",format=raw,if=none,id=drive0,readonly=on \
	-device virtio-blk-device,drive=drive0 >"$output" 2>&1 || status=$?

if test "$status" -ne 124; then
	cat "$output" >&2
	echo "QEMU exited with status $status before the Linux smoke timeout" >&2
	exit 1
fi

grep -q '^Fiwix riscv64 Linux Image header gate passed' "$output"
grep -q '^Linux version ' "$output"
grep -q '^SBI specification v0\.3 detected' "$output"
grep -q '^SBI TIME extension detected' "$output"
grep -q '^SBI SRST extension detected' "$output"
grep -q '^riscv-plic: .* mapped .* interrupts' "$output"
grep -q 'ttyS0 at MMIO 0x10000000' "$output"
grep -q '^Kernel panic - not syncing: No working init found' "$output"

if grep -q 'fatal .* trap' "$output"; then
	cat "$output" >&2
	exit 1
fi

cat "$output"
