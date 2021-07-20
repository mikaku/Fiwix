/*
 * fiwix/fs/ext2/namei.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_ext2.h>
#include <fiwix/buffer.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stat.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/* finds the entry 'name' with (optionally) inode 'i' in the directory 'dir' */
static struct buffer * find_dir_entry(struct inode *dir, struct inode *i, struct ext2_dir_entry_2 **d_res, char *name)
{
	__blk_t block;
	unsigned int blksize;
	unsigned int offset, doffset;
	struct buffer *buf;
	int basesize, rlen, nlen;

	basesize = sizeof((*d_res)->inode) + sizeof((*d_res)->rec_len) + sizeof((*d_res)->name_len) + sizeof((*d_res)->file_type);
	blksize = dir->sb->s_blocksize;
	offset = 0;

	/* nlen is the length of the new entry, used when searching for the first usable entry */
	nlen = basesize + strlen(name) + 3;
	nlen &= ~3;

	while(offset < dir->i_size) {
		if((block = bmap(dir, offset, FOR_READING)) < 0) {
			break;
		}
		if(block) {
			if(!(buf = bread(dir->dev, block, blksize))) {
				break;
			}
			doffset = 0;
			do {
				*d_res = (struct ext2_dir_entry_2 *)(buf->data + doffset);
				if(!i) {
					/* calculates the real length of the current entry */
					rlen = basesize + strlen((*d_res)->name) + 3;
					rlen &= ~3;
					/* returns the first entry where *name can fit in */
					if(!(*d_res)->inode) {
						if(nlen <= (*d_res)->rec_len) {
							return buf;
						}
					} else {
						if(rlen + nlen <= (*d_res)->rec_len) {
							int nrec_len;

							nrec_len = (*d_res)->rec_len - rlen;
							(*d_res)->rec_len = rlen;
							doffset += (*d_res)->rec_len;
							*d_res = (struct ext2_dir_entry_2 *)(buf->data + doffset);
							(*d_res)->rec_len = nrec_len;
							return buf;
						}
					}
					doffset += (*d_res)->rec_len;
				} else {
					if((*d_res)->inode == i->inode) {
						/* returns the first matching inode */
						if(!name) {
							return buf;
						}
						/* returns the matching inode and name */
						if((*d_res)->name_len == strlen(name)) {
							if(!strncmp((*d_res)->name, name, (*d_res)->name_len)) {
								return buf;
							}
						}
					}
					doffset += (*d_res)->rec_len;
				}
			} while(doffset < blksize);
			brelse(buf);
			offset += blksize;
		} else {
			break;
		}
	}

	*d_res = NULL;
	return NULL;
}

static struct buffer * add_dir_entry(struct inode *dir, struct ext2_dir_entry_2 **d_res, char *name)
{
	__blk_t block;
	struct buffer *buf;

	if(!(buf = find_dir_entry(dir, NULL, d_res, name))) {
		if((block = bmap(dir, dir->i_size, FOR_WRITING)) < 0) {
			return NULL;
		}
		if(!(buf = bread(dir->dev, block, dir->sb->s_blocksize))) {
			return NULL;
		}
		*d_res = (struct ext2_dir_entry_2 *)buf->data;
		dir->i_size += dir->sb->s_blocksize;
		(*d_res)->rec_len = dir->sb->s_blocksize;
	}

	return buf;
}

static int is_dir_empty(struct inode *dir)
{
	__blk_t block;
	unsigned int blksize;
	unsigned int offset, doffset;
	struct buffer *buf;
	struct ext2_dir_entry_2 *d;

	blksize = dir->sb->s_blocksize;
	offset = 0;

	while(offset < dir->i_size) {
		if((block = bmap(dir, offset, FOR_READING)) < 0) {
			break;
		}
		if(block) {
			if(!(buf = bread(dir->dev, block, blksize))) {
				break;
			}
			doffset = 0;
			do {
				if(doffset + offset >= dir->i_size) {
					break;
				}
				d = (struct ext2_dir_entry_2 *)(buf->data + doffset);
				doffset += d->rec_len;
				if(d->inode && d->name_len == 1 && d->name[0] == '.') {
					continue;
				}
				if(d->inode && d->name_len == 2 && d->name[0] == '.' && d->name[1] == '.') {
					continue;
				}
				if(d->inode) {
					brelse(buf);
					return 0;
				}
			} while(doffset < blksize);
			brelse(buf);
			offset += blksize;
		} else {
			break;
		}
	}

	return 1;
}

static int is_subdir(struct inode *dir_new, struct inode *i_old)
{
	__ino_t inode;
	int errno;

	errno = 0;
	dir_new->count++;
	for(;;) {
		if(dir_new == i_old) {
			errno = 1;
			break;
		}
		inode = dir_new->inode;
		if(ext2_lookup("..", dir_new, &dir_new)) {
			break;
		}
		if(dir_new->inode == inode) {
			break;
		}
	}
	iput(dir_new);
	return errno;
}

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

	while(offset < dir->i_size && !inode) {
		if((block = bmap(dir, offset, FOR_READING)) < 0) {
			return block;
		}
		if(block) {
			if(!(buf = bread(dir->dev, block, blksize))) {
				iput(dir);
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
				}
				doffset += d->rec_len;
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
					iput(dir);
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

int ext2_rmdir(struct inode *dir, struct inode *i)
{
	struct buffer *buf;
	struct ext2_dir_entry_2 *d;

	inode_lock(i);

	if(!is_dir_empty(i)) {
		inode_unlock(i);
		return -ENOTEMPTY;
	}

	inode_lock(dir);

	if(!(buf = find_dir_entry(dir, i, &d, NULL))) {
		inode_unlock(i);
		inode_unlock(dir);
		return -ENOENT;
	}

	d->inode = 0;
	i->i_nlink = 0;
	dir->i_nlink--;

	i->i_ctime = CURRENT_TIME;
	i->u.ext2.i_dtime = CURRENT_TIME;
	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;

	i->dirty = 1;
	dir->dirty = 1;

	bwrite(buf);

	inode_unlock(i);
	inode_unlock(dir);
	return 0;
}

int ext2_link(struct inode *i_old, struct inode *dir_new, char *name)
{
	struct buffer *buf;
	struct ext2_dir_entry_2 *d;
	char c;
	int n;

	inode_lock(i_old);
	inode_lock(dir_new);

	if(!(buf = add_dir_entry(dir_new, &d, name))) {
		inode_unlock(i_old);
		inode_unlock(dir_new);
		return -ENOSPC;
	}

	d->inode = i_old->inode;
	d->name_len = strlen(name);
	/* strcpy() can't be used here because it places a trailing NULL */
	for(n = 0; n < NAME_MAX; n++) {
		if((c = name[n])) {
			d->name[n] = c;
			continue;
		}
		break;
	}
	d->file_type = 0;	/* not used */

	i_old->i_nlink++;
	i_old->i_ctime = CURRENT_TIME;
	dir_new->i_mtime = CURRENT_TIME;
	dir_new->i_ctime = CURRENT_TIME;

	i_old->dirty = 1;
	dir_new->dirty = 1;

	bwrite(buf);

	inode_unlock(i_old);
	inode_unlock(dir_new);
	return 0;
}

int ext2_unlink(struct inode *dir, struct inode *i, char *name)
{
	struct buffer *buf;
	struct ext2_dir_entry_2 *d;

	inode_lock(dir);
	inode_lock(i);

	if(!(buf = find_dir_entry(dir, i, &d, name))) {
		inode_unlock(dir);
		inode_unlock(i);
		return -ENOENT;
	}

	/*
	 * FIXME: in order to avoid low performance when traversing large
	 * directories plenty of blank entries, it would be interesting
	 * to merge every removed entry with the previous entry.
	 */
	d->inode = 0;
	if(!--i->i_nlink) {
		i->u.ext2.i_dtime = CURRENT_TIME;
	}

	i->i_ctime = CURRENT_TIME;
	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;

	i->dirty = 1;
	dir->dirty = 1;

	bwrite(buf);

	inode_unlock(dir);
	inode_unlock(i);
	return 0;
}

int ext2_symlink(struct inode *dir, char *name, char *oldname)
{
	struct buffer *buf, *buf2;
	struct inode *i;
	struct ext2_dir_entry_2 *d;
	__blk_t block;
	char c, *data;
	int n;

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, S_IFLNK))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	if(!(buf = add_dir_entry(dir, &d, name))) {
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	i->i_mode = S_IFLNK | (S_IRWXU | S_IRWXG | S_IRWXO);
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->dev = dir->dev;
	i->count = 1;
	i->fsop = &ext2_symlink_fsop;

	if(strlen(oldname) >= EXT2_N_BLOCKS * sizeof(__u32)) {
		/* this will be a slow symlink */
		if((block = ext2_balloc(dir->sb)) < 0) {
			iput(i);
			brelse(buf);
			inode_unlock(dir);
			return block;
		}
		if(!(buf2 = bread(dir->dev, block, dir->sb->s_blocksize))) {
			iput(i);
			brelse(buf);
			ext2_bfree(dir->sb, block);
			inode_unlock(dir);
			return -EIO;
		}
		i->u.ext2.i_data[0] = block;
		for(n = 0; n < NAME_MAX; n++) {
			if((c = oldname[n])) {
				buf2->data[n] = c;
				continue;
			}
			break;
		}
		buf2->data[n] = 0;
		i->i_blocks = dir->sb->s_blocksize / 512;
		bwrite(buf2);
	} else {
		/* this will be a fast symlink */
		data = (char *)i->u.ext2.i_data;
		for(n = 0; n < NAME_MAX; n++) {
			if((c = oldname[n])) {
				data[n] = c;
				continue;
			}
			break;
		}
		data[n] = 0;
	}

	i->i_size = n;
	i->dirty = 1;
	i->i_nlink = 1;
	d->inode = i->inode;
	d->name_len = strlen(name);
	/* strcpy() can't be used here because it places a trailing NULL */
	for(n = 0; n < NAME_MAX; n++) {
		if((c = name[n])) {
			d->name[n] = c;
			continue;
		}
		break;
	}
	d->file_type = 0;	/* EXT2_FT_SYMLINK not used */

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->dirty = 1;

	bwrite(buf);
	iput(i);
	inode_unlock(dir);
	return 0;
}

int ext2_mkdir(struct inode *dir, char *name, __mode_t mode)
{
	struct buffer *buf, *buf2;
	struct inode *i;
	struct ext2_dir_entry_2 *d, *d2;
	__blk_t block;
	char c;
	int n;

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, S_IFDIR))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	i->i_mode = ((mode & (S_IRWXU | S_IRWXG | S_IRWXO)) & ~current->umask);
	i->i_mode |= S_IFDIR;
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->dev = dir->dev;
	i->count = 1;
	i->fsop = &ext2_dir_fsop;

	if((block = bmap(i, 0, FOR_WRITING)) < 0) {
		iput(i);
		inode_unlock(dir);
		return block;
	}

	if(!(buf2 = bread(i->dev, block, dir->sb->s_blocksize))) {
		ext2_bfree(dir->sb, block);
		iput(i);
		inode_unlock(dir);
		return -EIO;
	}

	if(!(buf = add_dir_entry(dir, &d, name))) {
		ext2_bfree(dir->sb, block);
		iput(i);
		brelse(buf2);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	d->name_len = strlen(name);
	/* strcpy() can't be used here because it places a trailing NULL */
	for(n = 0; n < NAME_MAX; n++) {
		if((c = name[n])) {
			if(c != '/') {
				d->name[n] = c;
				continue;
			}
		}
		break;
	}
	d->file_type = 0;	/* EXT2_FT_DIR not used */

	d2 = (struct ext2_dir_entry_2 *)buf2->data;
	d2->inode = i->inode;
	d2->name[0] = '.';
	d2->name[1] = 0;
	d2->name_len = 1;
	d2->rec_len = 12;
	d2->file_type = 0;	/* EXT2_FT_DIR not used */
	i->i_nlink = 1;
	d2 = (struct ext2_dir_entry_2 *)(buf2->data + 12);
	d2->inode = dir->inode;
	d2->name[0] = '.';
	d2->name[1] = '.';
	d2->name[2] = 0;
	d2->name_len = 2;
	d2->rec_len = i->sb->s_blocksize - 12;
	d2->file_type = 0;	/* EXT2_FT_DIR not used */
	i->i_nlink++;
	i->i_size = i->sb->s_blocksize;
	i->i_blocks = dir->sb->s_blocksize / 512;
	i->dirty = 1;

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->i_nlink++;
	dir->dirty = 1;

	bwrite(buf);
	bwrite(buf2);
	iput(i);
	inode_unlock(dir);
	return 0;
}

int ext2_mknod(struct inode *dir, char *name, __mode_t mode, __dev_t dev)
{
	struct buffer *buf;
	struct inode *i;
	struct ext2_dir_entry_2 *d;
	char c;
	int n;

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, mode & S_IFMT))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	if(!(buf = add_dir_entry(dir, &d, name))) {
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	d->name_len = strlen(name);
	/* strcpy() can't be used here because it places a trailing NULL */
	for(n = 0; n < NAME_MAX; n++) {
		if((c = name[n])) {
			d->name[n] = c;
			continue;
		}
		break;
	}

	i->i_mode = (mode & ~current->umask) & ~S_IFMT;
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->i_nlink = 1;
	i->dev = dir->dev;
	i->count = 1;
	i->dirty = 1;

	switch(mode & S_IFMT) {
		case S_IFCHR:
			i->fsop = &def_chr_fsop;
			i->rdev = dev;
			i->i_mode |= S_IFCHR;
			d->file_type = 0;	/* EXT2_FT_CHRDEV not used */
			break;
		case S_IFBLK:
			i->fsop = &def_blk_fsop;
			i->rdev = dev;
			i->i_mode |= S_IFBLK;
			d->file_type = 0;	/* EXT2_FT_BLKDEV not used */
			break;
		case S_IFIFO:
			i->fsop = &pipefs_fsop;
			i->i_mode |= S_IFIFO;
			/* it's a union so we need to clear pipefs_i */
			memset_b(&i->u.pipefs, NULL, sizeof(struct pipefs_inode));
			d->file_type = 0;	/* EXT2_FT_FIFO not used */
			break;
	}

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->dirty = 1;

	bwrite(buf);
	iput(i);
	inode_unlock(dir);
	return 0;
}

int ext2_create(struct inode *dir, char *name, __mode_t mode, struct inode **i_res)
{
	struct buffer *buf;
	struct inode *i;
	struct ext2_dir_entry_2 *d;
	char c;
	int n;

	if(IS_RDONLY_FS(dir)) {
		return -EROFS;
	}

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, S_IFREG))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	if(!(buf = add_dir_entry(dir, &d, name))) {
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	d->name_len = strlen(name);
	/* strcpy() can't be used here because it places a trailing NULL */
	for(n = 0; n < NAME_MAX; n++) {
		if((c = name[n])) {
			d->name[n] = c;
			continue;
		}
		break;
	}
	d->file_type = 0;	/* EXT2_FT_REG_FILE not used */

	i->i_mode = (mode & ~current->umask) & ~S_IFMT;
	i->i_mode |= S_IFREG;
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->i_nlink = 1;
	i->i_blocks = 0;
	i->dev = dir->dev;
	i->fsop = &ext2_file_fsop;
	i->count = 1;
	i->dirty = 1;

	i->u.ext2.i_dtime = 0;

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->dirty = 1;

	*i_res = i;
	bwrite(buf);
	inode_unlock(dir);
	return 0;
}

int ext2_rename(struct inode *i_old, struct inode *dir_old, struct inode *i_new, struct inode *dir_new, char *oldpath, char *newpath)
{
	struct buffer *buf_old, *buf_new;
	struct ext2_dir_entry_2 *d_old, *d_new;
	char c;
	int n, errno;

	errno = 0;

	if(is_subdir(dir_new, i_old)) {
		return -EINVAL;
	}

	inode_lock(i_old);
	inode_lock(dir_old);
	if(dir_old != dir_new) {
		inode_lock(dir_new);
	}

	if(!(buf_old = find_dir_entry(dir_old, i_old, &d_old, oldpath))) {
		errno = -ENOENT;
		goto end;
	}
	if(dir_old == dir_new) {
		/* free that buffer now to not block buf_new */
		brelse(buf_old);
		buf_old = NULL;
	}

	if(i_new) {
		if(S_ISDIR(i_old->i_mode)) {
			if(!is_dir_empty(i_new)) {
				if(buf_old) {
					brelse(buf_old);
				}
				errno = -ENOTEMPTY;
				goto end;
			}
		}
		if(!(buf_new = find_dir_entry(dir_new, i_new, &d_new, newpath))) {
			if(buf_old) {
				brelse(buf_old);
			}
			errno = -ENOENT;
			goto end;
		}
	} else {
		if(!(buf_new = add_dir_entry(dir_new, &d_new, newpath))) {
			if(buf_old) {
				brelse(buf_old);
			}
			errno = -ENOSPC;
			goto end;
		}
	}
	if(i_new) {
		i_new->i_nlink--;
	} else {
		i_new = i_old;
		d_new->name_len = strlen(newpath);
		/* strcpy() can't be used here because it places a trailing NULL */
		for(n = 0; n < NAME_MAX; n++) {
			if((c = newpath[n])) {
				d_new->name[n] = c;
				continue;
			}
			break;
		}
	}

	d_new->inode = i_old->inode;
	dir_new->i_mtime = CURRENT_TIME;
	dir_new->i_ctime = CURRENT_TIME;
	i_new->dirty = 1;
	dir_new->dirty = 1;

	dir_old->i_mtime = CURRENT_TIME;
	dir_old->i_ctime = CURRENT_TIME;
	i_old->dirty = 1;
	dir_old->dirty = 1;
	bwrite(buf_new);

	if(!buf_old) {
		if(!(buf_old = find_dir_entry(dir_old, i_old, &d_old, oldpath))) {
			errno = -ENOENT;
			goto end;
		}
	}
	d_old->inode = 0;
	bwrite(buf_old);

end:
	inode_unlock(i_old);
	inode_unlock(dir_old);
	inode_unlock(dir_new);
	return errno;
}
