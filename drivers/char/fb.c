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
#include <fiwix/bios.h>

static struct fs_operations fb_driver_fsop = {
	0,
	0,

	fb_open,
	fb_close,
	fb_read,
	fb_write,
	fb_ioctl,
	fb_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
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

int fb_ioctl(struct inode *i, int cmd, unsigned int arg)
{
	return -EINVAL;
}

__loff_t fb_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

void fb_init(void)
{
	unsigned int limit, from;

	SET_MINOR(fb_device.minors, 0);
	limit = (unsigned int)video.address + video.memsize - 1;

	/*
	 * Frame buffer memory must be marked as reserved because its memory
	 * range (e.g: 0xFD000000-0xFDFFFFFF) might conflict with the physical
	 * memory below 1GB (e.g: 0x3D000000-0x3DFFFFFF + PAGE_OFFSET).
	 */
	from = (unsigned int)video.address - PAGE_OFFSET;
	bios_map_reserve(from, from + video.memsize);

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
