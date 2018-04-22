/*
 * fiwix/kernel/syscalls/setfsgid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_setfsgid(__gid_t fsgid)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_setfsgid(%d) -> %d\n", current->pid, fsgid);
#endif /*__DEBUG__ */
	return 0;
}
