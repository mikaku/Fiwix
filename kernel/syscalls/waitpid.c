/*
 * fiwix/kernel/syscalls/waitpid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/syscalls.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_waitpid(__pid_t pid, int *status, int options)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_waitpid(%d, 0x%08x, %d)\n", current->pid, pid,  *status, options);
#endif /*__DEBUG__ */
	return sys_wait4(pid, status, options, NULL);
}
