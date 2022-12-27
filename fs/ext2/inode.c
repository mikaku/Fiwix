/*
 * fiwix/fs/ext2/inode.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_ext2.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/statfs.h>
#include <fiwix/sleep.h>
#include <fiwix/stat.h>
#include <fiwix/sched.h>
#include <fiwix/buffer.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define BLOCKS_PER_IND_BLOCK(sb)	(EXT2_BLOCK_SIZE(sb) / sizeof(unsigned int))
#define BLOCKS_PER_DIND_BLOCK(sb)	(BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb))
#define BLOCKS_PER_TIND_BLOCK(sb)	(BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb) * BLOCKS_PER_IND_BLOCK(sb))

#define EXT2_INODES_PER_BLOCK(sb)	(EXT2_BLOCK_SIZE(sb) / sizeof(struct ext2_inode))

static int free_dblock(struct inode *i, int block, int offset)
{
	int n;
	struct buffer *buf;
	__blk_t *dblock;

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		printk("WARNING: %s(): error reading block %d.\n", __FUNCTION__, block);
		return -EIO;
	}
	dblock = (__blk_t *)buf->data;
	for(n = offset; n < BLOCKS_PER_IND_BLOCK(i->sb); n++) {
		if(dblock[n]) {
			ext2_bfree(i->sb, dblock[n]);
			dblock[n] = 0;
			i->i_blocks -= i->sb->s_blocksize / 512;
		}
	}
	bwrite(buf);
	return 0;
}

static int free_indblock(struct inode *i, int block, int offset)
{
	int n, retval;
	struct buffer *buf;
	__blk_t dblock, *indblock;

	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		printk("%s(): error reading doubly indirect block %d.\n", __FUNCTION__, block);
		return -EIO;
	}
	indblock = (__blk_t *)buf->data;
	dblock = offset % BLOCKS_PER_IND_BLOCK(i->sb);
	for(n = offset / BLOCKS_PER_IND_BLOCK(i->sb); n < BLOCKS_PER_IND_BLOCK(i->sb); n++) {
		if(indblock[n]) {
			if((retval = free_dblock(i, indblock[n], dblock)) < 0) {
				brelse(buf);
				return retval;
			}
			if(!dblock) {
				ext2_bfree(i->sb, indblock[n]);
				indblock[n] = 0;
				i->i_blocks -= i->sb->s_blocksize / 512;
			}
		}
		dblock = 0;
	}
	bwrite(buf);
	return 0;
}

static int get_group_desc(struct superblock *sb, __blk_t block_group, struct ext2_group_desc *gd)
{
	__blk_t group_desc_block;
	int group_desc;
	struct buffer *buf;

	group_desc_block = block_group / EXT2_DESC_PER_BLOCK(sb);
	group_desc = block_group % EXT2_DESC_PER_BLOCK(sb);
	if(!(buf = bread(sb->dev, SUPERBLOCK + sb->u.ext2.sb.s_first_data_block + group_desc_block, sb->s_blocksize))) {
		return -EIO;
	}
	memcpy_b(gd, (void *)(buf->data + (group_desc * sizeof(struct ext2_group_desc))), sizeof(struct ext2_group_desc));
	brelse(buf);
	return 0;
}

int ext2_read_inode(struct inode *i)
{
	__blk_t block_group, block;
	unsigned int offset;
	struct superblock *sb;
	struct ext2_inode *ii;
	struct ext2_group_desc gd;
	struct buffer *buf;

	if(!(sb = get_superblock(i->dev))) {
		printk("WARNING: %s(): get_superblock() has returned NULL.\n");
		return -EINVAL;
	}
	block_group = ((i->inode - 1) / EXT2_INODES_PER_GROUP(sb));
	if(get_group_desc(sb, block_group, &gd)) {
		return -EIO;
	}
	block = (((i->inode - 1) % EXT2_INODES_PER_GROUP(sb)) / EXT2_INODES_PER_BLOCK(sb));

	if(!(buf = bread(i->dev, gd.bg_inode_table + block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = ((((i->inode - 1) % EXT2_INODES_PER_GROUP(sb)) % EXT2_INODES_PER_BLOCK(sb)) * sizeof(struct ext2_inode));

	ii = (struct ext2_inode *)(buf->data + offset);
	memcpy_b(&i->u.ext2.i_data, ii->i_block, sizeof(ii->i_block));

	i->i_mode = ii->i_mode;
	i->i_uid = ii->i_uid;
	i->i_size = ii->i_size;
	i->i_atime = ii->i_atime;
	i->i_ctime = ii->i_ctime;
	i->i_mtime = ii->i_mtime;
	i->i_gid = ii->i_gid;
	i->i_nlink = ii->i_links_count;
	i->i_blocks = ii->i_blocks;
	i->i_flags = ii->i_flags;
	i->count = 1;
	switch(i->i_mode & S_IFMT) {
		case S_IFCHR:
			i->fsop = &def_chr_fsop;
			i->rdev = ii->i_block[0];
			break;
		case S_IFBLK:
			i->fsop = &def_blk_fsop;
			i->rdev = ii->i_block[0];
			break;
		case S_IFIFO:
			i->fsop = &pipefs_fsop;
			/* it's a union so we need to clear pipefs_i */
			memset_b(&i->u.pipefs, 0, sizeof(struct pipefs_inode));
			break;
		case S_IFDIR:
			i->fsop = &ext2_dir_fsop;
			break;
		case S_IFREG:
			i->fsop = &ext2_file_fsop;
			break;
		case S_IFLNK:
			i->fsop = &ext2_symlink_fsop;
			break;
		case S_IFSOCK:
			i->fsop = NULL;
			break;
		default:
			printk("WARNING: %s(): invalid inode (%d) mode %08o.\n", __FUNCTION__, i->inode, i->i_mode);
			brelse(buf);
			return -ENOENT;
	}
	brelse(buf);
	return 0;
}

int ext2_write_inode(struct inode *i)
{
	__blk_t block_group, block;
	short int offset;
	struct superblock *sb;
	struct ext2_inode *ii;
	struct ext2_group_desc gd;
	struct buffer *buf;

	if(!(sb = get_superblock(i->dev))) {
		printk("WARNING: %s(): get_superblock() has returned NULL.\n");
		return -EINVAL;
	}
	block_group = ((i->inode - 1) / EXT2_INODES_PER_GROUP(sb));
	if(get_group_desc(sb, block_group, &gd)) {
		return -EIO;
	}
	block = (((i->inode - 1) % EXT2_INODES_PER_GROUP(sb)) / EXT2_INODES_PER_BLOCK(sb));

	if(!(buf = bread(i->dev, gd.bg_inode_table + block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = ((((i->inode - 1) % EXT2_INODES_PER_GROUP(sb)) % EXT2_INODES_PER_BLOCK(sb)) * sizeof(struct ext2_inode));
	ii = (struct ext2_inode *)(buf->data + offset);
	memset_b(ii, 0, sizeof(struct ext2_inode));

	ii->i_mode = i->i_mode;
	ii->i_uid = i->i_uid;
	ii->i_size = i->i_size;
	ii->i_atime = i->i_atime;
	ii->i_ctime = i->i_ctime;
	ii->i_mtime = i->i_mtime;
	ii->i_dtime = i->u.ext2.i_dtime;
	ii->i_gid = i->i_gid;
	ii->i_links_count = i->i_nlink;
	ii->i_blocks = i->i_blocks;
	ii->i_flags = i->i_flags;
	if(S_ISCHR(i->i_mode) || S_ISBLK(i->i_mode)) {
		ii->i_block[0] = i->rdev;
	} else {
		memcpy_b(ii->i_block, &i->u.ext2.i_data, sizeof(i->u.ext2.i_data));
	}
	i->dirty = 0;
	bwrite(buf);
	return 0;
}

int ext2_bmap(struct inode *i, __off_t offset, int mode)
{
	unsigned char level;
	__blk_t *indblock, *dindblock, *tindblock;
	__blk_t block, iblock, dblock, tblock, newblock;
	int blksize;
	struct buffer *buf, *buf2, *buf3, *buf4;

	blksize = i->sb->s_blocksize;
	block = offset / blksize;
	level = 0;
	buf3 = NULL;	/* makes GCC happy */

	if(block < EXT2_NDIR_BLOCKS) {
		level = EXT2_NDIR_BLOCKS - 1;
	} else {
		if(block < (BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
			level = EXT2_IND_BLOCK;
		} else if(block < ((BLOCKS_PER_IND_BLOCK(i->sb) * BLOCKS_PER_IND_BLOCK(i->sb)) + BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
			level = EXT2_DIND_BLOCK;
		} else {
			level = EXT2_TIND_BLOCK;
		}
		block -= EXT2_NDIR_BLOCKS;
	}

	if(level < EXT2_NDIR_BLOCKS) {
		if(!i->u.ext2.i_data[block] && mode == FOR_WRITING) {
			if((newblock = ext2_balloc(i->sb)) < 0) {
				return -ENOSPC;
			}
			/* initialize the new block */
			if(!(buf = bread(i->dev, newblock, blksize))) {
				ext2_bfree(i->sb, newblock);
				return -EIO;
			}
			memset_b(buf->data, 0, blksize);
			bwrite(buf);
			i->u.ext2.i_data[block] = newblock;
			i->i_blocks += blksize / 512;
		}
		return i->u.ext2.i_data[block];
	}

	if(!i->u.ext2.i_data[level]) {
		if(mode == FOR_WRITING) {
			if((newblock = ext2_balloc(i->sb)) < 0) {
				return -ENOSPC;
			}
			/* initialize the new block */
			if(!(buf = bread(i->dev, newblock, blksize))) {
				ext2_bfree(i->sb, newblock);
				return -EIO;
			}
			memset_b(buf->data, 0, blksize);
			bwrite(buf);
			i->u.ext2.i_data[level] = newblock;
			i->i_blocks += blksize / 512;
		} else {
			return 0;
		}
	}
	if(!(buf = bread(i->dev, i->u.ext2.i_data[level], blksize))) {
		return -EIO;
	}
	indblock = (__blk_t *)buf->data;
	dblock = block - BLOCKS_PER_IND_BLOCK(i->sb);
	tblock = block - (BLOCKS_PER_IND_BLOCK(i->sb) * BLOCKS_PER_IND_BLOCK(i->sb)) - BLOCKS_PER_IND_BLOCK(i->sb);

	if(level == EXT2_DIND_BLOCK) {
		block = dblock / BLOCKS_PER_IND_BLOCK(i->sb);
	}
	if(level == EXT2_TIND_BLOCK) {
		block = tblock / (BLOCKS_PER_IND_BLOCK(i->sb) * BLOCKS_PER_IND_BLOCK(i->sb));
	}

	if(!indblock[block]) {
		if(mode == FOR_WRITING) {
			if((newblock = ext2_balloc(i->sb)) < 0) {
				brelse(buf);
				return -ENOSPC;
			}
			/* initialize the new block */
			if(!(buf2 = bread(i->dev, newblock, blksize))) {
				ext2_bfree(i->sb, newblock);
				brelse(buf);
				return -EIO;
			}
			memset_b(buf2->data, 0, blksize);
			bwrite(buf2);
			indblock[block] = newblock;
			i->i_blocks += blksize / 512;
			if(level == EXT2_IND_BLOCK) {
				bwrite(buf);
				return newblock;
			}
			buf->flags |= (BUFFER_DIRTY | BUFFER_VALID);
		} else {
			brelse(buf);
			return 0;
		}
	}
	if(level == EXT2_IND_BLOCK) {
		newblock = indblock[block];
		brelse(buf);
		return newblock;
	}

	if(level == EXT2_TIND_BLOCK) {
		if(!(buf3 = bread(i->dev, indblock[block], blksize))) {
			printk("%s(): returning -EIO\n", __FUNCTION__);
			brelse(buf);
			return -EIO;
		}
		tindblock = (__blk_t *)buf3->data;
		tblock -= BLOCKS_PER_DIND_BLOCK(i->sb) * block;
		block = tindblock[tblock / BLOCKS_PER_IND_BLOCK(i->sb)];
		if(!block) {
			if(mode == FOR_WRITING) {
				if((newblock = ext2_balloc(i->sb)) < 0) {
					brelse(buf);
					brelse(buf3);
					return -ENOSPC;
				}
				/* initialize the new block */
				if(!(buf4 = bread(i->dev, newblock, blksize))) {
					ext2_bfree(i->sb, newblock);
					brelse(buf);
					brelse(buf3);
					return -EIO;
				}
				memset_b(buf4->data, 0, blksize);
				bwrite(buf4);
				tindblock[tblock / BLOCKS_PER_IND_BLOCK(i->sb)] = newblock;
				i->i_blocks += blksize / 512;
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

	dindblock = (__blk_t *)buf2->data;
	block = dindblock[dblock - (iblock * BLOCKS_PER_IND_BLOCK(i->sb))];
	if(!block && mode == FOR_WRITING) {
		if((newblock = ext2_balloc(i->sb)) < 0) {
			brelse(buf);
			if(level == EXT2_TIND_BLOCK) {
				brelse(buf3);
			}
			brelse(buf2);
			return -ENOSPC;
		}
		/* initialize the new block */
		if(!(buf4 = bread(i->dev, newblock, blksize))) {
			ext2_bfree(i->sb, newblock);
			brelse(buf);
			if(level == EXT2_TIND_BLOCK) {
				brelse(buf3);
			}
			brelse(buf2);
			return -EIO;
		}
		memset_b(buf4->data, 0, blksize);
		bwrite(buf4);
		dindblock[dblock - (iblock * BLOCKS_PER_IND_BLOCK(i->sb))] = newblock;
		i->i_blocks += blksize / 512;
		buf2->flags |= (BUFFER_DIRTY | BUFFER_VALID);
		block = newblock;
	}
	brelse(buf);
	if(level == EXT2_TIND_BLOCK) {
		brelse(buf3);
	}
	brelse(buf2);
	return block;
}

int ext2_truncate(struct inode *i, __off_t length)
{
	__blk_t block, indblock, *dindblock;
	struct buffer *buf;
	int n, retval, blksize;

	blksize = i->sb->s_blocksize;
	block = length / blksize;

	if(!S_ISDIR(i->i_mode) && !S_ISREG(i->i_mode) && !S_ISLNK(i->i_mode)) {
		return -EINVAL;
	}

	if(block < EXT2_NDIR_BLOCKS) {
		for(n = block; n < EXT2_NDIR_BLOCKS; n++) {
			if(i->u.ext2.i_data[n]) {
				ext2_bfree(i->sb, i->u.ext2.i_data[n]);
				i->u.ext2.i_data[n] = 0;
				i->i_blocks -= blksize / 512;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
		if(block) {
			block -= EXT2_NDIR_BLOCKS;
		}
		if(i->u.ext2.i_data[EXT2_IND_BLOCK]) {
			if((retval = free_dblock(i, i->u.ext2.i_data[EXT2_IND_BLOCK], block)) < 0) {
				return retval;
			}
			if(!block) {
				ext2_bfree(i->sb, i->u.ext2.i_data[EXT2_IND_BLOCK]);
				i->u.ext2.i_data[EXT2_IND_BLOCK] = 0;
				i->i_blocks -= blksize / 512;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_DIND_BLOCK(i->sb) + BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
		if(block) {
			block -= EXT2_NDIR_BLOCKS;
			block -= BLOCKS_PER_IND_BLOCK(i->sb);
		}
		if(i->u.ext2.i_data[EXT2_DIND_BLOCK]) {
			if((retval = free_indblock(i, i->u.ext2.i_data[EXT2_DIND_BLOCK], block)) < 0) {
				return retval;
			}
			if(!block) {
				ext2_bfree(i->sb, i->u.ext2.i_data[EXT2_DIND_BLOCK]);
				i->u.ext2.i_data[EXT2_DIND_BLOCK] = 0;
				i->i_blocks -= blksize / 512;
			}
		}
		block = 0;
	}

	if(!block || block < (BLOCKS_PER_TIND_BLOCK(i->sb) + BLOCKS_PER_DIND_BLOCK(i->sb) + BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
		if(block) {
			block -= EXT2_NDIR_BLOCKS;
			block -= BLOCKS_PER_IND_BLOCK(i->sb);
			block -= BLOCKS_PER_DIND_BLOCK(i->sb);
		}
		if(i->u.ext2.i_data[EXT2_TIND_BLOCK]) {
			if(!(buf = bread(i->dev, i->u.ext2.i_data[EXT2_TIND_BLOCK], blksize))) {
				printk("%s(): error reading the triply indirect block (%d).\n", __FUNCTION__, i->u.ext2.i_data[EXT2_TIND_BLOCK]);
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
						ext2_bfree(i->sb, dindblock[n]);
						dindblock[n] = 0;
						i->i_blocks -= blksize / 512;
					}
				}
				indblock = 0;
			}
			bwrite(buf);
			if(!block) {
				ext2_bfree(i->sb, i->u.ext2.i_data[EXT2_TIND_BLOCK]);
				i->u.ext2.i_data[EXT2_TIND_BLOCK] = 0;
				i->i_blocks -= blksize / 512;
			}
		}
	}

	i->i_mtime = CURRENT_TIME;
	i->i_ctime = CURRENT_TIME;
	i->i_size = length;
	i->dirty = 1;

	return 0;
}
