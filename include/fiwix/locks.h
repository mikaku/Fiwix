/*
 * fiwix/include/fiwix/locks.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_LOCKS_H
#define _FIWIX_LOCKS_H

#include <fiwix/config.h>
#include <fiwix/fs.h>
#include <fiwix/fcntl.h>

struct flock_file {
	struct inode *inode;	/* file */
	unsigned char type;	/* type of lock */
	struct proc *proc;	/* owner */
};

struct flock_file flock_file_table[NR_FLOCKS];

int posix_lock(int, int, struct flock *);

void flock_release_inode(struct inode *);
int flock_inode(struct inode *, int);
void flock_init(void);

#endif /* _FIWIX_LOCKS_H */
