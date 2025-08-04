/*
 * fiwix/fs/procfs/kmsg.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/stat.h>
#include <fiwix/fs.h>
#include <fiwix/fs_proc.h>
#include <fiwix/syslog.h>
#include <fiwix/syscalls.h>
#include <fiwix/string.h>

static int kmsg_open(struct inode *i, struct fd *f)
{
	return sys_syslog(SYSLOG_OPEN, NULL, 0);
}

static int kmsg_close(struct inode *i, struct fd *f)
{
	return sys_syslog(SYSLOG_CLOSE, NULL, 0);
}

static int kmsg_read(struct inode *i, struct fd *f, char *buffer, __size_t count)
{
	return sys_syslog(SYSLOG_READ, buffer, count);
}

static int kmsg_select(struct inode *i, struct fd *f, int flag)
{
	switch(flag) {
		case SEL_R:
			if(log_new_chars) {
				return 1;
			}
			break;
	}
	return 0;
}

struct fs_operations procfs_kmsg_fsop = {
	0,
	0,

	kmsg_open,
	kmsg_close,
	kmsg_read,
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	kmsg_select,

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
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
