/*
 * fiwix/include/fiwix/ide.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_IDE_H
#define _FIWIX_IDE_H

#include <fiwix/fs.h>
#include <fiwix/part.h>
#include <fiwix/sigcontext.h>
#include <fiwix/sleep.h>

#define IDE0_IRQ		14	/* primary controller interrupt */
#define IDE1_IRQ		15	/* secondary controller interrupt */

#define IDE0_MAJOR		3	/* 1st controller major number */
#define IDE1_MAJOR		22	/* 2nd controller major number */
#define IDE_MINORS		4	/* max. minors/partitions per unit */
#define IDE_MASTER_MSF		0	/* IDE master minor shift factor */
#define IDE_SLAVE_MSF		6	/* IDE slave minor shift factor */

#define IDE_PRIMARY		0
#define IDE_SECONDARY		1
#define IDE_MASTER		0
#define IDE_SLAVE		1
#define IDE_ATA			0
#define IDE_ATAPI		1
#define GET_IDE_DRIVE(_dev_)	((MINOR(_dev_) & (1 <<IDE_SLAVE_MSF)) ? IDE_SLAVE : IDE_MASTER)

#define NR_IDE_CTRLS		2	/* IDE controllers */
#define NR_IDE_DRVS		2	/* max. drives per IDE controller */

/* controller addresses */
#define IDE0_BASE		0x1F0	/* primary controller base addr */
#define IDE0_CTRL		0x3F4	/* primary controller control port */
#define IDE1_BASE		0x170	/* secondary controller base addr */
#define IDE1_CTRL		0x374	/* secondary controller control port */

#define IDE_BASE_LEN		7	/* controller address length */

#define IDE_RDY_RETR_LONG	50000	/* long delay for fast CPUs */
#define IDE_RDY_RETR_SHORT	500	/* short delay for slow CPUs */
#define MAX_IDE_ERR		10	/* number of retries */
#define MAX_CD_ERR		5	/* number of retries in CDROMs */

#define SET_IDE_RDY_RETR(retries)					\
	if((cpu_table.hz / 1000000) <= 100) {				\
		retries = IDE_RDY_RETR_SHORT;				\
	} else {							\
		retries = IDE_RDY_RETR_LONG;				\
	}

#define WAIT_FOR_IDE	(1 * HZ)	/* timeout for hard disk */
#define WAIT_FOR_CD 	(3 * HZ)	/* timeout for cdrom */

/* controller registers */
#define IDE_DATA		0x0	/* Data Port Register (R/W) */
#define IDE_ERROR		0x1	/* Error Register (R) */
#define IDE_FEATURES		0x1	/* Features Register (W) */
#define IDE_SECCNT		0x2	/* Sector Count Register (R/W) */
#define IDE_SECNUM		0x3	/* Sector Number Register (R/W) */
#define IDE_LCYL		0x4	/* Cylinder Low Register (R/W) */
#define IDE_HCYL		0x5	/* Cylinder High Register (R/W) */
#define IDE_DRVHD		0x6	/* Drive/Head Register (R/W) */
#define IDE_STATUS		0x7	/* Status Register (R) */
#define IDE_COMMAND		0x7	/* Command Register (W) */

#define IDE_ALT_STATUS		0x2	/* Alternate Register (R) */
#define IDE_DEV_CTRL		0x2	/* Device Control Register (W) */

/* error register bits */
#define IDE_ERR_AMNF		0x01	/* Address Mark Not Found */
#define IDE_ERR_TK0NF		0x02	/* Track 0 Not Found */
#define IDE_ERR_ABRT		0x04	/* Aborted Command */
#define IDE_ERR_MCR		0x08	/* Media Change Registered */
#define IDE_ERR_IDNF		0x10	/* Sector ID Field Not Found */
#define IDE_ERR_MC		0x20	/* Media Changed */
#define IDE_ERR_UNC		0x40	/* Uncorrectable Data Error */
#define IDE_ERR_BBK		0x80	/* Bad Block */

/* status register bits */
#define IDE_STAT_ERR		0x01	/* an error ocurred */
#define IDE_STAT_SENS		0x02	/* sense data available */
#define IDE_STAT_CORR		0x04	/* a correctable error ocurred */
#define IDE_STAT_DRQ		0x08	/* device is ready to transfer */
#define IDE_STAT_DSC		0x10	/* device requests service o intr. */
#define IDE_STAT_DWF		0x20	/* drive write fault */
#define IDE_STAT_RDY		0x40	/* drive is ready */
#define IDE_STAT_BSY		0x80	/* drive is busy */

#define IDE_CHS_MODE		0xA0	/* select CHS mode */
#define IDE_LBA_MODE		0xE0	/* select LBA mode */

/* alternate & device control register bits */
#define IDE_DEVCTR_DRQ		0x08	/* Data Request */
#define IDE_DEVCTR_NIEN		0x02	/* Disable Interrupt */
#define IDE_DEVCTR_SRST		0x04	/* Software Reset */

/* ATA commands */
#define ATA_READ_PIO		0x20	/* read sector(s) with retries */
#define ATA_READ_MULTIPLE_PIO	0xC4	/* read multiple sectors */
#define ATA_WRITE_PIO		0x30	/* write sector(s) with retries */
#define ATA_WRITE_MULTIPLE_PIO	0xC5	/* write multiple sectors */
#define ATA_SET_MULTIPLE_MODE	0xC6
#define ATA_PACKET		0xA0
#define ATA_IDENTIFY_PACKET	0xA1	/* identify ATAPI device */
#define ATA_IDENTIFY		0xEC	/* identify ATA device */

/* ATAPI commands */
#define ATAPI_TEST_UNIT		0x00
#define ATAPI_REQUEST_SENSE	0x03
#define ATAPI_START_STOP	0x1B
#define ATAPI_MEDIUM_REMOVAL	0x1E
#define ATAPI_READ10		0x28

#define CD_UNLOCK_MEDIUM	0x00	/* allow medium removal */
#define CD_LOCK_MEDIUM		0x01	/* prevent medium removal */
#define CD_EJECT		0x02	/* eject the CD if possible */
#define CD_LOAD			0x03	/* load the CD (closes tray) */

/* ATAPI CD additional sense code */
#define ASC_NOT_READY		0x04
#define ASC_NO_MEDIUM		0x3A

/* capabilities */
#define IDE_SUPPORTS_CFA	0x848A
#define IDE_HAS_DMA		0x100
#define IDE_HAS_LBA		0x200
#define IDE_MIN_LBA		16514064/* sectors limit for using CHS */

/* general configuration bits */
#define IDE_HAS_CURR_VALUES	0x01	/* current logical values are valid */
#define IDE_HAS_ADVANCED_PIO	0x02	/* device supports PIO 3 or 4 */
#define IDE_HAS_UDMA		0x04	/* device supports UDMA */
#define IDE_REMOVABLE		0x80	/* removable media device */

/* ATAPI types */
#define ATAPI_IS_SEQ_ACCESS	0x01	/* sequential-access device */
#define ATAPI_IS_PRINTER	0x02
#define ATAPI_IS_PROCESSOR	0x03
#define ATAPI_IS_WRITE_ONCE	0x04
#define ATAPI_IS_CDROM		0x05
#define ATAPI_IS_SCANNER	0x06

/* IDE drive flags */
#define DEVICE_IS_ATAPI		0x01
#define DEVICE_IS_CFA		0x02
#define DEVICE_IS_DISK		0x04
#define DEVICE_IS_CDROM		0x08
#define DEVICE_REQUIRES_LBA	0x10
#define DEVICE_HAS_RW_MULTIPLE	0x20

/* ATA/ATAPI-4 based */
struct ide_drv_ident {
	unsigned short int gen_config;		/* general configuration bits */
	unsigned short int logic_cyls;		/* logical cylinders */
	unsigned short int reserved2;
	unsigned short int logic_heads;		/* logical heads */
	unsigned short int retired4;
	unsigned short int retired5;
	unsigned short int logic_spt;		/* logical sectors/track */
	unsigned short int retired7;
	unsigned short int retired8;
	unsigned short int retired9;
	char serial_number[20];			/* serial number */
	unsigned short int vendor_spec20;
	unsigned short int buffer_cache;
	unsigned short int vendor_spec22;	/* reserved */
	char firmware_rev[8];			/* firmware version */
	char model_number[40];			/* model number */
	unsigned short int rw_multiple;
	unsigned short int reserved48;
	unsigned short int capabilities;	/* capabilities */
	unsigned short int reserved50;
	unsigned short int pio_mode;		/* PIO data transfer mode*/
	unsigned short int dma_mode;
	unsigned short int fields_validity;	/* fields validity */
	unsigned short int cur_log_cyls;	/* current logical cylinders */
	unsigned short int cur_log_heads;	/* current logical heads */
	unsigned short int cur_log_spt;		/* current logical sectors/track */
	unsigned short int cur_capacity;	/* current capacity in sectors */
	unsigned short int cur_capacity2;	/* 32bit number */
	unsigned short int mult_sect_set;	/* multiple sector settings */
	unsigned short int tot_sectors;		/* sectors (LBA only) */
	unsigned short int tot_sectors2;	/* 32bit number */
	unsigned short int singleword_dma;
	unsigned short int multiword_dma;	/* multiword DMA settings */
	unsigned short int adv_pio_modes;	/* advanced PIO modes */
	unsigned short int min_multiword;	/* min. Multiword DMA transfer */
	unsigned short int rec_multiword;	/* recommended Multiword DMS transfer */
	unsigned short int min_pio_wo_fc;	/* min. PIO w/o flow control */
	unsigned short int min_pio_w_fc;	/* min. PIO with flow control */
	unsigned short int reserved69;
	unsigned short int reserved70;
	unsigned short int reserved71;
	unsigned short int reserved72;
	unsigned short int reserved73;
	unsigned short int reserved74;
	unsigned short int queue_depth;		/* queue depth */
	unsigned short int reserved76;
	unsigned short int reserved77;
	unsigned short int reserved78;
	unsigned short int reserved79;
	unsigned short int majorver;		/* major version number */
	unsigned short int minorver;		/* minor version number */
	unsigned short int cmdset1;		/* command set supported */
	unsigned short int cmdset2;		/* command set supported */
	unsigned short int cmdsf_ext;		/* command set/feature sup.ext. */
	unsigned short int cmdsf_enable1;	/* command s/f enabled */
	unsigned short int cmdsf_enable2;	/* command s/f enabled */
	unsigned short int cmdsf_default;	/* command s/f default */
	unsigned short int ultradma;		/* ultra DMA mode */
	unsigned short int reserved89;
	unsigned short int reserved90;
	unsigned short int curapm;		/* current APM values */
	unsigned short int reserved92_126[35];
	unsigned short int r_status_notif;	/* removable media status notif. */
	unsigned short int security_status;	/* security status */
	unsigned short int vendor_spec129_159[31];
	unsigned short int reserved160_255[96];
};

struct ide_drv {
	int num;			/* master or slave */
	char *name;
	char *dev_name;
	unsigned char major;		/* major number */
	unsigned int flags;
	int minor_shift;		/* shift factor to get the real minor */
	int lba_cyls;
	int lba_heads;
	short int lba_factor;
	unsigned int nr_sects;		/* total sectors (LBA) */
	struct fs_operations *fsop;
	struct ide_drv_ident ident;
	struct partition part_table[NR_PARTITIONS];
};

struct ide {
	int channel;			/* primary or secondary */
	char *name;
	int base;			/* base address */
	int ctrl;			/* control port address */
	short int irq;
	struct resource resource;
	struct ide_drv drive[NR_IDE_DRVS];
};

extern struct ide ide_table[NR_IDE_CTRLS];

extern int ide0_need_reset;
extern int ide0_wait_interrupt;
extern int ide0_timeout;
extern int ide1_need_reset;
extern int ide1_wait_interrupt;
extern int ide1_timeout;

void irq_ide0(int, struct sigcontext *);
void ide0_timer(unsigned int);
void irq_ide1(int, struct sigcontext *);
void ide1_timer(unsigned int);

void ide_error(struct ide *, int);
void ide_delay(void);
void ide_wait400ns(struct ide *);
int ide_drvsel(struct ide *, int, int, unsigned char);
int ide_softreset(struct ide *);

struct ide *get_ide_controller(__dev_t);

int ide_open(struct inode *, struct fd *);
int ide_close(struct inode *, struct fd *);
int ide_read(__dev_t, __blk_t, char *, int);
int ide_write(__dev_t, __blk_t, char *, int);
int ide_ioctl(struct inode *, int, unsigned long int);

void ide_init(void);

#endif /* _FIWIX_IDE_H */
