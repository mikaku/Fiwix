/*
 * fiwix/kernel/syscalls/fchdir.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/process.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_fchdir(unsigned int ufd)
{
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_fchdir(%d)\n", current->pid, ufd);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	i = fd_table[current->fd[ufd]].inode;
	if(!S_ISDIR(i->i_mode)) {
		return -ENOTDIR;
	}
	iput(current->pwd);
	current->pwd = i;
	current->pwd->count++;
	return 0;
}
