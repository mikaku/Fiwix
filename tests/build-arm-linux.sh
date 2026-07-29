#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

if test "$#" -lt 2 || test "$#" -gt 3; then
	echo "usage: $0 LINUX_SOURCE OUTPUT_DIRECTORY [EXTRA_CONFIG]" >&2
	exit 2
fi

source=$1
output=$2
extra_config=${3:-}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
source=$(CDPATH='' cd -- "$source" && pwd)
mkdir -p "$output"
output=$(CDPATH='' cd -- "$output" && pwd)

command -v bc >/dev/null 2>&1 || {
	echo "bc is required as a native Linux Kbuild tool" >&2
	exit 1
}

: "${CROSS_COMPILE:=arm-linux-gnueabihf-}"
: "${JOBS:=1}"
: "${KBUILD_BUILD_USER:=fiwix}"
: "${KBUILD_BUILD_HOST:=bootstrap}"
: "${KBUILD_BUILD_TIMESTAMP:=2024-11-22 00:00:00 UTC}"
export CROSS_COMPILE KBUILD_BUILD_USER KBUILD_BUILD_HOST
export KBUILD_BUILD_TIMESTAMP

make -C "$source" O="$output" ARCH=arm tinyconfig
set -- "$output/.config" "$root/tests/arm-linux.config"
if test -n "$extra_config"; then
	test -f "$extra_config" || {
		echo "missing Linux config fragment: $extra_config" >&2
		exit 2
	}
	set -- "$@" "$extra_config"
fi
"$source/scripts/kconfig/merge_config.sh" -m -O "$output" "$@"
make -C "$source" O="$output" ARCH=arm olddefconfig
for required in \
	CONFIG_MMU=y \
	CONFIG_ARCH_VIRT=y \
	CONFIG_AEABI=y \
	CONFIG_PRINTK=y \
	CONFIG_TTY=y \
	CONFIG_SERIAL_AMBA_PL011=y \
	CONFIG_SERIAL_AMBA_PL011_CONSOLE=y \
	CONFIG_ARM_PSCI_FW=y
do
	grep -qx "$required" "$output/.config" || {
		echo "Linux configuration lost required option: $required" >&2
		exit 1
	}
done
make -C "$source" O="$output" ARCH=arm -j"$JOBS" zImage

image=$output/arch/arm/boot/zImage
size=$(wc -c < "$image")
test "$size" -le 8388608 || {
	echo "Linux zImage exceeds the 8 MiB Fiwix handoff window" >&2
	exit 1
}
echo "$image"
