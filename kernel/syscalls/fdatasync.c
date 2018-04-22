/*
 * fiwix/kernel/syscalls/fdatasync.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_fdatasync(int ufd)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_fdatasync(%d)\n", current->pid, ufd);
#endif /*__DEBUG__ */

	return sys_fsync(ufd);
}
