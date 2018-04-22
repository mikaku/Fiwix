/*
 * fiwix/fs/procfs/inode.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_proc.h>
#include <fiwix/statfs.h>
#include <fiwix/sleep.h>
#include <fiwix/stat.h>
#include <fiwix/sched.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int procfs_read_inode(struct inode *i)
{
	int lev;
	__mode_t mode;
	__nlink_t nlink;
	struct procfs_dir_entry *d;

	if((i->inode & 0xF0000FFF) == PROC_PID_INO) {	/* dynamic PID dir */
		mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
		nlink = 3;
		lev = PROC_PID_LEV;
	} else {
		if(!(d = get_procfs_by_inode(i))) {
			return NULL;
		}
		mode = d->mode;
		nlink = d->nlink;
		lev = d->lev;
	}

	i->i_mode = mode;
	i->i_uid = 0;
	i->i_size = 0;
	i->i_atime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->i_mtime = CURRENT_TIME;
	i->i_gid = 0;
	i->i_nlink = nlink;
	i->i_blocks = 0;
	i->i_flags = 0;
	i->locked = 1;
	i->dirty = 0;
	i->mount_point = NULL;
	i->count = 1;
	i->u.procfs.i_lev = lev;
	switch(i->i_mode & S_IFMT) {
		case S_IFDIR:
			i->fsop = &procfs_dir_fsop;
			break;
		case S_IFREG:
			i->fsop = &procfs_file_fsop;
			break;
		case S_IFLNK:
			i->fsop = &procfs_symlink_fsop;
			break;
		default:
			PANIC("invalid inode (%d) mode %08o.\n", i->inode, i->i_mode);
	}
	return 0;
}

int procfs_bmap(struct inode *i, __off_t offset, int mode)
{
	return i->u.procfs.i_lev;
}

void procfs_statfs(struct superblock *sb, struct statfs *statfsbuf)
{
	statfsbuf->f_type = PROC_SUPER_MAGIC;
	statfsbuf->f_bsize = sb->s_blocksize;
	statfsbuf->f_blocks = 0;
	statfsbuf->f_bfree = 0;
	statfsbuf->f_bavail = 0;
	statfsbuf->f_files = 0;
	statfsbuf->f_ffree = 0;
	/* statfsbuf->f_fsid = ? */
	statfsbuf->f_namelen = NAME_MAX;
}
