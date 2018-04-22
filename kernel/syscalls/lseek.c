/*
 * fiwix/kernel/syscalls/lseek.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/syscalls.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_lseek(unsigned int ufd, __off_t offset, unsigned int whence)
{
	struct inode *i;
	__off_t new_offset;

#ifdef __DEBUG__
	printk("(pid %d) sys_lseek(%d, %d, %d)", current->pid, ufd, offset, whence);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);

	i = fd_table[current->fd[ufd]].inode;
	switch(whence) {
		case SEEK_SET:
			new_offset = offset;
			break;
		case SEEK_CUR:
			new_offset = fd_table[current->fd[ufd]].offset + offset;
			break;
		case SEEK_END:
			new_offset = i->i_size + offset;
			break;
		default:
			return -EINVAL;
	}
	if((int)new_offset < 0) {
		return -EINVAL;
	}
	if(i->fsop && i->fsop->lseek) {
		fd_table[current->fd[ufd]].offset = new_offset;
		new_offset = i->fsop->lseek(i, new_offset);
	} else {
		return -EPERM;
	}

#ifdef __DEBUG__
	printk(" -> returning %d\n", new_offset);
#endif /*__DEBUG__ */

	return new_offset;
}
