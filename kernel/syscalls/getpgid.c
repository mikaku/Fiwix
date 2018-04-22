/*
 * fiwix/kernel/syscalls/getpgid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getpgid(__pid_t pid)
{
	struct proc *p;

#ifdef __DEBUG__
	printk("(pid %d) sys_getpgid(%d)\n", current->pid, pid);
#endif /*__DEBUG__ */

	if(pid < 0) {
		return -EINVAL;
	}
	if(!pid) {
		return current->pgid;
	}
	FOR_EACH_PROCESS(p) {
		if(p->state != PROC_UNUSED && p->pid == pid) {
			return p->pgid;
		}
	}
	return -ESRCH;
}
