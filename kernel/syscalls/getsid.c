/*
 * fiwix/kernel/syscalls/getsid.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getsid(__pid_t pid)
{
	struct proc *p;

#ifdef __DEBUG__
	printk("(pid %d) sys_getsid(%d)\n", current->pid, pid);
#endif /*__DEBUG__ */

	if(pid < 0) {
		return -EINVAL;
	}
	if(!pid) {
		return current->sid;
	}

	FOR_EACH_PROCESS(p) {
		if(p->pid == pid) {
			return p->sid;
		}
		p = p->next;
	}
	return -ESRCH;
}
