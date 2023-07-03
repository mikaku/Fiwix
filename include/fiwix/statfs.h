/*
 * fiwix/include/fiwix/statfs.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_STATFS_H
#define _FIWIX_STATFS_H

typedef struct {
	int val[2];
} fsid_t;

struct statfs {
	int f_type;
	int f_bsize;
	int f_blocks;
	int f_bfree;
	int f_bavail;
	int f_files;
	int f_ffree;
	fsid_t f_fsid;
	int f_namelen;
	int f_spare[6];
};

#endif /* _FIWIX_STATFS_H */
