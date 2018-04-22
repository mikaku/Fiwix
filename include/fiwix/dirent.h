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

#endif /* _FIWIX_DIRENT_H */
