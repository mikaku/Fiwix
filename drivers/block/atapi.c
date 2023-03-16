/*
 * fiwix/drivers/block/atapi.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/ata.h>
#include <fiwix/atapi.h>
#include <fiwix/atapi_cd.h>
#include <fiwix/timer.h>
#include <fiwix/cpu.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int send_packet_command(unsigned char *pkt, struct ide *ide, struct ata_drv *drive, int blksize)
{
	int n, retries, status;

	outport_b(ide->ctrl + ATA_DEV_CTRL, 0);
	ata_delay();
	outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE);
	ata_delay();
	if(ata_select_drv(ide, drive->num, ATA_CHS_MODE, 0)) {
		printk("WARNING: %s(): %s: drive not ready to receive PACKET command.\n", __FUNCTION__, drive->dev_name);
		return 1;
	}

	CLI();
	outport_b(ide->base + ATA_FEATURES, 0);
	outport_b(ide->base + ATA_SECCNT, 0);
	outport_b(ide->base + ATA_SECTOR, 0);
	outport_b(ide->base + ATA_LCYL, blksize & 0xFF);
	outport_b(ide->base + ATA_HCYL, blksize >> 8);
	outport_b(ide->base + ATA_DRVHD, drive->num << 4);
	outport_b(ide->base + ATA_COMMAND, ATA_PACKET);
	ata_wait400ns(ide);

	/*
	 * NOTE: Some devices prior to ATA/ATAPI-4 assert INTRQ if enabled
	 * at this point. See IDENTIFY PACKET DEVICE, word 0, bits 5-6 to
	 * determine if an interrupt will occur.
	 */

	SET_ATA_RDY_RETR(retries);

	for(n = 0; n < retries; n++) {
		status = inport_b(ide->base + ATA_STATUS);
		if((status & (ATA_STAT_DRQ | ATA_STAT_BSY)) == ATA_STAT_DRQ) {
			break;
		}
		ata_delay();
	}
	if(n >= retries) {
		printk("WARNING: %s(): %s: drive not ready to receive command packet (retries = %d).\n", __FUNCTION__, drive->dev_name, n);
		return 1;
	}

	outport_sw(ide->base + ATA_DATA, pkt, 12 / sizeof(short int));
	return 0;
}

int atapi_read_data(__dev_t dev, char *data, struct ide *ide, struct ata_drv *drive, int blksize, int offset)
{
	int errno, status;
	char *buffer;
	int retries, bytes;

	status = 0;

	for(retries = 0; retries < MAX_IDE_ERR; retries++) {
		if(ata_wait_irq(ide, WAIT_FOR_CD, drive->xfer.read_cmd)) {
			continue;
		}
		status = inport_b(ide->base + ATA_STATUS);
		if(status & ATA_STAT_ERR) {
			continue;
		}

		if((status & (ATA_STAT_DRQ | ATA_STAT_BSY)) == 0) {
			break;
		}

		bytes = (inport_b(ide->base + ATA_HCYL) << 8) + inport_b(ide->base + ATA_LCYL);
		if(!bytes || bytes > blksize) {
			break;
		}

		bytes = MAX(bytes, ATAPI_CD_SECTSIZE);	/* read more than 2048 bytes is not supported */
		buffer = data + offset;
		drive->xfer.read_fn(ide->base + ATA_DATA, (void *)buffer, bytes / drive->xfer.copy_raw_factor);
	}

	if(status & ATA_STAT_ERR) {
		errno = inport_b(ide->base + ATA_ERROR);
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

int atapi_cmd_testunit(struct ide *ide, struct ata_drv *drive)
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

int atapi_cmd_reqsense(struct ide *ide, struct ata_drv *drive)
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

int atapi_cmd_startstop(int action, struct ide *ide, struct ata_drv *drive)
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

int atapi_cmd_mediumrm(int action, struct ide *ide, struct ata_drv *drive)
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

int request_sense(char *buffer, __dev_t dev, struct ide *ide, struct ata_drv *drive)
{
	int errcode;
	int sense_key, sense_asc;

	errcode = inport_b(ide->base + ATA_ERROR);
	sense_key = (errcode & 0xF0) >> 4;
	printk("\tSense Key code indicates a '%s' condition.\n", sense_key_err[sense_key & 0xF]);
	errcode = atapi_cmd_reqsense(ide, drive);
	printk("reqsense() returned %d\n", errcode);
	errcode = atapi_read_data(dev, buffer, ide, drive, BLKSIZE_2K, 0);
	printk("atapi_read_data() returned %d\n", errcode);
	errcode = (int)(buffer[0] & 0x7F);
	sense_key = (int)(buffer[2] & 0xF);
	sense_asc = (int)(buffer[12] & 0xFF);
	printk("errcode = %x\n", errcode);
	printk("sense_key = %x\n", sense_key);
	printk("sense_asc = %x\n", sense_asc);
	return errcode;
}
