/*
 * fiwix/mm/swapper.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/config.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/ipc.h>
#include <fiwix/memdev.h>
#include <fiwix/serial.h>
#include <fiwix/lp.h>
#include <fiwix/ramdisk.h>
#include <fiwix/floppy.h>
#include <fiwix/ata.h>
#include <fiwix/buffer.h>
#include <fiwix/mm.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/pty.h>
#include <fiwix/net.h>
#include <fiwix/stdio.h>

/* kswapd continues the kernel initialization */
int kswapd(void)
{
	STI();

#ifdef CONFIG_SYSVIPC
	ipc_init();
#endif /* CONFIG_SYSVIPC */

	/* char devices */
	memdev_init();
	serial_init();
	lp_init();
#ifdef CONFIG_UNIX98_PTYS
	pty_init();
#endif /* CONFIG_UNIX98_PTYS */

	/* network */
#ifdef CONFIG_NET
	net_init();
#endif /* CONFIG_NET */

	/* block devices */
	ramdisk_init();
	floppy_init();
	ata_init();

	/* starting system */
	mem_stats();
	fs_init();
	mount_root();
	init_init();

	/* make sure interrupts are enabled after initializing devices */
	STI();

	for(;;) {
		sleep(&kswapd, PROC_INTERRUPTIBLE);
		if((kstat.pages_reclaimed = reclaim_buffers())) {
			continue;
		}
		wakeup(&get_free_page);
	}
}
