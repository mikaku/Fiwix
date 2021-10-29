/*
 * fiwix/kernel/pic.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/config.h>
#include <fiwix/limits.h>
#include <fiwix/errno.h>
#include <fiwix/pic.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/sigcontext.h>
#include <fiwix/sleep.h>

/* interrupt vector base addresses */
#define IRQ0_ADDR	0x20
#define IRQ8_ADDR	0x28

struct interrupt *irq_table[NR_IRQS];
static struct bh *bh_table = NULL;

/*
 * This sends the command OCW3 to PIC (master or slave) to obtain the register
 * values. Slave is chained and represents IRQs 8-15. Master represents IRQs
 * 0-7, with IRQ 2 being the chain.
 */
static unsigned short int pic_get_irq_reg(int ocw3)
{
	outport_b(PIC_MASTER, ocw3);
	outport_b(PIC_SLAVE, ocw3);
	return (inport_b(PIC_SLAVE) << 8) | inport_b(PIC_MASTER);
}

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

void enable_irq(int irq)
{
	int addr;

	addr = (irq > 7) ? PIC_SLAVE + DATA : PIC_MASTER + DATA;
	irq &= 0x0007;

	outport_b(addr, inport_b(addr) & ~(1 << irq));
}

void disable_irq(int irq)
{
	int addr;

	addr = (irq > 7) ? PIC_SLAVE + DATA : PIC_MASTER + DATA;
	irq &= 0x0007;

	outport_b(addr, inport_b(addr) | (1 << irq));
}

/* each ISR points to this function (interrupts are disabled) */
void irq_handler(int num, struct sigcontext sc)
{
	struct interrupt *irq;
	int real;

	if(num == -1) {
		printk("Unknown IRQ received!\n");
		return;
	}

	irq = irq_table[num];

	/* spurious interrupt treatment */
	if(!irq) {
		real = pic_get_irq_reg(PIC_READ_ISR);
		if(!real) {
			/*
			 * If IRQ was not real and came from slave, then send
			 * an EOI to master because it doesn't know if the IRQ
			 * was a spurious interrupt from slave.
			 */
			if(num > 7) {
				outport_b(PIC_MASTER, EOI);
			}
			if(kstat.sirqs < MAX_SPU_NOTICES) {
				printk("WARNING: spurious interrupt detected (unregistered IRQ %d).\n", num);
			} else if(kstat.sirqs == MAX_SPU_NOTICES) {
				printk("WARNING: too many spurious interrupts; not logging any more.\n");
			}
			kstat.sirqs++;
			return;
		}
		if(num > 7) {
			outport_b(PIC_SLAVE, EOI);
		}
		outport_b(PIC_MASTER, EOI);
		return;
	}

	if(num > 7) {
		outport_b(PIC_SLAVE, EOI);
	}
	outport_b(PIC_MASTER, EOI);

	kstat.irqs++;
	irq->ticks++;
	do {
		irq->handler(num, &sc);
		irq = irq->next;
	} while(irq);
}

/* do bottom halves (interrupts are enabled) */
void do_bh(void)
{
	struct bh *b;
	void (*fn)(void);

	if(!lock_area(AREA_BH)) {
		b = bh_table;
		while(b) {
			if(b->flags & BH_ACTIVE) {
				b->flags &= ~BH_ACTIVE;
				fn = b->fn;
				(*fn)();
			}
			b = b->next;
		}
		unlock_area(AREA_BH);
	}
}

void pic_init(void)
{
	memset_b(irq_table, NULL, sizeof(irq_table));

	/* remap interrupts for PIC1 */
	outport_b(PIC_MASTER, ICW1_RESET);
	outport_b(PIC_MASTER + DATA, IRQ0_ADDR);	/* ICW2 */
	outport_b(PIC_MASTER + DATA, 1 << CASCADE_IRQ);	/* ICW3 */
	outport_b(PIC_MASTER + DATA, ICW4_8086EOI);

	/* remap interrupts for PIC2 */
	outport_b(PIC_SLAVE, ICW1_RESET);
	outport_b(PIC_SLAVE + DATA, IRQ8_ADDR);		/* ICW2 */
	outport_b(PIC_SLAVE + DATA, CASCADE_IRQ);	/* ICW3 */
	outport_b(PIC_SLAVE + DATA, ICW4_8086EOI);

	/* mask all IRQs except cascade */
	outport_b(PIC_MASTER + DATA, ~(1 << CASCADE_IRQ));

	/* mask all IRQs */
	outport_b(PIC_SLAVE + DATA, OCW1);
}
