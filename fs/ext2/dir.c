/*
 * fiwix/fs/ext2/dir.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/stat.h>
#include <fiwix/dirent.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations ext2_dir_fsop = {
	0,
	0,

	ext2_dir_open,
	ext2_dir_close,
	ext2_dir_read,
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* lseek */
	ext2_dir_readdir,
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	ext2_bmap,
	ext2_lookup,
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

int ext2_dir_open(struct inode *i, struct fd *fd_table)
{
	fd_table->offset = 0;
	return 0;
}

int ext2_dir_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int ext2_dir_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	return -EISDIR;
}

int ext2_dir_readdir(struct inode *i, struct fd *fd_table, struct dirent *dirent, unsigned int count)
{
	__blk_t block;
	unsigned int doffset, offset;
	unsigned int size, dirent_len;
	struct ext2_dir_entry_2 *d;
	int base_dirent_len;
	int blksize;
	struct buffer *buf;

	if(!(S_ISDIR(i->i_mode))) {
		return -EBADF;
	}

	blksize = i->sb->s_blocksize;
	if(fd_table->offset > i->i_size) {
		fd_table->offset = i->i_size;
	}

	base_dirent_len = sizeof(dirent->d_ino) + sizeof(dirent->d_off) + sizeof(dirent->d_reclen);
	doffset = offset = size = 0;

	while(doffset < count) {
		if((block = bmap(i, fd_table->offset, FOR_READING)) < 0) {
			return block;
		}
		if(block) {
			if(!(buf = bread(i->dev, block, blksize))) {
				return -EIO;
			}

			doffset = fd_table->offset;
			offset = fd_table->offset % blksize;
			while(doffset < i->i_size && offset < blksize) {
				d = (struct ext2_dir_entry_2 *)(buf->data + offset);
				if(d->inode) {
					dirent_len = (base_dirent_len + (d->name_len + 1)) + 3;
					dirent_len &= ~3;	/* round up */
					dirent->d_ino = d->inode;
					if((size + dirent_len) < count) {
						dirent->d_off = doffset;
						dirent->d_reclen = dirent_len;
						memcpy_b(dirent->d_name, d->name, d->name_len);
						dirent->d_name[d->name_len] = NULL;
						dirent = (struct dirent *)((char *)dirent + dirent_len);
						size += dirent_len;
					} else {
						break;
					}
				}
				doffset += d->rec_len;
				offset += d->rec_len;
				if(!d->rec_len) {
					break;
				}
			}
			brelse(buf);
		}
		fd_table->offset &= ~(blksize - 1);
		doffset = fd_table->offset;
		fd_table->offset += offset;
		doffset += blksize;
	}

	return size;
}
