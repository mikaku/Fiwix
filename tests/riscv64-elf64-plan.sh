#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/riscv64/elf64.c" "$root/tests/riscv64-elf64-plan.c" \
	-o "$temporary/riscv64-elf64-plan"
"$temporary/riscv64-elf64-plan"
if test -n "${RISCV64_STAGE0_SEED:-}"; then
	"$temporary/riscv64-elf64-plan" "$RISCV64_STAGE0_SEED"
fi

echo "Fiwix riscv64 ELF64 plan gate passed"
