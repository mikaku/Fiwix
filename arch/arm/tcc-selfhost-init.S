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

    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     build_boot0
    blt     failed
    mov     r4, r0
    bl      wait_for_zero

    ldr     r0, =boot0_path
    movw    r1, #493
    mov     r7, #SYS_CHMOD
    svc     #0
    cmp     r0, #0
    blt     failed

    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     run_boot0
    blt     failed
    mov     r4, r0
    bl      wait_for_zero

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
    bne     failed
    bx      lr

build_boot0:
    ldr     r0, =source_directory
    mov     r7, #SYS_CHDIR
    svc     #0
    cmp     r0, #0
    blt     failed

    ldr     r0, =tcc_path
    ldr     r1, =build_argv
    ldr     r2, =process_envp
    mov     r7, #SYS_EXECVE
    svc     #0
    b       exec_failed

run_boot0:
    ldr     r0, =boot0_path
    ldr     r1, =version_argv
    ldr     r2, =process_envp
    mov     r7, #SYS_EXECVE
    svc     #0

exec_failed:
    mov     r0, #2
    ldr     r1, =exec_failure_message
    ldr     r2, =exec_failure_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

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
build_argv:
    .word   tcc_path
    .word   no_stdlib_option
    .word   static_option
    .word   text_address_option
    .word   output_option
    .word   boot0_name
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
version_argv:
    .word   boot0_path
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
failure_message:
    .ascii  "Fiwix ARM TinyCC self-host process tree failed\n"
failure_message_end:
tcc_path:
    .asciz  "/bin/tcc-mes"
boot0_path:
    .asciz  "/tcc/tcc-boot0"
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
