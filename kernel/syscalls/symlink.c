/*
 * fiwix/kernel/syscalls/symlink.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_symlink(const char *oldpath, const char *newpath)
{
	struct inode *i, *dir;
	char *tmp_oldpath, *tmp_newpath, *basename;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_symlink('%s', '%s')\n", current->pid, oldpath, newpath);
#endif /*__DEBUG__ */

	if((errno = malloc_name(oldpath, &tmp_oldpath)) < 0) {
		return errno;
	}
	if((errno = malloc_name(newpath, &tmp_newpath)) < 0) {
		free_name(tmp_oldpath);
		return errno;
	}
	basename = get_basename(tmp_newpath);
	if((errno = namei(tmp_newpath, &i, &dir, !FOLLOW_LINKS))) {
		if(!dir) {
			free_name(tmp_oldpath);
			free_name(tmp_newpath);
			return errno;
		}
	}
	if(!errno) {
		iput(i);
		iput(dir);
		free_name(tmp_oldpath);
		free_name(tmp_newpath);
		return -EEXIST;
	}
	if(IS_RDONLY_FS(dir)) {
		iput(dir);
		free_name(tmp_oldpath);
		free_name(tmp_newpath);
		return -EROFS;
	}

	if(check_permission(TO_EXEC | TO_WRITE, dir) < 0) {
		iput(dir);
		free_name(tmp_oldpath);
		free_name(tmp_newpath);
		return -EACCES;
	}

	if(dir->fsop && dir->fsop->symlink) {
		errno = dir->fsop->symlink(dir, basename, tmp_oldpath);
	} else {
		errno = -EPERM;
	}
	iput(dir);
	free_name(tmp_oldpath);
	free_name(tmp_newpath);
	return errno;
}
