/*
 * fiwix/arch/arm/pl011.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_devices.h>
#include <fiwix/arm_vm.h>
#include <fiwix/charq.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/kparms.h>
#include <fiwix/sleep.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/sysconsole.h>
#include <fiwix/tty.h>

#define PL011_DR	(*(volatile unsigned int *)(unsigned long)\
	(arm_vm_device_address(ARM_PL011_BASE) + 0x00U))
#define PL011_FR	(*(volatile unsigned int *)(unsigned long)\
	(arm_vm_device_address(ARM_PL011_BASE) + 0x18U))
#define PL011_FR_TXFF	0x20U

static struct fs_operations arm_pl011_fsop = {
	0,
	0,

	tty_open,
	tty_close,
	tty_read,
	tty_write,
	tty_ioctl,
	tty_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	tty_select,

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

static struct device arm_pl011_device = {
	"ttyS",
	ARM_PL011_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,
	NULL,
	&arm_pl011_fsop,
	NULL,
	NULL,
	NULL
};

static struct device arm_console_device = {
	"console",
	SYSCON_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,
	NULL,
	&arm_pl011_fsop,
	NULL,
	NULL,
	NULL
};

static void arm_pl011_output(struct tty *tty)
{
	unsigned char ch;

	while(tty->write_q.count) {
		while(PL011_FR & PL011_FR_TXFF) {
			/* Bootstrap console output remains deliberately polled. */
		}
		ch = charq_getchar(&tty->write_q);
		PL011_DR = ch;
	}
	wakeup(&tty_write);
}

int arm_pl011_init(void)
{
	struct tty *tty;
	__dev_t dev;

	dev = MKDEV(ARM_PL011_MAJOR, ARM_PL011_MINOR);
	SET_MINOR(arm_pl011_device.minors, ARM_PL011_MINOR);
	SET_MINOR(arm_console_device.minors, 0);
	SET_MINOR(arm_console_device.minors, 1);
	if(register_device(CHR_DEV, &arm_pl011_device) ||
		register_device(CHR_DEV, &arm_console_device)) {
		return -1;
	}
	tty = register_tty(dev);
	if(!tty) {
		return -1;
	}
	tty->deltab = tty_deltab;
	tty->reset = tty_reset;
	tty->input = do_cook;
	tty->output = arm_pl011_output;
	kparms.syscondev = dev;
	add_sysconsoledev(dev);
	register_console(tty);
	flush_log_buf(tty);
	return 0;
}
