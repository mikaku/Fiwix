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
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stat.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static int find_first_zero(struct superblock *sb, __blk_t bmblock, struct buffer **buf)
{
	unsigned char c;
	int n, n2;

	if(!(*buf = bread(sb->dev, bmblock, sb->s_blocksize))) {
		return -EIO;
	}
	for(n = 0; n < sb->s_blocksize; n++) {
		if((c = (unsigned char)(*buf)->data[n]) == 0xFF) {
			continue;
		}
		for(n2 = 0; n2 < 8; n2++) {
			if(!(c & (1 << n2))) {
				return (n * 8) + n2;
			}
		}
	}
	return -ENOSPC;
}

static int change_bit(int mode, struct superblock *sb, __blk_t bmblock, struct buffer *bmbuf, int item)
{
	int byte, bit, mask;
	struct buffer *buf;

	byte = (item % (sb->s_blocksize * 8)) / 8;
	bit = (item % (sb->s_blocksize * 8)) % 8;
	mask = 1 << bit;

	if(!bmbuf) {
		if(!(buf = bread(sb->dev, bmblock, sb->s_blocksize))) {
			return -EIO;
		}
	} else {
		buf = bmbuf;
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
int ext2_ialloc(struct inode *i, int mode)
{
	__ino_t inode;
	__blk_t block;
	struct superblock *sb;
	struct ext2_group_desc *gd;
	struct buffer *buf, *bmbuf;
	int bg, d, errno;

	sb = i->sb;
	superblock_lock(sb);

	block = SUPERBLOCK + sb->u.ext2.sb.s_first_data_block;
	buf = bmbuf = NULL;

	/* read through all group descriptors to find the first unallocated inode */
	for(bg = 0, d = 0; bg < sb->u.ext2.block_groups; bg++, d++) {
		if(!(bg % EXT2_DESC_PER_BLOCK(sb))) {
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
			if((errno = find_first_zero(sb, gd->bg_inode_bitmap, &bmbuf)) != -ENOSPC) {
				break;
			}
			brelse(bmbuf);
		}
	}
	if(errno < 0) {
		brelse(buf);
		superblock_unlock(sb);
		return errno;
	}

	inode = errno;
	errno = change_bit(SET_BIT, sb, gd->bg_inode_bitmap, bmbuf, inode);
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

	inode += (bg * EXT2_INODES_PER_GROUP(sb)) + 1;
	gd->bg_free_inodes_count--;
	sb->u.ext2.sb.s_free_inodes_count--;
	sb->state |= SUPERBLOCK_DIRTY;
	if(S_ISDIR(mode)) {
		gd->bg_used_dirs_count++;
	}
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
		invalidate_inode_pages(i);
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
	errno = change_bit(CLEAR_BIT, sb, gd->bg_inode_bitmap, NULL, (i->inode - 1) % EXT2_INODES_PER_GROUP(sb));

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to free inode %d.\n", __FUNCTION__, i->inode);
			brelse(buf);
			superblock_unlock(sb);
			return;
		} else {
			printk("WARNING: %s(): inode %d is already marked as free!\n", __FUNCTION__, i->inode);
		}
	}

	gd->bg_free_inodes_count++;
	sb->u.ext2.sb.s_free_inodes_count++;
	sb->state |= SUPERBLOCK_DIRTY;
	if(S_ISDIR(i->i_mode)) {
		gd->bg_used_dirs_count--;
	}
	bwrite(buf);

	i->i_size = 0;
	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->state |= INODE_DIRTY;

	superblock_unlock(sb);
	return;
}

int ext2_balloc(struct superblock *sb)
{
	__blk_t block;
	struct ext2_group_desc *gd;
	struct buffer *buf, *bmbuf;
	int bg, d, errno;

	superblock_lock(sb);

	block = SUPERBLOCK + sb->u.ext2.sb.s_first_data_block;
	buf = bmbuf = NULL;

	/* read through all group descriptors to find the first unallocated block */
	for(bg = 0, d = 0; bg < sb->u.ext2.block_groups; bg++, d++) {
		if(!(bg % EXT2_DESC_PER_BLOCK(sb))) {
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
		if(gd->bg_free_blocks_count) {
			if((errno = find_first_zero(sb, gd->bg_block_bitmap, &bmbuf)) != -ENOSPC) {
				break;
			}
			brelse(bmbuf);
		}
	}
	if(errno < 0) {
		brelse(buf);
		superblock_unlock(sb);
		return errno;
	}

	block = errno;
	errno = change_bit(SET_BIT, sb, gd->bg_block_bitmap, bmbuf, block);
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

	block += (bg * EXT2_BLOCKS_PER_GROUP(sb)) + sb->u.ext2.sb.s_first_data_block;
	gd->bg_free_blocks_count--;
	sb->u.ext2.sb.s_free_blocks_count--;
	sb->state |= SUPERBLOCK_DIRTY;
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
	bg = (block - sb->u.ext2.sb.s_first_data_block) / EXT2_BLOCKS_PER_GROUP(sb);
	if(!(buf = bread(sb->dev, b + (bg / EXT2_DESC_PER_BLOCK(sb)), sb->s_blocksize))) {
		superblock_unlock(sb);
		return;
	}
	gd = (struct ext2_group_desc *)(buf->data + ((bg % EXT2_DESC_PER_BLOCK(sb)) * sizeof(struct ext2_group_desc)));
	errno = change_bit(CLEAR_BIT, sb, gd->bg_block_bitmap, NULL, (block - sb->u.ext2.sb.s_first_data_block) % EXT2_BLOCKS_PER_GROUP(sb));

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
	sb->state |= SUPERBLOCK_DIRTY;
	bwrite(buf);

	superblock_unlock(sb);
	return;
}
