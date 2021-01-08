/*
 * fiwix/drivers/block/ramdisk.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/ramdisk.h>
#include <fiwix/ioctl.h>
#include <fiwix/devices.h>
#include <fiwix/part.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static unsigned int rd_sizes[256];

static struct fs_operations ramdisk_driver_fsop = {
	0,
	0,

	ramdisk_open,
	ramdisk_close,
	NULL,			/* read */
	NULL,			/* write */
	ramdisk_ioctl,
	ramdisk_lseek,
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

	ramdisk_read,
	ramdisk_write,

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct device ramdisk_device = {
	"ramdisk",
	RAMDISK_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	BLKSIZE_1K,
	&rd_sizes,
	&ramdisk_driver_fsop,
	NULL
};

static struct ramdisk * get_ramdisk(int minor)
{
	if(TEST_MINOR(ramdisk_device.minors, minor)) {
		return &ramdisk_table[minor];
	}
	return NULL;
}

int ramdisk_open(struct inode *i, struct fd *fd_table)
{
	if(!get_ramdisk(MINOR(i->rdev))) {
		return -ENXIO;
	}
	return 0;
}

int ramdisk_close(struct inode *i, struct fd *fd_table)
{
	if(!get_ramdisk(MINOR(i->rdev))) {
		return -ENXIO;
	}
	return 0;
}

int ramdisk_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int size;
	__off_t offset;
	struct ramdisk *ramdisk;

	if(!(ramdisk = get_ramdisk(MINOR(dev)))) {
		return -ENXIO;
	}

	size = rd_sizes[MINOR(dev)] * 1024;
	offset = block * blksize;
	if(offset > size) {
		printk("%s(): block %d is beyond the size of the ramdisk.\n", __FUNCTION__, block);
		return -EIO;
	}
	blksize = MIN(blksize, size - offset);
	memcpy_b((void *)buffer, ramdisk->addr + offset, blksize);
	return blksize;
}

int ramdisk_write(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int size;
	__off_t offset;
	struct ramdisk *ramdisk;

	if(!(ramdisk = get_ramdisk(MINOR(dev)))) {
		return -ENXIO;
	}

	size = rd_sizes[MINOR(dev)] * 1024;
	offset = block * blksize;
	if(offset > size) {
		printk("%s(): block %d is beyond the size of the ramdisk.\n", __FUNCTION__, block);
		return -EIO;
	}
	blksize = MIN(blksize, size - offset);
	memcpy_b((void *)ramdisk->addr + offset, buffer, blksize);
	return blksize;
}

int ramdisk_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	struct hd_geometry *geom;
	int errno;

	if(!get_ramdisk(MINOR(i->rdev))) {
		return -ENXIO;
	}

	switch(cmd) {
		case HDIO_GETGEO:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(struct hd_geometry)))) {
				return errno;
			}
			geom = (struct hd_geometry *)arg;
			geom->heads = 63;
			geom->sectors = 16;
			geom->cylinders = rd_sizes[MINOR(i->rdev)] * 1024 / BPS;
			geom->cylinders /= (geom->heads * geom->sectors);
			geom->start = 0;
			break;
		case BLKRRPART:
			break;
		case BLKGETSIZE:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int)))) {
				return errno;
			}
			*(int *)arg = rd_sizes[MINOR(i->rdev)] * 2;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int ramdisk_lseek(struct inode *i, __off_t offset)
{
	return offset;
}

void ramdisk_init(void)
{
	int n;
	struct ramdisk *ramdisk;

	if(!_noramdisk) {
		for(n = 0; n < RAMDISK_MINORS; n++) {
			SET_MINOR(ramdisk_device.minors, n);
			rd_sizes[n] = _ramdisksize;
			ramdisk = get_ramdisk(n);
			printk("ram%d      0x%08x-0x%08x %d RAMdisk(s) of %dKB size, %dKB blocksize\n", n, ramdisk->addr, ramdisk->addr + (_ramdisksize * 1024), RAMDISK_MINORS, _ramdisksize, BLKSIZE_1K / 1024);
		}
		register_device(BLK_DEV, &ramdisk_device);
	}
}
