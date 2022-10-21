/*
 * fiwix/include/fiwix/sem.h
 */

#ifdef CONFIG_SYSVIPC

#ifndef _FIWIX_SEM_H
#define _FIWIX_SEM_H

#include <fiwix/types.h>
#include <fiwix/ipc.h>

#define SEM_UNDO	0x1000		/* undo the operation on exit */

/* semctl() command definitions */
#define GETPID		11		/* get sempid */
#define GETVAL		12		/* get semval */
#define GETALL		13		/* get all semval's */
#define GETNCNT		14		/* get semncnt */
#define GETZCNT		15		/* get semzcnt */
#define SETVAL		16		/* set semval */
#define SETALL		17		/* set all semval's */

/* system-wide limits */
#define SEMMNI		128		/* number of semaphores sets */
#define SEMMSL		32		/* max. number of semaphores per id */
#define SEMMNS		(SEMMNI*SEMMSL)	/* max. number of messages in system */
#define SEMOPM		32		/* max. number of ops. per semop() */
#define SEMVMX		32767		/* semaphore maximum value */

#define SEM_STAT	18
#define SEM_INFO	19

struct semid_ds {
	struct ipc_perm sem_perm;	/* access permissions */
	__time_t sem_otime;		/* time of the last semop() */
	__time_t sem_ctime;		/* time of the last change */
	struct sem *sem_base;		/* ptr to the first semaphore in set */
	unsigned int unused1;
	unsigned int unused2;
	struct sem_undo *undo;		/* list of undo requests */
	unsigned short int sem_nsems;	/* number of semaphores in set */
};

/* semaphore buffer for semop() */
struct sembuf {
	unsigned short int sem_num;	/* semaphore number */
	short int sem_op;		/* semaphore operation */
	short int sem_flg;		/* operation flags */
};

/* arg for semctl() */
union semun {
	int val;			/* value for SETVAL */
	struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
	unsigned short int *array;	/* array for GETALL & SETALL */
	struct seminfo *__buf;		/* buffer for IPC_INFO */
};

/* semaphore information structure */
struct seminfo {
	int semmap;
	int semmni;
	int semmns;
	int semmnu;
	int semmsl;
	int semopm;
	int semume;
	int semusz;
	int semvmx;
	int semaem;
};

/* one semaphore structure for each semaphore in the system */
struct sem {
	short int semval;		/* current value */
	short int sempid;		/* pid of last operation */
	short int semncnt;		/* nprocs awaiting increase in semval */
	short int semzcnt;		/* nprocs awaiting semval = 0 */
};

/* list of undo requests executed automatically when the process exits */
struct sem_undo {
	struct sem_undo *proc_next;	/* next entry on this process */
	struct sem_undo *id_next;	/* next entry on this semaphore set */
	int semid;			/* semaphore set identifier */
	short int semadj;		/* adjustment during exit() */
	unsigned short int sem_num;	/* semaphore number */
};


extern struct semid_ds *semset[];
extern unsigned int num_semsets;
extern unsigned int num_sems;
extern unsigned int max_semid;
extern unsigned int sem_seq;

void sem_init(void);
struct semid_ds *sem_get_new_ss(void);
void sem_release_ss(struct semid_ds *);
struct sem *sem_get_new_sma(void);
void sem_release_sma(struct sem *);
struct sem_undo *sem_get_new_su(void);
void sem_release_su(struct sem_undo *);
void semexit(void);
int sys_semop(int, struct sembuf *, int);
int sys_semget(key_t, int, int);
int sys_semctl(int, int, int, void *);

#endif /* _FIWIX_SEM_H */

#endif /* CONFIG_SYSVIPC */
