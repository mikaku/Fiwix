/*
 * fiwix/kernel/syscalls/dup.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/syscalls.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_dup(unsigned int ufd)
{
	int new_ufd;

#ifdef __DEBUG__
	printk("(pid %d) sys_dup(%d)", current->pid, ufd);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	if((new_ufd = get_new_user_fd(0)) < 0) {
		return new_ufd;
	}

#ifdef __DEBUG__
	printk(" -> %d\n", new_ufd);
#endif /*__DEBUG__ */

	current->fd[new_ufd] = current->fd[ufd];
	fd_table[current->fd[new_ufd]].count++;
	return new_ufd;
}
