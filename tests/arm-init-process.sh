#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

ARMCC=${ARMCC:-clang}
ARMCC_TARGET=${ARMCC_TARGET:---target=arm-linux-gnueabihf}
AS=${AS:-arm-linux-gnueabihf-as}
NM=${NM:-arm-linux-gnueabihf-nm}
OBJCOPY=${OBJCOPY:-arm-linux-gnueabihf-objcopy}
READELF=${READELF:-arm-linux-gnueabihf-readelf}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$AS" -march=armv7-a -mfloat-abi=soft \
	-o "$temporary/init-trampoline.o" \
	"$root/arch/arm/init_trampoline.S"
"$READELF" -r "$temporary/init-trampoline.o" > "$temporary/relocations"
if ! grep -q 'There are no relocations' "$temporary/relocations"; then
	printf '%s\n' 'ARM init trampoline is not position-independent' >&2
	exit 1
fi
"$OBJCOPY" -O binary --only-section=.text \
	"$temporary/init-trampoline.o" "$temporary/init-trampoline.bin"
size=$(wc -c < "$temporary/init-trampoline.bin")
if [ "$size" -eq 0 ] || [ "$size" -gt 4096 ]; then
	printf 'ARM init trampoline has invalid size: %s\n' "$size" >&2
	exit 1
fi
for symbol in arm_init_trampoline_start arm_init_trampoline_end; do
	if ! "$NM" "$temporary/init-trampoline.o" | grep -q " $symbol\$"; then
		printf 'ARM init trampoline is missing %s\n' "$symbol" >&2
		exit 1
	fi
done

set -- "$ARMCC"
if [ -n "$ARMCC_TARGET" ]; then
	set -- "$@" "$ARMCC_TARGET"
fi
"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
	-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
	-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
	-ffreestanding -Wall -Werror -Wstrict-prototypes \
	-c "$root/kernel/init.c" -o "$temporary/init.o"

printf 'Fiwix ARM generic PID 1 construction gate passed (%s bytes)\n' "$size"
