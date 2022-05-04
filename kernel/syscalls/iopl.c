/*
 * fiwix/kernel/syscalls/iopl.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

/*
 * Chapter Input/Output of IA-32 Intel(R) Architecture Software Developer's
 * Manual Volume 1 Basic Architecture, says the processor permits applications
 * to access I/O ports in either of two ways: by using I/O address space or by
 * using memory-mapped I/O. Linux 2.0 and Fiwix uses the first one.
 *
 * This system call sets the IOPL field in the EFLAGS register to the value of
 * 'level' (which is pressumably zero), so the current process will have
 * privileges to use any port, even when the port if denied by the I/O bitmap
 * in TSS.  Otherwise the processor would check the I/O permission bitmap to
 * determine if access to a specific I/O port is allowed.
 *
 * This system call is dangerous and must not be used because it permits the
 * process to execute the Assembly instruction 'cli', which would freeze the
 * kernel.
 */

#include <fiwix/process.h>
#include <fiwix/segments.h>
#include <fiwix/sigcontext.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_iopl(int level, int arg2, int arg3, int arg4, int arg5, int arg6, struct sigcontext *sc)
#else
int sys_iopl(int level, int arg2, int arg3, int arg4, int arg5, struct sigcontext *sc)
#endif
{
#ifdef __DEBUG__
	printk("(pid %d) sys_iopl(%d) -> ", current->pid, level);
#endif /*__DEBUG__ */
	if(level > USR_PL) {
#ifdef __DEBUG__
	printk("-EINVAL\n");
#endif /*__DEBUG__ */
		return -EINVAL;
	}
	if(!IS_SUPERUSER) {
#ifdef __DEBUG__
	printk("-EPERM\n");
#endif /*__DEBUG__ */
		return -EPERM;
	}

	sc->eflags = (sc->eflags & 0xFFFFCFFF) | (level << EF_IOPL);
#ifdef __DEBUG__
	printk("0\n");
#endif /*__DEBUG__ */
	return 0;
}
