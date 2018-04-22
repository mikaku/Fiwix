/*
 * fiwix/kernel/syscalls/ioperm.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_ioperm(unsigned long int from, unsigned long int num, int turn_on)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_ioperm(0x%08x, 0x%08x, 0x%08x)\n", current->pid, from, num, turn_on);
#endif /*__DEBUG__ */

	if(!IS_SUPERUSER) {
		return -EPERM;
	}

	/* FIXME: to be implemented */

	return 0;
}
