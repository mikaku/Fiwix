/*
 * fiwix/fs/pipefs/pipe.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/stat.h>
#include <fiwix/fcntl.h>
#include <fiwix/ioctl.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct resource pipe_resource = { NULL, NULL };

int pipefs_close(struct inode *i, struct fd *fd_table)
{
	if((fd_table->flags & O_ACCMODE) == O_RDONLY) {
		if(!--i->u.pipefs.i_readers) {
			wakeup(&pipefs_write);
			wakeup(&do_select);
		}
	}
	if((fd_table->flags & O_ACCMODE) == O_WRONLY) {
		if(!--i->u.pipefs.i_writers) {
			wakeup(&pipefs_read);
			wakeup(&do_select);
		}
	}
	if((fd_table->flags & O_ACCMODE) == O_RDWR) {
		if(!--i->u.pipefs.i_readers) {
			wakeup(&pipefs_write);
			wakeup(&do_select);
		}
		if(!--i->u.pipefs.i_writers) {
			wakeup(&pipefs_read);
			wakeup(&do_select);
		}
	}
	return 0;
}

int pipefs_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	__off_t bytes_read;
	__size_t n, limit;
	char *data;

	bytes_read = 0;
	data = i->u.pipefs.i_data;

	while(count) {
		if(i->u.pipefs.i_writeoff) {
			if(i->u.pipefs.i_readoff >= i->u.pipefs.i_writeoff) {
				limit = PIPE_BUF - i->u.pipefs.i_readoff;
			} else {
				limit = i->u.pipefs.i_writeoff - i->u.pipefs.i_readoff;
			}
		} else {
			limit = PIPE_BUF - i->u.pipefs.i_readoff;
		}
		n = MIN(limit, count);
		if(i->i_size && n) {
			lock_resource(&pipe_resource);
			memcpy_b(buffer + bytes_read, data + i->u.pipefs.i_readoff, n);
			bytes_read += n;
			i->u.pipefs.i_readoff += n;
			i->i_size -= n;
			if(i->u.pipefs.i_writeoff >= PIPE_BUF) {
				i->u.pipefs.i_writeoff = 0;
			}
			unlock_resource(&pipe_resource);
			wakeup(&pipefs_write);
			wakeup(&do_select);
			break;
		} else {
			if(i->u.pipefs.i_writers) {
				if(fd_table->flags & O_NONBLOCK) {
					return -EAGAIN;
				}
				if(sleep(&pipefs_read, PROC_INTERRUPTIBLE)) {
					return -EINTR;
				}
			} else {
				if(i->i_size) {
					if(i->u.pipefs.i_readoff >= PIPE_BUF) {
						i->u.pipefs.i_readoff = 0;
						continue;
					}
				}
				break;
			}
		}
	}
	if(!i->i_size) {
		i->u.pipefs.i_readoff = 0;
		i->u.pipefs.i_writeoff = 0;
	}
	return bytes_read;
}

int pipefs_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	__off_t bytes_written;
	__size_t n;
	char *data;
	int limit;

	bytes_written = 0;
	data = i->u.pipefs.i_data;

	while(bytes_written < count) {
		/* if the read end closes then send signal and return */
		if(!i->u.pipefs.i_readers) {
			send_sig(current, SIGPIPE);
			return -EPIPE;
		}

		if(i->u.pipefs.i_readoff) {
			if(i->u.pipefs.i_writeoff <= i->u.pipefs.i_readoff) {
				limit = i->u.pipefs.i_readoff;
			} else {
				limit = PIPE_BUF;
			}
		} else {
			limit = PIPE_BUF;
		}

		n = MIN((count - bytes_written), (limit - i->u.pipefs.i_writeoff));

		/*
		 * POSIX requires that any write operation involving fewer than
		 * PIPE_BUF bytes must be automatically executed and finished
		 * without being interleaved with write operations of other
		 * processes to the same pipe.
		*/
		if(n && n <= PIPE_BUF) {
			lock_resource(&pipe_resource);
			memcpy_b(data + i->u.pipefs.i_writeoff, buffer + bytes_written, n);
			bytes_written += n;
			i->u.pipefs.i_writeoff += n;
			i->i_size += n;
			if(i->u.pipefs.i_readoff >= PIPE_BUF) {
				i->u.pipefs.i_readoff = 0;
			}
			unlock_resource(&pipe_resource);
			wakeup(&pipefs_read);
			wakeup(&do_select);
			continue;
		}

		wakeup(&pipefs_read);
		wakeup(&do_select);
		if(!(fd_table->flags & O_NONBLOCK)) {
			if(sleep(&pipefs_write, PROC_INTERRUPTIBLE)) {
				return -EINTR;
			}
		} else {
			return -EAGAIN;
		}
	}
	return bytes_written;
}

int pipefs_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	int errno;

	switch(cmd) {
		case FIONREAD:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int)))) {
				return errno;
			}
			memcpy_b((void *)arg, &i->i_size, sizeof(unsigned int));
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int pipefs_lseek(struct inode *i, __off_t offset)
{
	return -ESPIPE;
}

int pipefs_select(struct inode *i, int flag)
{
	switch(flag) {
		case SEL_R:
			/*
			 * if !i->i_size && !i->u.pipefs.i_writers
			 * should also return 1?
			 */
			if(i->i_size || !i->u.pipefs.i_writers) {
				return 1;
			}
			break;
		case SEL_W:
			/*
			 * if i->i_size == PIPE_BUF && !i->u.pipefs.i_readers
			 * should also return 1?
			 */
			if(i->i_size < PIPE_BUF || !i->u.pipefs.i_readers) {
				return 1;
			}
			break;
	}
	return 0;
}
