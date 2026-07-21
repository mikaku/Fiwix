#!/bin/sh

set -eu

GENERIC_CC=${GENERIC_CC:-riscv64-linux-gnu-gcc}
GENERIC_LD=${GENERIC_LD:-riscv64-linux-gnu-ld}
GENERIC_OUTPUT=${GENERIC_OUTPUT:-}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

compiled=0
excluded=0
objects=
cd "$root"

for source in arch/riscv64/elf64.c arch/riscv64/exec.c \
	arch/riscv64/process.c arch/riscv64/signal.c arch/riscv64/syscall.c \
	arch/riscv64/trap.c \
	$(find kernel mm fs drivers net lib -name '*.c' | sort); do
	case "$source" in
	kernel/gdt.c|kernel/idt.c|kernel/main.c)
		excluded=$((excluded + 1))
		continue
		;;
	drivers/video/font-lat9-*.c)
		# These data files are included by fonts.c, not built separately.
		continue
		;;
	esac

	output=$temporary/$(printf '%s' "$source" | tr / _).o
	"$GENERIC_CC" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
		-mcmodel=medany -msmall-data-limit=0 -std=c89 -D__KERNEL__ \
		-DCONFIG_ARCH_RISCV64 -I"$root/include" -O2 -fno-pie \
		-fno-pic -fno-common -fno-stack-protector -ffreestanding \
		-Wall -Wstrict-prototypes -c "$source" -o "$output"
	objects="$objects $output"
	compiled=$((compiled + 1))
done

test "$excluded" -eq 3
test "$compiled" -eq 260
if test -n "$GENERIC_OUTPUT"; then
	"$GENERIC_LD" -m elf64lriscv -r $objects -o "$GENERIC_OUTPUT"
fi
echo "Fiwix riscv64 generic compile gate passed: $compiled files; $excluded architecture boundaries remain"
