/*
 * fiwix/fs/iso9660/dir.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/stat.h>
#include <fiwix/dirent.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations iso9660_dir_fsop = {
	0,
	0,

	iso9660_dir_open,
	iso9660_dir_close,
	iso9660_dir_read,
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	iso9660_dir_readdir,
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	iso9660_bmap,
	iso9660_lookup,
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

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statsfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int iso9660_dir_open(struct inode *i, struct fd *fd_table)
{
	fd_table->offset = 0;
	return 0;
}

int iso9660_dir_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int iso9660_dir_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	return -EISDIR;
}

int iso9660_dir_readdir(struct inode *i, struct fd *fd_table, struct dirent *dirent, __size_t count)
{
	__blk_t block;
	unsigned int doffset, offset;
	unsigned int size, dirent_len;
	struct iso9660_directory_record *d;
	int base_dirent_len;
	int blksize;
	struct buffer *buf;
	int nm_len;
	char nm_name[NAME_MAX + 1];

	if(!(S_ISDIR(i->i_mode))) {
		return -EBADF;
	}

	blksize = i->sb->s_blocksize;
	if(fd_table->offset > i->i_size) {
		fd_table->offset = i->i_size;
	}

	base_dirent_len = sizeof(dirent->d_ino) + sizeof(dirent->d_off) + sizeof(dirent->d_reclen);
	doffset = size = 0;

	while(doffset < count) {
		if((block = bmap(i, fd_table->offset, FOR_READING)) < 0) {
			return block;
		}
		if(block) {
			if(!(buf = bread(i->dev, block, blksize))) {
				return -EIO;
			}

			doffset = fd_table->offset;
			offset = fd_table->offset % blksize;

			while(doffset < i->i_size && offset < blksize) {
				d = (struct iso9660_directory_record *)(buf->data + offset);
				if(isonum_711(d->length)) {
					dirent_len = (base_dirent_len + (isonum_711(d->name_len) + 1)) + 3;
					dirent_len &= ~3;	/* round up */
					if((size + dirent_len) < count) {
						dirent->d_ino = (block << ISO9660_INODE_BITS) + (doffset & ISO9660_INODE_MASK);
						dirent->d_off = doffset;
						dirent->d_reclen = dirent_len;
						if(isonum_711(d->name_len) == 1 && d->name[0] == 0) {
							dirent->d_name[0] = '.';
							dirent->d_name[1] = 0;
						} else if(isonum_711(d->name_len) == 1 && d->name[0] == 1) {
							dirent->d_name[0] = '.';
							dirent->d_name[1] = '.';
							dirent->d_name[2] = 0;
							dirent_len = 16;
							dirent->d_reclen = 16;
							if(i->u.iso9660.i_parent) {
								dirent->d_ino = i->u.iso9660.i_parent->inode;
							} else {
								dirent->d_ino = i->inode;
							}
						} else {
							nm_len = 0;
							if(i->sb->u.iso9660.rrip) {
								nm_len = get_rrip_filename(d, i, nm_name);
							}
							if(nm_len) {
								dirent->d_reclen = (base_dirent_len + nm_len + 1) + 3;
								dirent->d_reclen &= ~3;	/* round up */
								dirent_len = dirent->d_reclen;
								if((size + dirent_len) < count) {
									dirent->d_name[nm_len] = 0;
									memcpy_b(dirent->d_name, nm_name, nm_len);
								} else {
									break;
								}
							} else {
								memcpy_b(dirent->d_name, d->name, isonum_711(d->name_len));
								dirent->d_name[isonum_711(d->name_len)] = 0;
							}
						}
						if(!((char)d->flags[0] & ISO9660_FILE_ISDIR)) {
							iso9660_cleanfilename(dirent->d_name, isonum_711(d->name_len));
						}
						dirent = (struct dirent *)((char *)dirent + dirent_len);
						size += dirent_len;
					} else {
						break;
					}
					doffset += isonum_711(d->length);
					offset += isonum_711(d->length);
				} else {
					doffset &= ~(blksize - 1);
					doffset += blksize;
					break;
				}
			}
			brelse(buf);
		}
		fd_table->offset = doffset;
	}
	return size;
}
