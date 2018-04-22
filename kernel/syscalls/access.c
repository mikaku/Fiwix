/*
 * fiwix/kernel/syscalls/access.c
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

int sys_access(const char *filename, __mode_t mode)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_access('%s', %d)", current->pid, filename, mode);
#endif /*__DEBUG__ */

	if((mode & S_IRWXO) != mode) {
		return -EINVAL;
	}
	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	current->flags |= PF_USEREAL;
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		current->flags &= ~PF_USEREAL;
		free_name(tmp_name);
		return errno;
	}
	if(mode & TO_WRITE) {
		if(S_ISREG(i->i_mode) || S_ISDIR(i->i_mode) || S_ISLNK(i->i_mode)) {
			if(IS_RDONLY_FS(i)) {
				current->flags &= ~PF_USEREAL;
				iput(i);
				free_name(tmp_name);
				return -EROFS;
			}
		}
	}
	errno = check_permission(mode, i);

#ifdef __DEBUG__
	printk(" -> returning %d\n", errno);
#endif /*__DEBUG__ */

	current->flags &= ~PF_USEREAL;
	iput(i);
	free_name(tmp_name);
	return errno;
}
