/*
 * fiwix/include/fiwix/types.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_TYPES_H
#define _FIWIX_TYPES_H

typedef __signed__ char __s8;
typedef unsigned char __u8;
typedef __signed__ short int __s16;
typedef unsigned short int __u16;
typedef __signed__ int __s32;
typedef unsigned int __u32;
typedef __signed__ long long int __s64;
typedef unsigned long long int __u64;

typedef __u16 __uid_t;
typedef __u16 __gid_t;
typedef __u32 __ino_t;
typedef __u16 __mode_t;
typedef __u16 __nlink_t;
typedef __u32 __off_t;
typedef __s32 __pid_t;
typedef __s32 __ssize_t;
typedef __u32 __size_t;
typedef __u32 __clock_t;
typedef __u32 __time_t;
typedef __u16 __dev_t;
typedef __u16 __key_t;
typedef __s32 __blk_t;		/* must be signed in order to return error */
typedef __s32 __daddr_t;
typedef __s64 __loff_t;		/* must be signed in order to return error */

/* number of descriptors that can fit in an 'fd_set' */
/* WARNING: this value must be the same as in the C Library */
#define __FD_SETSIZE	64

#define __NFDBITS	(sizeof(unsigned int) * 8)
#define __FDELT(d)	((d) / __NFDBITS)
#define __FDMASK(d)	(1 << ((d) % __NFDBITS))

/* define the fd_set structure for select() */
typedef struct {
	unsigned int fds_bits[__FD_SETSIZE / __NFDBITS];
} fd_set;

/* define the iovec structure for readv/writev */
struct iovec {
	void  *iov_base;
	__size_t iov_len;
};

#define __FD_ZERO(set)		(memset_b((void *) (set), 0, sizeof(fd_set)))
#define __FD_SET(d, set)	((set)->fds_bits[__FDELT(d)] |= __FDMASK(d))
#define __FD_CLR(d, set)	((set)->fds_bits[__FDELT(d)] &= ~__FDMASK(d))
#define __FD_ISSET(d, set)	((set)->fds_bits[__FDELT(d)] & __FDMASK(d))

#endif /* _FIWIX_TYPES_H */
