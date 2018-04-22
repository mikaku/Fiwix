/*
 * fiwix/kernel/syscalls/setregid.c
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

int sys_setregid(__gid_t gid, __gid_t egid)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_setregid(%d, %d) -> ", current->pid, gid, egid);
#endif /*__DEBUG__ */

	if(IS_SUPERUSER) {
		if(egid >= 0) {
			if(gid >= 0 || (current->egid >= 0 && current->gid != egid)) {
				current->sgid = egid;
			}
			current->egid = egid;
		}
		if(gid >= 0) {
			current->gid = gid;
		}
	} else {
		if(egid >= 0 && (current->gid == egid || current->egid == egid || current->sgid == egid)) {
			if(gid >= 0 || (current->egid >= 0 && current->gid != egid)) {
				current->sgid = egid;
			}
			current->egid = egid;
		} else {
			return -EPERM;
		}
		if(gid >= 0 && (current->gid == gid || current->egid == gid)) {
			current->gid = gid;
		} else {
			return -EPERM;
		}
	}

	return 0;
}
