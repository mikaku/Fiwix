/*
 * fiwix/drivers/block/ide_cd.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/buffer.h>
#include <fiwix/ide.h>
#include <fiwix/ide_cd.h>
#include <fiwix/ioctl.h>
#include <fiwix/devices.h>
#include <fiwix/sleep.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/cpu.h>
#include <fiwix/fs.h>
#include <fiwix/part.h>
#include <fiwix/process.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/* default size of 1GB is enough to read a whole CDROM */
#define CDROM_DEFAULT_SIZE	(1024 * 1024)	/* in KBs */

static struct fs_operations ide_cd_driver_fsop = {
	0,
	0,

	ide_cd_open,
	ide_cd_close,
	NULL,			/* read */
	NULL,			/* write */
	ide_cd_ioctl,
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

	ide_cd_read,
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

static char *sense_key_err[] = {
	"NO SENSE",
	"RECOVERED ERROR",
	"NOT READY",
	"MEDIUM ERROR",
	"HARDWARE ERROR",
	"ILLEGAL REQUEST",
	"UNIT ATTENTION",
	"DATA PROTECT",
	"RESERVED",
	"RESERVED",
	"RESERVED",
	"ABORTED COMMAND",
	"MISCOMPARE",
	"RESERVED"
};

enum {
	RS_NO_SENSE,
	RS_RECOVERED_ERROR,
	RS_NOT_READY,
	RS_MEDIUM_ERROR,
	RS_HARDWARE_ERROR,
	RS_ILLEGAL_REQUEST,
	RS_UNIT_ATTENTION,
	RS_DATA_PROTECT,
	RS_RESERVED1,
	RS_RESERVED2,
	RS_RESERVED3,
	RS_ABORTED_COMMAND,
	RS_MISCOMPARE,
	RS_RESERVED4
};

static int send_packet_command(unsigned char *pkt, struct ide *ide, struct ide_drv *drive, int blksize)
{
	int n, retries, status;

	outport_b(ide->ctrl + IDE_DEV_CTRL, 0);
	ide_delay();
	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
	ide_delay();
	if(ide_drvsel(ide, drive->num, IDE_CHS_MODE, 0)) {
		printk("WARNING: %s(): %s: drive not ready to receive PACKET command.\n", __FUNCTION__, drive->dev_name);
		return 1;
	}

	CLI();
	outport_b(ide->base + IDE_FEATURES, 0);
	outport_b(ide->base + IDE_SECCNT, 0);
	outport_b(ide->base + IDE_SECNUM, 0);
	outport_b(ide->base + IDE_LCYL, blksize & 0xFF);
	outport_b(ide->base + IDE_HCYL, blksize >> 8);
	outport_b(ide->base + IDE_DRVHD, drive->num << 4);
	outport_b(ide->base + IDE_COMMAND, ATA_PACKET);
	ide_wait400ns(ide);

/*
 * NOTE: Some devices prior to ATA/ATAPI-4 assert INTRQ if enabled at this
 * point. See IDENTIFY PACKET DEVICE, word 0, bits 5-6 to determine if an
 * interrupt will occur.
 */
	SET_IDE_RDY_RETR(retries);

	for(n = 0; n < retries; n++) {
		status = inport_b(ide->base + IDE_STATUS);
		if((status & (IDE_STAT_DRQ | IDE_STAT_BSY)) == IDE_STAT_DRQ) {
			break;
		}
		ide_delay();
	}
	if(n >= retries) {
		printk("WARNING: %s(): %s: drive not ready to receive command packet (retries = %d).\n", __FUNCTION__, drive->dev_name, n);
		return 1;
	}

	outport_sw(ide->base + IDE_DATA, pkt, 12 / sizeof(short int));
	return 0;
}

static int atapi_read_data(__dev_t dev, char *data, struct ide *ide, int blksize, int offset)
{
	int errno, status;
	char *buffer;
	int retries, bytes;
	struct callout_req creq;

	status = 0;

	for(retries = 0; retries < MAX_IDE_ERR; retries++) {
		if(ide->channel == IDE_PRIMARY) {
			ide0_wait_interrupt = ide->base;
			creq.fn = ide0_timer;
			creq.arg = 0;
			add_callout(&creq, WAIT_FOR_IDE);
			sleep(&irq_ide0, PROC_UNINTERRUPTIBLE);
			if(ide0_timeout) {
				status = inport_b(ide->base + IDE_STATUS);
				if((status & (IDE_STAT_RDY | IDE_STAT_DRQ)) != (IDE_STAT_RDY | IDE_STAT_DRQ)) {
					continue;
				}
			}
			del_callout(&creq);
		}
		if(ide->channel == IDE_SECONDARY) {
			ide1_wait_interrupt = ide->base;
			creq.fn = ide1_timer;
			creq.arg = 0;
			add_callout(&creq, WAIT_FOR_IDE);
			sleep(&irq_ide1, PROC_UNINTERRUPTIBLE);
			if(ide1_timeout) {
				status = inport_b(ide->base + IDE_STATUS);
				if((status & (IDE_STAT_RDY | IDE_STAT_DRQ)) != (IDE_STAT_RDY | IDE_STAT_DRQ)) {
					continue;
				}
			}
			del_callout(&creq);
		}
		status = inport_b(ide->base + IDE_STATUS);
		if(status & IDE_STAT_ERR) {
			continue;
		}

		if((status & (IDE_STAT_DRQ | IDE_STAT_BSY)) == 0) {
			break;
		}

		bytes = (inport_b(ide->base + IDE_HCYL) << 8) + inport_b(ide->base + IDE_LCYL);
		if(!bytes || bytes > blksize) {
			break;
		}

		bytes = MAX(bytes, IDE_CD_SECTSIZE);	/* read more than 2048 bytes is not supported */
		buffer = data + offset;
		inport_sw(ide->base + IDE_DATA, (void *)buffer, bytes / sizeof(short int));
	}

	if(status & IDE_STAT_ERR) {
		errno = inport_b(ide->base + IDE_ERROR);
		printk("WARNING: %s(): error on cdrom device %d,%d, status=0x%x error=0x%x,\n", __FUNCTION__, MAJOR(dev), MINOR(dev), status, errno);
		return 1;
	}

	if(retries >= MAX_IDE_ERR) {
		printk("WARNING: %s(): timeout on cdrom device %d,%d, status=0x%x.\n", __FUNCTION__, MAJOR(dev), MINOR(dev), status);
		/* a reset may be required at this moment */
		return 1;
	}
	return 0;
}

static int atapi_cmd_testunit(struct ide *ide, struct ide_drv *drive)
{
	unsigned char pkt[12];

	pkt[0] = ATAPI_TEST_UNIT;
	pkt[1] = 0;
	pkt[2] = 0;
	pkt[3] = 0;
	pkt[4] = 0;
	pkt[5] = 0;
	pkt[6] = 0;
	pkt[7] = 0;
	pkt[8] = 0;
	pkt[9] = 0;
	pkt[10] = 0;
	pkt[11] = 0;
	return send_packet_command(pkt, ide, drive, 0);
}

static int atapi_cmd_reqsense(struct ide *ide, struct ide_drv *drive)
{
	unsigned char pkt[12];

	pkt[0] = ATAPI_REQUEST_SENSE;
	pkt[1] = 0;
	pkt[2] = 0;
	pkt[3] = 0;
	pkt[4] = 252;	/* this command can send up to 252 bytes */
	pkt[5] = 0;
	pkt[6] = 0;
	pkt[7] = 0;
	pkt[8] = 0;
	pkt[9] = 0;
	pkt[10] = 0;
	pkt[11] = 0;
	return send_packet_command(pkt, ide, drive, 0);
}

static int atapi_cmd_startstop(int action, struct ide *ide, struct ide_drv *drive)
{
	unsigned char pkt[12];

	pkt[0] = ATAPI_START_STOP;
	pkt[1] = 0;
	pkt[2] = 0;
	pkt[3] = 0;
	pkt[4] = action;
	pkt[5] = 0;
	pkt[6] = 0;
	pkt[7] = 0;
	pkt[8] = 0;
	pkt[9] = 0;
	pkt[10] = 0;
	pkt[11] = 0;
	return send_packet_command(pkt, ide, drive, 0);
}

static int atapi_cmd_mediumrm(int action, struct ide *ide, struct ide_drv *drive)
{
	unsigned char pkt[12];

	pkt[0] = ATAPI_MEDIUM_REMOVAL;
	pkt[1] = 0;
	pkt[2] = 0;
	pkt[3] = 0;
	pkt[4] = action;
	pkt[5] = 0;
	pkt[6] = 0;
	pkt[7] = 0;
	pkt[8] = 0;
	pkt[9] = 0;
	pkt[10] = 0;
	pkt[11] = 0;
	return send_packet_command(pkt, ide, drive, 0);
}

static int request_sense(char *buffer, __dev_t dev, struct ide *ide, struct ide_drv *drive)
{
	int errcode;
	int sense_key, sense_asc;

	errcode = inport_b(ide->base + IDE_ERROR);
	sense_key = (errcode & 0xF0) >> 4;
	printk("\tSense Key code indicates a '%s' condition.\n", sense_key_err[sense_key & 0xF]);
	errcode = atapi_cmd_reqsense(ide, drive);
	printk("reqsense() returned %d\n", errcode);
	errcode = atapi_read_data(dev, buffer, ide, BLKSIZE_2K, 0);
	printk("atapi_read_data() returned %d\n", errcode);
	errcode = (int)(buffer[0] & 0x7F);
	sense_key = (int)(buffer[2] & 0xF);
	sense_asc = (int)(buffer[12] & 0xFF);
	printk("errcode = %x\n", errcode);
	printk("sense_key = %x\n", sense_key);
	printk("sense_asc = %x\n", sense_asc);
	return errcode;
}

void ide_cd_timer(unsigned int arg)
{
	wakeup(&ide_cd_open);
}

int ide_cd_open(struct inode *i, struct fd *fd_table)
{
	char *buffer;
	int errcode;
	int sense_key, sense_asc;
	int retries;
	struct ide *ide;
	struct ide_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_IDE_DRIVE(i->rdev)];

	lock_resource(&ide->resource);

	if(!(buffer = (void *)kmalloc())) {
		unlock_resource(&ide->resource);
		return -ENOMEM;
	}

	if((errcode = atapi_cmd_testunit(ide, drive))) {
		printk("WARNING: %s(): cdrom device %d,%d is not ready for TEST_UNIT, error %d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), errcode);
		request_sense(buffer, i->rdev, ide, drive);
	}

	for(retries = 0; retries < MAX_CD_ERR; retries++) {
		if(!(errcode = atapi_cmd_startstop(CD_LOAD, ide, drive))) {
			break;
		}
		printk("WARNING: %s(): cdrom device %d,%d is not ready for CD_LOAD, error %d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), errcode);
		atapi_read_data(i->rdev, buffer, ide, BLKSIZE_2K, 0);
		errcode = request_sense(buffer, i->rdev, ide, drive);
		sense_key = (errcode & 0xF0) >> 4;
		/* trying to eject on slim drives may lead to an illegal request */
		if(!sense_key || sense_key == RS_ILLEGAL_REQUEST) {
			break;
		}
		if(errcode == 0x70 || errcode == 0x71) {
			sense_key = (int)(buffer[2] & 0xF);
			sense_asc = (int)(buffer[12] & 0xFF);
			if(sense_key == RS_NOT_READY && sense_asc == ASC_NO_MEDIUM) {
				kfree((unsigned int)buffer);
				unlock_resource(&ide->resource);
				return -ENOMEDIUM;
			}
		}
	}

	if(retries == MAX_CD_ERR) {
		if(sense_key == RS_NOT_READY) {
			kfree((unsigned int)buffer);
			unlock_resource(&ide->resource);
			return -ENOMEDIUM;
		}
	}

	if(atapi_cmd_mediumrm(CD_LOCK_MEDIUM, ide, drive)) {
		printk("WARNING: %s(): error on cdrom device %d,%d while trying to lock medium.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), ATAPI_MEDIUM_REMOVAL);
		request_sense(buffer, i->rdev, ide, drive);
	}

	/* this line just to catch interrupt */
	atapi_read_data(i->rdev, buffer, ide, BLKSIZE_2K, 0);
	kfree((unsigned int)buffer);

	unlock_resource(&ide->resource);
	return 0;
}

int ide_cd_close(struct inode *i, struct fd *fd_table)
{
	char *buffer;
	struct ide *ide;
	struct ide_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!(buffer = (void *)kmalloc())) {
		return -ENOMEM;
	}

	drive = &ide->drive[GET_IDE_DRIVE(i->rdev)];

	/* FIXME: only if device usage == 0 */
	invalidate_buffers(i->rdev);

	if(atapi_cmd_mediumrm(CD_UNLOCK_MEDIUM, ide, drive)) {
		printk("WARNING: %s(): error on cdrom device %d,%d during 0x%x command.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), ATAPI_MEDIUM_REMOVAL);
	}

	/* this line just to catch interrupt */
	atapi_read_data(i->rdev, buffer, ide, BLKSIZE_2K, 0);
	kfree((unsigned int)buffer);

	return 0;
}

int ide_cd_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int sectors_to_read;
	int n, retries;
	unsigned char pkt[12];
	struct ide *ide;
	struct ide_drv *drive;

	if(!(ide = get_ide_controller(dev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_IDE_DRIVE(dev)];
	blksize = BLKSIZE_2K;
	sectors_to_read = blksize / IDE_CD_SECTSIZE;

	pkt[0] = ATAPI_READ10;
	pkt[1] = 0;
	pkt[2] = (block >> 24) & 0xFF;
	pkt[3] = (block >> 16) & 0xFF;
	pkt[4] = (block >> 8) & 0xFF;
	pkt[5] = block & 0xFF;
	pkt[6] = 0;
	pkt[7] = (sectors_to_read >> 8) & 0xFF;
	pkt[8] = sectors_to_read & 0xFF;
	pkt[9] = 0;
	pkt[10] = 0;
	pkt[11] = 0;

	lock_resource(&ide->resource);
	for(n = 0; n < sectors_to_read; n++, block++) {
		for(retries = 0; retries < MAX_CD_ERR; retries++) {
			if(send_packet_command(pkt, ide, drive, blksize)) {
				printk("\tblock=%d, offset=%d\n", block, block * blksize);
				unlock_resource(&ide->resource);
				return -EIO;
			}
			if(atapi_read_data(dev, buffer, ide, blksize, n * IDE_CD_SECTSIZE)) {
				int errcode;
				int sense_key;
				errcode = inport_b(ide->base + IDE_ERROR);
				sense_key = (errcode & 0xF0) >> 4;
				printk("\tSense Key code indicates a '%s' condition.\n", sense_key_err[sense_key & 0xF]);
				if(sense_key) {
					continue;
				}
			}
			break;
		}
		if(retries == MAX_CD_ERR) {
			printk("\tblock=%d, offset=%d\n", block, block * blksize);
			unlock_resource(&ide->resource);
			return -EIO;
		}

	}
	unlock_resource(&ide->resource);
	return sectors_to_read * IDE_CD_SECTSIZE;
}

int ide_cd_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	struct ide *ide;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	switch(cmd) {
		default:
			return -EINVAL;
			break;
	}

	return 0;
}

int ide_cd_init(struct ide *ide, struct ide_drv *drive)
{
	struct device *d;
	unsigned char minor;

	drive->fsop = &ide_cd_driver_fsop;

	minor = !drive->minor_shift ? 0 : 1 << drive->minor_shift;

	if(!(d = get_device(BLK_DEV, MKDEV(drive->major, minor)))) {
		return -EINVAL;
	}
	SET_MINOR(d->minors, minor);
	((unsigned int *)d->device_data)[minor] = CDROM_DEFAULT_SIZE;

	return 0;
}
