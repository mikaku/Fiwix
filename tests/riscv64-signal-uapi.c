#define _GNU_SOURCE

#include <signal.h>
#include <stddef.h>
#include <sys/ucontext.h>

typedef char riscv64_linux_siginfo_size_must_be_128[
	(sizeof(siginfo_t) == 128) ? 1 : -1];
typedef char riscv64_linux_stack_size_must_be_24[
	(sizeof(stack_t) == 24) ? 1 : -1];
typedef char riscv64_linux_mcontext_size_must_be_784[
	(sizeof(mcontext_t) == 784) ? 1 : -1];
typedef char riscv64_linux_ucontext_size_must_be_960[
	(sizeof(ucontext_t) == 960) ? 1 : -1];
typedef char riscv64_linux_sigmask_offset_must_be_40[
	(offsetof(ucontext_t, uc_sigmask) == 40) ? 1 : -1];
typedef char riscv64_linux_mcontext_offset_must_be_176[
	(offsetof(ucontext_t, uc_mcontext) == 176) ? 1 : -1];

int riscv64_signal_uapi_layout(void)
{
	return 0;
}
