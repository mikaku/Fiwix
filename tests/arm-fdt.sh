#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

HOSTCC=${HOSTCC:-cc}
QEMU=${QEMU:-qemu-system-arm}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/arm/fdt.c" "$root/tests/arm-fdt.c" \
	-o "$temporary/arm-fdt"

for memory in 64M 128M 256M 512M; do
	"$QEMU" -machine "virt,dumpdtb=$temporary/$memory.dtb" \
		-cpu cortex-a15 -m "$memory" -nographic >/dev/null 2>&1
done

"$temporary/arm-fdt" "$temporary/64M.dtb" 16384 32
"$temporary/arm-fdt" "$temporary/128M.dtb" 32768 32
"$temporary/arm-fdt" "$temporary/256M.dtb" 65536 32
"$temporary/arm-fdt" "$temporary/512M.dtb" 65536 32

printf '%s\n' 'Fiwix ARM DTB memory and virtio discovery gate passed'
