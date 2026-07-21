/*
 * fiwix/arch/riscv64/vm.c
 *
 * Copyright 2026, Fiwix riscv64 contributors.
 * Distributed under the terms of the Fiwix License.
 */

#define PAGE_SIZE       4096UL
#define USER_TEXT_VA    0x00400000UL
#define USER_STACK_VA   0x00800000UL
#define KERNEL_RAM_PA   0x80000000UL
#define UART_PA         0x10000000UL

#define SATP_SV39       (8UL << 60)
#define PTE_V           0x001UL
#define PTE_R           0x002UL
#define PTE_W           0x004UL
#define PTE_X           0x008UL
#define PTE_U           0x010UL
#define PTE_A           0x040UL
#define PTE_D           0x080UL

typedef unsigned long u64;

static u64 root_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 low_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 user_text_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 user_stack_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 user_stack[PAGE_SIZE / sizeof(u64)]
	__attribute__((aligned(PAGE_SIZE)));

extern char __user_text_start[];
extern void riscv64_user_start(void);

static u64 table_entry(void *table)
{
	return (((u64)table >> 12) << 10) | PTE_V;
}

static u64 leaf_entry(u64 physical, u64 flags)
{
	return ((physical >> 12) << 10) | flags | PTE_V | PTE_A | PTE_D;
}

void riscv64_vm_enable(void)
{
	u64 satp;

	/* Root index 2 is the 1 GiB identity window containing kernel RAM. */
	root_page_table[2] = leaf_entry(KERNEL_RAM_PA, PTE_R | PTE_W | PTE_X);
	root_page_table[0] = table_entry(low_page_table);

	/* Supervisor-only 2 MiB leaves cover the finisher and UART. */
	low_page_table[0] = leaf_entry(0, PTE_R | PTE_W);
	low_page_table[UART_PA >> 21] = leaf_entry(UART_PA, PTE_R | PTE_W);

	low_page_table[USER_TEXT_VA >> 21] = table_entry(user_text_page_table);
	user_text_page_table[0] = leaf_entry((u64)__user_text_start,
		PTE_R | PTE_X | PTE_U);

	low_page_table[USER_STACK_VA >> 21] = table_entry(user_stack_page_table);
	user_stack_page_table[0] = leaf_entry((u64)user_stack,
		PTE_R | PTE_W | PTE_U);

	satp = SATP_SV39 | ((u64)root_page_table >> 12);
	__asm__ __volatile__("csrw satp, %0\n\tsfence.vma zero, zero"
		: : "r"(satp) : "memory");
	/* Permit supervisor syscall handlers to read validated user pages. */
	__asm__ __volatile__("li t0, 0x40000\n\tcsrs sstatus, t0"
		: : : "t0", "memory");
}

u64 riscv64_user_entry(void)
{
	return USER_TEXT_VA + ((u64)riscv64_user_start -
		(u64)__user_text_start);
}

u64 riscv64_user_stack_top(void)
{
	return USER_STACK_VA + PAGE_SIZE;
}
