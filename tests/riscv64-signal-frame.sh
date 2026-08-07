#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -fno-builtin -ffunction-sections -fdata-sections \
	-D__KERNEL__ -DCONFIG_ARCH_RISCV64 -I"$root/include" \
	-Wall -Wextra -Werror "$root/arch/riscv64/signal.c" \
	"$root/kernel/syscalls/sigaction.c" \
	"$root/tests/riscv64-signal-frame.c" -Wl,--gc-sections \
	-no-pie -o "$temporary/riscv64-signal-frame"
"$temporary/riscv64-signal-frame"

echo "Fiwix riscv64 signal-frame gate passed"
