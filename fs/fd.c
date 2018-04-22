/*
 * fiwix/fs/fd.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/errno.h>
#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/sleep.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fd *fd_table;

static struct resource fd_resource = { NULL, NULL };

int get_new_fd(struct inode *i)
{
	unsigned int n;

	lock_resource(&fd_resource);

	for(n = 1; n < NR_OPENS; n++) {
		if(fd_table[n].count == 0) {
			memset_b(&fd_table[n], NULL, sizeof(struct fd));
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

void fd_init(void)
{
	memset_b(fd_table, NULL, fd_table_size);
}
