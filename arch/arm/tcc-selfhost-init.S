/*
 * fiwix/arch/arm/tcc-selfhost-init.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm
    .text
    .globl _start

    .equ SYS_EXIT,     1
    .equ SYS_FORK,     2
    .equ SYS_WRITE,    4
    .equ SYS_EXECVE,  11
    .equ SYS_CHDIR,   12
    .equ SYS_CHMOD,   15
    .equ SYS_SYNC,    36
    .equ SYS_REBOOT,  88
    .equ SYS_WAIT4,  114

    .equ REBOOT_MAGIC1, 0xfee1dead
    .equ REBOOT_MAGIC2, 0x28121969
    .equ REBOOT_COMMAND, 0x01234567

_start:
    mov     r0, #1
    ldr     r1, =start_message
    ldr     r2, =start_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    bl      enter_source_directory

    ldr     r0, =tcc_path
    ldr     r1, =boot0_name
    bl      spawn_build
    ldr     r0, =boot0_path
    bl      make_executable
    ldr     r0, =boot0_path
    ldr     r1, =boot0_version_argv
    bl      spawn_and_wait

    ldr     r0, =boot0_path
    ldr     r1, =boot1_name
    bl      spawn_build
    ldr     r0, =boot1_path
    bl      make_executable
    ldr     r0, =boot1_path
    ldr     r1, =boot1_version_argv
    bl      spawn_and_wait

    ldr     r0, =boot1_path
    ldr     r1, =boot2_name
    bl      spawn_build
    ldr     r0, =boot2_path
    bl      make_executable
    ldr     r0, =boot2_path
    ldr     r1, =boot2_version_argv
    bl      spawn_and_wait

    ldr     r0, =boot2_path
    ldr     r1, =boot3_name
    bl      spawn_build
    ldr     r0, =boot3_path
    bl      make_executable
    ldr     r0, =boot3_path
    ldr     r1, =boot3_version_argv
    bl      spawn_and_wait

    mov     r0, #1
    ldr     r1, =complete_message
    ldr     r2, =complete_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r7, #SYS_SYNC
    svc     #0
    ldr     r0, =REBOOT_MAGIC1
    ldr     r1, =REBOOT_MAGIC2
    ldr     r2, =REBOOT_COMMAND
    mov     r7, #SYS_REBOOT
    svc     #0
    b       failed

spawn_build:
    push    {r4-r10, lr}
    sub     sp, sp, #256
    mov     r4, r0
    mov     r5, r1
    ldr     r6, =build_argv_template
    ldr     r7, =build_argv_template_end
    mov     r8, sp
1:
    cmp     r6, r7
    beq     2f
    ldr     r9, [r6], #4
    str     r9, [r8], #4
    b       1b
2:
    str     r4, [sp]
    str     r5, [sp, #20]
    mov     r0, r4
    mov     r1, sp
    bl      spawn_and_wait
    add     sp, sp, #256
    pop     {r4-r10, pc}

spawn_and_wait:
    push    {r4-r6, lr}
    mov     r4, r0
    mov     r5, r1
    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     1f
    blt     failed
    mov     r4, r0
    bl      wait_for_zero
    pop     {r4-r6, pc}
1:
    mov     r0, r4
    mov     r1, r5
    ldr     r2, =process_envp
    mov     r7, #SYS_EXECVE
    svc     #0
    b       exec_failed

wait_for_zero:
    sub     sp, sp, #8
    mov     r0, r4
    mov     r1, sp
    mov     r2, #0
    mov     r3, #0
    mov     r7, #SYS_WAIT4
    svc     #0
    cmp     r0, r4
    bne     failed
    ldr     r0, [sp]
    add     sp, sp, #8
    cmp     r0, #0
    beq     1f
    b       child_failed_status
1:
    bx      lr

make_executable:
    movw    r1, #493
    mov     r7, #SYS_CHMOD
    svc     #0
    cmp     r0, #0
    blt     failed
    bx      lr

enter_source_directory:
    ldr     r0, =source_directory
    mov     r7, #SYS_CHDIR
    svc     #0
    cmp     r0, #0
    blt     failed
    bx      lr

exec_failed:
    mov     r0, #2
    ldr     r1, =exec_failure_message
    ldr     r2, =exec_failure_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0
    b       failed

child_failed_status:
    push    {r4-r6, lr}
    sub     sp, sp, #16
    mov     r4, r0
    mov     r5, #8
    mov     r6, sp
1:
    lsr     r0, r4, #28
    cmp     r0, #10
    addlt   r0, r0, #'0'
    addge   r0, r0, #('a' - 10)
    strb    r0, [r6], #1
    lsl     r4, r4, #4
    subs    r5, r5, #1
    bne     1b
    mov     r0, #'\n'
    strb    r0, [r6]

    mov     r0, #2
    ldr     r1, =wait_status_message
    ldr     r2, =wait_status_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0
    mov     r0, #2
    mov     r1, sp
    mov     r2, #9
    mov     r7, #SYS_WRITE
    svc     #0
    add     sp, sp, #16
    pop     {r4-r6, lr}
    b       failed

failed:
    mov     r0, #2
    ldr     r1, =failure_message
    ldr     r2, =failure_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0
    mov     r0, #1
    mov     r7, #SYS_EXIT
    svc     #0
1:
    b       1b

    .balign 4
build_argv_template:
    .word   0
    .word   no_stdlib_option
    .word   static_option
    .word   text_address_option
    .word   output_option
    .word   0
    .word   define_option
    .word   bootstrap_define
    .word   define_option
    .word   float_define
    .word   define_option
    .word   bitfield_define
    .word   define_option
    .word   long_long_define
    .word   define_option
    .word   setjmp_define
    .word   define_option
    .word   arm_target_define
    .word   define_option
    .word   arm_eabi_define
    .word   define_option
    .word   arm_vfp_define
    .word   include_option
    .word   current_include
    .word   include_option
    .word   mes_include
    .word   define_option
    .word   tccdir_define
    .word   define_option
    .word   crtprefix_define
    .word   define_option
    .word   interpreter_define
    .word   define_option
    .word   library_paths_define
    .word   define_option
    .word   system_include_define
    .word   define_option
    .word   libgcc_define
    .word   define_option
    .word   libtcc1_define
    .word   define_option
    .word   tccboot_define
    .word   define_option
    .word   static_define
    .word   define_option
    .word   use_libgcc_define
    .word   define_option
    .word   version_define
    .word   define_option
    .word   one_source_define
    .word   crt1_path
    .word   crti_path
    .word   source_path
    .word   libc_path
    .word   libtcc1_path
    .word   libgetopt_path
    .word   crtn_path
    .word   0
build_argv_template_end:
    .if (build_argv_template_end - build_argv_template) > 256
    .error "TinyCC build argv exceeds its stack allocation"
    .endif
boot0_version_argv:
    .word   boot0_path
    .word   version_option
    .word   0
boot1_version_argv:
    .word   boot1_path
    .word   version_option
    .word   0
boot2_version_argv:
    .word   boot2_path
    .word   version_option
    .word   0
boot3_version_argv:
    .word   boot3_path
    .word   version_option
    .word   0
process_envp:
    .word   home_environment
    .word   path_environment
    .word   0

start_message:
    .ascii  "Fiwix ARM TinyCC self-host process tree entered\n"
start_message_end:
complete_message:
    .ascii  "Fiwix ARM TinyCC self-host process tree completed\n"
complete_message_end:
exec_failure_message:
    .ascii  "Fiwix ARM TinyCC self-host exec returned\n"
exec_failure_message_end:
wait_status_message:
    .ascii  "Fiwix ARM TinyCC self-host child wait status 0x"
wait_status_message_end:
failure_message:
    .ascii  "Fiwix ARM TinyCC self-host process tree failed\n"
failure_message_end:
tcc_path:
    .asciz  "/bin/tcc-mes"
boot0_path:
    .asciz  "/tcc/tcc-boot0"
boot1_path:
    .asciz  "/tcc/tcc-boot1"
boot2_path:
    .asciz  "/tcc/tcc-boot2"
boot3_path:
    .asciz  "/tcc/tcc-boot3"
source_directory:
    .asciz  "/tcc"
no_stdlib_option:
    .asciz  "-nostdlib"
static_option:
    .asciz  "-static"
text_address_option:
    .asciz  "-Wl,-Ttext=0x10074"
output_option:
    .asciz  "-o"
boot0_name:
    .asciz  "tcc-boot0"
boot1_name:
    .asciz  "tcc-boot1"
boot2_name:
    .asciz  "tcc-boot2"
boot3_name:
    .asciz  "tcc-boot3"
define_option:
    .asciz  "-D"
bootstrap_define:
    .asciz  "BOOTSTRAP=1"
float_define:
    .asciz  "HAVE_FLOAT=1"
bitfield_define:
    .asciz  "HAVE_BITFIELD=1"
long_long_define:
    .asciz  "HAVE_LONG_LONG=1"
setjmp_define:
    .asciz  "HAVE_SETJMP=1"
arm_target_define:
    .asciz  "TCC_TARGET_ARM=1"
arm_eabi_define:
    .asciz  "TCC_ARM_EABI=1"
arm_vfp_define:
    .asciz  "TCC_ARM_VFP=1"
include_option:
    .asciz  "-I"
current_include:
    .asciz  "."
mes_include:
    .asciz  "/mes/include"
tccdir_define:
    .asciz  "CONFIG_TCCDIR=\"/mes/lib/tcc\""
crtprefix_define:
    .asciz  "CONFIG_TCC_CRTPREFIX=\"/mes/lib\""
interpreter_define:
    .asciz  "CONFIG_TCC_ELFINTERP=\"/mes/loader\""
library_paths_define:
    .asciz  "CONFIG_TCC_LIBPATHS=\"/mes/lib:/mes/lib/tcc\""
system_include_define:
    .asciz  "CONFIG_TCC_SYSINCLUDEPATHS=\"/mes/include\""
libgcc_define:
    .asciz  "TCC_LIBGCC=\"/mes/lib/libc.a\""
libtcc1_define:
    .asciz  "TCC_LIBTCC1=\"libtcc1.a\""
tccboot_define:
    .asciz  "CONFIG_TCCBOOT=1"
static_define:
    .asciz  "CONFIG_TCC_STATIC=1"
use_libgcc_define:
    .asciz  "CONFIG_USE_LIBGCC=1"
version_define:
    .asciz  "TCC_VERSION=\"0.9.26\""
one_source_define:
    .asciz  "ONE_SOURCE=1"
crt1_path:
    .asciz  "/mes/lib/crt1.o"
crti_path:
    .asciz  "/mes/lib/crti.o"
source_path:
    .asciz  "tcc.c"
libc_path:
    .asciz  "/mes/lib/libc.a"
libtcc1_path:
    .asciz  "/mes/lib/tcc/libtcc1.a"
libgetopt_path:
    .asciz  "/mes/lib/libgetopt.a"
crtn_path:
    .asciz  "/mes/lib/crtn.o"
version_option:
    .asciz  "-version"
home_environment:
    .asciz  "HOME=/"
path_environment:
    .asciz  "PATH=/bin:/sbin"

    .ltorg
    .section .note.GNU-stack,"",%progbits
