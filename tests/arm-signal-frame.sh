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

"$HOSTCC" -std=c89 -fno-builtin -ffunction-sections -fdata-sections \
	-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
	-Wall -Wextra -Werror "$root/arch/arm/signal.c" \
	"$root/kernel/syscalls/sigaction.c" \
	"$root/tests/arm-signal-frame.c" -Wl,--gc-sections \
	-no-pie -o "$temporary/arm-signal-frame"
"$temporary/arm-signal-frame"

set -- "$ARMCC"
if [ -n "$ARMCC_TARGET" ]; then
	set -- "$@" "$ARMCC_TARGET"
fi
for source in arch/arm/signal.c kernel/signal.c; do
	object=$(printf '%s' "$source" | tr / -)
	"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
		-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
		-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
		-ffreestanding -Wall -Wextra -Werror -Wstrict-prototypes \
		-c "$root/$source" -o "$temporary/$object.o"
done

printf '%s\n' 'Fiwix ARM Linux signal-frame gate passed'
