/*
 * fiwix/include/fiwix/atapi_cd.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ATAPI_CD_H
#define _FIWIX_ATAPI_CD_H

#include <fiwix/fs.h>

#define ATA_CD_SECTSIZE		BLKSIZE_2K	/* sector size (in bytes) */

int ata_cd_open(struct inode *, struct fd *);
int ata_cd_close(struct inode *, struct fd *);
int ata_cd_read(__dev_t, __blk_t, char *, int);
int ata_cd_ioctl(struct inode *, int, unsigned long int);
int ata_cd_init(struct ide *, struct ata_drv *);

#endif /* _FIWIX_ATAPI_CD_H */
