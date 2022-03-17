/*
 * fiwix/kernel/syscalls/sethostname.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/utsname.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_sethostname(const char *name, int length)
{
	int errno;
	char *tmp_name;

#ifdef __DEBUG__
	printk("(pid %d) sys_sethostname('%s', %d)\n", current->pid, name, length);
#endif /*__DEBUG__ */

	if((errno = malloc_name(name, &tmp_name)) < 0) {
		return errno;
	}
	if(!IS_SUPERUSER) {
		free_name(tmp_name);
		return -EPERM;
	}
	if(length < 0 || length > _UTSNAME_LENGTH) {
		free_name(tmp_name);
		return -EINVAL;
	}
	memcpy_b(&sys_utsname.nodename, tmp_name, length);
	sys_utsname.nodename[length] = 0;
	free_name(tmp_name);
	return 0;
}
