/*
 * fiwix/kernel/syscalls/stime.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/timer.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_stime(__time_t *t)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_stime(0x%08x)\n", current->pid, (unsigned int)t);
#endif /*__DEBUG__ */

	if(!IS_SUPERUSER) {
		return -EPERM;
	}
	if((errno = check_user_area(VERIFY_READ, t, sizeof(__time_t)))) {
		return errno;
	}

	set_system_time(*t);
	return 0;
}
