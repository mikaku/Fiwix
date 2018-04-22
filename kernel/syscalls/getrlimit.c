/*
 * fiwix/kernel/syscalls/getrlimit.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/resource.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getrlimit(int resource, struct rlimit *rlim)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_getrlimit(%d, 0x%08x)\n", current->pid, resource, (unsigned int)rlim);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, rlim, sizeof(struct rlimit)))) {
		return errno;
	}
	if(resource < 0 || resource >= RLIM_NLIMITS) {
		return -EINVAL;
	}

	rlim->rlim_cur = current->rlim[resource].rlim_cur;
	rlim->rlim_max = current->rlim[resource].rlim_max;
	return 0;
}
