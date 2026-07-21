#!/bin/sh

set -eu

GENERIC_CC=${GENERIC_CC:-riscv64-linux-gnu-gcc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

compiled=0
excluded=0
cd "$root"

for source in $(find kernel mm fs drivers net lib -name '*.c' | sort); do
	case "$source" in
	kernel/gdt.c|kernel/idt.c|kernel/init.c|kernel/main.c|kernel/process.c|\
	kernel/syscalls/fork.c|mm/fault.c|mm/memory.c|mm/mmap.c)
		excluded=$((excluded + 1))
		continue
		;;
	esac

	output=$temporary/$(printf '%s' "$source" | tr / _).o
	"$GENERIC_CC" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
		-mcmodel=medany -msmall-data-limit=0 -std=c89 -D__KERNEL__ \
		-DCONFIG_ARCH_RISCV64 -I"$root/include" -O2 -fno-pie \
		-fno-pic -fno-common -fno-stack-protector -ffreestanding \
		-Wall -Wstrict-prototypes -c "$source" -o "$output"
	compiled=$((compiled + 1))
done

test "$excluded" -eq 9
echo "Fiwix riscv64 generic compile gate passed: $compiled files; $excluded architecture boundaries remain"
