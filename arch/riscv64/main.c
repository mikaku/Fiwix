/*
 * fiwix/arch/riscv64/main.c
 *
 * Copyright 2026, Fiwix riscv64 contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arch_process.h>

#define UART0_BASE     0x10000000UL
#define UART_THR       0
#define UART_LSR       5
#define UART_LSR_THRE  0x20
#define TEST_FINISHER  0x00100000UL
#define TEST_PASS      0x00005555U
#define TEST_FAIL      0x00013333U
#define USER_TEXT_VA   0x00400000UL
#define PAGE_SIZE      4096UL
#define SCAUSE_IRQ     (1UL << 63)
#define SCAUSE_SSIP    1UL
#define SCAUSE_U_ECALL 8UL
#define SYS_WRITE      64UL
#define SYS_EXIT       93UL

typedef unsigned long u64;
typedef unsigned char u8;

volatile u64 riscv64_timer_ticks;
volatile u64 riscv64_user_exit_status;

#define TASK_STACK_WORDS 512

static struct arch_context boot_context;
static struct arch_context task1_context;
static struct arch_context task2_context;
static u64 task1_stack[TASK_STACK_WORDS] __attribute__((aligned(16)));
static u64 task2_stack[TASK_STACK_WORDS] __attribute__((aligned(16)));
static volatile unsigned int task_switches;

extern void riscv64_context_switch(struct arch_context *,
	struct arch_context *);
extern void riscv64_vm_enable(void);
extern u64 riscv64_load_user_elf(void);
extern u64 riscv64_prepare_user_stack(u64);
extern u64 riscv64_enter_user(u64, u64);
extern int riscv64_virtio_block_gate(void);
extern int riscv64_ext2_gate(void);
extern int riscv64_linux_image_gate(void);
extern u64 riscv64_linux_image_entry(void);
extern void riscv64_linux_handoff(u64, u64, u64);
extern int riscv64_sbi_gate(void);

struct riscv64_trap_frame {
	u64 ra;
	u64 sp;
	u64 gp;
	u64 tp;
	u64 t0;
	u64 t1;
	u64 t2;
	u64 s0;
	u64 s1;
	u64 a0;
	u64 a1;
	u64 a2;
	u64 a3;
	u64 a4;
	u64 a5;
	u64 a6;
	u64 a7;
	u64 s2;
	u64 s3;
	u64 s4;
	u64 s5;
	u64 s6;
	u64 s7;
	u64 s8;
	u64 s9;
	u64 s10;
	u64 s11;
	u64 t3;
	u64 t4;
	u64 t5;
	u64 t6;
	u64 sepc;
	u64 sstatus;
	u64 stval;
};

typedef char arch_context_size_must_be_128[
	(sizeof(struct arch_context) == 128) ? 1 : -1];
typedef char trap_frame_size_must_be_272[
	(sizeof(struct riscv64_trap_frame) == 272) ? 1 : -1];

static void uart_putc(char c)
{
	volatile u8 *uart;

	uart = (volatile u8 *)UART0_BASE;
	while(!(uart[UART_LSR] & UART_LSR_THRE)) {
		/* polling is intentional during the bring-up milestone */
	}
	uart[UART_THR] = (u8)c;
}

static void uart_puts(const char *s)
{
	while(*s) {
		if(*s == '\n') {
			uart_putc('\r');
		}
		uart_putc(*s++);
	}
}

static void uart_puthex(u64 value)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	for(shift = 60; shift >= 0; shift -= 4) {
		uart_putc(digits[(value >> shift) & 0x0f]);
	}
}

static void finish(unsigned int status)
{
	volatile unsigned int *finisher;

	finisher = (volatile unsigned int *)TEST_FINISHER;
	*finisher = status;
	for(;;) {
		/* QEMU exits before this loop when the test device is present. */
	}
}

static void task2(void)
{
	for(;;) {
		task_switches++;
		riscv64_context_switch(&task2_context, &task1_context);
	}
}

static void task1(void)
{
	int n;

	for(n = 0; n < 3; n++) {
		task_switches++;
		riscv64_context_switch(&task1_context, &task2_context);
	}
	riscv64_context_switch(&task1_context, &boot_context);
	for(;;) {
		/* A resumed, exhausted test task is a port bug. */
	}
}

static void context_switch_gate(void)
{
	task1_context.ra = (u64)task1;
	task1_context.sp = (u64)(task1_stack + TASK_STACK_WORDS);
	task2_context.ra = (u64)task2;
	task2_context.sp = (u64)(task2_stack + TASK_STACK_WORDS);

	riscv64_context_switch(&boot_context, &task1_context);
	if(task_switches != 6) {
		uart_puts("Fiwix riscv64 context-switch gate failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 context-switch gate passed: 6 switches\n");
}

void riscv64_machine_main(u64 hartid, u64 dtb)
{
	uart_puts("Fiwix riscv64 milestone 1\n");
	uart_puts("hartid 0x");
	uart_puthex(hartid);
	uart_puts(" dtb 0x");
	uart_puthex(dtb);
	uart_puts("\nfirmware-free machine-mode entry passed\n");
}

void riscv64_supervisor_main(u64 hartid, u64 dtb)
{
	u64 entry;
	u64 status;
	int virtio_version;

	uart_puts("Fiwix riscv64 S-mode entry passed\n");
	context_switch_gate();
	while(riscv64_timer_ticks < 3) {
		__asm__ __volatile__("wfi");
	}
	uart_puts("Fiwix riscv64 timer gate passed: 3 ticks\n");
	if(riscv64_sbi_gate() < 0) {
		uart_puts("Fiwix riscv64 SBI gate failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 SBI base/time gate passed\n");
	entry = riscv64_load_user_elf();
	if(!entry) {
		uart_puts("Fiwix riscv64 ELF64 loader gate failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 ELF64 loader gate passed\n");
	riscv64_vm_enable();
	uart_puts("Fiwix riscv64 Sv39 gate passed\n");
	virtio_version = riscv64_virtio_block_gate();
	if(virtio_version == 1) {
		uart_puts("Fiwix riscv64 virtio-mmio v1 sector gate passed\n");
	} else if(virtio_version == 2) {
		uart_puts("Fiwix riscv64 virtio-mmio v2 sector gate passed\n");
	} else {
		uart_puts("Fiwix riscv64 virtio block gate failed\n");
		finish(TEST_FAIL);
	}
	if(riscv64_ext2_gate() < 0) {
		uart_puts("Fiwix riscv64 ext2 gate failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 ext2 file gate passed\n");
	status = riscv64_enter_user(entry, riscv64_prepare_user_stack(entry));
	if(status != 42) {
		uart_puts("Fiwix riscv64 U-mode exit syscall failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 U-mode exit syscall passed: 42\n");
	if(riscv64_linux_image_gate() < 0) {
		uart_puts("Fiwix riscv64 Linux Image gate failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 Linux Image header gate passed\n");
	riscv64_linux_handoff(riscv64_linux_image_entry(), hartid, dtb);
	finish(TEST_FAIL);
}

void riscv64_trap(u64 cause, u64 epc, u64 value)
{
	uart_puts("Fiwix riscv64 fatal machine trap\nmcause 0x");
	uart_puthex(cause);
	uart_puts(" mepc 0x");
	uart_puthex(epc);
	uart_puts(" mtval 0x");
	uart_puthex(value);
	uart_putc('\n');
	finish(TEST_FAIL);
}

void riscv64_supervisor_trap(u64 cause, u64 epc, u64 value)
{
	uart_puts("Fiwix riscv64 fatal supervisor trap\nscause 0x");
	uart_puthex(cause);
	uart_puts(" sepc 0x");
	uart_puthex(epc);
	uart_puts(" stval 0x");
	uart_puthex(value);
	uart_putc('\n');
	finish(TEST_FAIL);
}

u64 riscv64_user_trap(struct riscv64_trap_frame *frame, u64 cause)
{
	u64 address;
	u64 count;
	u64 n;
	const char *buffer;

	if(cause & SCAUSE_IRQ) {
		if((cause & ~SCAUSE_IRQ) == SCAUSE_SSIP) {
			__asm__ __volatile__("csrc sip, %0" : : "r"(SCAUSE_SSIP));
			riscv64_timer_ticks++;
			return 0;
		}
		riscv64_supervisor_trap(cause, frame->sepc, frame->stval);
	}

	if(cause != SCAUSE_U_ECALL) {
		riscv64_supervisor_trap(cause, frame->sepc, frame->stval);
	}
	frame->sepc += 4;

	if(frame->a7 == SYS_WRITE) {
		address = frame->a1;
		count = frame->a2;
		if(frame->a0 != 1 || address < USER_TEXT_VA ||
			count > PAGE_SIZE || address + count < address ||
			address + count > USER_TEXT_VA + PAGE_SIZE) {
			frame->a0 = (u64)-14;
			return 0;
		}
		buffer = (const char *)address;
		for(n = 0; n < count; n++) {
			uart_putc(buffer[n]);
		}
		frame->a0 = count;
		return 0;
	}

	if(frame->a7 == SYS_EXIT) {
		riscv64_user_exit_status = frame->a0;
		return 1;
	}

	frame->a0 = (u64)-38;
	return 0;
}
