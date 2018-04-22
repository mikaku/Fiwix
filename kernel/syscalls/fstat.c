/*
 * fiwix/kernel/syscalls/fstat.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/syscalls.h>
#include <fiwix/statbuf.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_fstat(unsigned int ufd, struct old_stat *statbuf)
{
	struct inode *i;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_fstat(%d, 0x%08x) -> returning structure\n", current->pid, ufd, (unsigned int )statbuf);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	if((errno = check_user_area(VERIFY_WRITE, statbuf, sizeof(struct old_stat)))) {
		return errno;
	}
	i = fd_table[current->fd[ufd]].inode;
	statbuf->st_dev = i->dev;
	statbuf->st_ino = i->inode;
	statbuf->st_mode = i->i_mode;
	statbuf->st_nlink = i->i_nlink;
	statbuf->st_uid = i->i_uid;
	statbuf->st_gid = i->i_gid;
	statbuf->st_rdev = i->rdev;
	statbuf->st_size = i->i_size;
	statbuf->st_atime = i->i_atime;
	statbuf->st_mtime = i->i_mtime;
	statbuf->st_ctime = i->i_ctime;
	return 0;
}
