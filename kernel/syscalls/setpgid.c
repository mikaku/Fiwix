/*
 * fiwix/kernel/syscalls/setpgid.c
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

int sys_setpgid(__pid_t pid, __pid_t pgid)
{
	struct proc *p;

#ifdef __DEBUG__
	printk("(pid %d) sys_setpgid(%d, %d)", current->pid, pid, pgid);
#endif /*__DEBUG__ */

	if(pid < 0 || pgid < 0) {
		return -EINVAL;
	}
	if(!pid) {
		pid = current->pid;
	}

	if(!(p = get_proc_by_pid(pid))) {
		return -EINVAL;
	}
	if(!pgid) {
		pgid = p->pid;
	}

	if(p != current && p->ppid != current) {
		return -ESRCH;
	}
	if(p->sid == p->pid || p->sid != current->sid) {
		return -EPERM;
	}

	{
		struct proc *p;

		FOR_EACH_PROCESS(p) {
			if(p->pgid == pgid && p->sid != current->sid) {
				return -EPERM;
			}
			p = p->next;
		}
	}

	if(p != current && p->flags & PF_PEXEC) {
		return -EACCES;
	}

	p->pgid = pgid;

#ifdef __DEBUG__
	printk(" -> 0\n");
#endif /*__DEBUG__ */

	return 0;
}
