/*
 * fiwix/kernel/core386.S
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#define ASM_FILE	1

#include <fiwix/config.h>
#include <fiwix/segments.h>
#include <fiwix/unistd.h>

#define CR0_MP	~(0x00000002)	/* CR0 bit-01 MP (Monitor Coprocessor) */
#define CR0_EM	0x00000004	/* CR0 bit-02 EM (Emulation) */

#define SS_RPL3		0x03	/* Request Privilege Level 3 */

#define GS		0x00
#define FS		0x04
#define ES		0x08
#define DS		0x0C
#define EDI		0x10	/* \				*/
#define ESI		0x14	/* |				*/
#define EBP		0x18	/* |				*/
#define ESP		0x1C	/* | saved by			*/
#define EBX		0x20	/* | 'pusha'			*/
#define EDX		0x24	/* |				*/
#define ECX		0x28	/* |				*/
#define EAX		0x2C	/* /				*/
#define ERR		0x30	/*   error code (or padding)	*/
#define EIP		0x34	/* \				*/
#define CS		0x38	/* | saved by processor		*/
#define FLAGS		0x3C	/* /				*/
#define OLDESP		0x40	/* \ saved by processor on	*/
#define OLDSS		0x44	/* / privilege level change	*/

#define SAVE_ALL							\
	pusha								;\
	pushl	%ds							;\
	pushl	%es							;\
	pushl	%fs							;\
	pushl	%gs

#define EXCEPTION(exception)						\
	pushl	$exception						;\
	call	trap_handler						;\
	addl	$4, %esp

#define IRQ(irq)							\
	pushl	$irq							;\
	call	irq_handler						;\
	addl	$4, %esp

#define BOTTOM_HALVES							\
	sti								;\
	call	do_bh

#define CHECK_IF_NESTED_INTERRUPT					\
	testl	$SS_RPL3, CS(%esp)					;\
	jz	2f

#define CHECK_IF_SIGNALS						\
	call	issig							;\
	testl	$0xFFFFFFFF, %eax					;\
	jz	1f							;\
	movl	%esp, %eax						;\
	pushl	%eax							;\
	call	psig							;\
	addl	$4, %esp						;\
1:

#define CHECK_IF_NEED_SCHEDULE						\
	movl	need_resched, %eax					;\
	testl	$0xFFFFFFFF, %eax					;\
	jz	2f							;\
	call	do_sched						;\
2:

#define RESTORE_ALL							\
	popl	%gs							;\
	popl	%fs							;\
	popl	%es							;\
	popl	%ds							;\
	popa								;\
	addl	$4, %esp	/* suppress error code from stack */


.text

#define BUILD_EXCEPTION_SIMUL_ERR(num, name)				\
.align 4								;\
.globl name; name:							;\
	pushl	$0		/* save simulated error code to stack */;\
	SAVE_ALL							;\
	EXCEPTION(num)							;\
	BOTTOM_HALVES							;\
	CHECK_IF_NESTED_INTERRUPT					;\
	CHECK_IF_SIGNALS						;\
	CHECK_IF_NEED_SCHEDULE						;\
	RESTORE_ALL							;\
	iret

#define BUILD_EXCEPTION(num, name)					\
.align 4								;\
.globl name; name:							;\
	SAVE_ALL							;\
	EXCEPTION(num)							;\
	BOTTOM_HALVES							;\
	CHECK_IF_NESTED_INTERRUPT					;\
	CHECK_IF_SIGNALS						;\
	CHECK_IF_NEED_SCHEDULE						;\
	RESTORE_ALL							;\
	iret

BUILD_EXCEPTION_SIMUL_ERR(0, except0)	/* DIVIDE ERROR */
BUILD_EXCEPTION_SIMUL_ERR(1, except1)	/* DEBUG */
BUILD_EXCEPTION_SIMUL_ERR(2, except2)	/* NMI INTERRUPT */
BUILD_EXCEPTION_SIMUL_ERR(3, except3)	/* BREAKPOINT INT3 */
BUILD_EXCEPTION_SIMUL_ERR(4, except4)	/* OVERFLOW */
BUILD_EXCEPTION_SIMUL_ERR(5, except5)	/* BOUND */
BUILD_EXCEPTION_SIMUL_ERR(6, except6)	/* INVALID OPCODE */

.align 4
.globl except7; except7:	# NO MATH COPROCESSOR
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x7)
	clts			# floating-opcode cached!
	BOTTOM_HALVES
	CHECK_IF_NESTED_INTERRUPT
	CHECK_IF_SIGNALS
	CHECK_IF_NEED_SCHEDULE
	RESTORE_ALL
	iret

BUILD_EXCEPTION(8, except8)		/* DOUBLE FAULT */
BUILD_EXCEPTION_SIMUL_ERR(9, except9)	/* COPROCESSOR SEGMENT OVERRUN */
BUILD_EXCEPTION(10, except10)		/* INVALID TSS */
BUILD_EXCEPTION(11, except11)		/* SEGMENT NOT PRESENT */
BUILD_EXCEPTION(12, except12)		/* STACK SEGMENT FAULT */
BUILD_EXCEPTION(13, except13)		/* GENERAL PROTECTION FAULT */
BUILD_EXCEPTION(14, except14)		/* PAGE FAULT */
BUILD_EXCEPTION_SIMUL_ERR(15, except15)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(16, except16)	/* FLOATING POINT ERROR */
BUILD_EXCEPTION(17, except17)		/* ALIGNMENT CHECK */
BUILD_EXCEPTION_SIMUL_ERR(18, except18)	/* MACHINE CHECK */
BUILD_EXCEPTION_SIMUL_ERR(19, except19)	/* SIMD FLOATING POINT */
BUILD_EXCEPTION_SIMUL_ERR(20, except20)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(21, except21)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(22, except22)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(23, except23)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(24, except24)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(25, except25)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(26, except26)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(27, except27)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(28, except28)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(29, except29)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(30, except30)	/* INTEL RESERVED */
BUILD_EXCEPTION_SIMUL_ERR(31, except31)	/* INTEL RESERVED */


#define BUILD_IRQ(num, name)						\
.align 4								;\
.globl name; name:							;\
	pushl	$0		/* save simulated error code to stack */;\
	SAVE_ALL							;\
	IRQ(num)							;\
	BOTTOM_HALVES							;\
	CHECK_IF_NESTED_INTERRUPT					;\
	CHECK_IF_SIGNALS						;\
	CHECK_IF_NEED_SCHEDULE						;\
	RESTORE_ALL							;\
	iret

BUILD_IRQ(0, irq0)
BUILD_IRQ(1, irq1)
BUILD_IRQ(2, irq2)
BUILD_IRQ(3, irq3)
BUILD_IRQ(4, irq4)
BUILD_IRQ(5, irq5)
BUILD_IRQ(6, irq6)
BUILD_IRQ(7, irq7)
BUILD_IRQ(8, irq8)
BUILD_IRQ(9, irq9)
BUILD_IRQ(10, irq10)
BUILD_IRQ(11, irq11)
BUILD_IRQ(12, irq12)
BUILD_IRQ(13, irq13)
BUILD_IRQ(14, irq14)
BUILD_IRQ(15, irq15)

.align 4
.globl unknown_irq; unknown_irq:
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	call	unknown_irq_handler
	RESTORE_ALL
	iret

.align 4
.globl switch_to_user_mode; switch_to_user_mode:
	cli
	xorl	%eax, %eax		# initialize %eax
	movl	%eax, %ebx		# initialize %ebx
	movl	%eax, %ecx		# initialize %ecx
	movl	%eax, %edx		# initialize %edx
	movl	%eax, %esi		# initialize %esi
	movl	%eax, %edi		# initialize %edi
	movl	%eax, %ebp		# initialize %ebp
	movl	$(USER_DS | SS_RPL3), %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	pushl	%eax
	pushl	$PAGE_OFFSET - 4	# user stack address
	pushl	$0x202			# initialize eflags (Linux 2.2 = 0x292)
	popfl
	pushfl
	movl	$(USER_CS | SS_RPL3), %eax
	pushl	%eax
	pushl	$PAGE_OFFSET - 0x1000	# go to init_trampoline() in user mode
	iret

.align 4
.globl sighandler_trampoline; sighandler_trampoline:
	/*
	 * This pushes twice the %eax register in order to save the 'signum'
	 * value from being clobbered by functions like printf(), during the
	 * signal handler calling.
	 * FIXME: is this a bug in Newlib or in my side?
	 */
	pushl	%eax			# save 'signum'
	pushl	%eax			# 'signum' is the argument of the handler
	call	*%ecx
	addl	$4, %esp		# discard the first push
	popl	%ebx			# restore 'signum'

	movl	$SYS_sigreturn, %eax
	int	$0x80

	# never reached, otherwise call sys_exit()
	movl	$SYS_exit, %eax
	int	$0x80
	ret
.align 4
.globl end_sighandler_trampoline; end_sighandler_trampoline:
	nop

.align 4
.globl syscall; syscall:		# SYSTEM CALL ENTRY
	pushl	%eax			# save the system call number
	SAVE_ALL

#ifdef CONFIG_SYSCALL_6TH_ARG
	pushl	%ebp			# + 6th argument
#endif /* CONFIG_SYSCALL_6TH_ARG */
	pushl	%edi			# \ 5th argument
	pushl	%esi			# | 4th argument
	pushl	%edx			# | 3rd argument
	pushl	%ecx			# | 2nd argument
	pushl	%ebx			# / 1st argument
	pushl	%eax			# system call number
	call	do_syscall
#ifdef CONFIG_SYSCALL_6TH_ARG
	addl	$28, %esp		# suppress all 7 pushl from the stack
#else
	addl	$24, %esp		# suppress all 6 pushl from the stack
#endif /* CONFIG_SYSCALL_6TH_ARG */
	movl	%eax, EAX(%esp)		# save the return value

	BOTTOM_HALVES
	CHECK_IF_SIGNALS
	CHECK_IF_NEED_SCHEDULE
.globl return_from_syscall; return_from_syscall:
	RESTORE_ALL
	iret

.align 4
.globl do_switch; do_switch:
	pusha
	pushfl
	movl	%esp, %ebx
	addl	$0x24, %ebx
	movl	0x4(%ebx), %eax		# save ESP to 'prev->tss.esp'
	movl	%esp, (%eax)
	movl	0x8(%ebx), %eax		# save EIP to 'prev->tss.eip'
	movl	$1f, (%eax)
	movl	0xC(%ebx), %esp		# load 'next->tss.esp' into ESP
	pushl	0x10(%ebx)		# push 'next->tss.eip' into ESP
	movl	0x14(%ebx), %eax	# load 'next->tss.cr3' into CR3
	ltr	0x18(%ebx)		# load TSS
	movl	%eax, %cr3
	ret
1:
	popfl
	popa

.align 4
.globl cpuid; cpuid:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	pushf
	pop	%eax			# put original EFLAGS in EAX
	mov	%eax, %ecx		# save original EFLAGS in ECX
	xor	$0x200000, %eax		# change bit 21 (ID) in EFLAGS
	push	%eax			# save new EFLAGS on stack
	popf				# replace current EFLAGS
	pushf
	pop	%eax			# put EFLAGS in EAX
	cmp	%ecx, %eax		# compare if both EFLAGS are equal

	je	test386			# can't toggle ID bit, no CPUID
	xor	%ebx, %ebx		# CPUID available, will return 0
	jmp	end_cpuid 

test386:
	mov	%ecx, %eax		# get original EFLAGS
	xor	$0x40000, %eax		# change bit 18 (AC) in EFLAGS
	push	%eax			# save new EFLAGS on stack
	popf				# replace current EFLAGS
	pushf
	pop	%eax
	cmp	%ecx, %eax		# compare if both EFLAGS are equal
	movb	$3, %bl			# looks like an i386, return 3
	je	end_cpuid
	movb	$4, %bl			# otherwise is an old i486, return 4

end_cpuid:
	push	%ecx			# push original EFLAGS
	popf				# restore original EFLAGS
	xor	%eax, %eax
	movb	%bl, %al		# put return value to AL

	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret

.align 4
.globl getfpu; getfpu:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	fninit
	movl	$0x5a5a, _fpstatus
	fnstsw	_fpstatus
	movl	_fpstatus, %eax
	cmp	$0, %al
	movl	$0, _fpstatus
	jne	end_getfpu

check_control_word:
	fnstcw	_fpstatus
	movl	_fpstatus, %eax
	andl	$0x103f, %eax
	cmp	$0x3f, %ax
	movl	$0, _fpstatus
	jne	end_getfpu
	movl	$1, _fpstatus

end_getfpu:
	movl	_fpstatus, %eax
	cmp	$0, %al
	jne	1f			# return if there is a coprocessor
	movl	%cr0, %eax		# otherwise (no math processor):
	orl	$CR0_EM, %eax		# - set   EM (Emulation)
	andl	$CR0_MP, %eax		# - clear MP (Monitor Coprocessor)
	movl	%eax, %cr0
	movl	$0, %eax
1:
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret

.align 4
.globl get_cpu_vendor_id; get_cpu_vendor_id:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edx
	pushl	%ecx

	mov	$0, %eax
	cpuid
	movl	%ebx, _vendorid		# save the 12 bytes of vendor ID string
	movl	%edx, _vendorid+4
	movl	%ecx, _vendorid+8

	popl	%ecx
	popl	%edx
	popl	%ebx
	popl	%ebp
	ret				# EAX returns the highest CPUID value

.align 4
.globl signature_flags; signature_flags:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%edx

	mov	$1, %eax
	cpuid
	movl	%eax, _cpusignature	# signature (model and stepping)
	movl	%ebx, _brandid		# misc. information
	movl	%edx, _cpuflags		# feature flags
	shrl	$8, %eax
	andl	$0xF, %eax
	movl	%eax, _cputype		# family

	popl	%edx
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret

.align 4
.globl brand_str; brand_str:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	$0x80000000, %eax
	cpuid
	cmp	$0x80000000, %eax	# check if brand string is supported
	jbe	no_brand_str
	movl	$0x80000002, %eax	# get first 16 bytes of brand string
	cpuid
	movl	%eax, _brandstr
	movl	%ebx, _brandstr+4
	movl	%ecx, _brandstr+8
	movl	%edx, _brandstr+12
	movl	$0x80000003, %eax	# get more 16 bytes of brand string
	cpuid
	movl	%eax, _brandstr+16
	movl	%ebx, _brandstr+20
	movl	%ecx, _brandstr+24
	movl	%edx, _brandstr+28
	movl	$0x80000004, %eax	# get last 16 bytes of brand string
	cpuid
	movl	%eax, _brandstr+32
	movl	%ebx, _brandstr+36
	movl	%ecx, _brandstr+40
	movl	%edx, _brandstr+44
	jmp	end_brand_str

no_brand_str:
	movl	$1, %eax

end_brand_str:
	movl	$0, %eax
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret

.align 4
.globl tlbinfo; tlbinfo:
	pushl	%edx
	pushl	%ecx
	pushl	%ebx
	mov	$2, %eax
	cpuid
	movl	%eax, _tlbinfo_eax	# store cache information
	movl	%ebx, _tlbinfo_ebx
	movl	%edx, _tlbinfo_ecx
	movl	%ecx, _tlbinfo_edx
	popl	%ebx
	popl	%ecx
	popl	%edx
	ret

.align 4
.globl inport_b; inport_b:
	pushl	%ebp
	movl	%esp, %ebp

	movw	0x08(%ebp), %dx		# port addr
	xorl	%eax, %eax		# initialize %eax
	inb	%dx, %al

	jmp	1f			# recovery time
1:	jmp	1f			# recovery time
1:	popl	%ebp
	ret

.align 4
.globl inport_w; inport_w:
	pushl	%ebp
	movl	%esp, %ebp

	movw	0x08(%ebp), %dx		# port addr
	inw	%dx, %ax

	jmp	1f			# recovery time
1:	jmp	1f			# recovery time
1:	popl	%ebp
	ret

.align 4
.globl inport_l; inport_l:
	pushl	%ebp
	movl	%esp, %ebp

	movl	0x08(%ebp), %edx	# port addr
	inl	%dx, %eax

	jmp	1f			# recovery time
1:	jmp	1f			# recovery time
1:	popl	%ebp
	ret

.align 4
.globl inport_sw; inport_sw:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%edi
	pushl	%ecx

	cld
	mov	0x8(%ebp), %edx		# port addr
	mov	0xC(%ebp), %edi		# dest
	mov	0x10(%ebp), %ecx	# count
	rep	insw

	popl	%ecx
	popl	%edi
	popl	%edx
	popl	%ebp
	ret

.align 4
.globl inport_sl; inport_sl:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%edi
	pushl	%ecx

	cld
	movl	0x8(%ebp), %edx		# port addr
	movl	0xC(%ebp), %edi		# dest
	movl	0x10(%ebp), %ecx	# count
	rep	insl

	popl	%ecx
	popl	%edi
	popl	%edx
	popl	%ebp
	ret

.align 4
.globl outport_b; outport_b:
	pushl	%ebp
	movl	%esp, %ebp

	movw	0x8(%ebp), %dx		# port addr
	movb	0xC(%ebp), %al		# data
	outb	%al, %dx

	jmp	1f			# recovery time
1:	jmp	1f			# recovery time
1:	popl	%ebp
	ret

.align 4
.globl outport_w; outport_w:
	pushl	%ebp
	movl	%esp, %ebp

	movw	0x8(%ebp), %dx		# port addr
	movw	0xC(%ebp), %ax		# data
	outw	%ax, %dx

	jmp	1f			# recovery time
1:	jmp	1f			# recovery time
1:	popl	%ebp
	ret

.align 4
.globl outport_l; outport_l:
	pushl	%ebp
	movl	%esp, %ebp

	movl	0x8(%ebp), %edx		# port addr
	movl	0xC(%ebp), %eax		# data
	outl	%eax, %dx

	jmp	1f			# recovery time
1:	jmp	1f			# recovery time
1:	popl	%ebp
	ret

.align 4
.globl outport_sw; outport_sw:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%esi
	pushl	%ecx

	cld
	mov	0x8(%ebp), %edx		# port addr
	mov	0xC(%ebp), %esi		# src
	mov	0x10(%ebp), %ecx	# count
	rep	outsw

	popl	%ecx
	popl	%esi
	popl	%edx
	popl	%ebp
	ret

.align 4
.globl outport_sl; outport_sl:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%esi
	pushl	%ecx

	cld
	movl	0x8(%ebp), %edx		# port addr
	movl	0xC(%ebp), %esi		# src
	movl	0x10(%ebp), %ecx	# count
	rep	outsl

	popl	%ecx
	popl	%esi
	popl	%edx
	popl	%ebp
	ret

.align 4
.globl load_gdt; load_gdt:
	movl	0x4(%esp), %eax
	lgdt	(%eax)
	movw	$KERNEL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	ljmp	$KERNEL_CS, $1f
1:
	ret

.align 4
.globl load_idt; load_idt:
	movl	0x4(%esp), %eax
	lidt	(%eax)
	ret

.align 4
.globl activate_kpage_dir; activate_kpage_dir:
	movl	kpage_dir, %eax
	movl	%eax, %cr3
	ret

.align 4
.globl load_tr; load_tr:
	mov	0x4(%esp), %ax
	ltr	%ax
	ret

.align 4
.globl get_rdtsc; get_rdtsc:
	cpuid
	rdtsc
	ret

.align 4
.globl invalidate_tlb; invalidate_tlb:
	movl	%cr3, %eax
	movl	%eax, %cr3
	ret


.data

.globl	_cputype
.globl	_cpusignature
.globl	_cpuflags
.globl	_fpstatus
.globl	_brandid
.globl	_vendorid
.globl	_brandstr
.globl	_tlbinfo_eax
.globl	_tlbinfo_ebx
.globl	_tlbinfo_ecx
.globl	_tlbinfo_edx

_cputype:	.int	0
_cpusignature:	.int	0
_cpuflags:	.int	0
_fpstatus:	.int	0
_brandid:	.int	0
_vendorid:	.fill	13,1,0
_brandstr:	.fill	49,1,0
_tlbinfo_eax:	.int	0
_tlbinfo_ebx:	.int	0
_tlbinfo_ecx:	.int	0
_tlbinfo_edx:	.int	0
