/*
 * fiwix/kernel/syscalls/newfstat.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/statbuf.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_newfstat(unsigned int ufd, struct new_stat *statbuf)
{
	struct inode *i;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_newfstat(%d, 0x%08x) -> returning structure\n", current->pid, ufd, (unsigned int )statbuf);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	if((errno = check_user_area(VERIFY_WRITE, statbuf, sizeof(struct new_stat)))) {
		return errno;
	}
	i = fd_table[current->fd[ufd]].inode;
	statbuf->st_dev = i->dev;
	statbuf->__pad1 = 0;
	statbuf->st_ino = i->inode;
	statbuf->st_mode = i->i_mode;
	statbuf->st_nlink = i->i_nlink;
	statbuf->st_uid = i->i_uid;
	statbuf->st_gid = i->i_gid;
	statbuf->st_rdev = i->rdev;
	statbuf->__pad2 = 0;
	statbuf->st_size = i->i_size;
	statbuf->st_blksize = i->sb->s_blocksize;
	statbuf->st_blocks = i->i_blocks;
	if(!i->i_blocks) {
		statbuf->st_blocks = (i->i_size / i->sb->s_blocksize * 2);
		statbuf->st_blocks++;
	}
	statbuf->st_atime = i->i_atime;
	statbuf->__unused1 = 0;
	statbuf->st_mtime = i->i_mtime;
	statbuf->__unused2 = 0;
	statbuf->st_ctime = i->i_ctime;
	statbuf->__unused3 = 0;
	statbuf->__unused4 = 0;
	statbuf->__unused5 = 0;
	return 0;
}
