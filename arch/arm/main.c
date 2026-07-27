/* SPDX-License-Identifier: GPL-2.0-or-later */

typedef unsigned int u32;
typedef unsigned char u8;

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
#define ARM_L1_TABLE 0x47E00000U
#define ARM_USER_PHYS 0x47000000U
#define ARM_USER_VIRT 0x00100000U
#define ARM_USER_STACK_PHYS 0x47100000U
#define ARM_USER_STACK_VIRT 0x00200000U
#define ARM_USER_STACK_TOP 0x00300000U
#define ARM_KERNEL_PROBE 0x40010000U
#define ARM_SECTION_SIZE 0x00100000U
#define ARM_SECTION_SUPERVISOR 0x0001140EU
#define ARM_SECTION_USER 0x00011C0EU
#define ARM_SECTION_USER_XN 0x00011C1EU
#define ARM_SECTION_DEVICE_XN 0x00010412U

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
extern void arm_enter_user(u32 entry, u32 stack);
extern u8 arm_user_fixture[];
extern u8 arm_user_fixture_end[];
extern u8 arm_alignment_probe[];

volatile u32 arm_timer_fired;
volatile u32 arm_data_abort_seen;
volatile u32 arm_permission_abort_seen;
volatile u32 arm_expected_abort_address;

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

static void arm_enable_mmu(u32 table_address)
{
	u32 zero;
	u32 domain;
	u32 control;

	zero = 0;
	domain = 1;
	__asm__ volatile("dsb");
	__asm__ volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(zero));
	__asm__ volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(table_address));
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(domain));
	__asm__ volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(zero));
	__asm__ volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(zero));
	__asm__ volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(zero));
	__asm__ volatile("dsb");
	__asm__ volatile("isb");
	__asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(control));
	/* D-cache waits for the generic cache-maintenance contract. */
	control |= 0x00801001U;	/* XP, I-cache, MMU; alignment is already set */
	__asm__ volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(control));
	__asm__ volatile("isb");
}

static void arm_process_mmu_init(void)
{
	volatile u32 *table;
	u8 *source;
	u8 *destination;
	u32 address;
	u32 n;
	u32 user_bytes;

	user_bytes = (u32)arm_user_fixture_end - (u32)arm_user_fixture;
	if(!user_bytes || user_bytes > ARM_SECTION_SIZE) {
		uart_puts("arm process failure: user image size\n");
		arm_poweroff();
	}
	source = arm_user_fixture;
	destination = (u8 *)ARM_USER_PHYS;
	for(n = 0; n < user_bytes; n++) {
		destination[n] = source[n];
	}

	table = (volatile u32 *)ARM_L1_TABLE;
	for(n = 0; n < 4096; n++) {
		table[n] = 0;
	}
	for(address = 0x40000000U; address < 0x48000000U;
		address += ARM_SECTION_SIZE) {
		table[address >> 20] = address | ARM_SECTION_SUPERVISOR;
	}
	table[0x080] = 0x08000000U | ARM_SECTION_DEVICE_XN;
	table[0x081] = 0x08100000U | ARM_SECTION_DEVICE_XN;
	table[0x090] = 0x09000000U | ARM_SECTION_DEVICE_XN;
	table[0x0A0] = 0x0A000000U | ARM_SECTION_DEVICE_XN;
	table[ARM_USER_VIRT >> 20] = ARM_USER_PHYS | ARM_SECTION_USER;
	table[ARM_USER_STACK_VIRT >> 20] =
		ARM_USER_STACK_PHYS | ARM_SECTION_USER_XN;

	arm_expected_abort_address = ARM_USER_VIRT +
		((u32)arm_alignment_probe - (u32)arm_user_fixture) + 1;
	arm_enable_mmu(ARM_L1_TABLE);
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
			if(frame->r[0] || !arm_timer_fired ||
				!arm_data_abort_seen ||
				!arm_permission_abort_seen) {
				uart_puts("arm trap failure: user exit r0=0x");
				uart_puthex32(frame->r[0]);
				uart_puts(" timer=0x");
				uart_puthex32(arm_timer_fired);
				uart_puts(" abort=0x");
				uart_puthex32(arm_data_abort_seen);
				uart_puts(" permission=0x");
				uart_puthex32(arm_permission_abort_seen);
				uart_putc('\n');
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
		frame->r[6] = 1;
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

	__asm__ volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(dfsr));
	__asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(dfar));
	if((dfsr & 0x40FU) == 1 &&
		dfar == arm_expected_abort_address) {
		arm_data_abort_seen = 1;
		frame->r[5] = 1;
		uart_puts("Fiwix ARMv7 data abort passed\n");
		return;
	}
	if((dfsr & 0x40FU) == 0xDU && dfar == ARM_KERNEL_PROBE) {
		arm_permission_abort_seen = 1;
		frame->r[4] = 1;
		uart_puts("Fiwix ARMv7 permission abort passed\n");
		return;
	}
	uart_puts("arm trap failure: data abort dfsr=0x");
	uart_puthex32(dfsr);
	uart_puts(" dfar=0x");
	uart_puthex32(dfar);
	uart_putc('\n');
	arm_poweroff();
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
	arm_process_mmu_init();
	uart_puts("arm process page tables passed\n");
	gic_init();
	timer_ticks = arm_timer_frequency() >> 10;
	if(!timer_ticks) {
		timer_ticks = 1000;
	}
	arm_timer_fired = 0;
	arm_data_abort_seen = 0;
	arm_permission_abort_seen = 0;
	__asm__ volatile("dsb");
	arm_timer_set(timer_ticks);
	uart_puts("arm vector and timer setup passed\n");
	arm_enter_user(ARM_USER_VIRT, ARM_USER_STACK_TOP);
	uart_puts("arm trap failure: returned from USR mode\n");
	arm_poweroff();
}
