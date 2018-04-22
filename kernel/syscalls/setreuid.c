/*
 * fiwix/kernel/syscalls/setreuid.c
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

int sys_setreuid(__uid_t uid, __uid_t euid)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_setreuid(%d, %d) -> ", current->pid, uid, euid);
#endif /*__DEBUG__ */

	if(IS_SUPERUSER) {
		if(euid >= 0) {
			if(uid >= 0 || (current->euid >= 0 && current->uid != euid)) {
				current->suid = euid;
			}
			current->euid = euid;
		}
		if(uid >= 0) {
			current->uid = uid;
		}
	} else {
		if(euid >= 0 && (current->uid == euid || current->euid == euid || current->suid == euid)) {
			if(uid >= 0 || (current->euid >= 0 && current->uid != euid)) {
				current->suid = euid;
			}
			current->euid = euid;
		} else {
			return -EPERM;
		}
		if(uid >= 0 && (current->uid == uid || current->euid == uid)) {
			current->uid = uid;
		} else {
			return -EPERM;
		}
	}

	return 0;
}
