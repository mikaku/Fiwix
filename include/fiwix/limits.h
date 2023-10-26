/*
 * fiwix/include/fiwix/limits.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_LIMITS_H
#define _FIWIX_LIMITS_H

#ifndef DEVNAME_MAX
#define DEVNAME_MAX	50	/* device name length in mount table */
#endif
#ifndef ARG_MAX
#define ARG_MAX		32	/* length (in pages) of argv+env in 'execve' */
#endif
#ifndef CHILD_MAX
#define CHILD_MAX	64	/* simultaneous processes per real user ID */
#endif
#ifndef LINK_MAX
#define LINK_MAX	255	/* maximum number of links to a file */
#endif
#ifndef MAX_CANON
#define MAX_CANON	255	/* bytes in a terminal canonical input queue */
#endif
#ifndef MAX_INPUT
#define MAX_INPUT	255	/* bytes for which space will be available in a
				   terminal input queue */
#endif
#ifndef NGROUPS_MAX
#define NGROUPS_MAX	32	/* simultaneous supplementary group IDs */
#endif
#ifndef OPEN_MAX
#define OPEN_MAX	256	/* files one process can have opened at once */
#endif
#ifndef FD_SETSIZE
#define FD_SETSIZE	OPEN_MAX /* descriptors that a process may examine with
				    'pselect' or 'select' */
#endif
#ifndef NAME_MAX
#define NAME_MAX	255	/* bytes in a filename */
#endif
#ifndef PATH_MAX
#define PATH_MAX	1024	/* bytes in a pathname */
#endif
#ifndef PIPE_BUF
#define PIPE_BUF	4096	/* bytes than can be written atomically to a
				   pipe */
#endif
#ifndef UIO_MAXIOV
#define UIO_MAXIOV	16	/* maximum number of scatter/gather elements
				   that can be processed in one call */
#endif

#endif /* _FIWIX_LIMITS_H */
