/*
 * fiwix/include/fiwix/fs_minix.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_FS_MINIX

#ifndef _FIWIX_FS_MINIX_H
#define _FIWIX_FS_MINIX_H

#include <fiwix/types.h>
#include <fiwix/limits.h>

#define MINIX_ROOT_INO		1	/* root inode */

#define MINIX_SUPER_MAGIC	0x137F	/* Minix v1, 14 char names */
#define MINIX_SUPER_MAGIC2	0x138F	/* Minix v1, 30 char names */
#define MINIX2_SUPER_MAGIC	0x2468	/* Minix v2, 14 char names */
#define MINIX2_SUPER_MAGIC2	0x2478	/* Minix v2, 30 char names */

#define MINIX_VALID_FS		1	/* clean filesystem */
#define MINIX_ERROR_FS		2	/* needs fsck */

#define V1_MAX_BITMAP_BLOCKS	8	/* 64MB filesystem size */
#define V2_MAX_BITMAP_BLOCKS	128	/* 1GB filesystem size */

/*
 * Minix (v1 and v2) file system physical layout:
 *
 * 		 +-----------------------------------------------
 * 		 |   size in blocks of BLKSIZE_1K (1024 bytes)	|
 * +-------------+----------------------------------------------+
 * | block 0     |					      1 |
 * +-------------+----------------------------------------------+
 * | superblock  |					      1 |
 * +-------------+----------------------------------------------+
 * | inode map   |	    number of inodes / (BLKSIZE_1K * 8) | 
 * +-------------+----------------------------------------------+
 * | zone map    |	     number of zones / (BLKSIZE_1K * 8) | 
 * +-------------+----------------------------------------------+
 * | inode table | ((32 or 64) * number of inodes) / BLKSIZE_1K |
 * +-------------+----------------------------------------------+
 * | data zones  |					    ... | 
 * +-------------+----------------------------------------------+
 *
 * The implementation of this filesystem in Fiwix might have slow disk writes
 * because I don't keep in memory the superblock, nor the blocks of the inode
 * map nor the blocks of the zone map. Keeping them in memory would be a waste
 * of 137KB per each mounted v2 filesystem (1GB of size).
 *
 * - superblock    ->   1KB
 * - inode map     ->   8KB (1KB (8192 bits) x   8 = 65536 inodes)
 * - zone map      -> 128KB (1KB (8192 bits) x 128 = 1048576 1k-blocks)
 *
 */

struct minix_super_block {
	__u16 s_ninodes;		/* number of inodes */
	__u16 s_nzones;			/* number of data zones */
	__u16 s_imap_blocks;		/* blocks used by inode bitmap */
	__u16 s_zmap_blocks;		/* blocks used by zone bitmap */
	__u16 s_firstdatazone;		/* number of first data zone */
	__u16 s_log_zone_size;		/* 1024 << s_log_zone_size */
	__u32 s_max_size;		/* maximum file size (in bytes) */
	__u16 s_magic;			/* magic number */
	__u16 s_state;			/* filesystem state */
	__u32 s_zones;			/* number of data zones (for v2 only) */
};

struct minix_inode {
	__u16 i_mode;
	__u16 i_uid;
	__u32 i_size;
	__u32 i_time;
	__u8  i_gid;
	__u8  i_nlinks;
	__u16 i_zone[9];
};

struct minix2_inode {
	__u16 i_mode;
	__u16 i_nlink;
	__u16 i_uid;
	__u16 i_gid;
	__u32 i_size;
	__u32 i_atime;
	__u32 i_mtime;
	__u32 i_ctime;
	__u32 i_zone[10];
};

struct minix_dir_entry {
	__u16 inode;
	char name[0];
};

/* super block in memory */
struct minix_sb_info {
	unsigned char namelen;
	unsigned char dirsize;
	unsigned short int version;
	unsigned int nzones;
	struct minix_super_block sb;
};

/* inode in memory */
struct minix_i_info {
	union {
		__u16 i1_zone[9];
		__u32 i2_zone[10];
	} u;
};

int v1_minix_read_inode(struct inode *);
int v1_minix_write_inode(struct inode *);
int v1_minix_ialloc(struct inode *, int);
void v1_minix_ifree(struct inode *);
int v1_minix_bmap(struct inode *, __off_t, int);
int v1_minix_truncate(struct inode *, __off_t);

int v2_minix_read_inode(struct inode *);
int v2_minix_write_inode(struct inode *);
int v2_minix_ialloc(struct inode *, int);
void v2_minix_ifree(struct inode *);
int v2_minix_bmap(struct inode *, __off_t, int);
int v2_minix_truncate(struct inode *, __off_t);

#endif /* _FIWIX_FS_MINIX_H */

#endif /* CONFIG_FS_MINIX */
