/*
 * fiwix/include/fiwix/ramdisk.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_RAMDISK_H
#define _FIWIX_RAMDISK_H

#include <fiwix/fs.h>

#define RAMDISK_MAJOR	1	/* ramdisk device major number */
#define RAMDISK_MINORS	1	/* number of minors */
#define RAMDISK_SIZE	4096	/* default ramdisk size in KBs */
#define RAMDISK_MAXSIZE	131072	/* maximum ramdisk size in KBs */

struct ramdisk {
	char *addr;		/* ramdisk memory address */
};

extern struct ramdisk ramdisk_table[RAMDISK_MINORS];

int ramdisk_open(struct inode *, struct fd *);
int ramdisk_close(struct inode *, struct fd *);
int ramdisk_read(__dev_t, __blk_t, char *, int);
int ramdisk_write(__dev_t, __blk_t, char *, int);
int ramdisk_ioctl(struct inode *, int, unsigned long int);
int ramdisk_lseek(struct inode *, __off_t);

void ramdisk_init(void);

#endif /* _FIWIX_RAMDISK_H */
