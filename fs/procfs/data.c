/*
 * fiwix/fs/procfs/data.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/system.h>
#include <fiwix/types.h>
#include <fiwix/process.h>
#include <fiwix/cmos.h>
#include <fiwix/dma.h>
#include <fiwix/ide.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/devices.h>
#include <fiwix/locks.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/fs_proc.h>
#include <fiwix/cpu.h>
#include <fiwix/irq.h>
#include <fiwix/sched.h>
#include <fiwix/timer.h>
#include <fiwix/utsname.h>
#include <fiwix/version.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define FSHIFT16	16
#define FIXED16_1	(1 << FSHIFT16)
#define LOAD_INT(x)     ((x) >> FSHIFT16)
#define LOAD_FRAC(x)    LOAD_INT(((x) & (FIXED16_1 - 1)) * 100)

static const char *pstate[] = {
	"? (unused!)",
	"R (running)",
	"S (sleeping)",
	"Z (zombie)",
	"T (stopped)",
	"D (idle)",
};

/*
 * procfs root directory related functions
 * ---------------------------------------
 */
int data_proc_self(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s", current->pidstr);
}

int data_proc_buddyinfo(char *buffer, __pid_t pid)
{
	int n, level, size;

	size = sprintk(buffer, "Sizes:");
	for(level = 32, n = 0; n < BUDDY_MAX_LEVEL; n++, level <<= 1) {
		size += sprintk(buffer + size, "\t%d", level);
	}
	size += sprintk(buffer + size, "\n");
	size += sprintk(buffer + size, "------------------------------------------------------------\n");
	for(n = 0; n < BUDDY_MAX_LEVEL; n++) {
		size += sprintk(buffer + size, "\t%d", kstat.buddy_low_count[n]);
	}
	size += sprintk(buffer + size, "\n\n");
	size += sprintk(buffer + size, "Memory requested (used): %d bytes (%dKB)\n", kstat.buddy_low_mem_requested, (kstat.buddy_low_num_pages * PAGE_SIZE / 1024));

	return size;
}

int data_proc_cmdline(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s\n", cmdline);
}

int data_proc_cpuinfo(char *buffer, __pid_t pid)
{
	int size;

	size = sprintk(buffer, "processor       : 0\n");
	size += sprintk(buffer + size, "cpu family      : %d86\n", cpu_table.family <= 6 ? cpu_table.family : 6);
	if(cpu_table.model >= 0) {
		size += sprintk(buffer + size, "model           : %d\n", cpu_table.model);
	} else {
		size += sprintk(buffer + size, "model           : unknown\n");
	}

	if(cpu_table.vendor_id) {
		size += sprintk(buffer + size, "vendor_id       : %s\n", cpu_table.vendor_id);
	}
	if(cpu_table.model_name) {
		size += sprintk(buffer + size, "model name      : %s\n", cpu_table.model_name);
	}
	if(cpu_table.stepping >= 0) {
		size += sprintk(buffer + size, "stepping        : %d\n", cpu_table.stepping);
	} else {
		size += sprintk(buffer + size, "stepping        : unknown\n");
	}

	size += sprintk(buffer + size, "cpu MHz         : ");
	if(cpu_table.hz) {
		size += sprintk(buffer + size, "%d.%d\n", (cpu_table.hz / 1000000), ((cpu_table.hz % 1000000) / 100000));
	} else {
		size += sprintk(buffer + size, "unknown\n");
	}
	if(cpu_table.cache) {
		size += sprintk(buffer + size, "cache size      : %s\n", cpu_table.cache);
	}
	size += sprintk(buffer + size, "cpuid           : %s\n", cpu_table.has_cpuid ? "yes" : "no");
	size += sprintk(buffer + size, "fpu             : %s\n", cpu_table.has_fpu ? "yes" : "no");
	size += get_cpu_flags(buffer + size);
	return size;
}

int data_proc_devices(char *buffer, __pid_t pid)
{
	int n, size;
	struct device *d;

	size = sprintk(buffer, "Character devices:\n");
	for(n = 0; n < NR_CHRDEV; n++) {
		d = chr_device_table[n];
		while(d) {
			size += sprintk(buffer + size, "%3d %s\n", d->major, d->name);
			d = d->next;
		}
	}

	size += sprintk(buffer + size, "\nBlock devices:\n");
	for(n = 0; n < NR_BLKDEV; n++) {
		d = blk_device_table[n];
		while(d) {
			size += sprintk(buffer + size, "%3d %s\n", d->major, d->name);
			d = d->next;
		}
	}
	return size;
}

int data_proc_dma(char *buffer, __pid_t pid)
{
	int n, size;

	size = 0;
	for(n = 0; n < DMA_CHANNELS; n++) {
		if(dma_resources[n]) {
			size += sprintk(buffer + size, "%2d: %s\n", n, dma_resources[n]);
		}
	}
	return size;
}

int data_proc_filesystems(char *buffer, __pid_t pid)
{
	int n, size;
	int nodev;

	size = 0;
	for(n = 0; n < NR_FILESYSTEMS; n++) {
		if(filesystems_table[n].name) {
			nodev = 0;
			if(filesystems_table[n].fsop->flags != FSOP_REQUIRES_DEV) {
				nodev = 1;
			}
			size += sprintk(buffer + size, "%s %s\n", nodev ? "nodev" : "     ", filesystems_table[n].name);
		}
	}
	return size;
}

int data_proc_interrupts(char *buffer, __pid_t pid)
{
	struct interrupt *irq;
	int n, size;

	size = 0;
	for(n = 0; n < NR_IRQS; n++) {
		if((irq = irq_table[n])) {
			size += sprintk(buffer + size, "%3d: %9u %s", n, irq->ticks, irq->name);
			while((irq = irq->next)) {
				size += sprintk(buffer + size, ",%s", irq->name);
			}
			size += sprintk(buffer + size, "\n");
		}
	}
	size += sprintk(buffer + size, "SPU: %9u %s\n", kstat.sirqs, "Spurious interrupts");
	return size;
}

int data_proc_loadavg(char *buffer, __pid_t pid)
{
	int a, b, c;
	int size;
	struct proc *p;
	int nrun = 0;
	int nprocs = 0;

	a = avenrun[0] << (SI_LOAD_SHIFT - FSHIFT);
	b = avenrun[1] << (SI_LOAD_SHIFT - FSHIFT);
	c = avenrun[2] << (SI_LOAD_SHIFT - FSHIFT);

	FOR_EACH_PROCESS(p) {
		nprocs++;
		if(p->state == PROC_RUNNING) {
			nrun++;
		}
		p = p->next;
	}

	size = sprintk(buffer, "%d.%02d %d.%02d %d.%02d %d/%d %d\n", LOAD_INT(a), LOAD_FRAC(a), LOAD_INT(b), LOAD_FRAC(b), LOAD_INT(c), LOAD_FRAC(c), nrun, nprocs, lastpid);
	return size;
}

int data_proc_locks(char *buffer, __pid_t pid)
{
	int n, size;
	struct flock_file *ff;

	size = 0;

	for(n = 0; n < NR_FLOCKS; n++) {
		ff = &flock_file_table[n];
		if(ff->inode) {
			size += sprintk(buffer + size, "%d: FLOCK  ADVISORY  %s ", n + 1, ff->type & LOCK_SH ? "READ " : "WRITE");
			size += sprintk(buffer + size, "%d %x:%d:%d 0 EOF\n", ff->proc->pid, MAJOR(ff->inode->dev), MINOR(ff->inode->dev), ff->inode->inode);
		}
	}

	return size;
}

int data_proc_meminfo(char *buffer, __pid_t pid)
{
	struct page *pg;
	int n, size;

	kstat.shared = 0;
	for(n = 0; n < kstat.physical_pages; n++) {
		pg = &page_table[n];
		if(pg->flags & PAGE_RESERVED) {
			continue;
		}
		if(!pg->count) {
			continue;
		}
		kstat.shared += pg->count - 1;
	}

	size = 0;
	size += sprintk(buffer + size, "        total:    used:    free:  shared: buffers:  cached:\n");
	size += sprintk(buffer + size, "Mem:  %8u %8u %8u %8u %8u %8u\n", kstat.total_mem_pages << PAGE_SHIFT, (kstat.total_mem_pages << PAGE_SHIFT) - (kstat.free_pages << PAGE_SHIFT), kstat.free_pages << PAGE_SHIFT, kstat.shared * 1024, kstat.buffers * 1024, kstat.cached * 1024);
	size += sprintk(buffer + size, "Swap: %8u %8u %8u\n", 0, 0, 0);
	size += sprintk(buffer + size, "MemTotal: %9d kB\n", kstat.total_mem_pages << 2);
	size += sprintk(buffer + size, "MemFree:  %9d kB\n", kstat.free_pages << 2);
	size += sprintk(buffer + size, "MemShared:%9d kB\n", kstat.shared);
	size += sprintk(buffer + size, "Buffers:  %9d kB\n", kstat.buffers);
	size += sprintk(buffer + size, "Cached:   %9d kB\n", kstat.cached);
	size += sprintk(buffer + size, "SwapTotal:%9d kB\n", 0);
	size += sprintk(buffer + size, "SwapFree: %9d kB\n", 0);
	size += sprintk(buffer + size, "Dirty:    %9d kB\n", kstat.dirty_buffers);
	return size;
}

int data_proc_mounts(char *buffer, __pid_t pid)
{
	int size;
	char *flag;
	struct mount *mp;

	size = 0;
	mp = mount_table;

	while(mp) {
		if(mp->fs->fsop->flags != FSOP_KERN_MOUNT) {
			flag = mp->sb.flags & MS_RDONLY ? "ro" : "rw";
			size += sprintk(buffer + size, "%s %s %s %s 0 0\n", mp->devname, mp->dirname, mp->fs->name, flag);
		}
		mp = mp->next;
	}
	return size;
}

int data_proc_partitions(char *buffer, __pid_t pid)
{
	int n, ctrl, drv, size;
	int minor, major;
	unsigned int blocks;
	struct ide *ide;
	struct ide_drv *drive;

	size = 0;
	size += sprintk(buffer + size, "major minor  #blocks  name\n\n");

	for(ctrl = 0; ctrl < NR_IDE_CTRLS; ctrl++) {
		ide = &ide_table[ctrl];
		for(drv = 0; drv < NR_IDE_DRVS; drv++) {
			drive = &ide->drive[drv];
			if(!drive->nr_sects) {
				continue;
			}
			if(drive->flags & DEVICE_IS_DISK) {
				major = (int)drive->major;
				minor = (int)drive->minor_shift;
				blocks = drive->nr_sects / 2;
				size += sprintk(buffer + size, "%4d  %4d  %9d %s\n", major, 0, blocks, drive->dev_name);
				for(n = 0; n < NR_PARTITIONS; n++) {
					if(drive->part_table[n].type) {
						blocks = drive->part_table[n].nr_sects / 2;
						size += sprintk(buffer + size, "%4d  %4d  %9u %s%d\n", major, (n + 1) << minor, blocks, drive->dev_name, n + 1);
					}
				}
			}
		}
	}
	return size;
}

int data_proc_rtc(char *buffer, __pid_t pid)
{
	int size;
	short int sec, min, hour;
	short int day, month, year, century;

	sec = cmos_read_date(CMOS_SEC);
	min = cmos_read_date(CMOS_MIN);
	hour = cmos_read_date(CMOS_HOUR);
	day = cmos_read_date(CMOS_DAY);
	month = cmos_read_date(CMOS_MONTH);
	year = cmos_read_date(CMOS_YEAR);
	century = cmos_read_date(CMOS_CENTURY);
	year += century * 100;

	size = 0;
	size += sprintk(buffer + size, "rtc_time\t: %02d:%02d:%02d\n", hour, min, sec);
	size += sprintk(buffer + size, "rtc_date\t: %02d-%02d-%02d\n", year, month, day);
	sec = cmos_read_date(CMOS_ASEC);
	min = cmos_read_date(CMOS_AMIN);
	hour = cmos_read_date(CMOS_AHOUR);
	size += sprintk(buffer + size, "alarm\t\t: %02d:%02d:%02d\n", hour, min, sec);
	size += sprintk(buffer + size, "DST_enable\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_DSE ? "yes" : "no");
	size += sprintk(buffer + size, "BCD\t\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_DM ? "no" : "yes");
	size += sprintk(buffer + size, "24hr\t\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_24H ? "yes" : "no");
	size += sprintk(buffer + size, "square_wave\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_SQWE ? "yes" : "no");
	size += sprintk(buffer + size, "alarm_IRQ\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_AIE ? "yes" : "no");
	size += sprintk(buffer + size, "update_IRQ\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_UIE ? "yes" : "no");
	size += sprintk(buffer + size, "periodic_IRQ\t: %s\n", cmos_read(CMOS_STATB) & CMOS_STATB_PIE ? "yes" : "no");
	size += sprintk(buffer + size, "periodic_freq\t: %s\n", (cmos_read(CMOS_STATA) & CMOS_STATA_IRQF) == 0x6 ? "1024" : "?");
	size += sprintk(buffer + size, "batt_status\t: %s\n", cmos_read(CMOS_STATD) & CMOS_STATD_VRT ? "okay" : "dead");
	return size;
}

int data_proc_stat(char *buffer, __pid_t pid)
{
	int n, size;
	unsigned int idle;
	struct interrupt *irq;

	idle = kstat.ticks - (kstat.cpu_user + kstat.cpu_nice + kstat.cpu_system);
	size = 0;
	size += sprintk(buffer + size, "cpu %d %d %d %d\n", kstat.cpu_user, kstat.cpu_nice, kstat.cpu_system, idle);
	size += sprintk(buffer + size, "disk 0 0 0 0\n");
	size += sprintk(buffer + size, "page 0 0\n");
	size += sprintk(buffer + size, "swap 0 0\n");
	size += sprintk(buffer + size, "intr %u", kstat.irqs);
	for(n = 0; n < NR_IRQS; n++) {
		irq = irq_table[n];
		if(irq) {
			size += sprintk(buffer + size, " %u", irq->ticks);
		}
	}
	size += sprintk(buffer + size, "\n");
	size += sprintk(buffer + size, "ctxt %u\n", kstat.ctxt);
	size += sprintk(buffer + size, "btime %d\n", kstat.boot_time);
	size += sprintk(buffer + size, "processes %d\n", kstat.processes);
	return size;
}

int data_proc_uptime(char *buffer, __pid_t pid)
{
	struct proc *p;
	unsigned long int idle;

	p = &proc_table[IDLE];
	idle = tv2ticks(&p->usage.ru_utime);
	idle += tv2ticks(&p->usage.ru_stime);
	return sprintk(buffer, "%u.%02u %u.%02u\n", kstat.uptime, kstat.ticks % HZ, idle / HZ, idle % HZ);
}

int data_proc_fullversion(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "Fiwix version %s %s\n", UTS_RELEASE, UTS_VERSION);
}


int data_proc_buffermax(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%d\n", kstat.max_buffers);
}

int data_proc_buffernr(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%d\n", kstat.nr_buffers);
}

int data_proc_domainname(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s\n", sys_utsname.domainname);
}

int data_proc_filemax(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%d\n", NR_OPENS);
}

int data_proc_filenr(char *buffer, __pid_t pid)
{
	int n, nr;

	nr = 0;
	for(n = 1; n < NR_OPENS; n++) {
		if(fd_table[n].count != 0) {
			nr++;
		}
	}
	return sprintk(buffer, "%d\n", nr);
}

int data_proc_hostname(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s\n", sys_utsname.nodename);
}

int data_proc_inodemax(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%d\n", kstat.max_inodes);
}

int data_proc_inodenr(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%d\n", kstat.nr_inodes);
}

int data_proc_osrelease(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s\n", UTS_RELEASE);
}

int data_proc_ostype(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s\n", UTS_SYSNAME);
}

int data_proc_version(char *buffer, __pid_t pid)
{
	return sprintk(buffer, "%s\n", UTS_VERSION);
}


/*
 * PID directory related functions
 * -------------------------------
 */
int data_proc_pid_cmdline(char *buffer, __pid_t pid)
{
	int n, size;
	char *arg;
	char **argv;
	unsigned int addr, offset;
	struct proc *p;

	size = 0;
	if((p = get_proc_by_pid(pid))) {
		for(n = 0; n < p->argc && (p->argv + n); n++) {
			argv = p->argv + n;
			offset = (int)argv & ~PAGE_MASK;
			addr = get_mapped_addr(p, (int)argv) & PAGE_MASK;
			addr = P2V(addr);
			argv = (char **)(addr + offset);
			offset = (int)argv[0] & ~PAGE_MASK;
			addr = get_mapped_addr(p, (int)argv[0]) & PAGE_MASK;
			addr = P2V(addr);
			arg = (char *)(addr + offset);
			if(size + strlen(arg) < (PAGE_SIZE - 1)) {
				size += sprintk(buffer + size, "%s", arg);
				buffer[size++] = 0;
			} else {
				break;
			}
		}
	}
	return size;
}

int data_proc_pid_cwd(char *buffer, __pid_t pid)
{
	int size;
	struct proc *p;
	struct inode *i;

	size = 0;
	if((p = get_proc_by_pid(pid))) {

		/* zombie processes don't have current working directory */
		if(!p->pwd) {
			return -ENOENT;
		}

		i = p->pwd;
		size = sprintk(buffer, "[%02d%02d]:%d", MAJOR(i->rdev), MINOR(i->rdev), i->inode);
	}
	return size;
}

int data_proc_pid_environ(char *buffer, __pid_t pid)
{
	int n, size;
	char *env;
	char **envp;
	unsigned int addr, offset;
	struct proc *p;

	size = 0;
	if((p = get_proc_by_pid(pid))) {
		for(n = 0; n < p->envc && (p->envp + n); n++) {
			envp = p->envp + n;
			offset = (int)envp & ~PAGE_MASK;
			addr = get_mapped_addr(p, (int)envp) & PAGE_MASK;
			addr = P2V(addr);
			envp = (char **)(addr + offset);
			offset = (int)envp[0] & ~PAGE_MASK;
			addr = get_mapped_addr(p, (int)envp[0]) & PAGE_MASK;
			addr = P2V(addr);
			env = (char *)(addr + offset);
			if(size + strlen(env) < (PAGE_SIZE - 1)) {
				size += sprintk(buffer + size, "%s", env);
				buffer[size++] = 0;
			} else {
				break;
			}
		}
	}
	return size;
}

int data_proc_pid_exe(char *buffer, __pid_t pid)
{
	int size;
	struct proc *p;
	struct inode *i;

	size = 0;
	if((p = get_proc_by_pid(pid))) {

		/* kernel and zombie processes are programless */

		/*
		 * This assumes that the first entry in the vma_table
		 * contains the program's inode.
		 */
		if(!p->vma_table || !p->vma_table->inode) {
			return -ENOENT;
		}

		i = p->vma_table->inode;
		size = sprintk(buffer, "[%02d%02d]:%d", MAJOR(i->rdev), MINOR(i->rdev), i->inode);
	}
	return size;
}

int data_proc_pid_maps(char *buffer, __pid_t pid)
{
	int size, len;
	__ino_t inode;
	int major, minor;
	char *section;
	char r, w, x, f;
	struct proc *p;
	struct vma *vma;

	size = 0;
	if((p = get_proc_by_pid(pid))) {
		vma = p->vma_table;
		while(vma) {
			r = vma->prot & PROT_READ ? 'r' : '-';
			w = vma->prot & PROT_WRITE ? 'w' : '-';
			x = vma->prot & PROT_EXEC ? 'x' : '-';
			if(vma->flags & MAP_SHARED) {
				f = 's';
			} else if(vma->flags & MAP_PRIVATE) {
				f = 'p';
			} else {
				f = '-';
			}
			switch(vma->s_type) {
				case P_TEXT:	section = "text";
						break;
				case P_DATA:	section = "data";
						break;
				case P_BSS:	section = "bss";
						break;
				case P_HEAP:	section = "heap";
						break;
				case P_STACK:	section = "stack";
						break;
				case P_MMAP:	section = "mmap";
						break;
				case P_SHM:	section = "shm";
						break;
				default:
					section = NULL;
					break;
			}
			inode = major = minor = 0;
			if(vma->inode) {
				inode = vma->inode->inode;
				major = MAJOR(vma->inode->dev);
				minor = MINOR(vma->inode->dev);
			}
			len = sprintk(buffer + size, "%08x-%08x %c%c%c%c %08x %02d:%02d %- 10u [%s]\n", vma->start, vma->end, r, w, x, f, vma->offset, major, minor, inode, section);
			size += len;
			vma = vma->next;
		}
	}
	return size;
}

int data_proc_pid_mountinfo(char *buffer, __pid_t pid)
{
	int n, size;
	char *flag, *devname;
	struct mount *mp;

	size = n = 0;
	mp = mount_table;

	while(mp) {
		if(mp->fs->fsop->flags != FSOP_KERN_MOUNT) {
			flag = mp->sb.flags & MS_RDONLY ? "ro" : "rw";
			devname = mp->devname;
			if(!strcmp(devname, "/dev/root")) {
				devname = _rootdevname;
			}
			size += sprintk(buffer + size, "%d 0 %d:%d %s %s %s - %s %s %s\n", n, MAJOR(mp->dev), MINOR(mp->dev), "/", mp->dirname, flag, mp->fs->name, devname, flag);
		}
		n++;
		mp = mp->next;
	}
	return size;
}

int data_proc_pid_root(char *buffer, __pid_t pid)
{
	int size;
	struct proc *p;
	struct inode *i;

	size = 0;
	if((p = get_proc_by_pid(pid))) {

		/* zombie processes don't have root directory */
		if(!p->root) {
			return -ENOENT;
		}

		i = p->root;
		size = sprintk(buffer, "[%02d%02d]:%d", MAJOR(i->rdev), MINOR(i->rdev), i->inode);
	}
	return size;
}

int data_proc_pid_stat(char *buffer, __pid_t pid)
{
	int size, vma_start, vma_end;
	unsigned int esp, eip;
	int signum, mask;
	__sigset_t sigignored, sigcaught;
	struct proc *p;
	struct sigcontext *sc;
	struct vma *vma;
	int text, data, stack, mmap;

	size = text = data = stack = mmap = 0;
	if((p = get_proc_by_pid(pid))) {
		vma = p->vma_table;
		if(!vma) {
			return 0;
		}

		/*
		 * This assumes that the first entry in the vma_table
		 * contains the program's inode.
		 */
		vma_start = vma->start;
		vma_end = vma->end;

		while(vma) {
			switch(vma->s_type) {
				case P_TEXT:
					text += vma->end - vma->start;
					break;
				case P_HEAP:
					data += vma->end - vma->start;
					break;
				case P_STACK:
					stack += vma->end - vma->start;
					break;
				case P_MMAP:
				case P_SHM:
					mmap += vma->end - vma->start;
					break;
			}
			vma = vma->next;
		}

		sigignored = sigcaught = 0;
		for(signum = 0, mask = 1; signum < NSIG; signum++, mask <<= 1) {
			if(p->sigaction[signum].sa_handler == SIG_IGN) {
				sigignored |= mask;
			}
			if(p->sigaction[signum].sa_handler == SIG_DFL) {
				sigcaught |= mask;
			}
		}

		esp = eip = 0;
		if(p->sp) {
			sc = (struct sigcontext *)p->sp;
			esp = sc->oldesp;
			eip = sc->eip;
		}
		size = sprintk(buffer, "%d (%s) %c %d %d %d %d %d %d %d %d %d %d %u %u %u %u %d %d %d %d %d %d %u %u %u %u %u %u %u %d %d %u %u %u\n",
			p->pid,
			p->argv0,
			pstate[p->state][0],
			p->ppid, p->pgid, p->sid,
			p->ctty ? p->ctty->dev : 0,
			p->ctty ? p->ctty->pgid : - 1,
			0,			/* flags */
			0, 0, 0, 0,		/* minflt, cminflt, majflt, cmajflt */
			tv2ticks(&p->usage.ru_utime),
			tv2ticks(&p->usage.ru_stime),
			tv2ticks(&p->cusage.ru_utime),
			tv2ticks(&p->cusage.ru_stime),
			0,			/* counter */
			0,			/* priority */
			0,			/* timeout */
			0,			/* itrealvalue */
			p->start_time,
			text + data + stack + mmap,
			p->rss,
			0x7FFFFFFF,		/* rlim */
			vma_start,		/* startcode */
			vma_end,		/* endcode */
			PAGE_OFFSET - 1,	/* startstack */
			esp,			/* kstkesp */
			eip,			/* kstkeip */
			p->sigpending,
			p->sigblocked,
			sigignored,
			sigcaught,
			p->sleep_address
		);
	}
	return size;
}

int data_proc_pid_statm(char *buffer, __pid_t pid)
{
	int size;
	struct proc *p;
	struct vma *vma;
	int text, data, stack, mmap;

	size = text = data = stack = mmap = 0;
	if((p = get_proc_by_pid(pid))) {
		vma = p->vma_table;
		if(!vma) {
			return 0;
		}
		while(vma) {
			switch(vma->s_type) {
				case P_TEXT:
					text += vma->end - vma->start;
					break;
				case P_HEAP:
					data += vma->end - vma->start;
					break;
				case P_STACK:
					stack += vma->end - vma->start;
					break;
				case P_MMAP:
				case P_SHM:
					mmap += vma->end - vma->start;
					break;
			}
			vma = vma->next;
		}

		size = sprintk(buffer, "%d", (text + data + stack + mmap) / PAGE_SIZE);
		size += sprintk(buffer + size, " %d", p->rss);
		size += sprintk(buffer + size, " 0");	/* shared mappings */
		size += sprintk(buffer + size, " %d", text / PAGE_SIZE);
		size += sprintk(buffer + size, " 0");
		size += sprintk(buffer + size, " %d", (data + stack) / PAGE_SIZE);
		size += sprintk(buffer + size, " 0\n");
	}
	return size;
}

int data_proc_pid_status(char *buffer, __pid_t pid)
{
	int size;
	int signum, mask;
	__sigset_t sigignored, sigcaught;
	struct proc *p;
	struct vma *vma;
	int text, data, stack, mmap;

	size = text = data = stack = mmap = 0;
	if((p = get_proc_by_pid(pid))) {
		vma = p->vma_table;
		if(!vma) {
			return 0;
		}
		while(vma) {
			switch(vma->s_type) {
				case P_TEXT:
					text += vma->end - vma->start;
					break;
				case P_HEAP:
					data += vma->end - vma->start;
					break;
				case P_STACK:
					stack += vma->end - vma->start;
					break;
				case P_MMAP:
				case P_SHM:
					mmap += vma->end - vma->start;
					break;
			}
			vma = vma->next;
		}

		size = sprintk(buffer, "Name:\t%s\n", p->argv0);
		size += sprintk(buffer + size, "State:\t%s\n", pstate[p->state]);
		size += sprintk(buffer + size, "Pid:\t%d\n", p->pid);
		size += sprintk(buffer + size, "PPid:\t%d\n", p->ppid);
		size += sprintk(buffer + size, "Uid:\t%d\t%d\t%d\t-\n", p->uid, p->euid, p->suid);
		size += sprintk(buffer + size, "Gid:\t%d\t%d\t%d\t-\n", p->gid, p->egid, p->sgid);
		size += sprintk(buffer + size, "VmSize:\t%8d kB\n", (text + data + stack + mmap) / 1024);
		size += sprintk(buffer + size, "VmLck:\t%8d kB\n", 0);
		size += sprintk(buffer + size, "VmRSS:\t%8d kB\n", p->rss << 2);
		size += sprintk(buffer + size, "VmData:\t%8d kB\n", data / 1024);
		size += sprintk(buffer + size, "VmStk:\t%8d kB\n", stack / 1024);
		size += sprintk(buffer + size, "VmExe:\t%8d kB\n", text / 1024);
		size += sprintk(buffer + size, "VmLib:\t%8d kB\n", 0);
		size += sprintk(buffer + size, "SigPnd:\t%08x\n", p->sigpending);
		size += sprintk(buffer + size, "SigBlk:\t%08x\n", p->sigblocked);
		sigignored = sigcaught = 0;
		for(signum = 0, mask = 1; signum < NSIG; signum++, mask <<= 1) {
			if(p->sigaction[signum].sa_handler == SIG_IGN) {
				sigignored |= mask;
			}
			if(p->sigaction[signum].sa_handler == SIG_DFL) {
				sigcaught |= mask;
			}
		}
		size += sprintk(buffer + size, "SigIgn:\t%08x\n", sigignored);
		size += sprintk(buffer + size, "SigCgt:\t%08x\n", sigcaught);
	}
	return size;
}
