/*
 * fiwix/arch/arm/elf32.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_elf.h>

#define ELFCLASS32	1
#define ELFDATA2LSB	1
#define EV_CURRENT	1
#define ET_EXEC		2
#define EM_ARM		40
#define PT_LOAD		1
#define PT_INTERP	3
#define PT_PHDR		6
#define PF_X		1
#define PF_W		2
#define PF_R		4
#define PROT_READ	1
#define PROT_WRITE	2
#define PROT_EXEC	4
#define ARM_PAGE_MASK	0xFFFFF000U
#define ARM_USER_BASE	0x00100000U

typedef char arm_elf_header_size_must_be_52[
	(sizeof(struct arm_elf32_header) == 52) ? 1 : -1];
typedef char arm_elf_program_header_size_must_be_32[
	(sizeof(struct arm_elf32_program_header) == 32) ? 1 : -1];

static int range_valid(unsigned int offset, unsigned int size,
	unsigned int limit)
{
	return offset <= limit && size <= limit - offset;
}

static int power_of_two(unsigned int value)
{
	return value && !(value & (value - 1));
}

int arm_elf32_plan(const void *header_data, unsigned int header_size,
	unsigned int file_size, unsigned int user_limit,
	struct arm_elf_plan *plan)
{
	const struct arm_elf32_header *header;
	const struct arm_elf32_program_header *program;
	unsigned int table_size;
	unsigned int start, end, other_start, other_end;
	unsigned short n, other;
	int entry_executable, phdr_mapped;

	if(header_size < sizeof(*header) || !plan) {
		return -1;
	}
	header = (const struct arm_elf32_header *)header_data;
	if(header->ident[0] != 0x7f || header->ident[1] != 'E' ||
		header->ident[2] != 'L' || header->ident[3] != 'F' ||
		header->ident[4] != ELFCLASS32 ||
		header->ident[5] != ELFDATA2LSB ||
		header->ident[6] != EV_CURRENT || header->type != ET_EXEC ||
		header->machine != EM_ARM || header->version != EV_CURRENT ||
		header->ehsize != sizeof(*header) ||
		header->phentsize != sizeof(*program) || !header->phnum ||
		header->phnum > ARM_ELF_MAX_SEGMENTS) {
		return -1;
	}
	table_size = (unsigned int)header->phnum * header->phentsize;
	if(!range_valid(header->phoff, table_size, header_size)) {
		return -1;
	}

	plan->entry = header->entry;
	plan->phdr = 0;
	plan->image_end = 0;
	plan->phnum = header->phnum;
	plan->load_count = 0;
	entry_executable = 0;
	for(n = 0; n < header->phnum; n++) {
		program = (const struct arm_elf32_program_header *)(
			(const unsigned char *)header_data + header->phoff +
			(unsigned int)n * header->phentsize);
		if(program->type == PT_INTERP) {
			return -1;
		}
		if(program->type == PT_PHDR) {
			plan->phdr = program->vaddr;
			continue;
		}
		if(program->type != PT_LOAD || !program->memsz) {
			continue;
		}
		if(program->filesz > program->memsz || !program->flags ||
			!range_valid(program->offset, program->filesz, file_size) ||
			program->vaddr < ARM_USER_BASE ||
			program->vaddr >= user_limit ||
			!range_valid(program->vaddr, program->memsz, user_limit) ||
			(program->align && (!power_of_two(program->align) ||
				((program->vaddr - program->offset) &
					(program->align - 1))))) {
			return -1;
		}
		start = program->vaddr & ARM_PAGE_MASK;
		end = (program->vaddr + program->memsz + 4095U) &
			ARM_PAGE_MASK;
		if(end < start) {
			return -1;
		}
		for(other = 0; other < plan->load_count; other++) {
			other_start = plan->load[other].vaddr & ARM_PAGE_MASK;
			other_end = (plan->load[other].vaddr +
				plan->load[other].memsz + 4095U) &
				ARM_PAGE_MASK;
			if(start < other_end && other_start < end) {
				return -1;
			}
		}
		plan->load[plan->load_count].offset = program->offset;
		plan->load[plan->load_count].vaddr = program->vaddr;
		plan->load[plan->load_count].filesz = program->filesz;
		plan->load[plan->load_count].memsz = program->memsz;
		plan->load[plan->load_count].prot = 0;
		if(program->flags & PF_R) {
			plan->load[plan->load_count].prot |= PROT_READ;
		}
		if(program->flags & PF_W) {
			plan->load[plan->load_count].prot |= PROT_WRITE;
		}
		if(program->flags & PF_X) {
			plan->load[plan->load_count].prot |= PROT_EXEC;
			if(header->entry >= program->vaddr &&
				header->entry < program->vaddr + program->memsz) {
				entry_executable = 1;
			}
		}
		plan->load_count++;
		if(program->vaddr + program->memsz > plan->image_end) {
			plan->image_end = program->vaddr + program->memsz;
		}
		if(!plan->phdr && header->phoff >= program->offset &&
			header->phoff + table_size <= program->offset +
				program->filesz) {
			plan->phdr = program->vaddr + header->phoff -
				program->offset;
		}
	}
	phdr_mapped = 0;
	for(n = 0; n < plan->load_count; n++) {
		if(plan->phdr >= plan->load[n].vaddr &&
			plan->phdr - plan->load[n].vaddr <=
				plan->load[n].filesz &&
			table_size <= plan->load[n].filesz -
				(plan->phdr - plan->load[n].vaddr)) {
			phdr_mapped = 1;
			break;
		}
	}
	if(!plan->load_count || !entry_executable ||
		(header->entry & 3U) || !plan->phdr ||
		!phdr_mapped) {
		return -1;
	}
	return 0;
}
