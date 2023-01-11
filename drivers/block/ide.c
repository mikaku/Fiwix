/*
 * fiwix/drivers/block/ide.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/ide.h>
#include <fiwix/ide_hd.h>
#include <fiwix/ide_cd.h>
#include <fiwix/devices.h>
#include <fiwix/sleep.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/cpu.h>
#include <fiwix/pic.h>
#include <fiwix/irq.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations ide_driver_fsop = {
	0,
	0,

	ide_open,
	ide_close,
	NULL,			/* read */
	NULL,			/* write */
	ide_ioctl,
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

	ide_read,
	ide_write,

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

int ide0_need_reset = 0;
int ide0_wait_interrupt = 0;
int ide0_timeout = 0;
int ide1_need_reset = 0;
int ide1_wait_interrupt = 0;
int ide1_timeout = 0;

struct ide ide_table[NR_IDE_CTRLS] = {
	{ IDE_PRIMARY, "primary", IDE0_BASE, IDE0_CTRL, IDE0_IRQ, { 0, 0 },
		{
			{ IDE_MASTER, "master", "hda", IDE0_MAJOR, 0, IDE_MASTER_MSF, 0, 0, 0, 0, 0, { 0 }, {{ 0 }} },
			{ IDE_SLAVE, "slave", "hdb", IDE0_MAJOR, 0, IDE_SLAVE_MSF, 0, 0, 0, 0, 0, { 0 }, {{ 0 }} }
		}
	},
	{ IDE_SECONDARY, "secondary", IDE1_BASE, IDE1_CTRL, IDE1_IRQ, { 0, 0 },
		{
			{ IDE_MASTER, "master", "hdc", IDE1_MAJOR, 0, IDE_MASTER_MSF, 0, 0, 0, 0, 0, { 0 }, {{ 0 }} },
			{ IDE_SLAVE, "slave", "hdd", IDE1_MAJOR, 0, IDE_SLAVE_MSF, 0, 0, 0, 0, 0, { 0 }, {{ 0 }} }
		}
	}
};

static unsigned int ide0_sizes[256];
static unsigned int ide1_sizes[256];

static struct device ide_device[NR_IDE_CTRLS] = {
	{
		"ide0",
		IDE0_MAJOR,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		&ide0_sizes,
		&ide_driver_fsop,
		NULL
	},

	{
		"ide1",
		IDE1_MAJOR,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		&ide1_sizes,
		&ide_driver_fsop,
		NULL
	}
};

static struct interrupt irq_config_ide[NR_IDE_CTRLS] = {
	{ 0, "ide0", &irq_ide0, NULL },
	{ 0, "ide1", &irq_ide1, NULL }
};


static int ide_identify(struct ide *ide, struct ide_drv *drive)
{
	short int status, *buffer;
	struct callout_req creq;

	if((status = ide_drvsel(ide, drive->num, IDE_CHS_MODE, 0))) {
		/* some controllers return 0xFF to indicate a non-drive condition */
		if(status == 0xFF) {
			return status;
		}
		printk("WARNING: %s(): error on device '%s'.\n", __FUNCTION__, drive->dev_name);
		ide_error(ide, status);
		return status;
	}

	if(ide->channel == IDE_PRIMARY) {
		ide0_wait_interrupt = ide->base;
		creq.fn = ide0_timer;
		creq.arg = 0;
		add_callout(&creq, WAIT_FOR_IDE);
		outport_b(ide->base + IDE_COMMAND, (drive->flags & DEVICE_IS_ATAPI) ? ATA_IDENTIFY_PACKET : ATA_IDENTIFY);
		if(ide0_wait_interrupt) {
			sleep(&irq_ide0, PROC_UNINTERRUPTIBLE);
		}
		if(ide0_timeout) {
			status = inport_b(ide->base + IDE_STATUS);
			if((status & (IDE_STAT_RDY | IDE_STAT_DRQ)) != (IDE_STAT_RDY | IDE_STAT_DRQ)) {
				return 1;
			}
		}
		del_callout(&creq);
	}
	if(ide->channel == IDE_SECONDARY) {
		ide1_wait_interrupt = ide->base;
		creq.fn = ide1_timer;
		creq.arg = 0;
		add_callout(&creq, WAIT_FOR_IDE);
		outport_b(ide->base + IDE_COMMAND, (drive->flags & DEVICE_IS_ATAPI) ? ATA_IDENTIFY_PACKET : ATA_IDENTIFY);
		if(ide1_wait_interrupt) {
			sleep(&irq_ide1, PROC_UNINTERRUPTIBLE);
		}
		if(ide1_timeout) {
			status = inport_b(ide->base + IDE_STATUS);
			if((status & (IDE_STAT_RDY | IDE_STAT_DRQ)) != (IDE_STAT_RDY | IDE_STAT_DRQ)) {
				return 1;
			}
		}
		del_callout(&creq);
	}

	status = inport_b(ide->base + IDE_STATUS);
	if((status & (IDE_STAT_RDY | IDE_STAT_DRQ)) != (IDE_STAT_RDY | IDE_STAT_DRQ)) {
		return 1;
	}

	if(!(buffer = (void *)kmalloc())) {
		return 1;
	}

	inport_sw(ide->base + IDE_DATA, (void *)buffer, IDE_HD_SECTSIZE / sizeof(short int));
	memcpy_b(&drive->ident, (void *)buffer, sizeof(struct ide_drv_ident));
	kfree((unsigned int)buffer);

	/* some basic checks to make sure that data received makes sense */
	if(drive->ident.logic_cyls > 0xF000 &&
	   drive->ident.logic_heads > 0xF000 &&
	   drive->ident.logic_spt > 0xF000 &&
	   drive->ident.buffer_cache > 0xF000) {
		memset_b(&drive->ident, 0, sizeof(struct ide_drv_ident));
		return 1;
	}

	if(drive->ident.gen_config == IDE_SUPPORTS_CFA) {
		drive->flags |= DEVICE_IS_CFA;
	}

	if(drive->flags & DEVICE_IS_ATAPI) {
		if(((drive->ident.gen_config >> 8) & 0x1F) == ATAPI_IS_CDROM) {
			drive->flags |= DEVICE_IS_CDROM;
		}
		if(drive->ident.gen_config & 0x3) {
			printk("WARNING: %s(): packet size must be 16 bytes!\n");
		}
	}

	/* more checks */
	if(!(drive->flags & DEVICE_IS_CDROM) &&
	   (!drive->ident.logic_cyls ||
	   !drive->ident.logic_heads ||
	   !drive->ident.logic_spt)) {
		memset_b(&drive->ident, 0, sizeof(struct ide_drv_ident));
		return 1;
	}

	/* only bits 0-7 are relevant */
	drive->ident.rw_multiple &= 0xFF;
	return 0;
}

static void get_device_size(struct ide_drv *drive)
{
	if(drive->ident.capabilities & IDE_HAS_LBA) {
		drive->lba_cyls = drive->ident.logic_cyls;
		drive->lba_heads = drive->ident.logic_heads;
		drive->lba_factor = 0;

		while(drive->lba_cyls > 1023) {
			if(drive->lba_heads < 255) {
				drive->lba_cyls >>= 1;
				drive->lba_heads <<= 1;
			} else {
				break;
			}
			drive->lba_factor++;
		}
		drive->nr_sects = drive->ident.tot_sectors | (drive->ident.tot_sectors2 << 16);
	}

	/* some old disk drives (ATA or ATA2) don't specify total sectors */
	if(!(drive->ident.capabilities & IDE_HAS_LBA)) {
		if(drive->nr_sects == 0) {
			drive->nr_sects = drive->ident.logic_cyls * drive->ident.logic_heads * drive->ident.logic_spt;
		}
	}

}

static int get_udma(struct ide_drv *drive)
{
	int udma;

	if(drive->ident.fields_validity & IDE_HAS_UDMA) {
		if((drive->ident.ultradma >> 13) & 1) {
			udma = 5;
		} else if((drive->ident.ultradma >> 12) & 1) {
			udma = 4;
		} else if((drive->ident.ultradma >> 11) & 1) {
			udma = 3;
		} else if((drive->ident.ultradma >> 10) & 1) {
			udma = 2;
		} else if((drive->ident.ultradma >> 9) & 1) {
			udma = 1;
		} else {
			udma = 0;
		}
	} else {
		udma = -1;
	}
	return udma;
}

static int get_piomode(struct ide_drv *drive)
{
	int piomode;

	piomode = 0;

	if(drive->ident.fields_validity & IDE_HAS_ADVANCED_PIO) {
		if(drive->ident.adv_pio_modes & 1) {
			piomode = 3;
		}
		if(drive->ident.adv_pio_modes & 2) {
			piomode = 4;
		}
	}

	return piomode;
}

static void ide_results(struct ide *ide, struct ide_drv *drive)
{
	unsigned int cyl, hds, sect;
	__loff_t size;
	int ksize;
	int udma, piomode;
	int udma_speed[] = { 16, 25, 33, 44, 66, 100 };

	cyl = drive->ident.logic_cyls;
	hds = drive->ident.logic_heads;
	sect = drive->ident.logic_spt;

	if(drive->ident.fields_validity & IDE_HAS_CURR_VALUES) {
		cyl = drive->ident.cur_log_cyls;
		hds = drive->ident.cur_log_heads;
		sect = drive->ident.cur_log_spt;
	}

	udma = get_udma(drive);
	piomode = get_piomode(drive);

	/*
	 * After knowing if the device is UDMA capable we could choose between
	 * the PIO transfer mode or the UDMA transfer mode.
	 * FIXME: Currently only PIO mode is supported.
	 */

	size = (__loff_t)drive->nr_sects * BPS;
	size = size / 1024;
	if(size < 1024) {
		/* the size is less than 1MB (will be reported in KB) */
		ksize = size;
		size = 0;
	} else {
		size = size / 1024;
		ksize = 0;
	}

	printk("%s       0x%04x-0x%04x    %d\t", drive->dev_name, ide->base, ide->base + IDE_BASE_LEN, ide->irq);
	swap_asc_word(drive->ident.model_number, 40);
	printk("%s %s ", ide->name, drive->name);

	if(!(drive->flags & DEVICE_IS_ATAPI)) {
		printk("ATA");
	} else {
		printk("ATAPI");
	}

	if(drive->flags & DEVICE_IS_CFA) {
		printk(" CFA");
	}

	if(drive->flags & DEVICE_IS_DISK) {
		if(ksize) {
			printk(" DISK drive %dKB\n", ksize);
		} else {
			printk(" DISK drive %dMB\n", (unsigned int)size);
		}
		printk("                                model=%s\n", drive->ident.model_number);
		if(drive->nr_sects < IDE_MIN_LBA) {
			printk("\t\t\t\tCHS=%d/%d/%d", cyl, hds, sect);
		} else {
			drive->flags |= DEVICE_REQUIRES_LBA;
			printk("\t\t\t\tsectors=%d", drive->nr_sects);
		}
		printk(" cache=%dKB", drive->ident.buffer_cache >> 1);
	}

	if(drive->flags & DEVICE_IS_CDROM) {
		printk(" CDROM drive\n");
		printk("\t\t\t\tmodel=%s\n", drive->ident.model_number);
		printk("\t\t\t\tcache=%dKB", drive->ident.buffer_cache >> 1);
	}

	/*
	if(udma >= 0) {
		printk(" UDMA%d(%d)", udma, udma_speed[udma]);
	}
	*/
	printk(" PIO mode=%d", piomode);

	if(drive->ident.capabilities & IDE_HAS_LBA) {
		drive->flags |= DEVICE_REQUIRES_LBA;
		printk(" LBA");
	}

	printk("\n");

	if(drive->ident.rw_multiple > 1) {
		/*
		 * Some very old controllers report a value of 16 here but they
		 * don't support read or write multiple in PIO mode. So far,
		 * I can detect these old controlers because they report a zero
		 * in the Advanced PIO Data Transfer Supported Field (word 64).
		 */
		if(piomode > 0) {
			drive->flags |= DEVICE_HAS_RW_MULTIPLE;
		}
	}

	/*
	printk("\n");
	printk("%s -> %s\n", drive->dev_name, drive->flags & DEVICE_IS_ATAPI ? "ATAPI" : "ATA");
	printk("general conf  = %d (%b) (0x%x)\n", drive->ident.gen_config, drive->ident.gen_config, drive->ident.gen_config);
	printk("logic_cyls    = %d (%b)\n", drive->ident.logic_cyls, drive->ident.logic_cyls);
	printk("reserved2     = %d (%b)\n", drive->ident.reserved2, drive->ident.reserved2);
	printk("logic_heads   = %d (%b)\n", drive->ident.logic_heads, drive->ident.logic_heads);
	printk("retired4      = %d (%b)\n", drive->ident.retired4, drive->ident.retired4);
	printk("retired5      = %d (%b)\n", drive->ident.retired5, drive->ident.retired5);
	printk("logic_spt     = %d (%b)\n", drive->ident.logic_spt, drive->ident.logic_spt);
	printk("retired7      = %d (%b)\n", drive->ident.retired7, drive->ident.retired7);
	printk("retired8      = %d (%b)\n", drive->ident.retired8, drive->ident.retired8);
	printk("retired9      = %d (%b)\n", drive->ident.retired9, drive->ident.retired9);
	printk("serial number = '%s'\n", drive->ident.serial_number);
	printk("vendor spec20 = %d (%b)\n", drive->ident.vendor_spec20, drive->ident.vendor_spec20);
	printk("buffer cache  = %d (%b)\n", drive->ident.buffer_cache, drive->ident.buffer_cache);
	printk("vendor spec22 = %d (%b)\n", drive->ident.vendor_spec22, drive->ident.vendor_spec22);
	printk("firmware rev  = '%s'\n", drive->ident.firmware_rev);
	printk("model number  = '%s'\n", drive->ident.model_number);
	printk("rw multiple   = %d (%b)\n", drive->ident.rw_multiple, drive->ident.rw_multiple);
	printk("reserved48    = %d (%b)\n", drive->ident.reserved48, drive->ident.reserved48);
	printk("capabilities  = %d (%b)\n", drive->ident.capabilities, drive->ident.capabilities);
	printk("reserved50    = %d (%b)\n", drive->ident.reserved50, drive->ident.reserved50);
	printk("pio mode      = %d (%b)\n", drive->ident.pio_mode, drive->ident.pio_mode);
	printk("dma mode      = %d (%b)\n", drive->ident.dma_mode, drive->ident.dma_mode);
	printk("fields validi = %d (%b)\n", drive->ident.fields_validity, drive->ident.fields_validity);
	printk("cur log cyls  = %d (%b)\n", drive->ident.cur_log_cyls, drive->ident.cur_log_cyls);
	printk("cur log heads = %d (%b)\n", drive->ident.cur_log_heads, drive->ident.cur_log_heads);
	printk("cur log spt   = %d (%b)\n", drive->ident.cur_log_spt, drive->ident.cur_log_spt);
	printk("cur capacity  = %d (%b)\n", drive->ident.cur_capacity | (drive->ident.cur_capacity2 << 16), drive->ident.cur_capacity | (drive->ident.cur_capacity2 << 16));
	printk("mult sect set = %d (%b)\n", drive->ident.mult_sect_set, drive->ident.mult_sect_set);
	printk("tot sectors   = %d (%b)\n", drive->ident.tot_sectors | (drive->ident.tot_sectors2 << 16), drive->ident.tot_sectors | (drive->ident.tot_sectors2 << 16));
	printk("singleword dma= %d (%b)\n", drive->ident.singleword_dma, drive->ident.singleword_dma);
	printk("multiword dma = %d (%b)\n", drive->ident.multiword_dma, drive->ident.multiword_dma);
	printk("adv pio modes = %d (%b)\n", drive->ident.adv_pio_modes, drive->ident.adv_pio_modes);
	printk("min multiword = %d (%b)\n", drive->ident.min_multiword, drive->ident.min_multiword);
	printk("rec multiword = %d (%b)\n", drive->ident.rec_multiword, drive->ident.rec_multiword);
	printk("min pio wo fc = %d (%b)\n", drive->ident.min_pio_wo_fc, drive->ident.min_pio_wo_fc);
	printk("min pio w fc  = %d (%b)\n", drive->ident.min_pio_w_fc, drive->ident.min_pio_w_fc);
	printk("reserved69    = %d (%b)\n", drive->ident.reserved69, drive->ident.reserved69);
	printk("reserved70    = %d (%b)\n", drive->ident.reserved70, drive->ident.reserved70);
	printk("reserved71    = %d (%b)\n", drive->ident.reserved71, drive->ident.reserved71);
	printk("reserved72    = %d (%b)\n", drive->ident.reserved72, drive->ident.reserved72);
	printk("reserved73    = %d (%b)\n", drive->ident.reserved73, drive->ident.reserved73);
	printk("reserved74    = %d (%b)\n", drive->ident.reserved74, drive->ident.reserved74);
	printk("queue depth   = %d (%b)\n", drive->ident.queue_depth, drive->ident.queue_depth);
	printk("reserved76    = %d (%b)\n", drive->ident.reserved76, drive->ident.reserved76);
	printk("reserved77    = %d (%b)\n", drive->ident.reserved77, drive->ident.reserved77);
	printk("reserved78    = %d (%b)\n", drive->ident.reserved78, drive->ident.reserved78);
	printk("reserved79    = %d (%b)\n", drive->ident.reserved79, drive->ident.reserved79);
	printk("major version = %d (%b)\n", drive->ident.majorver, drive->ident.majorver);
	printk("minor version = %d (%b)\n", drive->ident.minorver, drive->ident.minorver);
	printk("cmdset1       = %d (%b)\n", drive->ident.cmdset1, drive->ident.cmdset1);
	printk("cmdset2       = %d (%b)\n", drive->ident.cmdset2, drive->ident.cmdset2);
	printk("cmdsf ext     = %d (%b)\n", drive->ident.cmdsf_ext, drive->ident.cmdsf_ext);
	printk("cmdsf enable1 = %d (%b)\n", drive->ident.cmdsf_enable1, drive->ident.cmdsf_enable1);
	printk("cmdsf enable2 = %d (%b)\n", drive->ident.cmdsf_enable2, drive->ident.cmdsf_enable2);
	printk("cmdsf default = %d (%b)\n", drive->ident.cmdsf_default, drive->ident.cmdsf_default);
	printk("ultra dma     = %d (%b)\n", drive->ident.ultradma, drive->ident.ultradma);
	printk("reserved89    = %d (%b)\n", drive->ident.reserved89, drive->ident.reserved89);
	printk("reserved90    = %d (%b)\n", drive->ident.reserved90, drive->ident.reserved90);
	printk("current apm   = %d (%b)\n", drive->ident.curapm, drive->ident.curapm);
	*/
}

static int ide_get_status(struct ide *ide)
{
	int n, retries, status;

	status = 0;
	SET_IDE_RDY_RETR(retries);

	for(n = 0; n < retries; n++) {
		status = inport_b(ide->ctrl + IDE_ALT_STATUS);
		if(!(status & IDE_STAT_BSY)) {
			return 0;
		}
		ide_delay();
	}

	inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
	return status;
}

void irq_ide0(int num, struct sigcontext *sc)
{
	if(!ide0_wait_interrupt) {
		printk("WARNING: %s(): unexpected interrupt!\n", __FUNCTION__);
		ide0_need_reset = 1;
	} else {
		ide0_timeout = ide0_wait_interrupt = 0;
		wakeup(&irq_ide0);
	}
}

void irq_ide1(int num, struct sigcontext *sc)
{
	if(!ide1_wait_interrupt) {
		printk("WARNING: %s(): unexpected interrupt!\n", __FUNCTION__);
		ide1_need_reset = 1;
	} else {
		ide1_timeout = ide1_wait_interrupt = 0;
		wakeup(&irq_ide1);
	}
}

void ide0_timer(unsigned int arg)
{
	ide0_timeout = 1;
	ide0_wait_interrupt = 0;
	wakeup(&irq_ide0);
}

void ide1_timer(unsigned int arg)
{
	ide1_timeout = 1;
	ide1_wait_interrupt = 0;
	wakeup(&irq_ide1);
}

void ide_error(struct ide *ide, int status)
{
	int error;

	if(status & IDE_STAT_ERR) {
		error = inport_b(ide->base + IDE_ERROR);
		if(error) {
			printk("error=0x%x [", error);
			if(error & IDE_ERR_AMNF) {
				printk("address mark not found, ");
			}
			if(error & IDE_ERR_TK0NF) {
				printk("track 0 not found (no media) or media error, ");
			}
			if(error & IDE_ERR_ABRT) {
				printk("command aborted, ");
			}
			if(error & IDE_ERR_MCR) {
				printk("media change requested, ");
			}
			if(error & IDE_ERR_IDNF) {
				printk("id mark not found, ");
			}
			if(error & IDE_ERR_MC) {
				printk("media changer, ");
			}
			if(error & IDE_ERR_UNC) {
				printk("uncorrectable data, ");
			}
			if(error & IDE_ERR_BBK) {
				printk("bad block, ");
			}
			printk("]");
		}
	}
	if(status & IDE_STAT_DWF) {
		printk("device fault, ");
	}
	if(status & IDE_STAT_BSY) {
		printk("device busy, ");
	}
	printk("\n");
}

void ide_delay(void)
{
	int n;

	for(n = 0; n < 10000; n++) {
		NOP();
	}
}

void ide_wait400ns(struct ide *ide)
{
	int n;

	/* wait 400ns */
	for(n = 0; n < 4; n++) {
		inport_b(ide->ctrl + IDE_ALT_STATUS);
	}
}

int ide_drvsel(struct ide *ide, int drive, int mode, unsigned char lba24_head)
{
	int n, status;

	status = 0;

	for(n = 0; n < MAX_IDE_ERR; n++) {
		if((status = ide_get_status(ide))) {
			continue;
		}
		break;
	}
	if(status) {
		return status;
	}

	outport_b(ide->base + IDE_DRVHD, (mode + (drive << 4)) | lba24_head);
	ide_wait400ns(ide);

	for(n = 0; n < MAX_IDE_ERR; n++) {
		if((status = ide_get_status(ide))) {
			continue;
		}
		break;
	}
	return status;
}

int ide_softreset(struct ide *ide)
{
	int error;
	unsigned short int dev_type;

	error = 0;

	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
	ide_delay();

	outport_b(ide->ctrl + IDE_DEV_CTRL, IDE_DEVCTR_SRST | IDE_DEVCTR_NIEN);
	ide_delay();
	outport_b(ide->ctrl + IDE_DEV_CTRL, 0);
	ide_delay();

	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
	ide_delay();
	if(ide_get_status(ide)) {
		printk("WARNING: %s(): reset error on IDE(%d:0).\n", __FUNCTION__, ide->channel);
		error = 1;
	} else {
		/* find out the device type */
		if(inport_b(ide->base + IDE_SECCNT) == 1 && inport_b(ide->base + IDE_SECNUM) == 1) {
			dev_type = (inport_b(ide->base + IDE_HCYL) << 8) | inport_b(ide->base + IDE_LCYL);
			switch(dev_type) {
				case 0xEB14:
					ide->drive[IDE_MASTER].flags |= DEVICE_IS_ATAPI;
					break;
				case 0x0:
				default:
					ide->drive[IDE_MASTER].flags |= DEVICE_IS_DISK;
			}
		}
	}

	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE + (1 << 4));
	ide_delay();
	if(ide_get_status(ide)) {
		printk("WARNING: %s(): reset error on IDE(%d:1).\n", __FUNCTION__, ide->channel);
		outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
		ide_delay();
		ide_get_status(ide);
		error |= (1 << 4);
	}

	outport_b(ide->ctrl + IDE_DEV_CTRL, 0);
	ide_delay();
	if(error > 1) {
		return error;
	}

	/* find out the device type */
	if(inport_b(ide->base + IDE_SECCNT) == 1 && inport_b(ide->base + IDE_SECNUM) == 1) {
		dev_type = (inport_b(ide->base + IDE_HCYL) << 8) | inport_b(ide->base + IDE_LCYL);
		switch(dev_type) {
			case 0xEB14:
				ide->drive[IDE_SLAVE].flags |= DEVICE_IS_ATAPI;
				break;
			case 0x0:
			default:
				ide->drive[IDE_SLAVE].flags |= DEVICE_IS_DISK;
		}
	}

	return error;
}

struct ide *get_ide_controller(__dev_t dev)
{
	int controller;

	if(MAJOR(dev) == IDE0_MAJOR) {
		controller = IDE_PRIMARY;
	} else {
		if(MAJOR(dev) == IDE1_MAJOR) {
			controller = IDE_SECONDARY;
		} else {
			return NULL;
		}
	}
	return &ide_table[controller];
}

int get_ide_drive(__dev_t dev)
{
	int drive;

	drive = MINOR(dev);
	if(drive) {
		if(drive & (1 << IDE_SLAVE_MSF)) {
			drive = IDE_SLAVE;
		} else {
			drive = IDE_MASTER;
		}
	}
	return drive;
}

int ide_open(struct inode *i, struct fd *fd_table)
{
	int drive;
	struct ide *ide;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, i->rdev)) {
		return -ENXIO;
	}

	drive = get_ide_drive(i->rdev);
	if(ide->drive[drive].fsop && ide->drive[drive].fsop->open) {
		return ide->drive[drive].fsop->open(i, fd_table);
	}
	return -EINVAL;
}

int ide_close(struct inode *i, struct fd *fd_table)
{
	int drive;
	struct ide *ide;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, i->rdev)) {
		return -ENXIO;
	}

	drive = get_ide_drive(i->rdev);
	if(ide->drive[drive].fsop && ide->drive[drive].fsop->close) {
		return ide->drive[drive].fsop->close(i, fd_table);
	}
	return -EINVAL;
}

int ide_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int drive;
	struct ide *ide;

	if(!(ide = get_ide_controller(dev))) {
		printk("%s(): no ide controller!\n", __FUNCTION__);
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, dev)) {
		return -ENXIO;
	}

	drive = get_ide_drive(dev);
	if(ide->drive[drive].fsop && ide->drive[drive].fsop->read_block) {
		return ide->drive[drive].fsop->read_block(dev, block, buffer, blksize);
	}
	printk("WARNING: %s(): device %d,%d does not have the read_block() method!\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
	return -EINVAL;
}

int ide_write(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int drive;
	struct ide *ide;

	if(!(ide = get_ide_controller(dev))) {
		printk("%s(): no ide controller!\n", __FUNCTION__);
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, dev)) {
		return -ENXIO;
	}

	drive = get_ide_drive(dev);
	if(ide->drive[drive].fsop && ide->drive[drive].fsop->write_block) {
		return ide->drive[drive].fsop->write_block(dev, block, buffer, blksize);
	}
	printk("WARNING: %s(): device %d,%d does not have the write_block() method!\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
	return -EINVAL;
}

int ide_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	int drive;
	struct ide *ide;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, i->rdev)) {
		return -ENXIO;
	}

	drive = get_ide_drive(i->rdev);
	if(ide->drive[drive].fsop && ide->drive[drive].fsop->ioctl) {
		return ide->drive[drive].fsop->ioctl(i, cmd, arg);
	}
	return -EINVAL;
}

void ide_init(void)
{
	int channel, drv_num;
	int devices, errno;
	struct ide *ide;
	struct ide_drv *drive;

	for(channel = IDE_PRIMARY; channel <= IDE_SECONDARY; channel++) {
		ide = &ide_table[channel];
		if(!register_irq(ide->irq, &irq_config_ide[channel])) {
			enable_irq(ide->irq);
		}

		errno = ide_softreset(ide);
		devices = 0;
		for(drv_num = IDE_MASTER; drv_num <= IDE_SLAVE; drv_num++) {
			/*
			 * ide_softreset() returns error in the low nibble
			 * for master devices, and in the high nibble for
			 * slave devices.
			 */
			if(!(errno & (1 << (drv_num * 4)))) {
				drive = &ide->drive[drv_num];
				if(!(ide_identify(ide, drive))) {
					get_device_size(drive);
					ide_results(ide, drive);
					SET_MINOR(ide_device[channel].minors, drv_num << drive->minor_shift);
					if(!devices) {
						register_device(BLK_DEV, &ide_device[channel]);
					}
					if(drive->flags & DEVICE_IS_DISK) {
						if(!ide_hd_init(ide, drv_num)) {
							devices++;
						}
					}
					if(drive->flags & DEVICE_IS_CDROM) {
						if(!ide_cd_init(ide, drv_num)) {
							devices++;
						}
					}
				}
			}
		}
		if(!devices) {
			disable_irq(ide->irq);
			unregister_irq(ide->irq, &irq_config_ide[channel]);
		}
	}
}
