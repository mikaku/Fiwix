/*
 * fiwix/fs/devpts/inode.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_devpts.h>
#include <fiwix/statfs.h>
#include <fiwix/stat.h>
#include <fiwix/stdio.h>

#ifdef CONFIG_UNIX98_PTYS
int devpts_read_inode(struct inode *i)
{
	__mode_t mode;
	__nlink_t nlink;

	if(i->inode == DEVPTS_ROOT_INO) {
		mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
		nlink = 2;
		i->fsop = &devpts_dir_fsop;
	} else {
		mode = S_IFCHR | S_IRUSR | S_IWUSR;
		nlink = 1;
		i->fsop = &def_chr_fsop;
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
	i->state = INODE_LOCKED;
	i->count = 1;
	return 0;
}

void devpts_statfs(struct superblock *sb, struct statfs *statfsbuf)
{
	statfsbuf->f_type = DEVPTS_SUPER_MAGIC;
	statfsbuf->f_bsize = sb->s_blocksize;
	statfsbuf->f_blocks = 0;
	statfsbuf->f_bfree = 0;
	statfsbuf->f_bavail = 0;
	statfsbuf->f_files = 0;
	statfsbuf->f_ffree = 0;
	/* statfsbuf->f_fsid = ? */
	statfsbuf->f_namelen = NAME_MAX;
}
#endif /* CONFIG_UNIX98_PTYS */
