/*
 * fiwix/kernel/syscalls/rmdir.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_rmdir(const char *dirname)
{
	struct inode *i, *dir;
	char *tmp_dirname;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_rmdir(%s)\n", current->pid, dirname);
#endif /*__DEBUG__ */

	if((errno = malloc_name(dirname, &tmp_dirname)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_dirname, &i, &dir, !FOLLOW_LINKS))) {
		if(dir) {
			iput(dir);
		}
		free_name(tmp_dirname);
		return errno;
	}
	free_name(tmp_dirname);

	if(!S_ISDIR(i->i_mode)) {
		iput(i);
		iput(dir);
		return -ENOTDIR;
	}
	if(i == current->root || i->mount_point) {
		iput(i);
		iput(dir);
		return -EBUSY;
	}
	if(IS_RDONLY_FS(i)) {
		iput(i);
		iput(dir);
		return -EROFS;
	}
	if(i == dir) {
		iput(i);
		iput(dir);
		return -EPERM;
	}
	if(check_permission(TO_EXEC | TO_WRITE, dir) < 0) {
		iput(i);
		iput(dir);
		return -EACCES;
	}

	/* check sticky permission bit */
	if(dir->i_mode & S_ISVTX) {
		if(check_user_permission(i)) {
			iput(i);
			iput(dir);
			return -EPERM;
		}
	}

	if(i->fsop && i->fsop->rmdir) {
		errno = i->fsop->rmdir(dir, i);
	} else {
		errno = -EPERM;
	}
	iput(i);
	iput(dir);
	return errno;
}
