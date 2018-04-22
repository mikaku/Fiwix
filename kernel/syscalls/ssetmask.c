/*
 * fiwix/kernel/syscalls/ssetmask.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/signal.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_ssetmask(int newmask)
{
	int oldmask;

#ifdef __DEBUG__
	printk("(pid %d) sys_ssetmask(0x%08x) -> \n", current->pid, newmask);
#endif /*__DEBUG__ */

	oldmask = current->sigblocked;
	current->sigblocked = newmask & SIG_BLOCKABLE;
	return oldmask;
}
