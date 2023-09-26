/*
 * fiwix/fs/minix/super.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_minix.h>
#include <fiwix/buffer.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_FS_MINIX
struct fs_operations minix_fsop = {
	FSOP_REQUIRES_DEV,
	0,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lookup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	minix_read_inode,
	minix_write_inode,
	minix_ialloc,
	minix_ifree,
	minix_statfs,
	minix_read_superblock,
	minix_remount_fs,
	minix_write_superblock,
	minix_release_superblock
};

static void check_superblock(struct minix_super_block *sb)
{
	if(!(sb->s_state & MINIX_VALID_FS)) {
		printk("WARNING: filesystem not checked, fsck recommended.\n");
	}
	if(sb->s_state & MINIX_ERROR_FS) {
		printk("WARNING: filesystem contains errors, fsck recommended.\n");
	}
}

void minix_statfs(struct superblock *sb, struct statfs *statfsbuf)
{
	statfsbuf->f_type = sb->u.minix.sb.s_magic;
	statfsbuf->f_bsize = sb->s_blocksize;
	statfsbuf->f_blocks = sb->u.minix.nzones << sb->u.minix.sb.s_log_zone_size;
	statfsbuf->f_bfree = sb->u.minix.nzones - minix_count_free_blocks(sb);
	statfsbuf->f_bavail = statfsbuf->f_bfree;

	statfsbuf->f_files = sb->u.minix.sb.s_ninodes;
	statfsbuf->f_ffree = sb->u.minix.sb.s_ninodes - minix_count_free_inodes(sb);
	/* statfsbuf->f_fsid = ? */
	statfsbuf->f_namelen = sb->u.minix.namelen;
}

int minix_read_superblock(__dev_t dev, struct superblock *sb)
{
	struct buffer *buf;
	int maps;

	superblock_lock(sb);
	if(!(buf = bread(dev, SUPERBLOCK, BLKSIZE_1K))) {
		printk("WARNING: %s(): I/O error on device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		superblock_unlock(sb);
		return -EIO;
	}
	memcpy_b(&sb->u.minix.sb, buf->data, sizeof(struct minix_super_block));

	switch(sb->u.minix.sb.s_magic) {
		case MINIX_SUPER_MAGIC:
			sb->u.minix.namelen = 14;
			sb->u.minix.dirsize = sizeof(__u16) + sb->u.minix.namelen;
			sb->u.minix.version = 1;
			sb->u.minix.nzones = sb->u.minix.sb.s_nzones;
			printk("minix v1 (14 char names) filesystem detected on device %d,%d.\n", MAJOR(dev), MINOR(dev));
			break;
		case MINIX_SUPER_MAGIC2:
			sb->u.minix.namelen = 30;
			sb->u.minix.dirsize = sizeof(__u16) + sb->u.minix.namelen;
			sb->u.minix.version = 1;
			sb->u.minix.nzones = sb->u.minix.sb.s_nzones;
			printk("minix v1 (30 char names) filesystem detected on device %d,%d.\n", MAJOR(dev), MINOR(dev));
			break;
		case MINIX2_SUPER_MAGIC:
			sb->u.minix.namelen = 14;
			sb->u.minix.dirsize = sizeof(__u16) + sb->u.minix.namelen;
			sb->u.minix.version = 2;
			sb->u.minix.nzones = sb->u.minix.sb.s_zones;
			printk("minix v2 (14 char names) filesystem detected on device %d,%d.\n", MAJOR(dev), MINOR(dev));
			break;
		case MINIX2_SUPER_MAGIC2:
			sb->u.minix.namelen = 30;
			sb->u.minix.dirsize = sizeof(__u16) + sb->u.minix.namelen;
			sb->u.minix.version = 2;
			sb->u.minix.nzones = sb->u.minix.sb.s_zones;
			printk("minix v2 (30 char names) filesystem detected on device %d,%d.\n", MAJOR(dev), MINOR(dev));
			break;
		default:
			printk("ERROR: %s(): invalid filesystem type or bad superblock on device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
			superblock_unlock(sb);
			brelse(buf);
			return -EINVAL;
	}

	sb->dev = dev;
	sb->fsop = &minix_fsop;
	sb->s_blocksize = BLKSIZE_1K << sb->u.minix.sb.s_log_zone_size;

	if(sb->s_blocksize != BLKSIZE_1K) {
		printk("ERROR: %s(): block sizes > %d not supported in this filesystem.\n", __FUNCTION__, BLKSIZE_1K);
		superblock_unlock(sb);
		brelse(buf);
		return -EINVAL;
	}

	/*
	printk("s_ninodes       = %d\n", sb->u.minix.sb.s_ninodes);
	printk("s_nzones        = %d (nzones = %d)\n", sb->u.minix.sb.s_nzones, sb->u.minix.nzones);
	printk("s_imap_blocks   = %d\n", sb->u.minix.sb.s_imap_blocks);
	printk("s_zmap_blocks   = %d\n", sb->u.minix.sb.s_zmap_blocks);
	printk("s_firstdatazone = %d\n", sb->u.minix.sb.s_firstdatazone);
	printk("s_log_zone_size = %d\n", sb->u.minix.sb.s_log_zone_size);
	printk("s_max_size      = %d\n", sb->u.minix.sb.s_max_size);
	printk("s_magic         = %x\n", sb->u.minix.sb.s_magic);
	printk("s_state         = %d\n", sb->u.minix.sb.s_state);
	printk("s_zones         = %d\n", sb->u.minix.sb.s_zones);
	*/

	/* Minix fs size is limited to: # of bitmaps * 8192 * 1024 */
	if(sb->u.minix.version == 1) {
		maps = V1_MAX_BITMAP_BLOCKS;	/* 64MB limit */
	}
	if(sb->u.minix.version == 2) {
		maps = V2_MAX_BITMAP_BLOCKS;	/* 1GB limit */
	}

	if(sb->u.minix.sb.s_imap_blocks > maps) {
		printk("ERROR: %s(): number of imap blocks (%d) is greater than %d!\n", __FUNCTION__, sb->u.minix.sb.s_imap_blocks, maps);
		superblock_unlock(sb);
		brelse(buf);
		return -EINVAL;
	}
	if(sb->u.minix.sb.s_zmap_blocks > maps) {
		printk("ERROR: %s(): number of zmap blocks (%d) is greater than %d!\n", __FUNCTION__, sb->u.minix.sb.s_zmap_blocks, maps);
		superblock_unlock(sb);
		brelse(buf);
		return -EINVAL;
	}

	superblock_unlock(sb);

	if(!(sb->root = iget(sb, MINIX_ROOT_INO))) {
		printk("ERROR: %s(): unable to get root inode.\n", __FUNCTION__);
		brelse(buf);
		return -EINVAL;
	}

	check_superblock(&sb->u.minix.sb);

	if(!(sb->flags & MS_RDONLY)) {
		sb->u.minix.sb.s_state &= ~MINIX_VALID_FS;
		memcpy_b(buf->data, &sb->u.minix.sb, sizeof(struct minix_super_block));
		bwrite(buf);
	} else {
		brelse(buf);
	}

	return 0;
}

int minix_remount_fs(struct superblock *sb, int flags)
{
	struct buffer *buf;
	struct minix_super_block *minixsb;

	if((flags & MS_RDONLY) == (sb->flags & MS_RDONLY)) {
		return 0;
	}

	superblock_lock(sb);
	if(!(buf = bread(sb->dev, SUPERBLOCK, BLKSIZE_1K))) {
		superblock_unlock(sb);
		return -EIO;
	}
	minixsb = (struct minix_super_block *)buf->data;

	if(flags & MS_RDONLY) {
		/* switching from RW to RO */
		sb->u.minix.sb.s_state |= MINIX_VALID_FS;
		minixsb->s_state |= MINIX_VALID_FS;
	} else {
		/* switching from RO to RW */
		check_superblock(minixsb);
		memcpy_b(&sb->u.minix.sb, minixsb, sizeof(struct minix_super_block));
		sb->u.minix.sb.s_state &= ~MINIX_VALID_FS;
		minixsb->s_state &= ~MINIX_VALID_FS;
	}

	sb->dirty = 1;
	superblock_unlock(sb);
	bwrite(buf);
	return 0;
}

int minix_write_superblock(struct superblock *sb)
{
	struct buffer *buf;

	superblock_lock(sb);
	if(!(buf = bread(sb->dev, SUPERBLOCK, BLKSIZE_1K))) {
		superblock_unlock(sb);
		return -EIO;
	}

	memcpy_b(buf->data, &sb->u.minix.sb, sizeof(struct minix_super_block));
	sb->dirty = 0;
	superblock_unlock(sb);
	bwrite(buf);
	return 0;
}

void minix_release_superblock(struct superblock *sb)
{
	if(sb->flags & MS_RDONLY) {
		return;
	}

	superblock_lock(sb);

	sb->u.minix.sb.s_state |= MINIX_VALID_FS;
	sb->dirty = 1;

	superblock_unlock(sb);
}

int minix_init(void)
{
	return register_filesystem("minix", &minix_fsop);
}
#endif /* CONFIG_FS_MINIX */
