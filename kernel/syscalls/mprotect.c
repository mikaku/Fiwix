/*
 * fiwix/kernel/syscalls/mprotect.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/mman.h>
#include <fiwix/mm.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_mprotect(unsigned int addr, __size_t length, int prot)
{
	struct vma *vma;

#ifdef __DEBUG__
	printk("(pid %d) sys_mprotect(0x%08x, %d, %d)\n", current->pid, addr, length, prot);
#endif /*__DEBUG__ */

	if((addr & ~PAGE_MASK) || length < 0) {
		return -EINVAL;
	}
	if(prot > (PROT_READ | PROT_WRITE | PROT_EXEC)) {
		return -EINVAL;
	}
	if(!(vma = find_vma_region(addr))) {
		return -ENOMEM;
	}
	length = PAGE_ALIGN(length);
	if((addr + length) > vma->end) {
		return -ENOMEM;
	}
	if(vma->inode && (vma->flags & MAP_SHARED)) {
		if(prot & PROT_WRITE) {
			if(!(vma->o_mode & (O_WRONLY | O_RDWR))) {
				return -EACCES;
			}
		}
	}

	return do_mprotect(vma, addr, length, prot);
}
