/*
 * fiwix/include/fiwix/cpu.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CPU_H
#define _FIWIX_CPU_H

#define CPU_FPU		0x00000001	/* Floating-Point Unit on chip */
#define CPU_VME		0x00000002	/* Virtual 8086 Mode Enhancements */
#define CPU_DE		0x00000004	/* Debugging Extensions */
#define CPU_PSE		0x00000008	/* Page Size Extension */
#define CPU_TSC		0x00000010	/* Time Stamp Counter */
#define CPU_MSR		0x00000020	/* Model Specific Registers */
#define CPU_PAE		0x00000040	/* Physical Address Extension */
#define CPU_MCE		0x00000080	/* Machine Check Exception */
#define CPU_CX8		0x00000100	/* CMPXCHG8B instruction supported */
#define CPU_APIC	0x00000200	/* On-chip APIC hardware supported */
#define CPU_RES10	0x00000400	/* Reserved */
#define CPU_SEP		0x00000800	/* Fast System Call */
#define CPU_MTRR	0x00001000	/* Memory Type Range Registers */
#define CPU_PGE		0x00002000	/* Page Global Enable */
#define CPU_MCA		0x00004000	/* Machine Check Architecture */
#define CPU_CMOV	0x00008000	/* Conditional Move Instruction */
#define CPU_PAT		0x00010000	/* Page Attribute Table */
#define CPU_PSE36	0x00020000	/* 36-bit Page Size Extension */
#define CPU_PSN		0x00040000	/* Processor Serial Number */
#define CPU_CLFSH	0x00080000	/* CLFLUSH instruction supported */
#define CPU_RES20	0x00100000	/* Reserved */
#define CPU_DS		0x00200000	/* Debug Store */
#define CPU_ACPI	0x00400000	/* Thermal Monitor and others */
#define CPU_MMX		0x00800000	/* Intel Architecture MMX Technology */
#define CPU_FXSR	0x01000000	/* Fast Floating Point Save and Rest. */
#define CPU_SSE		0x02000000	/* Streaming SIMD Extensions */
#define CPU_SSE2	0x04000000	/* Streaming SIMD Extensions 2 */
#define CPU_SS		0x08000000	/* Self-Snoop */
#define CPU_HTT		0x10000000	/* Hyper-Threading Technology */
#define CPU_TM		0x20000000	/* Thermal Monitor */
#define CPU_RES30	0x40000000	/* Reserved */
#define CPU_PBE		0x80000000	/* Pending Break Enable */

#define RESERVED_DESC	0x80000000	/* TLB descriptor reserved */

struct cpu {
	char *vendor_id;
	char family;
	char model;
	char *model_name;
	char stepping;
	unsigned int hz;
	char *cache;
	char has_cpuid;
	char has_fpu;
	int flags;
};
extern struct cpu cpu_table;

struct cpu_type {
	int cpu;
	char *name[20];
};

int get_cpu_flags(char *);
void cpu_init(void);

#endif /* _FIWIX_CPU_H */
