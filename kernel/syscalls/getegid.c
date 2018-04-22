/*
 * fiwix/kernel/syscalls/getegid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_getegid(void)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_getegid() -> %d\n", current->pid, current->egid);
#endif /*__DEBUG__ */
	return current->egid;
}
