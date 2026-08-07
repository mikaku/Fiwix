#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

AS=${AS:-arm-linux-gnueabihf-as}
HOSTCC=${HOSTCC:-cc}
NM=${NM:-arm-linux-gnueabihf-nm}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$HOSTCC" -std=c89 -D__KERNEL__ -DCONFIG_ARCH_ARM \
	-I"$root/include" -Wall -Wextra -Werror \
	"$root/tests/arm-context-layout.c" \
	-o "$temporary/arm-context-layout"
"$temporary/arm-context-layout"

"$AS" -march=armv7-a -mfloat-abi=soft \
	-o "$temporary/context.o" "$root/arch/arm/context.S"
"$AS" -march=armv7-a -mfloat-abi=soft \
	-o "$temporary/ops.o" "$root/arch/arm/ops.S"
"$NM" "$temporary/context.o" > "$temporary/symbols"
grep -q '^00000138 A arm_arch_context_size$' "$temporary/symbols"
grep -q ' T arm_context_switch$' "$temporary/symbols"
grep -q ' T arm_vfp_context_save$' "$temporary/symbols"
grep -q ' T arm_context_gate_alternate$' "$temporary/symbols"
grep -q ' T arm_kernel_process_entry$' "$temporary/symbols"
grep -q ' T arm_user_process_entry$' "$temporary/symbols"
grep -q ' T arm_return_to_user$' "$temporary/symbols"
"$NM" "$temporary/ops.o" > "$temporary/ops-symbols"
grep -q ' T arm_interrupt_disable$' "$temporary/ops-symbols"
grep -q ' T arm_user_syscall3$' "$temporary/ops-symbols"
grep -q ' T invalidate_tlb$' "$temporary/ops-symbols"

printf '%s\n' 'Fiwix ARM scheduler context gate passed'
