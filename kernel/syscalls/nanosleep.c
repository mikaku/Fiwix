/*
 * fiwix/kernel/syscalls/nanosleep.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/fs.h>
#include <fiwix/time.h>
#include <fiwix/timer.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_nanosleep(const struct timespec *req, struct timespec *rem)
{
	int errno;
	long int nsec;
	unsigned long int timeout;
	unsigned int flags;

#ifdef __DEBUG__
	printk("(pid %d) sys_nanosleep(0x%08x, 0x%08x)\n", current->pid, (unsigned int)req, (unsigned int)rem);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, req, sizeof(struct timespec)))) {
		return errno;
	}
	if(req->tv_sec < 0 || req->tv_nsec >= 1000000000L || req->tv_nsec < 0) {
		return -EINVAL;
	}

	/*
	 * Since the current maximum precision of the kernel is only 10ms, we
	 * need to convert any lower request to a minimum of 10ms, even knowing
	 * that this might increase the sleep a bit more than the requested.
	 */
	nsec = req->tv_nsec;
	if(nsec < 10000000L) {
		nsec *= 10;
	}

	/*
	 * Interrupts must be disabled before setting current->timeout in order
	 * to avoid a race condition. Otherwise it might occur that timeout is
	 * so small that it would reach zero before the call to sleep(). In this
	 * case, the process would miss the wakeup() and would stay in the sleep
	 * queue forever.
	 */
	timeout = (req->tv_sec * HZ) + (nsec * HZ / 1000000000L);
	if(timeout) {
		SAVE_FLAGS(flags); CLI();
		current->timeout = timeout;
		sleep(&sys_nanosleep, PROC_INTERRUPTIBLE);
		RESTORE_FLAGS(flags);
		if(current->timeout) {
			if(rem) {
				if((errno = check_user_area(VERIFY_WRITE, rem, sizeof(struct timespec)))) {
					return errno;
				}
				rem->tv_sec = current->timeout / HZ;
				rem->tv_nsec = (current->timeout % HZ) * 1000000000L / HZ;
			}
			return -EINTR;
		}
	}
	return 0;
}
