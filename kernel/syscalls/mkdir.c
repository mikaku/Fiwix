/*
 * fiwix/kernel/syscalls/mkdir.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_mkdir(const char *dirname, __mode_t mode)
{
	struct inode *i, *dir;
	char *tmp_dirname, *basename;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_mkdir('%s', %o)\n", current->pid, dirname, mode);
#endif /*__DEBUG__ */

	if((errno = malloc_name(dirname, &tmp_dirname)) < 0) {
		return errno;
	}
	basename = remove_trailing_slash(tmp_dirname);
	if((errno = namei(basename, &i, &dir, !FOLLOW_LINKS))) {
		if(!dir) {
			free_name(tmp_dirname);
			return errno;
		}
	}
	if(!errno) {
		iput(i);
		iput(dir);
		free_name(tmp_dirname);
		return -EEXIST;
	}
	if(IS_RDONLY_FS(dir)) {
		iput(dir);
		free_name(tmp_dirname);
		return -EROFS;
	}

	if(check_permission(TO_EXEC | TO_WRITE, dir) < 0) {
		iput(dir);
		free_name(tmp_dirname);
		return -EACCES;
	}

	basename = get_basename(basename);
	if(dir->fsop && dir->fsop->mkdir) {
		errno = dir->fsop->mkdir(dir, basename, mode);
	} else {
		errno = -EPERM;
	}
	iput(dir);
	free_name(tmp_dirname);
	return errno;
}
