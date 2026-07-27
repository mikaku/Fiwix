#!/bin/sh
set -eu

ARMCC=${ARMCC:-clang}
ARMCC_TARGET=${ARMCC_TARGET:---target=arm-linux-gnueabihf}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

set -- "$ARMCC"
if [ -n "$ARMCC_TARGET" ]; then
	set -- "$@" "$ARMCC_TARGET"
fi
for source in kernel/process.c kernel/sched.c kernel/syscalls/fork.c; do
	object=$(printf '%s' "$source" | tr / -)
	"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
		-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
		-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
		-ffreestanding -Wall -Werror -Wstrict-prototypes \
		-c "$root/$source" -o "$temporary/$object.o"
done

printf '%s\n' 'Fiwix ARM generic process lifecycle compile gate passed'
