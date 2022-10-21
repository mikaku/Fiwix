/*
 * fiwix/include/fiwix/config.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CONFIG_H
#define _FIWIX_CONFIG_H

/* kernel tuning */

#define NR_PROCS		64	/* max. number of processes */
#define NR_CALLOUTS		NR_PROCS	/* max. active callouts */
#define NR_MOUNT_POINTS		8	/* max. number of mounted filesystems */
#define NR_OPENS		1024	/* max. number of opened files */
#define NR_FLOCKS		(NR_PROCS * 5)	/* max. number of flocks */


#define BUFFER_PERCENTAGE	100	/* % of memory for buffer cache */
#define BUFFER_HASH_PERCENTAGE	10	/* % of hash buckets relative to the
					   size of the buffer table */
#define NR_BUF_RECLAIM		250	/* buffers reclaimed in a single shot */

#define INODE_PERCENTAGE	1	/* % of memory for to the inode table
					   and hash table */
#define INODE_HASH_PERCENTAGE	10	/* % of hash buckets relative to the
					   size of the inode table */

#define MAX_PID_VALUE		32767	/* max. value for PID */
#define SCREENS_LOG		6	/* max. number of screens in console's
					   scroll back */
#define MAX_SPU_NOTICES		10	/* max. number of messages on spurious
					   interrupts */
#define VMA_REGIONS		150	/* max. number of virtual memory maps */


/* toggle configuration options */

#undef CONFIG_VERBOSE_SEGFAULTS
#define CONFIG_PCI
#define CONFIG_PCI_NAMES
#undef CONFIG_SYSCALL_6TH_ARG
#define CONFIG_SYSVIPC
#undef CONFIG_QEMU_DEBUGCON
#define CONFIG_LAZY_USER_ADDR_CHECK

#endif /* _FIWIX_CONFIG_H */
