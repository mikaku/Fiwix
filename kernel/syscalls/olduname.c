/*
 * fiwix/kernel/syscalls/olduname.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/utsname.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_olduname(struct oldold_utsname *uname)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_olduname(0x%0x)", current->pid, uname);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, uname, sizeof(struct oldold_utsname)))) {
		return errno;
	}
	memcpy_b(&uname->sysname, &sys_utsname.sysname, _OLD_UTSNAME_LENGTH);
	memset_b(&uname->sysname + _OLD_UTSNAME_LENGTH, 0, 1);
	memcpy_b(&uname->nodename, &sys_utsname.nodename, _OLD_UTSNAME_LENGTH);
	memset_b(&uname->nodename + _OLD_UTSNAME_LENGTH, 0, 1);
	memcpy_b(&uname->release, &sys_utsname.release, _OLD_UTSNAME_LENGTH);
	memset_b(&uname->release + _OLD_UTSNAME_LENGTH, 0, 1);
	memcpy_b(&uname->version, &sys_utsname.version, _OLD_UTSNAME_LENGTH);
	memset_b(&uname->version + _OLD_UTSNAME_LENGTH, 0, 1);
	memcpy_b(&uname->machine, &sys_utsname.machine, _OLD_UTSNAME_LENGTH);
	memset_b(&uname->machine + _OLD_UTSNAME_LENGTH, 0, 1);
	return 0;
}
