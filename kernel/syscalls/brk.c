/*
 * fiwix/kernel/syscalls/brk.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_brk(unsigned int brk)
{
	unsigned int newbrk;

#ifdef __DEBUG__
	printk("(pid %d) sys_brk(0x%08x) -> ", current->pid, brk);
#endif /*__DEBUG__ */

	if(!brk || brk < current->brk_lower) {
#ifdef __DEBUG__
		printk("0x%08x\n", current->brk);
#endif /*__DEBUG__ */
		return current->brk;
	}

	newbrk = PAGE_ALIGN(brk);
	if(newbrk == current->brk || newbrk < current->brk_lower) {
#ifdef __DEBUG__
		printk("0x%08x\n", current->brk);
#endif /*__DEBUG__ */
		return brk;
	}

	if(brk < current->brk) {
		do_munmap(newbrk, current->brk - newbrk);
		current->brk = brk;
		return brk;
	}
	if(!expand_heap(newbrk)) {
		current->brk = brk;
	} else {
		return -ENOMEM;
	}
#ifdef __DEBUG__
	printk("0x%08x\n", current->brk);
#endif /*__DEBUG__ */
	return current->brk;
}
