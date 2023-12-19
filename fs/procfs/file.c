/*
 * fiwix/fs/procfs/file.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_proc.h>
#include <fiwix/fcntl.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations procfs_file_fsop = {
	0,
	0,

	procfs_file_open,
	procfs_file_close,
	procfs_file_read,
	NULL,			/* write */
	NULL,			/* ioctl */
	procfs_file_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	procfs_bmap,
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

int procfs_file_open(struct inode *i, struct fd *fd_table)
{
	if(fd_table->flags & (O_WRONLY | O_RDWR | O_TRUNC | O_APPEND)) {
		return -EINVAL;
	}
	fd_table->offset = 0;
	return 0;
}

int procfs_file_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int procfs_file_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	__size_t total_read;
	unsigned int boffset, bytes, size;
	int blksize;
	struct procfs_dir_entry *d;
	char *buf;

	if(!(d = get_procfs_by_inode(i))) {
		return -EINVAL;
	}
	if(!d->data_fn) {
		return -EINVAL;
	}
	if(!(buf = (void *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	size = d->data_fn(buf, (i->inode >> 12) & 0xFFFF);
	blksize = i->sb->s_blocksize;
	if(fd_table->offset > size) {
		fd_table->offset = size;
	}

	total_read = 0;

	for(;;) {
		count = (fd_table->offset + count > size) ? size - fd_table->offset : count;
		if(!count) {
			break;
		}

		boffset = fd_table->offset % blksize;
		bytes = blksize - boffset;
		bytes = MIN(bytes, count);
		memcpy_b(buffer + total_read, buf + boffset, bytes);
		total_read += bytes;
		count -= bytes;
		fd_table->offset += bytes;
	}

	kfree((unsigned int)buf);
	return total_read;
}

__loff_t procfs_file_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}
