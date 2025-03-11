/*
 * fiwix/fs/minix/file.c
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

#ifdef CONFIG_FS_MINIX
struct fs_operations minix_file_fsop = {
	0,
	0,

	minix_file_open,
	minix_file_close,
	file_read,
	minix_file_write,
	NULL,			/* ioctl */
	minix_file_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	minix_bmap,
	NULL,			/* lookup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	minix_truncate,
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

int minix_file_open(struct inode *i, struct fd *f)
{
	f->offset = 0;
	if(f->flags & O_TRUNC) {
		i->i_size = 0;
		minix_truncate(i, 0);
	}
	return 0;
}

int minix_file_close(struct inode *i, struct fd *f)
{
	return 0;
}

int minix_file_write(struct inode *i, struct fd *f, const char *buffer, __size_t count)
{
	__blk_t block;
	__size_t total_written;
	unsigned int boffset, bytes;
	int blksize;
	struct buffer *buf;

	inode_lock(i);

	blksize = i->sb->s_blocksize;
	total_written = 0;

	if(f->flags & O_APPEND) {
		f->offset = i->i_size;
	}

	while(total_written < count) {
		boffset = f->offset & (blksize - 1);	/* mod blksize */
		if((block = bmap(i, f->offset, FOR_WRITING)) < 0) {
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
		update_page_cache(i, f->offset, buffer + total_written, bytes);
		bwrite(buf);
		total_written += bytes;
		f->offset += bytes;
	}

	if(f->offset > i->i_size) {
		i->i_size = f->offset;
	}
	i->i_ctime = CURRENT_TIME;
	i->i_mtime = CURRENT_TIME;
	i->state |= INODE_DIRTY;
	
	inode_unlock(i);
	return total_written;
}

__loff_t minix_file_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}
#endif /* CONFIG_FS_MINIX */
