/*
 * fiwix/drivers/block/part.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/ata.h>
#include <fiwix/ata_hd.h>
#include <fiwix/fs.h>
#include <fiwix/part.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int read_msdos_partition(__dev_t dev, struct partition *part)
{
	char *buffer;

	if(!(buffer = (void *)kmalloc(BLKSIZE_1K))) {
		return -ENOMEM;
	}

	if(ata_hd_read(dev, PARTITION_BLOCK, buffer, BLKSIZE_1K) <= 0) {
		printk("WARNING: %s(): unable to read partition block in device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		kfree((unsigned int)buffer);
		return -EIO;
	}

	memcpy_b(part, (void *)(buffer + MBR_CODE_SIZE), sizeof(struct partition) * NR_PARTITIONS);
	kfree((unsigned int)buffer);
	return 0;
}
