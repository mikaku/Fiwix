/*
 * fiwix/kernel/syscalls/sigprocmask.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_sigprocmask(int how, const __sigset_t *set, __sigset_t *oldset)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_sigprocmask(%d, 0x%08x, 0x%08x)\n", current->pid, how, set, oldset);
#endif /*__DEBUG__ */

	if(oldset) {
		if((errno = check_user_area(VERIFY_WRITE, oldset, sizeof(__sigset_t)))) {
			return errno;
		}
		*oldset = current->sigblocked;
	}

	if(set) {
		if((errno = check_user_area(VERIFY_READ, set, sizeof(__sigset_t)))) {
			return errno;
		}
		switch(how) {
			case SIG_BLOCK:
				current->sigblocked |= (*set & SIG_BLOCKABLE);
				break;
			case SIG_UNBLOCK:
				current->sigblocked &= ~(*set & SIG_BLOCKABLE);
				break;
			case SIG_SETMASK:
				current->sigblocked = (*set & SIG_BLOCKABLE);
				break;
			default:
				return -EINVAL;
		}
	}
	return 0;
}
