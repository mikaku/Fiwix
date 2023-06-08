/*
 * fiwix/include/fiwix/kernel.h
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_KERNEL_H
#define _FIWIX_KERNEL_H

#include <fiwix/limits.h>
#include <fiwix/i386elf.h>

#define QEMU_DEBUG_PORT		0xE9	/* for Bochs-style debug console */
#define BUDDY_MAX_LEVEL		7

#define PANIC(format, args...)						\
{									\
	printk("\nPANIC: in %s()", __FUNCTION__);			\
	printk("\n");							\
	printk(format, ## args);					\
	kstat.flags |= KF_HAS_PANICKED;					\
	stop_kernel();							\
}

#define CURRENT_TIME	(kstat.system_time)
#define CURRENT_TICKS	(kstat.ticks)
#define INIT_PROGRAM	"/sbin/init"

/* Max memory for page tables:
 * 0x101000 for page dir and 1GB physical
 * 0x001000 for low memory page table
 * 0x01E000 for 120MB ram drive page tables
 */
#define MAX_PGTABLE_SIZE 0x120000

/* kernel flags */
#define KF_HAS_PANICKED		0x01	/* the kernel has panic'ed */
#define KF_HAS_DEBUGCON		0x02	/* QEMU debug console support */

extern char *init_argv[];
extern char *init_envp[];
extern char *init_args;

extern Elf32_Shdr *symtab, *strtab;
extern unsigned int _last_data_addr;

extern int kparm_memsize;
extern int kparm_extmemsize;
extern int kparm_rootdev;
extern int kparm_ramdisksize;
extern char kparm_rootfstype[10];
extern char kparm_rootdevname[DEVNAME_MAX + 1];
extern char kparm_initrd[DEVNAME_MAX + 1];
extern int kparm_syscondev;
extern char kparm_bgaresolution[15];
extern int kparm_ro;
extern int kexec_proto;
extern int kexec_size;
extern char kexec_cmdline[NAME_MAX + 1];

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

extern char kernel_cmdline[NAME_MAX + 1];

struct kernel_stat {
	int flags;			/* kernel flags */
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
	int syscondev;			/* system console device */
	int physical_pages;		/* physical memory (in pages) */
	int kernel_reserved;		/* kernel memory reserved (in KB) */
	int physical_reserved;		/* physical memory reserved (in KB) */
	int total_mem_pages;		/* total memory (in pages) */
	int free_pages;			/* pages on free list */
	int max_inodes;			/* max. number of allocated inodes */
	int nr_inodes;			/* current allocated inodes */
	int max_buffers;		/* max. number of allocated buffers */
	int nr_buffers;			/* current allocated buffers */
	int buffers;			/* memory used by buffers (in KB) */
	int cached;			/* memory used to cache file pages */
	int shared;			/* pages with count > 1 */
	int max_dirty_buffers;		/* max. number of dirty buffers */
	int dirty_buffers;		/* dirty buffers (in KB) */
	int nr_dirty_buffers;		/* current dirty buffers */
	unsigned int random_seed;	/* next random seed */
	int pages_reclaimed;		/* last pages reclaimed from buffer */
	int nr_flocks;			/* current allocated file locks */

	/* buddy_low algorithm statistics */
	int buddy_low_count[BUDDY_MAX_LEVEL + 1];
	int buddy_low_num_pages;	/* number of pages used */
	int buddy_low_mem_requested;	/* total memory requested (in bytes) */

	int mount_points;		/* number of fs currently mounted */
};
extern struct kernel_stat kstat;

unsigned int get_last_boot_addr(unsigned int, unsigned int);
void multiboot(unsigned int, unsigned int);
void start_kernel(unsigned int, unsigned int, unsigned int);
void stop_kernel(void);
void init_init(void);
void cpu_idle(void);

#endif /* _FIWIX_KERNEL_H */
