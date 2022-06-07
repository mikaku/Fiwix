/*
 * fiwix/kernel/syscalls/msgctl.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/string.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>
#include <fiwix/ipc.h>
#include <fiwix/msg.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
int sys_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
	struct msqid_ds *mq;
	struct msginfo *mi;
	struct ipc_perm *perm;
	struct msg *m, *mn;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_msgctl(%d, %d, 0x%x)\n", current->pid, msqid, cmd, (int)buf);
#endif /*__DEBUG__ */

	if(msqid < 0) {
		return -EINVAL;
	}

	switch(cmd) {	
		case MSG_STAT:
		case IPC_STAT:
			if((errno = check_user_area(VERIFY_WRITE, buf, sizeof(struct msqid_ds)))) {
				return errno;
			}
			mq = msgque[msqid % MSGMNI];
			if(mq == IPC_UNUSED) {
				return -EINVAL;
			}
			if(!ipc_has_perms(&mq->msg_perm, IPC_R)) {
				return -EACCES;
			}
			memcpy_b(buf, mq, sizeof(struct msqid_ds));
			if(cmd == MSG_STAT) {
				return (mq->msg_perm.seq * MSGMNI) + msqid;
			}
			return 0;
		case IPC_SET:
			if((errno = check_user_area(VERIFY_READ, buf, sizeof(struct msqid_ds)))) {
				return errno;
			}
			mq = msgque[msqid % MSGMNI];
			if(mq == IPC_UNUSED) {
				return -EINVAL;
			}
			perm = &mq->msg_perm;
			if(!IS_SUPERUSER && current->euid != perm->uid && current->euid != perm->cuid) {
				return -EPERM;
			}
			if(!IS_SUPERUSER && buf->msg_qbytes > MSGMNB) {
				return -EPERM;
			}
			mq->msg_qbytes = buf->msg_qbytes;
			perm->uid = perm->uid;
			perm->gid = perm->gid;
			perm->mode = (perm->mode & ~0777) | (perm->mode & 0777);
			mq->msg_ctime = CURRENT_TIME;
			return 0;
		case IPC_RMID:
			mq = msgque[msqid % MSGMNI];
			if(mq == IPC_UNUSED) {
				return -EINVAL;
			}
			perm = &mq->msg_perm;
			if(!IS_SUPERUSER && current->euid != perm->uid && current->euid != perm->cuid) {
				return -EPERM;
			}
			if((m = mq->msg_first)) {
				do {
					mq->msg_qnum--;
					mq->msg_cbytes -= m->msg_ts;
					num_msgs--;
					mn = m->msg_next;
					msg_release_md(m);
					m = mn;
				} while(m);
			}
			msg_release_mq(mq);
			msgque[msqid % MSGMNI] = (struct msqid_ds *)IPC_UNUSED;
			num_queues--;
			msg_seq++;
			wakeup(mq);
			return 0;
		case MSG_INFO:
		case IPC_INFO:
			if((errno = check_user_area(VERIFY_WRITE, buf, sizeof(struct msqid_ds)))) {
				return errno;
			}
			mi = (struct msginfo *)buf;
			if(cmd == MSG_INFO) {
				mi->msgpool = num_queues;
				mi->msgmap = num_msgs;
				mi->msgssz = 0;		/* FIXME: pending to do */
				mi->msgtql = 0;		/* FIXME: pending to do */
				mi->msgseg = 0;		/* FIXME: pending to do */
			} else {
				mi->msgpool = 0;	/* FIXME: pending to do */
				mi->msgmap = 0;		/* FIXME: pending to do */
				mi->msgssz = 0;		/* FIXME: pending to do */
				mi->msgtql = MSGTQL;
				mi->msgseg = 0;		/* FIXME: pending to do */
			}
			mi->msgmax = MSGMAX;
			mi->msgmnb = MSGMNB;
			mi->msgmni = MSGMNI;
			return max_mqid;
	}

	return -EINVAL;
}
#endif
