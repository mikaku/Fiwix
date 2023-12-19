/*
 * fiwix/fs/iso9660/symlink.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations iso9660_symlink_fsop = {
	0,
	0,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	iso9660_readlink,
	iso9660_followlink,
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

int iso9660_readlink(struct inode *i, char *buffer, __size_t count)
{
	__off_t size_read;
	char *name;

	if(!(name = (char *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	inode_lock(i);
	name[0] = 0;
	if((size_read = get_rrip_symlink(i, name))) {
		size_read = MIN(size_read, count);
		memcpy_b(buffer, name, size_read);
	}
	kfree((unsigned int)name);
	inode_unlock(i);
	return size_read;
}

int iso9660_followlink(struct inode *dir, struct inode *i, struct inode **i_res)
{
	char *name;
	__ino_t errno;

	if(!i) {
		return -ENOENT;
	}
	if(!S_ISLNK(i->i_mode)) {
		printk("%s(): Oops, inode '%d' is not a symlink (!?).\n", __FUNCTION__, i->inode);
		return 0;
	}

	if(!(name = (char *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	name[0] = 0;
	if(get_rrip_symlink(i, name)) {
		iput(i);
		if((errno = parse_namei(name, dir, i_res, NULL, FOLLOW_LINKS))) {
			kfree((unsigned int)name);
			return errno;
		}
	}
	kfree((unsigned int)name);
	return 0;
}
