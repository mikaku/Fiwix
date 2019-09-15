/*
 * fiwix/fs/ext2/bitmaps.c
 *
 * Copyright 2019, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_ext2.h>
#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static int find_first_zero(struct superblock *sb, __blk_t block)
{
	unsigned char c;
	int blksize;
	int n, n2;
	struct buffer *buf;

	blksize = sb->s_blocksize;

	if(!(buf = bread(sb->dev, block, blksize))) {
		return -EIO;
	}
	for(n = 0; n < blksize; n++) {
		c = (unsigned char)buf->data[n];
		for(n2 = 0; n2 < 8; n2++) {
			if(!(c & (1 << n2))) {
				brelse(buf);
				return n2 + (n * 8) + 1;
			}
		}
	}
	brelse(buf);
	return 0;
}

static int change_bit(int mode, struct superblock *sb, __blk_t block, int item)
{
	int byte, bit, mask;
	struct buffer *buf;

	block += item / (sb->s_blocksize * 8);
	byte = (item % (sb->s_blocksize * 8)) / 8;
	bit = (item % (sb->s_blocksize * 8)) % 8;
	mask = 1 << bit;

	if(!(buf = bread(sb->dev, block, sb->s_blocksize))) {
		return -EIO;
	}

	if(mode == CLEAR_BIT) {
		if(!(buf->data[byte] & mask)) {
			brelse(buf);
			return 1;
		}
		buf->data[byte] &= ~mask;
	}
	if(mode == SET_BIT) {
		if((buf->data[byte] & mask)) {
			brelse(buf);
			return 1;
		}
		buf->data[byte] |= mask;
	}

	bwrite(buf);
	return 0;
}

/*
 * Unlike of what Ext2 specifies/suggests, this inode allocation does NOT
 * try to assign inodes in the same block group of the directory in which
 * they will be created.
 */
int ext2_ialloc(struct inode *i)
{
	__ino_t inode;
	__blk_t block;
	struct superblock *sb;
	struct ext2_group_desc *gd;
	struct buffer *buf;
	int bg, d, errno;

	sb = i->sb;
	superblock_lock(sb);

	block = SUPERBLOCK + sb->u.ext2.sb.s_first_data_block;
	inode = 0;
	buf = NULL;

	/* read through all group descriptors to find the first unallocated inode */
	for(bg = 0, d = 0; bg < sb->u.ext2.block_groups; bg++, d++) {
		if(!(bg % (sb->s_blocksize / sizeof(struct ext2_group_desc)))) {
			if(buf) {
				brelse(buf);
				block++;
				d = 0;
			}
			if(!(buf = bread(sb->dev, block, sb->s_blocksize))) {
				superblock_unlock(sb);
				return -EIO;
			}
		}
		gd = (struct ext2_group_desc *)(buf->data + (d * sizeof(struct ext2_group_desc)));
		if(gd->bg_free_inodes_count) {
			if((inode = find_first_zero(sb, gd->bg_inode_bitmap))) {
				break;
			}
		}
	}
	if(!inode) {
		brelse(buf);
		superblock_unlock(sb);
		return -ENOSPC;
	}

	errno = change_bit(SET_BIT, sb, gd->bg_inode_bitmap, inode - 1);
	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to set inode %d.\n", __FUNCTION__, inode);
			brelse(buf);
			superblock_unlock(sb);
			return errno;
		} else {
			printk("WARNING: %s(): inode %d is already marked as used!\n", __FUNCTION__, inode);
		}
	}

	inode += bg * EXT2_INODES_PER_GROUP(sb);
	gd->bg_free_inodes_count--;
	sb->u.ext2.sb.s_free_inodes_count--;
	// FIXME: gd->bg_used_dirs_count++; (if it's a directory)
	bwrite(buf);

	i->inode = inode;
	i->i_atime = CURRENT_TIME;
	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;

	superblock_unlock(sb);
	return 0;
}

void ext2_ifree(struct inode *i)
{
	struct ext2_group_desc *gd;
	struct buffer *buf;
	struct superblock *sb;
	__blk_t b, bg;
	int errno;

	if(!i->inode || i->inode > i->sb->u.ext2.sb.s_inodes_count) {
		printk("WARNING: %s(): invalid inode %d!\n", __FUNCTION__, i->inode);
		return;
	}

	if(i->i_blocks) {
		ext2_truncate(i, 0);
	}

	sb = i->sb;
	superblock_lock(sb);

	b = SUPERBLOCK + sb->u.ext2.sb.s_first_data_block;
	bg = (i->inode - 1) / EXT2_INODES_PER_GROUP(sb);
	if(!(buf = bread(sb->dev, b + (bg / EXT2_DESC_PER_BLOCK(sb)), sb->s_blocksize))) {
		superblock_unlock(sb);
		return;
	}
	gd = (struct ext2_group_desc *)(buf->data + ((bg % EXT2_DESC_PER_BLOCK(sb)) * sizeof(struct ext2_group_desc)));
	errno = change_bit(CLEAR_BIT, sb, gd->bg_inode_bitmap, (i->inode - 1) % EXT2_INODES_PER_GROUP(sb));

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to clear inode %d.\n", __FUNCTION__, i->inode);
			brelse(buf);
			superblock_unlock(sb);
			return;
		} else {
			printk("WARNING: %s(): inode %d is already marked as free!\n", __FUNCTION__, i->inode);
		}
	}

	gd->bg_free_inodes_count++;
	sb->u.ext2.sb.s_free_inodes_count++;
	// FIXME: gd->bg_used_dirs_count--; (if it's a directory)
	bwrite(buf);

	i->i_size = 0;
	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->dirty = 1;

	superblock_unlock(sb);
	return;
}

int ext2_balloc(struct superblock *sb)
{
	__blk_t b, block;
	struct ext2_group_desc *gd;
	struct buffer *buf;
	int bg, d, errno;

	superblock_lock(sb);

	b = SUPERBLOCK + sb->u.ext2.sb.s_first_data_block;
	block = 0;
	buf = NULL;

	/* read through all group descriptors to find the first unallocated block */
	for(bg = 0, d = 0; bg < sb->u.ext2.block_groups; bg++, d++) {
		if(!(bg % (sb->s_blocksize / sizeof(struct ext2_group_desc)))) {
			if(buf) {
				brelse(buf);
				b++;
				d = 0;
			}
			if(!(buf = bread(sb->dev, b, sb->s_blocksize))) {
				superblock_unlock(sb);
				return -EIO;
			}
		}
		gd = (struct ext2_group_desc *)(buf->data + (d * sizeof(struct ext2_group_desc)));
		if(gd->bg_free_blocks_count) {
			if((block = find_first_zero(sb, gd->bg_block_bitmap))) {
				break;
			}
		}
	}
	if(!block) {
		brelse(buf);
		superblock_unlock(sb);
		return -ENOSPC;
	}

	errno = change_bit(SET_BIT, sb, gd->bg_block_bitmap, block - 1);
	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to set block %d.\n", __FUNCTION__, block);
			brelse(buf);
			superblock_unlock(sb);
			return errno;
		} else {
			printk("WARNING: %s(): block %d is already marked as used!\n", __FUNCTION__, block);
		}
	}

	block += bg * EXT2_BLOCKS_PER_GROUP(sb);
	gd->bg_free_blocks_count--;
	sb->u.ext2.sb.s_free_blocks_count--;
	bwrite(buf);

	superblock_unlock(sb);
	return block;
}

void ext2_bfree(struct superblock *sb, int block)
{
	struct ext2_group_desc *gd;
	struct buffer *buf;
	__blk_t b, bg;
	int errno;

	if(!block || block > sb->u.ext2.sb.s_blocks_count) {
		printk("WARNING: %s(): invalid block %d!\n", __FUNCTION__, block);
		return;
	}

	superblock_lock(sb);

	b = SUPERBLOCK + sb->u.ext2.sb.s_first_data_block;
	bg = (block - 1) / EXT2_BLOCKS_PER_GROUP(sb);
	if(!(buf = bread(sb->dev, b + (bg / EXT2_DESC_PER_BLOCK(sb)), sb->s_blocksize))) {
		superblock_unlock(sb);
		return;
	}
	gd = (struct ext2_group_desc *)(buf->data + ((bg % EXT2_DESC_PER_BLOCK(sb)) * sizeof(struct ext2_group_desc)));
	errno = change_bit(CLEAR_BIT, sb, gd->bg_block_bitmap, (block - 1) % EXT2_BLOCKS_PER_GROUP(sb));

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to free block %d.\n", __FUNCTION__, block);
			brelse(buf);
			superblock_unlock(sb);
			return;
		} else {
			printk("WARNING: %s(): block %d is already marked as free!\n", __FUNCTION__, block);
		}
	}

	gd->bg_free_blocks_count++;
	sb->u.ext2.sb.s_free_blocks_count++;
	bwrite(buf);

	superblock_unlock(sb);
	return;
}
