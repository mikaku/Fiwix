/*
 * fiwix/fs/ext2/file.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
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
	ext2_file_write,
	NULL,			/* ioctl */
	ext2_file_llseek,
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
	ext2_truncate,
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
	if(fd_table->flags & O_APPEND) {
		fd_table->offset = i->i_size;
	} else {
		fd_table->offset = 0;
	}
	if(fd_table->flags & O_TRUNC) {
		i->i_size = 0;
		ext2_truncate(i, 0);
	}
	return 0;
}

int ext2_file_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int ext2_file_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	__blk_t block;
	__size_t total_written;
	unsigned int boffset, bytes;
	int blksize;
	struct buffer *buf;

	inode_lock(i);

	blksize = i->sb->s_blocksize;
	total_written = 0;

	if(fd_table->flags & O_APPEND) {
		fd_table->offset = i->i_size;
	}

	while(total_written < count) {
		boffset = fd_table->offset % blksize;
		if((block = bmap(i, fd_table->offset, FOR_WRITING)) < 0) {
			inode_unlock(i);
			return block;
		}
		bytes = blksize - boffset;
		bytes = MIN(bytes, (count - total_written));
		if(!(buf = bread(i->dev, block, blksize))) {
			inode_unlock(i);
			return -EIO;
		}
		memcpy_b(buf->data + boffset, buffer + total_written, bytes);
		update_page_cache(i, fd_table->offset, buffer + total_written, bytes);
		bwrite(buf);
		total_written += bytes;
		fd_table->offset += bytes;
	}

	if(fd_table->offset > i->i_size) {
		i->i_size = fd_table->offset;
	}
	i->i_ctime = CURRENT_TIME;
	i->i_mtime = CURRENT_TIME;
	i->state |= INODE_DIRTY;
	
	inode_unlock(i);
	return total_written;
}

__loff_t ext2_file_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}
