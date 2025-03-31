/*
 * fiwix/include/fiwix/fs_devpts.h
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_UNIX98_PTYS

#ifndef _FIWIX_FS_DEVPTS_H
#define _FIWIX_FS_DEVPTS_H

#include <fiwix/types.h>

#define DEVPTS_ROOT_INO		1	/* root inode */
#define DEVPTS_SUPER_MAGIC	0x1CD1	/* same as in Linux */

#define NR_PTYS			64

extern struct fs_operations devpts_fsop;

struct devpts_files {
	int count;
	struct inode *inode;
};
extern struct devpts_files *devpts_list;

#endif /* _FIWIX_FS_DEVPTS_H */

#endif /* CONFIG_UNIX98_PTYS */
