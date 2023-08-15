/*
 * fiwix/kernel/syscalls/fcntl.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/fcntl.h>
#include <fiwix/locks.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_fcntl(unsigned int ufd, int cmd, unsigned int arg)
{
	int new_ufd, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_fcntl(%d, %d, 0x%08x)\n", current->pid, ufd, cmd, arg);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	switch(cmd) {
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
			if(arg >= OPEN_MAX) {
				return -EINVAL;
			}
			if((new_ufd = get_new_user_fd(arg)) < 0) {
				return new_ufd;
			}
			current->fd[new_ufd] = current->fd[ufd];
			if (cmd == F_DUPFD_CLOEXEC) {
			    current->fd_flags[new_ufd] |= FD_CLOEXEC;
			}
			fd_table[current->fd[new_ufd]].count++;
#ifdef __DEBUG__
			printk("\t--> returning %d\n", new_ufd);
#endif /*__DEBUG__ */
			return new_ufd;
		case F_GETFD:
			return (current->fd_flags[ufd] & FD_CLOEXEC);
		case F_SETFD:
			current->fd_flags[ufd] = (arg & FD_CLOEXEC);
			break;
		case F_GETFL:
			return fd_table[current->fd[ufd]].flags;
		case F_SETFL:
			fd_table[current->fd[ufd]].flags &= ~(O_APPEND | O_NONBLOCK);
			fd_table[current->fd[ufd]].flags |= arg & (O_APPEND | O_NONBLOCK);
			break;
		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct flock)))) {
				return errno;
			}
			return posix_lock(ufd, cmd, (struct flock *)arg);
		default:
			return -EINVAL;
	}
	return 0;
}
