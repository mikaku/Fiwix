/*
 * fiwix/kernel/syscalls/utime.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/utime.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_utime(const char *filename, struct utimbuf *times)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_utime('%s', 0x%08x)\n", current->pid, filename, (int)times);
#endif /*__DEBUG__ */

	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}

	if(IS_RDONLY_FS(i)) {
		iput(i);
		free_name(tmp_name);
		return -EROFS;
	}

	if(!times) {
		if(check_user_permission(i) || check_permission(TO_WRITE, i)) {
			iput(i);
			free_name(tmp_name);
			return -EACCES;
		}
		i->i_atime = CURRENT_TIME;
		i->i_mtime = CURRENT_TIME;
	} else {
		if((errno = check_user_area(VERIFY_READ, times, sizeof(struct utimbuf)))) {
			iput(i);
			free_name(tmp_name);
			return errno;
		}
		if(check_user_permission(i)) {
			iput(i);
			free_name(tmp_name);
			return -EPERM;
		}
		i->i_atime = times->actime;
		i->i_mtime = times->modtime;
	}

	i->i_ctime = CURRENT_TIME;
	i->dirty = 1;
	iput(i);
	free_name(tmp_name);
	return 0;
}
