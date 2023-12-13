/*
 * fiwix/fs/sockfs/super.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_sock.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/sched.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
static unsigned int i_counter;

struct fs_operations sockfs_fsop = {
	FSOP_KERN_MOUNT,
	SOCK_DEV,

	sockfs_open,
	sockfs_close,
	sockfs_read,
	sockfs_write,
	NULL, 			/* ioctl */
	sockfs_llseek,
	NULL,			/* readdir */
	NULL,			/* mmap */
	sockfs_select,

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

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	sockfs_ialloc,
	sockfs_ifree,
	NULL,			/* statfs */
	sockfs_read_superblock,
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int sockfs_read_superblock(__dev_t dev, struct superblock *sb)
{
	superblock_lock(sb);
	sb->dev = dev;
	sb->fsop = &sockfs_fsop;
	sb->s_blocksize = BLKSIZE_1K;
	i_counter = 0;
	superblock_unlock(sb);
	return 0;
}

int sockfs_ialloc(struct inode *i, int mode)
{
	struct superblock *sb = i->sb;

	superblock_lock(sb);
	i_counter++;
	superblock_unlock(sb);

	i->i_mode = mode;
	i->dev = i->rdev = sb->dev;
	i->fsop = &sockfs_fsop;
	i->inode = i_counter;
	i->count = 1;
	/*
	if(!(i->u.sockfs.sock = (struct socket *)kmalloc(sizeof(struct socket)))) {
		return -ENOMEM;
	}
	*/
	memset_b(&i->u.sockfs.sock, 0, sizeof(struct socket));
	return 0;
}

void sockfs_ifree(struct inode *i)
{
	/*
	if(i->u.sockfs.sock) {
		kfree((unsigned int)i->u.sockfs.sock);
	}
	*/
}

int sockfs_init(void)
{
	return register_filesystem("sockfs", &sockfs_fsop);
}
#endif /* CONFIG_NET */
