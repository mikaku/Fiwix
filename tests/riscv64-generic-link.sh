#!/bin/sh

set -eu

GENERIC_CC=${GENERIC_CC:-riscv64-linux-gnu-gcc}
GENERIC_LD=${GENERIC_LD:-riscv64-linux-gnu-ld}
AS=${AS:-riscv64-linux-gnu-as}
NM=${NM:-riscv64-linux-gnu-nm}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

GENERIC_CC="$GENERIC_CC" GENERIC_LD="$GENERIC_LD" \
	GENERIC_OUTPUT="$temporary/generic-c.o" \
	"$root/tests/riscv64-generic-compile.sh"

objects="$temporary/generic-c.o"
for source in context generic-trap init_trampoline ops; do
	"$AS" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
		-o "$temporary/$source.o" "$root/arch/riscv64/$source.S"
	objects="$objects $temporary/$source.o"
done

"$GENERIC_LD" -m elf64lriscv -r $objects -o "$temporary/generic.o"
"$NM" -u "$temporary/generic.o" | sed 's/^ *U //' | sort -u \
	> "$temporary/unresolved"

if ! cmp -s "$root/tests/riscv64-generic-link.expected" \
	"$temporary/unresolved"; then
	diff -u "$root/tests/riscv64-generic-link.expected" \
		"$temporary/unresolved" || true
	echo "Fiwix riscv64 generic link contract changed" >&2
	exit 1
fi

if grep -q '^riscv64_' "$temporary/unresolved"; then
	echo "Fiwix riscv64 architecture symbol remains unresolved" >&2
	exit 1
fi

echo "Fiwix riscv64 generic link gate passed: 261 C files, 4 assembly files; 25 platform/linker boundaries remain"
