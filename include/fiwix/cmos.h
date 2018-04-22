/*
 * fiwix/include/fiwix/cmos.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CMOS_H
#define _FIWIX_CMOS_H

#define CMOS_INDEX	0x70
#define CMOS_DATA	0x71

#define CMOS_STATA_IRQF	0x0F	/* periodic interrupt frequency */
#define CMOS_STATA_UIP	0x80	/* time update in progress */

#define CMOS_STATB_DSE	0x01	/* enable daylight savings */
#define CMOS_STATB_24H	0x02	/* 24-hour mode (0=12h, 1=24h) */
#define CMOS_STATB_DM	0x04	/* time/date in binary mode (0=BCD, 1=binary) */
#define CMOS_STATB_SQWE	0x08	/* enable square wave frequency */
#define CMOS_STATB_UIE	0x10	/* enable update-ended interrupt */
#define CMOS_STATB_AIE	0x20	/* enable alarm interrupt */
#define CMOS_STATB_PIE	0x40	/* enable periodic interrupt */
#define CMOS_STATB_SET	0x80	/* abort clock update */

#define CMOS_STATD_VRT	0x80	/* valid RAM and time */

/* CMOS RAM data registers */
#define CMOS_SEC	0x00	/* second */
#define CMOS_ASEC	0x01	/* alarm second */
#define CMOS_MIN	0x02	/* minute */
#define CMOS_AMIN	0x03	/* alarm minute */
#define CMOS_HOUR	0x04	/* hour */
#define CMOS_AHOUR	0x05	/* alarm hour */
#define CMOS_DOW	0x06	/* day of week */
#define CMOS_DAY	0x07	/* day */
#define CMOS_MONTH	0x08	/* month */
#define CMOS_YEAR	0x09	/* last two digits of year */
#define CMOS_STATA	0x0A	/* status register A */
#define CMOS_STATB	0x0B	/* status register B */
#define CMOS_STATC	0x0C	/* status register C */
#define CMOS_STATD	0x0D	/* status register D */
#define CMOS_DIAG	0x0E	/* diagnostics status */
#define CMOS_FDDTYPE	0x10	/* floppy disk drive type */
#define CMOS_HDDTYPE	0x12	/* hard disk drive type */
#define CMOS_CENTURY	0x32	/* century */

/* conversions */
#define BCD2BIN(bcd)	(((bcd) >> 4) * 10) + ((bcd) & 0x0F)
#define BIN2BCD(bin)	((bin) % 10) | (((bin) / 10) << 4)

int cmos_update_in_progress(void);
unsigned char cmos_read_date(unsigned char);
void cmos_write_date(unsigned char, unsigned char);
unsigned char cmos_read(unsigned char);
void cmos_write(unsigned char, unsigned char);

#endif /* _FIWIX_CMOS_H */
