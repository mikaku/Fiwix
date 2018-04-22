/*
 * fiwix/kernel/syscalls/sigaction.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/signal.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_sigaction(__sigset_t signum, const struct sigaction *newaction, struct sigaction *oldaction)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_sigaction(%d, 0x%08x, 0x%08x)\n", current->pid, signum, (unsigned int)newaction, (unsigned int)oldaction);
#endif /*__DEBUG__ */

	if(signum < 1 || signum > NSIG) {
		return -EINVAL;
	}
	if(signum == SIGKILL || signum == SIGSTOP) {
		return -EINVAL;
	}
	if(oldaction) {
		if((errno = check_user_area(VERIFY_WRITE, oldaction, sizeof(struct sigaction)))) {
			return errno;
		}
		*oldaction = current->sigaction[signum - 1];
	}
	if(newaction) {
		if((errno = check_user_area(VERIFY_READ, newaction, sizeof(struct sigaction)))) {
			return errno;
		}
		current->sigaction[signum - 1] = *newaction;
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
	}
	return 0;
}
