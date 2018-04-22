/*
 * fiwix/kernel/syscalls/sysinfo.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/fs.h>
#include <fiwix/system.h>
#include <fiwix/sched.h>
#include <fiwix/mm.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_sysinfo(struct sysinfo *info)
{
	struct sysinfo tmp_info;
	struct proc *p;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_sysinfo(0x%08x)\n ", current->pid, (unsigned int)info);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, info, sizeof(struct sysinfo)))) {
		return errno;
	}
	memset_b(&tmp_info, NULL, sizeof(struct sysinfo));
	tmp_info.loads[0] = avenrun[0] << (SI_LOAD_SHIFT - FSHIFT);
	tmp_info.loads[1] = avenrun[1] << (SI_LOAD_SHIFT - FSHIFT);
	tmp_info.loads[2] = avenrun[2] << (SI_LOAD_SHIFT - FSHIFT);
	tmp_info.uptime = kstat.uptime;
	tmp_info.totalram = kstat.total_mem_pages << PAGE_SHIFT;
	tmp_info.freeram = kstat.free_pages << PAGE_SHIFT;
	tmp_info.sharedram = 0;
	tmp_info.bufferram = kstat.buffers * 1024;
	tmp_info.totalswap = 0;
	tmp_info.freeswap = 0;
	FOR_EACH_PROCESS(p) {
		if(p->state) {
			tmp_info.procs++;
		}
	}

	memcpy_b(info, &tmp_info, sizeof(struct sysinfo));
	return 0;
}
