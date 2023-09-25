/*
 * fiwix/fs/fd.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/errno.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/sleep.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fd *fd_table;

static struct resource fd_resource = { 0, 0 };

int get_new_fd(struct inode *i)
{
	unsigned int n;

	lock_resource(&fd_resource);

	for(n = 1; n < NR_OPENS; n++) {
		if(fd_table[n].count == 0) {
			memset_b(&fd_table[n], 0, sizeof(struct fd));
			fd_table[n].inode = i;
			fd_table[n].count = 1;
			unlock_resource(&fd_resource);
			return n;
		}
	}

	unlock_resource(&fd_resource);

	return -ENFILE;
}

void release_fd(unsigned int fd)
{
	lock_resource(&fd_resource);
	fd_table[fd].count = 0;
	unlock_resource(&fd_resource);
}

int get_new_user_fd(int fd)
{
	int n;

	for(n = fd; n < OPEN_MAX && n < current->rlim[RLIMIT_NOFILE].rlim_cur; n++) {
		if(current->fd[n] == 0) {
			current->fd[n] = -1;
			current->fd_flags[n] = 0;
			return n;
		}
	}

	return -EMFILE;
}

void release_user_fd(int ufd)
{
	current->fd[ufd] = 0;
}

void fd_init(void)
{
	memset_b(fd_table, 0, fd_table_size);
}
