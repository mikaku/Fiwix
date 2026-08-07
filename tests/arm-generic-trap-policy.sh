#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

HOSTCC=${HOSTCC:-cc}
ARMCC=${ARMCC:-clang}
ARMCC_TARGET=${ARMCC_TARGET:---target=arm-linux-gnueabihf}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -O2 -fno-builtin -Wall -Wextra -Werror \
	-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
	"$root/arch/arm/trap.c" "$root/tests/arm-generic-trap-policy.c" \
	-no-pie -o "$temporary/arm-generic-trap-policy"
"$temporary/arm-generic-trap-policy"

set -- "$ARMCC"
if [ -n "$ARMCC_TARGET" ]; then
	set -- "$@" "$ARMCC_TARGET"
fi
"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
	-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
	-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
	-ffreestanding -Wall -Wextra -Werror -Wstrict-prototypes \
	-c "$root/arch/arm/trap.c" -o "$temporary/arm-trap.o"

printf '%s\n' 'Fiwix ARM generic trap policy gate passed'
