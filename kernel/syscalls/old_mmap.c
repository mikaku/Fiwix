/*
 * fiwix/kernel/syscalls/old_mmap.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/mman.h>
#include <fiwix/mm.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int old_mmap(struct mmap *mmap)
{
	unsigned int page;
	struct inode *i;
	char flags;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) old_mmap(0x%08x, %d, 0x%02x, 0x%02x, %d, 0x%08x) -> ", current->pid, mmap->start, mmap->length, mmap->prot, mmap->flags, mmap->fd, mmap->offset);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_READ, mmap, sizeof(struct mmap)))) {
		return errno;
	}
	if(!mmap->length) {
		return -EINVAL;
	}

	i = NULL;
	flags = 0;
	if(!(mmap->flags & MAP_ANONYMOUS)) {
		CHECK_UFD(mmap->fd);
		if(!(i = fd_table[current->fd[mmap->fd]].inode)) {
			return -EBADF;
		}
		flags = fd_table[current->fd[mmap->fd]].flags & O_ACCMODE;
	}
	page = do_mmap(i, mmap->start, mmap->length, mmap->prot, mmap->flags, mmap->offset, P_MMAP, flags, NULL);
#ifdef __DEBUG__
	printk("0x%08x\n", page);
#endif /*__DEBUG__ */
	return page;
}
