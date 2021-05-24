/*
 * fiwix/include/fiwix/pic.h
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_PIC_H
#define _FIWIX_PIC_H

#include <fiwix/sigcontext.h>

#define NR_IRQS		16	/* hardware interrupts */
#define PIC_MASTER	0x20	/* I/O base address for master PIC */
#define PIC_SLAVE	0xA0	/* I/O base address for slave PIC */

#define DATA		0x01	/* offset to data port */
#define EOI		0x20	/* End-Of-Interrupt command code */

/* Inicialization Command Words */
#define ICW1_RESET	0x11	/* ICW1_INIT + ICW1_ICW4 */
#define CASCADE_IRQ	0x02
#define ICW4_8086EOI	0x01

#define PIC_READ_IRR	0x0A	/* OCW3 irq ready */
#define PIC_READ_ISR	0x0B	/* OCW3 irq service */

/* Operational Command Words */
#define OCW1		0xFF	/* mask (disable) all IRQs */

struct interrupt {
	unsigned int ticks;
	char *name;
	void (*handler)(int, struct sigcontext *);
	struct interrupt *next;
};
extern struct interrupt *irq_table[NR_IRQS];


#define BH_ACTIVE	0x01

struct bh {
	int flags;
	void (*fn)(void);
	struct bh *next;
};

void add_bh(struct bh *);
void enable_irq(int);
void disable_irq(int);
int register_irq(int, struct interrupt *);
int unregister_irq(int, struct interrupt *);
void irq_handler(int, struct sigcontext);
void do_bh(void);
void pic_init(void);

#endif /* _FIWIX_PIC_H */
