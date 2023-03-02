/*
 * fiwix/drivers/char/fb.c
 *
 * Copyright 2021-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/fb.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/fb.h>
#include <fiwix/pci.h>
#include <fiwix/mm.h>

static struct fs_operations fb_driver_fsop = {
	0,
	0,

	fb_open,
	fb_close,
	fb_read,
	fb_write,
	fb_ioctl,
	fb_lseek,
	NULL,			/* readdir */
	NULL,
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
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct device fb_device = {
	"fb",
	FB_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	NULL,
	&fb_driver_fsop,
	NULL
};

int fb_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int fb_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int fb_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	unsigned int addr;

	if(fd_table->offset >= video.memsize) {
		return 0;
	}

	addr = (unsigned int)video.address + fd_table->offset;
	count = MIN(count, video.memsize - fd_table->offset);
	memcpy_b(buffer, (void *)addr, count);
	fd_table->offset += count;
	return count;
}

int fb_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	unsigned int addr;

	if(fd_table->offset >= video.memsize) {
		return -ENOSPC;
	}

	addr = (unsigned int)video.address + fd_table->offset;
	count = MIN(count, video.memsize - fd_table->offset);
	memcpy_b((void *)addr, buffer, count);
	fd_table->offset += count;
	return count;
}

int fb_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	return -EINVAL;
}

int fb_lseek(struct inode *i, __off_t offset)
{
	return offset;
}

void fb_init(void)
{
	unsigned int limit;

	SET_MINOR(fb_device.minors, 0);
	limit = (unsigned int)video.address + video.memsize - 1;

	printk("fb0       0x%08x-0x%08x\ttype=%s %x.%x resolution=%dx%dx%d size=%dMB\n",
		video.address,
		limit,
		video.signature,
		video.fb_version >> 8,
		video.fb_version & 0xFF,
		video.fb_width,
		video.fb_height,
		video.fb_bpp,
		video.memsize / 1024 / 1024
	);
#ifdef CONFIG_PCI
	if(video.pci_dev) {
		pci_show_desc(video.pci_dev);
	}
#endif /* CONFIG_PCI */
	if(register_device(CHR_DEV, &fb_device)) {
		printk("ERROR: %s(): unable to register fb device.\n", __FUNCTION__);
		return;
	}
}
