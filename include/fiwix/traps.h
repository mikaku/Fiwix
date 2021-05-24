/*
 * fiwix/include/fiwix/traps.h
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_TRAPS_H
#define _FIWIX_TRAPS_H

#include <fiwix/sigcontext.h>

#define NR_EXCEPTIONS	32

struct traps {
	char *name;
	void (*handler)(unsigned int, struct sigcontext *);
	char errcode;
};

void do_divide_error(unsigned int, struct sigcontext *);
void do_debug(unsigned int, struct sigcontext *);
void do_nmi_interrupt(unsigned int, struct sigcontext *);
void do_breakpoint(unsigned int, struct sigcontext *);
void do_overflow(unsigned int, struct sigcontext *);
void do_bound(unsigned int, struct sigcontext *);
void do_invalid_opcode(unsigned int, struct sigcontext *);
void do_no_math_coprocessor(unsigned int, struct sigcontext *);
void do_double_fault(unsigned int, struct sigcontext *);
void do_coprocessor_segment_overrun(unsigned int, struct sigcontext *);
void do_invalid_tss(unsigned int, struct sigcontext *);
void do_segment_not_present(unsigned int, struct sigcontext *);
void do_stack_segment_fault(unsigned int, struct sigcontext *);
void do_general_protection(unsigned int, struct sigcontext *);
void do_page_fault(unsigned int, struct sigcontext *);
void do_reserved(unsigned int, struct sigcontext *);
void do_floating_point_error(unsigned int, struct sigcontext *);
void do_alignment_check(unsigned int, struct sigcontext *);
void do_machine_check(unsigned int, struct sigcontext *);
void do_simd_fault(unsigned int, struct sigcontext *);

void trap_handler(unsigned int, struct sigcontext);

const char * elf_lookup_symbol(unsigned int addr);
void stack_backtrace(void);
int dump_registers(unsigned int, struct sigcontext *);

#endif /* _FIWIX_TRAPS_H */
