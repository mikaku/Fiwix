#!/bin/sh

set -eu

KERNEL=${1:-./fiwix}
DISK=${2:-arch/riscv64/fixture/linux-root-disk.img}
QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-30}

tests/riscv64-ext2-check.sh "$DISK"

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

run_qemu()
{
	name=$1
	shift
	output=$temporary/$name.serial
	status=0
	timeout "$TIMEOUT" "$QEMU" \
		-machine virt -m 256M -smp 1 -nographic -bios none \
		-kernel "$KERNEL" -no-reboot \
		-append 'earlycon=uart8250,mmio,0x10000000 console=ttyS0,115200 root=/dev/vda ro rootfstype=ext2 init=/sbin/init' \
		-drive file="$DISK",format=raw,if=none,id=drive0,readonly=on \
		-device virtio-blk-device,drive=drive0 "$@" >"$output" 2>&1 || \
		status=$?
	if test "$status" -ne 0 ||
		! grep -q '^Fiwix riscv64 Linux Image header gate passed' "$output" ||
		! grep -q '^Linux version ' "$output" ||
		! grep -q 'VFS: Mounted root (ext2 filesystem) readonly' "$output" ||
		! grep -q '^Fiwix riscv64 Linux root PID 1 passed' "$output" ||
		grep -q 'Kernel panic\|fatal .* trap' "$output"; then
		cat "$output" >&2
		exit 1
	fi
}

run_qemu legacy
run_qemu modern -global virtio-mmio.force-legacy=false

echo "Fiwix riscv64 Linux ext2 root handoff passed"
