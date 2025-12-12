/*
 * fiwix/kernel/multiboot.c
 *
 * Copyright 2021-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/kernel.h>
#include <fiwix/multiboot1.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/limits.h>
#include <fiwix/kparms.h>
#include <fiwix/i386elf.h>
#include <fiwix/ramdisk.h>
#include <fiwix/kexec.h>
#include <fiwix/mm.h>
#include <fiwix/bios.h>
#include <fiwix/vgacon.h>
#include <fiwix/fb.h>
#include <fiwix/fbcon.h>
#include <fiwix/sysconsole.h>

static struct kernel_params_value kparamval_table[] = {
#ifdef CONFIG_BGA
	{ "bga=",
	   { "640x480x32", "800x600x32", "1024x768x32" },
	   { 0 }
	},
#endif /* CONFIG_BGA */
	{ "console=",
	   { "/dev/tty0", "/dev/tty1", "/dev/tty2", "/dev/tty3", "/dev/tty4",
	     "/dev/tty5", "/dev/tty6", "/dev/tty7", "/dev/tty8", "/dev/tty9",
	     "/dev/tty10", "/dev/tty11", "/dev/tty12",
	     "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3"
	   },
	   { 0x400, 0x401, 0x402, 0x403, 0x404,
	     0x405, 0x406, 0x407, 0x408, 0x409,
	     0x40A, 0x40B, 0x40C,
	     0x440, 0x441, 0x442, 0x443
	   }
	},
	{ "initrd=",
	   { 0 },
	   { 0 }
	},
#ifdef CONFIG_KEXEC
	{ "kexec_proto=",
	   { "multiboot1", "linux" },
	   { 1, 2 }
	},
	{ "kexec_size=",
	   { 0 },
	   { 0 }
	},
	{ "kexec_cmdline=",
	   { 0 },
	   { 0 }
	},
#endif /* CONFIG_KEXEC */
	{ "ps2_noreset",
	   { 0 },
	   { 0 }
	},
	{ "ramdisksize=",
	   { 0 },
	   { 0 }
	},
	{ "ro",
	   { 0 },
	   { 0 }
	},
	{ "root=",
	   { "/dev/ram0", "/dev/fd0", "/dev/fd1",
	     "/dev/hda", "/dev/hda1", "/dev/hda2", "/dev/hda3", "/dev/hda4",
	     "/dev/hdb", "/dev/hdb1", "/dev/hdb2", "/dev/hdb3", "/dev/hdb4",
	     "/dev/hdc", "/dev/hdc1", "/dev/hdc2", "/dev/hdc3", "/dev/hdc4",
	     "/dev/hdd", "/dev/hdd1", "/dev/hdd2", "/dev/hdd3", "/dev/hdd4",
	   },
	   { 0x100, 0x200, 0x201,
	     0x300, 0x301, 0x302, 0x303, 0x304,
	     0x340, 0x341, 0x342, 0x343, 0x344,
	     0x1600, 0x1601, 0x1602, 0x1603, 0x1604,
	     0x1640, 0x1641, 0x1642, 0x1643, 0x1644,
	   }
	},
	{ "rootfstype=",
	   { "minix", "ext2", "iso9660" },
	   { 0 }
	},

	{ NULL }
};

char kernel_cmdline[NAME_MAX + 1];
Elf32_Shdr *symtab, *strtab;

/* check the validity of a command line parameter */
static int check_param(struct kernel_params_value *kpv, const char *value)
{
	int n;

	if(!strcmp(kpv->name, "ps2_noreset")) {
		kparms.flags |= KPARMS_PS2_NORESET;
		return 0;
	}
#ifdef CONFIG_PCI
#ifdef CONFIG_BGA
	if(!strcmp(kpv->name, "bga=")) {
		for(n = 0; kpv->value[n]; n++) {
			if(!strcmp(kpv->value[n], value)) {
				strncpy(kparms.bgaresolution, value, 14);
				kparms.bgaresolution[14] = '\0';
				return 0;
			}
		}
		return 1;
	}
#endif /* CONFIG_BGA */
#endif /* CONFIG_PCI */
	if(!strcmp(kpv->name, "console=")) {
		for(n = 0; kpv->value[n]; n++) {
			if(!strcmp(kpv->value[n], value)) {
				if(kpv->sysval[n]) {
					if(add_sysconsoledev(kpv->sysval[n])) {
						kparms.syscondev = kpv->sysval[n];
					} else {
						printk("WARNING: console device '%s' exceeds NR_SYSCONSOLES.\n", value);
					}
					return 0;
				}
				printk("WARNING: device name for '%s' is not defined!\n", kpv->name);
			}
		}
		return 1;
	}
	if(!strcmp(kpv->name, "initrd=")) {
		if(value[0]) {
			strncpy(kparms.initrd, value, DEVNAME_MAX);
			return 0;
		}
	}
#ifdef CONFIG_KEXEC
	if(!strcmp(kpv->name, "kexec_proto=")) {
		for(n = 0; kpv->value[n]; n++) {
			if(!strcmp(kpv->value[n], value)) {
				if(kpv->sysval[n]) {
					kexec_proto = kpv->sysval[n];
					return 0;
				}
				printk("WARNING: kexec protocol '%s' is not defined!\n", kpv->name);
			}
		}
		return 1;
	}
	if(!strcmp(kpv->name, "kexec_size=")) {
		if(value[0]) {
			kexec_size = atoi(value);
			return 0;
		}
		return 1;
	}
	if(!strcmp(kpv->name, "kexec_cmdline=")) {
		if(value[0]) {
			/* copy the provided cmdline and also remove quotes */
			strncpy(kexec_cmdline, value + 1, MIN(strlen(value) - 2, NAME_MAX));
			return 0;
		}
		return 1;
	}
#endif /* CONFIG_KEXEC */
	if(!strcmp(kpv->name, "ramdisksize=")) {
		kparms.ramdisksize = atoi(value);
		ramdisk_minors = RAMDISK_DRIVES;
		return 0;
	}
	if(!strcmp(kpv->name, "ro")) {
		kparms.ro = 1;
		return 0;
	}
	if(!strcmp(kpv->name, "root=")) {
		for(n = 0; kpv->value[n]; n++) {
			if(!strcmp(kpv->value[n], value)) {
				kparms.rootdev = kpv->sysval[n];
				strncpy(kparms.rootdevname, value, DEVNAME_MAX);
				return 0;
			}
		}
		return 1;
	}
	if(!strcmp(kpv->name, "rootfstype=")) {
		for(n = 0; kpv->value[n]; n++) {
			if(!strcmp(kpv->value[n], value)) {
				strncpy(kparms.rootfstype, value, sizeof(kparms.rootfstype));
				return 0;
			}
		}
		return 1;
	}
	printk("WARNING: the parameter '%s' looks valid but it's not defined!\n", kpv->name);
	return 0;
}

static int parse_arg(const char *arg)
{
	int n;
	const char *value;

	/* '--' marks the beginning of the init arguments */
	if(!strcmp(arg, "--")) {
		return 1;
	}

	/* find out where the value starts (if it exists) */
	value = arg;
	while(*value && *value != '=') {
		value++;
	}
	if(*value) {
		value++;
	}

	for(n = 0; kparamval_table[n].name; n++) {
		if(!strncmp(arg, kparamval_table[n].name, value ? value - arg : strlen(arg))) {
			if(check_param(&kparamval_table[n], value)) {
				printk("WARNING: invalid value '%s' in the '%s' parameter.\n", value, kparamval_table[n].name);
			}
			return 0;
		}
	}
	printk("WARNING: invalid cmdline parameter: '%s'.\n", arg);
	return 0;
}

static char *parse_cmdline(const char *str)
{
	char *from, *to;
	char arg[CMDL_ARG_LEN];
	char c;
	int open, close, incomplete;

	from = to = (char *)str;
	open = close = 0;
	for(;;) {
		c = *(str++);
		if(c == '"') {
			if(open) {
				close = 1;
			}
			open = 1;
		}
		incomplete = open - close;

		if((c == ' ' || !c) && !incomplete) {
			if(to - from < CMDL_ARG_LEN) {
				memcpy_b(arg, from, to - from);
				arg[to - from] = 0;
				if(arg[0] != 0) {
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
				arg[CMDL_ARG_LEN - 1] = 0;
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

#ifdef CONFIG_BGA
static void parse_bgaresolution(void)
{
	char str[5], *p;
	int n;

	n = 0;
	for(;;) {
		p = &str[0];
		while(kparms.bgaresolution[n] && kparms.bgaresolution[n] != 'x') {
			*p = kparms.bgaresolution[n];
			p++;
			n++;
		}
		*p = '\0';
		if(!video.fb_width) {
			video.fb_width = atoi(str);
		} else if(!video.fb_height) {
			video.fb_height = atoi(str);
		} else if(!video.fb_bpp) {
			video.fb_bpp = atoi(str);
		} else {
			break;
		}
		n++;
	}
}
#endif /* CONFIG_BGA */

/*
 * This function returns the last address used by kernel symbols or the value
 * of 'mod_end' (in the module structure) of the last module loaded by GRUB.
 *
 * In the case where there are no ELF header tables, then it returns the last
 * .bss address plus one page.
 *
 * This is intended to place the kernel stack beyond all these addresses.
 */
unsigned int get_last_boot_addr(unsigned int magic, unsigned int info)
{
	struct multiboot_info *mbi;
	Elf32_Shdr *shdr;
	struct multiboot_elf_section_header_table *hdr;
	struct multiboot_mod_list *mod;
	unsigned short int n;
	unsigned int addr;

	if(magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		addr = ((unsigned int)_end & PAGE_MASK) + PAGE_SIZE;
		return P2V(addr);
	}

	mbi = (struct multiboot_info *)info;

	/* ELF header tables */
	if(mbi->flags & MULTIBOOT_INFO_ELF_SHDR) {
		symtab = strtab = NULL;
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
	} else {
		addr = ((unsigned int)_end & PAGE_MASK) + PAGE_SIZE;
	}

	/*
	 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
	 *
	 * Check if GRUB has loaded some modules and, if so, get the last
	 * address used by the last one.
	 */
	if(mbi->flags & MULTIBOOT_INFO_MODS) {
		mod = (struct multiboot_mod_list *)mbi->mods_addr;
		for(n = 0; n < mbi->mods_count; n++, mod++) {
			addr = mod->mod_end;
		}
	}

	return P2V(addr);
}

void multiboot(unsigned int magic, unsigned int info)
{
	struct multiboot_info mbi;

	memset_b(&video, 0, sizeof(struct video_parms));
	memset_b(&ramdisk_table, 0, sizeof(ramdisk_table));
	ramdisk_minors = 0;

	if(magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printk("WARNING: invalid multiboot magic number: 0x%x. Assuming 4MB of RAM.\n", magic);
		memset_b(&mbi, 0, sizeof(struct multiboot_info));
		kparms.memsize = 640;
		kparms.extmemsize = 3072;
		bios_map_init(NULL, 0);
		video.columns = 80;
		video.lines = 25;
		video.flags = VPF_VGA;
		video.memsize = 384 * 1024;
		return;
	}

	memcpy_b(&mbi, (void *)info, sizeof(struct multiboot_info));

	if(mbi.flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		printk("bootloader                  -\t%s\n", mbi.boot_loader_name);
	}

	if(!(mbi.flags & MULTIBOOT_INFO_MEMORY)) {
		printk("WARNING: values in mem_lower and mem_upper are not valid!\n");
	}
	kparms.memsize = (unsigned int)mbi.mem_lower;
	kparms.extmemsize = (unsigned int)mbi.mem_upper;


	if(mbi.flags & MULTIBOOT_INFO_CMDLINE) {
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
		strcpy(kernel_cmdline, p);
		init_args = parse_cmdline(kernel_cmdline);
	} else {
		printk("WARNING: no cmdline detected!\n");
	}
	printk("kernel    0x%08x        -\tcmdline='%s'\n", KERNEL_ADDR, kernel_cmdline);


	if(mbi.flags & MULTIBOOT_INFO_MODS) {
		unsigned int n, size;
		struct multiboot_mod_list *mod;

		mod = (struct multiboot_mod_list *)mbi.mods_addr;
		for(n = 0; n < mbi.mods_count; n++, mod++) {
			if(!strcmp((char *)mod->cmdline, kparms.initrd)) {
				printk("initrd    0x%08x-0x%08x file='%s' size=%dKB\n", mod->mod_start, mod->mod_end, mod->cmdline, (mod->mod_end - mod->mod_start) / 1024);
				size = mod->mod_end - mod->mod_start;
				ramdisk_table[0].addr = (char *)mod->mod_start;
				ramdisk_table[0].size = size / 1024;
				ramdisk_minors++;
			}
		}
	}


	if(!(mbi.flags & MULTIBOOT_INFO_ELF_SHDR)) {
		printk("WARNING: ELF section header table is not valid!\n");
	}

	if(mbi.flags & MULTIBOOT_INFO_MEM_MAP) {
		bios_map_init((struct multiboot_mmap_entry *)mbi.mmap_addr, mbi.mmap_length);
	} else {
		bios_map_init(NULL, 0);
	}

	if(mbi.flags & MULTIBOOT_INFO_VBE_INFO) {
		struct vbe_controller *vbec;
		struct vbe_mode *vbem;

		vbec = (struct vbe_controller *)mbi.vbe_control_info;
		vbem = (struct vbe_mode *)mbi.vbe_mode_info;

		video.flags = VPF_VESAFB;
		video.address = (unsigned int *)vbem->phys_base;
		video.port = 0;
		video.memsize = vbec->total_memory * vbem->win_size * 1024;
		strcpy((char *)video.signature, (char *)vbec->signature);
		video.columns = vbem->x_resolution / vbem->x_char_size;
		video.lines = vbem->y_resolution / vbem->y_char_size;
		video.fb_version = vbec->version;
		video.fb_width = vbem->x_resolution;
		video.fb_height = vbem->y_resolution;
		video.fb_char_width = vbem->x_char_size;
		video.fb_char_height = vbem->y_char_size;
		video.fb_bpp = vbem->bits_per_pixel;
		video.fb_pixelwidth = vbem->bits_per_pixel / 8;
		video.fb_pitch = vbem->bytes_per_scanline;
		video.fb_linesize = video.fb_pitch * video.fb_char_height;
		video.fb_size = vbem->x_resolution * vbem->y_resolution * video.fb_pixelwidth;
		video.fb_vsize = video.lines * video.fb_pitch * video.fb_char_height;
	}

#ifdef CONFIG_BGA
	if(*kparms.bgaresolution) {
		video.flags = VPF_VESAFB;
		parse_bgaresolution();
		video.fb_char_width = 8;
		video.fb_char_height = 16;
		video.columns = video.fb_width / video.fb_char_width;
		video.lines = video.fb_height / video.fb_char_height;
	}
#endif /* CONFIG_BGA */

	if(!video.flags) {
		/* fallback to standard VGA */
		video.flags = VPF_VGA;
		video.columns = 80;
		video.lines = 25;
		video.memsize = 384 * 1024;
	}
}
