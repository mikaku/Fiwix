#!/bin/sh

set -eu

AS=${AS:-riscv64-linux-gnu-as}
NM=${NM:-riscv64-linux-gnu-nm}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$AS" -march=rv64ima_zicsr_zifencei -mabi=lp64 \
	-o "$temporary/generic-trap.o" "$root/arch/riscv64/generic-trap.S"
"$NM" "$temporary/generic-trap.o" > "$temporary/symbols"

grep -q ' T riscv64_generic_trap_entry$' "$temporary/symbols"
grep -q ' T riscv64_generic_traps_install$' "$temporary/symbols"
grep -q ' U riscv64_generic_kernel_trap$' "$temporary/symbols"
grep -q ' U riscv64_generic_user_trap$' "$temporary/symbols"

echo "Fiwix riscv64 generic trap assembly gate passed"
