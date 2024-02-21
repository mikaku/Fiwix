/*
 * fiwix/kernel/traps.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/traps.h>
#include <fiwix/cpu.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/signal.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/sched.h>

/*
 * PS/2 System Control Port B
 * ---------------------------------------
 * bit 7 -> system board RAM parity check
 * bit 6 -> channel check
 * bit 5 -> timer 2 (speaker time) output
 * bit 4 -> refresh request (toggle)
 * bit 3 -> channel check status
 * bit 2 -> parity check status
 * bit 1 -> speaker data status
 * bit 0 -> timer 2 gate to speaker status
 */
#define PS2_SYSCTRL_B	0x61	/* PS/2 system control port B (read) */

struct traps traps_table[NR_EXCEPTIONS] = {
	{ "Divide Error", do_divide_error, 0 },
	{ "Debug", do_debug, 0 },
	{ "NMI Interrupt", do_nmi_interrupt, 0 },
	{ "Breakpoint", do_breakpoint, 0 },
	{ "Overflow" , do_overflow, 0 },
	{ "BOUND Range Exceeded", do_bound, 0 },
	{ "Invalid Opcode", do_invalid_opcode, 0 },
	{ "Device Not Available (No Math Coprocessor)", do_no_math_coprocessor, 0 },
	{ "Double Fault", do_double_fault, 1 },
	{ "Coprocessor Segment Overrun", do_coprocessor_segment_overrun, 0 },
	{ "Invalid TSS", do_invalid_tss, 1 },
	{ "Segment Not Present", do_segment_not_present, 1 },
	{ "Stack-Segment Fault", do_stack_segment_fault, 1 },
	{ "General Protection", do_general_protection, 1 },
	{ "Page Fault", do_page_fault, 1 },
	{ "Intel reserved", do_reserved, 0 },
	{ "x87 FPU Floating-Point Error", do_floating_point_error, 0 },
	{ "Alignment Check", do_alignment_check, 1 },
	{ "Machine Check", do_machine_check, 0 },
	{ "SIMD Floating-Point Exception", do_simd_fault, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 },
	{ "Intel reserved", do_reserved, 0 }
};

void do_divide_error(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGFPE);
	return;
}

void do_debug(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGTRAP);
	return;
}

void do_nmi_interrupt(unsigned int trap, struct sigcontext *sc)
{
	unsigned char error;

	error = inport_b(PS2_SYSCTRL_B);

	printk("NMI received: ", error);
	switch(error) {
		case 0x80:
			printk("parity check occurred. Defective RAM chips?\n");
			break;
		default:
			printk("unknown error 0x%x\n", error);
			break;
	}

	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_breakpoint(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGTRAP);
	return;
}

void do_overflow(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_bound(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_invalid_opcode(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGILL);
	return;
}

void do_no_math_coprocessor(unsigned int trap, struct sigcontext *sc)
{
	/* floating-point emulation would go here */

	if(dump_registers(trap, sc)) {
		PANIC("No coprocessor/emulation found.\n");
	}
	send_sig(current, SIGILL);
	return;
}

void do_double_fault(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_coprocessor_segment_overrun(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGFPE);
	return;
}

void do_invalid_tss(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_segment_not_present(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGBUS);
	return;
}

void do_stack_segment_fault(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGBUS);
	return;
}

void do_general_protection(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

/* do_page_fault() resides in mm/fault.c */

void do_reserved(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_floating_point_error(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGFPE);
	return;
}

void do_alignment_check(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_machine_check(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void do_simd_fault(unsigned int trap, struct sigcontext *sc)
{
	if(dump_registers(trap, sc)) {
		PANIC("");
	}
	send_sig(current, SIGSEGV);
	return;
}

void trap_handler(unsigned int trap, struct sigcontext sc)
{
	traps_table[trap].handler(trap, &sc);

	/* avoids confusion with -RESTART return value */
	sc.err = -sc.err;
}

const char *elf_lookup_symbol(unsigned int addr)
{
	Elf32_Sym *sym;
	unsigned int n;

	sym = (Elf32_Sym *)symtab->sh_addr;
	for(n = 0; n < symtab->sh_size / sizeof(Elf32_Sym); n++, sym++) {
		if(ELF32_ST_TYPE(sym->st_info) != STT_FUNC) {
			continue;
		}
		if(addr >= sym->st_value && addr < (sym->st_value + sym->st_size)) {
			return (const char *)strtab->sh_addr + sym->st_name;
		}
	}
	return NULL;
}

void stack_backtrace(void)
{
	int n;
	unsigned int addr, *sp;
	const char *str;

	printk("Stack:\n");
	GET_ESP(sp);
	/* eip, cs, eflags, oldesp and oldss cannot be counted here */
	sp += (sizeof(struct sigcontext) / sizeof(unsigned int)) - 5;
	sp = (unsigned int *)sp;
	for(n = 1; n <= 32; n++) {
		printk(" %08x", *sp);
		sp++;
		if(!(n % 8)) {
			printk("\n");
		}
	}
	printk("Backtrace:\n");
	GET_ESP(sp);
	/* eip, cs, eflags, oldesp and oldss cannot be counted here */
	sp += (sizeof(struct sigcontext) / sizeof(unsigned int)) - 5;
	sp = (unsigned int *)sp;
	for(n = 0; n < 256; n++) {
		addr = *sp;
		str = elf_lookup_symbol(addr);
		if(str) {
			printk("<0x%08x> %s()\n", addr, str);
		}
		sp++;
	}
}

int dump_registers(unsigned int trap, struct sigcontext *sc)
{
	unsigned int cr2;

	printk("\n");
	if(trap == 14) {	/* Page Fault */
		GET_CR2(cr2);
		printk("%s at 0x%08x (%s) with error code 0x%08x (0b%b)\n", traps_table[trap].name, cr2, sc->err & PFAULT_W ? "writing" : "reading", sc->err, sc->err);
	} else {
		printk("EXCEPTION: %s", traps_table[trap].name);
		if(traps_table[trap].errcode) {
			printk(": error code 0x%08x (0b%b)", sc->err, sc->err);
		}
		printk("\n");
	}

	printk("Process '%s' with pid %d", current->argv0, current->pid);
	if(sc->cs == KERNEL_CS) {
		printk(" in '%s()'.", elf_lookup_symbol(sc->eip));
	}
	printk("\n");

	printk(" cs: 0x%08x\teip: 0x%08x\tefl: 0x%08x\t ss: 0x%08x\tesp: 0x%08x\n", sc->cs, sc->eip, sc->eflags, sc->oldss, sc->oldesp);
	printk("eax: 0x%08x\tebx: 0x%08x\tecx: 0x%08x\tedx: 0x%08x\n", sc->eax, sc->ebx, sc->ecx, sc->edx);
	printk("esi: 0x%08x\tedi: 0x%08x\tesp: 0x%08x\tebp: 0x%08x\n", sc->esi, sc->edi, sc->esp, sc->ebp);
	printk(" ds: 0x%08x\t es: 0x%08x\t fs: 0x%08x\t gs: 0x%08x\n", sc->ds, sc->es, sc->fs, sc->gs);

	if(sc->cs == KERNEL_CS) {
		stack_backtrace();
	}

	/* panics if the exception has been in kernel mode */
	if(current->flags & PF_KPROC || sc->cs == KERNEL_CS) {
		return 1;
	}

	return 0;
}
