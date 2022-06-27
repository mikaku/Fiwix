/*
 * fiwix/kernel/syscalls/semop.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>
#include <fiwix/ipc.h>
#include <fiwix/sem.h>

#ifdef __DEBUG__
#include <fiwix/process.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
static void semundo_ops(struct semid_ds *ss, struct sembuf *sops, int max)
{
	struct sem *s;
	int n;

	for(n = 0; n < max; n++) {
		s = ss->sem_base + sops[n].sem_num;
		s->semval -= sops[n].sem_op;
	}
}

void semexit(void)
{
	struct semid_ds *ss;
	struct sem_undo *su, **sup, *ssu, **ssup;
	struct sem *s;

	sup = &current->semundo;
	while(*sup) {
		su = *sup;
		ss = semset[su->semid % SEMMNI];
		if(ss != IPC_UNUSED) {
			ssup = &ss->undo;
			while(*ssup) {
				ssu = *ssup;
				*ssup = ssu->id_next;
				if(su == ssu) {
					s = ss->sem_base + ssu->sem_num;
					if((ssu->semadj + s->semval) >= 0) {
						s->semval += ssu->semadj;
						s->sempid = current->pid;
						ss->sem_otime = CURRENT_TIME;
						if(s->semncnt) {
							wakeup(&s->semncnt);
						}
						if(!s->semval && s->semzcnt) {
							wakeup(&s->semzcnt);
						}
					}
				}
				ssup = &ssu->id_next;
			}
		}
		*sup = su->proc_next;
		sem_release_su(su);
	}
	current->semundo = NULL;
}

int sys_semop(int semid, struct sembuf *sops, int nsops)
{
	struct semid_ds *ss;
	struct sem *s;
	struct sem_undo *su;
	int need_alter, need_undo;
	int n, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_semop(%d, 0x%x, %d)\n", current->pid, semid, (int)sops, nsops);
#endif /*__DEBUG__ */

	if(semid < 0 || nsops <= 0) {
		return -EINVAL;
	}
	if(nsops > SEMOPM) {
		return -E2BIG;
	}
	if((errno = check_user_area(VERIFY_READ, sops, sizeof(struct sembuf)))) {
		return errno;
	}

	ss = semset[semid % SEMMNI];
	if(ss == IPC_UNUSED) {
		return -EINVAL;
	}

	/* check permissions and ranges for all semaphore operations */
	need_alter = 0;
	for(n = 0; n < nsops; n++) {
		if(sops[n].sem_num > ss->sem_nsems) {
			return -EFBIG;
		}
		/* only negative and positive operations ... */
		if(sops[n].sem_op) {
			/* will alter semaphores */
			need_alter++;
		}
	}
	if(!ipc_has_perms(&ss->sem_perm, need_alter ? IPC_W : IPC_R)) {
		return -EACCES;
	}

	need_undo = 0;

loop:
	for(n = 0; n < nsops; n++) {
		s = ss->sem_base + sops[n].sem_num;

		/* positive semaphore operation */
		if(sops[n].sem_op > 0) {
			if((sops[n].sem_op + s->semval) > SEMVMX) {
				/* reverse all semaphore ops */
				semundo_ops(ss, sops, n);
				return -ERANGE;
			}
			if(sops[n].sem_flg & SEM_UNDO) {
				need_undo = 1;
			}
			s->semval += sops[n].sem_op;
			if(s->semncnt) {
				wakeup(&s->semncnt);
			}
		}

		/* negative semaphore operation */
		if(sops[n].sem_op < 0) {
			if((sops[n].sem_op + s->semval) >= 0) {
				if(sops[n].sem_flg & SEM_UNDO) {
					need_undo = 1;
				}
				s->semval += sops[n].sem_op;
				if(!s->semval && s->semzcnt) {
					wakeup(&s->semzcnt);
				}
			} else {
				/* reverse all semaphore ops */
				semundo_ops(ss, sops, n);
				if(sops[n].sem_flg & IPC_NOWAIT) {
					return -EAGAIN;
				}
				s->semncnt++;
				errno = sleep(&s->semncnt, PROC_INTERRUPTIBLE);
				ss = semset[semid % SEMMNI];
				if(ss == IPC_UNUSED) {
					return -EIDRM;
				}
				s->semncnt--;
				if(errno) {
					return -EINTR;
				}
				goto loop;	/* start loop from beginning */
			}
		}

		/* semaphore operation is zero */
		if(!sops[n].sem_op) {
			if(s->semval) {
				/* reverse all semaphore ops */
				semundo_ops(ss, sops, n);
				if(sops[n].sem_flg & IPC_NOWAIT) {
					return -EAGAIN;
				}
				s->semzcnt++;
				errno = sleep(&s->semzcnt, PROC_INTERRUPTIBLE);
				ss = semset[semid % SEMMNI];
				if(ss == IPC_UNUSED) {
					return -EIDRM;
				}
				s->semzcnt--;
				if(errno) {
					return -EINTR;
				}
				goto loop;	/* start loop from beginning */
			}
		}
	}

	if(need_undo) {
		for(n = 0; n < nsops; n++) {
			if(sops[n].sem_flg & SEM_UNDO) {
				su = current->semundo;
				while(su) {
					if(su->semid == semid && su->sem_num == sops[n].sem_num) {
						break;
					}
					su = su->proc_next;
				}
				if(!su) {
					if(!(su = sem_get_new_su())) {
						semundo_ops(ss, sops, n);
						return -ENOMEM;
					}
					su->proc_next = current->semundo;
					su->id_next = ss->undo;
					su->semid = semid;
					su->semadj = 0;
					su->sem_num = sops[n].sem_num;
					current->semundo = su;
					ss->undo = su;
				}
				su->semadj -= sops[n].sem_op;
			}
		}
	}

	for(n = 0; n < nsops; n++) {
		s = ss->sem_base + sops[n].sem_num;
		s->sempid = current->pid;
	}
	ss->sem_otime = CURRENT_TIME;
	return 0;
}
#endif /* CONFIG_IPC */
