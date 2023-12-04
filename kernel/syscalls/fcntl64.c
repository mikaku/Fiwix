/*
 * fiwix/kernel/syscalls/fcntl64.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Copyright 2023, Richard R. Masters.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/fcntl.h>
#include <fiwix/locks.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/process.h>

int sys_fcntl64(unsigned int ufd, int cmd, unsigned int arg)
{
	int new_ufd;

#ifdef __DEBUG__
	printk("(pid %d) sys_fcntl64(%d, %d, 0x%08x)\n", current->pid, ufd, cmd, arg);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	switch(cmd) {
		case F_DUPFD_CLOEXEC:
		case F_DUPFD:
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
		case F_GETLK64:
		case F_SETLK64:
		case F_SETLKW64:
			printk("(pid %d) sys_fcntl64: WARNING: locks not implemented!\n", current->pid);
			return 0;
		default:
			return -EINVAL;
	}
	return 0;
}
