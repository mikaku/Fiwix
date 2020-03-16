/*
 * fiwix/fs/devices.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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

struct device chr_device_table[NR_CHRDEV];
struct device blk_device_table[NR_BLKDEV];

struct fs_operations def_chr_fsop = {
	0,
	0,

	chr_dev_open,
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* lseek */
	NULL,			/* readdir */
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
	blk_dev_lseek,
	NULL,			/* readdir */
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
	struct device *d;

	switch(type) {
		case CHR_DEV:
			if(new_d->major >= NR_CHRDEV) {
				printk("%s(): character device major %d is greater than NR_CHRDEV (%d).\n", __FUNCTION__, new_d->major, NR_CHRDEV);
				return 1;
			}
			d = chr_device_table;
			break;
		case BLK_DEV:
			if(new_d->major >= NR_BLKDEV) {
				printk("%s(): block device major %d is greater than NR_BLKDEV (%d).\n", __FUNCTION__, new_d->major, NR_BLKDEV);
				return 1;
			}
			d = blk_device_table;
			break;
		default:
			printk("WARNING: %s(): invalid device type %d.\n", __FUNCTION__, type);
			return 1;
			break;
	}

	if(d[new_d->major].major) {
		printk("%s(): device '%s' with major %d is already registered.\n", __FUNCTION__, new_d->name, new_d->major);
		return 1;
	}
	memcpy_b(d + new_d->major, new_d, sizeof(struct device));
	return 0;
}

struct device * get_device(int type, unsigned char major)
{
	char *name;
	struct device *d;

	switch(type) {
		case CHR_DEV:
			if(major >= NR_CHRDEV) {
				printk("%s(): character device major %d is greater than NR_CHRDEV (%d).\n", __FUNCTION__, major, NR_CHRDEV);
				return NULL;
			}
			d = chr_device_table;
			name = "character";
			break;
		case BLK_DEV:
			if(major >= NR_BLKDEV) {
				printk("%s(): block device major %d is greater than NR_BLKDEV (%d).\n", __FUNCTION__, major, NR_BLKDEV);
				return NULL;
			}
			d = blk_device_table;
			name = "block";
			break;
		default:
			printk("WARNING: %s(): invalid device type %d.\n", __FUNCTION__, type);
			return NULL;
	}

	if(d[major].major) {
		return &d[major];
	}

	printk("WARNING: %s(): no %s device found with major %d.\n", __FUNCTION__, name, major);
	return NULL;
}

int chr_dev_open(struct inode *i, struct fd *fd_table)
{
	struct device *d;

	if((d = get_device(CHR_DEV, MAJOR(i->rdev)))) {
		i->fsop = d->fsop;
		if(i->fsop && i->fsop->open) {
			return i->fsop->open(i, fd_table);
		}
	}

	return -EINVAL;
}

int blk_dev_open(struct inode *i, struct fd *fd_table)
{
	struct device *d;

	if((d = get_device(BLK_DEV, MAJOR(i->rdev)))) {
		if(d->fsop && d->fsop->open) {
			return d->fsop->open(i, fd_table);
		}
	}

	return -EINVAL;
}

int blk_dev_close(struct inode *i, struct fd *fd_table)
{
	struct device *d;

	if((d = get_device(BLK_DEV, MAJOR(i->rdev)))) {
		if(d->fsop && d->fsop->close) {
			return d->fsop->close(i, fd_table);
		}
	}

	printk("WARNING: %s(): block device %d,%d does not have the close() method.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
	return -EINVAL;
}

int blk_dev_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	__blk_t block;
	__off_t total_read;
	unsigned long long int device_size;
	int blksize;
	unsigned int boffset, bytes;
	struct buffer *buf;
	struct device *d;

	if(!(d = get_device(BLK_DEV, MAJOR(i->rdev)))) {
		return -EINVAL;
	}

	blksize = d->blksize ? d->blksize : BLKSIZE_1K;
	total_read = 0;
	if(!d->device_data) {
		printk("%s(): don't know the size of the block device %d,%d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
		return -EIO;
	}

	device_size = ((unsigned int *)d->device_data)[MINOR(i->rdev)];
	device_size *= 1024LLU;

	count = (fd_table->offset + count > device_size) ? device_size - fd_table->offset : count;
	if(!count || fd_table->offset > device_size) {
		return 0;
	}
	while(count) {
		boffset = fd_table->offset % blksize;
		block = (fd_table->offset / blksize);
		if(!(buf = bread(i->rdev, block, blksize))) {
			return -EIO;
		}
		bytes = blksize - boffset;
		bytes = MIN(bytes, count);
		memcpy_b(buffer + total_read, buf->data + boffset, bytes);
		total_read += bytes;
		count -= bytes;
		boffset += bytes;
		boffset %= blksize;
		fd_table->offset += bytes;
		brelse(buf);
	}
	return total_read;
}

int blk_dev_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	__blk_t block;
	__off_t total_written;
	unsigned long long int device_size;
	int blksize;
	unsigned int boffset, bytes;
	struct buffer *buf;
	struct device *d;

	if(!(d = get_device(BLK_DEV, MAJOR(i->rdev)))) {
		return -EINVAL;
	}

	blksize = d->blksize ? d->blksize : BLKSIZE_1K;
	total_written = 0;
	if(!d->device_data) {
		printk("%s(): don't know the size of the block device %d,%d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
		return -EIO;
	}

	device_size = ((unsigned int *)d->device_data)[MINOR(i->rdev)];
	device_size *= 1024LLU;

	count = (fd_table->offset + count > device_size) ? device_size - fd_table->offset : count;
	if(!count || fd_table->offset > device_size) {
		printk("%s(): I/O error on device %d,%d, offset %u.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), fd_table->offset);
		return -EIO;
	}
	while(count) {
		boffset = fd_table->offset % blksize;
		block = (fd_table->offset / blksize);
		if(!(buf = bread(i->rdev, block, blksize))) {
			return -EIO;
		}
		bytes = blksize - boffset;
		bytes = MIN(bytes, count);
		memcpy_b(buf->data + boffset, buffer + total_written, bytes);
		total_written += bytes;
		count -= bytes;
		boffset += bytes;
		boffset %= blksize;
		fd_table->offset += bytes;
		bwrite(buf);
	}
	return total_written;
}

int blk_dev_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	struct device *d;

	if((d = get_device(BLK_DEV, MAJOR(i->rdev)))) {
		if(d->fsop && d->fsop->ioctl) {
			return d->fsop->ioctl(i, cmd, arg);
		}
	}

	printk("WARNING: %s(): block device %d,%d does not have the ioctl() method.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
	return -EINVAL;
}

int blk_dev_lseek(struct inode *i, __off_t offset)
{
	struct device *d;

	if((d = get_device(BLK_DEV, MAJOR(i->rdev)))) {
		if(d->fsop && d->fsop->lseek) {
			return d->fsop->lseek(i, offset);
		}
	}

	return offset;
}

void dev_init(void)
{
	memset_b(chr_device_table, NULL, sizeof(chr_device_table));
	memset_b(blk_device_table, NULL, sizeof(blk_device_table));
}
