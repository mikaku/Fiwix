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
void riscv64_generic_traps_install(void);
void riscv64_generic_runtime_ready(void);
int riscv64_generic_user_trap(struct riscv64_trap_frame *, unsigned long);
int riscv64_generic_kernel_trap(unsigned long, unsigned long, unsigned long);
void riscv64_generic_trap_fatal(unsigned long, unsigned long, unsigned long);

#elif defined(CONFIG_ARCH_ARM)

struct proc;

struct arch_context {
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int sp;
	unsigned int lr;
	unsigned int ttbr0;
	unsigned int kernel_sp;
};

void arm_process_roots_init(void);
unsigned int *arm_process_root(const struct proc *);
int arm_process_address_space_create(struct proc *, const struct proc *);
int arm_process_address_space_release(struct proc *);
int arm_process_context_activate(const struct proc *);
void arm_context_switch(struct arch_context *, struct arch_context *);

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
