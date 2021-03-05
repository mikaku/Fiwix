/*
 * fiwix/kernel/multiboot.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
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
#include <fiwix/mm.h>
#include <fiwix/bios.h>
#include <fiwix/fb.h>

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
				if(parm->sysval[n]) {
					_syscondev = parm->sysval[n];
					return 0;
				}
				printk("WARNING: device name for '%s' is not defined!\n", parm->name);
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

/*
 * This function returns the last address used by kernel symbols or the value
 * of 'mod_end' (in the module structure) of the last module loaded by GRUB.
 *
 * This is intended to setup the kernel stack beyond all these addresses.
 */
unsigned int get_last_boot_addr(unsigned int info)
{
	struct multiboot_info *mbi;
	Elf32_Shdr *shdr;
	struct multiboot_elf_section_header_table *hdr;
	struct multiboot_mod_list *mod;
	unsigned short int n;
	unsigned int addr;

	symtab = strtab = NULL;
	mbi = (struct multiboot_info *)info;
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
	if(mbi->flags & MULTIBOOT_INFO_MODS) {
		mod = (struct multiboot_mod_list *)mbi->mods_addr;
		for(n = 0; n < mbi->mods_count; n++, mod++) {
			addr = mod->mod_end;
		}
	}

	return P2V(addr);
}

void multiboot(unsigned long magic, unsigned long info)
{
	struct multiboot_info mbi;

	if(magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printk("WARNING: invalid multiboot magic number: 0x%x. Assuming 4MB of RAM.\n", (unsigned long int)magic);
		memset_b(&mbi, NULL, sizeof(struct multiboot_info));
		_memsize = 640;
		_extmemsize = 3072;
		bios_map_init(NULL, 0);
		return;
	}

	memcpy_b(&mbi, (void *)info, sizeof(struct multiboot_info));

	if(mbi.flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		printk("bootloader                 -    %s\n", mbi.boot_loader_name);
	}

	if(!(mbi.flags & MULTIBOOT_INFO_MEMORY)) {
		printk("WARNING: values in mem_lower and mem_upper are not valid!\n");
	}
	_memsize = (unsigned int)mbi.mem_lower;
	_extmemsize = (unsigned int)mbi.mem_upper;


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
		strcpy(cmdline, p);
		init_args = parse_cmdline(cmdline);
	} else {
		printk("WARNING: no cmdline detected!\n");
	}
	printk("kernel    0x%08X       -    cmdline='%s'\n", KERNEL_ENTRY_ADDR, cmdline);


	if(mbi.flags & MULTIBOOT_INFO_MODS) {
		int n;
		struct multiboot_mod_list *mod;

		mod = (struct multiboot_mod_list *)mbi.mods_addr;
		for(n = 0; n < mbi.mods_count; n++, mod++) {
			if(!strcmp((char *)mod->cmdline, _initrd)) {
				printk("initrd    0x%08X-0x%08X file='%s' size=%dKB\n", mod->mod_start, mod->mod_end, mod->cmdline, (mod->mod_end - mod->mod_start) / 1024);
				ramdisk_table[0].addr = (char *)mod->mod_start;
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
		unsigned long int from, to;

		vbec = (struct vbe_controller *)mbi.vbe_control_info;
		vbem = (struct vbe_mode *)mbi.vbe_mode_info;
		memset_b(&video, 0, sizeof(struct video_parms));

		/*
		printk("vbe_control_info  = 0x%08x\n", mbi.vbe_control_info);
		printk("\tvbe_controller.signature        = '%s'\n", vbec->signature);
		printk("\tvbe_controller.version          = %x\n", vbec->version);
		printk("\tvbe_controller.oem_string       = '%s'\n", vbec->oem_string);
		printk("\tvbe_controller.capabilities     = 0x%08x\n", vbec->capabilities);
		printk("\tvbe_controller.video_mode       = 0x%08x\n", vbec->video_mode);
		printk("\tvbe_controller.total_memory     = 0x%08x\n", vbec->total_memory);
		printk("\tvbe_controller.oem_software_rev = 0x%08x\n", vbec->oem_software_rev);
		printk("\tvbe_controller.oem_vendor_name  = '%s'\n", vbec->oem_vendor_name);
		printk("\tvbe_controller.oem_product_name = '%s'\n", vbec->oem_product_name);
		printk("\tvbe_controller.oem_product_rev  = '%s'\n", vbec->oem_product_rev);

		printk("vbe_mode_info     = 0x%08x\n", mbi.vbe_mode_info);
		printk("\tvbe_mode.mode_attributes    = 0b%08b\n", vbem->mode_attributes);
		printk("\tvbe_mode.win_size           = %d\n", vbem->win_size);
		printk("\tvbe_mode.win_a_segment      = 0x%04x\n", vbem->win_a_segment);
		printk("\tvbe_mode.win_b_segment      = 0x%04x\n", vbem->win_b_segment);
		printk("\tvbe_mode.bytes_per_scanline = %d\n", vbem->bytes_per_scanline);
		printk("\tvbe_mode.x_resolution       = %d\n", vbem->x_resolution);
		printk("\tvbe_mode.y_resolution       = %d\n", vbem->y_resolution);
		printk("\tvbe_mode.x_char_size        = %d\n", vbem->x_char_size);
		printk("\tvbe_mode.y_char_size        = %d\n", vbem->y_char_size);
		printk("\tvbe_mode.number_of_planes   = %d\n", vbem->number_of_planes);
		printk("\tvbe_mode.bits_per_pixel     = %d\n", vbem->bits_per_pixel);
		printk("\tvbe_mode.number_of_banks    = %d\n", vbem->number_of_banks);
		printk("\tvbe_mode.memory_model       = %d\n", vbem->memory_model);
		printk("\tvbe_mode.number_of_image_pa = %d\n", vbem->number_of_image_pages);
		printk("\tvbe_mode.phys_base          = 0x%08x\n", vbem->phys_base);

		printk("vbe_mode          = 0x%08x\n", mbi.vbe_mode);
		printk("vbe_interface_seg = 0x%08x\n", mbi.vbe_interface_seg);
		printk("vbe_interface_off = 0x%08x\n", mbi.vbe_interface_off);
		printk("vbe_interface_len = 0x%08x\n", mbi.vbe_interface_len);
		*/
		video.address = (unsigned int *)vbem->phys_base;
		video.port = 0;
		video.type = (char *)vbec->signature;
		video.columns = vbem->x_resolution / vbem->x_char_size;
		video.lines = vbem->y_resolution / vbem->y_char_size;
		video.size = vbec->total_memory * vbem->win_size * 1024;
		video.version = vbec->version;
		video.xresolution = vbem->x_resolution;
		video.yresolution = vbem->y_resolution;
		video.xcharsize = vbem->x_char_size;
		video.ycharsize = vbem->y_char_size;
		video.bpp = vbem->bits_per_pixel;

		from = (unsigned long int)video.address;
		to = from + video.size;
		bios_map_add(from, to, MULTIBOOT_MEMORY_AVAILABLE, MULTIBOOT_MEMORY_AVAILABLE);
		from = (unsigned long int)video.address - KERNEL_BASE_ADDR;
		to = (from + video.size);
		bios_map_add(from, to, MULTIBOOT_MEMORY_AVAILABLE, MULTIBOOT_MEMORY_RESERVED);
		fb_init();
	}
}
