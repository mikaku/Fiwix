/*
 * fiwix/fs/devpts/super.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_devpts.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_UNIX98_PTYS
struct devpts_files *devpts_list;

struct fs_operations devpts_fsop = {
	0,
	DEVPTS_DEV,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL, 			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
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

	devpts_read_inode,
	NULL,			/* write_inode */
	devpts_ialloc,
	devpts_ifree,
	devpts_statfs,
	devpts_read_superblock,
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int devpts_ialloc(struct inode *i, int mode)
{
	struct superblock *sb = i->sb;
	int n;

	superblock_lock(sb);
	for(n = 0; n < NR_PTYS; n++) {
		if(!devpts_list[n].count) {
			devpts_list[n].count = 1;
			break;
		}
	}
	superblock_unlock(sb);
	if(n == NR_PTYS) {
		return -ENOSPC;
	}

	i->i_mode = mode | S_IRUSR | S_IWUSR;
	i->dev = sb->dev;
	i->rdev = MKDEV(0, 0);
	i->inode = DEVPTS_ROOT_INO + 1 + n;
	i->count = 1;
	i->fsop = &def_chr_fsop;
	i->i_uid = 0;
	i->i_size = 0;
	i->i_atime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->i_mtime = CURRENT_TIME;
	i->i_gid = 0;
	i->i_nlink = 1;
	i->i_blocks = 0;
	i->i_flags = 0;
	devpts_list[n].inode = i;
	return 0;
}

void devpts_ifree(struct inode *i)
{
	int n;

	for(n = 0; n < NR_PTYS; n++) {
		if(devpts_list[n].inode == i) {
			devpts_list[n].count = 0;
			memset_b(&devpts_list[n], 0, sizeof(struct devpts_files));
			return;
		}
	}
}

int devpts_read_superblock(__dev_t dev, struct superblock *sb)
{
	superblock_lock(sb);
	sb->dev = dev;
	sb->fsop = &devpts_fsop;
	sb->s_blocksize = BLKSIZE_1K;

	if(!(sb->root = iget(sb, DEVPTS_ROOT_INO))) {
		printk("WARNING: %s(): unable to get root inode.\n", __FUNCTION__);
		superblock_unlock(sb);
		return -EINVAL;
	}
	superblock_unlock(sb);
	return 0;
}

int devpts_init(void)
{
	if(!(devpts_list = (struct devpts_files *)kmalloc(sizeof(struct devpts_files) * NR_PTYS))) {
		return -ENOMEM;
	}
	memset_b(devpts_list, 0, sizeof(struct devpts_files) * NR_PTYS);
	return register_filesystem("devpts", &devpts_fsop);
}
#endif /* CONFIG_UNIX98_PTYS */
