/*
 * fiwix/kernel/syscalls/times.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/syscalls.h>
#include <fiwix/times.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_times(struct tms *buf)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_times(0x%08x) -> ", (unsigned int )buf);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, buf, sizeof(struct tms)))) {
		return errno;
	}
	if(buf) {
		buf->tms_utime = tv2ticks(&current->usage.ru_utime);
		buf->tms_stime = tv2ticks(&current->usage.ru_stime);
		buf->tms_cutime = tv2ticks(&current->cusage.ru_utime);
		buf->tms_cstime = tv2ticks(&current->cusage.ru_stime);
	}

	return kstat.ticks;
}
