/*
 * fiwix/drivers/block/atapi_cd.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/buffer.h>
#include <fiwix/atapi.h>
#include <fiwix/atapi_cd.h>
#include <fiwix/devices.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations atapi_cd_driver_fsop = {
	0,
	0,

	atapi_cd_open,
	atapi_cd_close,
	NULL,			/* read */
	NULL,			/* write */
	atapi_cd_ioctl,
	atapi_cd_llseek,
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

	atapi_cd_read,
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

static int setup_transfer(int mode, __dev_t dev, __blk_t block, char *buffer, int blksize)
{
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(dev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_DRIVE_NUM(dev)];
	drive->xd.dev = dev;
	drive->xd.block = block;
	drive->xd.buffer = buffer;
	drive->xd.blksize = BLKSIZE_2K;
	drive->xd.sectors_to_io = 1;
	drive->xd.offset = 0;
	drive->xd.datalen = ATAPI_CD_SECTSIZE;
	drive->xd.nrsectors = 1;
	drive->xd.count = 0;
	drive->xd.retries = 0;
	drive->xd.max_retries = MAX_CD_ERR;

	drive->xd.mode = "read";
	drive->xd.rw_end_fn = drive->read_end_fn;
	return drive->read_fn(ide, drive, &drive->xd);
}

static int atapi_pio_read(struct ide *ide, struct ata_drv *drive, struct xfer_data *xd)
{
	ide->device->xfer_data = xd;

	if(!xd->retries) {
		if(atapi_cmd_read10(ide, drive)) {
			printk("\tread error on block=%d, offset=%d\n", xd->block, xd->block * xd->blksize);
			return -EIO;
		}
	}
	ata_set_timeout(ide, WAIT_FOR_CD, 0);
	return 0;
}

static int atapi_pio_read_end(struct ide *ide, struct xfer_data *xd)
{
	int status, errno, bytes;
	int errcode, sense_key;
	struct ata_drv *drive;

	drive = &ide->drive[GET_DRIVE_NUM(xd->dev)];
	status = 0;

	while(xd->retries < xd->max_retries) {
		xd->retries++;
		if(ide->irq_timeout) {
			ide->irq_timeout = 0;
			status = inport_b(ide->base + ATA_STATUS);
			if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
				atapi_pio_read(ide, drive, xd);
				return 0;
			}
		}

		status = inport_b(ide->base + ATA_STATUS);
		if(status & ATA_STAT_ERR) {
			atapi_pio_read(ide, drive, xd);
			return 0;
		}

		if((status & (ATA_STAT_DRQ | ATA_STAT_BSY)) == 0) {
			break;
		}

		bytes = (inport_b(ide->base + ATA_HCYL) << 8) + inport_b(ide->base + ATA_LCYL);
		if(!bytes || bytes > xd->blksize) {
			break;
		}

		bytes = MAX(bytes, ATAPI_CD_SECTSIZE);	/* only 2KB blocksize is supported */
		drive->xfer.copy_read_fn(ide->base + ATA_DATA, (void *)xd->buffer, bytes / drive->xfer.copy_raw_factor);
		/* wait for another interrupt (FIXME: only if in PIO mode) */
		return atapi_pio_read(ide, drive, xd);
	}

	if(status & ATA_STAT_ERR) {
		errno = inport_b(ide->base + ATA_ERROR);
		printk("WARNING: %s(): error on cdrom device %d,%d, status=0x%x error=0x%x,\n", __FUNCTION__, MAJOR(xd->dev), MINOR(xd->dev), status, errno);
		if(xd->retries < xd->max_retries) {
			xd->retries++;
			atapi_pio_read(ide, drive, xd);
		}
		return 0;
	}

	if(xd->retries >= xd->max_retries) {
		errcode = inport_b(ide->base + ATA_ERROR);
		sense_key = (errcode & 0xF0) >> 4;
		printk("WARNING: %s(): timeout on cdrom device %d,%d, status=0x%x.\n", __FUNCTION__, MAJOR(xd->dev), MINOR(xd->dev), status);
		printk("\tSense Key code indicates a '%s' condition.\n", sense_key_err[sense_key & 0xF]);
		/* no need to retry after an illegal request */
		if((sense_key & 0xF) == RS_ILLEGAL_REQUEST) {
			return -EIO;
		}
		/* a drive reset may be required at this moment */
		xd->retries = 0;
		if(xd->max_retries) {
			xd->max_retries--;
			atapi_pio_read(ide, drive, xd);
			return 0;
		}
		return -EIO;
	}
        drive->xd.count = drive->xd.sectors_to_io;
	return ATAPI_CD_SECTSIZE;
}

static int request_sense(char *buffer, __dev_t dev, struct ide *ide, struct ata_drv *drive)
{
	int errcode;
	int sense_key, sense_asc;

	errcode = inport_b(ide->base + ATA_ERROR);
	sense_key = (errcode & 0xF0) >> 4;
	printk("\tSense Key code indicates a '%s' condition.\n", sense_key_err[sense_key & 0xF]);
	errcode = atapi_cmd_reqsense(ide, drive);
	printk("\t\treqsense() returned %d\n", errcode);
	errcode = atapi_pio_read(ide, drive, &drive->xd);
	printk("\t\tatapi_pio_read() returned %d\n", errcode);
	errcode = (int)(buffer[0] & 0x7F);
	sense_key = (int)(buffer[2] & 0xF);
	sense_asc = (int)(buffer[12] & 0xFF);
	printk("\t\terrcode = %x\n", errcode);
	printk("\t\tsense_key = %x\n", sense_key);
	printk("\t\tsense_asc = %x\n", sense_asc);
	return errcode;
}

static void get_capacity(struct inode *i, struct ide *ide, struct ata_drv *drive)
{
	struct device *d;
	unsigned int lba, blocklen;

	if(atapi_cmd_get_capacity(ide, drive)) {
		printk("WARNING: %s(): error on cdrom device %d,%d while trying to get capacity.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
	}
	/* this line just to catch interrupt */
	ata_set_timeout(ide, WAIT_FOR_CD, WAKEUP_AND_RETURN);
	drive->xfer.copy_read_fn(ide->base + ATA_DATA, (void *)&lba, 4 / drive->xfer.copy_raw_factor);
	drive->xfer.copy_read_fn(ide->base + ATA_DATA, (void *)&blocklen, 4 / drive->xfer.copy_raw_factor);
	d = ide->device;
	((unsigned int *)d->device_data)[MINOR(i->rdev)] = (__bswap32(lba) + 1) * (__bswap32(blocklen) / 1024);
}

int atapi_cd_open(struct inode *i, struct fd *f)
{
	char *buffer;
	int errcode;
	int sense_key, sense_asc;
	int retries;
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];

	if(!(buffer = (void *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	if((errcode = atapi_cmd_testunit(ide, drive))) {
		printk("WARNING: %s(): cdrom device %d,%d is not ready for TEST_UNIT, error %d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), errcode);
		request_sense(buffer, i->rdev, ide, drive);
	}
	/* this line just to catch interrupt */
	ata_set_timeout(ide, WAIT_FOR_CD, WAKEUP_AND_RETURN);

	for(retries = 0; retries < MAX_CD_ERR; retries++) {
		if(!(errcode = atapi_cmd_startstop(CD_LOAD, ide, drive))) {
			break;
		}
		/* FIXME: this code has not been tested */
		printk("WARNING: %s(): cdrom device %d,%d is not ready for CD_LOAD, error %d.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), errcode);
		atapi_pio_read(ide, drive, &drive->xd);
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
				return -ENOMEDIUM;
			}
		}
	}

	if(retries == MAX_CD_ERR) {
		if(sense_key == RS_NOT_READY) {
			kfree((unsigned int)buffer);
			return -ENOMEDIUM;
		}
	}

	if(atapi_cmd_mediumrm(CD_LOCK_MEDIUM, ide, drive)) {
		printk("WARNING: %s(): error on cdrom device %d,%d while trying to lock medium.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev));
		request_sense(buffer, i->rdev, ide, drive);
	}
	/* this line just to catch interrupt */
	ata_set_timeout(ide, WAIT_FOR_CD, WAKEUP_AND_RETURN);

	get_capacity(i, ide, drive);
	kfree((unsigned int)buffer);
	return 0;
}

int atapi_cd_close(struct inode *i, struct fd *f)
{
	struct ide *ide;
	struct ata_drv *drive;
	struct device *d;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];

	/* FIXME: only if device usage == 0 */
	invalidate_buffers(i->rdev);

	if(atapi_cmd_mediumrm(CD_UNLOCK_MEDIUM, ide, drive)) {
		printk("WARNING: %s(): error on cdrom device %d,%d during 0x%x command.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), ATAPI_MEDIUM_REMOVAL);
	}
	/* this line just to catch interrupt */
	ata_set_timeout(ide, WAIT_FOR_CD, WAKEUP_AND_RETURN);

	d = ide->device;
	((unsigned int *)d->device_data)[MINOR(i->rdev)] = 0;

	return 0;
}

int atapi_cd_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	return setup_transfer(BLK_READ, dev, block, buffer, blksize);
}

int atapi_cd_ioctl(struct inode *i, struct fd *f, int cmd, unsigned int arg)
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

__loff_t atapi_cd_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int atapi_cd_init(struct ide *ide, struct ata_drv *drive)
{
	struct device *d;
	unsigned char minor;

	drive->fsop = &atapi_cd_driver_fsop;

	minor = !drive->minor_shift ? 0 : 1 << drive->minor_shift;

	if(!(d = get_device(BLK_DEV, MKDEV(drive->major, minor)))) {
		return -EINVAL;
	}
	SET_MINOR(d->minors, minor);
	((unsigned int *)d->blksize)[minor] = BLKSIZE_2K;

	/* default transfer mode */
	drive->read_fn = atapi_pio_read;
	drive->write_fn = NULL;
	drive->read_end_fn = atapi_pio_read_end;
	drive->write_end_fn = NULL;
	return 0;
}
