/*
 * fiwix/kernel/syscalls/signal.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

unsigned int sys_signal(__sigset_t signum, void(* sighandler)(int))
{
	struct sigaction s;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_signal()\n", current->pid);
#endif /*__DEBUG__ */

	if(signum < 1 || signum > NSIG) {
		return -EINVAL;
	}
	if(signum == SIGKILL || signum == SIGSTOP) {
		return -EINVAL;
	}
	if(sighandler != SIG_DFL && sighandler != SIG_IGN) {
		if((errno = check_user_area(VERIFY_READ, sighandler, sizeof(void)))) {
			return errno;
		}
	}

	memset_b(&s, 0, sizeof(struct sigaction));
	s.sa_handler = sighandler;
	s.sa_mask = 0;
	s.sa_flags = SA_RESETHAND;
	sighandler = current->sigaction[signum - 1].sa_handler;
	current->sigaction[signum - 1] = s;
	if(current->sigaction[signum - 1].sa_handler == SIG_IGN) {
		if(signum != SIGCHLD) {
			current->sigpending &= SIG_MASK(signum);
		}
	}
	if(current->sigaction[signum - 1].sa_handler == SIG_DFL) {
		if(signum != SIGCHLD) {
			current->sigpending &= SIG_MASK(signum);
		}
	}
	return (unsigned int)sighandler;
}
