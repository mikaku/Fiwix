/*
 * fiwix/include/fiwix/atapi.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ATAPI_H
#define _FIWIX_ATAPI_H

#include <fiwix/ata.h>

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

int send_packet_command(unsigned char *, struct ide *, struct ata_drv *, int);
int atapi_cmd_testunit(struct ide *, struct ata_drv *);
int atapi_cmd_reqsense(struct ide *, struct ata_drv *);
int atapi_cmd_startstop(int, struct ide *, struct ata_drv *);
int atapi_cmd_mediumrm(int, struct ide *, struct ata_drv *);
int atapi_cmd_get_capacity(struct ide *, struct ata_drv *);
int atapi_cmd_read10(struct ide *, struct ata_drv *);

#endif /* _FIWIX_ATAPI_H */
