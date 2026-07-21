#!/bin/sh

set -eu

QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-10}
kernel=${1:-./fiwix-generic}
disk=${2:-arch/riscv64/fixture/disk.img}
temporary=$(mktemp)
modern=$(mktemp)
trap 'rm -f "$temporary" "$modern"' EXIT HUP INT TERM

tests/riscv64-ext2-check.sh "$disk"

run_qemu()
{
	output=$1
	shift
	if timeout "$TIMEOUT" "$QEMU" -machine virt -m 256M -smp 1 \
		-nographic -bios none -kernel "$kernel" -no-reboot \
		-drive file="$disk",format=raw,if=none,id=drive0,readonly=on \
		-device virtio-blk-device,drive=drive0 "$@" > "$output" 2>&1; then
		status=0
	else
		status=$?
	fi
	if test "$status" -ne 0 && test "$status" -ne 124; then
		cat "$output" >&2
		exit "$status"
	fi
	if ! grep -q 'Fiwix riscv64 generic PID 1 construction passed' \
		"$output" ||
		! grep -q 'Fiwix riscv64 PID 1 userspace passed' "$output"; then
		cat "$output" >&2
		exit 1
	fi
}

run_qemu "$temporary"
run_qemu "$modern" -global virtio-mmio.force-legacy=false

echo "Fiwix riscv64 generic boot smoke passed"
