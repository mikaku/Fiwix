/*
 * fiwix/include/fiwix/asm.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ASM_H
#define _FIWIX_ASM_H

extern void except0(void);
extern void except1(void);
extern void except2(void);
extern void except3(void);
extern void except4(void);
extern void except5(void);
extern void except6(void);
extern void except7(void);
extern void except8(void);
extern void except9(void);
extern void except10(void);
extern void except11(void);
extern void except12(void);
extern void except13(void);
extern void except14(void);
extern void except15(void);
extern void except16(void);
extern void except17(void);
extern void except18(void);
extern void except19(void);
extern void except20(void);
extern void except21(void);
extern void except22(void);
extern void except23(void);
extern void except24(void);
extern void except25(void);
extern void except26(void);
extern void except27(void);
extern void except28(void);
extern void except29(void);
extern void except30(void);
extern void except31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);
extern void unknown_irq(void);

extern void switch_to_user_mode(void);
extern void sighandler_trampoline(void);
extern void end_sighandler_trampoline(void);
extern void syscall(void);
extern void return_from_syscall(void);
extern void do_switch(unsigned int *, unsigned int *, unsigned int, unsigned int, unsigned int, unsigned short int);

int cpuid(void);
int getfpu(void);
int get_cpu_vendor_id(void);
int signature_flags(void);
int brand_str(void);
int tlbinfo(void);

unsigned char inport_b(unsigned int);
unsigned short int inport_w(unsigned int);
unsigned int inport_l(unsigned int);
void inport_sw(unsigned int, void *, unsigned int);
void inport_sl(unsigned int, void *, unsigned int);
void outport_b(unsigned int, unsigned char);
void outport_w(unsigned int, unsigned short int);
void outport_l(unsigned int, unsigned int);
void outport_sw(unsigned int, void *, unsigned int);
void outport_sl(unsigned int, void *, unsigned int);

void load_gdt(unsigned int);
void load_idt(unsigned int);
void activate_kpage_dir(void);
void load_tr(unsigned int);
unsigned long long int get_rdtsc(void);
void invalidate_tlb(void);

#define CLI() __asm__ __volatile__ ("cli":::"memory")
#define STI() __asm__ __volatile__ ("sti":::"memory")
#define NOP() __asm__ __volatile__ ("nop":::"memory")
#define HLT() __asm__ __volatile__ ("hlt":::"memory")

#define GET_CR2(cr2) __asm__ __volatile__ ("movl %%cr2, %0" : "=r" (cr2));
#define GET_ESP(esp) __asm__ __volatile__ ("movl %%esp, %0" : "=r" (esp));
#define SET_ESP(esp) __asm__ __volatile__ ("movl %0, %%esp" :: "r" (esp));

#define SAVE_FLAGS(flags)			\
	__asm__ __volatile__(			\
		"pushfl ; popl %0\n\t"		\
		: "=r" (flags)			\
		: /* no input */		\
		: "memory"			\
	);

#define RESTORE_FLAGS(x)			\
	__asm__ __volatile__(			\
		"pushl %0 ; popfl\n\t"		\
		: /* no output */		\
		: "r" (flags)			\
		: "memory"			\
	);

#ifdef __TINYC__
#define USER_SYSCALL(num, arg1, arg2, arg3)    \
	__asm__ __volatile__(                   \
		"movl   %0, %%eax\n\t"          \
		"movl   %1, %%ecx\n\t"          \
		"movl   %2, %%edx\n\t"          \
		"movl   %3, %%ebx\n\t"          \
		"int    $0x80\n\t"              \
		: /* no output */               \
		: "r"((unsigned int)num), "r"((unsigned int)arg2), "r"((unsigned int)arg3), "r"((unsigned int)arg1)     \
	);
#else
#define USER_SYSCALL(num, arg1, arg2, arg3)	\
	__asm__ __volatile__(			\
		"movl   %0, %%eax\n\t"		\
		"movl   %1, %%ebx\n\t"		\
		"movl   %2, %%ecx\n\t"		\
		"movl   %3, %%edx\n\t"		\
		"int    $0x80\n\t"		\
		: /* no output */		\
		: "eax"((unsigned int)num), "ebx"((unsigned int)arg1), "ecx"((unsigned int)arg2), "edx"((unsigned int)arg3)	\
	);
#endif

/*
static inline unsigned long long int get_rdtsc(void)
{
	unsigned int eax, edx;

	__asm__ __volatile__("rdtsc" : "=a" (eax), "=d" (edx));
	return ((unsigned long long int)eax) | (((unsigned long long int)edx) << 32);
}
*/

#endif /* _FIWIX_ASM_H */
