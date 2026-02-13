/*
 * fiwix/kernel/syscalls/socketcall.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/net.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_socketcall(int call, unsigned int *args)
{
#ifdef CONFIG_NET
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_socketcall(%d, 0x%08x)\n", current->pid, call, args);
#endif /*__DEBUG__ */

	switch(call) {
		case SYS_SOCKET:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 3))) {
				return errno;
			}
			return socket(args[0], args[1], args[2]);
		case SYS_BIND:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 3))) {
				return errno;
			}
			return bind(args[0], (struct sockaddr *)args[1], args[2]);
		case SYS_CONNECT:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 3))) {
				return errno;
			}
			return connect(args[0], (struct sockaddr *)args[1], args[2]);
		case SYS_LISTEN:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 2))) {
				return errno;
			}
			return listen(args[0], args[1]);
		case SYS_ACCEPT:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 3))) {
				return errno;
			}
			return accept(args[0], (struct sockaddr *)args[1], (unsigned int *)args[2]);
		case SYS_GETSOCKNAME:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 3))) {
				return errno;
			}
			return getname(args[0], (struct sockaddr *)args[1], (unsigned int *)args[2], SYS_GETSOCKNAME);
		case SYS_GETPEERNAME:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 3))) {
				return errno;
			}
			return getname(args[0], (struct sockaddr *)args[1], (unsigned int *)args[2], SYS_GETPEERNAME);
		case SYS_SOCKETPAIR:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 4))) {
				return errno;
			}
			return socketpair(args[0], args[1], args[2], (int *)args[3]);
		case SYS_SEND:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 4))) {
				return errno;
			}
			return send(args[0], (void *)args[1], args[2], args[3]);
		case SYS_RECV:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 4))) {
				return errno;
			}
			return recv(args[0], (void *)args[1], args[2], args[3]);
		case SYS_SENDTO:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 6))) {
				return errno;
			}
			return sendto(args[0], (void *)args[1], args[2], args[3], (struct sockaddr *)args[4], args[5]);
		case SYS_RECVFROM:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 6))) {
				return errno;
			}
			return recvfrom(args[0], (void *)args[1], args[2], args[3], (struct sockaddr *)args[4], (int *)args[5]);
		case SYS_SHUTDOWN:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 2))) {
				return errno;
			}
			return shutdown(args[0], args[1]);
		case SYS_SETSOCKOPT:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 5))) {
				return errno;
			}
			return setsockopt(args[0], args[1], args[2], (void *)args[3], args[4]);
		case SYS_GETSOCKOPT:
			if((errno = check_user_area(VERIFY_READ, args, sizeof(unsigned int) * 5))) {
				return errno;
			}
			return getsockopt(args[0], args[1], args[2], (void *)args[3], (socklen_t *)args[4]);
	}

	return -EINVAL;
#else
	return -ENOSYS;
#endif /* CONFIG_NET */
}
