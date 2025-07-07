/*
 * fiwix/fs/devices.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct device *chr_device_table[NR_CHRDEV];
struct device *blk_device_table[NR_BLKDEV];

struct fs_operations def_chr_fsop = {
	0,
	0,

	chr_dev_open,
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* stats */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

struct fs_operations def_blk_fsop = {
	0,
	0,

	blk_dev_open,
	blk_dev_close,
	blk_dev_read,
	blk_dev_write,
	blk_dev_ioctl,
	blk_dev_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* stats */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int register_device(int type, struct device *new_d)
{
	struct device **d;
	int n, minors;

	switch(type) {
		case CHR_DEV:
			if(new_d->major >= NR_CHRDEV) {
				printk("%s(): character device major %d is greater than NR_CHRDEV (%d).\n", __FUNCTION__, new_d->major, NR_CHRDEV);
				return 1;
			}
			d = &chr_device_table[new_d->major];
			break;
		case BLK_DEV:
			if(new_d->major >= NR_BLKDEV) {
				printk("%s(): block device major %d is greater than NR_BLKDEV (%d).\n", __FUNCTION__, new_d->major, NR_BLKDEV);
				return 1;
			}
			d = &blk_device_table[new_d->major];
			break;
		default:
			printk("WARNING: %s(): invalid device type %d.\n", __FUNCTION__, type);
			return 1;
			break;
	}

	/* make sure there are minors defined */
	for(n = 0, minors = 0; n < 8; n++) {
		minors += new_d->minors[n];
	}
	if(!minors) {
		printk("WARNING: %s(): device major %d with no defined minors.\n", __FUNCTION__, new_d->major);
		return 1;
	}

	if(*d) {
		if(&(*d)->minors == &new_d->minors || ((*d)->next && &(*d)->next->minors == &new_d->minors)) {
			return 1;
		}
		do {
			d = &(*d)->next;
		} while(*d);
	}
	*d = new_d;

	return 0;
}

void unregister_device(int type, struct device *device)
{
	struct device **d, **tmp;
	int n;

	/* make sure there are not minors defined */
	for(n = 0; n < 8; n++) {
		if(device->minors[n]) {
			return;
		}
	}

	switch(type) {
		case CHR_DEV:
			d = &chr_device_table[device->major];
			break;
		default:
			printk("WARNING: %s(): invalid device type %d.\n", __FUNCTION__, type);
			return;
	}
	tmp = NULL;
	while(*d) {
		if(&(*d)->minors == &device->minors) {
			if(!tmp) {
				*d = (*d)->next;
			} else {
				*tmp = (*d)->next;
			}
			return;
		}
		tmp = d;
		d = &(*d)->next;
	}
}

struct device *get_device(int type, __dev_t dev)
{
	char *name;
	unsigned char major;
	struct device *d;

	major = MAJOR(dev);

	switch(type) {
		case CHR_DEV:
			if(major >= NR_CHRDEV) {
				printk("%s(): character device major %d is greater than NR_CHRDEV (%d).\n", __FUNCTION__, major, NR_CHRDEV);
				return NULL;
			}
			d = chr_device_table[major];
			name = "character";
			break;
		case BLK_DEV:
			if(major >= NR_BLKDEV) {
				printk("%s(): block device major %d is greater than NR_BLKDEV (%d).\n", __FUNCTION__, major, NR_BLKDEV);
				return NULL;
			}
			d = blk_device_table[major];
			name = "block";
			break;
		default:
			printk("WARNING: %s(): invalid device type %d.\n", __FUNCTION__, type);
			return NULL;
	}

	while(d) {
		if(d->major == major) {
			if(TEST_MINOR(d->minors, MINOR(dev))) {
				return d;
			}
			d = d->next;
			continue;
		}
		break;
	}

	printk("WARNING: %s(): %s device %d,%d not found.\n", __FUNCTION__, name, major, MINOR(dev));
	return NULL;
}

int chr_dev_open(struct inode *i, struct fd *f)
{
	struct device *d;

	if((d = get_device(CHR_DEV, i->rdev))) {
		i->fsop = d->fsop;
		return i->fsop->open(i, f);
	}

	return -ENXIO;
}

int blk_dev_open(struct inode *i, struct fd *f)
{
	struct device *d;

	if((d = get_device(BLK_DEV, i->rdev))) {
		return d->fsop->open(i, f);
	}

	return -ENXIO;
}

int blk_dev_close(struct inode *i, struct fd *f)
{
	struct device *d;

	if((d = get_device(BLK_DEV, i->rdev))) {
		return d->fsop->close(i, f);
	}

	return -ENXIO;
}

int blk_dev_read(struct inode *i, struct fd *f, char *buffer, __size_t count)
{
	__blk_t block;
	__size_t total_read;
	__loff_t device_size;
	int n, blksize, blksize_bits;
	unsigned int boffset, bytes;
	struct buffer *buf;
	struct device *d;

	if(!(d = get_device(BLK_DEV, i->rdev))) {
		return -ENXIO;
	}

	total_read = 0;
	if(!d->device_data) {
		printk("%s(): don't know the size of the block device %d,%d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
		return -EIO;
	}

	blksize = ((unsigned int *)d->blksize)[MINOR(i->rdev)];
	device_size = ((unsigned int *)d->device_data)[MINOR(i->rdev)];
	device_size *= 1024LLU;

	for(n = blksize, blksize_bits = 0; n != 1; n >>= 1) {
		blksize_bits++;
	}

	count = (f->offset + count > device_size) ? device_size - f->offset : count;
	if(!count || f->offset > device_size) {
		return 0;
	}
	while(count) {
		boffset = f->offset & (blksize - 1);	/* mod blksize */
		block = (f->offset >> blksize_bits);
		if(!(buf = bread(i->rdev, block, blksize))) {
			return -EIO;
		}
		bytes = blksize - boffset;
		bytes = MIN(bytes, count);
		memcpy_b(buffer + total_read, buf->data + boffset, bytes);
		total_read += bytes;
		count -= bytes;
		f->offset += bytes;
		brelse(buf);
	}
	return total_read;
}

int blk_dev_write(struct inode *i, struct fd *f, const char *buffer, __size_t count)
{
	__blk_t block;
	__size_t total_written;
	__loff_t device_size;
	int n, blksize, blksize_bits;
	unsigned int boffset, bytes;
	struct buffer *buf;
	struct device *d;

	if(!(d = get_device(BLK_DEV, i->rdev))) {
		return -ENXIO;
	}

	total_written = 0;
	if(!d->device_data) {
		printk("%s(): don't know the size of the block device %d,%d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
		return -EIO;
	}

	blksize = ((unsigned int *)d->blksize)[MINOR(i->rdev)];
	device_size = ((unsigned int *)d->device_data)[MINOR(i->rdev)];
	device_size *= 1024LLU;

	for(n = blksize, blksize_bits = 0; n != 1; n >>= 1) {
		blksize_bits++;
	}

	count = (f->offset + count > device_size) ? device_size - f->offset : count;
	if(!count || f->offset > device_size) {
		return -ENOSPC;
	}
	while(count) {
		boffset = f->offset & (blksize - 1);	/* mod blksize */
		block = (f->offset >> blksize_bits);
		if(!(buf = bread(i->rdev, block, blksize))) {
			return -EIO;
		}
		bytes = blksize - boffset;
		bytes = MIN(bytes, count);
		memcpy_b(buf->data + boffset, buffer + total_written, bytes);
		total_written += bytes;
		count -= bytes;
		f->offset += bytes;
		bwrite(buf);
	}
	return total_written;
}

int blk_dev_ioctl(struct inode *i, struct fd *f, int cmd, unsigned int arg)
{
	struct device *d;

	if((d = get_device(BLK_DEV, i->rdev))) {
		return d->fsop->ioctl(i, f, cmd, arg);
	}

	return -ENXIO;
}

__loff_t blk_dev_llseek(struct inode *i, __loff_t offset)
{
	struct device *d;

	if((d = get_device(BLK_DEV, i->rdev))) {
		return d->fsop->llseek(i, offset);
	}

	return -ENXIO;
}

void dev_init(void)
{
	memset_b(chr_device_table, 0, sizeof(chr_device_table));
	memset_b(blk_device_table, 0, sizeof(blk_device_table));
}
