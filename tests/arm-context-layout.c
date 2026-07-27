/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arch_process.h>

#define OFFSET(type, member) __builtin_offsetof(type, member)

int main(void)
{
	if(sizeof(struct arch_context) != 48 ||
		OFFSET(struct arch_context, r4) != 0 ||
		OFFSET(struct arch_context, r11) != 28 ||
		OFFSET(struct arch_context, sp) != 32 ||
		OFFSET(struct arch_context, lr) != 36 ||
		OFFSET(struct arch_context, ttbr0) != 40 ||
		OFFSET(struct arch_context, kernel_sp) != 44) {
		return 1;
	}
	return 0;
}
