/* Linux RV64 syscall-number translation for the generic Fiwix kernel. */

#include <fiwix/arch_process.h>
#include <fiwix/errno.h>
#include <fiwix/riscv64_signal.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/syscalls.h>
#include <fiwix/unistd.h>

#define RV_SCAUSE_U_ECALL	8UL
#define RV_AT_FDCWD		(-100L)
#define RV_CLONE_FORK_FLAGS	17UL	/* SIGCHLD only */

#define RV_SYS_DUP		23
#define RV_SYS_UNLINKAT		35
#define RV_SYS_FACCESSAT	48
#define RV_SYS_CHDIR		49
#define RV_SYS_FCHMODAT		53
#define RV_SYS_OPENAT		56
#define RV_SYS_CLOSE		57
#define RV_SYS_LSEEK		62
#define RV_SYS_READ		63
#define RV_SYS_WRITE		64
#define RV_SYS_EXIT		93
#define RV_SYS_KILL		129
#define RV_SYS_RT_SIGACTION	134
#define RV_SYS_RT_SIGPROCMASK	135
#define RV_SYS_RT_SIGRETURN	139
#define RV_SYS_GETPID		172
#define RV_SYS_GETPPID		173
#define RV_SYS_BRK		214
#define RV_SYS_CLONE		220
#define RV_SYS_EXECVE		221
#define RV_SYS_WAIT4		260

static int riscv64_call_fiwix(unsigned int num,
	struct riscv64_trap_frame *frame, __sysarg_t arg1, __sysarg_t arg2,
	__sysarg_t arg3, __sysarg_t arg4, __sysarg_t arg5, __sysarg_t arg6)
{
	return do_syscall_frame(num, arg1, arg2, arg3, arg4, arg5, arg6,
		(struct sigcontext *)frame);
}

static int riscv64_at_cwd(unsigned long dirfd)
{
	return (signed long)dirfd == RV_AT_FDCWD;
}

int riscv64_user_syscall(struct riscv64_trap_frame *frame,
	unsigned long cause)
{
	int result;

	if(cause != RV_SCAUSE_U_ECALL) {
		return -1;
	}
	frame->sepc += 4;

	switch(frame->a7) {
		case RV_SYS_DUP:
			result = riscv64_call_fiwix(SYS_dup, frame, frame->a0, 0,
				0, 0, 0, 0);
			break;
		case RV_SYS_UNLINKAT:
			if(!riscv64_at_cwd(frame->a0) || frame->a2) {
				result = -EINVAL;
				break;
			}
			result = riscv64_call_fiwix(SYS_unlink, frame, frame->a1, 0,
				0, 0, 0, 0);
			break;
		case RV_SYS_FACCESSAT:
			if(!riscv64_at_cwd(frame->a0)) {
				result = -EBADF;
				break;
			}
			result = riscv64_call_fiwix(SYS_access, frame, frame->a1,
				frame->a2, 0, 0, 0, 0);
			break;
		case RV_SYS_CHDIR:
			result = riscv64_call_fiwix(SYS_chdir, frame, frame->a0, 0,
				0, 0, 0, 0);
			break;
		case RV_SYS_FCHMODAT:
			if(!riscv64_at_cwd(frame->a0)) {
				result = -EBADF;
				break;
			}
			result = riscv64_call_fiwix(SYS_chmod, frame, frame->a1,
				frame->a2, 0, 0, 0, 0);
			break;
		case RV_SYS_OPENAT:
			if(!riscv64_at_cwd(frame->a0)) {
				result = -EBADF;
				break;
			}
			result = riscv64_call_fiwix(SYS_open, frame, frame->a1,
				frame->a2, frame->a3, 0, 0, 0);
			break;
		case RV_SYS_CLOSE:
			result = riscv64_call_fiwix(SYS_close, frame, frame->a0, 0,
				0, 0, 0, 0);
			break;
		case RV_SYS_LSEEK:
			result = riscv64_call_fiwix(SYS_lseek, frame, frame->a0,
				frame->a1, frame->a2, 0, 0, 0);
			break;
		case RV_SYS_READ:
			result = riscv64_call_fiwix(SYS_read, frame, frame->a0,
				frame->a1, frame->a2, 0, 0, 0);
			break;
		case RV_SYS_WRITE:
			result = riscv64_call_fiwix(SYS_write, frame, frame->a0,
				frame->a1, frame->a2, 0, 0, 0);
			break;
		case RV_SYS_EXIT:
			result = riscv64_call_fiwix(SYS_exit, frame, frame->a0, 0,
				0, 0, 0, 0);
			break;
		case RV_SYS_KILL:
			if(frame->a1 >= NSIG) {
				result = -EINVAL;
				break;
			}
			result = riscv64_call_fiwix(SYS_kill, frame,
				frame->a0, frame->a1, 0, 0, 0, 0);
			break;
		case RV_SYS_RT_SIGACTION:
			result = riscv64_rt_sigaction(frame->a0,
				(const void *)frame->a1, (void *)frame->a2, frame->a3);
			break;
		case RV_SYS_RT_SIGPROCMASK:
			result = riscv64_rt_sigprocmask(frame->a0,
				(const void *)frame->a1, (void *)frame->a2, frame->a3);
			break;
		case RV_SYS_RT_SIGRETURN:
			result = riscv64_signal_return(frame);
			if(!result) {
				return 0;
			}
			break;
		case RV_SYS_GETPID:
			result = riscv64_call_fiwix(SYS_getpid, frame, 0, 0, 0, 0, 0, 0);
			break;
		case RV_SYS_GETPPID:
			result = riscv64_call_fiwix(SYS_getppid, frame, 0, 0, 0, 0, 0, 0);
			break;
		case RV_SYS_BRK:
			result = riscv64_call_fiwix(SYS_brk, frame, frame->a0, 0,
				0, 0, 0, 0);
			break;
		case RV_SYS_CLONE:
			/* Extra clone arguments are ignored without their enabling flags. */
			if(frame->a0 != RV_CLONE_FORK_FLAGS || frame->a1) {
				result = -EINVAL;
				break;
			}
			result = riscv64_call_fiwix(SYS_fork, frame, 0, 0, 0, 0, 0, 0);
			break;
		case RV_SYS_EXECVE:
			result = riscv64_call_fiwix(SYS_execve, frame, frame->a0,
				frame->a1, frame->a2, 0, 0, 0);
			break;
		case RV_SYS_WAIT4:
			result = riscv64_call_fiwix(SYS_wait4, frame, frame->a0,
				frame->a1, frame->a2, frame->a3, 0, 0);
			break;
		default:
			result = -ENOSYS;
	}
	frame->a0 = (signed long)result;
	return 0;
}
