/*
 * fiwix/include/fiwix/serial.h
 *
 * Copyright 2020-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SERIAL_H
#define _FIWIX_SERIAL_H

#define SERIAL4_IRQ	4	/* IRQ for serial ports 1 and 3 */
#define SERIAL3_IRQ	3	/* IRQ for serial ports 2 and 4 */

#define NR_SERIAL	4	/* maximum number of serial ttys */
#define SERIAL_MAJOR	4	/* major number for /dev/ttyS[n] */
#define SERIAL_MINORS	NR_SERIAL
#define SERIAL_MSF	6	/* serial minor shift factor */

/* UART registers */
#define UART_TD		0	/* W:  Transmitter Holding Buffer */
#define UART_RD		0	/* R:  Receiver Buffer */
#define UART_DLL	0	/* RW: Divisor Latch Low Byte */
#define UART_DLH	1	/* RW: Divisor Latch High Byte */
#define UART_IER	1	/* RW: Interrupt Enable Register */
#define UART_IIR	2	/* R:  Interrupt Identification Register */
#define UART_FCR	2	/* W:  FIFO Control Register */
#define UART_LCR	3	/* RW: Line Control Register */
#define UART_MCR	4	/* RW: Modem Control Register */
#define UART_LSR	5	/* R:  Line Status Register */
#define UART_MSR	6	/* R:  Modem Status Register */
#define UART_SR		7	/* RW: Scratch Register */

/* Interrupt Enable Register */
#define UART_IER_RDAI	0x1	/* enable Received Data Available Interrupt  */
#define UART_IER_THREI	0x2	/* enable Transmitter Holding Register Empty Interrupt */
#define UART_IER_RLSI	0x4	/* enable Receiver Line Status Interrupt */
#define UART_IER_MSI	0x8	/* enable Modem Status Interrupt */

/* Interrupt Identification Register */
#define UART_IIR_NOINT	0x01	/* no interrupts pending */
#define UART_IIR_MKINT	0x06	/* mask all interrupt flags */
#define UART_IIR_MSI	0x00	/* Modem Status Interrupt */
#define UART_IIR_THREI	0x02	/* Transmitter Holding Register Empty Interrupt */
#define UART_IIR_RDAI	0x04	/* Received Data Available Interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver Line Status Interrupt */
#define UART_IIR_FIFOTO	0xC0	/* FIFO TimeOut interrupt */
#define UART_IIR_FIFO64	0x20	/* 64 byte FIFO enabled (16750 only) */
#define UART_IIR_FIFO	0x40	/* FIFO is enabled (still needs bit #7 on) */
#define UART_IIR_FIFOKO	0x80	/* FIFO is enabled, but unusable */

/* FIFO Control Register */
#define UART_FCR_FIFO	0x07	/* enable FIFO (clear receive and transmit) */
#define UART_FCR_CRCVR	0x02	/* clear receiver */
#define UART_FCR_CXMTR	0x04	/* clear transmitter */
#define UART_FCR_DMA	0x08	/* DMA mode select */
#define UART_FCR_FIFO64	0x20	/* enable 64 byte FIFO (16750 only) */
#define UART_FCR_FIFO14	0xC0	/* set to 14 bytes 'trigger level' FIFO */

/* Line Control Register */
#define UART_LCR_WL5	0x00	/* word length 5 bits */
#define UART_LCR_WL6	0x01	/* word length 6 bits */
#define UART_LCR_WL7	0x02	/* word length 7 bits */
#define UART_LCR_WL8	0x03	/* word length 8 bits */
#define UART_LCR_2STB	0x04	/* 2 stop bits */
#define UART_LCR_1STB	0x00	/* 1 stop bit */
#define UART_LCR_NP	0x00	/* no parity */
#define UART_LCR_OP	0x08	/* odd parity */
#define UART_LCR_EP	0x18	/* even parity */
#define UART_LCR_SBRK	0x40	/* Set Break enable */
#define UART_LCR_DLAB	0x80	/* Divisor Latch Access Bit */

/* Modem Control Register */
#define UART_MCR_DTR	0x1	/* Data Terminal Ready */
#define UART_MCR_RTS	0x2	/* Request To Send */
#define UART_MCR_OUT2	0x8	/* Auxiliary Output 2 */

/* Line Status Register */
#define UART_LSR_RDA	0x01	/* Received Data Available */
#define UART_LSR_OE	0x02	/* Overrun Error */
#define UART_LSR_PE	0x04	/* Parity Error */
#define UART_LSR_FE	0x08	/* Framing Error */
#define UART_LSR_BI	0x10	/* Break Interrupt */
#define UART_LSR_THRE	0x20	/* Transmitter Holding Register Empty */
#define UART_LSR_EDHR	0x40	/* Empty Data Holding Registers TD and SH */
#define UART_LSR_EFIFO	0x80	/* Error in Received FIFO */


#define UART_FIFO_SIZE	16	/* 16 bytes */
#define UART_HAS_FIFO	0x02	/* has FIFO working */
#define UART_IS_8250	0x04	/* is a 8250 chip */
#define UART_IS_16450	0x08	/* is a 16450 chip */
#define UART_IS_16550	0x10	/* is a 16550 chip */
#define UART_IS_16550A	0x20	/* is a 16550A chip */

struct serial {
	short int addr;		/* port I/O address */
	char irq;
	int baud;
	char *name;
	short int lctrl;	/* line control flags (8N1, 7E2, ...) */
	int flags;
	struct tty *tty;
	struct serial *next;
};

int serial_open(struct tty *);
int serial_close(struct tty *);
int serial_ioctl(struct tty *, int, unsigned long int);
void serial_write(struct tty *);
void irq_serial(int, struct sigcontext *);
void irq_serial_bh(void);
void serial_init(void);

#endif /* _FIWIX_SERIAL_H */
