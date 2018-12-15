/*
 * fiwix/kernel/pic.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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

/* interrupt vector base addresses */
#define IRQ0_ADDR	0x20
#define IRQ8_ADDR	0x28

/*
 * pic.c implements a bottom half table using a singly linked list.
 *
 *  head                           tail
 * +---------+  ----------+  ...  ----------+
 * |data|next|  |data|next|  ...  |data|next|
 * |    |  -->  |    |  -->  ...  |    |  / |
 * +---------+  ----------+  ...  ----------+
 *  (bh)         (bh)              (bh)
 */

struct bh bh_pool[NR_BH];
struct bh *bh_pool_head;
struct bh *bh_head;
struct bh *bh_tail;

static struct bh *get_free_bh(void)
{
	struct bh *new;

	new = NULL;
	if(bh_pool_head) {
		new = bh_pool_head;
		bh_pool_head = bh_pool_head->next;
		new->next = NULL;
	}
	return new;
}

static void put_free_bh(struct bh *old)
{
	old->next = bh_pool_head;
	bh_pool_head = old;
}

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

void add_bh(void (*fn)(void))
{
	unsigned long int flags;
	struct bh *b;

	SAVE_FLAGS(flags); CLI();

	if(!(b = get_free_bh())) {
		RESTORE_FLAGS(flags);
		PANIC("no more bottom half slots!\n");
	}

	/* initialize bh */
	memset_b(b, NULL, sizeof(struct bh));
	b->fn = fn;

	if(!bh_tail) {
		bh_head = bh_tail = b;
	} else {
		bh_tail->next = b;
		bh_tail = b;
	}

	RESTORE_FLAGS(flags);
	return;
}

void del_bh(void)
{
	unsigned long int flags;
	struct bh *b;

	if(!bh_head) {
		return;
	}

	SAVE_FLAGS(flags); CLI();

	b = bh_head;
	if(bh_head == bh_tail) {
		bh_head = bh_tail = NULL;
	} else {
		bh_head = bh_head->next;
	}
	put_free_bh(b);

	RESTORE_FLAGS(flags);
	return;
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

int register_irq(int irq, char *name, void *addr)
{
	if(irq < 0  || irq >= NR_IRQS) {
		return -EINVAL;
	}

	if(irq_table[irq].registered) {
		printk("WARNING: %s(): interrupt %d already registered!\n", __FUNCTION__, irq);
		return -EINVAL;
	}
	irq_table[irq].ticks = 0;
	irq_table[irq].name = name;
	irq_table[irq].registered = 1;
	irq_table[irq].handler = addr;
	return 0;
}

int unregister_irq(int irq)
{
	if(irq < 0  || irq >= NR_IRQS) {
		return -EINVAL;
	}

	if(!irq_table[irq].registered) {
		printk("WARNING: %s(): trying to unregister an unregistered interrupt %d.\n", __FUNCTION__, irq);
		return -EINVAL;
	}
	memset_b(&irq_table[irq], NULL, sizeof(struct interrupts));
	return 0;
}

/* each ISR points to this function */
void irq_handler(int irq, struct sigcontext sc)
{
	int real;

	/* this should help to detect hardware problems */
	if(irq == -1) {
		printk("Unknown IRQ received!\n");
		return;
	}

	/* spurious interrupt treatment */
	if(!irq_table[irq].handler) {
		real = pic_get_irq_reg(PIC_READ_ISR);
		if(!real) {
			/*
			 * If IRQ was not real and came from slave, then send
			 * an EOI to master because it doesn't know if the IRQ
			 * was a spurious interrupt from slave.
			 */
			if(irq > 7) {
				outport_b(PIC_MASTER, EOI);
			}
			if(kstat.sirqs < MAX_SPU_NOTICES) {
				printk("WARNING: spurious interrupt detected (unregistered IRQ %d).\n", irq);
			} else if(kstat.sirqs == MAX_SPU_NOTICES) {
				printk("WARNING: too many spurious interrupts; not logging any more.\n");
			}
			kstat.sirqs++;
			return;
		}
		if(irq > 7) {
			outport_b(PIC_SLAVE, EOI);
		}
		outport_b(PIC_MASTER, EOI);
		return;
	}

	disable_irq(irq);
	if(irq > 7) {
		outport_b(PIC_SLAVE, EOI);
	}
	outport_b(PIC_MASTER, EOI);

	kstat.irqs++;
	irq_table[irq].ticks++;
	irq_table[irq].handler(&sc);
	enable_irq(irq);
}

/* do bottom halves (interrupts are (FIXME) enabled) */
void do_bh(void)
{
	struct bh *b;
	void (*fn)(void);

	if((b = bh_head)) {
		while(b) {
			fn = b->fn;
			b = b->next;
			del_bh();
			(*fn)();
		}
	}
}

void pic_init(void)
{
	int n;
	struct bh *b;

	memset_b(irq_table, NULL, sizeof(irq_table));
	memset_b(bh_pool, NULL, sizeof(bh_pool));

	/* bh free list initialization */
	bh_pool_head = NULL;
	n = NR_BH;
	while(n--) {
		b = &bh_pool[n];
		put_free_bh(b);
	}
	bh_head = bh_tail = NULL;

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
