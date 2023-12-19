/*
 * fiwix/fs/iso9660/super.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/buffer.h>
#include <fiwix/time.h>
#include <fiwix/sched.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations iso9660_fsop = {
	FSOP_REQUIRES_DEV,
	0,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
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

	iso9660_read_inode,
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	iso9660_statfs,
	iso9660_read_superblock,
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	iso9660_release_superblock
};

int isonum_711(char *str)
{
	unsigned char *le;

	le = (unsigned char *)str;
	return le[0];
}

/* return a 16bit little-endian number */
int isonum_723(char *str)
{
	unsigned char *le;

	le = (unsigned char *)str;
	return le[0] | (le[1] << 8);
}

/* return a 32bit little-endian number */
int isonum_731(char *str)
{
	unsigned char *le;

	le = (unsigned char *)str;
	return le[0] | (le[1] << 8) | (le[2] << 16) | (le[3] << 24);
}

/* return a 32bit little-endian number */
int isonum_733(char *p)
{
	return isonum_731(p);
}

/* return a date and time format */
unsigned int isodate(const char *p)
{
	struct mt mt;

	if(!p[0]) {
		return 0;
	}

	mt.mt_sec = p[5];
	mt.mt_min = p[4];
	mt.mt_hour = p[3];
	mt.mt_day = p[2];
	mt.mt_month = p[1];
	mt.mt_year = p[0];
	mt.mt_year += 1900;
	mt.mt_min += p[6] * 15;

	return mktime(&mt);
}

/* return a clean filename */
int iso9660_cleanfilename(char *filename, int len)
{
	int n;
	char *p;

	p = filename;
	if(len > 2) {
		for(n = 0; n < len; n++) {
			if((len - n) == 2) {
				if(p[n] == ';' && p[n + 1] == '1') {
					filename[n] = 0;
					if(p[n - 1] == '.') {
						filename[n - 1] = 0;
					}
					return 0;
				}
			}
		}
	}
	return 1;
}

void iso9660_statfs(struct superblock *sb, struct statfs *statfsbuf)
{
	statfsbuf->f_type = ISO9660_SUPER_MAGIC;
	statfsbuf->f_bsize = sb->s_blocksize;
	statfsbuf->f_blocks = isonum_733(sb->u.iso9660.sb->volume_space_size);
	statfsbuf->f_bfree = 0;
	statfsbuf->f_bavail = 0;
	statfsbuf->f_files = 0;		/* FIXME */
	statfsbuf->f_ffree = 0;
	/* statfsbuf->f_fsid = ? */
	statfsbuf->f_namelen = NAME_MAX;
}

int iso9660_read_superblock(__dev_t dev, struct superblock *sb)
{
	struct buffer *buf;
	struct iso9660_super_block *iso9660sb;
	struct iso9660_super_block *pvd;
	struct iso9660_directory_record *dr;
	__ino_t root_inode;
	int n;

	superblock_lock(sb);
	pvd = NULL;

	for(n = 0; n < ISO9660_MAX_VD; n++) {
		if(!(buf = bread(dev, ISO9660_SUPERBLOCK + n, BLKSIZE_2K))) {
			superblock_unlock(sb);
			return -EIO;
		}

		iso9660sb = (struct iso9660_super_block *)buf->data;
		if(strncmp(iso9660sb->id, ISO9660_STANDARD_ID, sizeof(iso9660sb->id)) || (isonum_711(iso9660sb->type) == ISO9660_VD_END)) {
			break;
		}
		if(isonum_711(iso9660sb->type) == ISO9660_VD_PRIMARY) {
			pvd = (struct iso9660_super_block *)buf->data;
			break;
		}
		brelse(buf);
	}
	if(!pvd) {
		printk("WARNING: %s(): invalid filesystem type or bad superblock on device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		superblock_unlock(sb);
		brelse(buf);
		return -EINVAL;
	}

	dr = (struct iso9660_directory_record *)pvd->root_directory_record;
	root_inode = isonum_711(dr->extent);

	sb->dev = dev;
	sb->fsop = &iso9660_fsop;
	sb->flags = MS_RDONLY;
	sb->s_blocksize = isonum_723(pvd->logical_block_size);
	sb->u.iso9660.rrip = 0;
	if(!(sb->u.iso9660.sb = (struct iso9660_super_block *)kmalloc(sizeof(struct iso9660_super_block)))) {
		superblock_unlock(sb);
		brelse(buf);
		return -ENOMEM;
	}
	memcpy_b(sb->u.iso9660.sb, pvd, sizeof(struct iso9660_super_block));
	brelse(buf);

	root_inode = (root_inode << ISO9660_INODE_BITS) + (0 & ISO9660_INODE_MASK);
	if(!(sb->root = iget(sb, root_inode))) {
		printk("WARNING: %s(): unable to get root inode.\n", __FUNCTION__);
		superblock_unlock(sb);
		return -EINVAL;
	}
	sb->u.iso9660.s_root_inode = root_inode;

	superblock_unlock(sb);
	return 0;
}

void iso9660_release_superblock(struct superblock *sb)
{
	kfree((unsigned int)sb->u.iso9660.sb);
	kfree((unsigned int)sb->u.iso9660.pathtable);
	kfree((unsigned int)sb->u.iso9660.pathtable_raw);
}

int iso9660_init(void)
{
	return register_filesystem("iso9660", &iso9660_fsop);
}
