/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_trap.h>
#include <fiwix/errno.h>
#include <fiwix/sigcontext.h>
#include <fiwix/syscalls.h>
#include <fiwix/unistd.h>

static unsigned int called_num;
static __sysarg_t called_args[6];
static struct sigcontext *called_frame;
static int syscall_result;

int do_syscall_frame(unsigned int num, __sysarg_t arg1, __sysarg_t arg2,
	__sysarg_t arg3, __sysarg_t arg4, __sysarg_t arg5, __sysarg_t arg6,
	struct sigcontext *frame)
{
	called_num = num;
	called_args[0] = arg1;
	called_args[1] = arg2;
	called_args[2] = arg3;
	called_args[3] = arg4;
	called_args[4] = arg5;
	called_args[5] = arg6;
	called_frame = frame;
	return syscall_result;
}

static void clear_frame(struct arm_trap_frame *frame)
{
	unsigned int *word;
	unsigned int n;

	word = (unsigned int *)frame;
	for(n = 0; n < sizeof(*frame) / sizeof(*word); n++) {
		word[n] = 0;
	}
	called_num = ~0U;
	called_frame = 0;
	syscall_result = 0;
}

int main(void)
{
	static const struct {
		unsigned int arm_num;
		unsigned int fiwix_num;
	} mappings[] = {
		{ 1, SYS_exit },
		{ 2, SYS_fork },
		{ 3, SYS_read },
		{ 4, SYS_write },
		{ 5, SYS_open },
		{ 6, SYS_close },
		{ 10, SYS_unlink },
		{ 11, SYS_execve },
		{ 12, SYS_chdir },
		{ 15, SYS_chmod },
		{ 19, SYS_lseek },
		{ 20, SYS_getpid },
		{ 33, SYS_access },
		{ 36, SYS_sync },
		{ 37, SYS_kill },
		{ 41, SYS_dup },
		{ 45, SYS_brk },
		{ 63, SYS_dup2 },
		{ 64, SYS_getppid },
		{ 88, SYS_reboot },
		{ 114, SYS_wait4 },
		{ 120, SYS_fork },
		{ 183, SYS_getcwd },
		{ 248, SYS_exit },
		{ 322, SYS_open },
		{ 323, SYS_mkdir },
		{ 328, SYS_unlink },
		{ 333, SYS_chmod },
		{ 334, SYS_access }
	};
	struct arm_trap_frame frame;
	unsigned int n;

	for(n = 0; n < sizeof(mappings) / sizeof(mappings[0]); n++) {
		clear_frame(&frame);
		frame.r[7] = mappings[n].arm_num;
		if(frame.r[7] >= 322) {
			frame.r[0] = (unsigned int)-100;
		}
		if(frame.r[7] == 120) {
			frame.r[0] = 17;
		}
		if(arm_eabi_user_syscall(&frame) ||
			called_num != mappings[n].fiwix_num ||
			called_frame != (struct sigcontext *)&frame) {
			return 10 + n;
		}
	}

	clear_frame(&frame);
	frame.pc = 0x00100200;
	frame.r[7] = 322;
	frame.r[0] = (unsigned int)-100;
	frame.r[1] = 0x00102000;
	frame.r[2] = 2;
	frame.r[3] = 0644;
	syscall_result = -EACCES;
	if(arm_eabi_user_syscall(&frame) || frame.pc != 0x00100200 ||
		(int)frame.r[0] != -EACCES || called_num != SYS_open ||
		called_args[0] != 0x00102000 || called_args[1] != 2 ||
		called_args[2] != 0644) {
		return 2;
	}

	clear_frame(&frame);
	frame.r[7] = 322;
	frame.r[0] = 4;
	if(arm_eabi_user_syscall(&frame) || (int)frame.r[0] != -EBADF ||
		called_num != ~0U) {
		return 3;
	}

	clear_frame(&frame);
	frame.r[7] = 120;
	frame.r[0] = 17;
	syscall_result = 37;
	if(arm_eabi_user_syscall(&frame) || frame.r[0] != 37 ||
		called_num != SYS_fork) {
		return 4;
	}

	clear_frame(&frame);
	frame.r[7] = 120;
	frame.r[0] = 0x111;
	if(arm_eabi_user_syscall(&frame) || (int)frame.r[0] != -EINVAL ||
		called_num != ~0U) {
		return 5;
	}

	clear_frame(&frame);
	frame.r[7] = 328;
	frame.r[0] = (unsigned int)-100;
	frame.r[2] = 1;
	if(arm_eabi_user_syscall(&frame) || (int)frame.r[0] != -EINVAL ||
		called_num != ~0U) {
		return 6;
	}

	clear_frame(&frame);
	frame.r[7] = 37;
	frame.r[1] = 64;
	if(arm_eabi_user_syscall(&frame) || (int)frame.r[0] != -EINVAL ||
		called_num != ~0U) {
		return 7;
	}

	clear_frame(&frame);
	frame.r[7] = 0xFFFF;
	if(arm_eabi_user_syscall(&frame) || (int)frame.r[0] != -ENOSYS ||
		called_num != ~0U) {
		return 8;
	}

	return 0;
}
