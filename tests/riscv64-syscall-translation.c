#include <fiwix/errno.h>
#include <fiwix/riscv64_trap.h>
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

static void clear_frame(struct riscv64_trap_frame *frame)
{
	unsigned long *word;
	unsigned int n;

	word = (unsigned long *)frame;
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
		unsigned long riscv64_num;
		unsigned int fiwix_num;
	} mappings[] = {
		{ 35, SYS_unlink },
		{ 48, SYS_access },
		{ 49, SYS_chdir },
		{ 53, SYS_chmod },
		{ 56, SYS_open },
		{ 57, SYS_close },
		{ 62, SYS_lseek },
		{ 63, SYS_read },
		{ 64, SYS_write },
		{ 93, SYS_exit },
		{ 172, SYS_getpid },
		{ 173, SYS_getppid },
		{ 214, SYS_brk },
		{ 220, SYS_fork },
		{ 260, SYS_wait4 }
	};
	struct riscv64_trap_frame frame;
	unsigned int n;

	clear_frame(&frame);
	frame.sepc = 100;
	if(riscv64_user_syscall(&frame, 9) != -1 || frame.sepc != 100 ||
		called_num != ~0U) {
		return 1;
	}
	for(n = 0; n < sizeof(mappings) / sizeof(mappings[0]); n++) {
		clear_frame(&frame);
		frame.a7 = mappings[n].riscv64_num;
		if(frame.a7 == 35 || frame.a7 == 48 || frame.a7 == 53 ||
			frame.a7 == 56) {
			frame.a0 = (unsigned long)-100L;
		}
		if(frame.a7 == 220) {
			frame.a0 = 17;
		}
		if(riscv64_user_syscall(&frame, 8) ||
			called_num != mappings[n].fiwix_num) {
			return 10 + n;
		}
	}

	clear_frame(&frame);
	frame.sepc = 200;
	frame.a7 = 56;
	frame.a0 = (unsigned long)-100L;
	frame.a1 = 0x100000001UL;
	frame.a2 = 2;
	frame.a3 = 0644;
	syscall_result = -EACCES;
	if(riscv64_user_syscall(&frame, 8) || frame.sepc != 204 ||
		(signed long)frame.a0 != -EACCES || called_num != SYS_open ||
		called_args[0] != 0x100000001UL || called_args[1] != 2 ||
		called_args[2] != 0644 ||
		called_frame != (struct sigcontext *)&frame) {
		return 2;
	}

	clear_frame(&frame);
	frame.a7 = 56;
	frame.a0 = 4;
	if(riscv64_user_syscall(&frame, 8) ||
		(signed long)frame.a0 != -EBADF || called_num != ~0U) {
		return 3;
	}

	clear_frame(&frame);
	frame.a7 = 220;
	frame.a0 = 17;
	syscall_result = 37;
	if(riscv64_user_syscall(&frame, 8) || frame.a0 != 37 ||
		called_num != SYS_fork) {
		return 4;
	}

	clear_frame(&frame);
	frame.a7 = 220;
	frame.a0 = 0x111;
	if(riscv64_user_syscall(&frame, 8) ||
		(signed long)frame.a0 != -EINVAL || called_num != ~0U) {
		return 5;
	}

	clear_frame(&frame);
	frame.a7 = 221;
	if(riscv64_user_syscall(&frame, 8) ||
		(signed long)frame.a0 != -ENOSYS || called_num != ~0U) {
		return 6;
	}

	clear_frame(&frame);
	frame.a7 = 260;
	frame.a0 = (unsigned long)-1L;
	frame.a1 = 0x200000002UL;
	frame.a2 = 1;
	frame.a3 = 0x300000003UL;
	if(riscv64_user_syscall(&frame, 8) || called_num != SYS_wait4 ||
		called_args[0] != (unsigned long)-1L ||
		called_args[1] != 0x200000002UL || called_args[2] != 1 ||
		called_args[3] != 0x300000003UL) {
		return 7;
	}

	return 0;
}
