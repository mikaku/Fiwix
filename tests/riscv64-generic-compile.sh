#!/bin/sh

set -eu

GENERIC_CC=${GENERIC_CC:-riscv64-linux-gnu-gcc}
GENERIC_LD=${GENERIC_LD:-riscv64-linux-gnu-ld}
GENERIC_OUTPUT=${GENERIC_OUTPUT:-}
GENERIC_OBJECT_DIR=${GENERIC_OBJECT_DIR:-}
GENERIC_OBJECT_LIST=${GENERIC_OBJECT_LIST:-}
GENERIC_SOURCE_LIST=${GENERIC_SOURCE_LIST:-}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
GENERIC_SOURCE_LIST=${GENERIC_SOURCE_LIST:-$root/tests/riscv64-generic-sources.list}
if test -n "$GENERIC_OBJECT_DIR"; then
	temporary=$GENERIC_OBJECT_DIR
	mkdir -p "$temporary"
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT HUP INT TERM
fi

compiled=0
objects=
cd "$root"

test -f "$GENERIC_SOURCE_LIST"
test -f kernel/gdt.c
test -f kernel/idt.c
if test -n "$GENERIC_OBJECT_LIST"; then
	: > "$GENERIC_OBJECT_LIST"
fi

while IFS= read -r source; do
	test -n "$source" || continue
	test -f "$source"
	output=$temporary/$(printf '%s' "$source" | tr / _).o
	"$GENERIC_CC" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
		-mcmodel=medany -msmall-data-limit=0 -std=c89 -D__KERNEL__ \
		-DCONFIG_ARCH_RISCV64 -I"$root/include" -O2 -fno-pie \
		-fno-pic -fno-common -fno-stack-protector -ffreestanding \
		-ffunction-sections -fdata-sections \
		-Wall -Wstrict-prototypes -c "$source" -o "$output"
	objects="$objects $output"
	if test -n "$GENERIC_OBJECT_LIST"; then
		printf '%s\n' "$output" >> "$GENERIC_OBJECT_LIST"
	fi
	compiled=$((compiled + 1))
done < "$GENERIC_SOURCE_LIST"

test "$compiled" -eq 265
if test -n "$GENERIC_OUTPUT"; then
	"$GENERIC_LD" -m elf64lriscv -r $objects -o "$GENERIC_OUTPUT"
fi
echo "Fiwix riscv64 generic compile gate passed: $compiled files; 2 architecture boundaries remain"
