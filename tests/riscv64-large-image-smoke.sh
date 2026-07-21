#!/bin/sh

set -eu

KERNEL=${1:-./fiwix}
LINUX_FIXTURE=${LINUX_FIXTURE:-arch/riscv64/fixture/linux.bin}
DISK_BUILDER=${DISK_BUILDER:-arch/riscv64/fixture/make-riscv64-disk}
PADDED_SIZE=${PADDED_SIZE:-307200}

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

cp "$LINUX_FIXTURE" "$temporary/linux.bin"
truncate -s "$PADDED_SIZE" "$temporary/linux.bin"
"$DISK_BUILDER" "$temporary/disk.img" "$temporary/linux.bin"

DISK="$temporary/disk.img" tests/riscv64-smoke.sh "$KERNEL"
