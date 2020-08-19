/*
 * fiwix/kernel/core386.S
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/const.h>
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
#define ERR		0x30	/*   error code or padding	*/
#define EIP		0x34	/* \				*/
#define CS		0x38	/* | saved by processor		*/
#define FLAGS		0x3C	/* /				*/
#define OLDESP		0x40	/* \ saved by processor on	*/
#define OLDSS		0x44	/* / privilege level change	*/

#define SAVE_ALL							\
	pushal								;\
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
	cmpl	$0, intr_count						;\
	jne	2f							;\
	incl	intr_count						;\
	call	do_bh							;\
	decl	intr_count

#define CHECK_SIGNALS							\
	call	issig							;\
	cmpl	$0, %eax						;\
	je	1f							;\
	movl	%esp, %eax						;\
	pushl	%eax							;\
	call	psig							;\
	addl	$4, %esp						;\
1:

#define SCHEDULE							\
	cmpl	$0, need_resched					;\
	je	2f							;\
	call	do_sched						;\
2:

#define RESTORE_ALL							\
	popl	%gs							;\
	popl	%fs							;\
	popl	%es							;\
	popl	%ds							;\
	popal								;\
	addl	$4, %esp	/* suppress error code from stack */


.text

.align 4
.globl except0;	except0:	# DIVIDE ERROR
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x0)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except1;	except1:	# DEBUG
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x1)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except2;	except2:	# NMI INTERRUPT
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x2)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except3; except3:	# BREAKPOINT INT3
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x3)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except4; except4:	# OVERFLOW
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x4)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except5; except5:	# BOUND
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x5)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except6; except6:	# INVALID OPCODE
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x6)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except7; except7:	# NO MATH COPROCESSOR
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x7)
	clts			# floating-opcode cached!
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except8; except8:	# DOUBLE FAULT
	SAVE_ALL
	EXCEPTION(0x8)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl except9; except9:	# COPROCESSOR SEGMENT OVERRUN
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x9)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl exceptA; exceptA:	# INVALID TSS
	SAVE_ALL
	EXCEPTION(0xA)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl exceptB; exceptB:	# SEGMENT NOT PRESENT
	SAVE_ALL
	EXCEPTION(0xB)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl exceptC; exceptC:	# STACK SEGMENT FAULT
	SAVE_ALL
	EXCEPTION(0xC)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl exceptD; exceptD:	# GENERAL PROTECTION FAULT
	SAVE_ALL
	EXCEPTION(0xD)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl exceptE; exceptE:	# PAGE FAULT
	SAVE_ALL
	EXCEPTION(0xE)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.align 4
.globl exceptF; exceptF:	# INTEL RESERVED
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0xF)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.globl except10; except10:	# FLOATING POINT ERROR
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x10)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.globl except11; except11:	# ALIGNMENT CHECK
	EXCEPTION(0x11)
	SAVE_ALL
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.globl except12; except12:	# MACHINE CHECK
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x12)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.globl except13; except13:	# SIMD FLOATING POINT
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x13)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret

.globl except14; except14:	# INTEL RESERVED
.globl except15; except15:	# INTEL RESERVED
.globl except16; except16:	# INTEL RESERVED
.globl except17; except17:	# INTEL RESERVED
.globl except18; except18:	# INTEL RESERVED
.globl except19; except19:	# INTEL RESERVED
.globl except1A; except1A:	# INTEL RESERVED
.globl except1B; except1B:	# INTEL RESERVED
.globl except1C; except1C:	# INTEL RESERVED
.globl except1D; except1D:	# INTEL RESERVED
.globl except1E; except1E:	# INTEL RESERVED
.globl except1F; except1F:	# INTEL RESERVED
	pushl	$0		# save simulated error code to stack
	SAVE_ALL
	EXCEPTION(0x14)
	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
	RESTORE_ALL
	iret


#define BUILD_IRQ(num, name)						\
.align 4								;\
.globl name; name:							;\
	pushl	$0		/* save simulated error code to stack */;\
	SAVE_ALL							;\
	IRQ(num)							;\
	BOTTOM_HALVES							;\
	CHECK_SIGNALS							;\
	SCHEDULE							;\
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
	IRQ(-1)
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
	pushl	$KERNEL_BASE_ADDR - 4	# user stack address
	pushl	$0x202			# initialize eflags (Linux 2.2 = 0x292)
	popfl
	pushfl
	movl	$(USER_CS | SS_RPL3), %eax
	pushl	%eax
	pushl	$KERNEL_BASE_ADDR - 0x1000	# go to init_trampoline() in user mode
	iret

.align 4
.globl sighandler_trampoline; sighandler_trampoline:
	pushl	%eax
	call	*%ecx
	popl	%ebx

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

	pushl	%edi			# \ 5th parameter
	pushl	%esi			# | 4th parameter
	pushl	%edx			# | 3rd parameter
	pushl	%ecx			# | 2nd parameter
	pushl	%ebx			# / 1st parameter
	pushl	%eax			# system call number
	call	do_syscall
	addl	$24, %esp		# suppress all 6 pushl from the stack
	movl	%eax, EAX(%esp)		# save the return value

	BOTTOM_HALVES
	CHECK_SIGNALS
	SCHEDULE
.align 4
.globl return_from_syscall; return_from_syscall:
	RESTORE_ALL
	iret

.align 4
.globl do_switch; do_switch:
	movl	%esp, %ebx
	pushal
	pushfl
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
	popal

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
.globl vendor_id; vendor_id:
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
	mov	$2, %eax
	cpuid
	movl	%eax, _tlbinfo_eax	# store cache information
	movl	%ebx, _tlbinfo_ebx
	movl	%edx, _tlbinfo_ecx
	movl	%ecx, _tlbinfo_edx
	popl	%ecx
	popl	%edx
	ret

.align 4
.globl inport_b; inport_b:
	pushl	%ebp
	movl	%esp, %ebp

	movw	0x08(%ebp), %dx		# port addr
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
	rep
	insw

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
	rep
	outsw

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
