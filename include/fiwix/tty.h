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
#include <fiwix/console.h>
#include <fiwix/serial.h>

#define NR_TTYS		NR_VCONSOLES + NR_SERIAL

#define CBSIZE		32	/* number of characters in cblock */
#define NR_CB_QUEUE	8	/* number of cblocks per queue */
#define CB_POOL_SIZE	128	/* number of cblocks in the central pool */

#define TAB_SIZE	8
#define MAX_TAB_COLS	132	/* maximum number of tab stops */

#define LAST_CHAR(q)	((q)->tail ? (q)->tail->data[(q)->tail->end_off - 1] : '\0')

/* tty flags */
#define TTY_HAS_LNEXT		0x01

struct clist {
	unsigned short int count;
	unsigned short int cb_num;
	struct cblock *head;
	struct cblock *tail;
};

struct cblock {
	unsigned short int start_off;
	unsigned short int end_off;
	unsigned char data[CBSIZE];
	struct cblock *prev;
	struct cblock *next;
};

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

	/* tty driver operations */
	void (*stop)(struct tty *);
	void (*start)(struct tty *);
	void (*deltab)(struct tty *);
	void (*reset)(struct tty *);
	void (*input)(struct tty *);
	void (*output)(struct tty *);
	int (*open)(struct tty *);
	int (*close)(struct tty *);
	void (*set_termios)(struct tty *);
};
extern struct tty tty_table[];

int register_tty(__dev_t);
struct tty *get_tty(__dev_t);
void disassociate_ctty(struct tty *);
void termios_reset(struct tty *);
void do_cook(struct tty *);
int tty_open(struct inode *, struct fd *);
int tty_close(struct inode *, struct fd *);
int tty_read(struct inode *, struct fd *, char *, __size_t);
int tty_write(struct inode *, struct fd *, const char *, __size_t);
int tty_ioctl(struct inode *, int cmd, unsigned int);
__loff_t tty_llseek(struct inode *, __loff_t);
int tty_select(struct inode *, int);
void tty_init(void);

int tty_queue_putchar(struct tty *, struct clist *, unsigned char);
int tty_queue_unputchar(struct clist *);
unsigned char tty_queue_getchar(struct clist *);
void tty_queue_flush(struct clist *);
int tty_queue_room(struct clist *q);
void tty_queue_init(void);

int vt_ioctl(struct tty *, int, unsigned int);

#endif /* _FIWIX_TTY_H */
