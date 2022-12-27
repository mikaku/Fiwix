/*
 * fiwix/fs/minix/v2_inode.c
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

#define BLOCKS_PER_IND_BLOCK(sb)	(sb->s_blocksize / sizeof(__u32))
#define BLOCKS_PER_DIND_BLOCK(sb)	(BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb))
#define BLOCKS_PER_TIND_BLOCK(sb)	(BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb))

#define MINIX2_INODES_PER_BLOCK(sb)	(sb->s_blocksize / sizeof(struct minix2_inode))

#define MINIX_NDIR_BLOCKS		7
#define MINIX_IND_BLOCK			MINIX_NDIR_BLOCKS
#define MINIX_DIND_BLOCK		(MINIX_NDIR_BLOCKS + 1)
#define MINIX_TIND_BLOCK		(MINIX_NDIR_BLOCKS + 2)

static int free_zone(struct inode *i, int block, int offset)
{
	int n;
	struct buffer *buf;
	__u32 *zone;

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		printk("WARNING: %s(): error reading block %d.\n", __FUNCTION__, block);
		return -EIO;
	}
	zone = (__u32 *)buf->data;
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
	__u32 *zone;
	__blk_t dblock;

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		printk("%s(): error reading doubly indirect block %d.\n", __FUNCTION__, block);
		return -EIO;
	}
	zone = (__u32 *)buf->data;
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

int v2_minix_read_inode(struct inode *i)
{
	__ino_t block;
	short int offset;
	struct minix2_inode *ii;
	struct buffer *buf;
	int errno;

	block = 1 + SUPERBLOCK + i->sb->u.minix.sb.s_imap_blocks + i->sb->u.minix.sb.s_zmap_blocks + (i->inode - 1) / MINIX2_INODES_PER_BLOCK(i->sb);

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = (i->inode - 1) % MINIX2_INODES_PER_BLOCK(i->sb);
	ii = ((struct minix2_inode *)buf->data) + offset;

	i->i_mode = ii->i_mode;
	i->i_nlink = ii->i_nlink;
	i->i_uid = ii->i_uid;
	i->i_gid = ii->i_gid;
	i->i_size = ii->i_size;
	i->i_atime = ii->i_atime;
	i->i_mtime = ii->i_mtime;
	i->i_ctime = ii->i_ctime;
	memcpy_b(i->u.minix.u.i2_zone, ii->i_zone, sizeof(ii->i_zone));
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
			memset_b(&i->u.pipefs, 0, sizeof(struct pipefs_inode));
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

int v2_minix_write_inode(struct inode *i)
{
	__ino_t block;
	short int offset;
	struct minix2_inode *ii;
	struct buffer *buf;

	block = 1 + SUPERBLOCK + i->sb->u.minix.sb.s_imap_blocks + i->sb->u.minix.sb.s_zmap_blocks + (i->inode - 1) / MINIX2_INODES_PER_BLOCK(i->sb);

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = (i->inode - 1) % MINIX2_INODES_PER_BLOCK(i->sb);
	ii = ((struct minix2_inode *)buf->data) + offset;

	ii->i_mode = i->i_mode;
	ii->i_nlink = i->i_nlink;
	ii->i_uid = i->i_uid;
	ii->i_gid = i->i_gid;
	ii->i_size = i->i_size;
	ii->i_atime = i->i_atime;
	ii->i_mtime = i->i_mtime;
	ii->i_ctime = i->i_ctime;
	if(S_ISCHR(i->i_mode) || S_ISBLK(i->i_mode)) {
		ii->i_zone[0] = i->rdev;
	} else {
		memcpy_b(ii->i_zone, i->u.minix.u.i2_zone, sizeof(i->u.minix.u.i2_zone));
	}
	i->dirty = 0;
	bwrite(buf);
	return 0;
}

int v2_minix_ialloc(struct inode *i, int mode)
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

void v2_minix_ifree(struct inode *i)
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

int v2_minix_bmap(struct inode *i, __off_t offset, int mode)
{
	unsigned char level;
	__u32 *indblock, *dindblock, *tindblock;
	__blk_t block, iblock, dblock, tblock, newblock;
	int blksize;
	struct buffer *buf, *buf2, *buf3, *buf4;

	blksize = i->sb->s_blocksize;
	block = offset / blksize;
	level = 0;
	buf3 = NULL;	/* makes GCC happy */

	if(block < MINIX_NDIR_BLOCKS) {
		level = MINIX_NDIR_BLOCKS - 1;
	} else {
		if(block < (BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
			level = MINIX_IND_BLOCK;
		} else if(block < ((BLOCKS_PER_IND_BLOCK(i->sb) * BLOCKS_PER_IND_BLOCK(i->sb)) + BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
			level = MINIX_DIND_BLOCK;
		} else {
			level = MINIX_TIND_BLOCK;
		}
		block -= MINIX_NDIR_BLOCKS;
	}

	if(level < MINIX_NDIR_BLOCKS) {
		if(!i->u.minix.u.i2_zone[block] && mode == FOR_WRITING) {
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
			i->u.minix.u.i2_zone[block] = newblock;
		}
		return i->u.minix.u.i2_zone[block];
	}

	if(!i->u.minix.u.i2_zone[level]) {
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
			i->u.minix.u.i2_zone[level] = newblock;
		} else {
			return 0;
		}
	}
	if(!(buf = bread(i->dev, i->u.minix.u.i2_zone[level], blksize))) {
		return -EIO;
	}
	indblock = (__u32 *)buf->data;
	dblock = block - BLOCKS_PER_IND_BLOCK(i->sb);
	tblock = block - (BLOCKS_PER_IND_BLOCK(i->sb) * BLOCKS_PER_IND_BLOCK(i->sb)) - BLOCKS_PER_IND_BLOCK(i->sb);

	if(level == MINIX_DIND_BLOCK) {
		block = dblock / BLOCKS_PER_IND_BLOCK(i->sb);
	}
	if(level == MINIX_TIND_BLOCK) {
		block = tblock / (BLOCKS_PER_IND_BLOCK(i->sb) * BLOCKS_PER_IND_BLOCK(i->sb));
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

	if(level == MINIX_TIND_BLOCK) {
		if(!(buf3 = bread(i->dev, indblock[block], blksize))) {
			printk("%s(): returning -EIO\n", __FUNCTION__);
			brelse(buf);
			return -EIO;
		}
		tindblock = (__u32 *)buf3->data;
		tblock -= BLOCKS_PER_DIND_BLOCK(i->sb) * block;
		block = tindblock[tblock / BLOCKS_PER_IND_BLOCK(i->sb)];
		if(!block) {
			if(mode == FOR_WRITING) {
				if((newblock = minix_balloc(i->sb)) < 0) {
					brelse(buf);
					brelse(buf3);
					return -ENOSPC;
				}
				/* initialize the new block */
				if(!(buf4 = bread(i->dev, newblock, blksize))) {
					minix_bfree(i->sb, newblock);
					brelse(buf);
					brelse(buf3);
					return -EIO;
				}
				memset_b(buf4->data, 0, blksize);
				bwrite(buf4);
				tindblock[tblock / BLOCKS_PER_IND_BLOCK(i->sb)] = newblock;
				buf3->flags |= (BUFFER_DIRTY | BUFFER_VALID);
				block = newblock;
			} else {
				brelse(buf);
				brelse(buf3);
				return 0;
			}
		}
		dblock = tblock;
		iblock = tblock / BLOCKS_PER_IND_BLOCK(i->sb);
		if(!(buf2 = bread(i->dev, block, blksize))) {
			printk("%s(): returning -EIO\n", __FUNCTION__);
			brelse(buf);
			brelse(buf3);
			return -EIO;
		}
	} else {
		iblock = block;
		if(!(buf2 = bread(i->dev, indblock[iblock], blksize))) {
			printk("%s(): returning -EIO\n", __FUNCTION__);
			brelse(buf);
			return -EIO;
		}
	}

	dindblock = (__u32 *)buf2->data;
	block = dindblock[dblock - (iblock * BLOCKS_PER_IND_BLOCK(i->sb))];
	if(!block && mode == FOR_WRITING) {
		if((newblock = minix_balloc(i->sb)) < 0) {
			brelse(buf);
			if(level == MINIX_TIND_BLOCK) {
				brelse(buf3);
			}
			brelse(buf2);
			return -ENOSPC;
		}
		/* initialize the new block */
		if(!(buf4 = bread(i->dev, newblock, blksize))) {
			minix_bfree(i->sb, newblock);
			brelse(buf);
			if(level == MINIX_TIND_BLOCK) {
				brelse(buf3);
			}
			brelse(buf2);
			return -EIO;
		}
		memset_b(buf4->data, 0, blksize);
		bwrite(buf4);
		dindblock[dblock - (iblock * BLOCKS_PER_IND_BLOCK(i->sb))] = newblock;
		buf2->flags |= (BUFFER_DIRTY | BUFFER_VALID);
		block = newblock;
	}
	brelse(buf);
	if(level == MINIX_TIND_BLOCK) {
		brelse(buf3);
	}
	brelse(buf2);
	return block;
}

int v2_minix_truncate(struct inode *i, __off_t length)
{
	__blk_t block, indblock, *dindblock;
	struct buffer *buf;
	int n, retval;

	block = length / i->sb->s_blocksize;

	if(!S_ISDIR(i->i_mode) && !S_ISREG(i->i_mode) && !S_ISLNK(i->i_mode)) {
		return -EINVAL;
	}

	if(block < MINIX_NDIR_BLOCKS) {
		for(n = block; n < MINIX_NDIR_BLOCKS; n++) {
			if(i->u.minix.u.i2_zone[n]) {
				minix_bfree(i->sb, i->u.minix.u.i2_zone[n]);
				i->u.minix.u.i2_zone[n] = 0;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
		if(block) {
			block -= MINIX_NDIR_BLOCKS;
		}
		if(i->u.minix.u.i2_zone[MINIX_IND_BLOCK]) {
			if((retval = free_zone(i, i->u.minix.u.i2_zone[MINIX_IND_BLOCK], block)) < 0) {
				return retval;
			}
			if(!block) {
				minix_bfree(i->sb, i->u.minix.u.i2_zone[MINIX_IND_BLOCK]);
				i->u.minix.u.i2_zone[MINIX_IND_BLOCK] = 0;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_DIND_BLOCK(i->sb) + BLOCKS_PER_IND_BLOCK(i->sb) + MINIX_NDIR_BLOCKS)) {
		if(block) {
			block -= MINIX_NDIR_BLOCKS;
			block -= BLOCKS_PER_IND_BLOCK(i->sb);
		}
		if(i->u.minix.u.i2_zone[MINIX_DIND_BLOCK]) {
			if((retval = free_indblock(i, i->u.minix.u.i2_zone[MINIX_DIND_BLOCK], block)) < 0) {
				return retval;
			}
			if(!block) {
				minix_bfree(i->sb, i->u.minix.u.i2_zone[MINIX_DIND_BLOCK]);
				i->u.minix.u.i2_zone[MINIX_DIND_BLOCK] = 0;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_TIND_BLOCK(i->sb) + BLOCKS_PER_DIND_BLOCK(i->sb) + BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
		if(block) {
			block -= MINIX_NDIR_BLOCKS;
			block -= BLOCKS_PER_IND_BLOCK(i->sb);
			block -= BLOCKS_PER_DIND_BLOCK(i->sb);
		}
		if(i->u.minix.u.i2_zone[MINIX_TIND_BLOCK]) {
			if(!(buf = bread(i->dev, i->u.minix.u.i2_zone[MINIX_TIND_BLOCK], i->sb->s_blocksize))) {
				printk("%s(): error reading the triply indirect block (%d).\n", __FUNCTION__, i->u.minix.u.i2_zone[MINIX_TIND_BLOCK]);
				return -EIO;
			}
			dindblock = (__blk_t *)buf->data;
			indblock = block % BLOCKS_PER_IND_BLOCK(i->sb);
			for(n = block / BLOCKS_PER_IND_BLOCK(i->sb); n < BLOCKS_PER_IND_BLOCK(i->sb); n++) {
				if(dindblock[n]) {
					if((retval = free_indblock(i, dindblock[n], indblock)) < 0) {
						brelse(buf);
						return retval;
					}
					if(!indblock) {
						minix_bfree(i->sb, dindblock[n]);
						dindblock[n] = 0;
					}
				}
				indblock = 0;
			}
			bwrite(buf);
			if(!block) {
				minix_bfree(i->sb, i->u.minix.u.i2_zone[MINIX_TIND_BLOCK]);
				i->u.minix.u.i2_zone[MINIX_TIND_BLOCK] = 0;
			}
		}
	}

	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->i_size = length;
	i->dirty = 1;

	return 0;
}
