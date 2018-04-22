/*
 * fiwix/kernel/syscalls/close.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/locks.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>

int sys_close(unsigned int ufd)
{
	unsigned int fd;
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_close(%d)\n", current->pid, ufd);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	fd = current->fd[ufd];
	release_user_fd(ufd);

	if(--fd_table[fd].count) {
		return 0;
	}
	i = fd_table[fd].inode;
	flock_release_inode(i);
	if(i->fsop && i->fsop->close) {
		i->fsop->close(i, &fd_table[fd]);
		release_fd(fd);
		iput(i);
		return 0;
	}
	printk("WARNING: %s(): ufd %d without the close() method!\n", __FUNCTION__, ufd);
	return -EINVAL;
}
