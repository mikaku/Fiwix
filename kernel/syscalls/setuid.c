/*
 * fiwix/kernel/syscalls/setuid.c
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

int sys_setuid(__uid_t uid)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_setuid(%d)\n", current->pid, uid);
#endif /*__DEBUG__ */

	if(IS_SUPERUSER) {
		current->uid = current->suid = uid;
	} else {
		if((current->uid != uid) && (current->suid != uid)) {
			return -EPERM;
		}
	}
	current->euid = uid;
	return 0;
}
