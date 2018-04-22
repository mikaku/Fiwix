/*
 * fiwix/kernel/syscalls/setsid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_setsid(void)
{
	struct proc *p;

#ifdef __DEBUG__
	printk("(pid %d) sys_setsid()\n", current->pid);
#endif /*__DEBUG__ */

	if(PG_LEADER(current)) {
		return -EPERM;
	}
	FOR_EACH_PROCESS(p) {	/* POSIX ANSI/IEEE Std 1003.1-1996 4.3.2 */
		if(p != current && p->pgid == current->pid) {
			return -EPERM;
		}
	}

	current->sid = current->pgid = current->pid;
	current->ctty = NULL;
	return current->sid;
}
