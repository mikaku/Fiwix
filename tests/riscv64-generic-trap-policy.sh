#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -fno-builtin -D__KERNEL__ -DCONFIG_ARCH_RISCV64 \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/riscv64/trap.c" \
	"$root/tests/riscv64-generic-trap-policy.c" \
	-o "$temporary/riscv64-generic-trap-policy"
"$temporary/riscv64-generic-trap-policy"

echo "Fiwix riscv64 generic trap policy gate passed"
