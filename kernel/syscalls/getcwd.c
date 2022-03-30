/*
 * fiwix/kernel/syscalls/getcwd.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>
#include <fiwix/mm.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_getcwd(char *buf, __size_t size)
{
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_getcwd(0x%08x, %d)\n", current->pid, (unsigned int)buf, size);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, buf, size))) {
		return errno;
	}
	/* Psedocode:
	 * dir <- current->pwd
	 * if dir == /
	 *	return "/"
	 * cwd <- ""
	 * while dir != /
	 * 	p <- lookup(..)
	 *	open(p)
	 *	dirent <- read(p)
	 *	for d in dirent
	 *		if d == dir
	 *			cwd <- '/' + d.name + cwd
	 *			dir <- d
	 *			break
	 * return cwd
	 */
	 if (size==0 || buf==NULL) {    /* buffer self allocation not supported */
		 return -EINVAL;
	 }
	 if (size<2) { /* the shortest path is "/"" */
		 return -ERANGE;
	 }
	struct dirent* dirent_buf;
	struct dirent* d_ptr;
	struct inode* cur = current->pwd;
	struct inode* up = cur;
	struct inode* root;
	int tmp_fd;
	int bytes_read;
	__size_t marker = size-2;	/* Reserve '\0' at the end */
	__size_t namelength;
	buf[size-1] = 0;
	char save;
	__size_t x;
	if((errno = namei("/",&root,0,FOLLOW_LINKS))) {
		return errno;
	}
	if (cur == root) {
		/* This case needs special handling, otherwise the loop skips over root */
		buf[0]='/';
		buf[1]='\0';
		iput(root);
		return (unsigned int)buf;
	}
	if(!(dirent_buf = (void *)kmalloc())) {
		iput(root);
		return -ENOMEM;
	}

	while(cur != root) {
		if((errno = parse_namei("..",cur,&up,0,FOLLOW_LINKS))) {
			iput(root);
			if (cur != current->pwd) {
				iput(cur);
			}
			kfree((unsigned int)dirent_buf);
			return errno;
		}
		if((tmp_fd = get_new_fd(up)) < 0) {
			iput(root);
			iput(up);
			if (cur != current->pwd) {
				iput(cur);
			}
			kfree((unsigned int)dirent_buf);
			return tmp_fd;
		}
		do {
			bytes_read = up->fsop->readdir(up, &fd_table[tmp_fd],
					dirent_buf, PAGE_SIZE);
			if (bytes_read < 0) {
				release_fd(tmp_fd);
				iput(root);
				iput(up);
				if (cur != current->pwd) {
					iput(cur);
				}
				kfree((unsigned int)dirent_buf);
				return bytes_read;
			}
			d_ptr = dirent_buf;
			while((void *) d_ptr < ((void *) dirent_buf + bytes_read)) {
				if(d_ptr->d_ino == cur->inode) {
					namelength = strlen(d_ptr->d_name);
					if (marker < namelength+1) {
						release_fd(tmp_fd);
						iput(root);
						iput(up);
						if (cur != current->pwd) {
							iput(cur);
						}
						kfree((unsigned int)dirent_buf);
						return -ERANGE;
					}
					marker -= namelength+1;	/* +1 for the leading '/' */
					save=buf[marker+namelength+1];
					strncpy(buf+marker+1, d_ptr->d_name, namelength);
					buf[marker] = '/';
					buf[marker+namelength+1] = save; /* strncp overrides '/' or '\0' */
					break;
				}
				d_ptr = (struct dirent *) ((void *)d_ptr+d_ptr->d_reclen);
			}
		} while(bytes_read!=0 && d_ptr->d_ino != cur->inode);

		release_fd(tmp_fd);
		if (d_ptr->d_ino != cur->inode) {   /* parent dir was fully read, child still not found */
			iput(root);
			iput(up);
			if (cur != current->pwd) {
				iput(cur);
			}
			kfree((unsigned int)dirent_buf);
			return -ENOENT;
		}
		if (cur != current->pwd) {
			iput(cur);
		}
		cur = up;
	}
	kfree((unsigned int)dirent_buf);
	iput(root); /* cur == up == root */
	/* Move the String to the start of the buffer */
	for ( x = marker; x < size; x++) {
		buf[x-marker]=buf[x];
	}
	return (unsigned int)buf;
}
