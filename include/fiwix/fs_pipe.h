/*
 * fiwix/include/fiwix/fs_pipe.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FS_PIPE_H
#define _FIWIX_FS_PIPE_H

#define PIPE_DEV	0xFFF0		/* special device number for nodev fs */

extern struct fs_operations pipefs_fsop;

struct pipefs_inode {
	char *i_data;			/* buffer */
	unsigned int i_readoff;		/* offset for reads */
	unsigned int i_writeoff;	/* offset for writes */
	unsigned int i_readers;		/* number of readers */
	unsigned int i_writers;		/* number of writers */
};

#endif /* _FIWIX_FS_PIPE_H */
