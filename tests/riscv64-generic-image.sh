#!/bin/sh

set -eu

GENERIC_CC=${GENERIC_CC:-riscv64-linux-gnu-gcc}
GENERIC_LD=${GENERIC_LD:-riscv64-linux-gnu-ld}
AS=${AS:-riscv64-linux-gnu-as}
NM=${NM:-riscv64-linux-gnu-nm}
READELF=${READELF:-riscv64-linux-gnu-readelf}
GENERIC_IMAGE=${GENERIC_IMAGE:-fiwix-generic}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

GENERIC_CC="$GENERIC_CC" GENERIC_LD="$GENERIC_LD" \
	GENERIC_OBJECT_DIR="$temporary/generic-c" \
	"$root/tests/riscv64-generic-compile.sh"

objects=$(find "$temporary/generic-c" -name '*.o' | sort)
for source in boot context generic-trap init_trampoline ops user; do
	"$AS" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
		-o "$temporary/$source.o" "$root/arch/riscv64/$source.S"
	objects="$objects $temporary/$source.o"
done
"$AS" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
	-o "$temporary/dead-stubs.o" \
	"$root/tests/riscv64-generic-dead-stubs.S"
objects="$objects $temporary/dead-stubs.o"

libgcc=$(
	"$GENERIC_CC" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
		-print-libgcc-file-name
)
"$GENERIC_LD" -m elf64lriscv --gc-sections \
	-T "$root/arch/riscv64/generic.ld" \
	-o "$GENERIC_IMAGE" $objects "$libgcc"

"$NM" "$GENERIC_IMAGE" > "$temporary/symbols"
if "$NM" -u "$GENERIC_IMAGE" | grep -q .; then
	"$NM" -u "$GENERIC_IMAGE" >&2
	echo "Fiwix riscv64 generic image retains undefined symbols" >&2
	exit 1
fi
"$NM" --defined-only "$temporary/dead-stubs.o" | \
	awk '$2 == "W" { print $3 }' > "$temporary/dead-symbols"
while read symbol; do
	if grep -q " [TW] $symbol\$" "$temporary/symbols"; then
		echo "Fiwix riscv64 generic image retained dead boundary: $symbol" >&2
		exit 1
	fi
done < "$temporary/dead-symbols"
grep -q ' T _start$' "$temporary/symbols"
grep -q ' T start_kernel$' "$temporary/symbols"
grep -q ' T riscv64_generic_trap_entry$' "$temporary/symbols"
grep -q ' T riscv64_generic_runtime_ready$' "$temporary/symbols"
"$READELF" -h "$GENERIC_IMAGE" | grep -q 'Class:.*ELF64'
"$READELF" -h "$GENERIC_IMAGE" | grep -q 'Machine:.*RISC-V'
if "$READELF" -lW "$GENERIC_IMAGE" | grep -q 'LOAD.*RWE'; then
	echo "Fiwix riscv64 generic image contains an RWE load segment" >&2
	exit 1
fi

echo "Fiwix riscv64 generic image link gate passed: $GENERIC_IMAGE"
