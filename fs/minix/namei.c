/*
 * fiwix/fs/minix/namei.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_minix.h>
#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/stat.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static int is_dir_empty(struct inode *dir)
{
	__blk_t block;
	unsigned int blksize;
	unsigned int offset, doffset;
	struct buffer *buf;
	struct minix_dir_entry *d;

	blksize = dir->sb->s_blocksize;
	doffset = dir->sb->u.minix.dirsize * 2;	/* accept only "." and ".." */
	offset = 0;

	while(offset < dir->i_size) {
		if((block = bmap(dir, offset, FOR_READING)) < 0) {
			break;
		}
		if(block) {
			if(!(buf = bread(dir->dev, block, blksize))) {
				break;
			}
			do {
				if(doffset + offset >= dir->i_size) {
					break;
				}
				d = (struct minix_dir_entry *)(buf->data + doffset);
				if(d->inode) {
					brelse(buf);
					return 0;
				}
				doffset += dir->sb->u.minix.dirsize;
			} while(doffset < blksize);
			brelse(buf);
			offset += blksize;
			doffset = 0;
		} else {
			break;
		}
	}

	return 1;
}

/* finds the entry 'name' with inode 'i' in the directory 'dir' */
static struct buffer * find_dir_entry(struct inode *dir, struct inode *i, struct minix_dir_entry **d_res, char *name)
{
	__blk_t block;
	unsigned int blksize;
	unsigned int offset, doffset;
	struct buffer *buf;

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
				*d_res = (struct minix_dir_entry *)(buf->data + doffset);
				if(!i) {
					/* returns the first empty entry */
					if(!(*d_res)->inode || (doffset + offset >= dir->i_size)) {
						/* the directory grows by directory entry size */
						if(doffset + offset >= dir->i_size) {
							dir->i_size += dir->sb->u.minix.dirsize;
						}
						return buf;
					}
				} else {
					if((*d_res)->inode == i->inode) {
						/* returns the first matching inode */
						if(!name) {
							return buf;
						}
						/* returns the matching inode and name */
						if(!strcmp((*d_res)->name, name)) {
							return buf;
						}
					}
				}
				doffset += dir->sb->u.minix.dirsize;
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

static struct buffer * add_dir_entry(struct inode *dir, struct minix_dir_entry **d_res)
{
	__blk_t block;
	struct buffer *buf;

	if(!(buf = find_dir_entry(dir, NULL, d_res, NULL))) {
		if((block = bmap(dir, dir->i_size, FOR_WRITING)) < 0) {
			return NULL;
		}
		if(!(buf = bread(dir->dev, block, dir->sb->s_blocksize))) {
			return NULL;
		}
		*d_res = (struct minix_dir_entry *)buf->data;
		dir->i_size += dir->sb->u.minix.dirsize;
	}

	return buf;
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
		if(minix_lookup("..", dir_new, &dir_new)) {
			break;
		}
		if(dir_new->inode == inode) {
			break;
		}
	}
	iput(dir_new);
	return errno;
}

int minix_lookup(const char *name, struct inode *dir, struct inode **i_res)
{
	__blk_t block;
	unsigned int blksize;
	unsigned int offset, doffset;
	struct buffer *buf;
	struct minix_dir_entry *d;
	__ino_t inode;

	blksize = dir->sb->s_blocksize;
	inode = offset = 0;

	while(offset < dir->i_size && !inode) {
		if((block = bmap(dir, offset, FOR_READING)) < 0) {
			iput(dir);
			return block;
		}
		if(block) {
			if(!(buf = bread(dir->dev, block, blksize))) {
				iput(dir);
				return -EIO;
			}
			doffset = 0;
			do {
				d = (struct minix_dir_entry *)(buf->data + doffset);
				if(d->inode) {
					if(strlen(d->name) == strlen(name)) {
						if(!(strcmp(d->name, name))) {
							inode = d->inode;
						}
					}
				}
				doffset += dir->sb->u.minix.dirsize;
			} while((doffset < blksize) && (!inode));

			brelse(buf);
			if(inode) {
				if(!(*i_res = iget(dir->sb, inode))) {
					iput(dir);
					return -EACCES;
				}
				iput(dir);
				return 0;
			}
			offset += blksize;
		} else {
			break;
		}
	}
	iput(dir);
	return -ENOENT;
}

int minix_rmdir(struct inode *dir, struct inode *i)
{
	struct buffer *buf;
	struct minix_dir_entry *d;

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
	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;

	i->dirty = 1;
	dir->dirty = 1;

	bwrite(buf);

	inode_unlock(i);
	inode_unlock(dir);
	return 0;
}

int minix_link(struct inode *i_old, struct inode *dir_new, char *name)
{
	struct buffer *buf;
	struct minix_dir_entry *d;
	int n;

	inode_lock(i_old);
	inode_lock(dir_new);

	if(!(buf = add_dir_entry(dir_new, &d))) {
		inode_unlock(i_old);
		inode_unlock(dir_new);
		return -ENOSPC;
	}

	d->inode = i_old->inode;
	for(n = 0; n < i_old->sb->u.minix.namelen; n++) {
		d->name[n] = name[n];
		if(!name[n]) {
			break;
		}
	}
	for(; n < i_old->sb->u.minix.namelen; n++) {
		d->name[n] = 0;
	}

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

int minix_unlink(struct inode *dir, struct inode *i, char *name)
{
	struct buffer *buf;
	struct minix_dir_entry *d;

	inode_lock(dir);
	inode_lock(i);

	if(!(buf = find_dir_entry(dir, i, &d, name))) {
		inode_unlock(dir);
		inode_unlock(i);
		return -ENOENT;
	}

	d->inode = 0;
	i->i_nlink--;

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

int minix_symlink(struct inode *dir, char *name, char *oldname)
{
	struct buffer *buf, *buf_new;
	struct inode *i;
	struct minix_dir_entry *d;
	unsigned int blksize;
	int n, block;
	char c;

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, S_IFLNK))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	i->i_mode = S_IFLNK | (S_IRWXU | S_IRWXG | S_IRWXO);
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->i_nlink = 1;
	i->dev = dir->dev;
	i->count = 1;
	i->fsop = &minix_symlink_fsop;
	i->dirty = 1;

	block = minix_balloc(dir->sb);
	if(block < 0) {
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	if(i->sb->u.minix.version == 1) {
		i->u.minix.u.i1_zone[0] = block;
	} else {
		i->u.minix.u.i2_zone[0] = block;
	}
	blksize = dir->sb->s_blocksize;
	if(!(buf_new = bread(dir->dev, block, blksize))) {
		minix_bfree(dir->sb, block);
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -EIO;
	}

	if(!(buf = add_dir_entry(dir, &d))) {
		minix_bfree(dir->sb, block);
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	for(n = 0; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = name[n];
		if(!name[n]) {
			break;
		}
	}
	for(; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = 0;
	}

	for(n = 0; n < NAME_MAX; n++) {
		if((c = oldname[n])) {
			buf_new->data[n] = c;
			continue;
		}
		break;
	}
	buf_new->data[n] = 0;
	i->i_size = n;

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->dirty = 1;

	bwrite(buf);
	bwrite(buf_new);
	iput(i);
	inode_unlock(dir);
	return 0;
}

int minix_mkdir(struct inode *dir, char *name, __mode_t mode)
{
	struct buffer *buf, *buf_new;
	struct inode *i;
	struct minix_dir_entry *d, *d_new;
	unsigned int blksize;
	int n, block;

	if(strlen(name) > dir->sb->u.minix.namelen) {
		return -ENAMETOOLONG;
	}

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, S_IFDIR))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	i->i_mode = ((mode & (S_IRWXU | S_IRWXG | S_IRWXO)) & ~current->umask);
	i->i_mode |= S_IFDIR;
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->i_nlink = 1;
	i->dev = dir->dev;
	i->count = 1;
	i->fsop = &minix_dir_fsop;
	i->dirty = 1;

	if((block = bmap(i, 0, FOR_WRITING)) < 0) {
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	blksize = dir->sb->s_blocksize;
	if(!(buf_new = bread(i->dev, block, blksize))) {
		minix_bfree(dir->sb, block);
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -EIO;
	}

	if(!(buf = add_dir_entry(dir, &d))) {
		minix_bfree(dir->sb, block);
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	for(n = 0; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = name[n];
		if(!name[n] || name[n] == '/') {
			break;
		}
	}
	for(; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = 0;
	}

	d_new = (struct minix_dir_entry *)buf_new->data;
	d_new->inode = i->inode;
	d_new->name[0] = '.';
	d_new->name[1] = 0;
	i->i_size += i->sb->u.minix.dirsize;
	i->i_nlink++;
	d_new = (struct minix_dir_entry *)(buf_new->data + i->sb->u.minix.dirsize);
	d_new->inode = dir->inode;
	d_new->name[0] = '.';
	d_new->name[1] = '.';
	d_new->name[2] = 0;
	i->i_size += i->sb->u.minix.dirsize;

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->i_nlink++;
	dir->dirty = 1;

	bwrite(buf);
	bwrite(buf_new);
	iput(i);
	inode_unlock(dir);
	return 0;
}

int minix_mknod(struct inode *dir, char *name, __mode_t mode, __dev_t dev)
{
	struct buffer *buf;
	struct inode *i;
	struct minix_dir_entry *d;
	int n;

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, mode & S_IFMT))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	if(!(buf = add_dir_entry(dir, &d))) {
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	for(n = 0; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = name[n];
		if(!name[n]) {
			break;
		}
	}
	for(; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = 0;
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
			break;
		case S_IFBLK:
			i->fsop = &def_blk_fsop;
			i->rdev = dev;
			i->i_mode |= S_IFBLK;
			break;
		case S_IFIFO:
			i->fsop = &pipefs_fsop;
			i->i_mode |= S_IFIFO;
			/* it's a union so we need to clear pipefs_i */
			memset_b(&i->u.pipefs, NULL, sizeof(struct pipefs_inode));
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

int minix_create(struct inode *dir, char *name, __mode_t mode, struct inode **i_res)
{
	struct buffer *buf;
	struct inode *i;
	struct minix_dir_entry *d;
	int n;

	if(IS_RDONLY_FS(dir)) {
		return -EROFS;
	}

	inode_lock(dir);

	if(!(i = ialloc(dir->sb, S_IFREG))) {
		inode_unlock(dir);
		return -ENOSPC;
	}

	if(!(buf = add_dir_entry(dir, &d))) {
		i->i_nlink = 0;
		iput(i);
		inode_unlock(dir);
		return -ENOSPC;
	}

	d->inode = i->inode;
	for(n = 0; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = name[n];
		if(!name[n]) {
			break;
		}
	}
	for(; n < i->sb->u.minix.namelen; n++) {
		d->name[n] = 0;
	}

	i->i_mode = (mode & ~current->umask) & ~S_IFMT;
	i->i_mode |= S_IFREG;
	i->i_uid = current->euid;
	i->i_gid = current->egid;
	i->i_nlink = 1;
	i->dev = dir->dev;
	i->fsop = &minix_file_fsop;
	i->count = 1;
	i->dirty = 1;

	dir->i_mtime = CURRENT_TIME;
	dir->i_ctime = CURRENT_TIME;
	dir->dirty = 1;

	*i_res = i;
	bwrite(buf);
	inode_unlock(dir);
	return 0;
}

int minix_rename(struct inode *i_old, struct inode *dir_old, struct inode *i_new, struct inode *dir_new, char *oldpath, char *newpath)
{
	struct buffer *buf_old, *buf_new;
	struct minix_dir_entry *d_old, *d_new;
	int errno;

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
		if(!(buf_new = add_dir_entry(dir_new, &d_new))) {
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
		strcpy(d_new->name, newpath);
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
