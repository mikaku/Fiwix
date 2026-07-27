#!/bin/sh
set -eu

OBJCOPY=${OBJCOPY:-arm-linux-gnueabihf-objcopy}
READELF=${READELF:-arm-linux-gnueabihf-readelf}
KERNEL=${1:-./fiwix}
fixture=${TMPDIR:-/tmp}/fiwix-arm-elf32-$$.elf
trap 'rm -f "$fixture"' EXIT HUP INT TERM

"$OBJCOPY" --dump-section .arm_elf_fixture="$fixture" "$KERNEL"
"$READELF" -h "$fixture" | grep -q 'Class:.*ELF32'
"$READELF" -h "$fixture" | grep -q 'Type:.*EXEC'
"$READELF" -h "$fixture" | grep -q 'Machine:.*ARM'
"$READELF" -h "$fixture" | grep -q 'Entry point address:.*0x100000'
test "$("$READELF" -lW "$fixture" | grep -c ' LOAD ')" -eq 1
"$READELF" -lW "$fixture" |
	grep -q 'LOAD.*0x00100000.*RWE.*0x1000'

printf '%s\n' 'ARM ELF32 fixture shape passed'
