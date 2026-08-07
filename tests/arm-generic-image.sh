#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

GENERIC_CC=${GENERIC_CC:-clang}
if [ "${GENERIC_CC_TARGET+x}" != x ]; then
	GENERIC_CC_TARGET=--target=arm-linux-gnueabihf
fi
GENERIC_LD=${GENERIC_LD:-arm-linux-gnueabihf-ld}
GENERIC_RUNTIME=${GENERIC_RUNTIME:-}
GENERIC_RETAINED_STUBS=${GENERIC_RETAINED_STUBS:-}
GENERIC_WORKDIR=${GENERIC_WORKDIR:-}
GENERIC_IMAGE=${GENERIC_IMAGE:-fiwix-arm-generic}
GENERIC_BINARY=${GENERIC_BINARY:-fiwix-arm-generic.bin}
AS=${AS:-arm-linux-gnueabihf-as}
NM=${NM:-arm-linux-gnueabihf-nm}
OBJCOPY=${OBJCOPY:-arm-linux-gnueabihf-objcopy}
READELF=${READELF:-arm-linux-gnueabihf-readelf}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
if [ -n "$GENERIC_WORKDIR" ]; then
	test "$GENERIC_WORKDIR" != / || {
		echo "refusing unsafe GENERIC_WORKDIR=/" >&2
		exit 1
	}
	temporary=$GENERIC_WORKDIR
	rm -rf "$temporary"
	mkdir -p "$temporary"
else
	temporary=$(mktemp -d)
fi
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
for source in boot context generic-trap handoff init_trampoline ops; do
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
: > "$temporary/retained-dead-symbols"
while IFS= read -r symbol; do
	if grep -q " [TW] $symbol\$" "$temporary/symbols"; then
		echo "$symbol" >> "$temporary/retained-dead-symbols"
	fi
done < "$temporary/dead-symbols"
if [ -n "$GENERIC_RETAINED_STUBS" ]; then
	sed '/^#/d; /^[[:space:]]*$/d' "$GENERIC_RETAINED_STUBS" \
		> "$temporary/expected-retained-dead-symbols"
	if ! cmp -s "$temporary/expected-retained-dead-symbols" \
		"$temporary/retained-dead-symbols"; then
		diff -u "$temporary/expected-retained-dead-symbols" \
			"$temporary/retained-dead-symbols" || true
		echo "Fiwix ARM generic image retained-stub contract changed" >&2
		exit 1
	fi
elif [ -s "$temporary/retained-dead-symbols" ]; then
	sed 's/^/Fiwix ARM generic image retained dead boundary: /' \
		"$temporary/retained-dead-symbols" >&2
	exit 1
fi

grep -q ' T _start$' "$temporary/symbols"
grep -q ' T start_kernel$' "$temporary/symbols"
grep -q ' T arm_generic_vector_table$' "$temporary/symbols"
grep -q ' T arm_generic_runtime_ready$' "$temporary/symbols"
grep -q ' T arm_fdt_boot_discover$' "$temporary/symbols"
grep -q ' T arm_linux_kexec$' "$temporary/symbols"
grep -q ' T arm_linux_handoff$' "$temporary/symbols"
grep -q ' T arm_pl011_init$' "$temporary/symbols"
grep -q ' T arm_generic_interrupt_init$' "$temporary/symbols"
grep -q ' T arm_ext2_writable_gate$' "$temporary/symbols"
grep -q ' T arm_virtio_block_init$' "$temporary/symbols"
grep -q ' T init_init$' "$temporary/symbols"
grep -q ' T arm_elf32_load$' "$temporary/symbols"
grep -q ' T arm_signal_deliver$' "$temporary/symbols"
grep -q ' T arm_fork_process_setup$' "$temporary/symbols"
"$READELF" -h "$GENERIC_IMAGE" | grep -q 'Class:.*ELF32'
"$READELF" -h "$GENERIC_IMAGE" | grep -q 'Machine:.*ARM'
if "$READELF" -lW "$GENERIC_IMAGE" | grep -q 'LOAD.*RWE'; then
	echo "Fiwix ARM generic image contains an RWE load segment" >&2
	exit 1
fi

printf 'Fiwix ARM generic image link gate passed: %s\n' "$GENERIC_IMAGE"
