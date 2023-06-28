/*
 * fiwix/include/fiwix/msg.h
 */

#ifdef CONFIG_SYSVIPC

#ifndef _FIWIX_MSG_H
#define _FIWIX_MSG_H

#include <fiwix/types.h>
#include <fiwix/ipc.h>

#define MSG_NOERROR	010000		/* no error if message is too big */
#define MSG_EXCEPT	020000		/* recv any msg except of specified type */

/* system-wide limits */
#define MSGMAX		4096		/* max. size of a message */
#define MSGMNI		128		/* max. number of message queues */
#define MSGMNB		16384		/* total size of message queue */
#define MSGTQL		1024		/* max. number of messages */

#define MSG_STAT	11
#define MSG_INFO	12

struct msqid_ds {
	struct ipc_perm msg_perm;	/* access permissions */
	struct msg *msg_first;		/* ptr to the first message in queue */
	struct msg *msg_last;		/* ptr to the last message in queue */
	__time_t msg_stime;		/* time of the last msgsnd() */
	__time_t msg_rtime;		/* time of the last msgrcv() */
	__time_t msg_ctime;		/* time of the last change */
	unsigned int unused1;
	unsigned int unused2;
	unsigned short int msg_cbytes;	/* number of bytes in queue */
	unsigned short int msg_qnum;	/* number of messages in queue */
	unsigned short int msg_qbytes;	/* max. number of bytes in queue */
	unsigned short int msg_lspid;	/* PID of last msgsnd() */
	unsigned short int msg_lrpid;	/* PID of last msgrcv() */
};

/* message buffer for msgsnd() and msgrcv() */
struct msgbuf {
	unsigned int mtype;		/* type of message */
	char mtext[1];      		/* message text */
};

/* buffer for msgctl() with IPC_INFO and MSG_INFO commands */
struct msginfo {
	int msgpool;
	int msgmap;
	int msgmax;			/* MSGMAX */
	int msgmnb;			/* MSGMNB */
	int msgmni;			/* MSGMNI */
	int msgssz;
	int msgtql;			/* MSGTQL */
	unsigned short int msgseg;
};

/* one msg structure for each message */
struct msg {
	struct msg *msg_next;		/* next message on queue */
	unsigned int msg_type;
	char *msg_spot;			/* message text address */
	__time_t msg_stime;		/* msgsnd time */
	short int msg_ts;		/* message text size */
};

extern struct msqid_ds *msgque[];
extern unsigned int num_queues;
extern unsigned int num_msgs;
extern unsigned int max_mqid;
extern unsigned int msg_seq;

void msg_init(void);
struct msqid_ds *msg_get_new_mq(void);
void msg_release_mq(struct msqid_ds *);
struct msg *msg_get_new_md(void);
void msg_release_md(struct msg *);
int sys_msgsnd(int, const void *, __size_t, int);
int sys_msgrcv(int, void *, __size_t, unsigned int, int);
int sys_msgget(key_t, int);
int sys_msgctl(int, int, struct msqid_ds *);

#endif /* _FIWIX_MSG_H */

#endif /* CONFIG_SYSVIPC */
