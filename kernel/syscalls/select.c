/*
 * fiwix/kernel/syscalls/select.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/process.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static int check_fds(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds)
{
	int n, bit;
	unsigned int set;

	n = 0;
	for(;;) {
		bit = n * __NFDBITS;
		if(bit >= nfds) {
			break;
		}
		set = rfds->fds_bits[n] | wfds->fds_bits[n] | efds->fds_bits[n];
		while(set) {
			if(__FD_ISSET(bit, rfds) || __FD_ISSET(bit, wfds) || __FD_ISSET(bit, efds)) {
				CHECK_UFD(bit);
			}
			set >>= 1;
			bit++;
		}
		n++;
	}

	return 0;
}

static int do_check(struct inode *i, struct fd *f, int flag)
{
	if(i->fsop && i->fsop->select) {
		if(i->fsop->select(i, f, flag)) {
			return 1;
		}
	}

	return 0;
}

int do_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds, fd_set *res_rfds, fd_set *res_wfds, fd_set *res_efds)
{
	int n, count;
	struct inode *i;

	count = 0;
	for(;;) {
		for(n = 0; n < nfds; n++) {
			if(!current->fd[n]) {
				continue;
			}
			i = fd_table[current->fd[n]].inode;
			if(__FD_ISSET(n, rfds)) {
				if(do_check(i, &fd_table[current->fd[n]], SEL_R)) {
					__FD_SET(n, res_rfds);
					count++;
				}
			}
			if(__FD_ISSET(n, wfds)) {
				if(do_check(i, &fd_table[current->fd[n]], SEL_W)) {
					__FD_SET(n, res_wfds);
					count++;
				}
			}
			if(__FD_ISSET(n, efds)) {
				if(do_check(i, &fd_table[current->fd[n]], SEL_E)) {
					__FD_SET(n, res_efds);
					count++;
				}
			}
		}

		if(count || !current->timeout || current->sigpending & ~current->sigblocked) {
			break;
		}
		if(sleep(&do_select, PROC_INTERRUPTIBLE)) {
			return -EINTR;
		}
	}

	return count;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	unsigned int t;
	fd_set rfds, wfds, efds;
	fd_set res_rfds, res_wfds, res_efds;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_select(%d, 0x%08x, 0x%08x, 0x%08x, 0x%08x [%d])\n", current->pid, nfds, (int)readfds, (int)writefds, (int)exceptfds, (int)timeout, (int)timeout ? tv2ticks(timeout): 0);
#endif /*__DEBUG__ */

	if(nfds < 0) {
		return -EINVAL;
	}
	if(nfds > MIN(__FD_SETSIZE, NR_OPENS)) {
		nfds = MIN(__FD_SETSIZE, NR_OPENS);
	}

	if(readfds) {
		if((errno = check_user_area(VERIFY_WRITE, readfds, sizeof(fd_set)))) {
			return errno;
		}
		memcpy_b(&rfds, readfds, sizeof(fd_set));
	} else {
		__FD_ZERO(&rfds);
	}
	if(writefds) {
		if((errno = check_user_area(VERIFY_WRITE, writefds, sizeof(fd_set)))) {
			return errno;
		}
		memcpy_b(&wfds, writefds, sizeof(fd_set));
	} else {
		__FD_ZERO(&wfds);
	}
	if(exceptfds) {
		if((errno = check_user_area(VERIFY_WRITE, exceptfds, sizeof(fd_set)))) {
			return errno;
		}
		memcpy_b(&efds, exceptfds, sizeof(fd_set));
	} else {
		__FD_ZERO(&efds);
	}

	/* check the validity of all fds */
	if((errno = check_fds(nfds, &rfds, &wfds, &efds)) < 0) {
		return errno;
	}

	if(timeout) {
		t = tv2ticks(timeout);
	} else {
		t = INFINITE_WAIT;
	}

	__FD_ZERO(&res_rfds);
	__FD_ZERO(&res_wfds);
	__FD_ZERO(&res_efds);

	current->timeout = t;
	if((errno = do_select(nfds, &rfds, &wfds, &efds, &res_rfds, &res_wfds, &res_efds)) < 0) {
		return errno;
	}
	t = current->timeout;
	current->timeout = 0;

	if(readfds) {
		memcpy_b(readfds, &res_rfds, sizeof(fd_set));
	}
	if(writefds) {
		memcpy_b(writefds, &res_wfds, sizeof(fd_set));
	}
	if(exceptfds) {
		memcpy_b(exceptfds, &res_efds, sizeof(fd_set));
	}
	if(timeout) {
		ticks2tv(t, timeout);
	}
	return errno;
}
