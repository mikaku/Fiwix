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
#include <fiwix/blk_queue.h>

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
	NULL,			/* readdir64 */
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
	fd_table->offset = 0;
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
	int blksize, retval;
	struct buffer *buf;
	struct device *d;
	struct blk_request brh, *br, *tmp;
#ifdef CONFIG_OFFSET64
	__loff_t offset;
#else
	__off_t offset;
#endif /* CONFIG_OFFSET64 */

	inode_lock(i);

	blksize = i->sb->s_blocksize;
	retval = total_written = 0;

	if(fd_table->flags & O_APPEND) {
		fd_table->offset = i->i_size;
	}
	offset = fd_table->offset;

	if(count > blksize) {
		if(!(d = get_device(BLK_DEV, i->dev))) {
			printk("WARNING: %s(): device major %d not found!\n", __FUNCTION__, MAJOR(i->dev));
			inode_unlock(i);
			return -ENXIO;
		}
		memset_b(&brh, 0, sizeof(struct blk_request));
		tmp = NULL;
		while(total_written < count) {
			if(!(br = (struct blk_request *)kmalloc(sizeof(struct blk_request)))) {
				printk("WARNING: %s(): no more free memory for block requests.\n", __FUNCTION__);
				retval = -ENOMEM;
				break;
			}
			if((block = bmap(i, offset, FOR_WRITING)) < 0) {
				retval = block;
				break;
			}
			memset_b(br, 0, sizeof(struct blk_request));
			br->dev = i->dev;
			br->block = block;
			br->size = blksize;
			br->device = d;
			br->fn = d->fsop->read_block;
			br->head_group = &brh;
			if(!brh.next_group) {
				brh.next_group = br;
			} else {
				tmp->next_group = br;
			}
			tmp = br;
			boffset = offset & (blksize - 1);	/* mod blksize */
			bytes = blksize - boffset;
			bytes = MIN(bytes, (count - total_written));
			total_written += bytes;
			offset += bytes;
		}
		if(!retval) {
			retval = gbread(d, &brh);
		}
		br = brh.next_group;
		offset = fd_table->offset;
		total_written = 0;
		while(br) {
			if(!retval) {
				boffset = offset & (blksize - 1);	/* mod blksize */
				bytes = blksize - boffset;
				bytes = MIN(bytes, (count - total_written));
				memcpy_b(br->buffer->data + boffset, buffer + total_written, bytes);
				update_page_cache(i, offset, buffer + total_written, bytes);
				bwrite(br->buffer);
				total_written += bytes;
				offset += bytes;
			} else {
				if(br->buffer) {
					brelse(br->buffer);
				}
			}
			tmp = br->next_group;
			kfree((unsigned int)br);
			br = tmp;
		}
	} else {
		while(total_written < count) {
			boffset = offset & (blksize - 1);	/* mod blksize */
			if((block = bmap(i, offset, FOR_WRITING)) < 0) {
				retval = block;
				break;
			}
			bytes = blksize - boffset;
			bytes = MIN(bytes, (count - total_written));
			if(!(buf = bread(i->dev, block, blksize))) {
				retval = -EIO;
				break;
			}
			memcpy_b(buf->data + boffset, buffer + total_written, bytes);
			update_page_cache(i, offset, buffer + total_written, bytes);
			bwrite(buf);
			total_written += bytes;
			offset += bytes;
		}
	}

	if(!retval) {
		fd_table->offset = offset;
		if(fd_table->offset > i->i_size) {
			i->i_size = fd_table->offset;
		}
		i->i_ctime = CURRENT_TIME;
		i->i_mtime = CURRENT_TIME;
		i->state |= INODE_DIRTY;
	}

	inode_unlock(i);

	if(retval) {
		return retval;
	}
	return total_written;
}

__loff_t ext2_file_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}
