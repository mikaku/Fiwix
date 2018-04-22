/*
 * fiwix/fs/minix/inode.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_minix.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/statfs.h>
#include <fiwix/sleep.h>
#include <fiwix/stat.h>
#include <fiwix/sched.h>
#include <fiwix/buffer.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int minix_read_inode(struct inode *i)
{
	if(i->sb->u.minix.version == 1) {
		return v1_minix_read_inode(i);
	}

	return v2_minix_read_inode(i);
}

int minix_write_inode(struct inode *i)
{
	if(i->sb->u.minix.version == 1) {
		return v1_minix_write_inode(i);
	}

	return v2_minix_write_inode(i);
}

int minix_ialloc(struct inode *i)
{
	if(i->sb->u.minix.version == 1) {
		return v1_minix_ialloc(i);
	}

	return v2_minix_ialloc(i);
}

void minix_ifree(struct inode *i)
{
	if(i->sb->u.minix.version == 1) {
		return v1_minix_ifree(i);
	}

	return v2_minix_ifree(i);
}

int minix_bmap(struct inode *i, __off_t offset, int mode)
{
	if(i->sb->u.minix.version == 1) {
		return v1_minix_bmap(i, offset, mode);
	}

	return v2_minix_bmap(i, offset, mode);
}

int minix_truncate(struct inode *i, __off_t length)
{
	if(i->sb->u.minix.version == 1) {
		return v1_minix_truncate(i, length);
	}

	return v2_minix_truncate(i, length);
}
