/*
 * fiwix/fs/pipefs/super.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static unsigned int i_counter;

struct fs_operations pipefs_fsop = {
	FSOP_KERN_MOUNT,
	PIPE_DEV,

	fifo_open,
	pipefs_close,
	pipefs_read,
	pipefs_write,
	pipefs_ioctl,
	pipefs_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	pipefs_select,

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
	pipefs_ialloc,
	pipefs_ifree,
	NULL,			/* statfs */
	pipefs_read_superblock,
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int pipefs_ialloc(struct inode *i, int mode)
{
	struct superblock *sb = i->sb;

	superblock_lock(sb);
	i_counter++;
	superblock_unlock(sb);

	i->i_mode = mode;
	i->dev = i->rdev = sb->dev;
	i->fsop = &pipefs_fsop;
	i->inode = i_counter;
	i->count = 2;
	if(!(i->u.pipefs.i_data = (void *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}
	i->u.pipefs.i_readoff = 0;
	i->u.pipefs.i_writeoff = 0;
	i->u.pipefs.i_readers = 1;
	i->u.pipefs.i_writers = 1;
	return 0;
}

void pipefs_ifree(struct inode *i)
{
	if(!i->u.pipefs.i_readers && !i->u.pipefs.i_writers) {
		/*
		 * We need to ask before to kfree() because this function is
		 * also called to free removed (with sys_unlink) fifo files.
		 */
		if(i->u.pipefs.i_data) {
			kfree((unsigned int)i->u.pipefs.i_data);
		}
	}
}

int pipefs_read_superblock(__dev_t dev, struct superblock *sb)
{
	superblock_lock(sb);
	sb->dev = dev;
	sb->fsop = &pipefs_fsop;
	sb->s_blocksize = BLKSIZE_1K;
	i_counter = 0;
	superblock_unlock(sb);
	return 0;
}

int pipefs_init(void)
{
	return register_filesystem("pipefs", &pipefs_fsop);
}
