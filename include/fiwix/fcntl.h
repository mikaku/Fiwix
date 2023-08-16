/*
 * fiwix/include/fiwix/fcntl.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FCNTL_H
#define _FIWIX_FCNTL_H

#include <fiwix/types.h>

#define O_ACCMODE	   0003
#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02

/* for open() only */
#define O_CREAT		   0100	/* create file if it does not exist */
#define O_EXCL		   0200	/* exclusive use flag */
#define O_NOCTTY	   0400	/* do not assign controlling terminal */
#define O_TRUNC		  01000	/* truncate flag */
#define O_DIRECTORY	0200000 /* only open if directory */
#define O_NOFOLLOW	0400000	/* do not follow symbolic links */

#define O_APPEND	  02000
#define O_NONBLOCK	  04000
#define O_NDELAY	O_NONBLOCK
#define O_SYNC		 010000

#define F_DUPFD		0	/* duplicate file descriptor */
#define F_GETFD		1	/* get file descriptor flags */
#define F_SETFD		2	/* set file descriptor flags */
#define F_GETFL		3	/* get status flags and file access modes */
#define F_SETFL		4	/* set file status flags */
#define F_GETLK		5	/* get record locking information */
#define F_SETLK		6	/* set record locking information */
#define F_SETLKW	7	/* same as F_SETLK; wait if blocked */
#define F_DUPFD_CLOEXEC	1030	/* duplicate file descriptor with close-on-exec*/

/* get/set process or process group ID to receive SIGURG signals */
#define F_SETOWN	8	/* for sockets only */
#define F_GETOWN	9	/* for sockets only */

/* for F_[GET|SET]FL */
#define FD_CLOEXEC	1	/* close the file descriptor upon exec() */

/* for POSIX fcntl() */
#define F_RDLCK		0	/* shared or read lock */
#define F_WRLCK		1	/* exclusive or write lock */
#define F_UNLCK		2	/* unlock */

/* for BSD flock() */
#define LOCK_SH		1	/* shared lock */
#define LOCK_EX		2	/* exclusive lock */
#define LOCK_NB		4	/* or'd with one of the above to prevent
				   blocking */
#define LOCK_UN		8	/* unlock */

/* IEEE Std 1003.1, 2004 Edition */
struct flock {
	short int l_type;	/* type of lock: F_RDLCK, F_WRLCK, F_UNLCK */
	short int l_whence;	/* flag for 'l_start': SEEK_SET, SEEK_CUR, ...*/
	__off_t l_start;	/* relative offset in bytes */
	__off_t l_len;		/* size; if 0 then until EOF */
	__pid_t l_pid;		/* PID holding the lock; returned in F_GETLK */
};

#endif /* _FIWIX_FCNTL_H */
