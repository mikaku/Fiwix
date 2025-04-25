/*
 * fiwix/include/fiwix/tty.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_TTY_H
#define _FIWIX_TTY_H

#include <fiwix/termios.h>
#include <fiwix/fs.h>
#include <fiwix/fd.h>
#include <fiwix/charq.h>
#include <fiwix/console.h>
#include <fiwix/serial.h>

#define TAB_SIZE	8
#define MAX_TAB_COLS	132	/* maximum number of tab stops */

/* tty flags */
#define TTY_HAS_LNEXT		0x01
#define TTY_OTHER_CLOSED	0x02
#define TTY_PTY_LOCK		0x04

struct kbd_state {
	char mode;
};

struct tty {
	__dev_t dev;
	struct clist read_q;
	struct clist cooked_q;
	struct clist write_q;
	short int count;
	struct termios termios;
	struct winsize winsize;
	struct kbd_state kbd;
	__pid_t pid, pgid, sid;
	void *driver_data;
	int canon_data;
	char tab_stop[132];
	int column;
	int flags;
	struct tty *link;
	struct tty *next;

	/* tty driver operations */
	void (*stop)(struct tty *);
	void (*start)(struct tty *);
	void (*deltab)(struct tty *);
	void (*reset)(struct tty *);
	void (*input)(struct tty *);
	void (*output)(struct tty *);
	int (*open)(struct tty *);
	int (*close)(struct tty *);
	int (*ioctl)(struct tty *, struct fd *, int cmd, unsigned int);
	void (*set_termios)(struct tty *);
};
extern struct tty *tty_table;

void tty_reset(struct tty *);
struct tty *register_tty(__dev_t);
void unregister_tty(struct tty *);
struct tty *get_tty(__dev_t);
void disassociate_ctty(struct tty *);
void termios_reset(struct tty *);
void tty_deltab(struct tty *);
void do_cook(struct tty *);
int tty_open(struct inode *, struct fd *);
int tty_close(struct inode *, struct fd *);
int tty_read(struct inode *, struct fd *, char *, __size_t);
int tty_write(struct inode *, struct fd *, const char *, __size_t);
int tty_ioctl(struct inode *, struct fd *, int cmd, unsigned int);
__loff_t tty_llseek(struct inode *, __loff_t);
int tty_select(struct inode *, struct fd *, int);
void tty_init(void);

int vt_ioctl(struct tty *, int, unsigned int);

#endif /* _FIWIX_TTY_H */
