/*
 * fiwix/fs/minix/v1_inode.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_minix.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/statfs.h>
#include <fiwix/sleep.h>
#include <fiwix/stat.h>
#include <fiwix/sched.h>
#include <fiwix/buffer.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define BLOCKS_PER_IND_BLOCK(sb)	(sb->s_blocksize / sizeof(__u16))
#define BLOCKS_PER_DIND_BLOCK(sb)	(BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb))

#define MINIX_INODES_PER_BLOCK(sb)	(sb->s_blocksize / sizeof(struct minix_inode))

#define MINIX_NDIR_BLOCKS		7
#define MINIX_IND_BLOCK			MINIX_NDIR_BLOCKS
#define MINIX_DIND_BLOCK		(MINIX_NDIR_BLOCKS + 1)

static int free_zone(struct inode *i, int block, int offset)
{
	int n;
	struct buffer *buf;
	__u16 *zone;

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		printk("WARNING: %s(): error reading block %d.\n", __FUNCTION__, block);
		return -EIO;
	}
	zone = (__u16 *)buf->data;
	for(n = offset; n < BLOCKS_PER_IND_BLOCK(i->sb); n++) {
		if(zone[n]) {
			minix_bfree(i->sb, zone[n]);
			zone[n] = 0;
		}
	}
	bwrite(buf);
	return 0;
}

static int free_indblock(struct inode *i, int block, int offset)
{
	int n, retval;
	struct buffer *buf;
	__u16 *zone;
	__blk_t dblock;

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		printk("%s(): error reading doubly indirect block %d.\n", __FUNCTION__, block);
		return -EIO;
	}
	zone = (__u16 *)buf->data;
	dblock = offset % BLOCKS_PER_IND_BLOCK(i->sb);
	for(n = offset / BLOCKS_PER_IND_BLOCK(i->sb); n < BLOCKS_PER_IND_BLOCK(i->sb); n++) {
		if(zone[n]) {
			if((retval = free_zone(i, zone[n], dblock)) < 0) {
				brelse(buf);
				return retval;
			}
			if(!dblock) {
				minix_bfree(i->sb, zone[n]);
				zone[n] = 0;
			}
		}
		dblock = 0;
	}
	bwrite(buf);
	return 0;
}

int v1_minix_read_inode(struct inode *i)
{
	__ino_t block;
	short int offset;
	struct minix_inode *ii;
	struct buffer *buf;
	int errno;

	block = 1 + SUPERBLOCK + i->sb->u.minix.sb.s_imap_blocks + i->sb->u.minix.sb.s_zmap_blocks + (i->inode - 1) / MINIX_INODES_PER_BLOCK(i->sb);

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = (i->inode - 1) % MINIX_INODES_PER_BLOCK(i->sb);
	ii = ((struct minix_inode *)buf->data) + offset;

	i->i_mode = ii->i_mode;
	i->i_uid = ii->i_uid;
	i->i_size = ii->i_size;
	i->i_atime = ii->i_time;
	i->i_ctime = ii->i_time;
	i->i_mtime = ii->i_time;
	i->i_gid = ii->i_gid;
	i->i_nlink = ii->i_nlinks;
	memcpy_b(i->u.minix.u.i1_zone, ii->i_zone, sizeof(ii->i_zone));
	i->count = 1;

	errno = 0;
	switch(i->i_mode & S_IFMT) {
		case S_IFCHR:
			i->fsop = &def_chr_fsop;
			i->rdev = ii->i_zone[0];
			break;
		case S_IFBLK:
			i->fsop = &def_blk_fsop;
			i->rdev = ii->i_zone[0];
			break;
		case S_IFIFO:
			i->fsop = &pipefs_fsop;
			/* it's a union so we need to clear pipefs_i */
			memset_b(&i->u.pipefs, NULL, sizeof(struct pipefs_inode));
			break;
		case S_IFDIR:
			i->fsop = &minix_dir_fsop;
			break;
		case S_IFREG:
			i->fsop = &minix_file_fsop;
			break;
		case S_IFLNK:
			i->fsop = &minix_symlink_fsop;
			break;
		case S_IFSOCK:
			i->fsop = NULL;
			break;
		default:
			printk("WARNING: %s(): invalid inode (%d) mode %o.\n", __FUNCTION__, i->inode, i->i_mode);
			errno = -ENOENT;
			break;
	}

	brelse(buf);
	return errno;
}

int v1_minix_write_inode(struct inode *i)
{
	__ino_t block;
	short int offset;
	struct minix_inode *ii;
	struct buffer *buf;

	block = 1 + SUPERBLOCK + i->sb->u.minix.sb.s_imap_blocks + i->sb->u.minix.sb.s_zmap_blocks + (i->inode - 1) / MINIX_INODES_PER_BLOCK(i->sb);

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = (i->inode - 1) % MINIX_INODES_PER_BLOCK(i->sb);
	ii = ((struct minix_inode *)buf->data) + offset;

	ii->i_mode = i->i_mode;
	ii->i_uid = i->i_uid;
	ii->i_size = i->i_size;
	ii->i_time = i->i_mtime;
	ii->i_gid = i->i_gid;
	ii->i_nlinks = i->i_nlink;
	if(S_ISCHR(i->i_mode) || S_ISBLK(i->i_mode)) {
		ii->i_zone[0] = i->rdev;
	} else {
		memcpy_b(ii->i_zone, i->u.minix.u.i1_zone, sizeof(i->u.minix.u.i1_zone));
	}
	i->dirty = 0;
	bwrite(buf);
	return 0;
}

int v1_minix_ialloc(struct inode *i, int mode)
{
	__blk_t offset;
	int inode, errno;
	struct superblock *sb;

	sb = i->sb;
	superblock_lock(sb);

	offset = 1 + SUPERBLOCK;

	if(!(inode = minix_find_first_zero(sb, offset, sb->u.minix.sb.s_ninodes, offset + sb->u.minix.sb.s_imap_blocks))) {
		superblock_unlock(sb);
		return -ENOSPC;
	}

	errno = minix_change_bit(SET_BIT, sb, offset, inode);

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to set inode %d.\n", __FUNCTION__, inode);
			superblock_unlock(sb);
			return errno;
		} else {
			printk("WARNING: %s(): inode %d is already marked as used!\n", __FUNCTION__, inode);
		}
	}

	i->inode = inode;
	i->i_atime = CURRENT_TIME;
	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	superblock_unlock(sb);
	return 0;
}

void v1_minix_ifree(struct inode *i)
{
	int errno;
	struct superblock *sb;

	minix_truncate(i, 0);

	sb = i->sb;
	superblock_lock(sb);

	errno = minix_change_bit(CLEAR_BIT, i->sb, 1 + SUPERBLOCK, i->inode);

	if(errno) {
		if(errno < 0) {
			printk("WARNING: %s(): unable to clear inode %d.\n", __FUNCTION__, i->inode);
		} else {
			printk("WARNING: %s(): inode %d is already marked as free!\n", __FUNCTION__, i->inode);
		}
	}

	i->i_size = 0;
	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->dirty = 1;
	superblock_unlock(sb);
}

int v1_minix_bmap(struct inode *i, __off_t offset, int mode)
{
	unsigned char level;
	__u16 *indblock, *dindblock;
	__blk_t block, iblock, dblock, newblock;
	int blksize;
	struct buffer *buf, *buf2, *buf3;

	blksize = i->sb->s_blocksize;
	block = offset / blksize;
	level = 0;

	if(block < MINIX_NDIR_BLOCKS) {
		level = MINIX_NDIR_BLOCKS - 1;
	} else {
		if(block < (BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
			level = MINIX_IND_BLOCK;
		} else {
			level = MINIX_DIND_BLOCK;
		}
		block -= MINIX_NDIR_BLOCKS;
	}

	if(level < MINIX_NDIR_BLOCKS) {
		if(!i->u.minix.u.i1_zone[block] && mode == FOR_WRITING) {
			if((newblock = minix_balloc(i->sb)) < 0) {
				return -ENOSPC;
			}
			/* initialize the new block */
			if(!(buf = bread(i->dev, newblock, blksize))) {
				minix_bfree(i->sb, newblock);
				return -EIO;
			}
			memset_b(buf->data, 0, blksize);
			bwrite(buf);
			i->u.minix.u.i1_zone[block] = newblock;
		}
		return i->u.minix.u.i1_zone[block];
	}

	if(!i->u.minix.u.i1_zone[level]) {
		if(mode == FOR_WRITING) {
			if((newblock = minix_balloc(i->sb)) < 0) {
				return -ENOSPC;
			}
			/* initialize the new block */
			if(!(buf = bread(i->dev, newblock, blksize))) {
				minix_bfree(i->sb, newblock);
				return -EIO;
			}
			memset_b(buf->data, 0, blksize);
			bwrite(buf);
			i->u.minix.u.i1_zone[level] = newblock;
		} else {
			return 0;
		}
	}
	if(!(buf = bread(i->dev, i->u.minix.u.i1_zone[level], blksize))) {
		return -EIO;
	}
	indblock = (__u16 *)buf->data;
	dblock = block - BLOCKS_PER_IND_BLOCK(i->sb);

	if(level == MINIX_DIND_BLOCK) {
		block = dblock / BLOCKS_PER_IND_BLOCK(i->sb);
	}

	if(!indblock[block]) {
		if(mode == FOR_WRITING) {
			if((newblock = minix_balloc(i->sb)) < 0) {
				brelse(buf);
				return -ENOSPC;
			}
			/* initialize the new block */
			if(!(buf2 = bread(i->dev, newblock, blksize))) {
				minix_bfree(i->sb, newblock);
				brelse(buf);
				return -EIO;
			}
			memset_b(buf2->data, 0, blksize);
			bwrite(buf2);
			indblock[block] = newblock;
			if(level == MINIX_IND_BLOCK) {
				bwrite(buf);
				return newblock;
			}
			buf->flags |= (BUFFER_DIRTY | BUFFER_VALID);
		} else {
			brelse(buf);
			return 0;
		}
	}
	if(level == MINIX_IND_BLOCK) {
		newblock = indblock[block];
		brelse(buf);
		return newblock;
	}

	iblock = block;
	if(!(buf2 = bread(i->dev, indblock[iblock], blksize))) {
		printk("%s(): returning -EIO\n", __FUNCTION__);
		brelse(buf);
		return -EIO;
	}
	dindblock = (__u16 *)buf2->data;
	block = dindblock[dblock - (iblock * BLOCKS_PER_IND_BLOCK(i->sb))];
	if(!block && mode == FOR_WRITING) {
		if((newblock = minix_balloc(i->sb)) < 0) {
			brelse(buf);
			brelse(buf2);
			return -ENOSPC;
		}
		/* initialize the new block */
		if(!(buf3 = bread(i->dev, newblock, blksize))) {
			minix_bfree(i->sb, newblock);
			brelse(buf);
			brelse(buf2);
			return -EIO;
		}
		memset_b(buf3->data, 0, blksize);
		bwrite(buf3);
		dindblock[dblock - (iblock * BLOCKS_PER_IND_BLOCK(i->sb))] = newblock;
		buf2->flags |= (BUFFER_DIRTY | BUFFER_VALID);
		block = newblock;
	}
	brelse(buf);
	brelse(buf2);
	return block;
}

int v1_minix_truncate(struct inode *i, __off_t length)
{
	__blk_t block;
	int n, retval;

	block = length / i->sb->s_blocksize;

	if(!S_ISDIR(i->i_mode) && !S_ISREG(i->i_mode) && !S_ISLNK(i->i_mode)) {
		return -EINVAL;
	}

	if(block < MINIX_NDIR_BLOCKS) {
		for(n = block; n < MINIX_NDIR_BLOCKS; n++) {
			if(i->u.minix.u.i1_zone[n]) {
				minix_bfree(i->sb, i->u.minix.u.i1_zone[n]);
				i->u.minix.u.i1_zone[n] = 0;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
		if(block) {
			block -= MINIX_NDIR_BLOCKS;
		}
		if(i->u.minix.u.i1_zone[MINIX_IND_BLOCK]) {
			if((retval = free_zone(i, i->u.minix.u.i1_zone[MINIX_IND_BLOCK], block)) < 0) {
				return retval;
			}
			if(!block) {
				minix_bfree(i->sb, i->u.minix.u.i1_zone[MINIX_IND_BLOCK]);
				i->u.minix.u.i1_zone[MINIX_IND_BLOCK] = 0;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_DIND_BLOCK(i->sb) + BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
		if(block) {
			block -= MINIX_NDIR_BLOCKS;
			block -= BLOCKS_PER_IND_BLOCK(i->sb);
		}
		if(i->u.minix.u.i1_zone[MINIX_DIND_BLOCK]) {
			if((retval = free_indblock(i, i->u.minix.u.i1_zone[MINIX_DIND_BLOCK], block)) < 0) {
				return retval;
			}
			if(!block) {
				minix_bfree(i->sb, i->u.minix.u.i1_zone[MINIX_DIND_BLOCK]);
				i->u.minix.u.i1_zone[MINIX_DIND_BLOCK] = 0;
			}
		}
		block = 0;
	}

	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->i_size = length;
	i->dirty = 1;

	return 0;
}
