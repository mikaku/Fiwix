#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

HOSTCC=${HOSTCC:-cc}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-fno-builtin -ffunction-sections -fdata-sections -I"$root/include" \
	"$root/arch/arm/linux.c" "$root/tests/arm-linux-image.c" \
	-Wl,--gc-sections -o "$temporary/arm-linux-image"
"$temporary/arm-linux-image"

echo "Fiwix ARM Linux zImage and reservation gates passed"
