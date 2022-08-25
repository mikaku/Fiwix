/*
 * fiwix/kernel/syscalls/msgrcv.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/string.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/mm.h>
#include <fiwix/ipc.h>
#include <fiwix/msg.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
int sys_msgrcv(int msqid, void *msgp, __size_t msgsz, long int msgtyp, int msgflg)
{
	struct msqid_ds *mq;
	struct msgbuf *mb;
	struct msg *m;
	int errno, count;

#ifdef __DEBUG__
	printk("(pid %d) sys_msgrcv(%d, 0x%08x, %d, %d, 0x%x)\n", current->pid, msqid, (int)msgp, msgsz, msgtyp, msgflg);
#endif /*__DEBUG__ */

	if(msqid < 0) {
		return -EINVAL;
	}
	if((errno = check_user_area(VERIFY_WRITE, msgp, sizeof(void *)))) {
		return errno;
	}
	mq = msgque[msqid % MSGMNI];
	if(mq == IPC_UNUSED) {
		return -EINVAL;
	}
	for(;;) {
		if(!ipc_has_perms(&mq->msg_perm, IPC_R)) {
			return -EACCES;
		}
		if((m = mq->msg_first)) {
			if(!msgtyp) {
				break;
			}
		}
		if(msgflg & IPC_NOWAIT) {
			return -ENOMSG;
		}
		if(sleep(mq, PROC_INTERRUPTIBLE)) {
			return -EINTR;
		}
		mq = msgque[msqid % MSGMNI];
		if(mq == IPC_UNUSED) {
			return -EIDRM;
		}
	}

	if(msgsz < m->msg_ts) {
		if(!(msgflg & MSG_NOERROR)) {
			return -E2BIG;
		}
		count = msgsz;
	} else {
		count = m->msg_ts;
	}

	mb = (struct msgbuf *)msgp;
	mb->mtype = m->msg_type;
	memcpy_b(mb->mtext, m->msg_spot, count);

	lock_resource(&ipcmsg_resource);
	kfree((unsigned int)m->msg_spot);
	mq->msg_first = m->msg_next;
	mq->msg_rtime = mq->msg_ctime = CURRENT_TIME;
	mq->msg_qnum--;
	mq->msg_cbytes -= m->msg_ts;
	mq->msg_lrpid = current->pid;
	num_msgs--;
	unlock_resource(&ipcmsg_resource);
	msg_release_md(m);
	wakeup(mq);
	return count;
}
#endif /* CONFIG_IPC */
