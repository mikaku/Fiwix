/*
 * fiwix/include/fiwix/ide_cd.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_IDE_CD_H
#define _FIWIX_IDE_CD_H

#include <fiwix/fs.h>

#define IDE_CD_SECTSIZE		BLKSIZE_2K	/* sector size (in bytes) */

void ide_cd_timer(unsigned int);

int ide_cd_open(struct inode *, struct fd *);
int ide_cd_close(struct inode *, struct fd *);
int ide_cd_read(__dev_t, __blk_t, char *, int);
int ide_cd_ioctl(struct inode *, int, unsigned long int);

int ide_cd_init(struct ide *, struct ide_drv *);

#endif /* _FIWIX_IDE_CD_H */
