#!/bin/sh

set -eu

if test "$#" -ne 2; then
	echo "usage: $0 LINUX_SOURCE OUTPUT_DIRECTORY" >&2
	exit 2
fi

source=$1
output=$2
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
source=$(CDPATH= cd -- "$source" && pwd)
mkdir -p "$output"
output=$(CDPATH= cd -- "$output" && pwd)

: "${CROSS_COMPILE:=riscv64-linux-gnu-}"
: "${JOBS:=1}"
: "${KBUILD_BUILD_USER:=fiwix}"
: "${KBUILD_BUILD_HOST:=bootstrap}"
: "${KBUILD_BUILD_TIMESTAMP:=2024-11-22 00:00:00 UTC}"
export CROSS_COMPILE KBUILD_BUILD_USER KBUILD_BUILD_HOST KBUILD_BUILD_TIMESTAMP

make -C "$source" O="$output" ARCH=riscv tinyconfig
"$source/scripts/kconfig/merge_config.sh" -m -O "$output" \
	"$output/.config" "$root/tests/riscv64-linux.config"
make -C "$source" O="$output" ARCH=riscv olddefconfig
make -C "$source" O="$output" ARCH=riscv -j"$JOBS" Image

echo "$output/arch/riscv/boot/Image"
