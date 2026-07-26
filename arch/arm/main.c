/* SPDX-License-Identifier: GPL-2.0-or-later */

typedef unsigned int u32;

#define PL011_BASE 0x09000000U
#define PL011_DR   (*(volatile u32 *)(PL011_BASE + 0x00U))
#define PL011_FR   (*(volatile u32 *)(PL011_BASE + 0x18U))
#define PL011_FR_TXFF 0x20U
#define GICD_BASE 0x08000000U
#define GICC_BASE 0x08010000U
#define GICD_CTLR (*(volatile u32 *)(GICD_BASE + 0x000U))
#define GICD_ISENABLER0 (*(volatile u32 *)(GICD_BASE + 0x100U))
#define GICD_ICENABLER0 (*(volatile u32 *)(GICD_BASE + 0x180U))
#define GICD_ICPENDR0 (*(volatile u32 *)(GICD_BASE + 0x280U))
#define GICD_IPRIORITYR7 (*(volatile u32 *)(GICD_BASE + 0x41CU))
#define GICC_CTLR (*(volatile u32 *)(GICC_BASE + 0x000U))
#define GICC_PMR (*(volatile u32 *)(GICC_BASE + 0x004U))
#define GICC_BPR (*(volatile u32 *)(GICC_BASE + 0x008U))
#define GICC_IAR (*(volatile u32 *)(GICC_BASE + 0x00CU))
#define GICC_EOIR (*(volatile u32 *)(GICC_BASE + 0x010U))
#define ARM_PHYS_TIMER_IRQ 30U

struct arm_trap_frame {
	u32 r[13];
	u32 pc;
	u32 cpsr;
	u32 user_sp;
	u32 user_lr;
	u32 vector;
};

extern u32 arm_boot_dtb;
extern void arm_poweroff(void);
extern void arm_trap_init(void);
extern void arm_enter_user(void);
extern u32 arm_alignment_probe;

volatile u32 arm_timer_fired;
volatile u32 arm_data_abort_seen;

static void uart_putc(int c)
{
	while(PL011_FR & PL011_FR_TXFF) {
	}
	PL011_DR = (u32)c;
}

static void uart_puts(const char *s)
{
	while(*s) {
		uart_putc(*s++);
	}
}

static void uart_puthex32(u32 value)
{
	static const char digits[] = "0123456789ABCDEF";
	int shift;

	for(shift = 28; shift >= 0; shift -= 4) {
		uart_putc(digits[(value >> shift) & 0x0FU]);
	}
}

static u32 read_cpsr(void)
{
	u32 value;

	__asm__ volatile("mrs %0, cpsr" : "=r"(value));
	return value;
}

static u32 read_midr(void)
{
	u32 value;

	__asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(value));
	return value;
}

static void arm_timer_set(u32 ticks)
{
	u32 control;

	control = 0;
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 1" : : "r"(control));
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 0" : : "r"(ticks));
	control = 1;
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 1" : : "r"(control));
	__asm__ volatile("isb");
}

static void arm_timer_disable(void)
{
	u32 control;

	control = 0;
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 1" : : "r"(control));
	__asm__ volatile("isb");
}

static u32 arm_timer_frequency(void)
{
	u32 frequency;

	__asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(frequency));
	return frequency;
}

static void gic_init(void)
{
	GICD_CTLR = 0;
	GICC_CTLR = 0;
	GICD_ICENABLER0 = 0xFFFFFFFFU;
	GICD_ICPENDR0 = 0xFFFFFFFFU;
	GICD_IPRIORITYR7 = 0x80808080U;
	GICD_ISENABLER0 = 1U << ARM_PHYS_TIMER_IRQ;
	GICC_BPR = 0;
	GICC_PMR = 0xFFU;
	GICC_CTLR = 1;
	GICD_CTLR = 1;
}

void arm_handle_svc(struct arm_trap_frame *frame)
{
	u32 fd;
	const char *buffer;
	u32 count;
	u32 n;

	switch(frame->r[7]) {
		case 4:
			fd = frame->r[0];
			if(fd != 1 && fd != 2) {
				frame->r[0] = (u32)-9;
				return;
			}
			buffer = (const char *)frame->r[1];
			count = frame->r[2];
			for(n = 0; n < count; n++) {
				uart_putc(buffer[n]);
			}
			frame->r[0] = count;
			return;
		case 1:
			if(frame->r[0] || !arm_timer_fired) {
				uart_puts("arm trap failure: user exit\n");
				arm_poweroff();
			}
			uart_puts("arm trap smoke passed\n");
			arm_poweroff();
			return;
		default:
			frame->r[0] = (u32)-38;
			return;
	}
}

void arm_handle_irq(struct arm_trap_frame *frame)
{
	u32 iar;
	u32 interrupt;

	(void)frame;
	iar = GICC_IAR;
	interrupt = iar & 0x3FFU;
	if(interrupt == ARM_PHYS_TIMER_IRQ) {
		arm_timer_disable();
		arm_timer_fired = 1;
		GICC_EOIR = iar;
		return;
	}
	GICC_EOIR = iar;
	uart_puts("arm trap failure: unexpected IRQ 0x");
	uart_puthex32(interrupt);
	uart_putc('\n');
	arm_poweroff();
}

void arm_handle_data_abort(struct arm_trap_frame *frame)
{
	u32 dfsr;
	u32 dfar;

	(void)frame;
	__asm__ volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(dfsr));
	__asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(dfar));
	if((dfsr & 0x40FU) != 1 ||
		dfar != (u32)&arm_alignment_probe + 1) {
		uart_puts("arm trap failure: data abort dfsr=0x");
		uart_puthex32(dfsr);
		uart_puts(" dfar=0x");
		uart_puthex32(dfar);
		uart_putc('\n');
		arm_poweroff();
	}
	arm_data_abort_seen = 1;
	uart_puts("Fiwix ARMv7 data abort passed\n");
}

void arm_handle_fatal(u32 vector, u32 cpsr, u32 pc)
{
	uart_puts("arm trap failure: vector=0x");
	uart_puthex32(vector);
	uart_puts(" cpsr=0x");
	uart_puthex32(cpsr);
	uart_puts(" pc=0x");
	uart_puthex32(pc);
	uart_putc('\n');
	arm_poweroff();
}

void arm_boot_main(void)
{
	u32 timer_ticks;

	uart_puts("Fiwix ARMv7 firmware-free boot\n");
	uart_puts("mode=0x");
	uart_puthex32(read_cpsr() & 0x1FU);
	uart_puts(" midr=0x");
	uart_puthex32(read_midr());
	uart_puts(" dtb=0x");
	uart_puthex32(arm_boot_dtb);
	uart_putc('\n');
	uart_puts("arm boot smoke passed\n");

	arm_trap_init();
	gic_init();
	timer_ticks = arm_timer_frequency() >> 10;
	if(!timer_ticks) {
		timer_ticks = 1000;
	}
	arm_timer_fired = 0;
	arm_data_abort_seen = 0;
	arm_timer_set(timer_ticks);
	uart_puts("arm vector and timer setup passed\n");
	arm_enter_user();
	uart_puts("arm trap failure: returned from USR mode\n");
	arm_poweroff();
}
