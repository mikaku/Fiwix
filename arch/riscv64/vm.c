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
#define STACK_FRAME_SIZE 128UL
#define STACK_ARGV0      96UL
#define AT_NULL          0UL
#define AT_PAGESZ        6UL
#define AT_ENTRY         9UL

#define PTE_V           0x001UL
#define PTE_R           0x002UL
#define PTE_W           0x004UL
#define PTE_X           0x008UL
#define PTE_U           0x010UL
#define PTE_A           0x040UL
#define PTE_D           0x080UL

typedef unsigned long u64;

extern void riscv64_vm_install(u64);

static u64 root_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 low_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 user_text_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 user_stack_page_table[512] __attribute__((aligned(PAGE_SIZE)));
static u64 user_text[PAGE_SIZE / sizeof(u64)]
	__attribute__((aligned(PAGE_SIZE)));
static u64 user_stack[PAGE_SIZE / sizeof(u64)]
	__attribute__((aligned(PAGE_SIZE)));

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
	user_text_page_table[0] = leaf_entry((u64)user_text,
		PTE_R | PTE_X | PTE_U);

	low_page_table[USER_STACK_VA >> 21] = table_entry(user_stack_page_table);
	user_stack_page_table[0] = leaf_entry((u64)user_stack,
		PTE_R | PTE_W | PTE_U);

	satp = (u64)root_page_table >> 12;
	/* The assembly helper also enables SUM for validated syscall buffers. */
	riscv64_vm_install(satp);
}

unsigned char *riscv64_user_text_page(void)
{
	return (unsigned char *)user_text;
}

u64 riscv64_prepare_user_stack(u64 entry)
{
	u64 *words;
	unsigned char *name;
	u64 n;
	static const char process_name[] = "fiwix-riscv64-fixture";

	for(n = 0; n < PAGE_SIZE / sizeof(u64); n++) {
		user_stack[n] = 0;
	}
	words = (u64 *)((unsigned char *)user_stack + PAGE_SIZE -
		STACK_FRAME_SIZE);
	name = (unsigned char *)words + STACK_ARGV0;
	for(n = 0; n < sizeof(process_name); n++) {
		name[n] = process_name[n];
	}
	words[0] = 1;
	words[1] = USER_STACK_VA + PAGE_SIZE - STACK_FRAME_SIZE + STACK_ARGV0;
	words[2] = 0;
	words[3] = 0;
	words[4] = AT_PAGESZ;
	words[5] = PAGE_SIZE;
	words[6] = AT_ENTRY;
	words[7] = entry;
	words[8] = AT_NULL;
	words[9] = 0;
	return USER_STACK_VA + PAGE_SIZE - STACK_FRAME_SIZE;
}
