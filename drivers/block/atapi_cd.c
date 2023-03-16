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

/* default size of 1GB is enough to read a whole CDROM */
#define CDROM_DEFAULT_SIZE	(1024 * 1024)	/* in KBs */

static struct fs_operations atapi_cd_driver_fsop = {
	0,
	0,

	atapi_cd_open,
	atapi_cd_close,
	NULL,			/* read */
	NULL,			/* write */
	atapi_cd_ioctl,
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

int atapi_cd_open(struct inode *i, struct fd *fd_table)
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
		atapi_read_data(i->rdev, buffer, ide, drive, BLKSIZE_2K, 0);
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
	atapi_read_data(i->rdev, buffer, ide, drive, BLKSIZE_2K, 0);
	kfree((unsigned int)buffer);

	unlock_resource(&ide->resource);
	return 0;
}

int atapi_cd_close(struct inode *i, struct fd *fd_table)
{
	char *buffer;
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!(buffer = (void *)kmalloc())) {
		return -ENOMEM;
	}

	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];

	/* FIXME: only if device usage == 0 */
	invalidate_buffers(i->rdev);

	if(atapi_cmd_mediumrm(CD_UNLOCK_MEDIUM, ide, drive)) {
		printk("WARNING: %s(): error on cdrom device %d,%d during 0x%x command.\n", __FUNCTION__, MAJOR(i->rdev), MINOR(i->rdev), ATAPI_MEDIUM_REMOVAL);
	}

	/* this line just to catch interrupt */
	atapi_read_data(i->rdev, buffer, ide, drive, BLKSIZE_2K, 0);
	kfree((unsigned int)buffer);

	return 0;
}

int atapi_cd_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int sectors_to_read;
	int n, retries;
	unsigned char pkt[12];
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(dev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_DRIVE_NUM(dev)];
	blksize = BLKSIZE_2K;
	sectors_to_read = blksize / ATAPI_CD_SECTSIZE;

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
			if(atapi_read_data(dev, buffer, ide, drive, blksize, n * ATAPI_CD_SECTSIZE)) {
				int errcode;
				int sense_key;
				errcode = inport_b(ide->base + ATA_ERROR);
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
	return sectors_to_read * ATAPI_CD_SECTSIZE;
}

int atapi_cd_ioctl(struct inode *i, int cmd, unsigned long int arg)
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
	((unsigned int *)d->device_data)[minor] = CDROM_DEFAULT_SIZE;

	return 0;
}
