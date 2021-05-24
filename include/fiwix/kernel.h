/*
 * fiwix/include/fiwix/kernel.h
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_KERNEL_H
#define _FIWIX_KERNEL_H

#include <fiwix/limits.h>
#include <fiwix/i386elf.h>

#define PANIC(format, args...)						\
{									\
	printk("\nPANIC: in %s()", __FUNCTION__);			\
	printk("\n");							\
	printk(format, ## args);					\
	stop_kernel();							\
}

#define CURRENT_TIME	(kstat.system_time)
#define CURRENT_TICKS	(kstat.ticks)
#define INIT_PROGRAM	"/sbin/init"

extern char *init_argv[];
extern char *init_envp[];
extern char *init_args;

extern Elf32_Shdr *symtab, *strtab;
extern unsigned int _last_data_addr;

extern int _memsize;
extern int _extmemsize;
extern int _rootdev;
extern int _noramdisk;
extern int _ramdisksize;
extern char _rootfstype[10];
extern char _rootdevname[DEVNAME_MAX + 1];
extern char _initrd[DEVNAME_MAX + 1];
extern int _syscondev;

extern int _cputype;
extern int _cpusignature;
extern int _cpuflags;
extern int _brandid;
extern char _vendorid[12];
extern char _brandstr[48];
extern unsigned int _tlbinfo_eax;
extern unsigned int _tlbinfo_ebx;
extern unsigned int _tlbinfo_ecx;
extern unsigned int _tlbinfo_edx;
extern char _etext[], _edata[], _end[];

extern char cmdline[NAME_MAX + 1];

struct kernel_stat {
	unsigned int cpu_user;		/* ticks in user-mode */
	unsigned int cpu_nice;		/* ticks in user-mode (with priority) */
	unsigned int cpu_system;	/* ticks in kernel-mode */
	unsigned int irqs;		/* irq counter */
	unsigned int sirqs;		/* spurious irq counter */
	unsigned int ctxt;		/* context switches */
	unsigned int ticks;		/* ticks (1/HZths of sec) since boot */
	unsigned int system_time;	/* current system time (since the Epoch) */
	unsigned int boot_time;		/* boot time (since the Epoch) */
	int tz_minuteswest;		/* minutes west of GMT */
	int tz_dsttime;			/* type of DST correction */
	unsigned int uptime;		/* seconds since boot */
	unsigned int processes;		/* number of forks since boot */
	unsigned int physical_pages;	/* physical memory in pages */
	unsigned int kernel_reserved;	/* kernel memory reserved in KB */
	unsigned int physical_reserved;	/* physical memory reserved in KB */
	unsigned int total_mem_pages;	/* total memory in pages */
	unsigned int free_pages;	/* pages on free list (available) */
	unsigned int buffers;		/* memory used by buffers in KB */
	unsigned int cached;		/* memory used to cache file pages */
	unsigned int shared;		/* pages with count > 1 */
	unsigned long int random_seed;	/* next random seed */
};
extern struct kernel_stat kstat;

unsigned int get_last_boot_addr(unsigned int);
void multiboot(unsigned long, unsigned long);
void start_kernel(unsigned long, unsigned long, unsigned int);
void stop_kernel(void);
void init_init(void);
void cpu_idle(void);

#endif /* _FIWIX_KERNEL_H */
