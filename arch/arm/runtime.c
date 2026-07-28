/*
 * fiwix/arch/arm/runtime.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

/*
 * The ARM EABI integer division helpers call these hooks on division by zero.
 * A freestanding kernel cannot use libgcc's Linux implementation because it
 * calls the userspace raise() function.
 */
int __aeabi_idiv0(int result)
{
	return result;
}

long long int __aeabi_ldiv0(long long int result)
{
	return result;
}
