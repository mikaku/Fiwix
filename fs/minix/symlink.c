/*
 * fiwix/fs/minix/symlink.c
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
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations minix_symlink_fsop = {
	0,
	0,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* lseek */
	NULL,			/* readdir */
	NULL,			/* mmap */
	NULL,			/* select */

	minix_readlink,
	minix_followlink,
	NULL,			/* bmap */
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

int minix_readlink(struct inode *i, char *buffer, __size_t count)
{
	__u32 blksize;
	struct buffer *buf;

	if(!S_ISLNK(i->i_mode)) {
		printk("%s(): Oops, inode '%d' is not a symlink (!?).\n", __FUNCTION__, i->inode);
		return 0;
	}

	inode_lock(i);
	blksize = i->sb->s_blocksize;
	count = MIN(count, i->i_size);
	if(!count) {
		inode_unlock(i);
		return 0;
	}
	count = MIN(count, blksize);
	if(i->sb->u.minix.version == 1) {
		if(!(buf = bread(i->dev, i->u.minix.u.i1_zone[0], blksize))) {
			inode_unlock(i);
			return -EIO;
		}
	} else {
		if(!(buf = bread(i->dev, i->u.minix.u.i2_zone[0], blksize))) {
			inode_unlock(i);
			return -EIO;
		}
	}
	memcpy_b(buffer, buf->data, count);
	brelse(buf);
	buffer[count] = 0;
	inode_unlock(i);
	return count;
}

int minix_followlink(struct inode *dir, struct inode *i, struct inode **i_res)
{
	struct buffer *buf;
	char *name;
	__ino_t errno;

	if(!i) {
		return -ENOENT;
	}

	if(!S_ISLNK(i->i_mode)) {
		printk("%s(): Oops, inode '%d' is not a symlink (!?).\n", __FUNCTION__, i->inode);
		return 0;
	}

	if(current->loopcnt > MAX_SYMLINKS) {
		printk("%s(): too many nested symbolic links!\n", __FUNCTION__);
		return -ELOOP;
	}

	inode_lock(i);
	if(i->sb->u.minix.version == 1) {
		if(!(buf = bread(i->dev, i->u.minix.u.i1_zone[0], i->sb->s_blocksize))) {
			inode_unlock(i);
			return -EIO;
		}
	} else {
		if(!(buf = bread(i->dev, i->u.minix.u.i2_zone[0], i->sb->s_blocksize))) {
			inode_unlock(i);
			return -EIO;
		}
	}
	name = buf->data;
	inode_unlock(i);

	current->loopcnt++;
	iput(i);
	brelse(buf);
	errno = parse_namei(name, dir, i_res, NULL, FOLLOW_LINKS);
	current->loopcnt--;
	return errno;
}
