/*
 * fiwix/kernel/syscalls/umount.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/process.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_umount(const char *target)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_umount(%s)\n", current->pid, target);
#endif /*__DEBUG__ */

	return sys_umount2(target, 0);
}
