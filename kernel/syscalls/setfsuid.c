/*
 * fiwix/kernel/syscalls/setfsuid.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_setfsuid(__uid_t fsuid)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_setfsuid(%d) -> %d\n", current->pid, fsuid);
#endif /*__DEBUG__ */
	return 0;
}
