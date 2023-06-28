/*
 * fiwix/include/fiwix/statfs.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_STATFS_H
#define _FIWIX_STATFS_H

typedef struct {
	unsigned int val[2];
} fsid_t;

struct statfs {
	unsigned int f_type;
	unsigned int f_bsize;
	unsigned int f_blocks;
	unsigned int f_bfree;
	unsigned int f_bavail;
	unsigned int f_files;
	unsigned int f_ffree;
	fsid_t f_fsid;
	unsigned int f_namelen;
	unsigned int f_spare[6];
};

#endif /* _FIWIX_STATFS_H */
