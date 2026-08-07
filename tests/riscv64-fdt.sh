#!/bin/sh

set -eu

HOSTCC=${HOSTCC:-cc}
QEMU=${QEMU:-qemu-system-riscv64}
root=$(cd "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_RISCV64 \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/arch/riscv64/fdt.c" "$root/tests/riscv64-fdt.c" \
	-o "$temporary/riscv64-fdt"

for memory in 256M 512M 2G 4G; do
	"$QEMU" -machine "virt,dumpdtb=$temporary/$memory.dtb" -m "$memory" \
		-nographic -bios none >/dev/null 2>&1
done

"$temporary/riscv64-fdt" "$temporary/256M.dtb" 65536
"$temporary/riscv64-fdt" "$temporary/512M.dtb" 131072
"$temporary/riscv64-fdt" "$temporary/2G.dtb" 524288
"$temporary/riscv64-fdt" "$temporary/4G.dtb" 524288

echo "Fiwix riscv64 DTB memory discovery gate passed"
