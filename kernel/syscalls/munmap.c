/*
 * fiwix/kernel/syscalls/munmap.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/mman.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_munmap(unsigned int addr, __size_t length)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_munmap(0x%08x, %d)\n", current->pid, addr, length);
#endif /*__DEBUG__ */
	return do_munmap(addr, length);
}
