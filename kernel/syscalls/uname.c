/*
 * fiwix/kernel/syscalls/uname.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/utsname.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_uname(struct old_utsname *uname)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_uname(0x%08x) -> returning ", current->pid, (unsigned int)uname);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, uname, sizeof(struct old_utsname)))) {
		return errno;
	}
	memcpy_b(&uname->sysname, &sys_utsname.sysname, sizeof(sys_utsname.sysname));
	memcpy_b(&uname->nodename, &sys_utsname.nodename, sizeof(sys_utsname.nodename));
	memcpy_b(&uname->release, &sys_utsname.release, sizeof(sys_utsname.release));
	memcpy_b(&uname->version, &sys_utsname.version, sizeof(sys_utsname.version));
	memcpy_b(&uname->machine, &sys_utsname.machine, sizeof(sys_utsname.machine));
	return 0;
}
