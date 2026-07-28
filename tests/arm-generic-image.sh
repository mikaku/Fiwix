#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

GENERIC_CC=${GENERIC_CC:-clang}
GENERIC_CC_TARGET=${GENERIC_CC_TARGET:---target=arm-linux-gnueabihf}
GENERIC_LD=${GENERIC_LD:-arm-linux-gnueabihf-ld}
GENERIC_RUNTIME=${GENERIC_RUNTIME:-}
GENERIC_IMAGE=${GENERIC_IMAGE:-fiwix-arm-generic}
GENERIC_BINARY=${GENERIC_BINARY:-fiwix-arm-generic.bin}
AS=${AS:-arm-linux-gnueabihf-as}
NM=${NM:-arm-linux-gnueabihf-nm}
OBJCOPY=${OBJCOPY:-arm-linux-gnueabihf-objcopy}
READELF=${READELF:-arm-linux-gnueabihf-readelf}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

GENERIC_CC="$GENERIC_CC" GENERIC_CC_TARGET="$GENERIC_CC_TARGET" \
	GENERIC_LD="$GENERIC_LD" \
	GENERIC_OBJECT_DIR="$temporary/generic-c" \
	GENERIC_OBJECT_LIST="$temporary/generic-objects.list" \
	"$root/tests/arm-generic-compile.sh"

objects=
while IFS= read -r object; do
	objects="$objects $object"
done < "$temporary/generic-objects.list"
for source in boot context generic-trap init_trampoline ops; do
	"$AS" -march=armv7-a -mfloat-abi=soft \
		-o "$temporary/$source.o" "$root/arch/arm/$source.S"
	objects="$objects $temporary/$source.o"
done
"$AS" -march=armv7-a -mfloat-abi=soft \
	-o "$temporary/dead-stubs.o" \
	"$root/tests/arm-generic-dead-stubs.S"
objects="$objects $temporary/dead-stubs.o"

if [ -z "$GENERIC_RUNTIME" ]; then
	set -- "$GENERIC_CC"
	if [ -n "$GENERIC_CC_TARGET" ]; then
		set -- "$@" "$GENERIC_CC_TARGET"
	fi
	GENERIC_RUNTIME=$("$@" -march=armv7-a -mfloat-abi=soft \
		-print-libgcc-file-name)
fi
test -f "$GENERIC_RUNTIME" || {
	printf 'Fiwix ARM runtime archive not found: %s\n' \
		"$GENERIC_RUNTIME" >&2
	exit 1
}

# shellcheck disable=SC2086
"$GENERIC_LD" -m armelf_linux_eabi --gc-sections \
	-T "$root/arch/arm/generic.ld" -o "$GENERIC_IMAGE" \
	$objects "$GENERIC_RUNTIME"
"$OBJCOPY" -O binary "$GENERIC_IMAGE" "$GENERIC_BINARY"

"$NM" "$GENERIC_IMAGE" > "$temporary/symbols"
if "$NM" -u "$GENERIC_IMAGE" | grep -q .; then
	"$NM" -u "$GENERIC_IMAGE" >&2
	echo "Fiwix ARM generic image retains undefined symbols" >&2
	exit 1
fi
"$NM" --defined-only "$temporary/dead-stubs.o" |
	awk '$2 == "W" { print $3 }' > "$temporary/dead-symbols"
while IFS= read -r symbol; do
	if grep -q " [TW] $symbol\$" "$temporary/symbols"; then
		printf 'Fiwix ARM generic image retained dead boundary: %s\n' \
			"$symbol" >&2
		exit 1
	fi
done < "$temporary/dead-symbols"

grep -q ' T _start$' "$temporary/symbols"
grep -q ' T start_kernel$' "$temporary/symbols"
grep -q ' T arm_generic_vector_table$' "$temporary/symbols"
grep -q ' T arm_generic_runtime_ready$' "$temporary/symbols"
grep -q ' T arm_fdt_parse$' "$temporary/symbols"
grep -q ' T arm_pl011_init$' "$temporary/symbols"
grep -q ' T arm_generic_interrupt_init$' "$temporary/symbols"
"$READELF" -h "$GENERIC_IMAGE" | grep -q 'Class:.*ELF32'
"$READELF" -h "$GENERIC_IMAGE" | grep -q 'Machine:.*ARM'
if "$READELF" -lW "$GENERIC_IMAGE" | grep -q 'LOAD.*RWE'; then
	echo "Fiwix ARM generic image contains an RWE load segment" >&2
	exit 1
fi

printf 'Fiwix ARM generic image link gate passed: %s\n' "$GENERIC_IMAGE"
