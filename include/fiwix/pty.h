/*
 * fiwix/include/fiwix/pty.h
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_UNIX98_PTYS

#ifndef _FIWIX_PTY_H
#define _FIWIX_PTY_H

#define PTY_MASTER_MAJOR	5	/* major number for /dev/ptmx */
#define PTY_MASTER_MINOR	2	/* minor number for /dev/ptmx */
#define PTY_SLAVE_MAJOR		136	/* major number for PTY slaves */

#define PTMX_DEV		MKDEV(PTY_MASTER_MAJOR, PTY_MASTER_MINOR)

void pty_wakeup_read(struct tty *);
int pty_open(struct tty *);
int pty_close(struct tty *);
int pty_read(struct inode *, struct fd *, char *, __size_t);
int pty_write(struct inode *, struct fd *, const char *, __size_t);
int pty_select(struct inode *, struct fd *, int);
void pty_init(void);

#endif /* _FIWIX_PTY_H */

#endif /* CONFIG_UNIX98_PTYS */
