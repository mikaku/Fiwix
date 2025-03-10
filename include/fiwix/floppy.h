/*
 * fiwix/include/fiwix/floppy.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FLOPPY_H
#define _FIWIX_FLOPPY_H

#include <fiwix/fs.h>
#include <fiwix/sigcontext.h>

#define FLOPPY_IRQ	6
#define FLOPPY_DMA	2	/* DMA channel */

#define FDC_MAJOR	2	/* fdd major number */

#define FDC_SECTSIZE	512	/* sector size (in bytes) */
#define FDC_TR_DEFAULT	0	/* timer reason is IRQ */
#define FDC_TR_MOTOR	1	/* timer reason is motor on */

#define FDC_SRA		0x3F0	/* Status Register A */
#define FDC_SRB		0x3F1	/* Status Register B */
#define FDC_DOR		0x3F2	/* Digital Output Register */
#define FDC_MSR		0x3F4	/* Main Status Register */
#define FDC_DATA 	0x3F5	/* command/data register */
#define FDC_DIR		0x3F7	/* Digital Input Register */
#define FDC_CCR		0x3F7	/* Configuration Control Register */

#define FDC_ENABLE	0x04	/* bit #2 FDC enabled (normal op) */
#define FDC_DMA_ENABLE	0x08	/* bit #3 DMA enabled */
#define FDC_DRIVE0	0x10	/* motor on for the first drive, the rest will
				 * be calculated by left-shifting this value
				 * with 'current_fdd'.
				 */

#define FDC_DIO		0x40	/* bit #6 DIO I/O direction */
#define FDC_RQM		0x80	/* bit #7 RQM is ready for I/O */

#define MAX_FDC_RESULTS	7
#define MAX_FDC_ERR	5

#define FDC_RESET	0xFF	/* reset indicador */
#define FDC_READ	0xE6
#define FDC_WRITE	0xC5
#define FDC_VERSION	0x10
#define FDC_FORMAT_TRK	0x4D
#define FDC_RECALIBRATE	0x07
#define FDC_SENSEI	0x08
#define FDC_SPECIFY	0x03
#define FDC_SEEK	0x0F
#define FDC_LOCK	0x14
#define FDC_PARTID	0x18

#define ST0		0x00	/* Status Register 0 */
#define ST1		0x01	/* Status Register 1 */
#define ST2		0x02	/* Status Register 2 */

#define ST0_IC		0xC0	/* bits #7 and #6 interrupt code */
#define ST0_SE		0x20	/* bit #5 successful implied seek */
#define ST0_RECALIBRATE	ST0_SE	/* bit #5 successful FDC_RECALIBRATE */
#define ST0_SEEK	ST0_SE	/* bit #5 successful FDC_SEEK */
#define ST0_UC		0x10	/* bit #4 unit needs check (fault) */
#define ST0_NR		0x8	/* bit #3 drive not ready */

#define ST1_NW		0x02	/* bit #1 not writable */

#define ST_PCN		0x01	/* present cylinder */
#define ST_CYL		0x03	/* cylinder returned */
#define ST_HEAD		0x04	/* head returned */
#define ST_SECTOR	0x05	/* sector returned */

/* floppy disk drive type */
struct fddt {
	short int size;	 	/* number of sectors */
	short int sizekb;	/* size in KB */
	char tracks;		/* number of tracks */
	char spt;	 	/* number of sectors per track */
	char heads;	 	/* number of heads */
	char gpl1;		/* GAP in READ/WRITE operations */
	char gpl2;		/* GAP in FORMAT TRACK operations */
	char rate;		/* data rate value */
	char spec;		/* SRT+HUT (StepRate + HeadUnload) Time */
	char hlt;		/* HLT (Head Load Time) */
	char *name;		/* unit name */
};

void irq_floppy(int, struct sigcontext *);
void fdc_timer(unsigned int);

int fdc_open(struct inode *, struct fd *);
int fdc_close(struct inode *, struct fd *);
int fdc_read(__dev_t, __blk_t, char *, int);
int fdc_write(__dev_t, __blk_t, char *, int);
int fdc_ioctl(struct inode *, struct fd *, int, unsigned int);
__loff_t fdc_llseek(struct inode *, __loff_t);

void floppy_init(void);

#endif /* _FIWIX_FLOPPY_H */
