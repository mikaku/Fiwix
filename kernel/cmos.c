/*
 * fiwix/kernel/cmos.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/cmos.h>

int cmos_update_in_progress(void)
{
	return(cmos_read(CMOS_STATA) & CMOS_STATA_UIP);
}

unsigned char cmos_read_date(unsigned char addr)
{
	/* make sure an update isn't in progress */
	while(cmos_update_in_progress());

	if(!(cmos_read(CMOS_STATB) & CMOS_STATB_DM)) {
		return BCD2BIN(cmos_read(addr));
	}
	return cmos_read(addr);
}

void cmos_write_date(unsigned char addr, unsigned char value)
{
	/* make sure an update isn't in progress */
	while(cmos_update_in_progress());

	if(!(cmos_read(CMOS_STATB) & CMOS_STATB_DM)) {
		cmos_write(addr, BIN2BCD(value));
	}
	cmos_write(addr, value);
}

unsigned char cmos_read(unsigned char addr)
{
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();
	outport_b(CMOS_INDEX, addr);
	RESTORE_FLAGS(flags);

	return inport_b(CMOS_DATA);
}

void cmos_write(unsigned char addr, unsigned char value)
{
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();
	outport_b(CMOS_INDEX, addr);
	outport_b(CMOS_DATA, value);
	RESTORE_FLAGS(flags);
}
