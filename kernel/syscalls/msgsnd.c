/*
 * fiwix/kernel/syscalls/msgsnd.c
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
int sys_msgsnd(int msqid, const void *msgp, __size_t msgsz, int msgflg)
{
	struct msqid_ds *mq;
	struct msgbuf *mb;
	struct msg *m;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_msgsnd(%d, 0x%08x, %d, 0x%x)\n", current->pid, msqid, (int)msgp, msgsz, msgflg);
#endif /*__DEBUG__ */

	if(msqid < 0 || msgsz > MSGMAX) {
		return -EINVAL;
	}
	if((errno = check_user_area(VERIFY_READ, msgp, sizeof(void *)))) {
		return errno;
	}
	mb = (struct msgbuf *)msgp;
	if(mb->mtype < 0) {
		return -EINVAL;
	}
	if((errno = check_user_area(VERIFY_READ, mb->mtext, msgsz))) {
		return errno;
	}

	mq = msgque[msqid % MSGMNI];
	if(mq == IPC_UNUSED) {
		return -EINVAL;
	}
	for(;;) {
		if(!ipc_has_perms(&mq->msg_perm, IPC_W)) {
			return -EACCES;
		}
		if(mq->msg_cbytes + msgsz > mq->msg_qbytes || mq->msg_qnum + 1 > mq->msg_qbytes) {
			if(msgflg & IPC_NOWAIT) {
				return -EAGAIN;
			}
			if(sleep(mq, PROC_INTERRUPTIBLE)) {
				return -EINTR;
			}
		}
		if(mq == IPC_UNUSED) {
			return -EIDRM;
		}
		break;
	}

	if(!(m = msg_get_new_md())) {
		return -ENOMEM;
	}
	m->msg_next = NULL;
	m->msg_type = mb->mtype;
	if(!(m->msg_spot = (void *)kmalloc())) {
		msg_release_md(m);
                return -ENOMEM;
        }
	memcpy_b(m->msg_spot, mb->mtext, msgsz);
	m->msg_stime = CURRENT_TIME;
	m->msg_ts = msgsz;
	lock_resource(&ipcmsg_resource);
	if(!mq->msg_first) {
		mq->msg_first = mq->msg_last = m;
	} else {
		mq->msg_last->msg_next = m;
		mq->msg_last = m;
	}
	mq->msg_stime = mq->msg_ctime = CURRENT_TIME;
	mq->msg_qnum++;
	mq->msg_cbytes += msgsz;
	mq->msg_lspid = current->pid;
	num_msgs++;
	unlock_resource(&ipcmsg_resource);
	wakeup(mq);
	return 0;
}
#endif
