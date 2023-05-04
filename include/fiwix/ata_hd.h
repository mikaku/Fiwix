/*
 * fiwix/include/fiwix/ata_hd.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ATA_HD_H
#define _FIWIX_ATA_HD_H

#include <fiwix/types.h>

#define ATA_HD_SECTSIZE		512	/* sector size (in bytes) */

int ata_hd_open(struct inode *, struct fd *);
int ata_hd_close(struct inode *, struct fd *);
int ata_hd_read(__dev_t, __blk_t, char *, int);
int ata_hd_write(__dev_t, __blk_t, char *, int);
int ata_hd_ioctl(struct inode *, int, unsigned long int);
int ata_hd_lseek(struct inode *, __off_t);
int ata_hd_init(struct ide *, struct ata_drv *);

#endif /* _FIWIX_ATA_HD_H */
