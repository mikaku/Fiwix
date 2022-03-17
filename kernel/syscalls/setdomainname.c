/*
 * fiwix/kernel/syscalls/setdomainname.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/utsname.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_setdomainname(const char *name, int length)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_setdomainname('%s', %d)\n", current->pid, name, length);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, name, length))) {
		return errno;
	}
	if(!IS_SUPERUSER) {
		return -EPERM;
	}
	if(length < 0 || length > _UTSNAME_LENGTH) {
		return -EINVAL;
	}
	memcpy_b(&sys_utsname.domainname, name, length);
	sys_utsname.domainname[length] = 0;
	return 0;
}
