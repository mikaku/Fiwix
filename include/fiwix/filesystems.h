/*
 * fiwix/include/fiwix/filesystems.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FILESYSTEMS_H
#define _FIWIX_FILESYSTEMS_H

#include <fiwix/types.h>
#include <fiwix/limits.h>

#define NR_FILESYSTEMS		6	/* supported filesystems */

/* special device numbers for nodev filesystems */
enum {
	FS_NODEV = 0xFFF0,
	DEVPTS_DEV,
	PIPE_DEV,
	PROC_DEV,
	SOCK_DEV,
};

struct filesystems {
	const char *name;		/* filesystem name */
	struct fs_operations *fsop;	/* filesystem operations */
	struct mount *mp;		/* mount-table entry (only for nodev) */
};
extern struct filesystems filesystems_table[NR_FILESYSTEMS];

struct mount {
	__dev_t dev;			/* device number */
	char *devname;			/* device name */
	char *dirname;			/* mount point directory name */
	struct superblock sb;		/* superblock */
	struct filesystems *fs;		/* pointer to filesystem structure */
	struct mount *prev;
	struct mount *next;
};
extern struct mount *mount_table;

int register_filesystem(const char *, struct fs_operations *);
struct filesystems *get_filesystem(const char *);
void fs_init(void);

struct superblock *get_superblock(__dev_t);
void sync_superblocks(__dev_t);
int kern_mount(__dev_t, struct filesystems *);
int mount_root(void);


/* minix prototypes */
int minix_file_open(struct inode *, struct fd *);
int minix_file_close(struct inode *, struct fd *);
int minix_file_write(struct inode *, struct fd *, const char *, __size_t);
__loff_t minix_file_llseek(struct inode *, __loff_t);
int minix_dir_open(struct inode *, struct fd *);
int minix_dir_close(struct inode *, struct fd *);
int minix_dir_read(struct inode *, struct fd *, char *, __size_t);
int minix_readdir(struct inode *, struct fd *, struct dirent *, __size_t);
int minix_readlink(struct inode *, char *, __size_t);
int minix_followlink(struct inode *, struct inode *, struct inode **);
int minix_bmap(struct inode *, __off_t, int);
int minix_lookup(const char *, struct inode *, struct inode **);
int minix_rmdir(struct inode *, struct inode *);
int minix_link(struct inode *, struct inode *, char *);
int minix_unlink(struct inode *, struct inode *, char *);
int minix_symlink(struct inode *, char *, char *);
int minix_mkdir(struct inode *, char *, __mode_t);
int minix_mknod(struct inode *, char *, __mode_t, __dev_t);
int minix_truncate(struct inode *, __off_t);
int minix_create(struct inode *, char *, int, __mode_t, struct inode **);
int minix_rename(struct inode *, struct inode *, struct inode *, struct inode *, char *, char *);
int minix_read_inode(struct inode *);
int minix_write_inode(struct inode *);
int minix_ialloc(struct inode *, int);
void minix_ifree(struct inode *);
void minix_statfs(struct superblock *, struct statfs *);
int minix_read_superblock(__dev_t, struct superblock *);
int minix_remount_fs(struct superblock *, int);
int minix_write_superblock(struct superblock *);
void minix_release_superblock(struct superblock *);
int minix_init(void);

/* ext2 prototypes */
int ext2_file_open(struct inode *, struct fd *);
int ext2_file_close(struct inode *, struct fd *);
int ext2_file_write(struct inode *, struct fd *, const char *, __size_t);
__loff_t ext2_file_llseek(struct inode *, __loff_t);
int ext2_dir_open(struct inode *, struct fd *);
int ext2_dir_close(struct inode *, struct fd *);
int ext2_dir_read(struct inode *, struct fd *, char *, __size_t);
int ext2_readdir(struct inode *, struct fd *, struct dirent *, __size_t);
int ext2_readdir64(struct inode *, struct fd *, struct dirent64 *, __size_t);
int ext2_readlink(struct inode *, char *, __size_t);
int ext2_followlink(struct inode *, struct inode *, struct inode **);
int ext2_bmap(struct inode *, __off_t, int);
int ext2_lookup(const char *, struct inode *, struct inode **);
int ext2_rmdir(struct inode *, struct inode *);
int ext2_link(struct inode *, struct inode *, char *);
int ext2_unlink(struct inode *, struct inode *, char *);
int ext2_symlink(struct inode *, char *, char *);
int ext2_mkdir(struct inode *, char *, __mode_t);
int ext2_mknod(struct inode *, char *, __mode_t, __dev_t);
int ext2_truncate(struct inode *, __off_t);
int ext2_create(struct inode *, char *, int, __mode_t, struct inode **);
int ext2_rename(struct inode *, struct inode *, struct inode *, struct inode *, char *, char *);
int ext2_read_inode(struct inode *);
int ext2_write_inode(struct inode *);
int ext2_ialloc(struct inode *, int);
void ext2_ifree(struct inode *);
void ext2_statfs(struct superblock *, struct statfs *);
int ext2_read_superblock(__dev_t, struct superblock *);
int ext2_remount_fs(struct superblock *, int);
int ext2_write_superblock(struct superblock *);
void ext2_release_superblock(struct superblock *);
int ext2_init(void);

/* pipefs prototypes */
int fifo_open(struct inode *, struct fd *);
int pipefs_close(struct inode *, struct fd *);
int pipefs_read(struct inode *, struct fd *, char *, __size_t);
int pipefs_write(struct inode *, struct fd *, const char *, __size_t);
int pipefs_ioctl(struct inode *, struct fd *, int, unsigned int);
__loff_t pipefs_llseek(struct inode *, __loff_t);
int pipefs_select(struct inode *, struct fd *, int);
int pipefs_ialloc(struct inode *, int);
void pipefs_ifree(struct inode *);
int pipefs_read_superblock(__dev_t, struct superblock *);
int pipefs_init(void);

/* iso9660 prototypes */
int iso9660_file_open(struct inode *, struct fd *);
int iso9660_file_close(struct inode *, struct fd *);
__loff_t iso9660_file_llseek(struct inode *, __loff_t);
int iso9660_dir_open(struct inode *, struct fd *);
int iso9660_dir_close(struct inode *, struct fd *);
int iso9660_dir_read(struct inode *, struct fd *, char *, __size_t);
int iso9660_readdir(struct inode *, struct fd *, struct dirent *, __size_t);
int iso9660_readlink(struct inode *, char *, __size_t);
int iso9660_followlink(struct inode *, struct inode *, struct inode **);
int iso9660_bmap(struct inode *, __off_t, int);
int iso9660_lookup(const char *, struct inode *, struct inode **);
int iso9660_read_inode(struct inode *);
void iso9660_statfs(struct superblock *, struct statfs *);
int iso9660_read_superblock(__dev_t, struct superblock *);
void iso9660_release_superblock(struct superblock *);
int iso9660_init(void);

/* procfs prototypes */
int procfs_file_open(struct inode *, struct fd *);
int procfs_file_close(struct inode *, struct fd *);
int procfs_file_read(struct inode *, struct fd *, char *, __size_t);
__loff_t procfs_file_llseek(struct inode *, __loff_t);
int procfs_dir_open(struct inode *, struct fd *);
int procfs_dir_close(struct inode *, struct fd *);
int procfs_dir_read(struct inode *, struct fd *, char *, __size_t);
int procfs_readdir(struct inode *, struct fd *, struct dirent *, __size_t);
int procfs_readlink(struct inode *, char *, __size_t);
int procfs_followlink(struct inode *, struct inode *, struct inode **);
int procfs_bmap(struct inode *, __off_t, int);
int procfs_lookup(const char *, struct inode *, struct inode **);
int procfs_read_inode(struct inode *);
void procfs_statfs(struct superblock *, struct statfs *);
int procfs_read_superblock(__dev_t, struct superblock *);
int procfs_init(void);

#ifdef CONFIG_NET
/* sockfs prototypes */
int sockfs_open(struct inode *, struct fd *);
int sockfs_close(struct inode *, struct fd *);
int sockfs_read(struct inode *, struct fd *, char *, __size_t);
int sockfs_write(struct inode *, struct fd *, const char *, __size_t);
int sockfs_ioctl(struct inode *, struct fd *, int, unsigned int);
__loff_t sockfs_llseek(struct inode *, __loff_t);
int sockfs_select(struct inode *, struct fd *, int);
int sockfs_ialloc(struct inode *, int);
void sockfs_ifree(struct inode *);
int sockfs_read_superblock(__dev_t, struct superblock *);
int sockfs_init(void);
#endif /* CONFIG_NET */

#ifdef CONFIG_UNIX98_PTYS
/* devpts prototypes */
int devpts_dir_open(struct inode *, struct fd *);
int devpts_dir_close(struct inode *, struct fd *);
int devpts_dir_read(struct inode *, struct fd *, char *, __size_t);
int devpts_readdir(struct inode *, struct fd *, struct dirent *, __size_t);
int devpts_lookup(const char *, struct inode *, struct inode **);
int devpts_read_inode(struct inode *);
void devpts_statfs(struct superblock *, struct statfs *);
int devpts_ialloc(struct inode *, int);
void devpts_ifree(struct inode *);
int devpts_read_superblock(__dev_t, struct superblock *);
int devpts_init(void);
#endif /* CONFIG_UNIX98_PTYS */

#endif /* _FIWIX_FILESYSTEMS_H */
