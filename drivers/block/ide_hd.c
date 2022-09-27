/*
 * fiwix/drivers/block/ide_hd.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/buffer.h>
#include <fiwix/ide.h>
#include <fiwix/ide_hd.h>
#include <fiwix/ioctl.h>
#include <fiwix/devices.h>
#include <fiwix/sleep.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/cpu.h>
#include <fiwix/fs.h>
#include <fiwix/part.h>
#include <fiwix/process.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations ide_hd_driver_fsop = {
	0,
	0,

	ide_hd_open,
	ide_hd_close,
	NULL,			/* read */
	NULL,			/* write */
	ide_hd_ioctl,
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

	ide_hd_read,
	ide_hd_write,

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

static void assign_minors(__dev_t rdev, struct ide *ide, struct partition *part)
{
	int n;
	int drive, minor;
	struct device *d;

	minor = 0;
	drive = get_ide_drive(rdev);

	if(!(d = get_device(BLK_DEV, rdev))) {
		printk("WARNING: %s(): unable to assign minors to device %d,%d.\n", __FUNCTION__, MAJOR(rdev), MINOR(rdev));
		return;
	}

	for(n = 0; n < NR_PARTITIONS; n++) {
		if(drive == IDE_MASTER) {
			minor = (1 << ide->drive[drive].minor_shift) + n;
		}
		if(drive == IDE_SLAVE) {
			minor = (1 << ide->drive[drive].minor_shift) + n + 1;
		}
		CLEAR_MINOR(d->minors, minor);
		if(part[n].type) {
			SET_MINOR(d->minors, minor);
			((unsigned int *)d->device_data)[minor] = part[n].nr_sects / 2;
		}
	}
}

static __off_t block2sector(__blk_t block, int blksize, struct partition *part, int minor)
{
	__off_t sector;

	sector = block * (blksize / IDE_HD_SECTSIZE);
	if(minor) {
		sector += part[minor - 1].startsect;
	}
	return sector;
}

static void sector2chs(__off_t offset, int *cyl, int *head, int *sector, struct ide_drv_ident *ident)
{
	int r;

	*cyl = offset / (ident->logic_spt * ident->logic_heads);
	r = offset % (ident->logic_spt * ident->logic_heads);
	*head = r / ident->logic_spt;
	*sector = (r % ident->logic_spt) + 1;
}

int ide_hd_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int ide_hd_close(struct inode *i, struct fd *fd_table)
{
	sync_buffers(i->rdev);
	return 0;
}

int ide_hd_read(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int minor;
	int drive;
	int sectors_to_read, cmd, mode, lba_head;
	int n, status, r, retries;
	int cyl, head, sector;
	__off_t offset;
	struct ide *ide;
	struct ide_drv_ident *ident;
	struct partition *part;
	struct callout_req creq;

	if(!(ide = get_ide_controller(dev))) {
		return -EINVAL;
	}

	minor = MINOR(dev);
	if((drive = get_ide_drive(dev))) {
		minor &= ~(1 << IDE_SLAVE_MSF);
	}

	SET_IDE_RDY_RETR(retries);

	blksize = blksize ? blksize : BLKSIZE_1K;
	sectors_to_read = MIN(blksize, PAGE_SIZE) / IDE_HD_SECTSIZE;

	ident = &ide->drive[drive].ident;
	part = ide->drive[drive].part_table;
	offset = block2sector(block, blksize, part, minor);

	CLI();
	lock_resource(&ide->resource);

	n = 0;

	while(n < sectors_to_read) {
		if(ide->drive[drive].flags & DEVICE_HAS_RW_MULTIPLE) {
			outport_b(ide->base + IDE_SECCNT, sectors_to_read);
			cmd = ATA_READ_MULTIPLE_PIO;
		} else {
			outport_b(ide->base + IDE_SECCNT, 1);
			cmd = ATA_READ_PIO;
		}

		if(ide->drive[drive].flags & DEVICE_REQUIRES_LBA) {
			outport_b(ide->base + IDE_SECNUM, offset & 0xFF);
			outport_b(ide->base + IDE_LCYL, (offset >> 8) & 0xFF);
			outport_b(ide->base + IDE_HCYL, (offset >> 16) & 0xFF);
			mode = IDE_LBA_MODE;
			lba_head = (offset >> 24) & 0x0F;
		} else {
			sector2chs(offset, &cyl, &head, &sector, ident);
			outport_b(ide->base + IDE_SECNUM, sector);
			outport_b(ide->base + IDE_LCYL, cyl);
			outport_b(ide->base + IDE_HCYL, (cyl >> 8));
			mode = IDE_CHS_MODE;
			lba_head = head;
		}
		if(ide_drvsel(ide, drive, mode, lba_head)) {
			printk("WARNING: %s(): %s: drive not ready.\n", __FUNCTION__, ide->drive[drive].dev_name);
			unlock_resource(&ide->resource);
			return -EIO;
		}
		outport_b(ide->base + IDE_COMMAND, cmd);

		if(ide->channel == IDE_PRIMARY) {
			ide0_wait_interrupt = ide->base;
			creq.fn = ide0_timer;
			creq.arg = 0;
			add_callout(&creq, WAIT_FOR_IDE);
			if(ide0_wait_interrupt) {
				sleep(&irq_ide0, PROC_UNINTERRUPTIBLE);
			}
			if(ide0_timeout) {
				printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during read.\n", __FUNCTION__, ide->drive[drive].dev_name, MAJOR(dev), MINOR(dev));
			} else {
				del_callout(&creq);
			}
		}
		if(ide->channel == IDE_SECONDARY) {
			ide1_wait_interrupt = ide->base;
			creq.fn = ide1_timer;
			creq.arg = 0;
			add_callout(&creq, WAIT_FOR_IDE);
			if(ide1_wait_interrupt) {
				sleep(&irq_ide1, PROC_UNINTERRUPTIBLE);
			}
			if(ide1_timeout) {
				printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during read.\n", __FUNCTION__, ide->drive[drive].dev_name, MAJOR(dev), MINOR(dev));
			} else {
				del_callout(&creq);
			}
		}
		for(r = 0; r < retries; r++) {
			status = inport_b(ide->base + IDE_STATUS);
			if(!(status & IDE_STAT_BSY) && (status & IDE_STAT_DRQ)) {
				break;
			}
			ide_delay();
		}
		if(status & IDE_STAT_ERR) {
			printk("WARNING: %s(): %s: error on hard disk dev %d,%d during read.\n", __FUNCTION__, ide->drive[drive].dev_name, MAJOR(dev), MINOR(dev));
			printk("\tstatus=0x%x ", status);
			ide_error(ide, status);
			printk("\tblock %d, sector %d.\n", block, offset);
			inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
			unlock_resource(&ide->resource);
			return -EIO;
		}

		if(cmd == ATA_READ_MULTIPLE_PIO) {
			inport_sw(ide->base + IDE_DATA, (void *)buffer, (IDE_HD_SECTSIZE * sectors_to_read) / sizeof(short int));
			break;
		}
		inport_sw(ide->base + IDE_DATA, (void *)buffer, IDE_HD_SECTSIZE / sizeof(short int));
		inport_b(ide->ctrl + IDE_ALT_STATUS);	/* ignore results */
		inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
		n++;
		offset++;
		buffer += IDE_HD_SECTSIZE;
	}
	inport_b(ide->ctrl + IDE_ALT_STATUS);	/* ignore results */
	inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
	unlock_resource(&ide->resource);
	return sectors_to_read * IDE_HD_SECTSIZE;
}

int ide_hd_write(__dev_t dev, __blk_t block, char *buffer, int blksize)
{
	int minor;
	int drive;
	int sectors_to_write, cmd, mode, lba_head;
	int n, status, r, retries;
	int cyl, head, sector;
	__off_t offset;
	struct ide *ide;
	struct ide_drv_ident *ident;
	struct partition *part;
	struct callout_req creq;

	if(!(ide = get_ide_controller(dev))) {
		return -EINVAL;
	}

	minor = MINOR(dev);
	if((drive = get_ide_drive(dev))) {
		minor &= ~(1 << IDE_SLAVE_MSF);
	}

	SET_IDE_RDY_RETR(retries);

	blksize = blksize ? blksize : BLKSIZE_1K;
	sectors_to_write = MIN(blksize, PAGE_SIZE) / IDE_HD_SECTSIZE;

	ident = &ide->drive[drive].ident;
	part = ide->drive[drive].part_table;
	offset = block2sector(block, blksize, part, minor);

	CLI();
	lock_resource(&ide->resource);

	n = 0;

	while(n < sectors_to_write) {
		if(ide->drive[drive].flags & DEVICE_HAS_RW_MULTIPLE) {
			outport_b(ide->base + IDE_SECCNT, sectors_to_write);
			cmd = ATA_WRITE_MULTIPLE_PIO;
		} else {
			outport_b(ide->base + IDE_SECCNT, 1);
			cmd = ATA_WRITE_PIO;
		}

		if(ide->drive[drive].flags & DEVICE_REQUIRES_LBA) {
			outport_b(ide->base + IDE_SECNUM, offset & 0xFF);
			outport_b(ide->base + IDE_LCYL, (offset >> 8) & 0xFF);
			outport_b(ide->base + IDE_HCYL, (offset >> 16) & 0xFF);
			mode = IDE_LBA_MODE;
			lba_head = (offset >> 24) & 0x0F;
		} else {
			sector2chs(offset, &cyl, &head, &sector, ident);
			outport_b(ide->base + IDE_SECNUM, sector);
			outport_b(ide->base + IDE_LCYL, cyl);
			outport_b(ide->base + IDE_HCYL, (cyl >> 8));
			mode = IDE_CHS_MODE;
			lba_head = head;
		}
		if(ide_drvsel(ide, drive, mode, lba_head)) {
			printk("WARNING: %s(): %s: drive not ready.\n", __FUNCTION__, ide->drive[drive].dev_name);
			unlock_resource(&ide->resource);
			return -EIO;
		}
		outport_b(ide->base + IDE_COMMAND, cmd);

		for(r = 0; r < retries; r++) {
			status = inport_b(ide->base + IDE_STATUS);
			if(!(status & IDE_STAT_BSY) && (status & IDE_STAT_DRQ)) {
				break;
			}
			ide_delay();
		}
		if(status & IDE_STAT_ERR) {
			printk("WARNING: %s(): %s: error on hard disk dev %d,%d during write.\n", __FUNCTION__, ide->drive[drive].dev_name, MAJOR(dev), MINOR(dev));
			printk("\tstatus=0x%x ", status);
			ide_error(ide, status);
			printk("\tblock %d, sector %d.\n", block, offset);
			inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
			unlock_resource(&ide->resource);
			return -EIO;
		}
		if(cmd == ATA_WRITE_MULTIPLE_PIO) {
			outport_sw(ide->base + IDE_DATA, (void *)buffer, (IDE_HD_SECTSIZE * sectors_to_write) / sizeof(short int));
		} else {
			outport_sw(ide->base + IDE_DATA, (void *)buffer, IDE_HD_SECTSIZE / sizeof(short int));
		}

		if(ide->channel == IDE_PRIMARY) {
			ide0_wait_interrupt = ide->base;
			creq.fn = ide0_timer;
			creq.arg = 0;
			add_callout(&creq, WAIT_FOR_IDE);
			if(ide0_wait_interrupt) {
				sleep(&irq_ide0, PROC_UNINTERRUPTIBLE);
			}
			if(ide0_timeout) {
				printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during write.\n", __FUNCTION__, ide->drive[drive].dev_name, MAJOR(dev), MINOR(dev));
			} else {
				del_callout(&creq);
			}
		}
		if(ide->channel == IDE_SECONDARY) {
			ide1_wait_interrupt = ide->base;
			creq.fn = ide1_timer;
			creq.arg = 0;
			add_callout(&creq, WAIT_FOR_IDE);
			if(ide1_wait_interrupt) {
				sleep(&irq_ide1, PROC_UNINTERRUPTIBLE);
			}
			if(ide1_timeout) {
				printk("WARNING: %s(): %s: timeout on hard disk dev %d,%d during write.\n", __FUNCTION__, ide->drive[drive].dev_name, MAJOR(dev), MINOR(dev));
			} else {
				del_callout(&creq);
			}
		}

		inport_b(ide->ctrl + IDE_ALT_STATUS);	/* ignore results */
		inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
		if(cmd == ATA_WRITE_MULTIPLE_PIO) {
			break;
		}
		n++;
		offset++;
		buffer += IDE_HD_SECTSIZE;
	}
	inport_b(ide->ctrl + IDE_ALT_STATUS);	/* ignore results */
	inport_b(ide->base + IDE_STATUS);	/* clear any pending interrupt */
	unlock_resource(&ide->resource);
	return sectors_to_write * IDE_HD_SECTSIZE;
}

int ide_hd_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	int minor;
	int drive;
	struct ide *ide;
	struct ide_drv_ident *ident;
	struct partition *part;
	struct hd_geometry *geom;
	int errno;

	if(!(ide = get_ide_controller(i->rdev))) {
		return -EINVAL;
	}

	minor = MINOR(i->rdev);
	drive = get_ide_drive(i->rdev);
	if(drive) {
		minor &= ~(1 << IDE_SLAVE_MSF);
	}

	ident = &ide->drive[drive].ident;
	part = ide->drive[drive].part_table;

	switch(cmd) {
		case HDIO_GETGEO:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(struct hd_geometry)))) {
				return errno;
			}
			geom = (struct hd_geometry *)arg;
	                geom->cylinders = ident->logic_cyls;
	                geom->heads = (char)ident->logic_heads;
	                geom->sectors = (char)ident->logic_spt;
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
				*(int *)arg = (unsigned int)ide->drive[drive].nr_sects;
			} else {
				*(int *)arg = (unsigned int)ide->drive[drive].part_table[minor - 1].nr_sects;
			}
			break;
		case BLKFLSBUF:
			sync_buffers(i->rdev);
			invalidate_buffers(i->rdev);
			break;
		case BLKRRPART:
			read_msdos_partition(i->rdev, part);
			assign_minors(i->rdev, ide, part);
			break;
		default:
			return -EINVAL;
			break;
	}

	return 0;
}

int ide_hd_init(struct ide *ide, int drive)
{
	int n;
	__dev_t rdev;
	struct device *d;
	struct partition *part;

	rdev = 0;
	ide->drive[drive].fsop = &ide_hd_driver_fsop;
	part = ide->drive[drive].part_table;

	if(ide->channel == IDE_PRIMARY) {
		if(drive == IDE_MASTER) {
			rdev = MKDEV(IDE0_MAJOR, drive);
			ide->drive[drive].minor_shift = IDE_MASTER_MSF;
			if(!(d = get_device(BLK_DEV, rdev))) {
				return -EINVAL;
			}
			((unsigned int *)d->device_data)[0] = ide->drive[drive].nr_sects / 2;
		} else {
			rdev = MKDEV(IDE0_MAJOR, 1 << IDE_SLAVE_MSF);
			ide->drive[drive].minor_shift = IDE_SLAVE_MSF;
			if(!(d = get_device(BLK_DEV, rdev))) {
				return -EINVAL;
			}
			((unsigned int *)d->device_data)[1 << IDE_SLAVE_MSF] = ide->drive[drive].nr_sects / 2;
		}
	} else if(ide->channel == IDE_SECONDARY) {
		if(drive == IDE_MASTER) {
			rdev = MKDEV(IDE1_MAJOR, drive);
			ide->drive[drive].minor_shift = IDE_MASTER_MSF;
			if(!(d = get_device(BLK_DEV, rdev))) {
				return -EINVAL;
			}
			((unsigned int *)d->device_data)[0] = ide->drive[drive].nr_sects / 2;
		} else {
			rdev = MKDEV(IDE1_MAJOR, 1 << IDE_SLAVE_MSF);
			ide->drive[drive].minor_shift = IDE_SLAVE_MSF;
			if(!(d = get_device(BLK_DEV, rdev))) {
				return -EINVAL;
			}
			((unsigned int *)d->device_data)[1 << IDE_SLAVE_MSF] = ide->drive[drive].nr_sects / 2;
		}
	} else {
		printk("WARNING: %s(): invalid drive number %d.\n", __FUNCTION__, drive);
		return 1;
	}

	read_msdos_partition(rdev, part);
	assign_minors(rdev, ide, part);
	printk("\t\t\t\tpartition summary: ");
	for(n = 0; n < NR_PARTITIONS; n++) {
		/* status values other than 0x00 and 0x80 are invalid */
		if(part[n].status && part[n].status != 0x80) {
			continue;
		}
		if(part[n].type) {
			printk("%s%d ", ide->drive[drive].dev_name, n + 1);
		}
	}
	printk("\n");

	outport_b(ide->ctrl + IDE_DEV_CTRL, IDE_DEVCTR_NIEN);
	if(ide->drive[drive].flags & DEVICE_HAS_RW_MULTIPLE) {
		/* read/write in 4KB blocks (8 sectors) by default */
		outport_b(ide->base + IDE_SECCNT, PAGE_SIZE / IDE_HD_SECTSIZE);
		outport_b(ide->base + IDE_COMMAND, ATA_SET_MULTIPLE_MODE);
		ide_wait400ns(ide);
		while(inport_b(ide->base + IDE_STATUS) & IDE_STAT_BSY);
	}
	outport_b(ide->ctrl + IDE_DEV_CTRL, IDE_DEVCTR_DRQ);

	return 0;
}
