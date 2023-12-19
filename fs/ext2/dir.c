/*
 * fiwix/fs/ext2/dir.c
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

struct fs_operations ext2_dir_fsop = {
	0,
	0,

	ext2_dir_open,
	ext2_dir_close,
	ext2_dir_read,
	ext2_dir_write,
	NULL,			/* ioctl */
	NULL,			/* llseek */
	ext2_dir_readdir,
	ext2_dir_readdir64,
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	ext2_bmap,
	ext2_lookup,
	ext2_rmdir,
	ext2_link,
	ext2_unlink,
	ext2_symlink,
	ext2_mkdir,
	ext2_mknod,
	NULL,			/* truncate */
	ext2_create,
	ext2_rename,

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

int ext2_dir_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	return -EBADF;
}

int ext2_dir_readdir(struct inode *i, struct fd *fd_table, struct dirent *dirent, __size_t count)
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
				d = (struct ext2_dir_entry_2 *)(buf->data + offset);
				if(d->inode) {
					dirent_len = (base_dirent_len + (d->name_len + 1)) + 3;
					dirent_len &= ~3;	/* round up */
					dirent->d_ino = d->inode;
					if((size + dirent_len) < count) {
						dirent->d_off = doffset;
						dirent->d_reclen = dirent_len;
						memcpy_b(dirent->d_name, d->name, d->name_len);
						dirent->d_name[d->name_len] = 0;
						dirent = (struct dirent *)((char *)dirent + dirent_len);
						size += dirent_len;
						count -= dirent_len;
					} else {
						count = 0;
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
		fd_table->offset += offset;
	}

	return size;
}

int ext2_dir_readdir64(struct inode *i, struct fd *fd_table, struct dirent64 *dirent, unsigned int count)
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

	base_dirent_len = sizeof(dirent->d_ino) + sizeof(dirent->d_off) + sizeof(dirent->d_reclen) + sizeof(dirent->d_type);
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
				d = (struct ext2_dir_entry_2 *)(buf->data + offset);
				if(d->inode) {
					dirent_len = (base_dirent_len + (d->name_len + 1)) + 3;
					dirent_len &= ~3;	/* round up */
					dirent->d_ino = d->inode;
					if((size + dirent_len) < count) {
						struct inode *dirent_inode = iget(i->sb, dirent->d_ino);
						dirent->d_off = doffset;
						dirent->d_reclen = dirent_len;
						memcpy_b(dirent->d_name, d->name, d->name_len);
						dirent->d_name[d->name_len] = 0;
						if (S_ISREG(dirent_inode->i_mode)) {
							dirent->d_type = DT_REG;
						} else if (S_ISDIR(dirent_inode->i_mode)) {
							dirent->d_type = DT_DIR;
						} else if (S_ISCHR(dirent_inode->i_mode)) {
							dirent->d_type = DT_CHR;
						} else if (S_ISBLK(dirent_inode->i_mode)) {
							dirent->d_type = DT_BLK;
						} else if (S_ISFIFO(dirent_inode->i_mode)) {
							dirent->d_type = DT_FIFO;
						} else if (S_ISSOCK(dirent_inode->i_mode)) {
							dirent->d_type = DT_SOCK;
						} else if (S_ISLNK(dirent_inode->i_mode)) {
							dirent->d_type = DT_LNK;
						} else {
							dirent->d_type = DT_UNKNOWN;
						}
						iput(dirent_inode);
						dirent = (struct dirent64 *)((char *)dirent + dirent_len);
						size += dirent_len;
						count -= dirent_len;
					} else {
						count = 0;
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
		fd_table->offset += offset;
	}

	return size;
}
