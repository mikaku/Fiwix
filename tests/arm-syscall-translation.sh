#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/arm/syscall.c" \
	"$root/tests/arm-syscall-translation.c" \
	-o "$temporary/arm-syscall-translation"
"$temporary/arm-syscall-translation"

printf '%s\n' 'Fiwix ARM EABI syscall translation gate passed'
