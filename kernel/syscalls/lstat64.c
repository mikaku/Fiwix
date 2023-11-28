/*
 * fiwix/kernel/syscalls/lstat64.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Copyright 2023, Richard R. Masters.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/statbuf.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_lstat64(const char *filename, struct stat64 *statbuf)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_lstat64('%s', 0x%08x) -> returning structure\n", current->pid, filename, (unsigned int )statbuf);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, statbuf, sizeof(struct stat64)))) {
		return errno;
	}
	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, !FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	statbuf->st_dev = i->dev;
	statbuf->__st_dev_padding = 0;
	statbuf->st_ino = i->inode;
	statbuf->st_mode = i->i_mode;
	statbuf->st_nlink = i->i_nlink;
	statbuf->st_uid = i->i_uid;
	statbuf->st_gid = i->i_gid;
	statbuf->st_rdev = i->rdev;
	statbuf->__st_rdev_padding = 0;
	statbuf->st_size = i->i_size;
	statbuf->st_blksize = i->sb->s_blocksize;
	statbuf->st_blocks = i->i_blocks;
	if(!i->i_blocks) {
		statbuf->st_blocks = (i->i_size / i->sb->s_blocksize) * 2;
		statbuf->st_blocks++;
	}
	statbuf->st_atime = i->i_atime;
	statbuf->st_mtime = i->i_mtime;
	statbuf->st_ctime = i->i_ctime;
	iput(i);
	free_name(tmp_name);
	return 0;
}
