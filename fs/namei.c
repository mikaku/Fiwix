/*
 * fiwix/fs/namei.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static int namei_lookup(char *name, struct inode *dir, struct inode **i_res)
{
	if(dir->fsop && dir->fsop->lookup) {
		return dir->fsop->lookup(name, dir, i_res);
	}
	return -EACCES;
}

static int do_namei(char *path, struct inode *dir, struct inode **i_res, struct inode **d_res, int follow_links)
{
	char *name, *ptr_name;
	struct inode *i;
	struct superblock *sb;

	int errno;

	*i_res = dir;
	for(;;) {
		while(*path == '/') {
			path++;
		}
		if(*path == NULL) {
			return 0;
		}

		/* extracts the next component of the path */
		if(!(name = (char *)kmalloc())) {
			return -ENOMEM;
		}
		ptr_name = name;
		while(*path != NULL && *path != '/') {
			if(ptr_name > name + NAME_MAX - 1) {
				break;
			}
			*ptr_name++ = *path++;
		}
		*ptr_name = NULL;

		/*
		 * If the inode is the root of a file system, then return the
		 * inode on which the file system was mounted.
		 */
		if(name[0] == '.' && name[1] == '.' && name[2] == NULL) {
			if(dir == dir->sb->root) {
				sb = dir->sb;
				iput(dir);
				dir = sb->dir;
				dir->count++;
			}
		}

		if((errno = check_permission(TO_EXEC, dir))) {
			break;
		}

		if((errno = namei_lookup(name, dir, &i))) {
			break;
		}

		kfree((unsigned int)name);
		if(*path == '/') {
			if(!S_ISDIR(i->i_mode) && !S_ISLNK(i->i_mode)) {
				iput(dir);
				iput(i);
				return -ENOTDIR;
			}
			if(S_ISLNK(i->i_mode)) {
				if(i->fsop && i->fsop->followlink) {
					if((errno = i->fsop->followlink(dir, i, &i))) {
						iput(dir);
						return errno;
					}
				}
			}
		} else {
			if((i->fsop && i->fsop->followlink) && follow_links) {
				if((errno = i->fsop->followlink(dir, i, &i))) {
					iput(dir);
					return errno;
				}
			}
		}

		if(d_res) {
			if(*d_res) {
				iput(*d_res);
			}
			*d_res = dir;
		} else {
			iput(dir);
		}
		dir = i;
		*i_res = i;
	}

	kfree((unsigned int)name);
	if(d_res) {
		if(*d_res) {
			iput(*d_res);
		}
		/*
		 * If that was the last component of the path,
		 * then return the directory.
		 */
		if(*path == NULL) {
			*d_res = dir;
			dir->count++;
		} else {
			/* that's an non-existent directory */
			*d_res = NULL;
			errno = -ENOTDIR;
		}
		iput(dir);
		*i_res = NULL;
	} else {
		iput(dir);
	}

	return errno;
}

int parse_namei(char *path, struct inode *base_dir, struct inode **i_res, struct inode **d_res, int follow_links)
{
	struct inode *dir;
	int errno;

	if(!path) {
		return -EFAULT;
	}
	if(*path == NULL) {
		return -ENOENT;
	}

	if(!(dir = base_dir)) {
		dir = current->pwd;
	}

	/* it is definitely an absolute path */
	if(path[0] == '/') {
		dir = current->root;
	}
	dir->count++;
	errno = do_namei(path, dir, i_res, d_res, follow_links);
	return errno;
}

/*
 * namei() returns:
 * i_res -> the inode of the last component of the path, or NULL.
 * d_res -> the inode of the directory where i_res resides, or NULL.
 */
int namei(char *path, struct inode **i_res, struct inode **d_res, int follow_links)
{
	*i_res = NULL;
	if(d_res) {
		*d_res = NULL;
	}
	return parse_namei(path, NULL, i_res, d_res, follow_links);
}
