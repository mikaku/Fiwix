/*
 * fiwix/kernel/syscalls/readv.c
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

int sys_readv(unsigned int ufd, const struct iovec *iov, int iovcnt)
{
	struct inode *i;
	int errno;
	int bytes_read = 0;
	int vi;	/* vector index */

#ifdef __DEBUG__
	printk("(pid %d) sys_readv(%d, 0x%08x, %d) -> ", current->pid, ufd, buf, count);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	for (vi = 0; vi < iovcnt; vi++) {
		const struct iovec *io_read = &iov[vi];
		if((errno = check_user_area(VERIFY_WRITE, io_read->iov_base, io_read->iov_len))) {
			return errno;
		}
		if(fd_table[current->fd[ufd]].flags & O_WRONLY) {
			return -EBADF;
		}
		if(!io_read->iov_len) {
			continue;
		}
		if(io_read->iov_len < 0) {
			return -EINVAL;
		}

		i = fd_table[current->fd[ufd]].inode;
		if(i->fsop && i->fsop->read) {
			errno = i->fsop->read(i, &fd_table[current->fd[ufd]], io_read->iov_base, io_read->iov_len);
			if (errno < 0) {
			    return errno;
			}
			bytes_read += errno;
			if (errno < io_read->iov_len) {
				break;
			}
		} else {
			return -EINVAL;
		}
	}
#ifdef __DEBUG__
	printk("%d\n", bytes_read);
#endif /*__DEBUG__ */
	return bytes_read;
}
