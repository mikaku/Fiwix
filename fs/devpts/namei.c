/*
 * fiwix/fs/devpts/namei.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_devpts.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_UNIX98_PTYS
extern struct devpts_files *devpts_list;

int devpts_lookup(const char *name, struct inode *dir, struct inode **i_res)
{
	__ino_t inode;
	int numpty;

	if(name[0] == '.' && name[1] == '\0') {
		*i_res = dir;
		return 0;
	} else if(name[0] == '.' && name[1] == '.') {
		return 0;
	}

	if((numpty = atoi(name)) >= NR_PTYS) {
		iput(dir);
		return -ENOENT;
	}
	if(devpts_list[numpty].count) {
		inode = devpts_list[numpty].inode->inode;
		if(!(*i_res = iget(dir->sb, inode))) {
			iput(dir);
			return -EACCES;
		}
		iput(dir);
		return 0;
	}
	iput(dir);
	return -ENOENT;
}
#endif /* CONFIG_UNIX98_PTYS */
