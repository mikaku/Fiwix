/*
 * fiwix/include/fiwix/statfs.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_STATFS_H
#define _FIWIX_STATFS_H

typedef struct {
	long int val[2];
} fsid_t;

struct statfs {
	long int f_type;
	long int f_bsize;
	long int f_blocks;
	long int f_bfree;
	long int f_bavail;
	long int f_files;
	long int f_ffree;
	fsid_t f_fsid;
	long int f_namelen;
	long int f_spare[6];
};

#endif /* _FIWIX_STATFS_H */
