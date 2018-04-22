/*
 * fiwix/kernel/syscalls/nanosleep.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

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

#ifdef __DEBUG__
	printk("(pid %d) sys_nanosleep(0x%08x, 0x%08x)\n", current->pid, (unsigned int)req, (unsigned int)rem);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, req, sizeof(struct timespec)))) {
		return errno;
	}
	if(req->tv_sec < 0 || req->tv_nsec >= 1000000000L || req->tv_nsec < 0) {
		return -EINVAL;
	}

	current->timeout = (req->tv_sec * HZ) + (req->tv_nsec * HZ / 1000000000L);
	if(current->timeout) {
		sleep(&sys_nanosleep, PROC_INTERRUPTIBLE);
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
