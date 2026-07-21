#!/bin/sh

set -eu

KERNEL=${1:-./fiwix}
QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-10}
READELF=${READELF:-riscv64-linux-gnu-readelf}
FIXTURE=${FIXTURE:-arch/riscv64/fixture/user.elf}
LINUX_FIXTURE=${LINUX_FIXTURE:-arch/riscv64/fixture/linux.elf}
DISK=${DISK:-arch/riscv64/fixture/disk.img}

output=$(mktemp)
modern_output=$(mktemp)
trap 'rm -f "$output" "$modern_output"' EXIT HUP INT TERM

"$READELF" -h "$KERNEL" | grep -q 'Class:.*ELF64'
"$READELF" -h "$KERNEL" | grep -q 'Machine:.*RISC-V'
"$READELF" -h "$KERNEL" | grep -q 'Entry point address:.*0x80000000'
"$READELF" -h "$FIXTURE" | grep -q 'Class:.*ELF64'
"$READELF" -h "$FIXTURE" | grep -q 'Machine:.*RISC-V'
"$READELF" -h "$FIXTURE" | grep -q 'Entry point address:.*0x400000'
"$READELF" -lW "$FIXTURE" | grep -q 'LOAD .* R E '
"$READELF" -h "$LINUX_FIXTURE" | grep -q 'Machine:.*RISC-V'
"$READELF" -h "$LINUX_FIXTURE" | grep -q 'Entry point address:.*0x0'

run_qemu()
{
	result=$1
	shift
	timeout "$TIMEOUT" "$QEMU" \
		-machine virt -m 256M -smp 1 -nographic -bios none \
		-kernel "$KERNEL" -no-reboot \
		-drive file="$DISK",format=raw,if=none,id=drive0,readonly=on \
		-device virtio-blk-device,drive=drive0 "$@" >"$result" 2>&1
}

check_common()
{
	result=$1
	grep -q '^Fiwix riscv64 milestone 1' "$result"
	grep -q '^firmware-free machine-mode entry passed' "$result"
	grep -q '^Fiwix riscv64 S-mode entry passed' "$result"
	grep -q '^Fiwix riscv64 context-switch gate passed: 6 switches' "$result"
	grep -q '^Fiwix riscv64 timer gate passed: 3 ticks' "$result"
	grep -q '^Fiwix riscv64 ELF64 loader gate passed' "$result"
	grep -q '^Fiwix riscv64 Sv39 gate passed' "$result"
	grep -q '^Fiwix riscv64 ext2 file gate passed' "$result"
	grep -q '^Fiwix riscv64 initial stack gate passed' "$result"
	grep -q '^Fiwix riscv64 U-mode write syscall passed' "$result"
	grep -q '^Fiwix riscv64 U-mode exit syscall passed: 42' "$result"
	grep -q '^Fiwix riscv64 Linux Image header gate passed' "$result"
	grep -q '^Fiwix riscv64 Linux Image handoff gate passed' "$result"
	if grep -q 'fatal .* trap' "$result"; then
		cat "$result" >&2
		exit 1
	fi
}

run_qemu "$output"
check_common "$output"
grep -q '^Fiwix riscv64 virtio-mmio v1 sector gate passed' "$output"

run_qemu "$modern_output" -global virtio-mmio.force-legacy=false
check_common "$modern_output"
grep -q '^Fiwix riscv64 virtio-mmio v2 sector gate passed' "$modern_output"

cat "$output"
cat "$modern_output"
