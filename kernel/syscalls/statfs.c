/*
 * fiwix/kernel/syscalls/statfs.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/statfs.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_statfs(const char *filename, struct statfs *statfsbuf)
{
	struct inode *i;
	char *tmp_name;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_statfs('%s', 0x%08x)\n", current->pid, filename, (unsigned int)statfsbuf);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, statfsbuf, sizeof(struct statfs)))) {
		return errno;
	}
	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	free_name(tmp_name);

	if(i->sb && i->sb->fsop && i->sb->fsop->statfs) {
		i->sb->fsop->statfs(i->sb, statfsbuf);
		iput(i);
		return 0;
	}
	iput(i);
	return -ENOSYS;
}
