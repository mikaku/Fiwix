/*
 * fiwix/kernel/syscalls/getcwd.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_getcwd(char *buf, __size_t size)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_getcwd(0x%08x, %d)\n", current->pid, (unsigned int)buf, size);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, buf, size))) {
		return errno;
	}
	return -ENOSYS;
}
