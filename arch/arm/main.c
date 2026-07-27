/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <fiwix/arm_trap.h>
#include <fiwix/arm_vm.h>

typedef unsigned int u32;
typedef unsigned short u16;
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
#define ARM_CONTEXT_VIRT 0x00300000U
#define ARM_CONTEXT_PRIMARY_PHYS 0x47200000U
#define ARM_CONTEXT_ALTERNATE_PHYS 0x47300000U
#define ARM_L1_ALTERNATE 0x47E04000U
#define ARM_KERNEL_PROBE 0x40010000U

struct arm_elf32_header {
	u8 ident[16];
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
};

struct arm_elf32_program_header {
	u32 type;
	u32 offset;
	u32 vaddr;
	u32 paddr;
	u32 filesz;
	u32 memsz;
	u32 flags;
	u32 align;
};

extern u32 arm_boot_dtb;
extern void arm_poweroff(void);
extern void arm_trap_init(void);
extern void arm_enter_user(u32 entry, u32 stack);
extern u8 arm_user_fixture[];
extern u8 arm_user_fixture_end[];
extern u8 arm_alignment_probe[];
extern u8 arm_user_elf_start[];
extern u8 arm_user_elf_end[];

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

static u32 arm_load_elf32(void)
{
	struct arm_elf32_header *header;
	struct arm_elf32_program_header *program;
	u8 *source;
	u8 *destination;
	u32 source_bytes;
	u32 loaded;
	u32 n;
	u32 p;

	source = arm_user_elf_start;
	source_bytes = (u32)arm_user_elf_end - (u32)arm_user_elf_start;
	if(source_bytes < sizeof(struct arm_elf32_header)) {
		goto invalid;
	}
	header = (struct arm_elf32_header *)source;
	if(header->ident[0] != 0x7FU || header->ident[1] != 'E' ||
		header->ident[2] != 'L' || header->ident[3] != 'F' ||
		header->ident[4] != 1 || header->ident[5] != 1 ||
		header->ident[6] != 1 || header->type != 2 ||
		header->machine != 40 || header->version != 1 ||
		header->ehsize != sizeof(struct arm_elf32_header) ||
		header->phentsize != sizeof(struct arm_elf32_program_header) ||
		!header->phnum || header->phnum > 16 ||
		header->phoff > source_bytes ||
		header->phnum >
			(source_bytes - header->phoff) / header->phentsize ||
		header->entry < ARM_USER_VIRT ||
		header->entry >= ARM_USER_VIRT + ARM_VM_SECTION_SIZE) {
		goto invalid;
	}
	program = (struct arm_elf32_program_header *)
		(source + header->phoff);
	loaded = 0;
	for(p = 0; p < header->phnum; p++, program++) {
		if(program->type != 1) {
			continue;
		}
		if(program->offset > source_bytes ||
			program->filesz > source_bytes - program->offset ||
			program->memsz < program->filesz ||
			program->vaddr < ARM_USER_VIRT ||
			program->vaddr - ARM_USER_VIRT >= ARM_VM_SECTION_SIZE ||
			program->memsz > ARM_VM_SECTION_SIZE -
				(program->vaddr - ARM_USER_VIRT)) {
			goto invalid;
		}
		destination = (u8 *)(ARM_USER_PHYS +
			(program->vaddr - ARM_USER_VIRT));
		for(n = 0; n < program->filesz; n++) {
			destination[n] = source[program->offset + n];
		}
		for(; n < program->memsz; n++) {
			destination[n] = 0;
		}
		loaded++;
	}
	if(!loaded) {
		goto invalid;
	}
	return header->entry;

invalid:
	uart_puts("arm process failure: invalid ELF32 image\n");
	arm_poweroff();
	return 0;
}

static int arm_process_root_switch_gate(u32 *primary, u32 *alternate)
{
	volatile u32 *primary_value;
	volatile u32 *alternate_value;
	volatile u32 *visible;

	primary_value = (volatile u32 *)ARM_CONTEXT_PRIMARY_PHYS;
	alternate_value = (volatile u32 *)ARM_CONTEXT_ALTERNATE_PHYS;
	visible = (volatile u32 *)ARM_CONTEXT_VIRT;
	*primary_value = 0x13579BDFU;
	*alternate_value = 0x2468ACE0U;

	if(*visible != *primary_value ||
		arm_vm_context_activate(alternate) ||
		*visible != *alternate_value ||
		arm_vm_context_activate(primary) ||
		*visible != *primary_value) {
		return -1;
	}
	return 0;
}

static u32 arm_process_mmu_init(void)
{
	u32 *alternate;
	u32 *table;
	u8 *destination;
	u32 entry;
	u32 n;

	destination = (u8 *)ARM_USER_PHYS;
	for(n = 0; n < ARM_VM_SECTION_SIZE; n++) {
		destination[n] = 0;
	}
	entry = arm_load_elf32();

	alternate = (u32 *)ARM_L1_ALTERNATE;
	table = (u32 *)ARM_L1_TABLE;
	if(arm_vm_root_init(table) ||
		arm_vm_map_user_section(table, ARM_USER_VIRT,
			ARM_USER_PHYS, 1) ||
		arm_vm_map_user_section(table, ARM_USER_STACK_VIRT,
			ARM_USER_STACK_PHYS, 0) ||
		arm_vm_map_user_section(table, ARM_CONTEXT_VIRT,
			ARM_CONTEXT_PRIMARY_PHYS, 0) ||
		arm_vm_root_clone(alternate, table) ||
		arm_vm_map_user_section(alternate, ARM_CONTEXT_VIRT,
			ARM_CONTEXT_ALTERNATE_PHYS, 0)) {
		uart_puts("arm process failure: invalid page-table policy\n");
		arm_poweroff();
	}

	arm_expected_abort_address = ARM_USER_VIRT +
		((u32)arm_alignment_probe - (u32)arm_user_fixture) + 1;
	if(arm_vm_activate(table)) {
		uart_puts("arm process failure: invalid TTBR0 root\n");
		arm_poweroff();
	}
	if(arm_process_root_switch_gate(table, alternate)) {
		uart_puts("arm process failure: root switch\n");
		arm_poweroff();
	}
	uart_puts("arm process root switch passed\n");
	return entry;
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
	u32 user_entry;

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
	user_entry = arm_process_mmu_init();
	uart_puts("arm ELF32 load passed\n");
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
	arm_enter_user(user_entry, ARM_USER_STACK_TOP);
	uart_puts("arm trap failure: returned from USR mode\n");
	arm_poweroff();
}
