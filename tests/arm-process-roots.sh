#!/bin/sh
set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/arm/vm.c" \
	"$root/arch/arm/process.c" \
	"$root/tests/arm-process-roots.c" \
	-o "$temporary/arm-process-roots"
"$temporary/arm-process-roots"

if [ -n "${ARMCC:-}" ]; then
	set -- "$ARMCC"
	if [ -n "${ARMCC_TARGET:-}" ]; then
		set -- "$@" "$ARMCC_TARGET"
	fi
	"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
		-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
		-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
		-ffreestanding -Wall -Wextra -Werror -Wstrict-prototypes \
		-c "$root/arch/arm/process.c" \
		-o "$temporary/arm-process.o"
	"$@" -march=armv7-a -mfloat-abi=soft -marm -std=c89 \
		-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" \
		-O2 -fno-pie -fno-pic -fno-common -fno-stack-protector \
		-ffreestanding -Wall -Wextra -Werror -Wstrict-prototypes \
		-c "$root/arch/arm/task.c" \
		-o "$temporary/arm-task.o"
fi

printf '%s\n' 'Fiwix ARM struct proc root ownership gate passed'
