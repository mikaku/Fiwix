/*
 * fiwix/kernel/syscalls/link.c
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

int sys_link(const char *oldname, const char *newname)
{
	struct inode *i, *dir, *i_new, *dir_new;
	char *tmp_oldname, *tmp_newname, *basename;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_link('%s', '%s')\n", current->pid, oldname, newname);
#endif /*__DEBUG__ */

	if((errno = malloc_name(oldname, &tmp_oldname)) < 0) {
		return errno;
	}
	if((errno = malloc_name(newname, &tmp_newname)) < 0) {
		free_name(tmp_oldname);
		return errno;
	}

	if((errno = namei(tmp_oldname, &i, &dir, !FOLLOW_LINKS))) {
		if(dir) {
			iput(dir);
		}
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return errno;
	}
	if(S_ISDIR(i->i_mode)) {
		iput(i);
		iput(dir);
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return -EPERM;
	}
	if(IS_RDONLY_FS(i)) {
		iput(i);
		iput(dir);
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return -EROFS;
	}
	if(i->i_nlink == LINK_MAX) {
		iput(i);
		iput(dir);
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return -EMLINK;
	}

	basename = get_basename(tmp_newname);
	if((errno = namei(tmp_newname, &i_new, &dir_new, !FOLLOW_LINKS))) {
		if(!dir_new) {
			iput(i);
			iput(dir);
			free_name(tmp_oldname);
			free_name(tmp_newname);
			return errno;
		}
	}
	if(!errno) {
		iput(i);
		iput(dir);
		iput(i_new);
		iput(dir_new);
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return -EEXIST;
	}
	if(i->dev != dir_new->dev) {
		iput(i);
		iput(dir);
		iput(dir_new);
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return -EXDEV;
	}
	if(check_permission(TO_EXEC | TO_WRITE, dir_new) < 0) {
		iput(i);
		iput(dir);
		iput(dir_new);
		free_name(tmp_oldname);
		free_name(tmp_newname);
		return -EACCES;
	}

	if(dir_new->fsop && dir_new->fsop->link) {
		errno = dir_new->fsop->link(i, dir_new, basename);
	} else {
		errno = -EPERM;
	}
	iput(i);
	iput(dir);
	iput(dir_new);
	free_name(tmp_oldname);
	free_name(tmp_newname);
	return errno;
}
