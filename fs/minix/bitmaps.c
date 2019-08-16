/*
 * fiwix/fs/minix/bitmaps.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_minix.h>
#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define COUNT		1
#define FIRST_ZERO	2

static int count_bits(struct superblock *sb, __blk_t offset, int num, int blocks, int mode)
{
	unsigned char c;
	int blksize;
	int n, n2, last, bits, count, mapb;
	struct buffer *buf;

	count = mapb = 0;
	blksize = sb->s_blocksize;

	while(offset < blocks) {
		if(!(buf = bread(sb->dev, offset, blksize))) {
			return -EIO;
		}
		last = (num / 8) > blksize ? blksize : (num / 8);
		for(n = 0; n < last; n++) {
			c = (unsigned char)buf->data[n];
			bits = n < last ? 8 : num & 8;
			for(n2 = 0; n2 < bits; n2++) {
				if(c & (1 << n2)) {
					if(mode == COUNT) {
						count++;
					}
				} else {
					if(mode == FIRST_ZERO) {
						brelse(buf);
						return n2 + ((n * 8) + (mapb * blksize * 8));
					}
				}
			}
		}
		offset++;
		mapb++;
		num -= (blksize * 8);
		brelse(buf);
	}
	return count;
}

int minix_change_bit(int mode, struct superblock *sb, int map, int item)
{
	int byte, bit, mask;
	struct buffer *buf;

	map += item / (sb->s_blocksize * 8);
	byte = (item % (sb->s_blocksize * 8)) / 8;
	bit = (item % (sb->s_blocksize * 8)) % 8;
	mask = 1 << bit;

	if(!(buf = bread(sb->dev, map, sb->s_blocksize))) {
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

int minix_balloc(struct superblock *sb)
{
	int map, block, errno;

	superblock_lock(sb);

	map = 1 + SUPERBLOCK + sb->u.minix.sb.s_imap_blocks;

	if(!(block = minix_find_first_zero(sb, map, sb->u.minix.nzones, map + sb->u.minix.sb.s_zmap_blocks))) {
		superblock_unlock(sb);
		return -ENOSPC;
	}

	errno = minix_change_bit(SET_BIT, sb, map, block);
	block += sb->u.minix.sb.s_firstdatazone - 1;

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to set block %d.\n", __FUNCTION__, block);
			superblock_unlock(sb);
			return error;
		} else {
			printk("WARNING: %s(): block %d is already marked as used!\n", __FUNCTION__, block);
		}
	}

	superblock_unlock(sb);
	return block;
}

void minix_bfree(struct superblock *sb, int block)
{
	int map, errno;

	if(block < sb->u.minix.sb.s_firstdatazone || block > sb->u.minix.nzones) {
		printk("WARNING: %s(): block %d is not in datazone.\n", __FUNCTION__, block);
		return;
	}

	superblock_lock(sb);

	map = 1 + SUPERBLOCK + sb->u.minix.sb.s_imap_blocks;
	block -= sb->u.minix.sb.s_firstdatazone - 1;
	errno = minix_change_bit(CLEAR_BIT, sb, map, block);

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to free block %d.\n", __FUNCTION__, block);
		} else {
			printk("WARNING: %s(): block %d is already marked as free!\n", __FUNCTION__, block);
		}
	}

	superblock_unlock(sb);
	return;
}

int minix_count_free_inodes(struct superblock *sb)
{
	__blk_t offset;

	offset = 1 + SUPERBLOCK;
	return count_bits(sb, offset, sb->u.minix.sb.s_ninodes, offset + sb->u.minix.sb.s_imap_blocks, COUNT);
}

int minix_count_free_blocks(struct superblock *sb)
{
	__blk_t offset;

	offset = 1 + SUPERBLOCK + sb->u.minix.sb.s_imap_blocks;
	return count_bits(sb, offset, sb->u.minix.nzones, offset + sb->u.minix.sb.s_zmap_blocks, COUNT);
}

int minix_find_first_zero(struct superblock *sb, __blk_t offset, int num, int blocks)
{
	return count_bits(sb, offset, num, blocks, FIRST_ZERO);
}
