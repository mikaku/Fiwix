#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_RISCV64 \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/riscv64/syscall.c" \
	"$root/tests/riscv64-syscall-translation.c" \
	-o "$temporary/riscv64-syscall-translation"
"$temporary/riscv64-syscall-translation"

echo "Fiwix riscv64 syscall translation gate passed"
