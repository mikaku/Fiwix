/*
 * fiwix/arch/arm/syscall.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_trap.h>
#include <fiwix/errno.h>
#include <fiwix/signal.h>
#include <fiwix/syscalls.h>
#include <fiwix/unistd.h>

#define ARM_AT_FDCWD			(-100)
#define ARM_CLONE_FORK_FLAGS		17U	/* SIGCHLD only */

#define ARM_SYS_EXIT			1
#define ARM_SYS_FORK			2
#define ARM_SYS_READ			3
#define ARM_SYS_WRITE			4
#define ARM_SYS_OPEN			5
#define ARM_SYS_CLOSE			6
#define ARM_SYS_UNLINK			10
#define ARM_SYS_EXECVE			11
#define ARM_SYS_CHDIR			12
#define ARM_SYS_CHMOD			15
#define ARM_SYS_LSEEK			19
#define ARM_SYS_GETPID			20
#define ARM_SYS_ACCESS			33
#define ARM_SYS_SYNC			36
#define ARM_SYS_KILL			37
#define ARM_SYS_DUP			41
#define ARM_SYS_BRK			45
#define ARM_SYS_DUP2			63
#define ARM_SYS_GETPPID			64
#define ARM_SYS_REBOOT			88
#define ARM_SYS_WAIT4			114
#define ARM_SYS_CLONE			120
#define ARM_SYS_GETCWD			183
#define ARM_SYS_EXIT_GROUP		248
#define ARM_SYS_OPENAT			322
#define ARM_SYS_MKDIRAT			323
#define ARM_SYS_UNLINKAT			328
#define ARM_SYS_FCHMODAT			333
#define ARM_SYS_FACCESSAT		334

static int arm_call_fiwix(unsigned int num, struct arm_trap_frame *frame,
	__sysarg_t arg1, __sysarg_t arg2, __sysarg_t arg3, __sysarg_t arg4,
	__sysarg_t arg5, __sysarg_t arg6)
{
	return do_syscall_frame(num, arg1, arg2, arg3, arg4, arg5, arg6,
		(struct sigcontext *)frame);
}

static int arm_at_cwd(unsigned int dirfd)
{
	return (int)dirfd == ARM_AT_FDCWD;
}

int arm_eabi_user_syscall(struct arm_trap_frame *frame)
{
	int result;

	switch(frame->r[7]) {
		case ARM_SYS_EXIT:
		case ARM_SYS_EXIT_GROUP:
			result = arm_call_fiwix(SYS_exit, frame, frame->r[0],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_FORK:
			result = arm_call_fiwix(SYS_fork, frame, 0, 0, 0, 0, 0, 0);
			break;
		case ARM_SYS_READ:
			result = arm_call_fiwix(SYS_read, frame, frame->r[0],
				frame->r[1], frame->r[2], 0, 0, 0);
			break;
		case ARM_SYS_WRITE:
			result = arm_call_fiwix(SYS_write, frame, frame->r[0],
				frame->r[1], frame->r[2], 0, 0, 0);
			break;
		case ARM_SYS_OPEN:
			result = arm_call_fiwix(SYS_open, frame, frame->r[0],
				frame->r[1], frame->r[2], 0, 0, 0);
			break;
		case ARM_SYS_CLOSE:
			result = arm_call_fiwix(SYS_close, frame, frame->r[0],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_UNLINK:
			result = arm_call_fiwix(SYS_unlink, frame, frame->r[0],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_EXECVE:
			result = arm_call_fiwix(SYS_execve, frame, frame->r[0],
				frame->r[1], frame->r[2], 0, 0, 0);
			break;
		case ARM_SYS_CHDIR:
			result = arm_call_fiwix(SYS_chdir, frame, frame->r[0],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_CHMOD:
			result = arm_call_fiwix(SYS_chmod, frame, frame->r[0],
				frame->r[1], 0, 0, 0, 0);
			break;
		case ARM_SYS_LSEEK:
			result = arm_call_fiwix(SYS_lseek, frame, frame->r[0],
				frame->r[1], frame->r[2], 0, 0, 0);
			break;
		case ARM_SYS_GETPID:
			result = arm_call_fiwix(SYS_getpid, frame, 0, 0, 0, 0, 0, 0);
			break;
		case ARM_SYS_ACCESS:
			result = arm_call_fiwix(SYS_access, frame, frame->r[0],
				frame->r[1], 0, 0, 0, 0);
			break;
		case ARM_SYS_SYNC:
			result = arm_call_fiwix(SYS_sync, frame, 0, 0, 0, 0, 0, 0);
			break;
		case ARM_SYS_KILL:
			if(frame->r[1] >= NSIG) {
				result = -EINVAL;
				break;
			}
			result = arm_call_fiwix(SYS_kill, frame, frame->r[0],
				frame->r[1], 0, 0, 0, 0);
			break;
		case ARM_SYS_DUP:
			result = arm_call_fiwix(SYS_dup, frame, frame->r[0],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_BRK:
			result = arm_call_fiwix(SYS_brk, frame, frame->r[0],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_DUP2:
			result = arm_call_fiwix(SYS_dup2, frame, frame->r[0],
				frame->r[1], 0, 0, 0, 0);
			break;
		case ARM_SYS_GETPPID:
			result = arm_call_fiwix(SYS_getppid, frame, 0, 0, 0, 0, 0, 0);
			break;
		case ARM_SYS_REBOOT:
			result = arm_call_fiwix(SYS_reboot, frame,
				(__sysarg_t)(int)frame->r[0],
				(__sysarg_t)(int)frame->r[1],
				(__sysarg_t)(int)frame->r[2], 0, 0, 0);
			break;
		case ARM_SYS_WAIT4:
			result = arm_call_fiwix(SYS_wait4, frame, frame->r[0],
				frame->r[1], frame->r[2], frame->r[3], 0, 0);
			break;
		case ARM_SYS_CLONE:
			if(frame->r[0] != ARM_CLONE_FORK_FLAGS || frame->r[1]) {
				result = -EINVAL;
				break;
			}
			result = arm_call_fiwix(SYS_fork, frame, 0, 0, 0, 0, 0, 0);
			break;
		case ARM_SYS_GETCWD:
			result = arm_call_fiwix(SYS_getcwd, frame, frame->r[0],
				frame->r[1], 0, 0, 0, 0);
			break;
		case ARM_SYS_OPENAT:
			if(!arm_at_cwd(frame->r[0])) {
				result = -EBADF;
				break;
			}
			result = arm_call_fiwix(SYS_open, frame, frame->r[1],
				frame->r[2], frame->r[3], 0, 0, 0);
			break;
		case ARM_SYS_MKDIRAT:
			if(!arm_at_cwd(frame->r[0])) {
				result = -EBADF;
				break;
			}
			result = arm_call_fiwix(SYS_mkdir, frame, frame->r[1],
				frame->r[2], 0, 0, 0, 0);
			break;
		case ARM_SYS_UNLINKAT:
			if(!arm_at_cwd(frame->r[0]) || frame->r[2]) {
				result = -EINVAL;
				break;
			}
			result = arm_call_fiwix(SYS_unlink, frame, frame->r[1],
				0, 0, 0, 0, 0);
			break;
		case ARM_SYS_FCHMODAT:
			if(!arm_at_cwd(frame->r[0])) {
				result = -EBADF;
				break;
			}
			result = arm_call_fiwix(SYS_chmod, frame, frame->r[1],
				frame->r[2], 0, 0, 0, 0);
			break;
		case ARM_SYS_FACCESSAT:
			if(!arm_at_cwd(frame->r[0])) {
				result = -EBADF;
				break;
			}
			result = arm_call_fiwix(SYS_access, frame, frame->r[1],
				frame->r[2], 0, 0, 0, 0);
			break;
		default:
			result = -ENOSYS;
	}
	frame->r[0] = (unsigned int)result;
	return 0;
}
