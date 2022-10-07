/*
 * fiwix/kernel/syscalls/semget.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/string.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/ipc.h>
#include <fiwix/sem.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
struct semid_ds *semset[SEMMNI];
unsigned int num_semsets;
unsigned int num_sems;
unsigned int max_semid;
unsigned int sem_seq;

/* FIXME: this should be allocated dynamically */
static struct semid_ds semset_pool[SEMMNI];
struct semid_ds *sem_get_new_ss(void)
{
	int n;

	for(n = 0; n < SEMMNI; n++) {
		if(semset_pool[n].sem_ctime == 0) {
			semset_pool[n].sem_ctime = 1;
			return &semset_pool[n];
		}
	}
	return NULL;
}

void sem_release_ss(struct semid_ds *ss)
{
	memset_b(ss, 0, sizeof(struct semid_ds));
}

struct sem sma_pool[SEMMNS];
struct sem *sem_get_new_sma(void)
{
	unsigned int n;

	lock_resource(&ipcsem_resource);

	for(n = 0; n < SEMMNS; n += SEMMSL) {
		if(sma_pool[n].sempid < 0) {
			sma_pool[n].sempid = 0;
			unlock_resource(&ipcsem_resource);
			return &sma_pool[n];
		}
	}

	unlock_resource(&ipcsem_resource);
	return NULL;
}

void sem_release_sma(struct sem *sma)
{
	lock_resource(&ipcsem_resource);
	memset_b(sma, 0, sizeof(struct sem) * SEMMSL);
	sma->sempid = -1;
	unlock_resource(&ipcsem_resource);
}

struct sem_undo semundo_pool[SEMMSL];
struct sem_undo *sem_get_new_su(void)
{
	int n;

	for(n = 0; n < SEMMSL; n++) {
		if(semundo_pool[n].semid < 0) {
			semundo_pool[n].semid = 0;
			return &semundo_pool[n];
		}
	}
	return NULL;
}

void sem_release_su(struct sem_undo *su)
{
	memset_b(su, 0, sizeof(struct sem_undo));
	su->semid = -1;
}

void sem_init(void)
{
	int n;

	for(n = 0; n < SEMMNI; n++) {
		semset[n] = (struct semid_ds *)IPC_UNUSED;
	}
	memset_b(semset_pool, 0, sizeof(semset_pool));
	memset_b(sma_pool, 0, sizeof(sma_pool));
	for(n = 0; n < SEMMNS; n += SEMMSL) {
		sma_pool[n].sempid = -1;
	}
	memset_b(semundo_pool, 0, sizeof(semundo_pool));
	for(n = 0; n < SEMMSL; n++) {
		semundo_pool[n].semid = -1;
	}
	num_semsets = num_sems = max_semid = sem_seq = 0;
}

int sys_semget(key_t key, int nsems, int semflg)
{
	struct semid_ds *ss;
	struct ipc_perm *perm;
	int n;

#ifdef __DEBUG__
	printk("(pid %d) sys_semget(%d, %d, 0x%x)\n", current->pid, (int)key, nsems, semflg);
#endif /*__DEBUG__ */

	if(nsems < 0 || nsems > SEMMSL) {
		return -EINVAL;
	}

	if(key == IPC_PRIVATE) {
		/* create a new semaphore set */
		if(!nsems) {
			return -EINVAL;
		}
		if(num_sems + nsems > SEMMNS) {
			return -ENOSPC;
		}
		if(!(ss = sem_get_new_ss())) {
			return -ENOMEM;
		}
		for(n = 0; n < SEMMNI; n++) {
			if(semset[n] == (struct semid_ds *)IPC_UNUSED) {
				goto init;
			}
		}
		sem_release_ss(ss);
		return -ENOSPC;
	}

	ss = NULL;

	for(n = 0; n < SEMMNI; n++) {
		if(semset[n] == (struct semid_ds *)IPC_UNUSED) {
			continue;
		}
		if(key == semset[n]->sem_perm.key) {
			ss = semset[n];
			break;
		}
	}

	if(!ss) {
		if(!(semflg & IPC_CREAT)) {
			return -ENOENT;
		}

		/* create a new semaphore set */
		if(!nsems) {
			return -EINVAL;
		}
		if(num_sems + nsems > SEMMNS) {
			return -ENOSPC;
		}
		if(!(ss = sem_get_new_ss())) {
			return -ENOMEM;
		}
		for(n = 0; n < SEMMNI; n++) {
			if(semset[n] == (struct semid_ds *)IPC_UNUSED) {
				goto init;
			}
		}
		sem_release_ss(ss);
		return -ENOSPC;
	} else {
		if((semflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
			return -EEXIST;
		}
		if(!ipc_has_perms(&ss->sem_perm, semflg)) {
			return -EACCES;
		}
		if(nsems > ss->sem_nsems) {
			return -EINVAL;
		}
		return (ss->sem_perm.seq * SEMMNI) + n;
	}

init:
	perm = &ss->sem_perm;
	perm->key = key;
	perm->uid = perm->cuid = current->euid;
	perm->gid = perm->cgid = current->egid;
	perm->mode = semflg & 0777;
	perm->seq = sem_seq;
	ss->sem_otime = 0;
	ss->sem_ctime = CURRENT_TIME;
	ss->sem_base = sem_get_new_sma();
	ss->undo = NULL;
	ss->sem_nsems = nsems;
	semset[n] = ss;
	if(n > max_semid) {
		max_semid = n;
	}
	num_sems += nsems;
	num_semsets++;
	return (ss->sem_perm.seq * SEMMNI) + n;
}
#endif /* CONFIG_IPC */
