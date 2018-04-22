/*
 * fiwix/fs/ext2/namei.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_ext2.h>
#include <fiwix/buffer.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int ext2_lookup(const char *name, struct inode *dir, struct inode **i_res)
{
	__blk_t block;
	unsigned int blksize;
	unsigned int offset, doffset;
	struct buffer *buf;
	struct ext2_dir_entry_2 *d;
	__ino_t inode;

	blksize = dir->sb->s_blocksize;
	inode = offset = 0;
	dir->count++;

	while(offset < dir->i_size && !inode) {
		if((block = bmap(dir, offset, FOR_READING)) < 0) {
			return block;
		}
		if(block) {
			if(!(buf = bread(dir->dev, block, blksize))) {
				return -EIO;
			}
			doffset = 0;
			do {
				d = (struct ext2_dir_entry_2 *)(buf->data + doffset);
				if(d->inode) {
					if(d->name_len == strlen(name)) {
						if(strncmp(d->name, name, d->name_len) == 0) {
							inode = d->inode;
						}
					}
					doffset += d->rec_len;
				} else {
					doffset += sizeof(struct ext2_dir_entry_2);
				}
			} while((doffset < blksize) && (!inode));

			brelse(buf);
			offset += blksize;
			if(inode) {
				/*
				 * This prevents a deadlock in iget() when
				 * trying to lock '.' when 'dir' is the same
				 * directory (ls -lai <dir>).
				 */
				if(inode == dir->inode) {
					*i_res = dir;
					return 0;
				}

				if(!(*i_res = iget(dir->sb, inode))) {
					return -EACCES;
				}
				iput(dir);
				return 0;
			}
		} else {
			break;
		}
	}
	iput(dir);
	return -ENOENT;
}
