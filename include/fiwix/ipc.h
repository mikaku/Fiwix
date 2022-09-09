/*
 * fiwix/include/fiwix/ipc.h
 */

#ifdef CONFIG_IPC

#ifndef _FIWIX_IPC_H
#define _FIWIX_IPC_H

#include <fiwix/types.h>
#include <fiwix/sleep.h>

#define IPC_CREAT	01000		/* create if key doesn't exist */
#define IPC_EXCL	02000		/* fail if key exists */
#define IPC_NOWAIT	04000		/* return error on wait */

#define IPC_PRIVATE	((key_t)0)	/* private key */

#define IPC_RMID	0		/* remove identifier */
#define IPC_SET		1		/* set options */
#define IPC_STAT	2		/* get options */
#define IPC_INFO	3		/* get system-wide limits */

#define IPC_R		0400		/* read or receive permission */
#define IPC_W		0200		/* write or send permission */

#define SEMOP		1
#define SEMGET		2
#define SEMCTL		3
#define MSGSND		11
#define MSGRCV		12
#define MSGGET		13
#define MSGCTL		14
#define SHMAT		21
#define SHMDT		22
#define SHMGET		23
#define SHMCTL		24

#define IPC_UNUSED	((void *) -1)

typedef int key_t;

struct sysipc_args {
	int arg1;
	int arg2;
	int arg3;
	void *ptr;
	int arg5;
};

/* IPC data structure */
struct ipc_perm {
	key_t key;			/* key */
	__uid_t uid;			/* effective UID of owner */
	__gid_t gid;			/* effective UID of owner */
	__uid_t cuid;			/* effective UID of creator */
	__gid_t cgid;			/* effective UID of creator */
	unsigned short int mode;	/* access modes */
	unsigned short int seq;		/* slot sequence number */
};

#endif /* _FIWIX_IPC_H */

extern struct resource ipcsem_resource;
extern struct resource ipcmsg_resource;

void ipc_init(void);
int ipc_has_perms(struct ipc_perm *, int);

#endif /* CONFIG_IPC */
