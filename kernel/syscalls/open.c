/*
 * fiwix/kernel/syscalls/open.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/stat.h>
#include <fiwix/types.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int sys_open(const char *filename, int flags, __mode_t mode)
{
	int fd, ufd;
	struct inode *i, *dir;
	char *tmp_name, *basename;
	int errno, follow_links, perms;

#ifdef __DEBUG__
	printk("(pid %d) sys_open('%s', %o, %o)\n", current->pid, filename, flags, mode);
#endif /*__DEBUG__ */

	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}

	basename = get_basename(tmp_name);
	follow_links = (flags & O_NOFOLLOW) ? !FOLLOW_LINKS : FOLLOW_LINKS;
	if((errno = namei(tmp_name, &i, &dir, follow_links))) {
		if(!dir) {
			free_name(tmp_name);
			if(flags & O_CREAT) {
				return -ENOENT;
			}
			return errno;
		}
	}

#ifdef __DEBUG__
	printk("\t(inode = %d)\n", i ? i->inode : -1);
#endif /*__DEBUG__ */

	if(!errno) {
		if(S_ISLNK(i->i_mode) && (flags & O_NOFOLLOW)) {
			iput(i);
			iput(dir);
			free_name(tmp_name);
			return -ELOOP;
		}
		if(!S_ISDIR(i->i_mode) && (flags & O_DIRECTORY)) {
			iput(i);
			iput(dir);
			free_name(tmp_name);
			return -ENOTDIR;
		}
	}

	if(flags & O_CREAT) {
		if(!errno && (flags & O_EXCL)) {
			iput(i);
			iput(dir);
			free_name(tmp_name);
			return -EEXIST;
		}
		if(!i) {
			/*
			 * If the file does not exist, you need enough
			 * permission in the directory to create it.
			 */
			if(check_permission(TO_EXEC | TO_WRITE, dir) < 0) {
				iput(i);
				iput(dir);
				free_name(tmp_name);
				return -EACCES;
			}
		}
		if(errno) {	/* assumes -ENOENT */
			if(dir->fsop && dir->fsop->create) {
				errno = dir->fsop->create(dir, basename, flags, mode, &i);
				if(errno) {
					iput(dir);
					free_name(tmp_name);
					return errno;
				}
			} else {
				iput(dir);
				free_name(tmp_name);
				return -EACCES;
			}
		}
	} else {
		if(errno) {
			iput(dir);
			free_name(tmp_name);
			return errno;
		}
		if(S_ISDIR(i->i_mode) && (flags & (O_RDWR | O_WRONLY | O_TRUNC))) {
			iput(i);
			iput(dir);
			free_name(tmp_name);
			return -EISDIR;
		}
		mode = 0;
	}

	if((flags & O_ACCMODE) == O_RDONLY) {
		perms = TO_READ;
	} else if((flags & O_ACCMODE) == O_WRONLY) {
		perms = TO_WRITE;
	} else {
		perms = TO_READ | TO_WRITE;
	}
	if((errno = check_permission(perms, i))) {
		iput(i);
		iput(dir);
		free_name(tmp_name);
		return errno;
	}
	if((fd = get_new_fd(i)) < 0) {
		iput(i);
		iput(dir);
		free_name(tmp_name);
		return fd;
	}
	if((ufd = get_new_user_fd(0)) < 0) {
		release_fd(fd);
		iput(i);
		iput(dir);
		free_name(tmp_name);
		return ufd;
	}

#ifdef __DEBUG__
	printk("\t(ufd = %d)\n", ufd);
#endif /*__DEBUG__ */

	fd_table[fd].flags = flags;
	current->fd[ufd] = fd;
	if(i->fsop && i->fsop->open) {
		if((errno = i->fsop->open(i, &fd_table[fd])) < 0) {
			release_fd(fd);
			release_user_fd(ufd);
			iput(i);
			iput(dir);
			free_name(tmp_name);
			return errno;
		}
		iput(dir);
		free_name(tmp_name);
		return ufd;
	}

	printk("WARNING: %s(): file '%s' (inode %d) without the open() method!\n", __FUNCTION__, tmp_name, i->inode);
	release_fd(fd);
	release_user_fd(ufd);
	iput(i);
	iput(dir);
	free_name(tmp_name);
	return -EINVAL;
}
