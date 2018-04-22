/*
 * fiwix/kernel/syscalls/time.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/kernel.h>
#include <fiwix/fs.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_time(__time_t *tloc)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_time() -> ", current->pid);
#endif /*__DEBUG__ */

	if(tloc) {
		if((errno = check_user_area(VERIFY_WRITE, tloc, sizeof(__time_t)))) {
			return errno;
		}
		*tloc = CURRENT_TIME;
	}

#ifdef __DEBUG__
	printk("%d\n", CURRENT_TIME);
#endif /*__DEBUG__ */

	return CURRENT_TIME;
}
