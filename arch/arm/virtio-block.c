/*
 * fiwix/arch/arm/virtio-block.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_devices.h>
#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/string.h>

typedef unsigned long long arm_block_u64;

static unsigned int arm_virtio_blksize[1];
static unsigned int arm_virtio_size[1];

static int arm_virtio_open(struct inode *inode, struct fd *fd)
{
	(void)fd;
	return MINOR(inode->rdev) == ARM_VIRTIO_BLK_MINOR ? 0 : -ENXIO;
}

static int arm_virtio_close(struct inode *inode, struct fd *fd)
{
	(void)fd;
	sync_buffers(inode->rdev);
	return 0;
}

static int arm_virtio_read(__dev_t dev, __blk_t block, char *buffer,
	int blksize)
{
	arm_block_u64 capacity;
	arm_block_u64 first_sector;
	unsigned int n;
	unsigned int sectors;

	if(MINOR(dev) != ARM_VIRTIO_BLK_MINOR || block < 0 || blksize <= 0 ||
		blksize % BPS) {
		return -EINVAL;
	}
	sectors = (unsigned int)blksize / BPS;
	first_sector = (arm_block_u64)(unsigned int)block * sectors;
	capacity = arm_virtio_capacity_sectors();
	if(first_sector > capacity || sectors > capacity - first_sector) {
		return -EIO;
	}
	for(n = 0; n < sectors; n++) {
		if(arm_virtio_read_sector(first_sector + n,
			buffer + n * BPS) < 0) {
			return -EIO;
		}
	}
	return blksize;
}

static int arm_virtio_write(__dev_t dev, __blk_t block, char *buffer,
	int blksize)
{
	arm_block_u64 capacity;
	arm_block_u64 first_sector;
	unsigned int n;
	unsigned int sectors;

	if(MINOR(dev) != ARM_VIRTIO_BLK_MINOR || block < 0 || blksize <= 0 ||
		blksize % BPS) {
		return -EINVAL;
	}
	sectors = (unsigned int)blksize / BPS;
	first_sector = (arm_block_u64)(unsigned int)block * sectors;
	capacity = arm_virtio_capacity_sectors();
	if(first_sector > capacity || sectors > capacity - first_sector) {
		return -EIO;
	}
	for(n = 0; n < sectors; n++) {
		if(arm_virtio_write_sector(first_sector + n,
			buffer + n * BPS) < 0) {
			return -EIO;
		}
	}
	return blksize;
}

static int arm_virtio_ioctl(struct inode *inode, struct fd *fd, int cmd,
	unsigned int arg)
{
	(void)inode;
	(void)fd;
	(void)cmd;
	(void)arg;
	return -EINVAL;
}

static __loff_t arm_virtio_llseek(struct inode *inode, __loff_t offset)
{
	(void)inode;
	return offset;
}

static struct fs_operations arm_virtio_fsop = {
	0,
	0,

	arm_virtio_open,
	arm_virtio_close,
	NULL,			/* read */
	NULL,			/* write */
	arm_virtio_ioctl,
	arm_virtio_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lookup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	arm_virtio_read,
	arm_virtio_write,

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

static struct device arm_virtio_device = {
	"virtio-blk",
	ARM_VIRTIO_BLK_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	arm_virtio_blksize,
	arm_virtio_size,
	&arm_virtio_fsop,
	NULL,
	NULL,
	NULL
};

int arm_virtio_block_init(void)
{
	arm_block_u64 sectors;

	if(arm_virtio_transport_init() < 0) {
		return -ENXIO;
	}
	sectors = arm_virtio_capacity_sectors();
	if(!sectors || sectors > 0x7fffffULL) {
		return -EIO;
	}
	arm_virtio_blksize[0] = BLKSIZE_1K;
	arm_virtio_size[0] = (unsigned int)(sectors / 2);
	SET_MINOR(arm_virtio_device.minors, ARM_VIRTIO_BLK_MINOR);
	if(register_device(BLK_DEV, &arm_virtio_device)) {
		return -EIO;
	}
	return 0;
}
