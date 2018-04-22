/*
 * fiwix/kernel/syscalls/iopl.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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
 * privileges to use any port, even if that port is beyond of the default size
 * of the I/O bitmap in TSS (which is IO_BITMAP_SIZE = 32). Otherwise the
 * processor checks the I/O permission bit map to determine if access to a
 * specific I/O port is allowed.
 *
 * So, we leave it here as in Linux 2.0. That means, leaving to I/O bit map to
 * control the ports up to 0x3FF, and the rest of ports will be controlled by
 * using this system call.
 */

#include <fiwix/process.h>
#include <fiwix/segments.h>
#include <fiwix/sigcontext.h>
#include <fiwix/errno.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_iopl(int level, int arg2, int arg3, int arg4, int arg5, struct sigcontext *sc)
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
