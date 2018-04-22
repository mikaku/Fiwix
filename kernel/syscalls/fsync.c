/*
 * fiwix/kernel/syscalls/fsync.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/process.h>
#include <fiwix/stat.h>
#include <fiwix/buffer.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_fsync(int ufd)
{
	struct inode *i;

#ifdef __DEBUG__
	printk("(pid %d) sys_fsync(%d)\n", current->pid, ufd);
#endif /*__DEBUG__ */

	CHECK_UFD(ufd);
	i = fd_table[current->fd[ufd]].inode;
	if(!S_ISREG(i->i_mode)) {
		return -EINVAL;
	}
	if(IS_RDONLY_FS(i)) {
		return -EROFS;
	}
	sync_superblocks(i->dev);
	sync_inodes(i->dev);
	sync_buffers(i->dev);
	return 0;
}
