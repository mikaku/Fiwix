/*
 * fiwix/fs/ext2/super.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_ext2.h>
#include <fiwix/buffer.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations ext2_fsop = {
	FSOP_REQUIRES_DEV,
	NULL,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* lseek */
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

	ext2_read_inode,
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	ext2_statfs,
	ext2_read_superblock,
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static void check_superblock(struct ext2_super_block *sb)
{
	if(!(sb->s_state & EXT2_VALID_FS)) {
		printk("WARNING: filesystem unchecked, fsck recommended.\n");
	} else if((sb->s_state & EXT2_ERROR_FS)) {
		printk("WARNING: filesystem contains errors, fsck recommended.\n");
	} else if(sb->s_max_mnt_count >= 0 && sb->s_mnt_count >= (unsigned short int)sb->s_max_mnt_count) {
		printk("WARNING: maximal mount count reached, fsck recommended.\n");
	} else if(sb->s_checkinterval && (sb->s_lastcheck + sb->s_checkinterval <= CURRENT_TIME)) {
		printk("WARNING: checktime reached, fsck recommended.\n");
	}
}

void ext2_statfs(struct superblock *sb, struct statfs *statfsbuf)
{
	statfsbuf->f_type = EXT2_SUPER_MAGIC;
	statfsbuf->f_bsize = sb->s_blocksize;
	statfsbuf->f_blocks = sb->u.ext2.s_blocks_count;
	statfsbuf->f_bfree = sb->u.ext2.s_free_blocks_count;
	if(statfsbuf->f_bfree >= sb->u.ext2.s_r_blocks_count) {
		statfsbuf->f_bavail = statfsbuf->f_bfree - sb->u.ext2.s_r_blocks_count;
	} else {
		statfsbuf->f_bavail = 0;
	}
	statfsbuf->f_files = sb->u.ext2.s_inodes_count;
	statfsbuf->f_ffree = sb->u.ext2.s_free_inodes_count;
	/* statfsbuf->f_fsid = ? */
	statfsbuf->f_namelen = EXT2_NAME_LEN;
}

int ext2_read_superblock(__dev_t dev, struct superblock *sb)
{
	struct buffer *buf;
	struct ext2_super_block *ext2sb;

	superblock_lock(sb);
	if(!(buf = bread(dev, SUPERBLOCK, BLKSIZE_1K))) {
		superblock_unlock(sb);
		return -EIO;
	}

	ext2sb = (struct ext2_super_block *)buf->data;
	if(ext2sb->s_magic != EXT2_SUPER_MAGIC) {
		printk("WARNING: %s(): invalid filesystem type or bad superblock on device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		superblock_unlock(sb);
		brelse(buf);
		return -EINVAL;
	}

	/* sparse-superblock feature not supported (only for read-write mode) */
	if(!sb->flags & MS_RDONLY) {
		if(ext2sb->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
			printk("WARNING: %s(): sparse-superblock feature is not supported.\n", __FUNCTION__);
			printk("filesystem structure not supported. Try with '-o ro'.\n");
			superblock_unlock(sb);
			brelse(buf);
			return -EINVAL;
		}
	}

	sb->dev = dev;
	sb->fsop = &ext2_fsop;
	sb->s_blocksize = EXT2_MIN_BLOCK_SIZE << ext2sb->s_log_block_size;
	memcpy_b(&sb->u.ext2, ext2sb, sizeof(struct ext2_super_block));

	if(!(sb->root = iget(sb, EXT2_ROOT_INO))) {
		printk("WARNING: %s(): unable to get root inode.\n", __FUNCTION__);
		superblock_unlock(sb);
		brelse(buf);
		return -EINVAL;
	}

	superblock_unlock(sb);
	check_superblock(ext2sb);
	brelse(buf);
	return 0;
}

int ext2_init(void)
{
	return register_filesystem("ext2", &ext2_fsop);
}
