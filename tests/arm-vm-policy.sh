#!/bin/sh
set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/arm/vm.c" \
	"$root/tests/arm-vm-policy.c" \
	-o "$temporary/arm-vm-policy"
"$temporary/arm-vm-policy"

printf '%s\n' 'Fiwix ARM process VM policy gate passed'
