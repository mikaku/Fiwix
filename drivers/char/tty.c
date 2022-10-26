/*
 * fiwix/drivers/char/tty.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/ioctl.h>
#include <fiwix/tty.h>
#include <fiwix/ctype.h>
#include <fiwix/console.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/sched.h>
#include <fiwix/timer.h>
#include <fiwix/sleep.h>
#include <fiwix/process.h>
#include <fiwix/fcntl.h>
#include <fiwix/kd.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct tty tty_table[NR_TTYS];
extern short int current_cons;

static void wait_vtime_off(unsigned int arg)
{
	unsigned int *fn = (unsigned int *)arg;

	wakeup(fn);
}

static void termios2termio(struct termios *termios, struct termio *termio)
{
	int n;

	termio->c_iflag = termios->c_iflag;
	termio->c_oflag = termios->c_oflag;
	termio->c_cflag = termios->c_cflag;
	termio->c_lflag = termios->c_lflag;
	termio->c_line = termios->c_line;
	for(n = 0; n < NCC; n++) {
		termio->c_cc[n] = termios->c_cc[n];
	}
}

static void termio2termios(struct termio *termio, struct termios *termios)
{
	int n;

	termios->c_iflag = termio->c_iflag;
	termios->c_oflag = termio->c_oflag;
	termios->c_cflag = termio->c_cflag;
	termios->c_lflag = termio->c_lflag;
	termios->c_line = termio->c_line;
	for(n = 0; n < NCC; n++) {
		termios->c_cc[n] = termio->c_cc[n];
	}
}

static int opost(struct tty *tty, unsigned char ch)
{
	int status;

	status = 0;

	if(tty->termios.c_oflag & OPOST) {
		switch(ch) {
			case '\n':
				if(tty->termios.c_oflag & ONLCR) {
					if(tty_queue_room(&tty->write_q) >= 2) {
						tty_queue_putchar(tty, &tty->write_q, '\r');
						tty->column = 0;
					} else {
						return -1;
					}
				}
				break;
			case '\t':
				while(tty->column < (tty->winsize.ws_col - 1)) {
					if(tty->tab_stop[++tty->column]) {
						break;
					}
				}
				break;
			case '\b':
				if(tty->column > 0) {
					tty->column--;
				}
			default:
				if(tty->termios.c_oflag & OLCUC) {
					ch = TOUPPER(ch);
				}
				if(!ISCNTRL(ch)) {
					tty->column++;
				}
				break;
		}
	}
	if(tty_queue_putchar(tty, &tty->write_q, ch) < 0) {
		status = -1;
	}
	return status;
}

static void out_char(struct tty *tty, unsigned char ch)
{
	if(ISCNTRL(ch) && !ISSPACE(ch) && (tty->termios.c_lflag & ECHOCTL)) {
		if(tty->flags & TTY_HAS_LNEXT || (!(tty->flags & TTY_HAS_LNEXT) && ch != tty->termios.c_cc[VEOF])) {
			tty_queue_putchar(tty, &tty->write_q, '^');
			tty_queue_putchar(tty, &tty->write_q, ch + 64);
			tty->column += 2;
		}
	} else {
		opost(tty, ch);
	}
}

static void erase_char(struct tty *tty, unsigned char erasechar)
{
	unsigned char ch;

	if(erasechar == tty->termios.c_cc[VERASE]) {
		if((ch = tty_queue_unputchar(&tty->cooked_q))) {
			if(tty->termios.c_lflag & ECHO) {
				tty_queue_putchar(tty, &tty->write_q, '\b');
				tty_queue_putchar(tty, &tty->write_q, ' ');
				tty_queue_putchar(tty, &tty->write_q, '\b');
				if(ch == '\t') {
					tty->deltab(tty);
				}
				if(ISCNTRL(ch) && !ISSPACE(ch) && tty->termios.c_lflag & ECHOCTL) {
					tty_queue_putchar(tty, &tty->write_q, '\b');
					tty_queue_putchar(tty, &tty->write_q, ' ');
					tty_queue_putchar(tty, &tty->write_q, '\b');
				}
			}
		}
	}
	if(erasechar == tty->termios.c_cc[VWERASE]) {
		unsigned char word_seen = 0;

		while(tty->cooked_q.count > 0) {
			ch = LAST_CHAR(&tty->cooked_q);
			if((ch == ' ' || ch == '\t') && word_seen) {
				break;
			}
			if(ch != ' ' && ch != '\t') {
				word_seen = 1;
			}
			erase_char(tty, tty->termios.c_cc[VERASE]);
		}
	}
	if(erasechar == tty->termios.c_cc[VKILL]) {
		while(tty->cooked_q.count > 0) {
			erase_char(tty, tty->termios.c_cc[VERASE]);
		}
		if(tty->termios.c_lflag & ECHOK && !(tty->termios.c_lflag & ECHOE)) {
			tty_queue_putchar(tty, &tty->write_q, '\n');
		}
	}
}

static void set_termios(struct tty *tty, struct termios *new_termios)
{
	memcpy_b(&tty->termios, new_termios, sizeof(struct termios));
	if(tty->set_termios) {
		tty->set_termios(tty);
	}
}

static void set_termio(struct tty *tty, struct termio *new_termio)
{
	struct termios new_termios;

	termio2termios(new_termio, &new_termios);
	memcpy_b(&tty->termios, &new_termios, sizeof(struct termios));
}

int register_tty(__dev_t dev)
{
	int n;

	for(n = 0; n < NR_TTYS; n++) {
		if(tty_table[n].dev == dev) {
			printk("ERROR: %s(): tty device %d,%d already registered!\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
			return 1;
		}
		if(!tty_table[n].dev) {
			tty_table[n].dev = dev;
			tty_table[n].count = 0;
			return 0;
		}
	}
	printk("ERROR: %s(): tty table is full!\n", __FUNCTION__);
	return 1;
}

struct tty *get_tty(__dev_t dev)
{
	int n;

	if(!dev) {
		return NULL;
	}

	/* /dev/console = system console */
	if(dev == MKDEV(SYSCON_MAJOR, 1)) {
		dev = (__dev_t)_syscondev;
	}

	/* /dev/tty0 = current virtual console */
	if(dev == MKDEV(VCONSOLES_MAJOR, 0)) {
		dev = MKDEV(VCONSOLES_MAJOR, current_cons);
	}

	/* /dev/tty = controlling TTY device */
	if(dev == MKDEV(SYSCON_MAJOR, 0)) {
		if(!current->ctty) {
			return NULL;
		}
		dev = current->ctty->dev;
	}

	for(n = 0; n < NR_TTYS; n++) {
		if(tty_table[n].dev == dev) {
			return &tty_table[n];
		}
	}
	return NULL;
}

void disassociate_ctty(struct tty *tty)
{
	struct proc *p;

	if(!tty) {
		return;
	}

	/* this tty is no longer the controlling tty of any session */
	tty->pgid = tty->sid = 0;

	/* clear the controlling tty for all processes in the same SID */
	FOR_EACH_PROCESS(p) {
		if(p->sid == current->sid) {
			p->ctty = NULL;
		}
		p = p->next;
	}
	kill_pgrp(current->pgid, SIGHUP, KERNEL);
	kill_pgrp(current->pgid, SIGCONT, KERNEL);
}

void termios_reset(struct tty *tty)
{
	tty->kbd.mode = K_XLATE;
	tty->termios.c_iflag = ICRNL | IXON | IXOFF;
	tty->termios.c_oflag = OPOST | ONLCR;
	tty->termios.c_cflag = B9600 | CS8 | HUPCL | CREAD | CLOCAL;
	tty->termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
	tty->termios.c_line = 0;
	tty->termios.c_cc[VINTR]    = 3;	/* ^C */
	tty->termios.c_cc[VQUIT]    = 28;	/* ^\ */
	tty->termios.c_cc[VERASE]   = BS;	/* ^? (127) not '\b' (^H) */
	tty->termios.c_cc[VKILL]    = 21;	/* ^U */
	tty->termios.c_cc[VEOF]     = 4;	/* ^D */
	tty->termios.c_cc[VTIME]    = 0;
	tty->termios.c_cc[VMIN]     = 1;
	tty->termios.c_cc[VSWTC]    = 0;
	tty->termios.c_cc[VSTART]   = 17;	/* ^Q */
	tty->termios.c_cc[VSTOP]    = 19;	/* ^S */
	tty->termios.c_cc[VSUSP]    = 26;	/* ^Z */
	tty->termios.c_cc[VEOL]     = '\n';	/* ^J */
	tty->termios.c_cc[VREPRINT] = 18;	/* ^R */
	tty->termios.c_cc[VDISCARD] = 15;	/* ^O */
	tty->termios.c_cc[VWERASE]  = 23;	/* ^W */
	tty->termios.c_cc[VLNEXT]   = 22;	/* ^V */
	tty->termios.c_cc[VEOL2]    = 0;	
}

void do_cook(struct tty *tty)
{
	int n;
	unsigned char ch;
	struct cblock *cb;

	while(tty->read_q.count > 0) {
		ch = tty_queue_getchar(&tty->read_q);

		if((tty->termios.c_lflag & ISIG) && !(tty->flags & TTY_HAS_LNEXT)) {
			if(ch == tty->termios.c_cc[VINTR]) {
				if(!(tty->termios.c_lflag & NOFLSH)) {
					tty_queue_flush(&tty->read_q);
					tty_queue_flush(&tty->cooked_q);
				}
				if(tty->pgid > 0) {
					kill_pgrp(tty->pgid, SIGINT, KERNEL);
				}
				break;
			}
			if(ch == tty->termios.c_cc[VQUIT]) {
				if(tty->pgid > 0) {
					kill_pgrp(tty->pgid, SIGQUIT, KERNEL);
				}
				break;
			}
			if(ch == tty->termios.c_cc[VSUSP]) {
				if(tty->pgid > 0) {
					kill_pgrp(tty->pgid, SIGTSTP, KERNEL);
				}
				break;
			}
		}

		if(tty->termios.c_iflag & ISTRIP) {
			ch = TOASCII(ch);
		}
		if(tty->termios.c_iflag & IUCLC) {
			if(ISUPPER(ch)) {
				ch = TOLOWER(ch);
			}
		}

		if(!(tty->flags & TTY_HAS_LNEXT)) {
			if(ch == '\r') {
				if(tty->termios.c_iflag & IGNCR) {
					continue;
				}
				if(tty->termios.c_iflag & ICRNL) {
					ch = '\n';
				}
			} else {
				if(ch == '\n') {
					if(tty->termios.c_iflag & INLCR) {
						ch = '\r';
					}
				}
			}
		}

		if(tty->termios.c_lflag & ICANON && !(tty->flags & TTY_HAS_LNEXT)) {
			if(ch == tty->termios.c_cc[VERASE] || ch == tty->termios.c_cc[VWERASE] || ch == tty->termios.c_cc[VKILL]) {
				erase_char(tty, ch);
				continue;
			}

			if(ch == tty->termios.c_cc[VREPRINT]) {
				out_char(tty, ch);
				tty_queue_putchar(tty, &tty->write_q, '\n');
				cb = tty->cooked_q.head;
				while(cb) {
					for(n = 0; n < cb->end_off; n++) {
						if(n >= cb->start_off) {
							out_char(tty, cb->data[n]);
						}
					}
					cb = cb->next;
				}
				continue;
			}

			if(ch == tty->termios.c_cc[VLNEXT] && tty->termios.c_lflag & IEXTEN) {
				tty->flags |= TTY_HAS_LNEXT;
				if(tty->termios.c_lflag & ECHOCTL) {
					tty_queue_putchar(tty, &tty->write_q, '^');
					tty_queue_putchar(tty, &tty->write_q, '\b');
				}
				break;
			}

			if(tty->termios.c_iflag & IXON) {
				if(ch == tty->termios.c_cc[VSTART]) {
					tty->start(tty);
					continue;
				}
				if(ch == tty->termios.c_cc[VSTOP]) {
					tty->stop(tty);
					continue;
				}
				if(tty->termios.c_iflag & IXANY) {
					tty->start(tty);
				}
			}
		}

		/* FIXME: using ISSPACE here makes LNEXT working incorrectly */
		if(tty->termios.c_lflag & ICANON) {
			if(ISCNTRL(ch) && !ISSPACE(ch) && (tty->termios.c_lflag & ECHOCTL)) {
				out_char(tty, ch);
				tty_queue_putchar(tty, &tty->cooked_q, ch);
				tty->flags &= ~TTY_HAS_LNEXT;
				continue;
			}
			if(ch == '\n') {
				tty->canon_data = 1;
			}
		}

		if(tty->termios.c_lflag & ECHO) {
			out_char(tty, ch);
		} else {
			if((tty->termios.c_lflag & ECHONL) && (ch == '\n')) {
				out_char(tty, ch);
			}
		}
		tty_queue_putchar(tty, &tty->cooked_q, ch);
		tty->flags &= ~TTY_HAS_LNEXT;
	}
	tty->output(tty);
	if(!(tty->termios.c_lflag & ICANON) || ((tty->termios.c_lflag & ICANON) && tty->canon_data)) {
		wakeup(&do_select);
	}
	wakeup(&tty_read);
}

int tty_open(struct inode *i, struct fd *fd_table)
{
	int noctty_flag;
	struct tty *tty;
	int errno;
	 
	noctty_flag = fd_table->flags & O_NOCTTY;

	if(MAJOR(i->rdev) == SYSCON_MAJOR && MINOR(i->rdev) == 0) {
		if(!current->ctty) {
			return -ENXIO;
		}
	}

	if(MAJOR(i->rdev) == VCONSOLES_MAJOR && MINOR(i->rdev) == 0) {
		noctty_flag = 1;
	}

	if(!(tty = get_tty(i->rdev))) {
		printk("%s(): oops! (%x)\n", __FUNCTION__, i->rdev);
		printk("_syscondev = %x\n", _syscondev);
		return -ENXIO;
	}

	if(tty->open) {
		if((errno = tty->open(tty)) < 0) {
			return errno;
		}
	}
	tty->count++;
	tty->column = 0;

	if(SESS_LEADER(current) && !current->ctty && !noctty_flag && !tty->sid) {
		current->ctty = tty;
		tty->sid = current->sid;
		tty->pgid = current->pgid;
	}
	return 0;
}

int tty_close(struct inode *i, struct fd *fd_table)
{
	struct proc *p;
	struct tty *tty;
	int errno;

	if(!(tty = get_tty(i->rdev))) {
		printk("%s(): oops! (%x)\n", __FUNCTION__, i->rdev);
		return -ENXIO;
	}

	if(tty->close) {
		if((errno = tty->close(tty)) < 0) {
			return errno;
		}
	}
	tty->count--;
	if(!tty->count) {
		termios_reset(tty);
		tty->pgid = tty->sid = 0;

		/* this tty is no longer the controlling tty of any process */
		FOR_EACH_PROCESS(p) {
			if(p->ctty == tty) {
				p->ctty = NULL;
			}
			p = p->next;
		}
	}
	return 0;
}

int tty_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	unsigned int min;
	unsigned char ch;
	struct tty *tty;
	struct callout_req creq;
	int n;

	if(!(tty = get_tty(i->rdev))) {
		printk("%s(): oops! (%x)\n", __FUNCTION__, i->rdev);
		return -ENXIO;
	}

	/* only the foreground process group is allowed to read from the tty */
	if(current->ctty == tty && current->pgid != tty->pgid) {
		if(current->sigaction[SIGTTIN - 1].sa_handler == SIG_IGN || current->sigblocked & (1 << (SIGTTIN - 1)) || is_orphaned_pgrp(current->pgid)) {
			return -EIO;
		}
		kill_pgrp(current->pgid, SIGTTIN, KERNEL);
		return -ERESTART;
	}

	n = min = 0;
	while(count > 0) {
		if(tty->kbd.mode == K_RAW || tty->kbd.mode == K_MEDIUMRAW) {
			n = 0;
			while(n < count) {
				if((ch = tty_queue_getchar(&tty->read_q))) {
					buffer[n++] = ch;
				} else {
					break;
				}
			}
			if(n) {
				break;
			}
		}

		if(tty->termios.c_lflag & ICANON) {
			if((ch = LAST_CHAR(&tty->cooked_q))) {
				if(ch == '\n' || ch == tty->termios.c_cc[VEOL] || ch == tty->termios.c_cc[VEOF] || (tty->termios.c_lflag & IEXTEN && ch == tty->termios.c_cc[VEOL2] && tty->termios.c_cc[VEOL2] != 0)) {

					tty->canon_data = 0;
					/* EOF is not passed to the reading process */
					if(ch == tty->termios.c_cc[VEOF]) {
						tty_queue_unputchar(&tty->cooked_q);
					}

					while(n < count) {
						if((ch = tty_queue_getchar(&tty->cooked_q))) {
							buffer[n++] = ch;
						} else {
							break;
						}
					}
					break;
				}
			}
		} else {
			if(tty->termios.c_cc[VTIME] > 0) {
				unsigned int ini_ticks = kstat.ticks;
				unsigned int timeout;

				if(!tty->termios.c_cc[VMIN]) {
					/* VTIME is measured in tenths of second */
					timeout = tty->termios.c_cc[VTIME] * (HZ / 10);

					while(kstat.ticks - ini_ticks < timeout && !tty->cooked_q.count) {
						creq.fn = wait_vtime_off;
						creq.arg = (unsigned int)&tty->cooked_q;
						add_callout(&creq, timeout);
						if(fd_table->flags & O_NONBLOCK) {
							return -EAGAIN;
						}
						if(sleep(&tty_read, PROC_INTERRUPTIBLE)) {
							return -EINTR;
						}
					}
					while(n < count) {
						if((ch = tty_queue_getchar(&tty->cooked_q))) {
							buffer[n++] = ch;
						} else {
							break;
						}
					}
					break;
				} else {
					if(tty->cooked_q.count > 0) {
						if(n < MIN(tty->termios.c_cc[VMIN], count)) {
							ch = tty_queue_getchar(&tty->cooked_q);
							buffer[n++] = ch;
						}
						if(n >= MIN(tty->termios.c_cc[VMIN], count)) {
							del_callout(&creq);
							break;
						}
						timeout = tty->termios.c_cc[VTIME] * (HZ / 10);
						creq.fn = wait_vtime_off;
						creq.arg = (unsigned int)&tty->cooked_q;
						add_callout(&creq, timeout);
						if(fd_table->flags & O_NONBLOCK) {
							n = -EAGAIN;
							break;
						}
						if(sleep(&tty_read, PROC_INTERRUPTIBLE)) {
							n = -EINTR;
							break;
						}
						if(!tty->cooked_q.count) {
							break;
						}
						continue;
					}
				}
			} else {
				if(tty->cooked_q.count > 0) {
					if(min < tty->termios.c_cc[VMIN] || !tty->termios.c_cc[VMIN]) {
						if(n < count) {
							ch = tty_queue_getchar(&tty->cooked_q);
							buffer[n++] = ch;
							if(--tty->canon_data < 0) {
								tty->canon_data = 0;
							}
						}
						min++;
					}
				}
				if(min >= tty->termios.c_cc[VMIN]) {
					break;
				}
			}
		}
		if(fd_table->flags & O_NONBLOCK) {
			n = -EAGAIN;
			break;
		}
		if(sleep(&tty_read, PROC_INTERRUPTIBLE)) {
			n = -EINTR;
			break;
		}
	}

	if(n) {
		i->i_atime = CURRENT_TIME;
	}
	return n;
}

int tty_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	unsigned char ch;
	struct tty *tty;
	int n;

	if(!(tty = get_tty(i->rdev))) {
		printk("%s(): oops! (%x)\n", __FUNCTION__, i->rdev);
		return -ENXIO;
	}

	/* only the foreground process group is allowed to write to the tty */
	if(current->ctty == tty && current->pgid != tty->pgid) {
		if(tty->termios.c_lflag & TOSTOP) {
			if(current->sigaction[SIGTTIN - 1].sa_handler != SIG_IGN && !(current->sigblocked & (1 << (SIGTTIN - 1)))) {
				if(is_orphaned_pgrp(current->pgid)) {
					return -EIO;
				}
				kill_pgrp(current->pgid, SIGTTOU, KERNEL);
				return -ERESTART;
			}
		}
	}

	n = 0;
	for(;;) {
		if(current->sigpending & ~current->sigblocked) {
			return -ERESTART;
		}
		while(count && n < count) {
			ch = *(buffer + n);
			/* FIXME: check if *(buffer + n) address is valid */
			if(opost(tty, ch) < 0) {
				break;
			}
			n++;
		}
		tty->output(tty);
		if(n == count) {
			break;
		}
		if(fd_table->flags & O_NONBLOCK) {
			n = -EAGAIN;
			break;
		}
		if(tty->write_q.count > 0) {
			if(sleep(&tty_write, PROC_INTERRUPTIBLE)) {
				n = -EINTR;
				break;
			}
		}
		if(need_resched) {
			do_sched();
		}
	}

	if(n) {
		i->i_mtime = CURRENT_TIME;
	}
	return n;
}

/* FIXME: http://www.lafn.org/~dave/linux/termios.txt (doc/termios.txt) */
int tty_ioctl(struct inode *i, int cmd, unsigned long int arg)
{
	struct proc *p;
	struct tty *tty;
	int errno;

	if(!(tty = get_tty(i->rdev))) {
		printk("%s(): oops! (%x)\n", __FUNCTION__, i->rdev);
		return -ENXIO;
	}

	switch(cmd) {
		/*
		 * Fetch and store the current terminal parameters to a termios
		 * structure pointed to by the argument.
		 */
		case TCGETS:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(struct termios)))) {
				return errno;
			}
			memcpy_b((struct termios *)arg, &tty->termios, sizeof(struct termios));
			break;

		/*
		 * Set the current terminal parameters according to the
		 * values in the termios structure pointed to by the argument.
		 */
		case TCSETS:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct termios)))) {
				return errno;
			}
			set_termios(tty, (struct termios *)arg);
			break;

		/*
		 * Same as TCSETS except it doesn't take effect until all
		 * the characters queued for output have been transmitted.
		 */
		case TCSETSW:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct termios)))) {
				return errno;
			}
			/* not tested */
			while(tty->write_q.count) {
				if(sleep(&tty_write, PROC_INTERRUPTIBLE)) {
					return -EINTR;
				}
				do_sched();
			}
			set_termios(tty, (struct termios *)arg);
			break;

		/*
		 * Same as TCSETSW except that all characters queued for
		 * input are discarded.
		 */
		case TCSETSF:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct termios)))) {
				return errno;
			}
			/* not tested */
			while(tty->write_q.count) {
				if(sleep(&tty_write, PROC_INTERRUPTIBLE)) {
					return -EINTR;
				}
				do_sched();
			}
			set_termios(tty, (struct termios *)arg);
			tty_queue_flush(&tty->read_q);
			break;

		/*
		 * Fetch and store the current terminal parameters to a termio
		 * structure pointed to by the argument.
		 */
		case TCGETA:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(struct termio)))) {
				return errno;
			}
			termios2termio(&tty->termios, (struct termio *)arg);
			break;

		/*
		 * Set the current terminal parameters according to the
		 * values in the termio structure pointed to by the argument.
		 */
		case TCSETA:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct termio)))) {
				return errno;
			}
			set_termio(tty, (struct termio *)arg);
			break;

		/*
		 * Same as TCSET except it doesn't take effect until all
		 * the characters queued for output have been transmitted.
		 */
		case TCSETAW:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct termio)))) {
				return errno;
			}
			/* not tested */
			while(tty->write_q.count) {
				if(sleep(&tty_write, PROC_INTERRUPTIBLE)) {
					return -EINTR;
				}
				do_sched();
			}
			set_termio(tty, (struct termio *)arg);
			break;

		/*
		 * Same as TCSETAW except that all characters queued for
		 * input are discarded.
		 */
		case TCSETAF:
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct termio)))) {
				return errno;
			}
			/* not tested */
			while(tty->write_q.count) {
				if(sleep(&tty_write, PROC_INTERRUPTIBLE)) {
					return -EINTR;
				}
				do_sched();
			}
			set_termio(tty, (struct termio *)arg);
			tty_queue_flush(&tty->read_q);
			break;

		/* Perform start/stop control */
		case TCXONC:
			switch(arg) {
				case TCOOFF:
					tty->stop(tty);
					break;
				case TCOON:
					tty->start(tty);
					break;
				default:
					return -EINVAL;
			}
			break;
		case TCFLSH:
			switch(arg) {
				case TCIFLUSH:
					tty_queue_flush(&tty->read_q);
					tty_queue_flush(&tty->cooked_q);
					break;
				case TCOFLUSH:
					tty_queue_flush(&tty->write_q);
					break;
				case TCIOFLUSH:
					tty_queue_flush(&tty->read_q);
					tty_queue_flush(&tty->cooked_q);
					tty_queue_flush(&tty->write_q);
					break;
				default:
					return -EINVAL;
			}
			break;
		case TIOCSCTTY:
			if(SESS_LEADER(current) && (current->sid == tty->sid)) {
				return 0;
			}
			if(!SESS_LEADER(current) || current->ctty) {
				return -EPERM;
			}
			if(tty->sid) {
				if((arg == 1) && IS_SUPERUSER) {
					FOR_EACH_PROCESS(p) {
						if(p->ctty == tty) {
							p->ctty = NULL;
						}
						p = p->next;
					}
				} else {
					return -EPERM;
				}
			}
			current->ctty = tty;
			tty->sid = current->sid;
			tty->pgid = current->pgid;
			break;

		/*
		 * Get the process group ID of the '__pid_t' pointed to by
		 * the arg to the foreground processes group ID.
		 */
		case TIOCGPGRP:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(__pid_t)))) {
				return errno;
			}
			memcpy_b((void *)arg, &tty->pgid, sizeof(__pid_t));
			break;

		/*
		 * Associate the process pointed to by '__pid_t' in the arg to
		 * the value of the terminal.
		 */
		case TIOCSPGRP:
			if(arg < 1) {
				return -EINVAL;
			}
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(__pid_t)))) {
				return errno;
			}
			memcpy_b(&tty->pgid, (void *)arg, sizeof(__pid_t));
			break;

		/*
		 * The session ID of the terminal is fetched and stored in
		 * the '__pid_t' pointed to by the arg.
		case TIOCSID:	FIXME
		 */

		/*
		 * The terminal drivers notion of terminal size is stored in
		 * the 'winsize' structure pointed to by the arg.
		 */
		case TIOCGWINSZ:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(struct winsize)))) {
				return errno;
			}
			memcpy_b((void *)arg, &tty->winsize, sizeof(struct winsize));
			break;

		/*
		 * The terminal drivers notion of the terminal size is set
		 * to value in the 'winsize' structure pointed to by the arg.
		 */
		case TIOCSWINSZ:
		{
			struct winsize *ws = (struct winsize *)arg;
			short int changed;

			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(struct winsize)))) {
				return errno;
			}
			changed = 0;
			if(tty->winsize.ws_row != ws->ws_row) {
				changed = 1;
			}
			if(tty->winsize.ws_col != ws->ws_col) {
				changed = 1;
			}
			if(tty->winsize.ws_xpixel != ws->ws_xpixel) {
				changed = 1;
			}
			if(tty->winsize.ws_ypixel != ws->ws_ypixel) {
				changed = 1;
			}
			tty->winsize.ws_row = ws->ws_row;
			tty->winsize.ws_col = ws->ws_col;
			tty->winsize.ws_xpixel = ws->ws_xpixel;
			tty->winsize.ws_ypixel = ws->ws_ypixel;
			if(changed) {
				kill_pgrp(tty->pgid, SIGWINCH, KERNEL);
			}
		}
			break;
		case TIOCNOTTY:
			if(current->ctty != tty) {
				return -ENOTTY;
			}
			if(SESS_LEADER(current)) {
				disassociate_ctty(tty);
			}
			break;
		case TIOCLINUX:
		{
			int val = *(unsigned char *)arg;
			if((errno = check_user_area(VERIFY_READ, (void *)arg, sizeof(unsigned char)))) {
				return errno;
			}
			switch(val) {
				case 12:	/* get current console */
					return current_cons;
					break;
				default:
					return -EINVAL;
					break;
			}
			break;
		}

		default:
			return vt_ioctl(tty, cmd, arg);
	}
	return 0;
}

int tty_lseek(struct inode *i, __off_t offset)
{
	return -ESPIPE;
}

int tty_select(struct inode *i, int flag)
{
	struct tty *tty;

	if(!(tty = get_tty(i->rdev))) {
		printk("%s(): oops! (%x)\n", __FUNCTION__, i->rdev);
		return 0;
	}

	switch(flag) {
		case SEL_R:
			if(tty->cooked_q.count > 0) {
				if(!(tty->termios.c_lflag & ICANON) || ((tty->termios.c_lflag & ICANON) && tty->canon_data)) {
					return 1;
				}
			}
			break;
		case SEL_W:
			if(!tty->write_q.count) {
				return 1;
			}
			break;
	}
	return 0;
}

void tty_init(void)
{
	tty_queue_init();
	memset_b(tty_table, 0, sizeof(tty_table));
}
