/*
 * fiwix/kernel/syscalls/sigreturn.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/sigcontext.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_sigreturn(unsigned int signum, int arg2, int arg3, int arg4, int arg5, int arg6, struct sigcontext *sc)
#else
int sys_sigreturn(unsigned int signum, int arg2, int arg3, int arg4, int arg5, struct sigcontext *sc)
#endif /* CONFIG_SYSCALL_6TH_ARG */
{
#ifdef __DEBUG__
	printk("(pid %d) sys_sigreturn(0x%08x)\n", current->pid, signum);
#endif /*__DEBUG__ */

	current->sigblocked &= ~current->sigexecuting;
	current->sigexecuting = 0;
	memcpy_b(sc, &current->sc[signum - 1], sizeof(struct sigcontext));

	/*
	 * We return here the value that the syscall was returning when it was
	 * interrupted by a signal.
	 */
	return current->sc[signum - 1].eax;
}
