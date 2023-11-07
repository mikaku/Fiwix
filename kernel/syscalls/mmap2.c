/*
 * fiwix/kernel/syscalls/mmap2.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Copyright 2023, Richard R. Masters. All rights reserved.
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

#ifdef CONFIG_SYSCALL_6TH_ARG
#ifdef CONFIG_MMAP2
int sys_mmap2(unsigned int start, unsigned int length, unsigned int prot, unsigned int user_flags, int fd, unsigned int offset)
{
	unsigned int page;
	struct inode *i;
	char flags;

#ifdef __DEBUG__
	printk("(pid %d) sys_mmap2(0x%08x, %d, 0x%02x, 0x%02x, %d, 0x%08x) -> ", current->pid, start, length, prot, user_flags, fd, offset);
#endif /*__DEBUG__ */

	if(!length) {
		return -EINVAL;
	}

	i = NULL;
	flags = 0;
	if(!(user_flags & MAP_ANONYMOUS)) {
		CHECK_UFD(fd);
		if(!(i = fd_table[current->fd[fd]].inode)) {
			return -EBADF;
		}
		flags = fd_table[current->fd[fd]].flags & O_ACCMODE;
	}
	page = do_mmap(i, start, length, prot, user_flags, offset*4096, P_MMAP, flags, NULL);
#ifdef __DEBUG__
	printk("0x%08x\n", page);
#endif /*__DEBUG__ */
	return page;
}
#endif /* CONFIG_MMAP2 */
#endif /* CONFIG_SYSCALL_6TH_ARG */
