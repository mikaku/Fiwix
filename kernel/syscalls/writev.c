/*
 * fiwix/kernel/syscalls/writev.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Copyright 2023, Richard R. Masters.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_writev(int ufd, const struct iovec *iov, int iovcnt)
{
	struct inode *i;
	int errno;
	int bytes_written = 0;
	int vi;	/* vector index */

#ifdef __DEBUG__
	printk("(pid %d) sys_writev(%d, iov, %d) -> ", current->pid, ufd, iovcnt);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	for (vi = 0; vi < iovcnt; vi++) {
		const struct iovec *io_write = &iov[vi];
		if(!io_write->iov_len) {
			continue;
		}
		if((errno = check_user_area(VERIFY_READ, io_write->iov_base, io_write->iov_len))) {
			return errno;
		}
		if(fd_table[current->fd[ufd]].flags & O_RDONLY) {
			return -EBADF;
		}
		if(io_write->iov_len < 0) {
			return -EINVAL;
		}
		i = fd_table[current->fd[ufd]].inode;
		if(i->fsop && i->fsop->write) {
			errno = i->fsop->write(i, &fd_table[current->fd[ufd]], io_write->iov_base, io_write->iov_len);
			if (errno < 0) {
				return errno;
			}
			bytes_written += errno;
		} else {
			return -EINVAL;
		}
	}
#ifdef __DEBUG__
	printk("%d\n", bytes_written);
#endif /*__DEBUG__ */
	return bytes_written;
}
