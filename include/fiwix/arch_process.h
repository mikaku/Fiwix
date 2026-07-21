/*
 * fiwix/include/fiwix/arch_process.h
 *
 * Architecture-owned process context. The structure remains the first member
 * of struct proc so low-level switch code can use stable field addresses.
 */

#ifndef _FIWIX_ARCH_PROCESS_H
#define _FIWIX_ARCH_PROCESS_H

#ifdef CONFIG_ARCH_RISCV64

struct proc;
struct riscv64_trap_frame;

struct arch_context {
	unsigned long ra;
	unsigned long sp;
	unsigned long s0;
	unsigned long s1;
	unsigned long s2;
	unsigned long s3;
	unsigned long s4;
	unsigned long s5;
	unsigned long s6;
	unsigned long s7;
	unsigned long s8;
	unsigned long s9;
	unsigned long s10;
	unsigned long s11;
	unsigned long satp;
	unsigned long kernel_sp;
};

int riscv64_process_setup(struct proc *, int (*)(void));
int riscv64_user_process_setup(struct proc *, unsigned long, unsigned long);
int riscv64_fork_process_setup(struct proc *, struct riscv64_trap_frame *);
void riscv64_process_release(struct proc *);
void riscv64_context_activate(struct arch_context *);
unsigned long riscv64_read_satp(void);
unsigned long riscv64_make_satp(unsigned long);
void riscv64_kernel_process_entry(void);
void riscv64_user_process_entry(void);
void riscv64_return_to_user(void);
int riscv64_user_syscall(struct riscv64_trap_frame *, unsigned long);

#else

/* Intel 386 Task Switch State. */
struct arch_context {
	unsigned int prev_tss;
	unsigned int esp0;
	unsigned int ss0;
	unsigned int esp1;
	unsigned int ss1;
	unsigned int esp2;
	unsigned int ss2;
	unsigned int cr3;
	unsigned int eip;
	unsigned int eflags;
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;
	unsigned int es;
	unsigned int cs;
	unsigned int ss;
	unsigned int ds;
	unsigned int fs;
	unsigned int gs;
	unsigned int ldt;
	unsigned short int debug_trap;
	unsigned short int io_bitmap_addr;
	unsigned char io_bitmap[IO_BITMAP_SIZE + 1];
};

#endif

#endif /* _FIWIX_ARCH_PROCESS_H */
