/*
 * fiwix/drivers/block/ata_hd.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/buffer.h>
#include <fiwix/ata.h>
#include <fiwix/ata_pci.h>
#include <fiwix/ata_hd.h>
#include <fiwix/ioctl.h>
#include <fiwix/devices.h>
#include <fiwix/timer.h>
#include <fiwix/cpu.h>
#include <fiwix/part.h>
#include <fiwix/mm.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/pci.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations ata_hd_driver_fsop = {
	0,
	0,

	ata_hd_open,
	ata_hd_close,
	NULL,			/* read */
	NULL,			/* write */
	ata_hd_ioctl,
	ata_hd_llseek,
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

	ata_hd_read,
	ata_hd_write,

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

static void assign_minors(__dev_t rdev, struct ata_drv *drive, struct partition *part)
{
	int n, minor;
	struct device *d;

	minor = 0;

	if(!(d = get_device(BLK_DEV, rdev))) {
		printk("WARNING: %s(): unable to assign minors to device %d,%d.\n", __FUNCTION__, MAJOR(rdev), MINOR(rdev));
		return;
	}

	for(n = 0; n < NR_PARTITIONS; n++) {
		if(drive->num == IDE_MASTER) {
			minor = (1 << drive->minor_shift) + n;
		}
		if(drive->num == IDE_SLAVE) {
			minor = (1 << drive->minor_shift) + n + 1;
		}
		CLEAR_MINOR(d->minors, minor);
		if(part[n].type) {
			SET_MINOR(d->minors, minor);
			((unsigned int *)d->blksize)[minor] = BLKSIZE_1K;
			((unsigned int *)d->device_data)[minor] = part[n].nr_sects / 2;
		}
	}
}

static __off_t block2sector(__blk_t block, int blksize, struct partition *part, int minor)
{
	__off_t sector;

	sector = block * (blksize / ATA_HD_SECTSIZE);
	if(minor) {
		sector += part[minor - 1].startsect;
	}
	return sector;
}

static int setup_transfer(int mode, __dev_t dev, __blk_t block, char *buffer, int blksize)
{
	struct ide *ide;
	struct ata_drv *drive;
	struct partition *part;

	if(!(ide = get_ide_controller(dev))) {
		return -EINVAL;
	}

	drive = &ide->drive[GET_DRIVE_NUM(dev)];
	drive->xd.minor = MINOR(dev);
	if(drive->num) {
		drive->xd.minor &= ~(1 << IDE_SLAVE_MSF);
	}

	blksize = blksize ? blksize : BLKSIZE_1K;
	drive->xd.sectors_to_io = MIN(blksize, PAGE_SIZE) / ATA_HD_SECTSIZE;

	part = drive->part_table;
	drive->xd.offset = block2sector(block, blksize, part, drive->xd.minor);

	if(drive->flags & DRIVE_HAS_RW_MULTIPLE) {
		drive->xd.nrsectors = MIN(drive->xd.sectors_to_io, drive->multi);
		drive->xd.datalen = ATA_HD_SECTSIZE * drive->xd.nrsectors;
	} else {
		drive->xd.nrsectors = 1;
		drive->xd.datalen = ATA_HD_SECTSIZE;
	}

	drive->xd.dev = dev;
	drive->xd.block = block;
	drive->xd.buffer = buffer;
	drive->xd.blksize = blksize;
	drive->xd.count = 0;

	if(mode == BLK_READ) {
#ifdef CONFIG_PCI
		drive->xd.bm_cmd = BM_COMMAND_READ;
#endif /* CONFIG_PCI */
		drive->xd.cmd = drive->xfer.read_cmd;
		drive->xd.mode = "read";
		drive->xd.rw_end_fn = drive->read_end_fn;
		return drive->read_fn(ide, drive, &drive->xd);
	} else {
#ifdef CONFIG_PCI
		drive->xd.bm_cmd = BM_COMMAND_WRITE;
#endif /* CONFIG_PCI */
		drive->xd.cmd = drive->xfer.write_cmd;
		drive->xd.mode = "write";
		drive->xd.rw_end_fn = drive->write_end_fn;
		return drive->write_fn(ide, drive, &drive->xd);
	}
}

static int pio_read(struct ide *ide, struct ata_drv *drive, struct xfer_data *xd)
{
	ide->device->xfer_data = xd;

	if(ata_io(ide, drive, xd->offset, xd->nrsectors)) {
		return -EIO;
	}
	ata_set_timeout(ide, WAIT_FOR_DISK, 0);
	outport_b(ide->base + ATA_COMMAND, xd->cmd);
	return 0;
}

static int pio_read_end(struct ide *ide, struct xfer_data *xd)
{
	struct ata_drv *drive;
	int status;

	drive = &ide->drive[GET_DRIVE_NUM(xd->dev)];

	if(ide->irq_timeout) {
		status = inport_b(ide->base + ATA_STATUS);
		if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
			printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during read.\n", __FUNCTION__, drive->dev_name, MAJOR(xd->dev), MINOR(xd->dev));
			return -EIO;
		}
	}

	status = ata_wait_nobusy(ide);
	if(status & ATA_STAT_ERR) {
		printk("WARNING: %s(): %s: error on hard disk dev %d,%d during read.\n", __FUNCTION__, drive->dev_name, MAJOR(xd->dev), MINOR(xd->dev));
		printk("\tstatus=0x%x ", status);
		ata_error(ide, status);
		printk("\tblock %d, sector %d.\n", xd->block, xd->offset);
		inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
		return -EIO;
	}
	drive->xfer.copy_read_fn(ide->base + ATA_DATA, (void *)xd->buffer, xd->datalen / drive->xfer.copy_raw_factor);
	xd->count += xd->nrsectors;
	if(xd->count < xd->sectors_to_io) {
		xd->offset += xd->nrsectors;
		xd->buffer += (ATA_HD_SECTSIZE * xd->nrsectors);
		return pio_read(ide, drive, xd);
	}
	inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
	return xd->sectors_to_io * ATA_HD_SECTSIZE;
}

static int pio_write(struct ide *ide, struct ata_drv *drive, struct xfer_data *xd)
{
	int status;

	ide->device->xfer_data = xd;

	if(ata_io(ide, drive, xd->offset, xd->nrsectors)) {
		return -EIO;
	}
	outport_b(ide->base + ATA_COMMAND, drive->xfer.write_cmd);
	status = ata_wait_nobusy(ide);
	if(status & ATA_STAT_ERR) {
		printk("WARNING: %s(): %s: error on hard disk dev %d,%d during write.\n", __FUNCTION__, drive->dev_name, MAJOR(xd->dev), MINOR(xd->dev));
		printk("\tstatus=0x%x ", status);
		ata_error(ide, status);
		printk("\tblock %d, sector %d.\n", xd->block, xd->offset);
		inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
		return -EIO;
	}
	ata_set_timeout(ide, WAIT_FOR_DISK, 0);
	drive->xfer.copy_write_fn(ide->base + ATA_DATA, (void *)xd->buffer, xd->datalen / drive->xfer.copy_raw_factor);
	return 0;
}

static int pio_write_end(struct ide *ide, struct xfer_data *xd)
{
	struct ata_drv *drive;
	int status;

	drive = &ide->drive[GET_DRIVE_NUM(xd->dev)];

	if(ide->irq_timeout) {
		status = inport_b(ide->base + ATA_STATUS);
		if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
			printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during write.\n", __FUNCTION__, drive->dev_name, MAJOR(xd->dev), MINOR(xd->dev));
			return -EIO;
		}
	}

	xd->count += xd->nrsectors;
	if(xd->count < xd->sectors_to_io) {
		xd->offset += xd->nrsectors;
		xd->buffer += (ATA_HD_SECTSIZE * xd->nrsectors);
		return pio_write(ide, drive, xd);
	}
	inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
	return xd->sectors_to_io * ATA_HD_SECTSIZE;
}

#ifdef CONFIG_PCI
static int dma_transfer(struct ide *ide, struct ata_drv *drive, struct xfer_data *xd)
{
	ide->device->xfer_data = xd;

	if(ata_io(ide, drive, xd->offset, xd->nrsectors)) {
		return -EIO;
	}

	ata_setup_dma(ide, drive, xd->buffer, xd->datalen);
	ata_start_dma(ide, drive, xd->bm_cmd);
	ata_set_timeout(ide, WAIT_FOR_DISK, 0);
	outport_b(ide->base + ATA_COMMAND, xd->cmd);
	return 0;
}

static int dma_transfer_end(struct ide *ide, struct xfer_data *xd)
{
	struct ata_drv *drive;
	int status;

	drive = &ide->drive[GET_DRIVE_NUM(xd->dev)];

	if(ide->irq_timeout) {
		status = inport_b(ide->base + ATA_STATUS);
		if((status & (ATA_STAT_RDY | ATA_STAT_DRQ)) != (ATA_STAT_RDY | ATA_STAT_DRQ)) {
			ata_stop_dma(ide, drive);
			printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during %s.\n", __FUNCTION__, drive->dev_name, MAJOR(xd->dev), MINOR(xd->dev), xd->mode);
			return -EIO;
		}
	}

	ata_stop_dma(ide, drive);
	status = ata_wait_nobusy(ide);
	if(status & ATA_STAT_ERR) {
		printk("WARNING: %s(): %s: error on hard disk dev %d,%d during %s.\n", __FUNCTION__, drive->dev_name, MAJOR(xd->dev), MINOR(xd->dev), xd->mode);
		printk("\tstatus=0x%x ", status);
		ata_error(ide, status);
		printk("\tblock %d, sector %d.\n", xd->block, xd->offset);
		inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
		return -EIO;
	}
	xd->count += xd->nrsectors;
	if(xd->count < xd->sectors_to_io) {
		xd->offset += xd->nrsectors;
		xd->buffer += (ATA_HD_SECTSIZE * xd->nrsectors);
		dma_transfer(ide, drive, xd);
	}
	inport_b(ide->base + ATA_STATUS);	/* clear any pending interrupt */
	return xd->sectors_to_io * ATA_HD_SECTSIZE;
}
#endif /* CONFIG_PCI */

int ata_hd_open(struct inode *i, struct fd *f)
{
	return 0;
}

int ata_hd_close(struct inode *i, struct fd *f)
{
	sync_buffers(i->rdev);
	return 0;
}

int ata_hd_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	return setup_transfer(BLK_READ, dev, block, buffer, blksize);
}

int ata_hd_write(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	return setup_transfer(BLK_WRITE, dev, block, buffer, blksize);
}

int ata_hd_ioctl(struct inode *i, struct fd *f, int cmd, unsigned int arg)
{
	int minor;
	struct ide *ide;
	struct ata_drv *drive;
	struct partition *part;
	struct hd_geometry *geom;
	struct device *d;
	int errno;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	minor = MINOR(i->rdev);
	drive = &ide->drive[GET_DRIVE_NUM(i->rdev)];
	if(drive->num) {
		minor &= ~(1 << IDE_SLAVE_MSF);
	}

	part = drive->part_table;

	switch(cmd) {
		case HDIO_GETGEO:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(struct hd_geometry)))) {
				return errno;
			}
			geom = (struct hd_geometry *)arg;
	                geom->cylinders = drive->ident.logic_cyls;
	                geom->heads = (char)drive->ident.logic_heads;
	                geom->sectors = (char)drive->ident.logic_spt;
			geom->start = 0;
			if(minor) {
	                	geom->start = part[minor - 1].startsect;
			}
			break;
		case BLKGETSIZE:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int)))) {
				return errno;
			}
			if(!minor) {
				*(int *)arg = (unsigned int)drive->nr_sects;
			} else {
				*(int *)arg = (unsigned int)drive->part_table[minor - 1].nr_sects;
			}
			break;
		case BLKSSZGET:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int)))) {
				return errno;
			}
			*(int *)arg = 512;
			break;
		case BLKBSZGET:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int)))) {
				return errno;
			}
			d = ide->device;
			*(int *)arg = ((unsigned int *)d->blksize)[MINOR(i->rdev)];
			break;
		case BLKFLSBUF:
			sync_buffers(i->rdev);
			invalidate_buffers(i->rdev);
			break;
		case BLKRRPART:
			invalidate_buffers(i->rdev);
			read_msdos_partition(i->rdev, part);
			assign_minors(i->rdev, drive, part);
			break;
		default:
			return -EINVAL;
			break;
	}

	return 0;
}

__loff_t ata_hd_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int ata_hd_init(struct ide *ide, struct ata_drv *drive)
{
	int n, status;
	__dev_t rdev;
	struct device *d;
	struct partition *part;

	rdev = 0;
	drive->fsop = &ata_hd_driver_fsop;
	part = drive->part_table;

	if(drive->num == IDE_MASTER) {
		rdev = MKDEV(drive->major, drive->num);
		drive->minor_shift = IDE_MASTER_MSF;
		if(!(d = get_device(BLK_DEV, rdev))) {
			return -EINVAL;
		}
		((unsigned int *)d->device_data)[0] = drive->nr_sects / 2;
	} else {
		rdev = MKDEV(drive->major, 1 << IDE_SLAVE_MSF);
		drive->minor_shift = IDE_SLAVE_MSF;
		if(!(d = get_device(BLK_DEV, rdev))) {
			return -EINVAL;
		}
		((unsigned int *)d->device_data)[1 << IDE_SLAVE_MSF] = drive->nr_sects / 2;
	}

	outport_b(ide->ctrl + ATA_DEV_CTRL, ATA_DEVCTR_NIEN);
	if(drive->flags & DRIVE_HAS_RW_MULTIPLE) {
		/* read/write in 4KB blocks (8 sectors) as maximum */
		outport_b(ide->base + ATA_SECCNT, MIN(PAGE_SIZE / ATA_HD_SECTSIZE, drive->multi));
		outport_b(ide->base + ATA_COMMAND, ATA_SET_MULTIPLE_MODE);
		ata_wait400ns(ide);
		while(inport_b(ide->base + ATA_STATUS) & ATA_STAT_BSY);
	}
	outport_b(ide->ctrl + ATA_DEV_CTRL, ATA_DEVCTR_DRQ);

	/* setup the transfer mode */
	for(;;) {
		status = inport_b(ide->base + ATA_STATUS);
		if(!(status & ATA_STAT_BSY)) {
			break;
		}
		ata_delay();
	}
	if(ata_select_drv(ide, drive->num, 0, 0)) {
		printk("WARNING: %s(): %s: drive not ready.\n", __FUNCTION__, drive->dev_name);
	}

	/* default transfer mode */
	drive->read_fn = pio_read;
	drive->write_fn = pio_write;
	drive->read_end_fn = pio_read_end;
	drive->write_end_fn = pio_write_end;

	outport_b(ide->base + ATA_FEATURES, ATA_SET_XFERMODE);
	if(drive->flags & DRIVE_HAS_DMA) {
		outport_b(ide->base + ATA_SECCNT, 0x20 | drive->dma_mode);
	} else {
		if(drive->pio_mode > 2) {
			outport_b(ide->base + ATA_SECCNT, 0x08 | drive->pio_mode);
		} else {
			outport_b(ide->base + ATA_SECCNT, 0x00);
		}
	}
	outport_b(ide->base + ATA_SECTOR, 0);
	outport_b(ide->base + ATA_LCYL, 0);
	outport_b(ide->base + ATA_HCYL, 0);
	outport_b(ide->base + ATA_DRVHD, drive->num << 4);
	ata_set_timeout(ide, WAIT_FOR_DISK, WAKEUP_AND_RETURN);
	outport_b(ide->base + ATA_COMMAND, ATA_SET_FEATURES);
	if(ide->wait_interrupt) {
		sleep(ide, PROC_UNINTERRUPTIBLE);
	}
	while(!(inport_b(ide->base + ATA_STATUS) & ATA_STAT_RDY));
	ata_wait400ns(ide);
	status = inport_b(ide->base + ATA_STATUS);
	if(status & (ATA_STAT_ERR | ATA_STAT_DWF)) {
		printk("WARNING: %s(): error while setting transfer mode.\n", __FUNCTION__);
		printk("\t");
		ata_error(ide, status);
		printk("\n");
	}

#ifdef CONFIG_PCI
	/* set DMA Capable drive bit */
	if(drive->flags & DRIVE_HAS_DMA) {
		drive->read_fn = drive->write_fn = dma_transfer;
		drive->read_end_fn = drive->write_end_fn = dma_transfer_end;
		outport_b(ide->bm + BM_STATUS, BM_STATUS_DRVDMA << drive->num);
	}
#endif /* CONFIG_PCI */

	/* show disk partition summary */
	printk("\t\t\t\tpartition summary: ");
	if(!read_msdos_partition(rdev, part)) {
		assign_minors(rdev, drive, part);
		for(n = 0; n < NR_PARTITIONS; n++) {
			/* status values other than 0x00 and 0x80 are invalid */
			if(part[n].status && part[n].status != 0x80) {
				continue;
			}
			if(part[n].type) {
				printk("%s%d ", drive->dev_name, n + 1);
			}
		}
	}
	printk("\n");

	return 0;
}
