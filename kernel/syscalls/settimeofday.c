/*
 * fiwix/kernel/syscalls/settimeofday.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/time.h>
#include <fiwix/timer.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_settimeofday(const struct timeval *tv, const struct timezone *tz)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_settimeofday()\n", current->pid);
#endif /*__DEBUG__ */

	if(!IS_SUPERUSER) {
		return -EPERM;
	}

	if(tv) {
		if((errno = check_user_area(VERIFY_READ, tv, sizeof(struct timeval)))) {
			return errno;
		}
		CURRENT_TIME = tv->tv_sec;
		set_system_time(CURRENT_TIME);
	}
	if(tz) {
		if((errno = check_user_area(VERIFY_READ, tz, sizeof(struct timezone)))) {
			return errno;
		}
		kstat.tz_minuteswest = tz->tz_minuteswest;
		kstat.tz_dsttime = tz->tz_dsttime;
	}
	return 0;
}
