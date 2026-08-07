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

"$HOSTCC" -std=c89 -fno-builtin -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/arm/vm.c" \
	"$root/arch/arm/process.c" \
	"$root/arch/arm/memory.c" \
	"$root/tests/arm-generic-memory.c" \
	-o "$temporary/arm-generic-memory"
"$temporary/arm-generic-memory"

set -- "$ARMCC"
if [ -n "$ARMCC_TARGET" ]; then
	set -- "$@" "$ARMCC_TARGET"
fi
"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
	-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
	-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
	-ffreestanding -Wall -Wextra -Werror -Wstrict-prototypes \
	-c "$root/arch/arm/memory.c" \
	-o "$temporary/arm-memory.o"
"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
	-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
	-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
	-ffreestanding -Wall -Werror -Wstrict-prototypes \
	-c "$root/mm/memory.c" \
	-o "$temporary/generic-memory.o"

printf '%s\n' 'Fiwix ARM generic memory backend gate passed'
