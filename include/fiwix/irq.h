/*
 * fiwix/include/fiwix/irq.h
 *
 * Copyright 2021-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_IRQ_H
#define _FIWIX_IRQ_H

#include <fiwix/sigcontext.h>

#define NR_IRQS		16	/* hardware interrupts */

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
	void (*fn)(struct sigcontext *);
	struct bh *next;
};

void add_bh(struct bh *);
int register_irq(int, struct interrupt *);
int unregister_irq(int, const struct interrupt *);
void irq_handler(int, struct sigcontext);
void unknown_irq_handler(void);
void do_bh(struct sigcontext);
void irq_init(void);

#endif /* _FIWIX_IRQ_H */
