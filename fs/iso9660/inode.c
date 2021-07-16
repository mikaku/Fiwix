/*
 * fiwix/fs/iso9660/inode.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/buffer.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static int read_pathtable(struct inode *i)
{
	int n, offset, pt_len, pt_blk;
	struct iso9660_sb_info *sbi;
	struct iso9660_pathtable_record *ptr;
	struct buffer *buf;

	sbi = (struct iso9660_sb_info *)&i->sb->u.iso9660;
	pt_len = isonum_733(sbi->sb->path_table_size);
	pt_blk = isonum_731(sbi->sb->type_l_path_table);

	if(pt_len > PAGE_SIZE) {
		printk("WARNING: %s(): path table record size (%d) > 4096, not supported yet.\n", __FUNCTION__, pt_len);
		return -EINVAL;
	}

	if(!(sbi->pathtable_raw = (void *)kmalloc())) {
		return -ENOMEM;
	}
	offset = 0;
	while(offset < pt_len) {
		if(!(buf = bread(i->dev, pt_blk++, BLKSIZE_2K))) {
			kfree((unsigned int)sbi->pathtable_raw);
			return -EIO;
		}
		memcpy_b(sbi->pathtable_raw + offset, (void *)buf->data, MIN(pt_len - offset, BLKSIZE_2K));
		offset += MIN(pt_len - offset, BLKSIZE_2K);
		brelse(buf);
	}

	/* allocate and count the number of records in the Path Table */
	offset = n = 0;
	if(!(sbi->pathtable = (struct iso9660_pathtable_record **)kmalloc())) {
		kfree((unsigned int)sbi->pathtable_raw);
		return -ENOMEM;
	}
	sbi->pathtable[n] = NULL;
	while(offset < pt_len) {
		ptr = (struct iso9660_pathtable_record *)(sbi->pathtable_raw + offset);
		sbi->pathtable[++n] = ptr;
		offset += sizeof(struct iso9660_pathtable_record) + isonum_711(ptr->length) + (isonum_711(ptr->length) & 1);
	}
	sbi->paths = n;

	return 0;
}

static int get_parent_dir_size(struct superblock *sb, __blk_t extent)
{
	int n;
	struct iso9660_pathtable_record *ptr;
	__blk_t parent;

	for(n = 0; n < sb->u.iso9660.paths; n++) {
		ptr = (struct iso9660_pathtable_record *)sb->u.iso9660.pathtable[n];
		if(isonum_731(ptr->extent) == extent) {
			
			parent = isonum_723(ptr->parent);
			ptr = (struct iso9660_pathtable_record *)sb->u.iso9660.pathtable[parent];
			parent = isonum_731(ptr->extent);
			return parent;
		}
	}
	printk("WARNING: %s(): unable to locate extent '%d' in path table.\n", __FUNCTION__, extent);
	return 0;
}

int iso9660_read_inode(struct inode *i)
{
	int errno;
	__u32 blksize;
	struct superblock *sb;
	struct iso9660_directory_record *d;
	struct buffer *buf;
	__blk_t dblock;
	__off_t doffset;

	sb = (struct superblock *)i->sb;
	if(!sb->u.iso9660.pathtable) {
		if((errno = read_pathtable(i))) {
			return errno;
		}
	}

	dblock = (i->inode & ~ISO9660_INODE_MASK) >> ISO9660_INODE_BITS;
	doffset = i->inode & ISO9660_INODE_MASK;
	blksize = i->sb->s_blocksize;

	/* FIXME: it only looks in one directory block */
	if(!(buf = bread(i->dev, dblock, blksize))) {
		return -EIO;
	}

	if(doffset >= blksize) {
		printk("WARNING: %s(): inode %d (dblock=%d, doffset=%d) not found in directory entry.\n", __FUNCTION__, i->inode, dblock, doffset);
		brelse(buf);
		return -EIO;
	}
	d = (struct iso9660_directory_record *)(buf->data + doffset);

	i->i_mode = S_IFREG;
	if((char)d->flags[0] & ISO9660_FILE_ISDIR) {
		i->i_mode = S_IFDIR;
	}
	if(!((char)d->flags[0] & ISO9660_FILE_HASOWNER)) {
		i->i_mode |= S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	}
	i->i_uid = 0;
	i->i_size = isonum_733(d->size);
	i->i_atime = isodate(d->date);
	i->i_ctime = isodate(d->date);
	i->i_mtime = isodate(d->date);
	i->i_gid = 0;
	i->i_nlink = 1;
	i->count = 1;

	i->u.iso9660.i_extent = isonum_733(d->extent);
	check_rrip_inode(d, i);
	brelse(buf);

	switch(i->i_mode & S_IFMT) {
		case S_IFCHR:
			i->fsop = &def_chr_fsop;
			break;
		case S_IFBLK:
			i->fsop = &def_blk_fsop;
			break;
		case S_IFIFO:
			i->fsop = &pipefs_fsop;
			/* it's a union so we need to clear pipefs_inode */
			memset_b(&i->u.pipefs, NULL, sizeof(struct pipefs_inode));
			break;
		case S_IFDIR:
			i->fsop = &iso9660_dir_fsop;
			i->i_nlink++;
			break;
		case S_IFREG:
			i->fsop = &iso9660_file_fsop;
			break;
		case S_IFLNK:
			i->fsop = &iso9660_symlink_fsop;
			break;
		case S_IFSOCK:
			i->fsop = NULL;
			break;
		default:
			PANIC("invalid inode (%d) mode %08o.\n", i->inode, i->i_mode);
	}
	return 0;
}

int iso9660_bmap(struct inode *i, __off_t offset, int mode)
{
	__blk_t block;

	block = i->u.iso9660.i_extent + (offset / i->sb->s_blocksize);
	return block;
}
