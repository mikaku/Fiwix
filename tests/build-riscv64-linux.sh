#!/bin/sh

set -eu

if test "$#" -lt 2 || test "$#" -gt 3; then
	echo "usage: $0 LINUX_SOURCE OUTPUT_DIRECTORY [EXTRA_CONFIG]" >&2
	exit 2
fi

source=$1
output=$2
extra_config=${3:-}
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
set -- "$output/.config" "$root/tests/riscv64-linux.config"
if test -n "$extra_config"; then
	test -f "$extra_config" || {
		echo "missing Linux config fragment: $extra_config" >&2
		exit 2
	}
	set -- "$@" "$extra_config"
fi
"$source/scripts/kconfig/merge_config.sh" -m -O "$output" \
	"$@"
make -C "$source" O="$output" ARCH=riscv olddefconfig
make -C "$source" O="$output" ARCH=riscv -j"$JOBS" Image

echo "$output/arch/riscv/boot/Image"
