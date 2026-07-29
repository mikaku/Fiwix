/*
 * fiwix/arch/arm/tcc-seed-init.S
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
.ifdef ARM_LINUX_TCC_SEED
    .equ REBOOT_KEXEC, 0x45584543
    .equ REBOOT_COMMAND, 0x4321fedc
.else
    .equ REBOOT_COMMAND, 0x01234567
.endif

_start:
.ifdef ARM_LINUX_TCC_SEED
    ldr     r0, =REBOOT_MAGIC1
    ldr     r1, =REBOOT_MAGIC2
    ldr     r2, =REBOOT_KEXEC
    mov     r7, #SYS_REBOOT
    svc     #0
.endif

    mov     r0, #1
    ldr     r1, =start_message
    ldr     r2, =start_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    ldr     r0, =work_directory
    mov     r7, #SYS_CHDIR
    svc     #0
    cmp     r0, #0
    blt     failed

    ldr     r5, =compile_argv
    bl      run_mes
    cmp     r0, #0
    bne     failed

    mov     r0, #1
    ldr     r1, =compile_message
    ldr     r2, =compile_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    ldr     r5, =link_argv
    bl      run_mes
    cmp     r0, #0
    bne     failed

    ldr     r0, =seed_path
    movw    r1, #493
    mov     r7, #SYS_CHMOD
    svc     #0
    cmp     r0, #0
    blt     failed

    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     run_seed
    blt     failed
    mov     r4, r0
    bl      wait_child
    cmp     r0, #0
    bne     failed

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

run_mes:
    push    {r4, r6, lr}
    mov     r6, r5
    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     run_mes_child
    blt     run_mes_fork_failed
    mov     r4, r0
    bl      wait_child
    pop     {r4, r6, pc}

run_mes_child:
    ldr     r0, =mes_path
    mov     r1, r6
    ldr     r2, =process_envp
    mov     r7, #SYS_EXECVE
    svc     #0
    b       exec_failed

run_mes_fork_failed:
    mvn     r0, #0
    pop     {r4, r6, pc}

wait_child:
    push    {r5, lr}
    sub     sp, sp, #8
    mov     r0, r4
    mov     r1, sp
    mov     r2, #0
    mov     r3, #0
    mov     r7, #SYS_WAIT4
    svc     #0
    cmp     r0, r4
    bne     wait_failed
    ldr     r0, [sp]
    b       wait_done

wait_failed:
    mvn     r0, #0

wait_done:
    add     sp, sp, #8
    pop     {r5, pc}

run_seed:
    ldr     r0, =seed_path
    ldr     r1, =seed_argv
    ldr     r2, =minimal_envp
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
compile_argv:
    .word   mes_path
    .word   no_auto_compile_option
    .word   entry_option
    .word   main_name
    .word   mescc_path
    .word   assembly_option
    .word   output_option
    .word   assembly_path
    .word   include_option
    .word   mes_include_path
    .word   include_option
    .word   arm_include_path
    .word   define_option
    .word   bootstrap_define
    .word   define_option
    .word   long_long_define
    .word   include_option
    .word   current_directory
    .word   define_option
    .word   target_define
    .word   define_option
    .word   eabi_define
    .word   define_option
    .word   vfp_define
    .word   define_option
    .word   inline_define
    .word   define_option
    .word   tccdir_define
    .word   define_option
    .word   sysroot_define
    .word   define_option
    .word   crt_prefix_define
    .word   define_option
    .word   interpreter_define
    .word   define_option
    .word   include_paths_define
    .word   define_option
    .word   libgcc_define
    .word   define_option
    .word   libtcc1_mes_define
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
    .word   source_path
    .word   0
link_argv:
    .word   mes_path
    .word   no_auto_compile_option
    .word   entry_option
    .word   main_name
    .word   mescc_path
    .word   output_option
    .word   seed_path
    .word   library_option
    .word   library_path
    .word   assembly_path
    .word   link_option
    .word   libc_name
    .word   0
seed_argv:
    .word   seed_path
    .word   version_option
    .word   0
process_envp:
    .word   home_environment
    .word   path_environment
    .word   load_path_environment
    .word   prefix_environment
    .word   arena_environment
    .word   max_arena_environment
    .word   stack_environment
    .word   m1_environment
    .word   hex2_environment
    .word   0
minimal_envp:
    .word   home_environment
    .word   path_environment
    .word   0

start_message:
.ifdef ARM_LINUX_TCC_SEED
    .ascii  "Fiwix ARM Linux MesCC TinyCC seed rebuild entered\n"
.else
    .ascii  "Fiwix ARM MesCC TinyCC seed rebuild entered\n"
.endif
start_message_end:
compile_message:
.ifdef ARM_LINUX_TCC_SEED
    .ascii  "Fiwix ARM Linux MesCC TinyCC source compile completed\n"
.else
    .ascii  "Fiwix ARM MesCC TinyCC source compile completed\n"
.endif
compile_message_end:
complete_message:
.ifdef ARM_LINUX_TCC_SEED
    .ascii  "Fiwix ARM Linux MesCC TinyCC seed rebuild completed\n"
.else
    .ascii  "Fiwix ARM MesCC TinyCC seed rebuild completed\n"
.endif
complete_message_end:
exec_failure_message:
    .ascii  "Fiwix ARM MesCC TinyCC seed exec returned\n"
exec_failure_message_end:
failure_message:
.ifdef ARM_LINUX_TCC_SEED
    .ascii  "Fiwix ARM Linux MesCC TinyCC seed rebuild failed\n"
.else
    .ascii  "Fiwix ARM MesCC TinyCC seed rebuild failed\n"
.endif
failure_message_end:
mes_path:
    .asciz  "/mes/bin/mes-m2"
no_auto_compile_option:
    .asciz  "--no-auto-compile"
entry_option:
    .asciz  "-e"
main_name:
    .asciz  "main"
mescc_path:
    .asciz  "/mes/mescc.scm"
assembly_option:
    .asciz  "-S"
output_option:
    .asciz  "-o"
assembly_path:
    .asciz  "tcc.s"
include_option:
    .asciz  "-I"
mes_include_path:
    .asciz  "/mes/include"
arm_include_path:
    .asciz  "/mes/include/linux/arm"
define_option:
    .asciz  "-D"
bootstrap_define:
    .asciz  "BOOTSTRAP=1"
long_long_define:
    .asciz  "HAVE_LONG_LONG=0"
current_directory:
    .asciz  "."
target_define:
    .asciz  "TCC_TARGET_ARM=1"
eabi_define:
    .asciz  "TCC_ARM_EABI=1"
vfp_define:
    .asciz  "TCC_ARM_VFP=1"
inline_define:
    .asciz  "inline="
tccdir_define:
    .asciz  "CONFIG_TCCDIR=\"/mes/lib/arm-mes/tcc\""
sysroot_define:
    .asciz  "CONFIG_SYSROOT=\"/\""
crt_prefix_define:
    .asciz  "CONFIG_TCC_CRTPREFIX=\"/mes/lib/arm-mes\""
interpreter_define:
    .asciz  "CONFIG_TCC_ELFINTERP=\"/mes/loader\""
include_paths_define:
    .asciz  "CONFIG_TCC_SYSINCLUDEPATHS=\"/mes/include\""
libgcc_define:
    .asciz  "TCC_LIBGCC=\"/mes/lib/arm-mes/libc.a\""
libtcc1_mes_define:
    .asciz  "CONFIG_TCC_LIBTCC1_MES=0"
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
source_path:
    .asciz  "tcc.c"
library_option:
    .asciz  "-L"
library_path:
    .asciz  "/mes/lib"
link_option:
    .asciz  "-l"
libc_name:
    .asciz  "c+tcc"
seed_path:
    .asciz  "/tcc-mes"
version_option:
    .asciz  "-version"
work_directory:
    .asciz  "/tcc"
home_environment:
    .asciz  "HOME=/tcc"
path_environment:
    .asciz  "PATH=/bin:/sbin"
load_path_environment:
    .asciz  "GUILE_LOAD_PATH=/mes/mes/module:/mes/module:/nyacc/module"
prefix_environment:
    .asciz  "MES_PREFIX=/mes"
arena_environment:
    .asciz  "MES_ARENA=50000000"
max_arena_environment:
    .asciz  "MES_MAX_ARENA=50000000"
stack_environment:
    .asciz  "MES_STACK=15000000"
m1_environment:
    .asciz  "M1=/bin/M1"
hex2_environment:
    .asciz  "HEX2=/bin/hex2"

    .ltorg
    .section .note.GNU-stack,"",%progbits
