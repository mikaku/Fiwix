/*
 * fiwix/kernel/syscalls/mknod.c
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

int do_mknod(char *pathname, __mode_t mode, __dev_t dev)
{
	struct inode *i, *dir;
	char *basename;
	int errno;

	basename = get_basename(pathname);
	if((errno = namei(pathname, &i, &dir, !FOLLOW_LINKS))) {
		if(!dir) {
			return errno;
		}
	}
	if(!errno) {
		iput(i);
		iput(dir);
		return -EEXIST;
	}
	if(IS_RDONLY_FS(dir)) {
		iput(dir);
		return -EROFS;
	}
	if(check_permission(TO_EXEC | TO_WRITE, dir) < 0) {
		iput(dir);
		return -EACCES;
	}

	if(dir->fsop && dir->fsop->mknod) {
		errno = dir->fsop->mknod(dir, basename, mode, dev);
	} else {
		errno = -EPERM;
	}
	iput(dir);
	return errno;
}

int sys_mknod(const char *pathname, __mode_t mode, __dev_t dev)
{
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_mknod('%s', %d, %x)\n", current->pid, pathname, mode, dev);
#endif /*__DEBUG__ */

	if(!S_ISCHR(mode) && !S_ISBLK(mode) && !S_ISFIFO(mode)) {
		return -EINVAL;
	}
	if(!S_ISFIFO(mode) && !IS_SUPERUSER) {
		return -EPERM;
	}

	if((errno = malloc_name(pathname, &tmp_name)) < 0) {
		return errno;
	}
	errno = do_mknod(tmp_name, mode, dev);
	free_name(tmp_name);
	return errno;
}
