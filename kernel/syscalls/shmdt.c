/*
 * fiwix/kernel/syscalls/shmdt.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/string.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/ipc.h>
#include <fiwix/shm.h>
#include <fiwix/stdio.h>

#ifdef CONFIG_SYSVIPC
int sys_shmdt(char *shmaddr)
{
	struct vma *vma;
	struct shmid_ds *seg;
	unsigned int addr;
	int n;

#ifdef __DEBUG__
	printk("(pid %d) sys_shmdt(0x%x)\n", current->pid, (int)shmaddr);
#endif /*__DEBUG__ */

	addr = (unsigned int)shmaddr;

	if(!(vma = find_vma_region(addr))) {
		printk("WARNING: %s(): no vma region found!\n", __FUNCTION__);
		return 0;
	}
	if(vma->s_type != P_SHM) {
		printk("WARNING: %s(): vma region is not a shared memory!\n", __FUNCTION__);
		return 0;
	}
	if(!(seg = (struct shmid_ds *)vma->object)) {
		printk("WARNING: %s(): object is NULL!\n", __FUNCTION__);
		return 0;
	}

	for(n = 0; n < NUM_ATTACHES_PER_SEG; n++) {
		if(seg->shm_attaches[n].start == addr) {
			do_munmap(addr, seg->shm_attaches[n].end - seg->shm_attaches[n].start);
			shm_release_attach(&seg->shm_attaches[n]);
			seg->shm_nattch--;
		}
	}

	return 0;
}
#endif /* CONFIG_SYSVIPC */
