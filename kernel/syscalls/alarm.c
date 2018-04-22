/*
 * fiwix/kernel/syscalls/alarm.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/time.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_alarm(unsigned int secs)
{
	struct itimerval value, ovalue;

#ifdef __DEBUG__
	printk("(pid %d) sys_alarm(%d)\n", current->pid, secs);
#endif /*__DEBUG__ */

	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	value.it_value.tv_sec = secs;
	value.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &value, &ovalue);

	/* 
	 * If there are still some usecs left and since the return value has
	 * not enough granularity for them, then just add 1 second to it.
	 */
	if(ovalue.it_value.tv_usec) {
		ovalue.it_value.tv_sec++;
	}

	return ovalue.it_value.tv_sec;
}
