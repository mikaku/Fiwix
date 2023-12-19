/*
 * fiwix/kernel/syscalls/getdents64.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Copyright 2023, Richard R. Masters.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/dirent.h>
#include <fiwix/process.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getdents64(unsigned int ufd, struct dirent64 *dirent, unsigned int count)
{
	struct inode *i;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_getdents64(%d, 0x%08x, %d)", current->pid, ufd, (unsigned int)dirent, count);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	if((errno = check_user_area(VERIFY_WRITE, dirent, sizeof(struct dirent64)))) {
		return errno;
	}
	i = fd_table[current->fd[ufd]].inode;

	if(!S_ISDIR(i->i_mode)) {
		return -ENOTDIR;
	}

	if(i->fsop && i->fsop->readdir64) {
		errno = i->fsop->readdir64(i, &fd_table[current->fd[ufd]], dirent, count);
	#ifdef __DEBUG__
		printk(" -> returning %d\n", errno);
	#endif /*__DEBUG__ */
		return errno;
	}
	return -EINVAL;
}
