/*
 * fiwix/kernel/syscalls/msgget.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/string.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/ipc.h>
#include <fiwix/msg.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_SYSVIPC
struct msqid_ds *msgque[MSGMNI];
unsigned int num_queues;
unsigned int num_msgs;
unsigned int max_mqid;
unsigned int msg_seq;

/* FIXME: this should be allocated dynamically */
static struct msqid_ds msgque_pool[MSGMNI];
struct msqid_ds *msg_get_new_mq(void)
{
	int n;

	for(n = 0; n < MSGMNI; n++) {
		if(msgque_pool[n].msg_ctime == 0) {
			msgque_pool[n].msg_ctime = 1;
			return &msgque_pool[n];
		}
	}
	return NULL;
}

void msg_release_mq(struct msqid_ds *mq)
{
	memset_b(mq, 0, sizeof(struct msqid_ds));
}

struct msg msg_pool[MSGTQL];
struct msg *msg_get_new_md(void)
{
	unsigned int n;

	lock_resource(&ipcmsg_resource);

	for(n = 0; n < MSGTQL; n++) {
		if(msg_pool[n].msg_stime == 0) {
			msg_pool[n].msg_stime = 1;
			unlock_resource(&ipcmsg_resource);
			return &msg_pool[n];
		}
	}

	unlock_resource(&ipcmsg_resource);
	return NULL;
}

void msg_release_md(struct msg *msg)
{
	lock_resource(&ipcmsg_resource);
	memset_b(msg, 0, sizeof(struct msg));
	unlock_resource(&ipcmsg_resource);
}

void msg_init(void)
{
	int n;

	for(n = 0; n < MSGMNI; n++) {
		msgque[n] = (struct msqid_ds *)IPC_UNUSED;
	}
	memset_b(msgque_pool, 0, sizeof(msgque_pool));
	memset_b(msg_pool, 0, sizeof(msg_pool));
	num_queues = num_msgs = max_mqid = msg_seq = 0;
}

int sys_msgget(key_t key, int msgflg)
{
	struct msqid_ds *mq;
	struct ipc_perm *perm;
	int n;

#ifdef __DEBUG__
	printk("(pid %d) sys_msgget(%d, 0x%x)\n", current->pid, (int)key, msgflg);
#endif /*__DEBUG__ */

	if(key == IPC_PRIVATE) {
		/* create a new message queue */
		if(!(mq = msg_get_new_mq())) {
			return -ENOMEM;
		}
		for(n = 0; n < MSGMNI; n++) {
			if(msgque[n] == (struct msqid_ds *)IPC_UNUSED) {
				goto init;
			}
		}
		msg_release_mq(mq);
		return -ENOSPC;
	}

	mq = NULL;

	for(n = 0; n < MSGMNI; n++) {
		if(msgque[n] == (struct msqid_ds *)IPC_UNUSED) {
			continue;
		}
		if(key == msgque[n]->msg_perm.key) {
			mq = msgque[n];
			break;
		}
	}

	if(!mq) {
		if(!(msgflg & IPC_CREAT)) {
			return -ENOENT;
		}

		/* create a new message queue */
		if(!(mq = msg_get_new_mq())) {
			return -ENOMEM;
		}
		for(n = 0; n < MSGMNI; n++) {
			if(msgque[n] == (struct msqid_ds *)IPC_UNUSED) {
				goto init;
			}
		}
		msg_release_mq(mq);
		return -ENOSPC;
	} else {
		if((msgflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
			return -EEXIST;
		}
		if(!ipc_has_perms(&mq->msg_perm, msgflg)) {
			return -EACCES;
		}
		return (mq->msg_perm.seq * MSGMNI) + n;
	}

init:
	perm = &mq->msg_perm;
	perm->key = key;
	perm->uid = perm->cuid = current->euid;
	perm->gid = perm->cgid = current->egid;
	perm->mode = msgflg & 0777;
	perm->seq = msg_seq;
	mq->msg_first = mq->msg_last = NULL;
	mq->msg_stime = mq->msg_rtime = 0;
	mq->msg_ctime = CURRENT_TIME;
	mq->unused1 = mq->unused2 = 0;
	mq->msg_cbytes = mq->msg_qnum = 0;
	mq->msg_qbytes = MSGMNB;
	mq->msg_lspid = mq->msg_lrpid = 0;
	msgque[n] = mq;
	if(n > max_mqid) {
		max_mqid = n;
	}
	num_queues++;
	return (mq->msg_perm.seq * MSGMNI) + n;
}
#endif /* CONFIG_SYSVIPC */
