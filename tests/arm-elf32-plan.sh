#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/arm/elf32.c" "$root/tests/arm-elf32-plan.c" \
	-o "$temporary/arm-elf32-plan"
"$temporary/arm-elf32-plan"

printf '%s\n' 'Fiwix ARM ELF32 plan gate passed'
