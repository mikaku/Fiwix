/*
 * fiwix/kernel/syscalls/gettimeofday.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/process.h>
#include <fiwix/time.h>
#include <fiwix/timer.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_gettimeofday()\n", current->pid);
#endif /*__DEBUG__ */

	if(tv) {
		if((errno = check_user_area(VERIFY_WRITE, tv, sizeof(struct timeval)))) {
			return errno;
		}
		tv->tv_sec = CURRENT_TIME;
		tv->tv_usec = ((kstat.ticks % HZ) * 1000000) / HZ;
		tv->tv_usec += gettimeoffset();
	}
	if(tz) {
		if((errno = check_user_area(VERIFY_WRITE, tz, sizeof(struct timezone)))) {
			return errno;
		}
		tz->tz_minuteswest = kstat.tz_minuteswest;
		tz->tz_dsttime = kstat.tz_dsttime;
	}
	return 0;
}
