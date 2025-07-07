/*
 * fiwix/include/fiwix/devices.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_DEVICES_H
#define _FIWIX_DEVICES_H

#include <fiwix/types.h>
#include <fiwix/fs.h>

#define NR_BLKDEV	255	/* maximum number of block devices */
#define NR_CHRDEV	255	/* maximum number of char devices */
#define BLK_DEV		1	/* block device */
#define CHR_DEV		2	/* character device */
#define MAX_MINORS	256	/* maximum number of minors per device */
#define MINOR_BITS	(MAX_MINORS / (sizeof(unsigned int) * 8))

#define SET_MINOR(minors, bit)   ((minors[(bit) / 32]) |= (1 << ((bit) % 32)))
#define CLEAR_MINOR(minors, bit) ((minors[(bit) / 32]) &= ~(1 << ((bit) % 32)))
#define TEST_MINOR(minors, bit)	 ((minors[(bit) / 32]) & (1 << ((bit) % 32)))

struct device {
	char *name;
	unsigned char major;
	unsigned int minors[MINOR_BITS];/* bitmap of MAX_MINORS bits */
	unsigned int *blksize;		/* default minor blocksizes, in KB */
	void *device_data;		/* mostly used for minor sizes, in KB */
	struct fs_operations *fsop;
	void *requests_queue;
	void *xfer_data;
	struct device *next;
};

extern struct device *chr_device_table[NR_CHRDEV];
extern struct device *blk_device_table[NR_BLKDEV];

int register_device(int, struct device *);
void unregister_device(int, struct device *);
struct device *get_device(int, __dev_t);
int chr_dev_open(struct inode *, struct fd *);
int blk_dev_open(struct inode *, struct fd *);
int blk_dev_close(struct inode *, struct fd *);
int blk_dev_read(struct inode *, struct fd *, char *, __size_t);
int blk_dev_write(struct inode *, struct fd *, const char *, __size_t);
int blk_dev_ioctl(struct inode *, struct fd *, int, unsigned int);
__loff_t blk_dev_llseek(struct inode *, __loff_t);

void dev_init(void);

#endif /* _FIWIX_DEVICES_H */
