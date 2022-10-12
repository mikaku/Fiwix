/*
 * fiwix/kernel/syscalls/semctl.c
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
#include <fiwix/ipc.h>
#include <fiwix/sem.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
int sys_semctl(int semid, int semnum, int cmd, void *arg)
{
	struct semid_ds *ss, *tmp;
	struct seminfo *si;
	struct sem_undo *un;
	struct ipc_perm *perm;
	struct sem *s;
	unsigned short int *p;
	int n, errno, retval, val;

#ifdef __DEBUG__
	printk("(pid %d) sys_semctl(%d, %d, %d, 0x%x)\n", current->pid, semid, semnum, cmd, (int)arg);
#endif /*__DEBUG__ */

	if(semid < 0) {
		return -EINVAL;
	}

	switch(cmd) {	
		case IPC_STAT:
		case SEM_STAT:
			if((errno = check_user_area(VERIFY_WRITE, arg, sizeof(struct semid_ds)))) {
				return errno;
			}
			if(cmd == IPC_STAT) {
				ss = semset[semid % SEMMNI];
			} else {
				ss = semset[semid];
			}
			if(ss == IPC_UNUSED) {
				return -EINVAL;
			}
			if(!ipc_has_perms(&ss->sem_perm, IPC_R)) {
				return -EACCES;
			}
			if(cmd == IPC_STAT) {
				retval = 0;
			} else {
				retval = (ss->sem_perm.seq * SEMMNI) + semid;
			}
			memcpy_b(arg, ss, sizeof(struct semid_ds));
			return retval;

		case IPC_SET:
			if((errno = check_user_area(VERIFY_READ, arg, sizeof(struct semid_ds)))) {
				return errno;
			}
			ss = semset[semid % SEMMNI];
			if(ss == IPC_UNUSED) {
				return -EINVAL;
			}
			perm = &ss->sem_perm;
			if(!IS_SUPERUSER && current->euid != perm->uid && current->euid != perm->cuid) {
				return -EPERM;
			}
			tmp = (struct semid_ds *)arg;
			perm->uid = tmp->sem_perm.uid;
			perm->gid = tmp->sem_perm.gid;
			perm->mode = (perm->mode & ~0777) | (tmp->sem_perm.mode & 0777);
			ss->sem_ctime = CURRENT_TIME;
			return 0;

		case IPC_RMID:
			ss = semset[semid % SEMMNI];
			if(ss == IPC_UNUSED) {
				return -EINVAL;
			}
			perm = &ss->sem_perm;
			if(!IS_SUPERUSER && current->euid != perm->uid && current->euid != perm->cuid) {
				return -EPERM;
			}
			un = ss->undo;
			while(un) {
				if(semnum == un->sem_num) {
					un->semadj = 0;
				}
				un = un->id_next;
			}
			for(n = 0; n < ss->sem_nsems; n++) {
				s = ss->sem_base + n;
				if(s->semncnt) {
					wakeup(&s->semncnt);
				}
				if(s->semzcnt) {
					wakeup(&s->semzcnt);
				}
			}
			num_sems -= ss->sem_nsems;
			sem_release_ss(ss);
			semset[semid % SEMMNI] = (struct semid_ds *)IPC_UNUSED;
			num_semsets--;
			sem_seq++;
			if((semid % SEMMNI) == max_semid) {
				while(max_semid) {
					if(semset[max_semid] != IPC_UNUSED) {
						break;
					}
					max_semid--;
				}
			}
			wakeup(ss);
			return 0;

		case IPC_INFO:
		case SEM_INFO:
			if((errno = check_user_area(VERIFY_WRITE, arg, sizeof(struct seminfo)))) {
				return errno;
			}
			si = (struct seminfo *)arg;
			if(cmd == IPC_INFO) {
				si->semusz = sizeof(struct sem_undo);
				si->semaem = 0;	/* FIXME: pending to do */
			} else {
				si->semusz = num_semsets;
				si->semaem = num_sems;
			}
			si->semmap = 0;		/* FIXME: pending to do */
			si->semmni = SEMMNI;
			si->semmns = SEMMNS;
			si->semmnu = 0;		/* FIXME: pending to do */
			si->semmsl = SEMMSL;
			si->semopm = SEMOPM;
			si->semume = 0;		/* FIXME: pending to do */
			si->semvmx = SEMVMX;
			return max_semid;

		case GETPID:
		case GETVAL:
		case GETALL:
		case GETNCNT:
		case GETZCNT:
			ss = semset[semid % SEMMNI];
			if(ss == IPC_UNUSED) {
				return -EINVAL;
			}
			if(!ipc_has_perms(&ss->sem_perm, IPC_R)) {
				return -EACCES;
			}
			s = ss->sem_base + semnum;
			switch(cmd) {
				case GETPID:
					return s->sempid;
				case GETVAL:
					return s->semval;
				case GETALL:
					if((errno = check_user_area(VERIFY_WRITE, arg, ss->sem_nsems * sizeof(short int)))) {
						return errno;
					}
					p = (unsigned short int *)arg;
					for(n = 0; n < ss->sem_nsems; n++) {
						memcpy_b(p, &ss->sem_base[n].semval, sizeof(short int));
						p++;
					}
					return 0;
				case GETNCNT:
					return s->semncnt;
				case GETZCNT:
					return s->semzcnt;
			}

		case SETVAL:
			ss = semset[semid % SEMMNI];
			if(ss == IPC_UNUSED) {
				return -EINVAL;
			}
			if(!ipc_has_perms(&ss->sem_perm, IPC_W)) {
				return -EACCES;
			}
			val = (int)(int*)arg;
			if(val < 0 || val > SEMVMX) {
				return -ERANGE;
			}
			if(semnum < 0 || semnum > ss->sem_nsems) {
				return -EINVAL;
			}
			s = ss->sem_base + semnum;
			s->semval = val;
			ss->sem_ctime= CURRENT_TIME;
			un = ss->undo;
			while(un) {
				if(semnum == un->sem_num) {
					un->semadj = 0;
				}
				un = un->id_next;
			}
			if(s->semncnt) {
				wakeup(&s->semncnt);
			}
			if(s->semzcnt) {
				wakeup(&s->semzcnt);
			}
			return 0;

		case SETALL:
			ss = semset[semid % SEMMNI];
			if(ss == IPC_UNUSED) {
				return -EINVAL;
			}
			if((errno = check_user_area(VERIFY_READ, arg, ss->sem_nsems * sizeof(short int)))) {
				return errno;
			}
			if(!ipc_has_perms(&ss->sem_perm, IPC_W)) {
				return -EACCES;
			}
			p = (unsigned short int *)arg;
			for(n = 0; n < ss->sem_nsems; n++) {
				ss->sem_base[n].semval = *p;
				p++;
				s = ss->sem_base + n;
				if(s->semncnt) {
					wakeup(&s->semncnt);
				}
				if(s->semzcnt) {
					wakeup(&s->semzcnt);
				}
			}
			ss->sem_ctime= CURRENT_TIME;
			un = ss->undo;
			while(un) {
				if(semnum == un->sem_num) {
					un->semadj = 0;
				}
				un = un->id_next;
			}
			return 0;
	}

	return -EINVAL;
}
#endif /* CONFIG_IPC */
