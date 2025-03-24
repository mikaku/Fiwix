/*
 * fiwix/include/fiwix/fd.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FD_H
#define _FIWIX_FD_H

#include <fiwix/config.h>
#include <fiwix/types.h>

#define CHECK_UFD(ufd)							\
{									\
	if((ufd) > (OPEN_MAX - 1) || current->fd[(ufd)] == 0) {		\
		return -EBADF;						\
	}								\
}									\

extern unsigned int fd_table_size;	/* size in bytes */
extern struct fd *fd_table;

struct fd {
	struct inode *inode;		/* file inode */
	unsigned short int flags;	/* flags */
	unsigned short int count;	/* number of opened instances */
#ifdef CONFIG_OFFSET64
	__loff_t offset;		/* r/w pointer position */
#else
	__off_t offset;			/* r/w pointer position */
#endif /* CONFIG_OFFSET64 */
	void *private_data;		/* needed for tty driver */
};

#endif /* _FIWIX_FS_H */
