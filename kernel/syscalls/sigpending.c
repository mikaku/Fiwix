/*
 * fiwix/kernel/syscalls/sigpending.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_sigpending(__sigset_t *set)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_sigpending(0x%08x) -> ", current->pid, set);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, set, sizeof(__sigset_t)))) {
		return errno;
	}
	memcpy_b(set, &current->sigpending, sizeof(__sigset_t));
	return 0;
}
