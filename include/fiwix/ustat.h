/*
 * fiwix/include/fiwix/ustat.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_USTAT_H
#define _FIWIX_USTAT_H

#include <fiwix/types.h>

struct ustat {
	__daddr_t f_tfree;	/* total free blocks */
	__ino_t f_tinode;	/* number of free inodes */
	char f_fname;		/* filesystem name */
	char f_fpack;		/* filesystem pack name */
};

#endif /* _FIWIX_USTAT_H */
