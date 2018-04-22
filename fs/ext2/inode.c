/*
 * fiwix/fs/ext2/inode.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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

static int get_group_desc(struct inode *i, struct superblock *sb, struct ext2_group_desc *gd)
{
	int block_group;
	int desc_per_block, group_desc_block, group_desc;
	struct buffer *buf;

	block_group = ((i->inode - 1) / sb->u.ext2.s_inodes_per_group);
	desc_per_block = sb->s_blocksize / sizeof(struct ext2_group_desc);
	group_desc_block = block_group / desc_per_block;
	group_desc = block_group % desc_per_block;
	if(!(buf = bread(i->dev, SUPERBLOCK + sb->u.ext2.s_first_data_block + group_desc_block, i->sb->s_blocksize))) {
		return -EIO;
	}
	memcpy_b(gd, (void *)(buf->data + (group_desc * sizeof(struct ext2_group_desc))), sizeof(struct ext2_group_desc));
	brelse(buf);
	return 0;
}

int ext2_read_inode(struct inode *i)
{
	__ino_t block;
	unsigned int offset;
	struct superblock *sb;
	struct ext2_inode *ii;
	struct ext2_group_desc gd;
	struct buffer *buf;

	if(!(sb = get_superblock(i->dev))) {
		printk("WARNING: %s(): get_superblock() has returned NULL.\n");
		return -EINVAL;
	}
	if(get_group_desc(i, sb, &gd)) {
		return -EIO;
	}
	block = (((i->inode - 1) % sb->u.ext2.s_inodes_per_group) / EXT2_INODES_PER_BLOCK(sb));

	if(!(buf = bread(i->dev, gd.bg_inode_table + block, i->sb->s_blocksize))) {
		return -EIO;
	}
	offset = ((((i->inode - 1) % sb->u.ext2.s_inodes_per_group) % EXT2_INODES_PER_BLOCK(sb)) * sizeof(struct ext2_inode));

	ii = (struct ext2_inode *)(buf->data + offset);
	memcpy_b(&i->u.ext2.i_block, ii->i_block, sizeof(ii->i_block));

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
			memset_b(&i->u.pipefs, NULL, sizeof(struct pipefs_inode));
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

int ext2_bmap(struct inode *i, __off_t offset, int mode)
{
	unsigned char level;
	short int dind_block;
	__blk_t *indblock;
	__blk_t *dindblock;
	__blk_t block;
	struct buffer *buf;

	block = offset / i->sb->s_blocksize;
	level = 0;

	if(block < EXT2_NDIR_BLOCKS) {
		level = EXT2_NDIR_BLOCKS - 1;
	} else {
		if(block < (BLOCKS_PER_IND_BLOCK(i->sb) + EXT2_NDIR_BLOCKS)) {
			level = EXT2_IND_BLOCK;
			block -= EXT2_NDIR_BLOCKS;
		} else {
			if(block < BLOCKS_PER_DIND_BLOCK(i->sb)) {
				level = EXT2_DIND_BLOCK;
				block -= EXT2_NDIR_BLOCKS;
				block -= BLOCKS_PER_IND_BLOCK(i->sb);
			} else {
				level = EXT2_TIND_BLOCK;
				block = 0;
			}
		}
	}

	if(level == EXT2_TIND_BLOCK) {
		printk("(level = %d) (offset = %d) (block = %d)\n", level, offset, block);
		printk("WARNING: triple-indirect blocks are not supported!\n");
		return -EINVAL;
	}

	if(level < EXT2_NDIR_BLOCKS) {
		return i->u.ext2.i_block[block];
	}

	if(!(indblock = (void *)kmalloc())) {
		printk("%s(): returning -ENOMEM.\n", __FUNCTION__);
		return -ENOMEM;
	}
	if(i->u.ext2.i_block[level] == 0) {
		printk("WARNING: %s(): will return 0 as an indirect block request! (inode %d).\n", __FUNCTION__, i->inode);
		kfree((unsigned int)indblock);
		return 0;
	}
	if(!(buf = bread(i->dev, i->u.ext2.i_block[level], i->sb->s_blocksize))) {
		kfree((unsigned int)indblock);
		printk("%s(): returning -EIO.\n", __FUNCTION__);
		return -EIO;
	}
	memcpy_l(indblock, buf->data, BLOCKS_PER_IND_BLOCK(i->sb));
	brelse(buf);

	if(level == EXT2_IND_BLOCK) {
		kfree((unsigned int)indblock);
		return indblock[block];
	}

	if(!(dindblock = (void *)kmalloc())) {
		kfree((unsigned int)indblock);
		printk("%s(): returning -ENOMEM.\n", __FUNCTION__);
		return -ENOMEM;
	}
	dind_block = block / BLOCKS_PER_IND_BLOCK(i->sb);
	if(!(buf = bread(i->dev, indblock[dind_block], i->sb->s_blocksize))) {
		kfree((unsigned int)indblock);
		kfree((unsigned int)dindblock);
		printk("%s(): returning -EIO.\n", __FUNCTION__);
		return -EIO;
	}
	memcpy_l(dindblock, buf->data, BLOCKS_PER_IND_BLOCK(i->sb));
	brelse(buf);
	block = dindblock[block - (dind_block * BLOCKS_PER_IND_BLOCK(i->sb))];
	kfree((unsigned int)indblock);
	kfree((unsigned int)dindblock);
	return block;
}
