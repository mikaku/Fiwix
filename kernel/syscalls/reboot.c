/*
 * fiwix/kernel/syscalls/reboot.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/syscalls.h>
#include <fiwix/reboot.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_reboot(int magic1, int magic2, int flag)
{
#ifdef __DEBUG__
	printk("(pid %d) sys_reboot(0x%08x, %d, 0x%08x)\n", current->pid, magic1, magic2, flag);
#endif /*__DEBUG__ */

	if(!IS_SUPERUSER) {
		return -EPERM;
	}
	if((magic1 != BMAGIC_1) || (magic2 != BMAGIC_2)) {
		return -EINVAL;
	}
	switch(flag) {
		case BMAGIC_SOFT:
			ctrl_alt_del = 0;
			break;
		case BMAGIC_HARD:
			ctrl_alt_del = 1;
			break;
		case BMAGIC_REBOOT:
			reboot();
			break;
		case BMAGIC_HALT:
			sys_kill(-1, SIGKILL);
			stop_kernel();
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
