/*
 * fiwix/drivers/block/ata.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/ata.h>
#include <fiwix/ata_pci.h>
#include <fiwix/ata_hd.h>
#include <fiwix/atapi_cd.h>
#include <fiwix/devices.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/cpu.h>
#include <fiwix/pic.h>
#include <fiwix/irq.h>
#include <fiwix/pci.h>
#include <fiwix/fs.h>
#include <fiwix/blk_queue.h>
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
	ata_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
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

static struct ide default_ide_table[NR_IDE_CTRLS] = {
	{ IDE_PRIMARY, "primary", IDE0_BASE, IDE0_CTRL, 0, IDE0_IRQ, 0, 0, &ide0_timer, { 0 }, 0, 0,
		{
			{ IDE_MASTER, "master", "hda", IDE0_MAJOR, 0, IDE_MASTER_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0, 0, 0, {{ 0 }} },
			{ IDE_SLAVE, "slave", "hdb", IDE0_MAJOR, 0, IDE_SLAVE_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0, 0, 0, {{ 0 }} }
		}
	},
	{ IDE_SECONDARY, "secondary", IDE1_BASE, IDE1_CTRL, 0, IDE1_IRQ, 0, 0, &ide1_timer, { 0 }, 0, 0,
		{
			{ IDE_MASTER, "master", "hdc", IDE1_MAJOR, 0, IDE_MASTER_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0, 0, 0, {{ 0 }} },
			{ IDE_SLAVE, "slave", "hdd", IDE1_MAJOR, 0, IDE_SLAVE_MSF, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0, 0, 0, {{ 0 }} }
		}
	}
};

static struct device ide_device[NR_IDE_CTRLS] = {
	{
		"ide0",
		IDE0_MAJOR,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		0,
		&ata_driver_fsop,
		NULL,
		NULL,
		NULL
	},
	{
		"ide1",
		IDE1_MAJOR,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		0,
		&ata_driver_fsop,
		NULL,
		NULL,
		NULL
	}
};

static struct interrupt irq_config_ide[NR_IDE_CTRLS] = {
	{ 0, "ide0", &irq_ide0, NULL },
	{ 0, "ide1", &irq_ide1, NULL }
};


static void ata_identify_device(struct ide *ide, struct ata_drv *drive)
{
	int cmd, status;

	if((status = ata_select_drv(ide, drive->num, ATA_CHS_MODE, 0))) {
		/* some controllers return 0xFF to indicate a non-drive condition */
		if(status == 0xFF) {
			return;
		}
		printk("WARNING: %s(): error on device '%s'.\n", __FUNCTION__, drive->dev_name);
		ata_error(ide, status);
	}

	cmd = (drive->flags & DRIVE_IS_ATAPI) ? ATAPI_IDENTIFY_PACKET : ATA_IDENTIFY;

	outport_b(ide->base + ATA_FEATURES, 0);
	outport_b(ide->base + ATA_SECCNT, 0);
	outport_b(ide->base + ATA_SECTOR, 0);
	outport_b(ide->base + ATA_LCYL, 0);
	outport_b(ide->base + ATA_HCYL, 0);
	ata_set_timeout(ide, WAIT_FOR_DISK, WAKEUP_AND_RETURN);
	outport_b(ide->base + ATA_COMMAND, cmd);
	if(ide->wait_interrupt) {
		sleep(ide, PROC_UNINTERRUPTIBLE);
	}
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
	if(!(buffer = (void *)kmalloc(ATA_HD_SECTSIZE))) {
		return 1;
	}
	inport_sw(ide->base + ATA_DATA, (void *)buffer, ATA_HD_SECTSIZE / sizeof(short int));

	/* re-read again using a 32bit transfer */
	ata_identify_device(ide, drive);
	status = inport_b(ide->base + ATA_STATUS);
	if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
		kfree((unsigned int)buffer);
		return 1;
	}
	if(!(buffer2 = (void *)kmalloc(ATA_HD_SECTSIZE))) {
		kfree((unsigned int)buffer);
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
	kfree((unsigned int)buffer2);
	memcpy_b(&drive->ident, (void *)buffer, sizeof(struct ata_drv_ident));
	kfree((unsigned int)buffer);


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

/*
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
*/

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
	/*int udma, udma_speed[] = { 16, 25, 33, 44, 66, 100 };*/

	if(!(drive->flags & (DRIVE_IS_DISK | DRIVE_IS_CDROM))) {
		return;
	}

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
	/*udma = get_udma(drive);*/

	size = (__loff_t)drive->nr_sects * BPS;
	size = size >> 10;
	if(size < 1024) {
		/* the size is less than 1MB (will be reported in KB) */
		ksize = size;
		size = 0;
	} else {
		size = size >> 10;
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
		drive->xfer.read_cmd = ATA_READ_PIO;
		drive->xfer.write_cmd = ATA_WRITE_PIO;
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

	/* default common values for 'xfer' */
	drive->xfer.copy_read_fn = inport_sw;
	drive->xfer.copy_write_fn = outport_sw;
	drive->xfer.copy_raw_factor = 2;	/* 16bit */

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
		}
	}

#ifdef CONFIG_PCI
	if(ide->pci_dev) {
		if(drive->flags & DRIVE_IS_DISK) {
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
	}
#endif /* CONFIG_PCI */

	if(drive->flags & DRIVE_HAS_DATA32) {
		printk(", 32bit");
		drive->xfer.copy_read_fn = inport_sl;
		drive->xfer.copy_write_fn = outport_sl;
		drive->xfer.copy_raw_factor = 4;
	} else {
		printk(", 16bit");
	}
	printk(", multi %d", drive->multi);
	if(drive->flags & DRIVE_HAS_RW_MULTIPLE) {
		printk("(%d)", drive->ident.rw_multiple & 0xFF);
	}

	if(drive->ident.capabilities & ATA_HAS_LBA) {
		drive->flags |= DRIVE_REQUIRES_LBA;
		printk(", LBA");
	}

	printk("\n");
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
	ata_set_timeout(ide, WAIT_FOR_DISK, WAKEUP_AND_RETURN);
	if(ide->wait_interrupt) {
		sleep(ide, PROC_UNINTERRUPTIBLE);
	}

	if(ata_wait_nobusy(ide)) {
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

	if(ata_wait_nobusy(ide)) {
		printk("WARNING: %s(): reset error on ide%d(1).\n", __FUNCTION__, ide->channel);
		/* select drive 0 (don't care of ATA_STAT_BSY bit) */
		outport_b(ide->base + ATA_DRVHD, ATA_CHS_MODE);
		ata_delay();
		ata_wait_nobusy(ide);
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
		ata_end_request(ide);
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
		ata_end_request(ide);
	}
}

void ide0_timer(unsigned int arg)
{
	struct ide *ide;

	ide = &ide_table[IDE_PRIMARY];
	ide->irq_timeout = 1;
	ide->wait_interrupt = 0;
	ata_end_request(ide);
}

void ide1_timer(unsigned int arg)
{
	struct ide *ide;

	ide = &ide_table[IDE_SECONDARY];
	ide->irq_timeout = 1;
	ide->wait_interrupt = 0;
	ata_end_request(ide);
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

int ata_wait_nobusy(struct ide *ide)
{
	int n, retries, status;

	status = 0;
	SET_ATA_RDY_RETR(retries);

	for(n = 0; n < retries; n++) {
		status = inport_b(ide->base + ATA_STATUS);
		if(!(status & ATA_STAT_BSY)) {
			return 0;
		}
		ata_delay();
	}
	return status;
}

int ata_wait_state(struct ide *ide, unsigned char state)
{
	int n, retries, status;

	status = 0;
	SET_ATA_RDY_RETR(retries);

	for(n = 0; n < retries; n++) {
		status = inport_b(ide->base + ATA_STATUS);
		if(!(status & ATA_STAT_BSY)) {
			if(status & (ATA_STAT_ERR | ATA_STAT_DWF)) {
				return status;
			}
			if(status & state) {
				return 0;
			}
		}
		ata_delay();
	}

	return status;
}

int ata_io(struct ide *ide, struct ata_drv *drive, __off_t offset, int nrsectors)
{
	int cyl, sector, head;

	if(drive->flags & DRIVE_REQUIRES_LBA) {
		if(!ata_select_drv(ide, drive->num, ATA_LBA_MODE, offset >> 24)) {
			outport_b(ide->base + ATA_FEATURES, 0);
			outport_b(ide->base + ATA_SECCNT, nrsectors);
			outport_b(ide->base + ATA_LOWLBA, offset & 0xFF);
			outport_b(ide->base + ATA_MIDLBA, (offset >> 8) & 0xFF);
			outport_b(ide->base + ATA_HIGHLBA, (offset >> 16) & 0xFF);
			return 0;
		}
	} else {
		sector2chs(offset, &cyl, &head, &sector, &drive->ident);
		if(!ata_select_drv(ide, drive->num, ATA_CHS_MODE, head)) {
			outport_b(ide->base + ATA_FEATURES, 0);
			outport_b(ide->base + ATA_SECCNT, nrsectors);
			outport_b(ide->base + ATA_SECTOR, sector);
			outport_b(ide->base + ATA_LCYL, cyl);
			outport_b(ide->base + ATA_HCYL, (cyl >> 8));
			return 0;
		}
	}

	printk("WARNING: %s(): %s: drive not ready.\n", __FUNCTION__, drive->dev_name);
	return -EIO;
}

void ata_set_timeout(struct ide *ide, int timeout, int reason)
{
	ide->wait_interrupt = ide->base;
	ide->creq.fn = ide->timer_fn;
	ide->creq.arg = reason;
	add_callout(&ide->creq, timeout);
}

void ata_end_request(struct ide *ide)
{
	struct blk_request *br;
	struct xfer_data *xd;

	if(!ide->irq_timeout) {
		del_callout(&ide->creq);
	}

	if(ide->creq.arg == WAKEUP_AND_RETURN) {
		wakeup(ide);
		return;
	}

	if((br = (struct blk_request *)ide->device->requests_queue)) {
		if(br->status != BR_PROCESSING) {	/* FIXME: needed? */
			printk("WARNING: block request: flag is %d in block %d.\n", br->status, br->block);
		}

		xd = (struct xfer_data *)br->device->xfer_data;
		br->errno = xd->rw_end_fn(ide, xd);
		/* FIXME: if -EIO then reset controller and return
		if(br->errno < 0) {
		}
		*/
		if(br->errno < 0 || xd->count == xd->sectors_to_io) {
			ide->device->requests_queue = (void *)br->next;
			br->status = BR_COMPLETED;
			wakeup(br);
			if(br->errno < 0) {
				return;
			}
		}
		if(br->next) {
			run_blk_request(br->device);
		}
	}
}

int ata_select_drv(struct ide *ide, int drive, int mode, unsigned char lba28_head)
{
	int n, status;

	status = 0;

	for(n = 0; n < MAX_IDE_ERR; n++) {
		if((status = ata_wait_nobusy(ide))) {
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
		if((status = ata_wait_nobusy(ide))) {
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

/* set default values */
void ide_table_init(struct ide *ide, int channel)
{
	memcpy_b(ide, &default_ide_table[channel], sizeof(struct ide));
}

int ata_channel_init(struct ide *ide)
{
	int drv_num;
	int devices, errno;
	struct ata_drv *drive;

	if(!register_irq(ide->irq, &irq_config_ide[ide->channel])) {
		enable_irq(ide->irq);
	}

	ide->device = &ide_device[ide->channel];
	errno = ata_softreset(ide);
	devices = 0;

	ide_device[ide->channel].blksize = (unsigned int *)kmalloc(1024);
	ide_device[ide->channel].device_data = (unsigned int *)kmalloc(1024);
	memset_b(ide_device[ide->channel].device_data, 0, 1024);
	memset_b(ide_device[ide->channel].blksize, 0, 1024);

	for(drv_num = IDE_MASTER; drv_num <= IDE_SLAVE; drv_num++) {
		/*
		 * ata_softreset() returns error in the low nibble for master
		 * devices, and in the high nibble for slave devices.
		 */
		if(!(errno & (1 << (drv_num * 4)))) {
			drive = &ide->drive[drv_num];
			if(!(identify_drive(ide, drive))) {
				get_device_size(drive);
				show_capabilities(ide, drive);
				SET_MINOR(ide_device[ide->channel].minors, drv_num << drive->minor_shift);
				ide_device[ide->channel].blksize[drv_num << drive->minor_shift] = BLKSIZE_1K;
				if(!devices) {
					register_device(BLK_DEV, &ide_device[ide->channel]);
				}
				if(drive->flags & DRIVE_IS_DISK) {
					if(!ata_hd_init(ide, drive)) {
						devices++;
					}
				}
				if(drive->flags & DRIVE_IS_CDROM) {
					if(!atapi_cd_init(ide, drive)) {
						devices++;
					}
				}
			}
		}
	}

	if(!devices) {
		disable_irq(ide->irq);
		unregister_irq(ide->irq, &irq_config_ide[ide->channel]);
		kfree((unsigned int)ide_device[ide->channel].blksize);
		kfree((unsigned int)ide_device[ide->channel].device_data);
	}

	return devices;
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
	return drive->fsop->open(i, fd_table);
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
	return drive->fsop->close(i, fd_table);
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
	return drive->fsop->read_block(dev, block, buffer, blksize);
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
	return drive->fsop->write_block(dev, block, buffer, blksize);
}

int ata_ioctl(struct inode *i, int cmd, unsigned int arg)
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
	return drive->fsop->ioctl(i, cmd, arg);
}

__loff_t ata_llseek(struct inode *i, __loff_t offset)
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
	return drive->fsop->llseek(i, offset);
}

void ata_init(void)
{
	int channel;
	struct ide *ide;

	ide_table = (struct ide *)kmalloc(sizeof(struct ide) * NR_IDE_CTRLS);
	memset_b(ide_table, 0, PAGE_SIZE);
	channel = IDE_PRIMARY;

#ifdef CONFIG_PCI
	channel = ata_pci(ide_table);
#endif /* CONFIG_PCI */

	/* ISA addresses are discarded if ide_pci() found a controller */
	channel = channel ? NR_IDE_CTRLS : IDE_PRIMARY;
	ide = ide_table;
	while(channel < NR_IDE_CTRLS) {
		ide_table_init(ide, channel);
		printk("ide%d	  0x%04x-0x%04x    %d\t", channel, ide->base, ide->base + IDE_BASE_LEN, ide->irq);
		printk("ISA IDE controller\n");
		if(!ata_channel_init(ide)) {
			printk("\t\t\t\tno drives detected\n");
		}
		channel++;
		ide++;
	}
}
