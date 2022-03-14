/*
 * fiwix/kernel/syscalls/fchmod.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_fchmod(unsigned int ufd, __mode_t mode)
{
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_fchmod(%d, %d)\n", current->pid, ufd, mode);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	i = fd_table[current->fd[ufd]].inode;

	if(IS_RDONLY_FS(i)) {
		return -EROFS;
	}
	if(check_user_permission(i)) {
		return -EPERM;
	}

	i->i_mode &= S_IFMT;
	i->i_mode |= mode & ~S_IFMT;
	i->i_ctime = CURRENT_TIME;
	i->dirty = 1;
	return 0;
}
