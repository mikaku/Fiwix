/*
 * fiwix/include/fiwix/fs.h
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FS_H
#define _FIWIX_FS_H

#include <fiwix/statfs.h>
#include <fiwix/limits.h>
#include <fiwix/process.h>
#include <fiwix/dirent.h>
#include <fiwix/fd.h>
#include <fiwix/fs_minix.h>
#include <fiwix/fs_ext2.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/fs_proc.h>
#include <fiwix/fs_sock.h>

#define BPS			512	/* bytes per sector */
#define BLKSIZE_1K		1024	/* 1KB block size */
#define BLKSIZE_2K		2048	/* 2KB block size */
#define SUPERBLOCK		1	/* block 1 is for superblock */

#define MAJOR(dev)		(((__dev_t) (dev)) >> 8)
#define MINOR(dev)		(((__dev_t) (dev)) & 0xFF)
#define MKDEV(major, minor)	(((major) << 8) | (minor))

/* filesystem independent mount-flags */
#define MS_RDONLY		0x01	/* mount read-only */
#define MS_REMOUNT		0x20	/* alter flags of a mounted FS */

/* old magic mount flag and mask */
#define MS_MGC_VAL		0xC0ED0000
#define MS_MGC_MSK		0xFFFF0000

#define IS_RDONLY_FS(inode) (((inode)->sb) && ((inode)->sb->flags & MS_RDONLY))

#define FOLLOW_LINKS	1
#define MAX_SYMLINKS	8	/* this prevents infinite loops in symlinks */

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#define FOR_READING	0
#define FOR_WRITING	1

#define VERIFY_READ	1
#define VERIFY_WRITE	2

#define SEL_R		1
#define SEL_W		2
#define SEL_E		4

#define CLEAR_BIT	0
#define SET_BIT		1

#define INODE_LOCKED	0x01
#define INODE_DIRTY	0x02

struct inode {
	__mode_t	i_mode;		/* file mode */
	__u32		i_uid;		/* owner uid */
	__size_t	i_size;		/* size in bytes */
	__u32		i_atime;	/* access time */
	__u32		i_ctime;	/* creation time */
	__u32		i_mtime;	/* modification time */
	__u32		i_gid;		/* group id */
	__nlink_t	i_nlink;	/* links count */
	__blk_t		i_blocks;	/* blocks count */
	__u32		i_flags;	/* file flags */
	struct inode *mount_point;
	__u32		state;
	__dev_t		dev;
	__ino_t		inode;
	__s16		count;
	__dev_t		rdev;
	struct fs_operations *fsop;
	struct superblock *sb;
	struct inode *prev;
	struct inode *next;
	struct inode *prev_hash;
	struct inode *next_hash;
	struct inode *prev_free;
	struct inode *next_free;
	union {
#ifdef CONFIG_FS_MINIX
		struct minix_i_info minix;
#endif /* CONFIG_FS_MINIX */
		struct ext2_i_info ext2;
		struct pipefs_inode pipefs;
		struct iso9660_inode iso9660;
		struct procfs_inode procfs;
#ifdef CONFIG_NET
		struct sockfs_inode sockfs;
#endif /* CONFIG_NET */
	} u;
};
extern struct inode *inode_table;
extern struct inode **inode_hash_table;

/* values to be determined during system startup */
extern unsigned int inode_hash_table_size;	/* size in bytes */

#define SUPERBLOCK_LOCKED	0x01
#define SUPERBLOCK_DIRTY	0x02

struct superblock {
	__dev_t dev;
	unsigned int flags;
	unsigned int state;
	struct inode *root;		/* root inode of mounted fs */
	struct inode *dir;		/* inode on which the fs was mounted */
	struct fs_operations *fsop;
	__u32 s_blocksize;
	unsigned char s_blocksize_bits;
	union {
#ifdef CONFIG_FS_MINIX
		struct minix_sb_info minix;
#endif /* CONFIG_FS_MINIX */
		struct ext2_sb_info ext2;
		struct iso9660_sb_info iso9660;
	} u;
};


#define FSOP_REQUIRES_DEV	1	/* requires a block device */
#define FSOP_KERN_MOUNT		2	/* mounted by kernel */

struct fs_operations {
	int flags;
	int fsdev;			/* internal filesystem (nodev) */

/* file operations */
	int (*open)(struct inode *, struct fd *);
	int (*close)(struct inode *, struct fd *);
	int (*read)(struct inode *, struct fd *, char *, __size_t);
	int (*write)(struct inode *, struct fd *, const char *, __size_t);
	int (*ioctl)(struct inode *, int, unsigned int);
	__loff_t (*llseek)(struct inode *, __loff_t);
	int (*readdir)(struct inode *, struct fd *, struct dirent *, __size_t);
	int (*readdir64)(struct inode *, struct fd *, struct dirent64 *, __size_t);
	int (*mmap)(struct inode *, struct vma *);
	int (*select)(struct inode *, int);

/* inode operations */
	int (*readlink)(struct inode *, char *, __size_t);
	int (*followlink)(struct inode *, struct inode *, struct inode **);
	int (*bmap)(struct inode *, __off_t, int);
	int (*lookup)(const char *, struct inode *, struct inode **);
	int (*rmdir)(struct inode *, struct inode *);
	int (*link)(struct inode *, struct inode *, char *);
	int (*unlink)(struct inode *, struct inode *, char *);
	int (*symlink)(struct inode *, char *, char *);
	int (*mkdir)(struct inode *, char *, __mode_t);
	int (*mknod)(struct inode *, char *, __mode_t, __dev_t);
	int (*truncate)(struct inode *, __off_t);
	int (*create)(struct inode *, char *, int, __mode_t, struct inode **);
	int (*rename)(struct inode *, struct inode *, struct inode *, struct inode *, char *, char *);

/* block device I/O operations */
	int (*read_block)(__dev_t, __blk_t, char *, int);
	int (*write_block)(__dev_t, __blk_t, char *, int);

/* superblock operations */
	int (*read_inode)(struct inode *);
	int (*write_inode)(struct inode *);
	int (*ialloc)(struct inode *, int);
	void (*ifree)(struct inode *);
	void (*statfs)(struct superblock *, struct statfs *);
	int (*read_superblock)(__dev_t, struct superblock *);
	int (*remount_fs)(struct superblock *, int);
	int (*write_superblock)(struct superblock *);
	void (*release_superblock)(struct superblock *);
};

extern struct fs_operations def_chr_fsop;
extern struct fs_operations def_blk_fsop;

/* fs_minix.h prototypes */
extern struct fs_operations minix_fsop;
extern struct fs_operations minix_file_fsop;
extern struct fs_operations minix_dir_fsop;
extern struct fs_operations minix_symlink_fsop;
extern int minix_count_free_inodes(struct superblock *);
extern int minix_count_free_blocks(struct superblock *);
extern int minix_find_first_zero(struct superblock *, __blk_t, int, int);
extern int minix_change_bit(int, struct superblock *, int, int);
extern int minix_balloc(struct superblock *);
extern void minix_bfree(struct superblock *, int);

/* fs_ext2.h prototypes */
extern struct fs_operations ext2_fsop;
extern struct fs_operations ext2_file_fsop;
extern struct fs_operations ext2_dir_fsop;
extern struct fs_operations ext2_symlink_fsop;
extern int ext2_balloc(struct superblock *);
extern void ext2_bfree(struct superblock *, int);

/* fs_proc.h prototypes */
extern struct fs_operations procfs_fsop;
extern struct fs_operations procfs_file_fsop;
extern struct fs_operations procfs_dir_fsop;
extern struct fs_operations procfs_symlink_fsop;
struct procfs_dir_entry *get_procfs_by_inode(struct inode *);

/* fs_iso9660.h prototypes */
extern int isonum_711(char *);
extern int isonum_723(char *);
extern int isonum_731(char *);
extern int isonum_733(char *);
extern unsigned int isodate(const char *);
extern int iso9660_cleanfilename(char *, int);
extern struct fs_operations iso9660_fsop;
extern struct fs_operations iso9660_file_fsop;
extern struct fs_operations iso9660_dir_fsop;
extern struct fs_operations iso9660_symlink_fsop;
void check_rrip_inode(struct iso9660_directory_record *, struct inode *);
int get_rrip_filename(struct iso9660_directory_record *, struct inode *, char *);
int get_rrip_symlink(struct inode *, char *);


/* generic VFS function prototypes */
void inode_lock(struct inode *);
void inode_unlock(struct inode *);
struct inode *ialloc(struct superblock *, int);
struct inode *iget(struct superblock *, __ino_t);
int bmap(struct inode *, __off_t, int);
int check_fs_busy(__dev_t, struct inode *);
void iput(struct inode *);
void sync_inodes(__dev_t);
void invalidate_inodes(__dev_t);
void inode_init(void);

int parse_namei(char *, struct inode *, struct inode **, struct inode **, int);
int namei(char *, struct inode **, struct inode **, int);

void superblock_lock(struct superblock *);
void superblock_unlock(struct superblock *);
struct mount *add_mount_point(__dev_t, const char *, const char *);
void del_mount_point(struct mount *);
struct mount *get_mount_point(struct inode *);

int get_new_fd(struct inode *);
void release_fd(unsigned int);
int get_new_user_fd(int);
void release_user_fd(int);
void fd_init(void);

void free_name(const char *);
int malloc_name(const char *, char **);
int check_user_permission(struct inode *);
int check_group(struct inode *);
int check_user_area(int, const void *, unsigned int);
int check_permission(int, struct inode *);

int do_mknod(char *, __mode_t, __dev_t);
int do_select(int, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *);

#endif /* _FIWIX_FS_H */
