/*
 * fiwix/kernel/syscalls/fchown.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_fchown(unsigned int ufd, __uid_t owner, __gid_t group)
{
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_fchown(%d, %d, %d)\n", current->pid, ufd, owner, group);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	i = fd_table[current->fd[ufd]].inode;

	if(IS_RDONLY_FS(i)) {
		return -EROFS;
	}
	if(check_user_permission(i)) {
		return -EPERM;
	}

	if(owner == (__uid_t)-1) {
		owner = i->i_uid;
	} else {
		i->i_mode &= ~(S_ISUID);
	}
	if(group == (__gid_t)-1) {
		group = i->i_gid;
	} else {
		i->i_mode &= ~(S_ISGID);
	}

	i->i_uid = owner;
	i->i_gid = group;
	i->i_ctime = CURRENT_TIME;
	i->state |= INODE_DIRTY;
	return 0;
}
