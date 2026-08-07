/* RISC-V CPU identity exposed through the generic Fiwix interfaces. */

#include <fiwix/cpu.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/utsname.h>

char UTS_MACHINE[_UTSNAME_LENGTH + 1];
struct cpu cpu_table;

int get_cpu_flags(char *buffer)
{
	return sprintk(buffer,
		"isa             : rv64ima_zicsr_zifencei\n");
}

void cpu_init(void)
{
	memset_b(&cpu_table, 0, sizeof(cpu_table));
	cpu_table.vendor_id = "RISC-V";
	cpu_table.family = 64;
	cpu_table.model_name = "RV64IMA";
	cpu_table.model = 0;
	cpu_table.stepping = 0;
	cpu_table.has_cpuid = 0;
	cpu_table.has_fpu = 0;

	strcpy(UTS_MACHINE, "riscv64");
	strncpy(sys_utsname.machine, UTS_MACHINE, _UTSNAME_LENGTH);
	printk("cpu       - RISC-V RV64IMA\n");
}
