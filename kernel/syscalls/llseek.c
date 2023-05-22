/*
 * fiwix/kernel/syscalls/llseek.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_llseek(unsigned int ufd, unsigned long int offset_high, unsigned long int offset_low, __loff_t *result, unsigned int whence)
{
	struct inode *i;
	__loff_t offset, new_offset;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_llseek(%d, %u, %u, %08x, %d)", current->pid, ufd, offset_high, offset_low, result, whence);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	if((errno = check_user_area(VERIFY_WRITE, result, sizeof(__loff_t)))) {
		return errno;
	}
	i = fd_table[current->fd[ufd]].inode;
	offset = (__loff_t)(((__loff_t)offset_high << 32) | offset_low);
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
	if(i->fsop && i->fsop->llseek) {
		fd_table[current->fd[ufd]].offset = new_offset;
		if((new_offset = i->fsop->llseek(i, new_offset)) < 0) {
			return (int)new_offset;
		}
	} else {
		return -EPERM;
	}
	memcpy_b(result, &new_offset, sizeof(__loff_t));

#ifdef __DEBUG__
	printk(" -> returning %lu\n", *result);
#endif /*__DEBUG__ */

	return 0;
}
