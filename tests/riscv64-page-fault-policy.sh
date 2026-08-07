#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -fno-builtin -ffunction-sections -fdata-sections \
	-D__KERNEL__ -DCONFIG_ARCH_RISCV64 -I"$root/include" \
	-Wall -Wextra -Werror "$root/mm/fault.c" \
	"$root/tests/riscv64-page-fault-policy.c" -Wl,--gc-sections \
	-o "$temporary/riscv64-page-fault-policy"
"$temporary/riscv64-page-fault-policy"

echo "Fiwix riscv64 page-fault policy gate passed"
