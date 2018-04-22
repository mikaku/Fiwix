/*
 * fiwix/kernel/syscalls/chown.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_chown(const char *filename, __uid_t owner, __gid_t group)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_chown('%s', %d, %d)\n", current->pid, filename, owner, group);
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
	if(check_user_permission(i)) {
		iput(i);
		free_name(tmp_name);
		return -EPERM;
	}

	if(owner == (__uid_t)-1) {
		owner = i->i_uid;
	} else {
		i->i_mode &= ~(S_ISUID);
		i->i_ctime = CURRENT_TIME;
	}
	if(group == (__gid_t)-1) {
		group = i->i_gid;
	} else {
		i->i_mode &= ~(S_ISGID);
		i->i_ctime = CURRENT_TIME;
	}

	i->i_uid = owner;
	i->i_gid = group;
	i->dirty = 1;
	iput(i);
	free_name(tmp_name);
	return 0;
}
