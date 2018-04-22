/*
 * fiwix/kernel/syscalls/getpid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getpid(void)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_getpid() -> %d\n", current->pid, current->pid);
#endif /*__DEBUG__ */
	return current->pid;
}
