/*
 * fiwix/kernel/syscalls/personality.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_personality(unsigned long int persona)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_personality(%d) -> %d\n", current->pid, persona, persona);
#endif /*__DEBUG__ */
	return persona;
}
