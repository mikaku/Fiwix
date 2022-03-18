/*
 * fiwix/kernel/syscalls/rename.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
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

int sys_rename(const char *oldpath, const char *newpath)
{
	struct inode *i, *dir, *i_new, *dir_new;
	char *tmp_oldpath, *tmp_newpath;
	char *oldbasename, *newbasename;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_rename('%s', '%s')\n", current->pid, oldpath, newpath);
#endif /*__DEBUG__ */

	if((errno = malloc_name(oldpath, &tmp_oldpath)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_oldpath, &i, &dir, !FOLLOW_LINKS))) {
		if(dir) {
			iput(dir);
		}
		free_name(tmp_oldpath);
		return errno;
	}
	if(IS_RDONLY_FS(i)) {
		iput(i);
		iput(dir);
		free_name(tmp_oldpath);
		return -EROFS;
	}

	if((errno = malloc_name(newpath, &tmp_newpath)) < 0) {
		iput(i);
		iput(dir);
		free_name(tmp_oldpath);
		return errno;
	}
	newbasename = remove_trailing_slash(tmp_newpath);
	if((errno = namei(newbasename, &i_new, &dir_new, !FOLLOW_LINKS))) {
		if(!dir_new) {
			iput(i);
			iput(dir);
			free_name(tmp_oldpath);
			free_name(tmp_newpath);
			return errno;
		}
	}
	if(dir->dev != dir_new->dev) {
		errno = -EXDEV;
		goto end;
	}
	newbasename = get_basename(newbasename);
	if((newbasename[0] == '.' && newbasename[1] == '\0') || (newbasename[0] == '.' && newbasename[1] == '.' && newbasename[2] == '\0')) {
		errno = -EINVAL;
		goto end;
	}

	oldbasename = get_basename(tmp_oldpath);
	oldbasename = remove_trailing_slash(oldbasename);
	if((oldbasename[0] == '.' && oldbasename[1] == '\0') || (oldbasename[0] == '.' && oldbasename[1] == '.' && oldbasename[2] == '\0')) {
		errno = -EINVAL;
		goto end;
	}

	if(i_new) {
		if(S_ISREG(i->i_mode)) {
			if(S_ISDIR(i_new->i_mode)) {
				errno = -EISDIR;
				goto end;
			}
		}
		if(S_ISDIR(i->i_mode)) {
			if(!S_ISDIR(i_new->i_mode)) {
				errno = -ENOTDIR;
				goto end;
			}
		}
		if(i->inode == i_new->inode) {
			errno = 0;
			goto end;
		}
	}

	if(check_permission(TO_EXEC | TO_WRITE, dir_new) < 0) {
		errno = -EACCES;
		goto end;
	}

	if(dir_new->fsop && dir_new->fsop->rename) {
		errno = dir_new->fsop->rename(i, dir, i_new, dir_new, oldbasename, newbasename);
	} else {
		errno = -EPERM;
	}

end:
	iput(i);
	iput(dir);
	iput(i_new);
	iput(dir_new);
	free_name(tmp_oldpath);
	free_name(tmp_newpath);
	return errno;
}
