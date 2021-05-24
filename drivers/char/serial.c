/*
 * fiwix/drivers/char/serial.c
 *
 * Copyright 2020-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/pic.h>
#include <fiwix/sleep.h>
#include <fiwix/serial.h>
#include <fiwix/tty.h>
#include <fiwix/ctype.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations serial_driver_fsop = {
	0,
	0,

	tty_open,
	tty_close,
	tty_read,
	tty_write,
	tty_ioctl,
	tty_lseek,
	NULL,			/* readdir */
	NULL,			/* mmap */
	tty_select,

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct device serial_device = {
	"ttyS",
	SERIAL_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	NULL,
	&serial_driver_fsop,
	NULL
};

/* 9600,N,8,1 by default */
struct serial serial_table[NR_SERIAL] = {
	{ 0x3F8, SERIAL4_IRQ, 9600, "ttyS0", UART_LCR_NP | UART_LCR_WL8 | UART_LCR_1STB, 0, NULL, NULL },
	{ 0x2F8, SERIAL3_IRQ, 9600, "ttyS1", UART_LCR_NP | UART_LCR_WL8 | UART_LCR_1STB, 0, NULL, NULL },
	{ 0x3E8, SERIAL4_IRQ, 9600, "ttyS2", UART_LCR_NP | UART_LCR_WL8 | UART_LCR_1STB, 0, NULL, NULL },
	{ 0x2E8, SERIAL3_IRQ, 9600, "ttyS3", UART_LCR_NP | UART_LCR_WL8 | UART_LCR_1STB, 0, NULL, NULL }
};

char *serial_chip[] = {
	NULL,
	"8250",
	"16450",
	"16550",
	"16550A",
};

static int baud_table[] = {
	0,
	50,
	75,
	110,
	134,
	150,
	200,
	300,
	600,
	1200,
	1800,
	2400,
	4800,
	9600,
	19200,
	38400,
	57200,
	115200,
	0
};

static struct serial *serial_ports = NULL;
static struct bh serial_bh = { 0, &irq_serial_bh, NULL };
static struct interrupt irq_config_serial0 = { 0, "serial", &irq_serial, NULL };
static struct interrupt irq_config_serial1 = { 0, "serial", &irq_serial, NULL };

static int is_serial(__dev_t dev)
{
	if(MAJOR(dev) == SERIAL_MAJOR && ((MINOR(dev) >= (1 << SERIAL_MSF) && MINOR(dev) < (1 << SERIAL_MSF) + SERIAL_MINORS))) {
		return 1;
	}

	return 0;
}

static int serial_identify(struct serial *s)
{
	int value;

	/* set all features in FCR register to test the status of FIFO */
	outport_b(s->addr + UART_FCR, (UART_FCR_FIFO |
					UART_FCR_DMA |
					UART_FCR_FIFO64 |
					UART_FCR_FIFO14));

	value = inport_b(s->addr + UART_IIR);
	if(value & UART_IIR_FIFOKO) {
		if(value & UART_IIR_FIFO) {
			if(value & UART_IIR_FIFO64) {
				/* 16750 chip is not supported */
			} else {
				s->flags |= UART_IS_16550A | UART_HAS_FIFO;
				return 4;
			}
		} else {
			s->flags |= UART_IS_16550;
			return 3;
		}
	} else {
		/*
		 * At this point we know this device don't has FIFO,
		 * the Scratch Register will help us to know the final chip.
		 */
		value = inport_b(s->addr + UART_SR);	/* save its value */
		outport_b(s->addr + UART_SR, 0xAA);	/* put a random value */
		if(inport_b(s->addr + UART_SR) != 0xAA) {
			s->flags |= UART_IS_8250;
			return 1;
		} else {
			outport_b(s->addr + UART_SR, value);	/* restore it */
			s->flags |= UART_IS_16450;
			return 2;
		}
	}
	return 0;
}

static void serial_default(struct serial *s)
{
	int divisor;

	outport_b(s->addr + UART_IER, 0);	/* disable all interrupts */

	divisor = 115200 / s->baud;
	outport_b(s->addr + UART_LCR, UART_LCR_DLAB);	/* enable DLAB */
	outport_b(s->addr + UART_DLL, divisor & 0xFF);	/* LSB of divisor */
	outport_b(s->addr + UART_DLH, divisor >> 8);	/* MSB of divisor */
	outport_b(s->addr + UART_LCR, s->lctrl);	/* line control */
}

/* disable transmitter interrupts */
static void serial_stop(struct tty *tty)
{
	struct serial *s;
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	s = (struct serial *)tty->driver_data;
	outport_b(s->addr + UART_IER, UART_IER_RDAI);
	RESTORE_FLAGS(flags);
}

/* enable transmitter interrupts */
static void serial_start(struct tty *tty)
{
	struct serial *s;
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	s = (struct serial *)tty->driver_data;
	outport_b(s->addr + UART_IER, UART_IER_RDAI | UART_IER_THREI);
	RESTORE_FLAGS(flags);
}

static void serial_deltab(struct tty *tty)
{
	unsigned short int col, n, count;
	struct cblock *cb;
	unsigned char ch;

	cb = tty->cooked_q.head;
	col = count = 0;

	while(cb) {
		for(n = 0; n < cb->end_off; n++) {
			if(n >= cb->start_off) {
				ch = cb->data[n];
				if(ch == '\t') {
					while(!tty->tab_stop[++col]);
				} else {
					col++;
					if(ISCNTRL(ch) && !ISSPACE(ch) && tty->termios.c_lflag & ECHOCTL) {
						col++;
					}
				}
				col %= 80;
			}
		}
		cb = cb->next;
	}
	count = tty->column - col;

	while(count--) {
		tty_queue_putchar(tty, &tty->write_q, '\b');
		tty->column--;
	}
}

static void serial_reset(struct tty *tty)
{
	termios_reset(tty);
	tty->winsize.ws_row = 25;
	tty->winsize.ws_col = 80;
	tty->winsize.ws_xpixel = 0;
	tty->winsize.ws_ypixel = 0;
	tty->lnext = 0;
}

static void serial_errors(struct serial *s, int status)
{
	struct tty *tty;

	tty = s->tty;

	if(!(tty->termios.c_iflag & IGNBRK) && tty->termios.c_iflag & BRKINT) {
		if(status & UART_LSR_BI) {
			printk("WARNING: break interrupt in %s.\n", s->name);
		}
	}

	/* this includes also overrun errors */
	if(!(tty->termios.c_iflag & IGNPAR) && tty->termios.c_iflag & PARMRK) {
		if(status & UART_LSR_OE) {
			printk("WARNING: overrun error in %s.\n", s->name);

		} else if(status & UART_LSR_PE) {
			printk("WARNING: parity error in %s.\n", s->name);

		} else if(status & UART_LSR_FE) {
			printk("WARNING: framing error in %s.\n", s->name);
	
		} else if(status & UART_LSR_EFIFO) {
			printk("WARNING: FIFO error in %s.\n", s->name);
		}
	}
}

static void serial_send(struct tty *tty)
{
	unsigned char ch;
	struct serial *s;
	int count;

	s = (struct serial *)tty->driver_data;

	if(!tty->write_q.count) {
		outport_b(s->addr + UART_IER, UART_IER_RDAI);
		return;
	}

	count = 0;
	while(tty->write_q.count > 0 && count < UART_FIFO_SIZE) {
		ch = tty_queue_getchar(&tty->write_q);
		outport_b(s->addr + UART_TD, ch);
		count++;
	}

	if(!tty->write_q.count) {
		outport_b(s->addr + UART_IER, UART_IER_RDAI);
	}
	wakeup(&tty_write);
}

static int serial_receive(struct serial *s)
{
	int status, errno;
	unsigned char ch;
	struct tty *tty;

	errno = 0;
	tty = s->tty;

	do {
		if(!tty_queue_room(&tty->read_q)) {
			errno = -EAGAIN;
			break;
		}
		ch = inport_b(s->addr + UART_RD);
		tty_queue_putchar(tty, &tty->read_q, ch);
		status = inport_b(s->addr + UART_LSR);
	} while(status & UART_LSR_RDA);

	serial_bh.flags |= BH_ACTIVE;
	return errno;
}

void irq_serial(int num, struct sigcontext *sc)
{
	struct serial *s;
	int status;

	s = serial_ports;

	if(s) {
		do {
			if(s->irq == num) {
				while(!(inport_b(s->addr + UART_IIR) & UART_IIR_NOINT)) {
					status = inport_b(s->addr + UART_LSR);
					if(status & UART_LSR_RDA) {
						if(serial_receive(s)) {
							break;
						}
					}
					if(status & UART_LSR_THRE) {
						serial_send(s->tty);
					}
					serial_errors(s, status);
				}
			}
			s = s->next;
		} while(s);
	}
}

int serial_open(struct tty *tty)
{
	struct serial *s;
	int minor;

	minor = MINOR(tty->dev);
	if(!TEST_MINOR(serial_device.minors, minor)) {
		return -ENXIO;
	}

	s = (struct serial *)tty->driver_data;

	/* enable FIFO */
	if(s->flags & UART_HAS_FIFO) {
		outport_b(s->addr + UART_FCR, UART_FCR_FIFO | UART_FCR_FIFO14);
	}
	outport_b(s->addr + UART_MCR, UART_MCR_OUT2 | UART_MCR_RTS | UART_MCR_DTR);

	/* enable interrupts */
	outport_b(s->addr + UART_IER, UART_IER_RDAI);

	/* clear all input registers */
	inport_b(s->addr + UART_RD);
	inport_b(s->addr + UART_IIR);
	inport_b(s->addr + UART_LSR);
	inport_b(s->addr + UART_MSR);

	return 0;
}

int serial_close(struct tty *tty)
{
	struct serial *s;
	int minor;

	minor = MINOR(tty->dev);
	if(!TEST_MINOR(serial_device.minors, minor)) {
		return -ENXIO;
	}

	s = (struct serial *)tty->driver_data;

	if(tty->count > 1) {
		return 0;
	}

	/* disable all interrupts */
	outport_b(s->addr + UART_IER, 0);

	/* disable FIFO */
	outport_b(s->addr + UART_FCR, UART_FCR_CRCVR | UART_FCR_CXMTR);

	/* clear all input register */
	inport_b(s->addr + UART_RD);

	return 0;
}

void serial_set_termios(struct tty *tty)
{
	short int divisor;
	int baud, size, stop;
	int lctrl;
	struct serial *s;

	s = (struct serial *)tty->driver_data;
	lctrl = 0;

	if(!(baud = tty->termios.c_cflag & CBAUD)) {
		return;
	}
	divisor = 115200 / baud_table[baud];

	outport_b(s->addr + UART_LCR, UART_LCR_DLAB);	/* enable DLAB */
	outport_b(s->addr + UART_DLL, divisor & 0xFF);	/* LSB of divisor */
	outport_b(s->addr + UART_DLH, divisor >> 8);	/* MSB of divisor */

	size = tty->termios.c_cflag & CSIZE;
	switch(size) {
		case CS5:
			lctrl = UART_LCR_WL5;
			break;
		case CS6:
			lctrl = UART_LCR_WL6;
			break;
		case CS7:
			lctrl = UART_LCR_WL7;
			break;
		case CS8:
			lctrl = UART_LCR_WL8;
			break;
		default:
			lctrl = UART_LCR_WL5;
			break;
	}

	stop = tty->termios.c_cflag & CSTOPB;
	if(stop) {
		lctrl |= UART_LCR_2STB;
	} else {
		lctrl |= UART_LCR_1STB;
	}

	if(tty->termios.c_cflag & PARENB) {
		lctrl |= UART_LCR_EP;
	} else if(tty->termios.c_cflag & PARODD) {
		lctrl |= UART_LCR_OP;
	} else {
		lctrl |= UART_LCR_NP;
	}

	/* FIXME: flow control RTSCTS no supported */

	outport_b(s->addr + UART_LCR, lctrl);	/* line control */
}

void serial_write(struct tty *tty)
{
	struct serial *s;
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	s = (struct serial *)tty->driver_data;
	outport_b(s->addr + UART_IER, UART_IER_RDAI | UART_IER_THREI);
	RESTORE_FLAGS(flags);
}

void irq_serial_bh(void)
{
	struct tty *tty;
	struct serial *s;

	s = serial_ports;

	if(s) {
		do {
			tty = s->tty;
			if(tty->read_q.count) {
				if(!lock_area(AREA_SERIAL_READ)) {
					tty->input(tty);
					unlock_area(AREA_SERIAL_READ);
				} else {
					serial_bh.flags |= BH_ACTIVE;
				}
			}
			s = s->next;
		} while(s);
	}
}

void serial_init(void)
{
	int n, n2, type, found;
	struct serial **sp, *s;
	struct tty *tty;

	for(n = 0, found = 0; n < SERIAL_MINORS; n++) {
		s = &serial_table[n];
		if((type = serial_identify(s))) {
			printk("ttyS%d     0x%04X-0x%04X    %d    type=%s%s\n", n, s->addr, s->addr + 7, s->irq, serial_chip[type], s->flags & UART_HAS_FIFO ? " FIFO=yes" : "");

			SET_MINOR(serial_device.minors, (1 << SERIAL_MSF) + n);
			serial_default(s);
			sp = &serial_ports;
			if(*sp) {
				do {
					sp = &(*sp)->next;
				} while(*sp);
			}
			if(!register_tty(MKDEV(SERIAL_MAJOR, (1 << SERIAL_MSF) + n))) {
				tty = get_tty(MKDEV(SERIAL_MAJOR, (1 << SERIAL_MSF) + n));
				tty->driver_data = (void *)s;
				tty->stop = serial_stop;
				tty->start = serial_start;
				tty->deltab = serial_deltab;
				tty->reset = serial_reset;
				tty->input = do_cook;
				tty->output = serial_write;
				tty->open = serial_open;
				tty->close = serial_close;
				tty->set_termios = serial_set_termios;
				serial_reset(tty);
				for(n2 = 0; n2 < MAX_TAB_COLS; n2++) {
					if(!(n2 % TAB_SIZE)) {
						tty->tab_stop[n2] = 1;
					} else {
						tty->tab_stop[n2] = 0;
					}
				}
				tty->count = 0;
				s->tty = tty;
				*sp = s;
				found++;
			} else {
				printk("WARNING: %s(): unable to register ttyS%d.\n", __FUNCTION__, n);
			}
		}
	}
	if(found) {
		add_bh(&serial_bh);
		if(register_device(CHR_DEV, &serial_device)) {
			printk("WARNING: %s(): unable to register serial device.\n", __FUNCTION__);
		}
		if(!register_irq(SERIAL4_IRQ, &irq_config_serial0)) {
			enable_irq(SERIAL4_IRQ);
		}
		if(found > 1) {
			if(!register_irq(SERIAL3_IRQ, &irq_config_serial1)) {
				enable_irq(SERIAL3_IRQ);
			}
		}

		if(is_serial(_syscondev)) {
			register_console(console_flush_log_buf);
		}
	}
}
