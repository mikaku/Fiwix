/*
 * fiwix/kernel/syscalls/ftime.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/timeb.h>
#include <fiwix/timer.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_ftime(struct timeb *tp)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_ftime()\n", current->pid);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, tp, sizeof(struct timeb)))) {
		return errno;
	}
	tp->time = CURRENT_TIME;
	tp->millitm = ((kstat.ticks % HZ) * 1000000) / HZ;
	/* FIXME: 'timezone' and 'dstflag' fields are not used */

	return 0;
}
