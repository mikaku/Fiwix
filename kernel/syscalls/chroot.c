/*
 * fiwix/kernel/syscalls/chroot.c
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

int sys_chroot(const char *dirname)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_chroot('%s')\n", current->pid, dirname);
#endif /*__DEBUG__ */

	if((errno = malloc_name(dirname, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	if(!S_ISDIR(i->i_mode)) {
		iput(i);
		free_name(tmp_name);
		return -ENOTDIR;
	}
	iput(current->root);
	current->root = i;
	free_name(tmp_name);
	return 0;
}
