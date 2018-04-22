/*
 * fiwix/kernel/syscalls/getgroups.c
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

int sys_getgroups(__ssize_t size, __gid_t *list)
{
	int n, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_getgroups(%d, 0x%08x)\n", current->pid, size, (unsigned int)list);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, list, sizeof(__gid_t)))) {
		return errno;
	}
	for(n = 0; n < NGROUPS_MAX; n++) {
		if(current->groups[n] == -1) {
			break;
		}
		if(size) {
			if(n > size) {
				return -EINVAL;
			}
			list[n] = (__gid_t)current->groups[n];
		}
	}
	return n;
}
