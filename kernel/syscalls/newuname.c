/*
 * fiwix/kernel/syscalls/newuname.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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

int sys_newuname(struct new_utsname *uname)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_newuname(0x%08x)\n", current->pid, (int)uname);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, uname, sizeof(struct new_utsname)))) {
		return errno;
	}
	if(!uname) {
		return -EFAULT;
	}
	memcpy_b(uname, &sys_utsname, sizeof(struct new_utsname));
	return 0;
}
