/*
 * fiwix/kernel/syscalls/umask.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/process.h>
#include <fiwix/stat.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_umask(__mode_t mask)
{
	__mode_t old_umask;

#ifdef __DEBUG__
	printk("(pid %d) sys_umask(%d)\n", current->pid, mask);
#endif /*__DEBUG__ */

	old_umask = current->umask;
	current->umask = mask & (S_IRWXU | S_IRWXG | S_IRWXO);
	return old_umask;
}
