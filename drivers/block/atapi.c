/*
 * fiwix/drivers/block/atapi.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/ata.h>
#include <fiwix/atapi_cd.h>
#include <fiwix/timer.h>
#include <fiwix/cpu.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int send_packet_command(unsigned char *pkt, struct ide *ide, struct ata_drv *drive, int len)
{
	int status;

	outport_b(ide->ctrl + ATA_DEV_CTRL, 0);
	ata_delay();
	outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE);
	ata_delay();
	if(ata_select_drv(ide, drive->num, ATA_CHS_MODE, 0)) {
		printk("WARNING: %s(): %s: drive not ready to receive PACKET command.\n", __FUNCTION__, drive->dev_name);
		return 1;
	}

	status = ata_wait_state(ide, ATA_STAT_RDY);
	outport_b(ide->base + ATA_FEATURES, 0);
	outport_b(ide->base + ATA_SECCNT, 0);
	outport_b(ide->base + ATA_SECTOR, 0);
	outport_b(ide->base + ATA_LCYL, len & 0xFF);
	outport_b(ide->base + ATA_HCYL, len >> 8);
	outport_b(ide->base + ATA_DRVHD, drive->num << 4);
	outport_b(ide->base + ATA_COMMAND, ATA_PACKET);
	ata_wait400ns(ide);

	/*
	 * NOTE: Some devices prior to ATA/ATAPI-4 assert INTRQ if enabled
	 * at this point. See IDENTIFY PACKET DEVICE, word 0, bits 5-6 to
	 * determine if an interrupt will occur.
	 */

	status = ata_wait_state(ide, ATA_STAT_DRQ);
	if(status) {
		printk("WARNING: %s(): %s: drive not ready after PACKET sent.\n", __FUNCTION__, drive->dev_name);
		return 1;
	}

	outport_sw(ide->base + ATA_DATA, pkt, 12 / sizeof(short int));
	return 0;
}

int atapi_cmd_testunit(struct ide *ide, struct ata_drv *drive)
{
	unsigned char pkt[12];

	memset_b(pkt, 0, sizeof(pkt));
	pkt[0] = ATAPI_TEST_UNIT;
	return send_packet_command(pkt, ide, drive, 0);
}

int atapi_cmd_reqsense(struct ide *ide, struct ata_drv *drive)
{
	unsigned char pkt[12];

	memset_b(pkt, 0, sizeof(pkt));
	pkt[0] = ATAPI_REQUEST_SENSE;
	pkt[4] = 252;	/* this command can send up to 252 bytes */
	return send_packet_command(pkt, ide, drive, 0);
}

int atapi_cmd_startstop(int action, struct ide *ide, struct ata_drv *drive)
{
	unsigned char pkt[12];

	memset_b(pkt, 0, sizeof(pkt));
	pkt[0] = ATAPI_START_STOP;
	pkt[4] = action;
	return send_packet_command(pkt, ide, drive, 0);
}

int atapi_cmd_mediumrm(int action, struct ide *ide, struct ata_drv *drive)
{
	unsigned char pkt[12];

	memset_b(pkt, 0, sizeof(pkt));
	pkt[0] = ATAPI_MEDIUM_REMOVAL;
	pkt[4] = action;
	return send_packet_command(pkt, ide, drive, 0);
}

int atapi_cmd_get_capacity(struct ide *ide, struct ata_drv *drive)
{
	unsigned char pkt[12];

	memset_b(pkt, 0, sizeof(pkt));
	pkt[0] = ATAPI_GET_CAPACITY;
	return send_packet_command(pkt, ide, drive, 8);
}

int atapi_cmd_read10(struct ide *ide, struct ata_drv *drive)
{
	unsigned char pkt[12];
	struct xfer_data *xd;

	ide->device->xfer_data = &drive->xd;
	xd = &drive->xd;

	memset_b(pkt, 0, sizeof(pkt));
	pkt[0] = ATAPI_READ10;
	pkt[2] = (xd->block >> 24) & 0xFF;
	pkt[3] = (xd->block >> 16) & 0xFF;
	pkt[4] = (xd->block >> 8) & 0xFF;
	pkt[5] = xd->block & 0xFF;
	pkt[7] = (xd->sectors_to_io >> 8) & 0xFF;
	pkt[8] = xd->sectors_to_io & 0xFF;
	return send_packet_command(pkt, ide, drive, BLKSIZE_2K);
}
