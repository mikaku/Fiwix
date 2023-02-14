/*
 * fiwix/drivers/block/ata.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/ata.h>
#include <fiwix/ata_hd.h>
#include <fiwix/atapi_cd.h>
#include <fiwix/devices.h>
#include <fiwix/sleep.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/cpu.h>
#include <fiwix/pic.h>
#include <fiwix/irq.h>
#include <fiwix/pci.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations ata_driver_fsop = {
	0,
	0,

	ata_open,
	ata_close,
	NULL,			/* read */
	NULL,			/* write */
	ata_ioctl,
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

	ata_read,
	ata_write,

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

struct ide *ide_table;

#ifdef CONFIG_PCI
static struct pci_supported_devices supported[] = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_1, 5 },	/* 82371SB PIIX3 [Natoma/Triton II] */
	{ 0, 0 }
};
#endif /* CONFIG_PCI */

struct ide default_ide_table[NR_IDE_CTRLS] = {
	{ IDE_PRIMARY, "primary", IDE0_BASE, IDE0_CTRL, 0, IDE0_IRQ, 0, 0, &ide0_timer, 0, { 0, 0 },
		{
			{ IDE_MASTER, "master", "hda", IDE0_MAJOR, 0, IDE_MASTER_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, {{ 0 }} },
			{ IDE_SLAVE, "slave", "hdb", IDE0_MAJOR, 0, IDE_SLAVE_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, {{ 0 }} }
		}
	},
	{ IDE_SECONDARY, "secondary", IDE1_BASE, IDE1_CTRL, 0, IDE1_IRQ, 0, 0, &ide1_timer, 0, { 0, 0 },
		{
			{ IDE_MASTER, "master", "hdc", IDE1_MAJOR, 0, IDE_MASTER_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, {{ 0 }} },
			{ IDE_SLAVE, "slave", "hdd", IDE1_MAJOR, 0, IDE_SLAVE_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, {{ 0 }} }
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
		&ata_driver_fsop,
		NULL
	},
	{
		"ide1",
		IDE1_MAJOR,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		&ide1_sizes,
		&ata_driver_fsop,
		NULL
	}
};

static struct interrupt irq_config_ide[NR_IDE_CTRLS] = {
	{ 0, "ide0", &irq_ide0, NULL },
	{ 0, "ide1", &irq_ide1, NULL }
};


static int ata_identify_device(struct ide *ide, struct ata_drv *drive)
{
	int cmd, status;

	if((status = ata_select_drv(ide, drive->num, ATA_CHS_MODE, 0))) {
		/* some controllers return 0xFF to indicate a non-drive condition */
		if(status == 0xFF) {
			return 1;
		}
		printk("WARNING: %s(): error on device '%s'.\n", __FUNCTION__, drive->dev_name);
		ata_error(ide, status);
	}

	cmd = (drive->flags & DRIVE_IS_ATAPI) ? ATA_IDENTIFY_PACKET : ATA_IDENTIFY;

	outport_b(ide->base + ATA_FEATURES, 0);
	outport_b(ide->base + ATA_SECCNT, 0);
	outport_b(ide->base + ATA_SECTOR, 0);
	outport_b(ide->base + ATA_LCYL, 0);
	outport_b(ide->base + ATA_HCYL, 0);
	ata_wait_irq(ide, WAIT_FOR_DISK, cmd);
	return 0;
}

static int identify_drive(struct ide *ide, struct ata_drv *drive)
{
	short int status;
	unsigned char *buffer, *buffer2;
	int n;

	/* read device identification using a 16bit transfer */
	ata_identify_device(ide, drive);
	status = inport_b(ide->base + ATA_STATUS);
	if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
		return 1;
	}
	if(!(buffer = (void *)kmalloc2(ATA_HD_SECTSIZE))) {
		return 1;
	}
	inport_sw(ide->base + ATA_DATA, (void *)buffer, ATA_HD_SECTSIZE / sizeof(short int));

	/* re-read again using a 32bit transfer */
	ata_identify_device(ide, drive);
	status = inport_b(ide->base + ATA_STATUS);
	if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
		kfree2((unsigned int)buffer);
		return 1;
	}
	if(!(buffer2 = (void *)kmalloc2(ATA_HD_SECTSIZE))) {
		kfree2((unsigned int)buffer);
		return 1;
	}
	inport_sl(ide->base + ATA_DATA, (void *)buffer2, ATA_HD_SECTSIZE / sizeof(unsigned int));

	/* now compare results */
	drive->flags |= DRIVE_HAS_DATA32;
	for(n = 0; n < ATA_HD_SECTSIZE; n++) {
		if(buffer[n] != buffer2[n]) {
			/* not good, fall back to 16bits */
			drive->flags &= ~DRIVE_HAS_DATA32;
			break;
		}
	}
	kfree2((unsigned int)buffer2);
	memcpy_b(&drive->ident, (void *)buffer, sizeof(struct ata_drv_ident));
	kfree2((unsigned int)buffer);


	/* some basic checks to make sure that data received makes sense */
	if(drive->ident.logic_cyls > 0x7F00 &&
	   drive->ident.logic_heads > 0x7F00 &&
	   drive->ident.logic_spt > 0x7F00 &&
	   drive->ident.buffer_cache > 0x7F00) {
		memset_b(&drive->ident, 0, sizeof(struct ata_drv_ident));
		return 1;
	}

	if(drive->flags & DRIVE_IS_ATAPI) {
		if(((drive->ident.gen_config >> 8) & 0x1F) == ATAPI_IS_CDROM) {
			drive->flags |= DRIVE_IS_CDROM;
		}
		if(drive->ident.gen_config & 0x3) {
			printk("WARNING: %s(): packet size must be 16 bytes!\n");
		}
	}

	/* more checks */
	if(!(drive->flags & DRIVE_IS_CDROM) &&
	   (!drive->ident.logic_cyls ||
	   !drive->ident.logic_heads ||
	   !drive->ident.logic_spt)) {
		memset_b(&drive->ident, 0, sizeof(struct ata_drv_ident));
		return 1;
	}

	return 0;
}

static void get_device_size(struct ata_drv *drive)
{
	if(drive->ident.capabilities & ATA_HAS_LBA) {
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
	if(!(drive->ident.capabilities & ATA_HAS_LBA)) {
		if(drive->nr_sects == 0) {
			drive->nr_sects = drive->ident.logic_cyls * drive->ident.logic_heads * drive->ident.logic_spt;
		}
	}

}

static int get_piomode(struct ata_drv *drive)
{
	int piomode;

	piomode = 0;

	if(drive->ident.fields_validity & ATA_HAS_ADVANCED_PIO) {
		if(drive->ident.adv_pio_modes & 1) {
			piomode = 3;
		}
		if(drive->ident.adv_pio_modes & 2) {
			piomode = 4;
		}
	}

	return piomode;
}

static int get_dma(struct ata_drv *drive)
{
	int dma;

	dma = 0;

	if(drive->ident.multiword_dma & 7) {
		if(drive->ident.multiword_dma & 2) {
			dma = 1;
		}
		if(drive->ident.multiword_dma & 4) {
			dma = 2;
		}
	}

	return dma;
}

static int get_udma(struct ata_drv *drive)
{
	int udma;

	if(drive->ident.fields_validity & ATA_HAS_UDMA) {
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

static int get_ata(struct ata_drv *drive)
{
	int ata;

	ata = 3;	/* minimum is ATA-3 */
	while(drive->ident.majorver & (1 << (ata + 1))) {
		ata++;
	}

	if(!ata) {
		ata = 1;	/* now minimum is ATA-1 */
		while(drive->ident.majorver & (1 << (ata + 1))) {
			ata++;
		}
	}

	return ata;
}

static void show_capabilities(struct ide *ide, struct ata_drv *drive)
{
	unsigned int cyl, hds, sect;
	__loff_t size;
	int ksize, nrsectors;
	int udma, udma_speed[] = { 16, 25, 33, 44, 66, 100 };

	cyl = drive->ident.logic_cyls;
	hds = drive->ident.logic_heads;
	sect = drive->ident.logic_spt;

	if(drive->ident.fields_validity & ATA_HAS_CURR_VALUES) {
		cyl = drive->ident.cur_log_cyls;
		hds = drive->ident.cur_log_heads;
		sect = drive->ident.cur_log_spt;
	}

	drive->pio_mode = get_piomode(drive);
	drive->dma_mode = get_dma(drive);
	udma = get_udma(drive);

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

	printk("%s\t\t\t\t", drive->dev_name);
	swap_asc_word(drive->ident.model_number, 40);
	printk("%s %s ", ide->name, drive->name);

	if(!(drive->flags & DRIVE_IS_ATAPI)) {
		printk("ATA");
		printk("%d", get_ata(drive));
	} else {
		printk("ATAPI");
	}

	if(drive->ident.gen_config == ATA_SUPPORTS_CFA) {
		drive->flags |= DRIVE_IS_CFA;
		printk(" CFA");
	}

	if(drive->flags & DRIVE_IS_DISK) {
		if(ksize) {
			printk(" disk drive %dKB\n", ksize);
		} else {
			printk(" disk drive %dMB\n", (unsigned int)size);
		}
		printk("                                model=%s\n", drive->ident.model_number);
		if(drive->nr_sects < ATA_MIN_LBA) {
			printk("\t\t\t\tCHS=%d/%d/%d", cyl, hds, sect);
		} else {
			drive->flags |= DRIVE_REQUIRES_LBA;
			printk("\t\t\t\tsectors=%d", drive->nr_sects);
		}
		printk(" cache=%dKB\n", drive->ident.buffer_cache >> 1);

		/* default values for 'xfer' */
		drive->xfer.read_fn = inport_sw;
		drive->xfer.read_cmd = ATA_READ_PIO;
		drive->xfer.write_fn = outport_sw;
		drive->xfer.write_cmd = ATA_WRITE_PIO;
		drive->xfer.copy_raw_factor = 2;	/* 16bit */
	}

	if(drive->flags & DRIVE_IS_CDROM) {
		printk(" CDROM drive\n");
		printk("\t\t\t\tmodel=%s\n", drive->ident.model_number);
		printk("\t\t\t\tcache=%dKB\n", drive->ident.buffer_cache >> 1);
	}

	/*
	if(udma >= 0) {
		printk(" UDMA%d(%d)", udma, udma_speed[udma]);
	}
	*/

	drive->multi = 1;
	printk("\t\t\t\tPIO%d", drive->pio_mode);
	if((drive->ident.rw_multiple & 0xFF) > 1) {
		/*
		 * Some very old controllers report a value of 16 here but they
		 * don't support read or write multiple in PIO mode. So far,
		 * I can detect these old controlers because they report a zero
		 * in the Advanced PIO Data Transfer Supported Field (word 64).
		 */
		if(drive->pio_mode > 0) {
			drive->flags |= DRIVE_HAS_RW_MULTIPLE;
			drive->xfer.read_cmd = ATA_READ_MULTIPLE_PIO;
			drive->xfer.write_cmd = ATA_WRITE_MULTIPLE_PIO;
			drive->multi = drive->ident.rw_multiple & 0xFF;
			nrsectors = PAGE_SIZE / ATA_HD_SECTSIZE;
			drive->multi = MIN(drive->multi, nrsectors);

			/* FIXME: pending to rethink buffer cache */
			drive->multi = 2;
		}
	}

#ifdef CONFIG_PCI
	if(ide->pci_dev) {
		if(drive->ident.capabilities & ATA_HAS_DMA) {
			drive->flags |= DRIVE_HAS_DMA;
			drive->xfer.read_cmd = ATA_READ_DMA;
			drive->xfer.write_cmd = ATA_WRITE_DMA;
			drive->xfer.bm_command = BM_COMMAND;
			drive->xfer.bm_status = BM_STATUS;
			drive->xfer.bm_prd_addr = BM_PRD_ADDRESS;
			printk(", DMA%d", drive->dma_mode);
		}
	}
#endif /* CONFIG_PCI */

	if(drive->flags & DRIVE_HAS_DATA32) {
		printk(", 32bit");
		drive->xfer.read_fn = inport_sl;
		drive->xfer.write_fn = outport_sl;
		drive->xfer.copy_raw_factor = 4;
	} else {
		printk(", 16bit");
	}
	printk(", %d sectors/blk", drive->multi);

	if(drive->ident.capabilities & ATA_HAS_LBA) {
		drive->flags |= DRIVE_REQUIRES_LBA;
		printk(", LBA");
	}

	printk("\n");

	/*
	printk("\n");
	printk("%s -> %s\n", drive->dev_name, drive->flags & DRIVE_IS_ATAPI ? "ATAPI" : "ATA");
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
	SET_ATA_RDY_RETR(retries);

	for(n = 0; n < retries; n++) {
		status = inport_b(ide->ctrl + ATA_ALT_STATUS);
		if(!(status & ATA_STAT_BSY)) {
			return 0;
		}
		ata_delay();
	}

	inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
	return status;
}

static int ata_softreset(struct ide *ide)
{
	int error;
	unsigned short int dev_type;

	error = 0;

	/* select drive 0 (don't care of ATA_STAT_BSY bit) */
	outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE);
	ata_delay();

	outport_b(ide->ctrl + ATA_DEV_CTRL, ATA_DEVCTR_SRST | ATA_DEVCTR_NIEN);
	ata_delay();
	outport_b(ide->ctrl + ATA_DEV_CTRL, 0);
	ata_delay();

	/* select drive 0 (don't care of ATA_STAT_BSY bit) */
	outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE);
	ata_delay();
	ata_wait_irq(ide, WAIT_FOR_DISK, 0);

	if(ide_get_status(ide)) {
		printk("WARNING: %s(): reset error on ide%d(0).\n", __FUNCTION__, ide->channel);
		error = 1;
	} else {
		/* find out the device type */
		if(inport_b(ide->base + ATA_SECCNT) == 1 && inport_b(ide->base + ATA_SECTOR) == 1) {
			dev_type = (inport_b(ide->base + ATA_HCYL) << 8) | inport_b(ide->base + ATA_LCYL);
			switch(dev_type) {
				case 0xEB14:
					ide->drive[IDE_MASTER].flags |= DRIVE_IS_ATAPI;
					break;
				case 0x0:
				default:
					ide->drive[IDE_MASTER].flags |= DRIVE_IS_DISK;
			}
		}
	}

	/* select drive 0 (don't care of ATA_STAT_BSY bit) */
	outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE + (1 << 4));
	ata_delay();

	if(ide_get_status(ide)) {
		printk("WARNING: %s(): reset error on ide%d(1).\n", __FUNCTION__, ide->channel);
		/* select drive 0 (don't care of ATA_STAT_BSY bit) */
		outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE);
		ata_delay();
		ide_get_status(ide);
		error |= (1 << 4);
	}

	outport_b(ide->ctrl + ATA_DEV_CTRL, 0);
	ata_delay();
	if(error > 1) {
		return error;
	}

	/* find out the device type */
	if(inport_b(ide->base + ATA_SECCNT) == 1 && inport_b(ide->base + ATA_SECTOR) == 1) {
		dev_type = (inport_b(ide->base + ATA_HCYL) << 8) | inport_b(ide->base + ATA_LCYL);
		switch(dev_type) {
			case 0xEB14:
				ide->drive[IDE_SLAVE].flags |= DRIVE_IS_ATAPI;
				break;
			case 0x0:
			default:
				ide->drive[IDE_SLAVE].flags |= DRIVE_IS_DISK;
		}
	}

	return error;
}

static int ide_channel_init(struct ide *ide)
{
	int drv_num;
	int devices, errno;
	struct ata_drv *drive;

	if(!register_irq(ide->irq, &irq_config_ide[ide->channel])) {
		enable_irq(ide->irq);
	}

	errno = ata_softreset(ide);
	devices = 0;
	for(drv_num = IDE_MASTER; drv_num <= IDE_SLAVE; drv_num++) {
		/*
		 * ata_softreset() returns error in the low nibble
		 * for master devices, and in the high nibble for
		 * slave devices.
		 */
		if(!(errno & (1 << (drv_num * 4)))) {
			drive = &ide->drive[drv_num];
			if(!(identify_drive(ide, drive))) {
				get_device_size(drive);
				show_capabilities(ide, drive);
				SET_MINOR(ide_device[ide->channel].minors, drv_num << drive->minor_shift);
				if(!devices) {
					register_device(BLK_DEV, &ide_device[ide->channel]);
				}
				if(drive->flags & DRIVE_IS_DISK) {
					if(!ata_hd_init(ide, drive)) {
						devices++;
					}
				}
				if(drive->flags & DRIVE_IS_CDROM) {
					if(!ata_cd_init(ide, drive)) {
						devices++;
					}
				}
			}
		}
	}

	if(!devices) {
		disable_irq(ide->irq);
		unregister_irq(ide->irq, &irq_config_ide[ide->channel]);
	}

	return devices;
}

static void sector2chs(__off_t offset, int *cyl, int *head, int *sector, struct ata_drv_ident *ident)
{
	int r;

	*cyl = offset / (ident->logic_spt * ident->logic_heads);
	r = offset % (ident->logic_spt * ident->logic_heads);
	*head = r / ident->logic_spt;
	*sector = (r % ident->logic_spt) + 1;
}

void irq_ide0(int num, struct sigcontext *sc)
{
	struct ide *ide;

	ide = &ide_table[IDE_PRIMARY];
	if(!ide->wait_interrupt) {
		printk("WARNING: %s(): unexpected interrupt!\n", __FUNCTION__);
	} else {
		ide->irq_timeout = ide->wait_interrupt = 0;
		wakeup(&irq_ide0);
	}
}

void irq_ide1(int num, struct sigcontext *sc)
{
	struct ide *ide;

	ide = &ide_table[IDE_SECONDARY];
	if(!ide->wait_interrupt) {
		printk("WARNING: %s(): unexpected interrupt!\n", __FUNCTION__);
	} else {
		ide->irq_timeout = ide->wait_interrupt = 0;
		wakeup(&irq_ide1);
	}
}

void ide0_timer(unsigned int arg)
{
	struct ide *ide;

	ide = &ide_table[IDE_PRIMARY];
	ide->irq_timeout = 1;
	ide->wait_interrupt = 0;
	wakeup(&irq_ide0);
}

void ide1_timer(unsigned int arg)
{
	struct ide *ide;

	ide = &ide_table[IDE_SECONDARY];
	ide->irq_timeout = 1;
	ide->wait_interrupt = 0;
	wakeup(&irq_ide1);
}

void ata_error(struct ide *ide, int status)
{
	int error;

	if(status & ATA_STAT_ERR) {
		error = inport_b(ide->base + ATA_ERROR);
		if(error) {
			printk("error=0x%x [", error);
			if(error & ATA_ERR_AMNF) {
				printk("address mark not found, ");
			}
			if(error & ATA_ERR_TK0NF) {
				printk("track 0 not found (no media) or media error, ");
			}
			if(error & ATA_ERR_ABRT) {
				printk("command aborted, ");
			}
			if(error & ATA_ERR_MCR) {
				printk("media change requested, ");
			}
			if(error & ATA_ERR_IDNF) {
				printk("id mark not found, ");
			}
			if(error & ATA_ERR_MC) {
				printk("media changer, ");
			}
			if(error & ATA_ERR_UNC) {
				printk("uncorrectable data, ");
			}
			if(error & ATA_ERR_BBK) {
				printk("bad block, ");
			}
			printk("]");
		}
	}
	if(status & ATA_STAT_DWF) {
		printk("device fault, ");
	}
	if(status & ATA_STAT_BSY) {
		printk("device busy, ");
	}
	printk("\n");
}

void ata_delay(void)
{
	int n;

	for(n = 0; n < 10000; n++) {
		NOP();
	}
}

void ata_wait400ns(struct ide *ide)
{
	int n;

	/* wait 400ns */
	for(n = 0; n < 4; n++) {
		inport_b(ide->ctrl + ATA_ALT_STATUS);
	}
}

int ata_io(struct ide *ide, struct ata_drv *drive, __off_t offset, int nrsectors)
{
	int mode;
	int cyl, sector, head;
	int lba_head;

	outport_b(ide->base + ATA_FEATURES, 0);
	outport_b(ide->base + ATA_SECCNT, nrsectors);
	if(drive->flags & DRIVE_REQUIRES_LBA) {
		outport_b(ide->base + ATA_LOWLBA, offset & 0xFF);
		outport_b(ide->base + ATA_MIDLBA, (offset >> 8) & 0xFF);
		outport_b(ide->base + ATA_HIGHLBA, (offset >> 16) & 0xFF);
		mode = ATA_LBA_MODE;
		lba_head = (offset >> 24) & 0x0F;
	} else {
		sector2chs(offset, &cyl, &head, &sector, &drive->ident);
		outport_b(ide->base + ATA_SECTOR, sector);
		outport_b(ide->base + ATA_LCYL, cyl);
		outport_b(ide->base + ATA_HCYL, (cyl >> 8));
		mode = ATA_CHS_MODE;
		lba_head = head;
	}
	if(ata_select_drv(ide, drive->num, mode, lba_head)) {
		printk("WARNING: %s(): %s: drive not ready.\n", __FUNCTION__, drive->dev_name);
		return -EIO;
	}
	return 0;
}

int ata_wait_irq(struct ide *ide, int timeout, int cmd)
{
	int status;
	struct callout_req creq;

	ide->wait_interrupt = ide->base;
	creq.fn = ide->timer_fn;
	creq.arg = 0;
	add_callout(&creq, timeout);

	if(cmd) {
		outport_b(ide->base + ATA_COMMAND, cmd);
	}
	if(ide->wait_interrupt) {
		sleep(irq_config_ide[ide->channel].handler, PROC_UNINTERRUPTIBLE);
	}
	if(ide->irq_timeout) {
		status = inport_b(ide->base + ATA_STATUS);
		if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
			return 1;
		}
	}
	del_callout(&creq);
	return 0;
}

#ifdef CONFIG_PCI
void ata_setup_dma(struct ide *ide, struct ata_drv *drive, char *buffer, int datalen)
{
	struct prd *prd_table = &drive->xfer.prd_table;

	prd_table->addr = (unsigned int)V2P(buffer);
	prd_table->size = datalen;
	prd_table->eot = PRDT_MARK_END;
	outport_l(ide->bm + drive->xfer.bm_prd_addr, V2P((unsigned int)prd_table));

	/* clear Error and Interrupt bits */
	outport_b(ide->bm + drive->xfer.bm_status, BM_STATUS_ERROR | BM_STATUS_INTR);
}

void ata_start_dma(struct ide *ide, struct ata_drv *drive, int mode)
{
	outport_b(ide->bm + drive->xfer.bm_command, BM_COMMAND_START | mode);
}

void ata_stop_dma(struct ide *ide, struct ata_drv *drive)
{
	int status;

	status = inport_b(ide->bm + drive->xfer.bm_status);
	outport_b(ide->bm + drive->xfer.bm_command, 0);	/* stop bus master */
	outport_b(ide->bm + drive->xfer.bm_status, status);
}
#endif /* CONFIG_PCI */

int ata_select_drv(struct ide *ide, int drive, int mode, unsigned char lba28_head)
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

	/* 0x80 and 0x20 are for the obsolete bits #7 and #5 respectively */
	outport_b(ide->base + ATA_DRVHD, 0x80 | 0x20 | (mode + (drive << 4)) | lba28_head);
	ata_wait400ns(ide);

	for(n = 0; n < MAX_IDE_ERR; n++) {
		if((status = ide_get_status(ide))) {
			continue;
		}
		break;
	}
	return status;
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

int ata_open(struct inode *i, struct fd *fd_table)
{
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, i->rdev)) {
		return -ENXIO;
	}

	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];
	if(drive->fsop && drive->fsop->open) {
		return drive->fsop->open(i, fd_table);
	}
	return -EINVAL;
}

int ata_close(struct inode *i, struct fd *fd_table)
{
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, i->rdev)) {
		return -ENXIO;
	}

	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];
	if(drive->fsop && drive->fsop->close) {
		return drive->fsop->close(i, fd_table);
	}
	return -EINVAL;
}

int ata_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(dev))) {
		printk("%s(): no ide controller!\n", __FUNCTION__);
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, dev)) {
		return -ENXIO;
	}

	drive = &ide->drive[GET_DRIVE_NUM(dev)];
	if(drive->fsop && drive->fsop->read_block) {
		return drive->fsop->read_block(dev, block, buffer, blksize);
	}
	printk("WARNING: %s(): device %d,%d does not have the read_block() method!\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
	return -EINVAL;
}

int ata_write(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(dev))) {
		printk("%s(): no ide controller!\n", __FUNCTION__);
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, dev)) {
		return -ENXIO;
	}

	drive = &ide->drive[GET_DRIVE_NUM(dev)];
	if(drive->fsop && drive->fsop->write_block) {
		return drive->fsop->write_block(dev, block, buffer, blksize);
	}
	printk("WARNING: %s(): device %d,%d does not have the write_block() method!\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
	return -EINVAL;
}

int ata_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	struct ide *ide;
	struct ata_drv *drive;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!get_device(BLK_DEV, i->rdev)) {
		return -ENXIO;
	}

	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];
	if(drive->fsop && drive->fsop->ioctl) {
		return drive->fsop->ioctl(i, cmd, arg);
	}
	return -EINVAL;
}

#ifdef CONFIG_PCI
static int ide_pci(struct ide *ide)
{
	struct pci_device *pci_dev;
	int bus, dev, func, bar;
	int channel, found;
	int size;

	/* FIXME: this driver supports one PCI card only */
	if(!(pci_dev = pci_get_device(supported[0].vendor_id, supported[0].device_id))) {
		return 0;
	}

	bus = pci_dev->bus;
	dev = pci_dev->dev;
	func = pci_dev->func;

	/* disable I/O space and address space */
	pci_write_short(bus, dev, func, PCI_COMMAND, ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY));

	for(bar = 0; bar < supported[0].bars; bar++) {
		pci_dev->bar[bar] = pci_read_long(bus, dev, func, PCI_BASE_ADDRESS_0 + (bar * 4));
		if(pci_dev->bar[bar]) {
			pci_dev->size[bar] = pci_get_barsize(bus, dev, func, bar);
		}
	}

	/* enable I/O space */
	pci_write_short(bus, dev, func, PCI_COMMAND, pci_dev->command | PCI_COMMAND_IO);

	for(found = 0, channel = 0; channel < NR_IDE_CTRLS; channel++, ide++) {
		bar = channel * 2;
		printk("ide%d	  ", channel);
		if(pci_dev->bar[bar]) {
			if((pci_dev->bar[bar] & PCI_BASE_ADDR_SPACE) == PCI_BASE_ADDR_SPACE_MEM ||
			   (pci_dev->bar[bar + 1] & PCI_BASE_ADDR_SPACE) == PCI_BASE_ADDR_SPACE_MEM ||
			   (pci_dev->bar[4 + (channel * 8)] & PCI_BASE_ADDR_SPACE) == PCI_BASE_ADDR_SPACE_MEM) {
				printk("MMIO-based BAR registers are not supported, channel discarded.\n");
				continue;
			}
		}
		found++;
		memcpy_b(ide, &default_ide_table[channel], sizeof(struct ide));
		size = IDE_BASE_LEN;
		if(pci_dev->bar[bar]) {
			if(pci_dev->prog_if & 0xF) {
				ide->base = pci_dev->bar[bar] & 0xFFFC;
				ide->ctrl = pci_dev->bar[bar + 1] & 0xFFFC;
				ide->irq = pci_dev->irq;
				size = pci_dev->size[bar];
			}
		}
		printk("0x%04x-0x%04x    %d\t", ide->base, ide->base + size, ide->irq);
		switch(pci_dev->prog_if & 0xF) {
			case 0:
				printk("ISA IDE controller in compatibility mode-only\n");
				break;
			/*
			case 0x5:
				printk("PCI IDE controller in native mode-only\n");
				break;
			*/
			case 0xA:
				printk("ISA IDE controller in compatibility mode,\n");
				printk("\t\t\t\tsupports both channels switched to PCI native mode\n");
				break;
			/*
			case 0xF:
				printk("PCI IDE controller in native mode,\n");
				printk("\t\t\t\tsupports both channels switched to ISA compatibility mode\n");
				break;
			*/
		}
		if(pci_dev->prog_if & 0x80) {
			ide->bm = (pci_dev->bar[4] + (channel * 8)) & 0xFFFC;
			printk("\t\t\t\tbus master at 0x%x\n", ide->bm);
			/* enable bus master */
			pci_write_short(bus, dev, func, PCI_COMMAND, pci_dev->command | PCI_COMMAND_MASTER);
			ide->pci_dev = pci_dev;

			/* set PCI Latency Timer and transfers timing */
			switch(pci_dev->device_id) {
				case PCI_DEVICE_ID_INTEL_82371SB_1:
					pci_write_char(bus, dev, func, PCI_LATENCY_TIMER, 64);
					/* from the book 'FYSOS: Media Storage Devices', Appendix F */
					pci_write_short(bus, dev, func, 0x40, 0xA344);
					pci_write_short(bus, dev, func, 0x42, 0xA344);
					break;
			}
		}

		pci_show_desc(pci_dev);
		if(!ide_channel_init(ide)) {
			printk("\t\t\t\tno drives detected\n");
		}
	}

	return found;
}
#endif /* CONFIG_PCI */

void ata_init(void)
{
	int channel;
	struct ide *ide;

        ide_table = (struct ide *)kmalloc();
	/* FIXME: this should be:
        ide_table = (struct ide *)kmalloc2(sizeof(struct ide) * NR_IDE_CTRLS);

	 * when kmalloc2() be merged with kmalloc() and accept sizes > 2048
	 */
	memset_b(ide_table, 0, PAGE_SIZE);
	channel = 0;

#ifdef CONFIG_PCI
	channel = ide_pci(ide_table);
#endif /* CONFIG_PCI */

	/* ISA addresses are discarded if ide_pci() found a controller */
	channel = channel ? NR_IDE_CTRLS : IDE_PRIMARY;
	ide = ide_table;
	while(channel < NR_IDE_CTRLS) {
		memcpy_b(ide, &default_ide_table[channel], sizeof(struct ide));
		printk("ide%d	  0x%04x-0x%04x    %d\t", channel, ide->base, ide->base + IDE_BASE_LEN, ide->irq);
		printk("ISA IDE controller\n");
		if(!ide_channel_init(ide)) {
			printk("\t\t\t\tno drives detected\n");
		}
		channel++;
		ide++;
	}
}
