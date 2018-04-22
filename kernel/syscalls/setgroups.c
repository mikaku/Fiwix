/*
 * fiwix/kernel/syscalls/setgroups.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_setgroups(__ssize_t size, const __gid_t *list)
{
	int n, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_setgroups(%d, 0x%08x)\n", current->pid, size, (unsigned int)list);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, list, sizeof(__gid_t)))) {
		return errno;
	}
	if(!IS_SUPERUSER) {
		return -EPERM;
	}
	if(size > NGROUPS_MAX) {
		return -EINVAL;
	}
	for(n = 0; n < NGROUPS_MAX; n++) {
		current->groups[n] = -1;
		if(n < size) {
			current->groups[n] = list[n];
		}
	}
	return 0;
}
