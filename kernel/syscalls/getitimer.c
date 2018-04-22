/*
 * fiwix/kernel/syscalls/getitimer.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/time.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getitimer(int which, struct itimerval *curr_value)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_getitimer(%d, 0x%08x) -> \n", current->pid, which, (unsigned int)curr_value);
#endif /*__DEBUG__ */

	if((unsigned int)curr_value) {
		if((errno = check_user_area(VERIFY_WRITE, curr_value, sizeof(struct itimerval)))) {
			return errno;
		}
	}

	switch(which) {
		case ITIMER_REAL:
			ticks2tv(current->it_real_interval, &curr_value->it_interval);
			ticks2tv(current->it_real_value, &curr_value->it_value);
			break;
		case ITIMER_VIRTUAL:
			ticks2tv(current->it_virt_interval, &curr_value->it_interval);
			ticks2tv(current->it_virt_value, &curr_value->it_value);
			break;
		case ITIMER_PROF:
			ticks2tv(current->it_prof_interval, &curr_value->it_interval);
			ticks2tv(current->it_prof_value, &curr_value->it_value);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
