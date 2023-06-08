/*
 * fiwix/kernel/syscalls/ioperm.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

/*
 * This sys_ioperm() implementation, unlike what Linux 2.0 API said, covers all
 * the I/O address space. That is, up to 65536 I/O ports can be specified using
 * this system call, which makes sys_iopl() not needed anymore.
 */
int sys_ioperm(unsigned int from, unsigned int num, int turn_on)
{
	unsigned int n;

#ifdef __DEBUG__
	printk("(pid %d) sys_ioperm(0x%08x, 0x%08x, 0x%08x)\n", current->pid, from, num, turn_on);
#endif /*__DEBUG__ */

	if(!IS_SUPERUSER) {
		return -EPERM;
	}
	if(from + num >= (IO_BITMAP_SIZE * 8)) {
		return -EINVAL;
	}

	/*
	 * The Linux 2.0 API states that if 'turn_on' is non-zero the access to
	 * port must be guaranteed, otherwise the access must be denied.
	 *
	 * The Intel specification works in the opposite way. That is, if bit
	 * is zero the access to port is guaranteed, otherwise is not.
	 *
	 * That's the reason why we need to negate the value of 'turn_on' before
	 * changing the I/O permission bitmap.
	 */
	turn_on = !turn_on;

	for(n = from; n < (from + num); n++) {
		if(!turn_on) {
			current->tss.io_bitmap[n / 8] &= ~(1 << (n % 8));
		} else {
			current->tss.io_bitmap[n / 8] |= ~(1 << (n % 8));
		}
	}

	return 0;
}
