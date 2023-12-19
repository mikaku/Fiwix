/*
 * fiwix/drivers/char/lp.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/lp.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct lp lp_table[LP_MINORS];

static struct fs_operations lp_driver_fsop = {
	0,
	0,

	lp_open,
	lp_close,
	NULL,			/* read */
	lp_write,
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
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct device lp_device = {
	"lp",
	LP_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	NULL,
	&lp_driver_fsop,
	NULL
};

struct lp lp_table[LP_MINORS] = {
	{ LP0_ADDR, LP0_ADDR + 1, LP0_ADDR + 2, 0 }
};

static void lp_delay(void)
{
	int n;

	for(n = 0; n < 10000; n++) {
		NOP();
	}
}

static int lp_ready(int minor)
{
	int n;

	for(n = 0; n < LP_RDY_RETR; n++) {
		if(inport_b(lp_table[minor].stat) & LP_STAT_BUS) {
			break;
		}
		lp_delay();
	}
	if(n == LP_RDY_RETR) {
		return 0;
	}
	return 1;
}

static int lp_probe(int minor)
{
	/* first check */
	outport_b(lp_table[minor].data, 0x55);
	lp_delay();
	if(inport_b(lp_table[minor].data) != 0x55) {
		return 1;		/* did not retain data */
	}

	/* second check */
	outport_b(lp_table[minor].data, 0xAA);
	lp_delay();
	if(inport_b(lp_table[minor].data) != 0xAA) {
		return 1;		/* did not retain data */
	}
	return 0;
}

static int lp_write_data(int minor, unsigned char c)
{
	unsigned char ctrl;

	if(!lp_ready(minor)) {
		return -EBUSY;
	}
	outport_b(lp_table[minor].data, c);
	ctrl = inport_b(lp_table[minor].ctrl);
	outport_b(lp_table[minor].ctrl, ctrl | LP_CTRL_STR);
	lp_delay();
	outport_b(lp_table[minor].ctrl, ctrl);
	if(!lp_ready(minor)) {
		return -EBUSY;
	}
	return 1;
}

int lp_open(struct inode *i, struct fd *fd_table)
{
	int minor;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(lp_device.minors, minor)) {
		return -ENXIO;
	}
	if(!(lp_table[minor].flags & LP_CTRL_SEL)) {
		return -ENXIO;
	}
	if(lp_table[minor].flags & LP_STAT_BUS) {
		return -EBUSY;
	}
	lp_table[minor].flags |= LP_STAT_BUS;
	return 0;
}

int lp_close(struct inode *i, struct fd *fd_table)
{
	int minor;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(lp_device.minors, minor)) {
		return -ENXIO;
	}
	lp_table[minor].flags &= ~LP_STAT_BUS;
	return 0;
}

int lp_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	unsigned int n;
	int bytes_written, total_written;
	int minor;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(lp_device.minors, minor)) {
		return -ENXIO;
	}

	total_written = 0;
	for(n = 0; n < count; n++) {
		bytes_written = lp_write_data(minor, buffer[n]);
		if(bytes_written != 1) {
			break;
		}
		total_written += bytes_written;
	}

	return total_written;
}

void lp_init(void)
{
	int n;
	unsigned char ctrl;

	for(n = 0; n < LP_MINORS; n++) {
		if(!lp_probe(n)) {
			ctrl = inport_b(lp_table[n].ctrl);
			ctrl &= ~LP_CTRL_AUT;	/* disable auto LF */
			ctrl |= LP_CTRL_INI;	/* initialize */
			ctrl |= LP_CTRL_SEL;	/* select in */
			ctrl &= ~LP_CTRL_IRQ;	/* disable IRQ */
			ctrl &= ~LP_CTRL_BID;	/* disable bidirectional mode */
			outport_b(lp_table[n].ctrl, ctrl);
			lp_table[n].flags |= LP_CTRL_SEL;
			printk("lp%d       0x%04x-0x%04x     -\n", n, lp_table[n].data, lp_table[n].data + 2);
			SET_MINOR(lp_device.minors, n);
		}
	}

	for(n = 0; n < LP_MINORS; n++) {
		if(lp_table[n].flags & LP_CTRL_SEL) {
			if(register_device(CHR_DEV, &lp_device)) {
				printk("WARNING: %s(): unable to register lp device.\n", __FUNCTION__);
			}
			break;
		}
	}
}
