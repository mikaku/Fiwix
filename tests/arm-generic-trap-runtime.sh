#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

AS=${AS:-arm-linux-gnueabihf-as}
LD=${LD:-arm-linux-gnueabihf-ld}
NM=${NM:-arm-linux-gnueabihf-nm}
OBJCOPY=${OBJCOPY:-arm-linux-gnueabihf-objcopy}
QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-10}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

for source in arch/arm/boot.S arch/arm/generic-trap.S \
	tests/arm-generic-trap-runtime.S; do
	object=$(printf '%s' "$source" | tr / -)
	"$AS" -march=armv7-a -mfloat-abi=soft \
		-o "$temporary/$object.o" "$root/$source"
done
"$NM" "$temporary/arch-arm-generic-trap.S.o" >"$temporary/symbols"
grep -q '^00000048 A arm_generic_trap_frame_size$' "$temporary/symbols"
"$LD" -m armelf_linux_eabi -T "$root/arch/arm/fiwix.ld" \
	-o "$temporary/runtime.elf" "$temporary"/*.o
"$OBJCOPY" -O binary "$temporary/runtime.elf" "$temporary/runtime.bin"

timeout "$TIMEOUT" "$QEMU" \
	-M virt,virtualization=on,secure=off -cpu cortex-a15 \
	-m 128M -smp 1 -nographic -kernel "$temporary/runtime.bin" \
	-no-reboot >"$temporary/runtime.log" 2>&1

grep -q '^Fiwix ARM generic trap runtime passed' "$temporary/runtime.log"
if grep -Eq 'failed|panic|unhandled' "$temporary/runtime.log"; then
	cat "$temporary/runtime.log" >&2
	exit 1
fi

printf '%s\n' 'Fiwix ARM generic trap runtime gate passed'
