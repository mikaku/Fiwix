/*
 * fiwix/kernel/syscalls/setrlimit.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/resource.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_setrlimit(int resource, const struct rlimit *rlim)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_setrlimit(%d, 0x%08x)\n", current->pid, resource, (unsigned int)rlim);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, rlim, sizeof(struct rlimit)))) {
		return errno;
	}
	if(resource < 0 || resource >= RLIM_NLIMITS) {
		return -EINVAL;
	}
	if(rlim->rlim_cur > rlim->rlim_max) {
		return -EINVAL;
	}
	if(!IS_SUPERUSER) {
		if(rlim->rlim_max > current->rlim[resource].rlim_max) {
			return -EPERM;
		}
	}
	memcpy_b(&current->rlim[resource], rlim, sizeof(struct rlimit));
	return 0;
}
