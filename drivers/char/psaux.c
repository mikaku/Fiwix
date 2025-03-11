/*
 * fiwix/drivers/char/psaux.c
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/ps2.h>
#include <fiwix/psaux.h>
#include <fiwix/pic.h>
#include <fiwix/irq.h>
#include <fiwix/fcntl.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_PSAUX
static struct fs_operations psaux_driver_fsop = {
	0,
	0,

	psaux_open,
	psaux_close,
	psaux_read,
	psaux_write,
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	psaux_select,

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

static struct device psaux_device = {
	"psaux",
	PSAUX_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	NULL,
	&psaux_driver_fsop,
	NULL,
	NULL,
	NULL
};

struct psaux psaux_table;

static struct interrupt irq_config_psaux = { 0, "psaux", &irq_psaux, NULL };

extern volatile unsigned char ack;
static unsigned char status[3] = { 0, 0, 0};
static unsigned char is_ps2 = 0;
static char id = -1;

static int psaux_command_write(const unsigned char byte)
{
	ps2_write(PS2_COMMAND, PS2_CMD_CH2_PREFIX);
	ps2_write(PS2_DATA, byte);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on %x command!\n", __FUNCTION__, byte);
		return 1;
	}
	return 0;
}

static void psaux_identify(void)
{
	/* disable */
	psaux_command_write(PS2_AUX_DISABLE);

	/* status information */
	psaux_command_write(PS2_DEV_GETINFO);
	status[0] = ps2_read(PS2_DATA);	/* status */
	status[2] = ps2_read(PS2_DATA);	/* resolution */
	status[2] = ps2_read(PS2_DATA);	/* sample rate */

	/* identify */
	psaux_command_write(PS2_DEV_RATE);
	psaux_command_write(200);
	psaux_command_write(PS2_DEV_RATE);
	psaux_command_write(100);
	psaux_command_write(PS2_DEV_RATE);
	psaux_command_write(80);
	psaux_command_write(PS2_DEV_IDENTIFY);
	id = ps2_read(PS2_DATA);
	ps2_clear_buffer();

	/* enable */
	psaux_command_write(PS2_DEV_ENABLE);
}

void irq_psaux(int num, struct sigcontext *sc)
{
	unsigned char ch;

	ch = inport_b(PS2_DATA);

	/* aux controller said 'acknowledge!' */
	if(ch == DEV_ACK) {
		ack = 1;
	}
	if(!psaux_table.count) {
		return;
	}
	charq_putchar(&psaux_table.read_q, ch);
	wakeup(&psaux_read);
	wakeup(&do_select);
}

int psaux_open(struct inode *i, struct fd *f)
{
	int minor;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(psaux_device.minors, minor)) {
		return -ENXIO;
	}
	if(psaux_table.count++) {
		return 0;
	}
	memset_b(&psaux_table.read_q, 0, sizeof(struct clist));
	memset_b(&psaux_table.write_q, 0, sizeof(struct clist));
	return 0;
}

int psaux_close(struct inode *i, struct fd *f)
{
	int minor;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(psaux_device.minors, minor)) {
		return -ENXIO;
	}
	psaux_table.count--;
	return 0;
}

int psaux_read(struct inode *i, struct fd *f, char *buffer, __size_t count)
{
	int minor, bytes_read;
	unsigned char ch;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(psaux_device.minors, minor)) {
		return -ENXIO;
	}

	while(!psaux_table.read_q.count) {
		if(f->flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		if(sleep(&psaux_read, PROC_INTERRUPTIBLE)) {
			return -EINTR;
		}
	}
	bytes_read = 0;
	while(bytes_read < count) {
		if(psaux_table.read_q.count) {
			ch = charq_getchar(&psaux_table.read_q);
			buffer[bytes_read++] = ch;
			continue;
		}
		break;
	}
	if(bytes_read) {
		i->i_atime = CURRENT_TIME;
	}
	return bytes_read;
}

int psaux_write(struct inode *i, struct fd *f, const char *buffer, __size_t count)
{
	int minor, bytes_written;
	unsigned char ch;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(psaux_device.minors, minor)) {
		return -ENXIO;
	}

	bytes_written = 0;
	while(bytes_written < count) {
		ch = buffer[bytes_written++];
		psaux_command_write(ch);
	}
	if(bytes_written) {
		i->i_mtime = CURRENT_TIME;
	}
	return bytes_written;
}

int psaux_select(struct inode *i, struct fd *f, int flag)
{
	int minor;

	minor = MINOR(i->rdev);
	if(!TEST_MINOR(psaux_device.minors, minor)) {
		return -ENXIO;
	}

	switch(flag) {
		case SEL_R:
			if(psaux_table.read_q.count) {
				return 1;
			}
			break;
	}
	return 0;
}

void psaux_init(void)
{
	int errno;

	/* reset device */
	psaux_command_write(PS2_DEV_RESET);
	if((errno = ps2_read(PS2_DATA)) != DEV_RESET_OK) {
		printk("psaux     0x%04x-0x%04x       \tdevice not detected\n", 0x60, 0x64);
		return;
	}
	if(ps2_read(PS2_DATA) == 0) {
		is_ps2 = 1;
	}

	if(!register_irq(PSAUX_IRQ, &irq_config_psaux)) {
		enable_irq(PSAUX_IRQ);
	}

	ps2_clear_buffer();
	psaux_identify();
	printk("psaux     0x%04x-0x%04x    %d", 0x60, 0x64, PSAUX_IRQ);
	printk("\ttype=%s", is_ps2 ? "PS/2" : "unknown");
	switch(id) {
		case -1:
			printk(", unknown ID %x", id & 0xFF);
			break;
		case 0:
			printk(", standard mouse");
			break;
		case 2:
			printk(", track ball");
			break;
		case 3:
			printk(", 3-button wheel mouse");
			break;
		case 4:
			printk(", 5-button wheel mouse");
			break;
		default:
			printk(", unknown mouse");
			break;
	}
	printk("\n");
	memset_b(&psaux_table, 0, sizeof(struct psaux));
	SET_MINOR(psaux_device.minors, PSAUX_MINOR);
	if(register_device(CHR_DEV, &psaux_device)) {
		printk("WARNING: %s(): unable to register psaux device.\n", __FUNCTION__);
	}
}
#endif /* CONFIG_PSAUX */
