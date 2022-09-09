/*
 * fiwix/kernel/syscalls/shmget.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/string.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/mm.h>
#include <fiwix/ipc.h>
#include <fiwix/shm.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
struct shmid_ds *shmseg[SHMMNI];
unsigned int num_segs;
unsigned int max_segid;
unsigned int shm_seq;
unsigned int shm_tot;
unsigned int shm_rss;

/* FIXME: this should be allocated dynamically */
static struct shmid_ds shmseg_pool[SHMMNI];
struct shmid_ds *shm_get_new_seg(void)
{
	int n;

	for(n = 0; n < SHMMNI; n++) {
		if(shmseg_pool[n].shm_ctime == 0) {
			shmseg_pool[n].shm_ctime = 1;
			if(!(shmseg_pool[n].shm_pages = (unsigned int *)kmalloc())) {
				return NULL;
			}
			memset_b(shmseg_pool[n].shm_pages, 0, PAGE_SIZE);
			return &shmseg_pool[n];
		}
	}
	return NULL;
}

void shm_release_seg(struct shmid_ds *seg)
{
	kfree((unsigned int)seg->shm_pages);
	if(seg->shm_attaches) {
		kfree((unsigned int)seg->shm_attaches);
	}
	memset_b(seg, 0, sizeof(struct shmid_ds));
}

void free_seg(int shmid)
{
	struct shmid_ds *seg;
	int npages;

	seg = shmseg[shmid % SHMMNI];
	npages = (seg->shm_segsz + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
	num_segs--;
	shm_tot -= npages;
	shm_seq++;
	shmseg[shmid % SHMMNI] = (struct shmid_ds *)IPC_UNUSED;
	shm_release_seg(seg);
}

struct vma *shm_get_new_attach(struct shmid_ds *seg)
{
	int n;

	if(!seg->shm_attaches) {
		if(!(seg->shm_attaches = (void *)kmalloc())) {
			return NULL;
		}
		memset_b(seg->shm_attaches, 0, PAGE_SIZE);
	}
	for(n = 0; n < NUM_ATTACHES_PER_SEG; n++) {
		if(!seg->shm_attaches[n].start && !seg->shm_attaches[n].end) {
			return &seg->shm_attaches[n];
		}
	}
	return NULL;
}

void shm_release_attach(struct vma *attach)
{
	memset_b(attach, 0, sizeof(struct vma));
}

void shm_init(void)
{
	int n;

	for(n = 0; n < SHMMNI; n++) {
		shmseg[n] = (struct shmid_ds *)IPC_UNUSED;
	}
	memset_b(shmseg_pool, 0, sizeof(shmseg_pool));
	num_segs = max_segid = shm_seq = shm_tot = shm_rss = 0;
}

int sys_shmget(key_t key, __size_t size, int shmflg)
{
	struct shmid_ds *seg;
	struct ipc_perm *perm;
	int n, npages;

#ifdef __DEBUG__
	printk("(pid %d) sys_shmget(%d, %d, 0x%x)\n", current->pid, (int)key, size, shmflg);
#endif /*__DEBUG__ */

	if(size < 0 || size > SHMMAX) {
		return -EINVAL;
	}

	if(key == IPC_PRIVATE) {
		/* create a new segment */
		if(size < SHMMIN) {
			return -EINVAL;
		}
		npages = (size + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		if(shm_tot + npages >= SHMALL) {
			return -ENOSPC;
		}
		if(!(seg = shm_get_new_seg())) {
			return -ENOMEM;
		}
		for(n = 0; n < SHMMNI; n++) {
			if(shmseg[n] == (struct shmid_ds *)IPC_UNUSED) {
				goto init;
			}
		}
		shm_release_seg(seg);
		return -ENOSPC;
	}

	seg = NULL;

	for(n = 0; n < SHMMNI; n++) {
		if(shmseg[n] == (struct shmid_ds *)IPC_UNUSED) {
			continue;
		}
		if(key == shmseg[n]->shm_perm.key) {
			seg = shmseg[n];
			break;
		}
	}

	if(!seg) {
		if(!(shmflg & IPC_CREAT)) {
			return -ENOENT;
		}

		/* create a new segment */
		if(size < SHMMIN) {
			return -EINVAL;
		}
		npages = (size + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		if(shm_tot + npages >= SHMALL) {
			return -ENOSPC;
		}
		if(!(seg = shm_get_new_seg())) {
			return -ENOMEM;
		}
		for(n = 0; n < SHMMNI; n++) {
			if(shmseg[n] == (struct shmid_ds *)IPC_UNUSED) {
				goto init;
			}
		}
		shm_release_seg(seg);
		return -ENOSPC;
	} else {
		if((shmflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
			return -EEXIST;
		}
		if(!ipc_has_perms(&seg->shm_perm, shmflg)) {
			return -EACCES;
		}
		if(size > seg->shm_segsz) {
			return -EINVAL;
		}
		return (seg->shm_perm.seq * SHMMNI) + n;
	}

init:
	perm = &seg->shm_perm;
	perm->key = key;
	perm->uid = perm->cuid = current->euid;
        perm->gid = perm->cgid = current->egid;
	perm->mode = shmflg & 0777;
	perm->seq = shm_seq;
	seg->shm_segsz = size;
	seg->shm_atime = seg->shm_dtime = 0;
	seg->shm_ctime = CURRENT_TIME;
	seg->shm_cpid = current->pid;
	seg->shm_lpid = 0;
	seg->shm_nattch = 0;
	seg->shm_npages = 0;
	seg->shm_attaches = 0;
	shmseg[n] = seg;
	if(n > max_segid) {
		max_segid = n;
	}
	num_segs++;
	shm_tot += npages;
	return (seg->shm_perm.seq * SHMMNI) + n;
}
#endif /* CONFIG_IPC */
