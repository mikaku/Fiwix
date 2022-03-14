/*
 * fiwix/kernel/syscalls/flock.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/process.h>
#include <fiwix/locks.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_flock(unsigned int ufd, int op)
{
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_flock(%d, %d)\n", current->pid, ufd, op);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	i = fd_table[current->fd[ufd]].inode;
	return flock_inode(i, op);
}
