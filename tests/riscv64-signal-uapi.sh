#!/bin/sh

set -eu

GENERIC_CC=${GENERIC_CC:-riscv64-linux-gnu-gcc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$GENERIC_CC" -std=c89 -Wall -Wextra -Werror -c \
	"$root/tests/riscv64-signal-uapi.c" \
	-o "$temporary/riscv64-signal-uapi.o"

echo "Fiwix riscv64 Linux signal-UAPI layout gate passed"
