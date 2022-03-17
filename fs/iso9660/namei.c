/*
 * fiwix/fs/iso9660/namei.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/buffer.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int iso9660_lookup(const char *name, struct inode *dir, struct inode **i_res)
{
	__blk_t dblock;
	__u32 blksize;
	int len, dnlen;
	unsigned int offset, doffset;
	struct buffer *buf;
	struct iso9660_directory_record *d;
	__ino_t inode;
	int nm_len;
	char *nm_name;

	blksize = dir->sb->s_blocksize;
	inode = offset = 0;
	len = strlen(name);

	while(offset < dir->i_size && !inode) {
		if((dblock = bmap(dir, offset, FOR_READING)) < 0) {
			return dblock;
		}
		if(dblock) {
			if(!(buf = bread(dir->dev, dblock, blksize))) {
				return -EIO;
			}
			doffset = 0;
			do {
				d = (struct iso9660_directory_record *)(buf->data + doffset);
				if(isonum_711(d->length) == 0) {
					break;
				}
				if(len == 1) {
					if(name[0] == '.' && name[1] == '\0') {
						if(isonum_711(d->name_len) == 1 && d->name[0] == 0) {
							inode = dir->inode;
						}
					}
				}
				if(len == 2) {
					if(name[0] == '.' && name[1] == '.' && name[2] == '\0') {
						if(isonum_711(d->name_len) == 1 && d->name[0] == 1) {
							inode = dir->u.iso9660.i_parent->inode;
						}
					}
				}
				if(!(nm_name = (char *)kmalloc())) {
					return -ENOMEM;
				}
				nm_len = 0;
				if(dir->sb->u.iso9660.rrip) {
					nm_len = get_rrip_filename(d, dir, nm_name);
				}
				if(nm_len) {
					dnlen = nm_len;
				} else {
					dnlen = isonum_711(d->name_len);
					if(!((char)d->flags[0] & ISO9660_FILE_ISDIR)) {
						iso9660_cleanfilename(d->name, dnlen);
						dnlen = strlen(d->name);
					}
				}
				if(len == dnlen) {
					if(nm_len) {
						if(strncmp(nm_name, name, dnlen) == 0) {
							inode = (dblock << ISO9660_INODE_BITS) + (doffset & ISO9660_INODE_MASK);
						}
					} else {
						if(strncmp(d->name, name, dnlen) == 0) {
							inode = (dblock << ISO9660_INODE_BITS) + (doffset & ISO9660_INODE_MASK);
						}
					}
				}
				kfree((unsigned int)nm_name);
				doffset += isonum_711(d->length);
			} while((doffset < blksize) && (!inode));
			brelse(buf);
			offset += blksize;
			if(inode) {
				/*
				 * This prevents a deadlock in iget() when
				 * trying to lock '.' when 'dir' is the same
				 * directory (ls -lai <tmp>).
				 */
				if(inode == dir->inode) {
					*i_res = dir;
					return 0;
				}

				if(!(*i_res = iget(dir->sb, inode))) {
					return -EACCES;
				}
				if(S_ISDIR((*i_res)->i_mode)) {
					if(!(*i_res)->u.iso9660.i_parent) {
						(*i_res)->u.iso9660.i_parent = dir;
					}
				}
				iput(dir);
				return 0;
			}
		}
	}
	iput(dir);
	return -ENOENT;
}
