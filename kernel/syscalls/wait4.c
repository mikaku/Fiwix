/*
 * fiwix/kernel/syscalls/wait4.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/resource.h>
#include <fiwix/signal.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_wait4(__pid_t pid, int *status, int options, struct rusage *ru)
{
	struct proc *p;
	int flag, signum, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_wait4(%d, status, %d)\n", current->pid, pid, options);
#endif /*__DEBUG__ */

	if(ru) {
		if((errno = check_user_area(VERIFY_WRITE, ru, sizeof(struct rusage)))) {
			return errno;
		}
	}
	while(current->children) {
		flag = 0;
		FOR_EACH_PROCESS(p) {
			if(p->ppid != current) {
				p = p->next;
				continue;
			}
			if(pid > 0) {
				if(p->pid == pid) {
					flag = 1;
				}
			}
			if(!pid) {
				if(p->pgid == current->pgid) {
					flag = 1;
				}
			}
			if(pid < -1) {
				if(p->pgid == -pid) {
					flag = 1;
				}
			}
			if(pid == -1) {
				flag = 1;
			}
			if(flag) {
				if(p->state == PROC_STOPPED) {
					if(!p->exit_code) {
						p = p->next;
						continue;
					}
					if(status) {
						*status = (p->exit_code << 8) | 0x7F;
					}
					p->exit_code = 0;
					if(ru) {
						get_rusage(p, ru);
					}
					return p->pid;
				}
				if(p->state == PROC_ZOMBIE) {
					add_rusage(p);
					if(status) {
						*status = p->exit_code;
					}
					if(ru) {
						get_rusage(p, ru);
					}
					return remove_zombie(p);
				}
			}
			p = p->next;
			flag = 0;
		}
		if(options & WNOHANG) {
			if(flag) {
				return 0;
			}
			break;
		}
		if((signum = sleep(&sys_wait4, PROC_INTERRUPTIBLE))) {
			return signum;
		}
		current->sigpending &= SIG_MASK(SIGCHLD);
	}
	return -ECHILD;
}
