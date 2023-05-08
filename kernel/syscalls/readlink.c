/*
 * fiwix/kernel/syscalls/readlink.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_readlink(const char *filename, char *buffer, __size_t bufsize)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_readlink(%s, 0x%08x, %d)\n", current->pid, filename, (unsigned int)buffer, bufsize);
#endif /*__DEBUG__ */

	if(bufsize <= 0) {
		return -EINVAL;
	}
	if((errno = check_user_area(VERIFY_WRITE, buffer, bufsize))) {
		return errno;
	}
	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, !FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	free_name(tmp_name);

	if(!S_ISLNK(i->i_mode)) {
		iput(i);
		return -EINVAL;
	}

	if(i->fsop && i->fsop->readlink) {
		errno = i->fsop->readlink(i, buffer, bufsize);
		iput(i);
		return errno;
	}
	iput(i);
	return -EINVAL;
}
