/*
 * fiwix/include/fiwix/statbuf.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_STATBUF_H
#define _FIWIX_STATBUF_H

struct old_stat {
	__dev_t st_dev;
	unsigned short int st_ino;
	__mode_t st_mode;
	__nlink_t st_nlink;
	__uid_t st_uid;
	__gid_t st_gid;
	__dev_t st_rdev;
	unsigned int st_size;
	__time_t st_atime;
	__time_t st_mtime;
	__time_t st_ctime;
};

struct new_stat {
	__dev_t st_dev;
	unsigned short int __pad1;
	__ino_t st_ino;
	__mode_t st_mode;
	__nlink_t st_nlink;
	__uid_t st_uid;
	__gid_t st_gid;
	__dev_t st_rdev;
	unsigned short int __pad2;
	__off_t st_size;
	__blk_t st_blksize;
	__blk_t st_blocks;
	__time_t st_atime;
	unsigned int __unused1;
	__time_t st_mtime;
	unsigned int __unused2;
	__time_t st_ctime;
	unsigned int __unused3;
	unsigned int __unused4;
	unsigned int __unused5;
};

struct stat64 {
        unsigned long long st_dev;
        int __st_dev_padding;
        long __st_ino_truncated;
        unsigned st_mode;
        unsigned int st_nlink;
        unsigned st_uid;
        unsigned st_gid;
        unsigned long long st_rdev;
        int __st_rdev_padding;
        long long st_size;
        long st_blksize;
        long long st_blocks;
        long st_atime;
        long st_atime_nsec;
        long st_mtime;
        long st_mtime_nsec;
        long st_ctime;
        long st_ctime_nsec;
        unsigned long long st_ino;
};

#endif /* _FIWIX_STATBUF_H */
