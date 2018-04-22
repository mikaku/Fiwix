/*
 * fiwix/include/fiwix/ide_hd.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_IDE_HD_H
#define _FIWIX_IDE_HD_H

#include <fiwix/types.h>

#define IDE_HD_SECTSIZE		512	/* sector size (in bytes) */

int ide_hd_open(struct inode *, struct fd *);
int ide_hd_close(struct inode *, struct fd *);
int ide_hd_read(__dev_t, __blk_t, char *, int);
int ide_hd_write(__dev_t, __blk_t, char *, int);
int ide_hd_ioctl(struct inode *, int, unsigned long int);

int ide_hd_init(struct ide *, int);

#endif /* _FIWIX_IDE_HD_H */
