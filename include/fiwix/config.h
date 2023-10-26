/*
 * fiwix/include/fiwix/config.h
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CONFIG_H
#define _FIWIX_CONFIG_H

/* kernel tuning */
#ifndef NR_PROCS
#define NR_PROCS		64	/* max. number of processes */
#endif
#ifndef NR_CALLOUTS
#define NR_CALLOUTS		NR_PROCS	/* max. active callouts */
#endif
#ifndef NR_MOUNT_POINTS
#define NR_MOUNT_POINTS		8	/* max. number of mounted filesystems */
#endif
#ifndef NR_OPENS
#define NR_OPENS		1024	/* max. number of opened files */
#endif
#ifndef NR_FLOCKS
#define NR_FLOCKS		(NR_PROCS * 5)	/* max. number of flocks */
#endif

#ifndef FREE_PAGES_RATIO
#define FREE_PAGES_RATIO	5	/* % minimum of free memory pages */
#endif
#ifndef BUFFER_PERCENTAGE
#define BUFFER_PERCENTAGE	100	/* % of memory for buffer cache */
#endif
#ifndef BUFFER_HASH_PERCENTAGE
#define BUFFER_HASH_PERCENTAGE	10	/* % of hash buckets relative to the
					   size of the buffer table */
#endif
#ifndef NR_BUF_RECLAIM
#define NR_BUF_RECLAIM		250	/* buffers reclaimed in a single shot */
#endif
#ifndef BUFFER_DIRTY_RATIO
#define BUFFER_DIRTY_RATIO	5	/* % of dirty buffers in buffer cache */
#endif
#ifndef INODE_PERCENTAGE
#define INODE_PERCENTAGE	1	/* % of memory for the inode table and
					   hash table */
#endif
#ifndef INODE_HASH_PERCENTAGE
#define INODE_HASH_PERCENTAGE	10	/* % of hash buckets relative to the
					   size of the inode table */
#endif

#ifndef MAX_PID_VALUE
#define MAX_PID_VALUE		32767	/* max. value for PID */
#endif
#ifndef SCREENS_LOG
#define SCREENS_LOG		6	/* max. number of screens in console's
					   scroll back */
#endif
#ifndef MAX_SPU_NOTICES
#define MAX_SPU_NOTICES		10	/* max. number of messages on spurious
					   interrupts */
#endif


/* toggle configuration options */
#ifndef NO_CONFIG_PCI
#define CONFIG_PCI
#endif
#ifndef NO_CONFIG_PCI_NAMES
#define CONFIG_PCI_NAMES
#endif
/* #define CONFIG_SYSCALL_6TH_ARG */
#ifndef NO_CONFIG_SYSVIPC
#define CONFIG_SYSVIPC
#endif
#ifndef NO_CONFIG_LAZY_USER_ADDR_CHECK
#define CONFIG_LAZY_USER_ADDR_CHECK
#endif
#ifndef NO_CONFIG_BGA
#define CONFIG_BGA
#endif
/* #define CONFIG_KEXEC */
#ifndef NO_CONFIG_OFFSET64
#define CONFIG_OFFSET64
#endif
/* #define CONFIG_VM_SPLIT22 */
/* #define CONFIG_FS_MINIX */


/* configuration options to help debugging */
#ifndef NO_CONFIG_VERBOSE_SEGFAULTS
#define CONFIG_VERBOSE_SEGFAULTS
#endif
/* #define CONFIG_QEMU_DEBUGCON */

#endif /* _FIWIX_CONFIG_H */
