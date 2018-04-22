/*
 * fiwix/kernel/syscalls/pipe.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>

int sys_pipe(int pipefd[2])
{
	int rfd, rufd;
	int wfd, wufd;
	struct filesystems *fs;
	struct inode *i;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_pipe()", current->pid);
#endif /*__DEBUG__ */

	if(!(fs = get_filesystem("pipefs"))) {
		printk("WARNING: %s(): pipefs filesystem is not registered!\n", __FUNCTION__);
		return -EINVAL;
	}
	if((errno = check_user_area(VERIFY_WRITE, pipefd, sizeof(int) * 2))) {
		return errno;
	}
	if(!(i = ialloc(&fs->mt->sb))) {
		return -EINVAL;
	}
	if((rfd = get_new_fd(i)) < 0) {
		iput(i);
		return -ENFILE;
	}
	if((wfd = get_new_fd(i)) < 0) {
		release_fd(rfd);
		iput(i);
		return -ENFILE;
	}
	if((rufd = get_new_user_fd(0)) < 0) {
		release_fd(rfd);
		release_fd(wfd);
		iput(i);
		return -EMFILE;
	}
	if((wufd = get_new_user_fd(0)) < 0) {
		release_fd(rfd);
		release_fd(wfd);
		release_user_fd(rufd);
		iput(i);
		return -EMFILE;
	}

	pipefd[0] = rufd;
	pipefd[1] = wufd;
	current->fd[rufd] = rfd;
	current->fd[wufd] = wfd;
	fd_table[rfd].flags = O_RDONLY;
	fd_table[wfd].flags = O_WRONLY;

#ifdef __DEBUG__
	printk(" -> inode=%d, rufd=%d wufd=%d (rfd=%d wfd=%d)\n", i->inode, rufd, wufd, rfd, wfd);
#endif /*__DEBUG__ */

	return 0;
}
