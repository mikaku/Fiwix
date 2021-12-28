/*
 * fiwix/kernel/irq.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/errno.h>
#include <fiwix/irq.h>
#include <fiwix/pic.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/sigcontext.h>
#include <fiwix/sleep.h>

struct interrupt *irq_table[NR_IRQS];
static struct bh *bh_table = NULL;

int register_irq(int num, struct interrupt *new_irq)
{
	struct interrupt **irq;

	if(num < 0  || num >= NR_IRQS) {
		return -EINVAL;
	}

	irq = &irq_table[num];

	while(*irq) {
		if(*irq == new_irq) {
			printk("WARNING: %s(): interrupt %d already registered!\n", __FUNCTION__, num);
			return -EINVAL;
		}
		irq = &(*irq)->next;
	}
	*irq = new_irq;
	new_irq->ticks = 0;
	return 0;
}

int unregister_irq(int num, struct interrupt *old_irq)
{
	struct interrupt **irq, *prev_irq;

	if(num < 0  || num >= NR_IRQS) {
		return -EINVAL;
	}

	irq = &irq_table[num];
	prev_irq = NULL;

	while(*irq) {
		if(*irq == old_irq) {
			if((*irq)->next) {
				printk("WARNING: %s(): cannot unregister interrupt %d.\n", __FUNCTION__, num);
				return -EINVAL;
			}
			*irq = NULL;
			if(prev_irq) {
				prev_irq->next = NULL;
			}
			break;
		}
		prev_irq = *irq;
		irq = &(*irq)->next;
	}
	return 0;
}

void add_bh(struct bh *new)
{
	unsigned long int flags;
	struct bh **b;

	SAVE_FLAGS(flags); CLI();

	b = &bh_table;
	while(*b) {
		b = &(*b)->next;
	}
	*b = new;

	RESTORE_FLAGS(flags);
}

/* each ISR points to this function (interrupts are disabled) */
void irq_handler(int num, struct sigcontext sc)
{
	struct interrupt *irq;

	disable_irq(num);

	irq = irq_table[num];

	/* spurious interrupt treatment */
	if(!irq) {
		spurious_interrupt(num);
		goto end;
	}

	ack_pic_irq(num);

	kstat.irqs++;
	irq->ticks++;
	do {
		irq->handler(num, &sc);
		irq = irq->next;
	} while(irq);

end:
	enable_irq(num);
}

void unknown_irq_handler(void)
{
	printk("Unknown IRQ received!\n");
	return;
}

/* execute bottom halves (interrupts are enabled) */
void do_bh(void)
{
	struct bh *b;
	void (*fn)(void);

	b = bh_table;
	while(b) {
		if(b->flags & BH_ACTIVE) {
			b->flags &= ~BH_ACTIVE;
			fn = b->fn;
			(*fn)();
		}
		b = b->next;
	}
}

void irq_init(void)
{
	memset_b(irq_table, NULL, sizeof(irq_table));
}
