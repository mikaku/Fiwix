/*
 * fiwix/kernel/syscalls/truncate64.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
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

int sys_truncate64(const char *path, __loff_t length)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_truncate64(%s, %llu)\n", current->pid, path, length);
#endif /*__DEBUG__ */

	if((errno = malloc_name(path, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	free_name(tmp_name);

	if(S_ISDIR(i->i_mode)) {
		iput(i);
		return -EISDIR;
	}
	if(IS_RDONLY_FS(i)) {
		iput(i);
		return -EROFS;
	}
	if(check_permission(TO_WRITE, i) < 0) {
		iput(i);
		return -EACCES;
	}
	if((__off_t)length == i->i_size) {
		iput(i);
		return 0;
	}

	if(i->fsop && i->fsop->truncate) {
		inode_lock(i);
		errno = i->fsop->truncate(i, (__off_t)length);
		inode_unlock(i);
		iput(i);
		return errno;
	}
	iput(i);
	return -EINVAL;
}
