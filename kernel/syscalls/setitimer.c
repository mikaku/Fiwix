/*
 * fiwix/kernel/syscalls/setitimer.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/time.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_setitimer(%d, 0x%08x, 0x%08x) -> \n", current->pid, which, (unsigned int)new_value, (unsigned int)old_value);
#endif /*__DEBUG__ */

	if((unsigned int)old_value) {
		if((errno = check_user_area(VERIFY_WRITE, old_value, sizeof(struct itimerval)))) {
			return errno;
		}
	}

	return setitimer(which, new_value, old_value);
}
