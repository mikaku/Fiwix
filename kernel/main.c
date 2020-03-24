/*
 * fiwix/kernel/main.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/system.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/cpu.h>
#include <fiwix/pic.h>
#include <fiwix/fs.h>
#include <fiwix/devices.h>
#include <fiwix/console.h>
#include <fiwix/keyboard.h>
#include <fiwix/ramdisk.h>
#include <fiwix/version.h>
#include <fiwix/limits.h>
#include <fiwix/segments.h>
#include <fiwix/syscalls.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/kparms.h>
#include <fiwix/i386elf.h>
#include <fiwix/bios.h>

/*
 * check if the bit BIT in Multiboot FLAGS is set
 * ----------------------------------------------
 * bit 11 -> vbe_*
 * bit 10 -> apm_table
 * bit  9 -> boot_loader_name
 * bit  8 -> config_table
 * bit  7 -> drives_length and drives_addr
 * bit  6 -> mmap_length and mmap_addr
 * bit  5 -> ELF symbols
 * bit  4 -> a.out symbols
 * bit  3 -> mods_count and mods_addr
 * bit  2 -> cmdline
 * bit  1 -> boot_device
 * bit  0 -> mem_lower and mem_upper values
 */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))

Elf32_Shdr *symtab, *strtab;
unsigned int _last_data_addr;
int _memsize;
int _extmemsize;
int _rootdev;
int _noramdisk;
int _ramdisksize;
char _rootfstype[10];
char _rootdevname[DEVNAME_MAX + 1];
char _initrd[DEVNAME_MAX + 1];
int _syscondev;
char *init_args;

char cmdline[NAME_MAX + 1];

struct new_utsname sys_utsname = {
	UTS_SYSNAME,
	UTS_NODENAME,
	UTS_RELEASE,
	UTS_VERSION,
	"",
	UTS_DOMAINNAME,
};

struct kernel_stat kstat;

/*
 * This function returns the last address used by kernel symbols or the value
 * of 'mod_end' (in the module structure) of the last module loaded by GRUB.
 *
 * This is intended to setup the kernel stack beyond all these addresses.
 */
unsigned int get_last_boot_addr(unsigned int info)
{
	multiboot_info_t *mbi;
	Elf32_Shdr *shdr;
	elf_section_header_table_t *hdr;
	struct module *mod;
	unsigned short int n;
	unsigned int addr;

	symtab = strtab = NULL;
	mbi = (multiboot_info_t *)info;
	hdr = &(mbi->u.elf_sec);
	for(n = 0; n < hdr->num; n++) {
		shdr = (Elf32_Shdr *)(hdr->addr + (n * hdr->size));
		if(shdr->sh_type == SHT_SYMTAB) {
			symtab = shdr;
		}
		if(shdr->sh_type == SHT_STRTAB) {
			strtab = shdr;
		}
	}

	addr = strtab->sh_addr + strtab->sh_size;

	/*
	 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
	 *
	 * Check if GRUB has loaded some modules and, if so, get the last
	 * address used by the last one.
	 */
	if(CHECK_FLAG(mbi->flags, 3)) {
		mod = (struct module *)mbi->mods_addr;
		for(n = 0; n < mbi->mods_count; n++, mod++) {
			addr = mod->mod_end;
		}
	}

	return P2V(addr);
}

/* check the validity of a command line parameter */
static int check_parm(struct kparms *parm, const char *value)
{
	int n;

	if(!strcmp(parm->name, "root=")) {
		for(n = 0; parm->value[n]; n++) {
			if(!strcmp(parm->value[n], value)) {
				_rootdev = parm->sysval[n];
				strncpy(_rootdevname, value, DEVNAME_MAX);
				return 0;
			}
		}
		return 1;
	}
	if(!strcmp(parm->name, "noramdisk")) {
		_noramdisk = 1;
		return 0;
	}
	if(!strcmp(parm->name, "ramdisksize=")) {
		int size = atoi(value);
		if(!size || size > RAMDISK_MAXSIZE) {
			printk("WARNING: 'ramdisksize' value is out of limits, defaulting to 4096KB.\n");
			_ramdisksize = 0;
		} else {
			_ramdisksize = size;
		}
		return 0;
	}
	if(!strcmp(parm->name, "initrd=")) {
		if(value[0]) {
			strncpy(_initrd, value, DEVNAME_MAX);
			return 0;
		}
	}
	if(!strcmp(parm->name, "rootfstype=")) {
		for(n = 0; parm->value[n]; n++) {
			if(!strcmp(parm->value[n], value)) {
				strncpy(_rootfstype, value, sizeof(_rootfstype));
				return 0;
			}
		}
		return 1;
	}
	if(!strcmp(parm->name, "console=")) {
		for(n = 0; parm->value[n]; n++) {
			if(!strcmp(parm->value[n], value)) {
				_syscondev = parm->sysval[n];
				return 0;
			}
		}
		return 1;
	}
	printk("WARNING: the parameter '%s' looks valid but it's not defined!\n", parm->name);
	return 0;
}

static int parse_arg(const char *arg)
{
	int n;

	/* '--' marks the beginning of the init arguments */
	if(!strcmp(arg, "--")) {
		return 1;
	}

	for(n = 0; parm_table[n].name; n++) {
		if(!strncmp(arg, parm_table[n].name, strlen(parm_table[n].name))) {
			arg += strlen(parm_table[n].name);
			if(check_parm(&parm_table[n], arg)) {
				printk("WARNING: invalid value '%s' in the '%s' parameter.\n", arg, parm_table[n].name);
			}
			return 0;
		}
	}
	printk("WARNING: invalid cmdline parameter: '%s'.\n", arg);
	return 0;
}

static char * parse_cmdline(const char *str)
{
	char *from, *to;
	char arg[CMDL_ARG_LEN];
	char c;

	from = to = (char *)str;
	for(;;) {
		c = *(str++);
		if(c == ' ' || !c) {
			if(to - from < CMDL_ARG_LEN) {
				memcpy_b(arg, from, to - from);
				arg[to - from] = NULL;
				if(arg[0] != NULL) {
					if(parse_arg(arg)) {
						while(*(from++)) {
							if(*from != '-' && *from != ' ') {
								break;
							}
						}
						return from;
					}
				}
			} else {
				memcpy_b(arg, from, CMDL_ARG_LEN);
				arg[CMDL_ARG_LEN - 1] = NULL;
				printk("WARNING: invalid length of the cmdline parameter '%s'.\n", arg);
			}
			from = ++to;
			if(!c) {
				break;
			}
			continue;
		}
		to++;
	}

	return NULL;
}

void start_kernel(unsigned long magic, unsigned long info, unsigned int stack)
{
	struct proc *init, *p_kswapd;
	multiboot_info_t mbi;

	/* default kernel values */
	strcpy(_rootfstype, "ext2");		/* filesystem is ext2 */
	_syscondev = MKDEV(VCONSOLES_MAJOR, 0); /* console is /dev/tty0 */

	pic_init();
	idt_init();
	dev_init();
	tty_init();

	printk("                                Welcome to %s\n", UTS_SYSNAME);
	printk("                     Copyright (c) 2018-2020, Jordi Sanfeliu\n");
	printk("\n");
	printk("                      kernel v%s for i386 architecture\n", UTS_RELEASE);
	printk("               (GCC %s, built on %s)\n", __VERSION__, UTS_VERSION);
	printk("\n");
	printk("DEVICE    ADDRESS         IRQ   COMMENT\n");
	printk("-------------------------------------------------------------------------------\n");

	if(magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printk("WARNING: invalid multiboot-bootloader magic number: 0x%x.\n\n", (unsigned long int)magic);
		memset_b(&mbi, NULL, sizeof(struct multiboot_info));
	} else {
		memcpy_b(&mbi, (void *)info, sizeof(struct multiboot_info));
	}

	memset_b(&kstat, NULL, sizeof(kstat));

	cpu_init();

	/* check if a command line was supplied */
	if(CHECK_FLAG(mbi.flags, 2)) {
		int n, len;
		char c;
		char *p;

		p = (char *)mbi.cmdline;
		len = strlen(p);
		/* suppress 'fiwix' */
		for(n = 0; n < len; n++) {
			c = *(p++);
			if(c == ' ') {
				break;
			}
		}
		strcpy(cmdline, p);
		init_args = parse_cmdline(cmdline);
	} else {
		printk("WARNING: no cmdline detected!\n");
	}

	printk("kernel    0x%08X       -    cmdline='%s'\n", KERNEL_ENTRY_ADDR, cmdline);

	timer_init();
	vconsole_init();
	keyboard_init();

	if(CHECK_FLAG(mbi.flags, 3)) {
		int n;
		struct module *mod;

		mod = (struct module *)mbi.mods_addr;
		for(n = 0; n < mbi.mods_count; n++, mod++) {
			if(!strcmp((char *)mod->string, _initrd)) {
				printk("initrd    0x%08X-0x%08X file='%s' size=%dKB\n", mod->mod_start, mod->mod_end, mod->string, (mod->mod_end - mod->mod_start) / 1024);
				ramdisk_table[0].addr = (char *)mod->mod_start;
			}
		}
	}

	if(!CHECK_FLAG(mbi.flags, 0)) {
		printk("WARNING: values in mem_lower and mem_upper are not valid!\n");
	}
	_memsize = (unsigned int)mbi.mem_lower;
	_extmemsize = (unsigned int)mbi.mem_upper;

	if(CHECK_FLAG(mbi.flags, 6)) {
		bios_map_init((memory_map_t *)mbi.mmap_addr, mbi.mmap_length);
	} else {
		bios_map_init(NULL, 0);
	}

	_last_data_addr = stack - KERNEL_BASE_ADDR;
	mem_init();
	proc_init();

	if(!(CHECK_FLAG(mbi.flags, 5))) {
		printk("WARNING: ELF section header table is not valid!\n");
	}

	/* IDLE is now the current process */
	set_tss(current);
	load_tr(TSS);
	current->tss.cr3 = (unsigned int)kpage_dir;
	current->flags |= PF_KPROC;

	/* reserve the slot 1 for the INIT process */
	init = get_proc_free();
	proc_slot_init(init);
	init->pid = get_unused_pid();

	/* create and setup kswapd process */
	p_kswapd = kernel_process(kswapd);

	/* kswapd will take over the rest of the kernel initialization */
	p_kswapd->state = PROC_RUNNING;
	need_resched = 1;

	STI();		/* let's rock! */
	cpu_idle();
}

void stop_kernel(void)
{
	struct proc *p;

	printk("\n");
	printk("**    Safe to Power Off    **\n");
	printk("            -or-\n");
	printk("** Press Any Key to Reboot **\n");
	any_key_to_reboot = 1;

	/* put all processes to sleep and reset all pending signals */
	FOR_EACH_PROCESS(p) {
		p->state = PROC_SLEEPING;
		p->sigpending = 0;
	}

	/* TODO: disable all interrupts */
	CLI();
	disable_irq(TIMER_IRQ);

	/* switch to IDLE process */
	if(current) {
		do_sched();
	}

	STI();
	enable_irq(KEYBOARD_IRQ);
	cpu_idle();
}
