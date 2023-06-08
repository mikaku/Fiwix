/*
 * fiwix/kernel/syscalls/old_select.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/syscalls.h>
#include <fiwix/fs.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int old_select(unsigned int *params)
{
	int nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *exceptfds;
	struct timeval *timeout;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) old_select(0x%08x)\n", current->pid, (int)params);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, (void *)params, sizeof(unsigned int) * 5))) {
		return errno;
	}
	nfds = *(int *)params;
	readfds = *(fd_set **)(params + 1);
	writefds = *(fd_set **)(params + 2);
	exceptfds = *(fd_set **)(params + 3);
	timeout = *(struct timeval **)(params + 4);

	return sys_select(nfds, readfds, writefds, exceptfds, timeout);
}
