/*
 * fiwix/arch/arm/cpu.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/cpu.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/utsname.h>

char UTS_MACHINE[_UTSNAME_LENGTH + 1];
struct cpu cpu_table;

int get_cpu_flags(char *buffer)
{
	return sprintk(buffer, "isa             : ARMv7-A\n");
}

void cpu_init(void)
{
	memset_b(&cpu_table, 0, sizeof(cpu_table));
	cpu_table.vendor_id = "ARM";
	cpu_table.family = 7;
	cpu_table.model_name = "ARMv7-A";

	strcpy(UTS_MACHINE, "armv7l");
	strncpy(sys_utsname.machine, UTS_MACHINE, _UTSNAME_LENGTH);
	printk("cpu       - ARMv7-A\n");
}
