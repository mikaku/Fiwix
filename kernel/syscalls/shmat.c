/*
 * fiwix/kernel/syscalls/shmat.c
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
#include <fiwix/fcntl.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/ipc.h>
#include <fiwix/shm.h>
#include <fiwix/stdio.h>

#ifdef __DEBUG__
#include <fiwix/process.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_SYSVIPC
int shm_map_page(struct vma *vma, unsigned int cr2)
{
	struct shmid_ds *seg;
	struct page *pg;
	unsigned int addr, index;

	seg = (struct shmid_ds *)vma->object;
	index = (cr2 - vma->start) / PAGE_SIZE;
	addr = seg->shm_pages[index];

	if(!addr) {
		if(!(addr = map_page(current, cr2, 0, vma->prot))) {
			printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
			return 1;
		}
		seg->shm_pages[index] = addr;
		shm_rss++;
	} else {
		if(!(addr = map_page(current, cr2, V2P(addr), vma->prot))) {
			printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
			return 1;
		}
	}

	pg = &page_table[V2P(addr) >> PAGE_SHIFT];
	pg->count++;	/* FIXME: temporary until fix count++ in map_page() (check 'todo') */
	return 0;
}

int sys_shmat(int shmid, char *shmaddr, int shmflg, unsigned long int *raddr)
{
	struct shmid_ds *seg;
	struct vma *sega;
	unsigned long int addr;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_shmat(%d, 0x%x, %d, 0x%x)\n", current->pid, shmid, (int)shmaddr, shmflg, (int)raddr);
#endif /*__DEBUG__ */

	if(shmid < 0) {
		return -EINVAL;
	}

	seg = shmseg[shmid % SHMMNI];
	if(seg == IPC_UNUSED) {
		return -EINVAL;
	}

	addr = (unsigned long int)shmaddr;
	if(addr) {
		if(shmflg & SHM_RND) {
			addr &= ~(SHMLBA - 1);
		} else {
			if(addr & (SHMLBA - 1)) {
				return -EINVAL;
			}
		}
		if(find_vma_intersection(addr, addr + seg->shm_segsz)) {
			return -EINVAL;
		}
	} else {
		if(!(addr = get_unmapped_vma_region(seg->shm_segsz))) {
			return -ENOMEM;
		}
	}

	if(!ipc_has_perms(&seg->shm_perm, shmflg & SHM_RDONLY ? IPC_R : IPC_R | IPC_W)) {
		return -EACCES;
	}

	if(!(sega = shm_get_new_attach(seg))) {
		return -ENOMEM;
	}

	sega->start = addr;
	sega->end = addr + seg->shm_segsz;
	sega->prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	sega->flags = MAP_PRIVATE | MAP_FIXED;
	sega->offset = 0;
	sega->s_type = P_SHM;
	sega->inode = NULL;
	sega->o_mode = shmflg & SHM_RDONLY ? O_RDONLY : O_RDWR;
	seg->shm_nattch++;

	errno = do_mmap(NULL, addr, seg->shm_segsz, sega->prot, sega->flags, sega->offset, sega->s_type, sega->o_mode, seg);
	if(errno < 0 && errno > -PAGE_SIZE) {
		return errno;
	}

	seg->shm_atime = CURRENT_TIME;
	seg->shm_lpid = current->pid;
	*raddr = addr;

	return 0;
}
#endif /* CONFIG_SYSVIPC */
