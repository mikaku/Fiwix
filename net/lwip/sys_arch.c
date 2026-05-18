#include <lwip/sys.h>
#include <fiwix/sleep.h>
#include <fiwix/mm.h>
#include <fiwix/string.h>
#include <fiwix/timer.h>
#include <fiwix/asm.h>
#include <fiwix/sched.h>
#include <fiwix/kernel.h>

#define MS_TO_TICKS(ms)		((ms) * HZ / 1000)
#define TICKS_TO_MS(ticks)	((ticks) * 1000 / HZ)

static struct resource protect_lk;

/* SEMAPHORES */

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	if(current->pid == 2) {
		return ERR_OK;
	}
	memset_b(sem, 0, sizeof(struct semaphore));
	sem->sem.locked = count;
	sem->valid = 1;
	return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
	/* Optimization: don't disable interrupts, etc if there is nothing to do. */
	if (!sem->sem.locked) {
		return;
	}
	unlock_resource(&sem->sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	int retval;

	if(timeout) {
		timeout = MS_TO_TICKS(timeout);
		if(timeout < 1) {
			timeout = 1;
		}
	} else {
		timeout = INFINITE_WAIT;
	}
	if((retval = lock_resource_timeout(&sem->sem, timeout))) {
		if(retval != 1) {
			current->flags |= PF_LWIPINTR;
		}
		return SYS_ARCH_TIMEOUT;
	}
	return 0;
}

void sys_sem_free(sys_sem_t *sem)
{
	memset_b(sem, 0, sizeof(struct semaphore));
}

int sys_sem_valid(sys_sem_t *sem)
{
	return sem->valid;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
	sem->valid = 0;
}

/* MAILBOXES */

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	memset_b(mbox, 0, sizeof(struct mailbox));
	mbox->max_length = size;
	mbox->valid = 1;
	lock_resource(&mbox->empty);
	return ERR_OK;
}

static void mbox_dopost(sys_mbox_t *mbox, void *msg);
static void *mbox_dofetch(sys_mbox_t *mbox);

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
	lock_resource(&mbox->full);
	lock_resource(&mbox->lock);

	mbox_dopost(mbox, msg);
	if(mbox->length < mbox->max_length) {
		unlock_resource(&mbox->full);
	}

	unlock_resource(&mbox->empty);
	unlock_resource(&mbox->lock);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
	lock_resource(&mbox->lock);
	if(mbox->full.locked) {
		unlock_resource(&mbox->lock);
		return ERR_MEM;
	}
	lock_resource(&mbox->full);

	mbox_dopost(mbox, msg);
	if(mbox->length < mbox->max_length) {
		unlock_resource(&mbox->full);
	}

	unlock_resource(&mbox->empty);
	unlock_resource(&mbox->lock);
	return ERR_OK;
}

/* XXX may not be needed in Fiwix */
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
	if(mbox->length >= mbox->max_length) {
		return ERR_MEM;
	}

	mbox_dopost(mbox, msg);
	return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	int retval;

	if(timeout) {
		timeout = MS_TO_TICKS(timeout);
		if(timeout < 1) {
			timeout = 1;
		}
	} else {
		timeout = INFINITE_WAIT;
	}
	if((retval = lock_resource_timeout(&mbox->empty, timeout))) {
		if(retval != 1) {
			current->flags |= PF_LWIPINTR;
		}
		return SYS_ARCH_TIMEOUT;
	}

	lock_resource(&mbox->lock);
	if(msg != NULL) {
		*msg = mbox_dofetch(mbox);
	} else {
		mbox_dofetch(mbox);	/* discarded */
	}
	if(mbox->length > 0) {
		unlock_resource(&mbox->empty);
	}
	unlock_resource(&mbox->full);
	unlock_resource(&mbox->lock);
	return ERR_OK;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	if(mbox->empty.locked) {
		unlock_resource(&mbox->lock);
		return SYS_MBOX_EMPTY;
	}
	lock_resource(&mbox->lock);
	lock_resource(&mbox->empty);

	*msg = mbox_dofetch(mbox);
	if(mbox->length > 0) {
		unlock_resource(&mbox->empty);
	}
	unlock_resource(&mbox->full);
	unlock_resource(&mbox->lock);
	return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
	lock_resource(&mbox->lock);
	if(mbox->length != 0) {
		printf("ERROR: lwIP mailboxes: mailbox still had mail in it\n");
	}
	memset_b(mbox, 0, sizeof(struct mailbox));
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
	return mbox->valid;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	lock_resource(&mbox->lock);
	mbox->valid = 0;
	unlock_resource(&mbox->lock);
}

static void mbox_dopost(sys_mbox_t *mbox, void *msg)
{
	struct mail *entry, *q;

	entry = (struct mail *)kmalloc(sizeof(struct mail));
	memset_b(entry, 0, sizeof(struct mail));
	entry->msg = msg;

	if((q = mbox->queue)) {
		while(q->next) {
			q = q->next;
		}
		q->next = entry;
	} else {
		mbox->queue = entry;
	}
	mbox->length++;
}

static void *mbox_dofetch(sys_mbox_t *mbox)
{
	void *msg;
	struct mail *old_queue;

	msg = mbox->queue->msg;
	old_queue = mbox->queue;
	mbox->queue = mbox->queue->next;
	mbox->length--;
	kfree((unsigned int)old_queue);
	return msg;
}

/* MISC */

void sys_msleep(u32_t ms)
{
	unsigned int flags;
	/* see kernel/syscalls/nanosleep.c */
	SAVE_FLAGS(flags); CLI();
	current->timeout = MS_TO_TICKS(ms);
	sleep(&sys_msleep, PROC_UNINTERRUPTIBLE);
	RESTORE_FLAGS(flags);
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
                    int stacksize, int prio) {
	/* This cast is fine, a int is the same size as a void * */
	return kernel_process_arg(name, (int (*)(void *))thread, arg);
}

void sys_init(void) {
	memset_b(&protect_lk, 0, sizeof(struct resource));
}

void *my_calloc(__size_t n, __size_t size)
{
	void *mem = (void *)kmalloc(n * size);
	memset_b(mem, 0, size * n);
	return mem;
}

/* TIME */

u32_t sys_now(void)
{
	u32_t now;

	/* Based on gettimeofday syscall */
	now = ((CURRENT_TICKS % HZ) * 1000) / HZ;
	now += (gettimeoffset() / 10);
	return now;
}

/* CRITICAL SECTIONS */

int sys_arch_protect(void)
{
	lock_resource(&protect_lk);
	return 0;
}

void sys_arch_unprotect(sys_prot_t lev)
{
	unlock_resource(&protect_lk);
}
