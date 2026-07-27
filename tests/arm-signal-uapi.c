/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#define _GNU_SOURCE

#include <signal.h>
#include <stddef.h>
#include <sys/ucontext.h>

typedef char arm_linux_siginfo_size_must_be_128[
	(sizeof(siginfo_t) == 128) ? 1 : -1];
typedef char arm_linux_stack_size_must_be_12[
	(sizeof(stack_t) == 12) ? 1 : -1];
typedef char arm_linux_mcontext_size_must_be_84[
	(sizeof(mcontext_t) == 84) ? 1 : -1];
typedef char arm_linux_ucontext_size_must_be_744[
	(sizeof(ucontext_t) == 744) ? 1 : -1];
typedef char arm_linux_mcontext_offset_must_be_20[
	(offsetof(ucontext_t, uc_mcontext) == 20) ? 1 : -1];
typedef char arm_linux_sigmask_offset_must_be_104[
	(offsetof(ucontext_t, uc_sigmask) == 104) ? 1 : -1];
typedef char arm_linux_regspace_offset_must_be_232[
	(offsetof(ucontext_t, uc_regspace) == 232) ? 1 : -1];

int arm_signal_uapi_layout(void)
{
	return 0;
}
