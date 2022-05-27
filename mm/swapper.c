/*
 * fiwix/mm/swapper.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/limits.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/tty.h>
#include <fiwix/memdev.h>
#include <fiwix/serial.h>
#include <fiwix/lp.h>
#include <fiwix/ramdisk.h>
#include <fiwix/floppy.h>
#include <fiwix/ide.h>
#include <fiwix/buffer.h>
#include <fiwix/mm.h>
#include <fiwix/fs.h>
#include <fiwix/locks.h>
#include <fiwix/filesystems.h>
#include <fiwix/stdio.h>
#include <fiwix/ipc.h>

/* kswapd continues the kernel initialization */
int kswapd(void)
{
	STI();

	/* char devices */
	memdev_init();
	serial_init();
	lp_init();

	/* block devices */
	ramdisk_init();
	floppy_init();
	ide_init();

	/* data structures */
	sleep_init();
	buffer_init();
	sched_init();
	mount_init();
	inode_init();
	fd_init();
	flock_init();

#ifdef CONFIG_IPC
	ipc_init();
#endif
	mem_stats();
	fs_init();
	mount_root();
	init_init();

	for(;;) {
		sleep(&kswapd, PROC_UNINTERRUPTIBLE);
		if((kstat.pages_reclaimed = reclaim_buffers())) {
			continue;
		}
		printk("WARNING: %s(): out of memory and swapping is not implemented yet, sorry.\n", __FUNCTION__);
		wakeup(&get_free_page);
	}
}
