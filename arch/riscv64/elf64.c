/* Pure ELF64/RISC-V validation shared by kernel loading and host tests. */

#include <fiwix/riscv64_elf.h>

#define ELFCLASS64	2
#define ELFDATA2LSB	1
#define EV_CURRENT	1
#define ET_EXEC		2
#define EM_RISCV	243
#define PT_LOAD		1
#define PT_INTERP	3
#define PT_PHDR		6
#define PF_X		1
#define PF_W		2
#define PF_R		4
#define PROT_READ	1
#define PROT_WRITE	2
#define PROT_EXEC	4
#define RISCV64_PAGE_MASK	0xFFFFFFFFFFFFF000UL

typedef char riscv64_elf_header_size_must_be_64[
	(sizeof(struct riscv64_elf64_header) == 64) ? 1 : -1];
typedef char riscv64_elf_program_header_size_must_be_56[
	(sizeof(struct riscv64_elf64_program_header) == 56) ? 1 : -1];

static int range_valid(unsigned long offset, unsigned long size,
	unsigned long limit)
{
	return offset <= limit && size <= limit - offset;
}

static int power_of_two(unsigned long value)
{
	return value && !(value & (value - 1));
}

int riscv64_elf_plan(const void *header_data, unsigned long header_size,
	unsigned long file_size, unsigned long user_limit,
	struct riscv64_elf_plan *plan)
{
	const struct riscv64_elf64_header *header;
	const struct riscv64_elf64_program_header *program;
	unsigned long table_size;
	unsigned long start, end, other_start, other_end;
	unsigned short n, other;
	int entry_executable, phdr_mapped;

	if(header_size < sizeof(*header) || !plan) {
		return -1;
	}
	header = (const struct riscv64_elf64_header *)header_data;
	if(header->ident[0] != 0x7f || header->ident[1] != 'E' ||
		header->ident[2] != 'L' || header->ident[3] != 'F' ||
		header->ident[4] != ELFCLASS64 ||
		header->ident[5] != ELFDATA2LSB ||
		header->ident[6] != EV_CURRENT || header->type != ET_EXEC ||
		header->machine != EM_RISCV || header->version != EV_CURRENT ||
		header->ehsize != sizeof(*header) ||
		header->phentsize != sizeof(*program) || !header->phnum ||
		header->phnum > RISCV64_ELF_MAX_SEGMENTS) {
		return -1;
	}
	table_size = (unsigned long)header->phnum * header->phentsize;
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
		program = (const struct riscv64_elf64_program_header *)(
			(const unsigned char *)header_data + header->phoff +
			(unsigned long)n * header->phentsize);
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
			program->vaddr >= user_limit ||
			!range_valid(program->vaddr, program->memsz, user_limit) ||
			(program->align && (!power_of_two(program->align) ||
				((program->vaddr - program->offset) &
				(program->align - 1))))) {
			return -1;
		}
		start = program->vaddr & RISCV64_PAGE_MASK;
		end = (program->vaddr + program->memsz + 4095UL) &
			RISCV64_PAGE_MASK;
		if(!start || end < start || end - start > 0xffffffffUL) {
			return -1;
		}
		for(other = 0; other < plan->load_count; other++) {
			other_start = plan->load[other].vaddr & RISCV64_PAGE_MASK;
			other_end = (plan->load[other].vaddr +
				plan->load[other].memsz + 4095UL) &
				RISCV64_PAGE_MASK;
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
			plan->phdr - plan->load[n].vaddr <= plan->load[n].filesz &&
			table_size <= plan->load[n].filesz -
				(plan->phdr - plan->load[n].vaddr)) {
			phdr_mapped = 1;
			break;
		}
	}
	if(!plan->load_count || !entry_executable || !plan->phdr ||
		!phdr_mapped) {
		return -1;
	}
	return 0;
}
