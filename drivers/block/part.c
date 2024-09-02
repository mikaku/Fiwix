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
#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int read_msdos_partition(__dev_t dev, struct partition *part)
{
	struct buffer *buf;
	struct device *d;
	int blksize;

	if(!(d = get_device(BLK_DEV, dev))) {
		return -ENXIO;
	}

	blksize = ((unsigned int *)d->blksize)[MINOR(dev)];
	if(!(buf = bread(dev, PARTITION_BLOCK, blksize))) {
		printk("WARNING: %s(): unable to read partition block in device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		return -EIO;
	}

	memcpy_b(part, (void *)(buf->data + MBR_CODE_SIZE), sizeof(struct partition) * NR_PARTITIONS);
	brelse(buf);
	return 0;
}
