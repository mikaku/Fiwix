/*
 * fiwix/include/fiwix/config.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CONFIG_H
#define _FIWIX_CONFIG_H

/* maximum number of processes */
#define NR_PROCS		64

/* maximum number of callout functions (timer) */
#define NR_CALLOUTS		NR_PROCS

/* maximum number of bottom halves in pool */
#define NR_BH			NR_PROCS

/* maximum number of mounted filesystems */
#define NR_MOUNT_POINTS		8

/* maximum number of opened files in system */
#define NR_OPENS		1024

/* maximum number of flocks in system */
#define NR_FLOCKS		(NR_PROCS * 5)



/* percentage of memory that buffer cache will borrow from available memory */
#define BUFFER_PERCENTAGE	100

/* percentage of hash buckets relative to the size of the buffer table */
#define BUFFER_HASH_PERCENTAGE	10

/* buffers reclaimed in a single call */
#define NR_BUF_RECLAIM		150


/* percentage of memory assigned to the inode table and hash table */
#define INODE_PERCENTAGE	5
#define INODE_HASH_PERCENTAGE	10

/* percentage of memory assigned to the page hash table */
#define PAGE_HASH_PERCENTAGE	10


/* maximum value for PID */
#define MAX_PID_VALUE		32767

/* number of screens in console' scroll back */
#define SCREENS_LOG		6

/* maximum number of messages on spurious interrupts */
#define MAX_SPU_NOTICES		10

/* maximum number of virtual memory mappings (regions) */
#define VMA_REGIONS		150

#endif /* _FIWIX_CONFIG_H */
