/*
 * Fiwix block adapter for the polled virtio-mmio transport.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/riscv64_devices.h>
#include <fiwix/string.h>

static unsigned int riscv64_virtio_blksize[1];
static unsigned int riscv64_virtio_size[1];

static int riscv64_virtio_open(struct inode *inode, struct fd *fd)
{
	(void)fd;
	return MINOR(inode->rdev) == RISCV64_VIRTIO_BLK_MINOR ? 0 : -ENXIO;
}

static int riscv64_virtio_close(struct inode *inode, struct fd *fd)
{
	(void)fd;
	sync_buffers(inode->rdev);
	return 0;
}

static int riscv64_virtio_read(__dev_t dev, __blk_t block, char *buffer,
	int blksize)
{
	unsigned long first_sector;
	unsigned long sectors;
	unsigned long n;

	if(MINOR(dev) != RISCV64_VIRTIO_BLK_MINOR || blksize <= 0 ||
		blksize % BPS) {
		return -EINVAL;
	}
	sectors = (unsigned long)blksize / BPS;
	first_sector = (unsigned long)block * sectors;
	if(first_sector + sectors > riscv64_virtio_capacity_sectors()) {
		return -EIO;
	}
	for(n = 0; n < sectors; n++) {
		if(riscv64_virtio_read_sector(first_sector + n,
			buffer + n * BPS) < 0) {
			return -EIO;
		}
	}
	return blksize;
}

static int riscv64_virtio_write(__dev_t dev, __blk_t block, char *buffer,
	int blksize)
{
	unsigned long first_sector;
	unsigned long sectors;
	unsigned long n;

	if(MINOR(dev) != RISCV64_VIRTIO_BLK_MINOR || blksize <= 0 ||
		blksize % BPS) {
		return -EINVAL;
	}
	sectors = (unsigned long)blksize / BPS;
	first_sector = (unsigned long)block * sectors;
	if(first_sector + sectors > riscv64_virtio_capacity_sectors()) {
		return -EIO;
	}
	for(n = 0; n < sectors; n++) {
		if(riscv64_virtio_write_sector(first_sector + n,
			buffer + n * BPS) < 0) {
			return -EIO;
		}
	}
	return blksize;
}

static int riscv64_virtio_ioctl(struct inode *inode, struct fd *fd, int cmd,
	unsigned int arg)
{
	(void)inode;
	(void)fd;
	(void)cmd;
	(void)arg;
	return -EINVAL;
}

static __loff_t riscv64_virtio_llseek(struct inode *inode, __loff_t offset)
{
	(void)inode;
	return offset;
}

static struct fs_operations riscv64_virtio_fsop = {
	0,
	0,

	riscv64_virtio_open,
	riscv64_virtio_close,
	NULL,			/* read */
	NULL,			/* write */
	riscv64_virtio_ioctl,
	riscv64_virtio_llseek,
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

	riscv64_virtio_read,
	riscv64_virtio_write,

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

static struct device riscv64_virtio_device = {
	"virtio-blk",
	RISCV64_VIRTIO_BLK_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	riscv64_virtio_blksize,
	riscv64_virtio_size,
	&riscv64_virtio_fsop,
	NULL,
	NULL,
	NULL
};

int riscv64_virtio_block_init(void)
{
	unsigned long sectors;

	if(riscv64_virtio_transport_init() < 0) {
		return -ENXIO;
	}
	sectors = riscv64_virtio_capacity_sectors();
	if(!sectors || sectors > 0x7fffffUL) {
		return -EIO;
	}
	riscv64_virtio_blksize[0] = BLKSIZE_1K;
	riscv64_virtio_size[0] = (unsigned int)(sectors / 2);
	SET_MINOR(riscv64_virtio_device.minors, RISCV64_VIRTIO_BLK_MINOR);
	if(register_device(BLK_DEV, &riscv64_virtio_device)) {
		return -EIO;
	}
	return 0;
}
