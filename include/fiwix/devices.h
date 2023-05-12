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

#define NR_BLKDEV	128	/* maximum number of block devices */
#define NR_CHRDEV	128	/* maximum number of char devices */

#define BLK_DEV		1	/* block device */
#define CHR_DEV		2	/* character device */

#define SET_MINOR(minors, bit)   ((minors[(bit) / 32]) |= (1 << ((bit) % 32)))
#define CLEAR_MINOR(minors, bit) ((minors[(bit) / 32]) &= ~(1 << ((bit) % 32)))
#define TEST_MINOR(minors, bit)	 ((minors[(bit) / 32]) & (1 << ((bit) % 32)))

struct device {
	char *name;
	unsigned char major;
	unsigned int minors[8];		/* bitmap of 256 bits */
	int blksize;
	void *device_data;		/* mostly used for minor sizes, in KB */
	struct fs_operations *fsop;
	struct device *next;
};

extern struct device *chr_device_table[NR_CHRDEV];
extern struct device *blk_device_table[NR_BLKDEV];

int register_device(int, struct device *);
struct device *get_device(int, __dev_t);
int chr_dev_open(struct inode *, struct fd *);
int blk_dev_open(struct inode *, struct fd *);
int blk_dev_close(struct inode *, struct fd *);
int blk_dev_read(struct inode *, struct fd *, char *, __size_t);
int blk_dev_write(struct inode *, struct fd *, const char *, __size_t);
int blk_dev_ioctl(struct inode *, int, unsigned long int);
int blk_dev_llseek(struct inode *, __off_t);

void dev_init(void);

#endif /* _FIWIX_DEVICES_H */
