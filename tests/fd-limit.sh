#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -fno-builtin -D__KERNEL__ -DCONFIG_ARCH_RISCV64 \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/fs/fd.c" \
	"$root/kernel/syscalls/dup2.c" \
	"$root/tests/fd-limit.c" \
	-o "$temporary/fd-limit"
"$temporary/fd-limit"

echo "Fiwix descriptor limit gate passed"
