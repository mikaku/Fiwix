/*
 * fiwix/kernel/syscalls/write.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_write(unsigned int ufd, const char *buf, int count)
{
	struct inode *i;
	int errno;

#ifdef __DEBUG__
/*	printk("(pid %d) sys_write(%d, '%s', %d)\n", current->pid, ufd, buf, count);*/
	printk("(pid %d) sys_write(%d, 0x%08x, %d) -> ", current->pid, ufd, buf, count);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	if((errno = check_user_area(VERIFY_READ, buf, count))) {
		return errno;
	}
	if(fd_table[current->fd[ufd]].flags & O_RDONLY) {
		return -EBADF;
	}
	if(!count) {
		return 0;
	}
	if(count < 0) {
		return -EINVAL;
	}
	i = fd_table[current->fd[ufd]].inode;
	if(i->fsop && i->fsop->write) {
		errno = i->fsop->write(i, &fd_table[current->fd[ufd]], buf, count);
#ifdef __DEBUG__
		printk("%d\n", errno);
#endif /*__DEBUG__ */
		return errno;
	}
	return -EINVAL;
}
