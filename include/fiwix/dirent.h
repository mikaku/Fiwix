/*
 * fiwix/include/fiwix/dirent.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_DIRENT_H
#define _FIWIX_DIRENT_H

#include <fiwix/types.h>
#include <fiwix/limits.h>

struct dirent {
	__ino_t d_ino;			/* inode number */
	__off_t d_off;			/* offset to next dirent */
	unsigned short int d_reclen;	/* length of this dirent */
	char d_name[NAME_MAX + 1];	/* file name (null-terminated) */
};

struct dirent64 {
	__ino64_t d_ino;		/* inode number */
	__loff_t d_off;			/* offset to next dirent */
	unsigned short d_reclen;	/* length of this dirent */
	unsigned char d_type;		/* file type */
	char d_name[];			/* file name (null-terminated) */
};

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

#endif /* _FIWIX_DIRENT_H */
