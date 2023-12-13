/*
 * fiwix/include/fiwix/fs_sock.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_FS_SOCK_H
#define _FIWIX_FS_SOCK_H

#include <fiwix/net.h>

extern struct fs_operations sockfs_fsop;

struct sockfs_inode {
	struct socket sock;
};

#endif /* _FIWIX_FS_SOCK_H */

#endif /* CONFIG_NET */
