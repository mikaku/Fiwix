/*
 * fiwix/kernel/syscalls/sigsuspend.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/syscalls.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_sigsuspend(__sigset_t *mask)
{
	__sigset_t old_mask;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_sigsuspend(0x%08x) -> ", current->pid, mask);
#endif /*__DEBUG__ */

	old_mask = current->sigblocked;
	if(mask) {
		if((errno = check_user_area(VERIFY_READ, mask, sizeof(__sigset_t)))) {
			return errno;
		}
		current->sigblocked = (int)*mask & SIG_BLOCKABLE;
	} else {
		current->sigblocked = 0 & SIG_BLOCKABLE;
	}
	sys_pause();
	current->sigblocked = old_mask;

#ifdef __DEBUG__
	printk("-EINTR\n");
#endif /*__DEBUG__ */

	return -EINTR;
}
