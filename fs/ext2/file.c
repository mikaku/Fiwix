/*
 * fiwix/fs/ext2/file.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/fcntl.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations ext2_file_fsop = {
	0,
	0,

	ext2_file_open,
	ext2_file_close,
	file_read,
	NULL,			/* write */
	NULL,			/* ioctl */
	ext2_file_lseek,
	NULL,			/* readdir */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	ext2_bmap,
	NULL,			/* lookup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int ext2_file_open(struct inode *i, struct fd *fd_table)
{
	if(fd_table->flags & (O_WRONLY | O_RDWR | O_TRUNC | O_APPEND)) {
		return -ENOENT;
	}
	fd_table->offset = 0;
	return 0;
}

int ext2_file_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int ext2_file_lseek(struct inode *i, __off_t offset)
{
	return offset;
}
