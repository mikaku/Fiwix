/*
 * fiwix/kernel/syscalls/ioctl.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_ioctl(unsigned int fd, int cmd, unsigned long int arg)
{
	int errno;
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_ioctl(%d, 0x%x, 0x%08x) -> ", current->pid, fd, cmd, arg);
#endif /*__DEBUG__ */

	CHECK_UFD(fd);
	i = fd_table[current->fd[fd]].inode;
	if(i->fsop && i->fsop->ioctl) {
		errno = i->fsop->ioctl(i, cmd, arg);

#ifdef __DEBUG__
		printk("%d\n", errno);
#endif /*__DEBUG__ */

		return errno;
	}

#ifdef __DEBUG__
	printk("%d\n", -ENOTTY);
#endif /*__DEBUG__ */

	return -ENOTTY;
}
