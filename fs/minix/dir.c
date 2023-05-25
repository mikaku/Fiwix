/*
 * fiwix/fs/minix/dir.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
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

struct fs_operations minix_dir_fsop = {
	0,
	0,

	minix_dir_open,
	minix_dir_close,
	minix_dir_read,
	minix_dir_write,
	NULL,			/* ioctl */
	NULL,			/* llseek */
	minix_dir_readdir,
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	minix_bmap,
	minix_lookup,
	minix_rmdir,
	minix_link,
	minix_unlink,
	minix_symlink,
	minix_mkdir,
	minix_mknod,
	NULL,			/* truncate */
	minix_create,
	minix_rename,

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

int minix_dir_open(struct inode *i, struct fd *fd_table)
{
	fd_table->offset = 0;
	return 0;
}

int minix_dir_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int minix_dir_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	return -EISDIR;
}

int minix_dir_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	return -EBADF;
}

int minix_dir_readdir(struct inode *i, struct fd *fd_table, struct dirent *dirent, __size_t count)
{
	__blk_t block;
	unsigned int doffset, offset;
	unsigned int size, dirent_len;
	struct minix_dir_entry *d;
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
	offset = size = 0;

	while(fd_table->offset < i->i_size && count > 0) {
		if((block = bmap(i, fd_table->offset, FOR_READING)) < 0) {
			return block;
		}
		if(block) {
			if(!(buf = bread(i->dev, block, blksize))) {
				return -EIO;
			}

			doffset = fd_table->offset;
			offset = fd_table->offset % blksize;
			while(offset < blksize) {
				d = (struct minix_dir_entry *)(buf->data + offset);
				if(d->inode) {
					dirent_len = (base_dirent_len + (strlen(d->name) + 1)) + 3;
					dirent_len &= ~3;	/* round up */
					dirent->d_ino = d->inode;
					if((size + dirent_len) < count) {
						dirent->d_off = doffset;
						dirent->d_reclen = dirent_len;
						memcpy_b(dirent->d_name, d->name, strlen(d->name));
						dirent->d_name[strlen(d->name)] = 0;
						dirent = (struct dirent *)((char *)dirent + dirent_len);
						size += dirent_len;
						count -= dirent_len;
					} else {
						count = 0;
						break;
					}
				}
				doffset += i->sb->u.minix.dirsize;
				offset += i->sb->u.minix.dirsize;
			}
			brelse(buf);
		}
		fd_table->offset &= ~(blksize - 1);
		fd_table->offset += offset;
	}

	return size;
}
