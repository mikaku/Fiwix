#!/bin/sh

set -eu

AS=${AS:-riscv64-linux-gnu-as}
LD=${LD:-riscv64-linux-gnu-ld}
QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-10}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$AS" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
	-o "$temporary/generic-trap.o" "$root/arch/riscv64/generic-trap.S"
"$AS" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
	-o "$temporary/runtime.o" "$root/tests/riscv64-generic-trap-runtime.S"
"$LD" -m elf64lriscv -T "$root/tests/riscv64-generic-trap-runtime.ld" \
	-o "$temporary/runtime.elf" "$temporary/runtime.o" \
	"$temporary/generic-trap.o"

timeout "$TIMEOUT" "$QEMU" -machine virt -m 256M -smp 1 -nographic \
	-bios none -kernel "$temporary/runtime.elf" -no-reboot >/dev/null 2>&1

echo "Fiwix riscv64 generic trap runtime gate passed"
