/*
 * fiwix/drivers/block/ide.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int ide0_need_reset = 0;
int ide0_wait_interrupt = 0;
int ide0_timeout = 0;
int ide1_need_reset = 0;
int ide1_wait_interrupt = 0;
int ide1_timeout = 0;

struct ide ide_table[NR_IDE_CTRLS] = {
	{ IDE_PRIMARY, IDE0_BASE, IDE0_CTRL, IDE0_IRQ,
		{
			{ IDE_MASTER, "hda", IDE0_MAJOR, 0, IDE_MASTER_MSF, NULL, NULL, NULL, NULL, NULL, { NULL }, {{ NULL }} },
			{ IDE_SLAVE, "hdb", IDE0_MAJOR, 0, IDE_SLAVE_MSF, NULL, NULL, NULL, NULL, NULL, { NULL }, {{ NULL }} }
		}
	},
	{ IDE_SECONDARY, IDE1_BASE, IDE1_CTRL, IDE1_IRQ,
		{
			{ IDE_MASTER, "hdc", IDE1_MAJOR, 0, IDE_MASTER_MSF, NULL, NULL, NULL, NULL, NULL, { NULL }, {{ NULL }} },
			{ IDE_SLAVE, "hdd", IDE1_MAJOR, 0, IDE_SLAVE_MSF, NULL, NULL, NULL, NULL, NULL, { NULL }, {{ NULL }} }
		}
	}
};

static char *ide_ctrl_name[] = { "primary", "secondary" };
static char *ide_drv_name[] = { "master", "slave" };

static unsigned int ide0_sizes[256];
static unsigned int ide1_sizes[256];

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

static struct device ide0_device = {
        "ide0",
	IDE0_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	&ide0_sizes,
	&ide_driver_fsop,
	NULL
};

static struct device ide1_device = {
        "ide1",
	IDE1_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	&ide1_sizes,
	&ide_driver_fsop,
	NULL
};

static struct interrupt irq_config_ide0 = { 0, "ide0", &irq_ide0, NULL };
static struct interrupt irq_config_ide1 = { 0, "ide1", &irq_ide1, NULL };

static int ide_identify(struct ide *ide, int drive)
{
	short int status, *buffer;
	struct callout_req creq;

	if((status = ide_drvsel(ide, drive, IDE_CHS_MODE, 0))) {
		/* some controllers return 0xFF to indicate a non-drive condition */
		if(status == 0xFF) {
			return status;
		}
		printk("WARNING: %s(): error on device '%s'.\n", __FUNCTION__, ide->drive[drive].dev_name);
		ide_error(ide, status);
		return status;
	}

	outport_b(ide->base + IDE_COMMAND, (ide->drive[drive].flags & DEVICE_IS_ATAPI) ? ATA_IDENTIFY_PACKET : ATA_IDENTIFY);
	if(ide->channel == IDE_PRIMARY) {
		ide0_wait_interrupt = ide->base;
		creq.fn = ide0_timer;
		creq.arg = 0;
		add_callout(&creq, WAIT_FOR_IDE);
		sleep(&irq_ide0, PROC_UNINTERRUPTIBLE);
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
		sleep(&irq_ide1, PROC_UNINTERRUPTIBLE);
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
	memcpy_b(&ide->drive[drive].ident, (void *)buffer, sizeof(struct ide_drv_ident));
	kfree((unsigned int)buffer);

	if(ide->drive[drive].ident.gen_config == IDE_SUPPORTS_CFA) {
		ide->drive[drive].flags |= DEVICE_IS_CFA;
	}

	if(ide->drive[drive].flags & DEVICE_IS_ATAPI) {
		if(((ide->drive[drive].ident.gen_config >> 8) & 0x1F) == ATAPI_IS_CDROM) {
			ide->drive[drive].flags |= DEVICE_IS_CDROM;
		}
		if(ide->drive[drive].ident.gen_config & 0x3) {
			printk("WARNING: %s(): packet size must be 16 bytes!\n");
		}
	}

	/* only bits 0-7 are relevant */
	ide->drive[drive].ident.rw_multiple &= 0xFF;
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

static int get_udma(struct ide *ide, int drive)
{
	int udma;

	if(ide->drive[drive].ident.fields_validity & IDE_HAS_UDMA) {
		if((ide->drive[drive].ident.ultradma >> 13) & 1) {
			udma = 5;
		} else if((ide->drive[drive].ident.ultradma >> 12) & 1) {
			udma = 4;
		} else if((ide->drive[drive].ident.ultradma >> 11) & 1) {
			udma = 3;
		} else if((ide->drive[drive].ident.ultradma >> 10) & 1) {
			udma = 2;
		} else if((ide->drive[drive].ident.ultradma >> 9) & 1) {
			udma = 1;
		} else {
			udma = 0;
		}
	} else {
		udma = -1;
	}
	return udma;
}

static void ide_results(struct ide *ide, int drive)
{
	unsigned int cyl, hds, sect;
	__loff_t capacity;
	int udma;
	int udma_speed[] = { 16, 25, 33, 44, 66, 100 };

	cyl = ide->drive[drive].ident.logic_cyls;
	hds = ide->drive[drive].ident.logic_heads;
	sect = ide->drive[drive].ident.logic_spt;

	udma = get_udma(ide, drive);
	/*
	 * After knowing if the device is UDMA capable we could choose between
	 * the PIO transfer mode or the UDMA transfer mode.
	 * FIXME: Currently only PIO mode is supported.
	 */

	capacity = (__loff_t)ide->drive[drive].nr_sects * BPS;
	capacity = capacity / 1024 / 1024;

	printk("%s       0x%04X-0x%04X   %2d    ", ide->drive[drive].dev_name, ide->base, ide->base + IDE_BASE_LEN, ide->irq);
	swap_asc_word(ide->drive[drive].ident.model_number, 40);
	printk("%s %s ", ide_ctrl_name[ide->channel], ide_drv_name[ide->drive[drive].drive]);

	if(!(ide->drive[drive].flags & DEVICE_IS_ATAPI)) {
		printk("ATA");
	} else {
		printk("ATAPI");
	}

	if(ide->drive[drive].flags & DEVICE_IS_CFA) {
		printk(" CFA");
	}

	if(ide->drive[drive].flags & DEVICE_IS_DISK) {
		printk(" DISK drive %dMB\n", (unsigned int)capacity);
		printk("                                model=%s\n", ide->drive[drive].ident.model_number);
		if(ide->drive[drive].nr_sects < IDE_MIN_LBA) {
			printk("                                CHS=%d/%d/%d", cyl, hds, sect);
		} else {
			ide->drive[drive].flags |= DEVICE_REQUIRES_LBA;
			printk("                                sectors=%d", ide->drive[drive].nr_sects);
		}
		printk(" cache=%dKB", ide->drive[drive].ident.buffer_cache >> 1);
	}

	if(ide->drive[drive].flags & DEVICE_IS_CDROM) {
		printk(" CDROM drive\n");
		printk("                                model=%s\n", ide->drive[drive].ident.model_number);
		printk("                                cache=%dKB", ide->drive[drive].ident.buffer_cache >> 1);
	}

	if(udma >= 0) {
		printk(" UDMA%d(%d)", udma, udma_speed[udma]);
	}
	if(ide->drive[drive].ident.capabilities & IDE_HAS_LBA) {
		ide->drive[drive].flags |= DEVICE_REQUIRES_LBA;
		printk(" LBA");
	}

	printk("\n");

	if(ide->drive[drive].ident.rw_multiple > 1) {
		ide->drive[drive].flags |= DEVICE_HAS_RW_MULTIPLE;
	}

	/*
	printk("\n");
	printk("%s -> %s\n", ide->drive[drive].dev_name, ide->drive[drive].flags & DEVICE_IS_ATAPI ? "ATAPI" : "ATA");
	printk("general conf  = %d (%b) (0x%x)\n", ide->drive[drive].ident.gen_config, ide->drive[drive].ident.gen_config, ide->drive[drive].ident.gen_config);
	printk("logic_cyls    = %d (%b)\n", ide->drive[drive].ident.logic_cyls, ide->drive[drive].ident.logic_cyls);
	printk("reserved2     = %d (%b)\n", ide->drive[drive].ident.reserved2, ide->drive[drive].ident.reserved2);
	printk("logic_heads   = %d (%b)\n", ide->drive[drive].ident.logic_heads, ide->drive[drive].ident.logic_heads);
	printk("retired4      = %d (%b)\n", ide->drive[drive].ident.retired4, ide->drive[drive].ident.retired4);
	printk("retired5      = %d (%b)\n", ide->drive[drive].ident.retired5, ide->drive[drive].ident.retired5);
	printk("logic_spt     = %d (%b)\n", ide->drive[drive].ident.logic_spt, ide->drive[drive].ident.logic_spt);
	printk("retired7      = %d (%b)\n", ide->drive[drive].ident.retired7, ide->drive[drive].ident.retired7);
	printk("retired8      = %d (%b)\n", ide->drive[drive].ident.retired8, ide->drive[drive].ident.retired8);
	printk("retired9      = %d (%b)\n", ide->drive[drive].ident.retired9, ide->drive[drive].ident.retired9);
	printk("serial number = '%s'\n", ide->drive[drive].ident.serial_number);
	printk("vendor spec20 = %d (%b)\n", ide->drive[drive].ident.vendor_spec20, ide->drive[drive].ident.vendor_spec20);
	printk("buffer cache  = %d (%b)\n", ide->drive[drive].ident.buffer_cache, ide->drive[drive].ident.buffer_cache);
	printk("vendor spec22 = %d (%b)\n", ide->drive[drive].ident.vendor_spec22, ide->drive[drive].ident.vendor_spec22);
	printk("firmware rev  = '%s'\n", ide->drive[drive].ident.firmware_rev);
	printk("model number  = '%s'\n", ide->drive[drive].ident.model_number);
	printk("rw multiple   = %d (%b)\n", ide->drive[drive].ident.rw_multiple, ide->drive[drive].ident.rw_multiple);
	printk("reserved48    = %d (%b)\n", ide->drive[drive].ident.reserved48, ide->drive[drive].ident.reserved48);
	printk("capabilities  = %d (%b)\n", ide->drive[drive].ident.capabilities, ide->drive[drive].ident.capabilities);
	printk("reserved50    = %d (%b)\n", ide->drive[drive].ident.reserved50, ide->drive[drive].ident.reserved50);
	printk("pio mode      = %d (%b)\n", ide->drive[drive].ident.pio_mode, ide->drive[drive].ident.pio_mode);
	printk("dma mode      = %d (%b)\n", ide->drive[drive].ident.dma_mode, ide->drive[drive].ident.dma_mode);
	printk("fields validi = %d (%b)\n", ide->drive[drive].ident.fields_validity, ide->drive[drive].ident.fields_validity);
	printk("cur log cyls  = %d (%b)\n", ide->drive[drive].ident.cur_log_cyls, ide->drive[drive].ident.cur_log_cyls);
	printk("cur log heads = %d (%b)\n", ide->drive[drive].ident.cur_log_heads, ide->drive[drive].ident.cur_log_heads);
	printk("cur log spt   = %d (%b)\n", ide->drive[drive].ident.cur_log_spt, ide->drive[drive].ident.cur_log_spt);
	printk("cur capacity  = %d (%b)\n", ide->drive[drive].ident.cur_capacity | (ide->drive[drive].ident.cur_capacity2 << 16), ide->drive[drive].ident.cur_capacity | (ide->drive[drive].ident.cur_capacity2 << 16));
	printk("mult sect set = %d (%b)\n", ide->drive[drive].ident.mult_sect_set, ide->drive[drive].ident.mult_sect_set);
	printk("tot sectors   = %d (%b)\n", ide->drive[drive].ident.tot_sectors | (ide->drive[drive].ident.tot_sectors2 << 16), ide->drive[drive].ident.tot_sectors | (ide->drive[drive].ident.tot_sectors2 << 16));
	printk("singleword dma= %d (%b)\n", ide->drive[drive].ident.singleword_dma, ide->drive[drive].ident.singleword_dma);
	printk("multiword dma = %d (%b)\n", ide->drive[drive].ident.multiword_dma, ide->drive[drive].ident.multiword_dma);
	printk("adv pio modes = %d (%b)\n", ide->drive[drive].ident.adv_pio_modes, ide->drive[drive].ident.adv_pio_modes);
	printk("min multiword = %d (%b)\n", ide->drive[drive].ident.min_multiword, ide->drive[drive].ident.min_multiword);
	printk("rec multiword = %d (%b)\n", ide->drive[drive].ident.rec_multiword, ide->drive[drive].ident.rec_multiword);
	printk("min pio wo fc = %d (%b)\n", ide->drive[drive].ident.min_pio_wo_fc, ide->drive[drive].ident.min_pio_wo_fc);
	printk("min pio w fc  = %d (%b)\n", ide->drive[drive].ident.min_pio_w_fc, ide->drive[drive].ident.min_pio_w_fc);
	printk("reserved69    = %d (%b)\n", ide->drive[drive].ident.reserved69, ide->drive[drive].ident.reserved69);
	printk("reserved70    = %d (%b)\n", ide->drive[drive].ident.reserved70, ide->drive[drive].ident.reserved70);
	printk("reserved71    = %d (%b)\n", ide->drive[drive].ident.reserved71, ide->drive[drive].ident.reserved71);
	printk("reserved72    = %d (%b)\n", ide->drive[drive].ident.reserved72, ide->drive[drive].ident.reserved72);
	printk("reserved73    = %d (%b)\n", ide->drive[drive].ident.reserved73, ide->drive[drive].ident.reserved73);
	printk("reserved74    = %d (%b)\n", ide->drive[drive].ident.reserved74, ide->drive[drive].ident.reserved74);
	printk("queue depth   = %d (%b)\n", ide->drive[drive].ident.queue_depth, ide->drive[drive].ident.queue_depth);
	printk("reserved76    = %d (%b)\n", ide->drive[drive].ident.reserved76, ide->drive[drive].ident.reserved76);
	printk("reserved77    = %d (%b)\n", ide->drive[drive].ident.reserved77, ide->drive[drive].ident.reserved77);
	printk("reserved78    = %d (%b)\n", ide->drive[drive].ident.reserved78, ide->drive[drive].ident.reserved78);
	printk("reserved79    = %d (%b)\n", ide->drive[drive].ident.reserved79, ide->drive[drive].ident.reserved79);
	printk("major version = %d (%b)\n", ide->drive[drive].ident.majorver, ide->drive[drive].ident.majorver);
	printk("minor version = %d (%b)\n", ide->drive[drive].ident.minorver, ide->drive[drive].ident.minorver);
	printk("cmdset1       = %d (%b)\n", ide->drive[drive].ident.cmdset1, ide->drive[drive].ident.cmdset1);
	printk("cmdset2       = %d (%b)\n", ide->drive[drive].ident.cmdset2, ide->drive[drive].ident.cmdset2);
	printk("cmdsf ext     = %d (%b)\n", ide->drive[drive].ident.cmdsf_ext, ide->drive[drive].ident.cmdsf_ext);
	printk("cmdsf enable1 = %d (%b)\n", ide->drive[drive].ident.cmdsf_enable1, ide->drive[drive].ident.cmdsf_enable1);
	printk("cmdsf enable2 = %d (%b)\n", ide->drive[drive].ident.cmdsf_enable2, ide->drive[drive].ident.cmdsf_enable2);
	printk("cmdsf default = %d (%b)\n", ide->drive[drive].ident.cmdsf_default, ide->drive[drive].ident.cmdsf_default);
	printk("ultra dma     = %d (%b)\n", ide->drive[drive].ident.ultradma, ide->drive[drive].ident.ultradma);
	printk("reserved89    = %d (%b)\n", ide->drive[drive].ident.reserved89, ide->drive[drive].ident.reserved89);
	printk("reserved90    = %d (%b)\n", ide->drive[drive].ident.reserved90, ide->drive[drive].ident.reserved90);
	printk("current apm   = %d (%b)\n", ide->drive[drive].ident.curapm, ide->drive[drive].ident.curapm);
	*/
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
		}
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

int ide_ready(struct ide *ide)
{
	int n, retries, status;

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

int ide_drvsel(struct ide *ide, int drive, int mode, unsigned char lba24_head)
{
	int n;
	int status;

	for(n = 0; n < MAX_IDE_ERR; n++) {
		if((status = ide_ready(ide))) {
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
		if((status = ide_ready(ide))) {
			continue;
		}
		break;
	}
	return status;
}

int ide_softreset(struct ide *ide)
{
	int error;

	error = 0;

	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
	ide_delay();

	outport_b(ide->ctrl + IDE_DEV_CTRL, IDE_DEVCTR_SRST | IDE_DEVCTR_NIEN);
	ide_delay();
	outport_b(ide->ctrl + IDE_DEV_CTRL, 0);
	ide_delay();

	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
	ide_delay();
	if(ide_ready(ide)) {
		printk("WARNING: %s(): reset error on IDE(%d:0).\n", __FUNCTION__, ide->channel);
		error = 1;
	} else {
		/* device is disk by default */
		ide->drive[IDE_MASTER].flags |= DEVICE_IS_DISK;

		/* check if it's an ATAPI device */
		if(inport_b(ide->base + IDE_SECCNT) == 1 && inport_b(ide->base + IDE_SECNUM) == 1) {
			if(inport_b(ide->base + IDE_LCYL) == 0x14 && inport_b(ide->base + IDE_HCYL) == 0xEB) {
				ide->drive[IDE_MASTER].flags &= ~DEVICE_IS_DISK;
				ide->drive[IDE_MASTER].flags |= DEVICE_IS_ATAPI;
			}
		}
	}

	outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE + (1 << 4));
	ide_delay();
	if(ide_ready(ide)) {
		printk("WARNING: %s(): reset error on IDE(%d:1).\n", __FUNCTION__, ide->channel);
		outport_b(ide->base + IDE_DRVHD, IDE_CHS_MODE);
		ide_delay();
		ide_ready(ide);
		error |= (1 << 4);
	}

	outport_b(ide->ctrl + IDE_DEV_CTRL, 0);
	ide_delay();
	if(error > 1) {
		return error;
	}

	/* device is disk by default */
	ide->drive[IDE_SLAVE].flags |= DEVICE_IS_DISK;

	/* check if it's an ATAPI device */
	if(inport_b(ide->base + IDE_SECCNT) == 1 && inport_b(ide->base + IDE_SECNUM) == 1) {
		if(inport_b(ide->base + IDE_LCYL) == 0x14 && inport_b(ide->base + IDE_HCYL) == 0xEB) {
			ide->drive[IDE_SLAVE].flags &= ~DEVICE_IS_DISK;
			ide->drive[IDE_SLAVE].flags |= DEVICE_IS_ATAPI;
		}
	}

	return error;
}

struct ide * get_ide_controller(__dev_t dev)
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
	struct device *d;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!(d = get_device(BLK_DEV, i->rdev))) {
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
	struct device *d;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!(d = get_device(BLK_DEV, i->rdev))) {
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
	struct device *d;

	if(!(ide = get_ide_controller(dev))) {
		printk("%s(): no ide controller!\n", __FUNCTION__);
		return -EINVAL;
	}

	if(!(d = get_device(BLK_DEV, dev))) {
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
	struct device *d;

	if(!(ide = get_ide_controller(dev))) {
		printk("%s(): no ide controller!\n", __FUNCTION__);
		return -EINVAL;
	}

	if(!(d = get_device(BLK_DEV, dev))) {
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
	struct device *d;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	if(!(d = get_device(BLK_DEV, i->rdev))) {
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
	int devices, errno;
	struct ide *ide;

	if(!register_irq(IDE0_IRQ, &irq_config_ide0)) {
		enable_irq(IDE0_IRQ);
	}
	devices = 0;

	ide = &ide_table[IDE_PRIMARY];
	errno = ide_softreset(ide);
	if(!(errno & 1)) {
		if(!(ide_identify(ide, IDE_MASTER))) {
			get_device_size(&ide->drive[IDE_MASTER]);
			ide_results(ide, IDE_MASTER);
			SET_MINOR(ide0_device.minors, 0);
			register_device(BLK_DEV, &ide0_device);
			if(ide->drive[IDE_MASTER].flags & DEVICE_IS_DISK) {
				if(!ide_hd_init(ide, IDE_MASTER)) {
					devices++;
				}
			}
			if(ide->drive[IDE_MASTER].flags & DEVICE_IS_CDROM) {
				if(!ide_cd_init(ide, IDE_MASTER)) {
					devices++;
				}
			}
		}
	}
	if(!(errno & 0x10)) {
		if(!(ide_identify(ide, IDE_SLAVE))) {
			get_device_size(&ide->drive[IDE_SLAVE]);
			ide_results(ide, IDE_SLAVE);
			SET_MINOR(ide0_device.minors, 1 << IDE_SLAVE_MSF);
			if(!devices) {
				register_device(BLK_DEV, &ide0_device);
			}
			if(ide->drive[IDE_SLAVE].flags & DEVICE_IS_DISK) {
				if(!ide_hd_init(ide, IDE_SLAVE)) {
					devices++;
				}
			}
			if(ide->drive[IDE_SLAVE].flags & DEVICE_IS_CDROM) {
				if(!ide_cd_init(ide, IDE_SLAVE)) {
					devices++;
				}
			}
		}
	}
	if(!devices) {
		disable_irq(IDE0_IRQ);
		unregister_irq(IDE0_IRQ, &irq_config_ide0);
	}

	if(!register_irq(IDE1_IRQ, &irq_config_ide1)) {
		enable_irq(IDE1_IRQ);
	}
	devices = 0;
	ide = &ide_table[IDE_SECONDARY];
	errno = ide_softreset(ide);
	if(!(errno & 1)) {
		if(!(ide_identify(ide, IDE_MASTER))) {
			get_device_size(&ide->drive[IDE_MASTER]);
			ide_results(ide, IDE_MASTER);
			SET_MINOR(ide1_device.minors, 0);
			register_device(BLK_DEV, &ide1_device);
			if(ide->drive[IDE_MASTER].flags & DEVICE_IS_DISK) {
				if(!ide_hd_init(ide, IDE_MASTER)) {
					devices++;
				}
			}
			if(ide->drive[IDE_MASTER].flags & DEVICE_IS_CDROM) {
				if(!ide_cd_init(ide, IDE_MASTER)) {
					devices++;
				}
			}
		}
	}
	if(!(errno & 0x10)) {
		if(!(ide_identify(ide, IDE_SLAVE))) {
			get_device_size(&ide->drive[IDE_SLAVE]);
			ide_results(ide, IDE_SLAVE);
			SET_MINOR(ide1_device.minors, 1 << IDE_SLAVE_MSF);
			if(!devices) {
				register_device(BLK_DEV, &ide1_device);
			}
			if(ide->drive[IDE_SLAVE].flags & DEVICE_IS_DISK) {
				if(!ide_hd_init(ide, IDE_SLAVE)) {
					devices++;
				}
			}
			if(ide->drive[IDE_SLAVE].flags & DEVICE_IS_CDROM) {
				if(!ide_cd_init(ide, IDE_SLAVE)) {
					devices++;
				}
			}
		}
	}
	if(!devices) {
		disable_irq(IDE1_IRQ);
		unregister_irq(IDE1_IRQ, &irq_config_ide1);
	}
}
