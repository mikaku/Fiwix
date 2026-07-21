#!/bin/sh

set -eu

output=${1:?usage: make-riscv64-disk.sh OUTPUT}

dd if=/dev/zero of="$output" bs=512 count=8 2>/dev/null
printf 'Fiwix riscv64 virtio sector gate\n' |
	dd of="$output" bs=512 count=1 conv=notrunc 2>/dev/null
