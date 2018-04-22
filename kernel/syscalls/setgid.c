/*
 * fiwix/kernel/syscalls/setgid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_setgid(__gid_t gid)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_setgid(%d)\n", current->pid, gid);
#endif /*__DEBUG__ */

	if(IS_SUPERUSER) {
		current->gid = current->egid = current->sgid = gid;
	} else {
		if((current->gid == gid) || (current->sgid == gid)) {
			current->egid = gid;
		} else {
			return -EPERM;
		}
	}
	return 0;
}
