/*
 * fiwix/kernel/syscalls/ftruncate.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/fcntl.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_ftruncate(int ufd, __off_t length)
{
	struct inode *i;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_ftruncate(%d, %d)\n", current->pid, ufd, length);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	i = fd_table[current->fd[ufd]].inode;
	if((fd_table[current->fd[ufd]].flags & O_ACCMODE) == O_RDONLY) {
		return -EINVAL;
	}
	if(S_ISDIR(i->i_mode)) {
		return -EISDIR;
	}
	if(IS_RDONLY_FS(i)) {
		return -EROFS;
	}
	if(check_permission(TO_WRITE, i) < 0) {
		return -EPERM;
	}
	if(length == i->i_size) {
		return 0;
	}

	errno = 0;
	if(i->fsop && i->fsop->truncate) {
		inode_lock(i);
		errno = i->fsop->truncate(i, length);
		inode_unlock(i);
	}
	return errno;
}
