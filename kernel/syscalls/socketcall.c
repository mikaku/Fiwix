/*
 * fiwix/kernel/syscalls/socketcall.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_socketcall(int call, unsigned long int *args)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_socketcall(%d, 0x%08x) -> ENOENT\n", current->pid, call, args);
#endif /*__DEBUG__ */

	/* FIXME: to be implemented */

	return -ENOENT;
}
