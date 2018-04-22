#ifndef _FIWIX_STAT_H
#define _FIWIX_STAT_H

#include <fiwix/statbuf.h>

/* Encoding of the file mode.  These are the standard Unix values,
   but POSIX.1 does not specify what values should be used.  */

#define S_IFMT		0170000		/* Type of file mask */

/* File types.  */
#define S_IFIFO		0010000		/* Named pipe (fifo) */
#define S_IFCHR		0020000		/* Character special */
#define S_IFDIR		0040000		/* Directory */
#define S_IFBLK		0060000		/* Block special */
#define S_IFREG		0100000		/* Regular */
#define S_IFLNK		0120000		/* Symbolic link */
#define S_IFSOCK 	0140000		/* Socket */

/* Protection bits.  */
#define S_IXUSR		00100		/* USER   --x------ */
#define S_IWUSR		00200		/* USER   -w------- */
#define S_IRUSR		00400		/* USER   r-------- */
#define S_IRWXU		00700		/* USER   rwx------ */

#define S_IXGRP		00010		/* GROUP  -----x--- */
#define S_IWGRP		00020		/* GROUP  ----w---- */
#define S_IRGRP		00040		/* GROUP  ---r----- */
#define S_IRWXG		00070		/* GROUP  ---rwx--- */

#define S_IXOTH		00001		/* OTHERS --------x */
#define S_IWOTH		00002		/* OTHERS -------w- */
#define S_IROTH		00004		/* OTHERS ------r-- */
#define S_IRWXO		00007		/* OTHERS ------rwx */

#define S_ISUID		0004000		/* set user id on execution */
#define S_ISGID		0002000		/* set group id on execution */
#define S_ISVTX		0001000		/* sticky bit */

#define S_IREAD		S_IRUSR		/* Read by owner.  */
#define S_IWRITE	S_IWUSR		/* Write by owner.  */
#define S_IEXEC		S_IXUSR		/* Execute by owner.  */

#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) 	(((m) & S_IFMT) == S_IFSOCK)

#define TO_READ		4	/* test for read permission */
#define TO_WRITE	2	/* test for write permission */
#define TO_EXEC		1	/* test for execute permission */

#endif /* _FIWIX_STAT_H */
