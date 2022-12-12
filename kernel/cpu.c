/*
 * fiwix/kernel/cpu.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/utsname.h>
#include <fiwix/pic.h>
#include <fiwix/pit.h>
#include <fiwix/cpu.h>
#include <fiwix/timer.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

char UTS_MACHINE[_UTSNAME_LENGTH + 1];

static struct cpu_type intel[] = {
	{ 4,
	   { "i486 DX", "i486 DX", "i486 SX", "i486 DX/2",
	     "i486 SL", "i486 SX/2", NULL, "i486 DX/2 WBE",
	     "i486 DX/4", NULL, NULL, NULL, NULL, NULL, NULL, NULL }
	},
	{ 5,
	   { NULL, "Pentium 60/66", "Pentium 75-200", "Pentium ODfor486",
	     "PentiumMMX", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	     NULL, NULL, NULL }
	},
	{ 6,
	   { NULL, "Pentium Pro", NULL, "Pentium II", NULL, "Pentium II",
	     "Intel Celeron", "Pentium III", "Pentium III", NULL,
	     "Pentium III Xeon", "Pentium III", NULL, NULL, NULL, NULL }
	}
};

static const char *cpu_flags[] = {
	"FPU", "VME", "DE", "PSE", "TSC", "MSR", "PAE", "MCE", "CX8", "APIC",
	"10", "SEP", "MTRR", "PGE", "MCA", "CMOV", "PAT", "PSE-36", "PSN",
	"CLFSH", "20", "DS", "ACPI", "MMX", "FXSR", "SSE", "SSE2", "SS",
	"HTT", "TM", "30", "PBE"
};

static unsigned long int detect_cpuspeed(void)
{
	unsigned long long int tsc1, tsc2;

	outport_b(MODEREG, SEL_CHAN2 | LSB_MSB | TERM_COUNT | BINARY_CTR);
	outport_b(CHANNEL2, (OSCIL / HZ) & 0xFF);
	outport_b(CHANNEL2, (OSCIL / HZ) >> 8);
	outport_b(PS2_SYSCTRL_B, inport_b(PS2_SYSCTRL_B) | ENABLE_SDATA | ENABLE_TMR2G);

	tsc1 = 0;
	tsc1 = get_rdtsc();

	while(!(inport_b(PS2_SYSCTRL_B) & 0x20));

	tsc2 = 0;
	tsc2 = get_rdtsc();

	outport_b(PS2_SYSCTRL_B, inport_b(PS2_SYSCTRL_B) & ~(ENABLE_SDATA | ENABLE_TMR2G));

	return (tsc2 - tsc1) * HZ;
}

/*
 * These are the 2nd and 3rd level cache values according to Intel Processor
 * Identification and the CPUID Instruction.
 * Application Note 485. Document Number: 241618-031. September 2006.
 */
static void show_cache(int value)
{
	switch(value) {
		/* 2nd level cache */
		case 0x39:
		case 0x3B:
		case 0x41:
		case 0x79:
			cpu_table.cache = "128KB L2";
			break;
		case 0x3A:
			cpu_table.cache = "192KB L2";
			break;
		case 0x3C:
		case 0x42:
		case 0x7A:
		case 0x82:
			cpu_table.cache = "256KB L2";
			break;
		case 0x3D:
			cpu_table.cache = "384KB L2";
			break;
		case 0x3E:
		case 0x43:
		case 0x7B:
		case 0x7F:
		case 0x83:
		case 0x86:
			cpu_table.cache = "512KB L2";
			break;
		case 0x44:
		case 0x78:
		case 0x7C:
		case 0x84:
		case 0x87:
			cpu_table.cache = "1MB L2";
			break;
		case 0x45:
		case 0x7D:
		case 0x85:
			cpu_table.cache = "2MB L2";
			break;

		/* 3rd level cache */
		case 0x22:
			cpu_table.cache = "512KB L3";
			break;
		case 0x23:
			cpu_table.cache = "1MB L3";
			break;
		case 0x25:
			cpu_table.cache = "2MB L3";
			break;
		case 0x29:
		case 0x46:
			cpu_table.cache = "4MB L3";
			break;
		case 0x49:
			cpu_table.cache = "4MB L3 & L2";
			break;
		case 0x4A:
			cpu_table.cache = "6MB L3";
			break;
		case 0x47:
		case 0x4B:
			cpu_table.cache = "8MB L3";
			break;
		case 0x4C:
			cpu_table.cache = "12MB L3";
			break;
		case 0x4D:
			cpu_table.cache = "16MB L3";
			break;
		default:
			break;
	}
}

static void check_cache(int maxcpuid)
{
	int n, maxcpuids;

	maxcpuids = 1;
	if(maxcpuid >= 2) {
		for(n = 0; n < maxcpuids; n++) {
			tlbinfo();
			maxcpuids = _tlbinfo_eax & 0xFF;
			show_cache((_tlbinfo_eax >> 8) & 0xFF);
			show_cache((_tlbinfo_eax >> 16) & 0xFF);
			show_cache((_tlbinfo_eax >> 24) & 0xFF);
			if(!(_tlbinfo_ebx & RESERVED_DESC)) {
				show_cache(_tlbinfo_ebx & 0xFF);
				show_cache((_tlbinfo_ebx >> 8) & 0xFF);
				show_cache((_tlbinfo_ebx >> 16) & 0xFF);
				show_cache((_tlbinfo_ebx >> 24) & 0xFF);
			}
			if(!(_tlbinfo_ecx & RESERVED_DESC)) {
				show_cache(_tlbinfo_ecx & 0xFF);
				show_cache((_tlbinfo_ecx >> 8) & 0xFF);
				show_cache((_tlbinfo_ecx >> 16) & 0xFF);
				show_cache((_tlbinfo_ecx >> 24) & 0xFF);
			}
			if(!(_tlbinfo_edx & RESERVED_DESC)) {
				show_cache(_tlbinfo_edx & 0xFF);
				show_cache((_tlbinfo_edx >> 8) & 0xFF);
				show_cache((_tlbinfo_edx >> 16) & 0xFF);
				show_cache((_tlbinfo_edx >> 24) & 0xFF);
			}
		}
	}
}

int get_cpu_flags(char *buffer)
{
	int n, size;
	unsigned int mask;

	size = sprintk(buffer, "flags           :");
	for(n = 0, mask = 1; n < 32; n++, mask <<= 1) {
		if(_cpuflags & mask) {
			size += sprintk(buffer + size, " %s", cpu_flags[n]);
		}
	}
	size += sprintk(buffer + size, "\n");
	return size;
}

void cpu_init(void)
{
	unsigned int n;
	int maxcpuid;

	memset_b(&cpu_table, 0, sizeof(cpu_table));
	cpu_table.model = -1;
	cpu_table.stepping = -1;

	printk("cpu       -                 -\t");
	cpu_table.family = cpuid();
	if(!cpu_table.family) {
		cpu_table.has_cpuid = 1;
		maxcpuid = get_cpu_vendor_id();
		cpu_table.vendor_id = _vendorid;
		if(maxcpuid >= 1) {
			signature_flags();
			cpu_table.family = _cputype;
			cpu_table.flags = _cpuflags;
			if(!strcmp((char *)_vendorid, "GenuineIntel")) {
				printk("Intel ");
				for(n = 0; n < sizeof(intel) / sizeof(struct cpu_type); n++) {
					if(intel[n].cpu == _cputype) {
						cpu_table.model_name = !intel[n].name[(((int)_cpusignature >> 4) & 0xF)] ? NULL : intel[n].name[(((int)_cpusignature >> 4) & 0xF)];
						break;
					}
				}
				if(cpu_table.model_name) {
					printk("%s", cpu_table.model_name);
				} else {
					printk("processor");
				}
			} else if(!strcmp((char *)_vendorid, "AuthenticAMD")) {
					printk("AMD processor");
			} else {
				printk("x86");
			}
			if(_cpuflags & CPU_TSC) {
				cpu_table.hz = detect_cpuspeed();
				printk(" at %d.%d Mhz", (cpu_table.hz / 1000000), ((cpu_table.hz % 1000000) / 100000));
				check_cache(maxcpuid);
				if(cpu_table.cache) {
					printk(" (%s)", cpu_table.cache);
				}
			}
			printk("\n");
			printk("\t\t\t\tvendorid=%s ", _vendorid);
			cpu_table.model = (_cpusignature >> 4) & 0xF;
			cpu_table.stepping = _cpusignature & 0xF;
			printk("model=%d stepping=%d\n", cpu_table.model, cpu_table.stepping);
		}
		if(!brand_str()) {
			cpu_table.model_name = _brandstr;
			if(cpu_table.model_name[0]) {
				printk("\t\t\t\t%s\n", cpu_table.model_name);
			}
		}
	} else {
		printk("80%d86\n", cpu_table.family);
		cpu_table.has_cpuid = 0;
	}
	strcpy(UTS_MACHINE, "i386");
	strncpy(sys_utsname.machine, UTS_MACHINE, _UTSNAME_LENGTH);
	cpu_table.has_fpu = getfpu();
}
