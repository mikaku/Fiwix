#include <stdio.h>

#include <fiwix/riscv64_elf.h>

#define HEADER_BYTES	512
#define USER_LIMIT	0x4000000000UL

static unsigned char image[HEADER_BYTES];

static void clear_image(void)
{
	unsigned int n;

	for(n = 0; n < sizeof(image); n++) {
		image[n] = 0;
	}
}

static void make_valid(void)
{
	struct riscv64_elf64_header *header;
	struct riscv64_elf64_program_header *program;

	clear_image();
	header = (struct riscv64_elf64_header *)image;
	header->ident[0] = 0x7f;
	header->ident[1] = 'E';
	header->ident[2] = 'L';
	header->ident[3] = 'F';
	header->ident[4] = 2;
	header->ident[5] = 1;
	header->ident[6] = 1;
	header->type = 2;
	header->machine = 243;
	header->version = 1;
	header->entry = 0x10100;
	header->phoff = sizeof(*header);
	header->ehsize = sizeof(*header);
	header->phentsize = sizeof(*program);
	header->phnum = 2;

	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	program[0].type = 1;
	program[0].flags = 5;
	program[0].offset = 0;
	program[0].vaddr = 0x10000;
	program[0].filesz = 0x1000;
	program[0].memsz = 0x1000;
	program[0].align = 0x1000;
	program[1].type = 1;
	program[1].flags = 6;
	program[1].offset = 0x1000;
	program[1].vaddr = 0x12000;
	program[1].filesz = 0x100;
	program[1].memsz = 0x500;
	program[1].align = 0x1000;
}

static int check_stage0_file(const char *path)
{
	FILE *file;
	long size;
	unsigned long header_size;
	struct riscv64_elf_plan plan;

	file = fopen(path, "rb");
	if(!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
		fseek(file, 0, SEEK_SET)) {
		if(file) {
			fclose(file);
		}
		return 20;
	}
	clear_image();
	header_size = size < HEADER_BYTES ? (unsigned long)size : HEADER_BYTES;
	if(fread(image, 1, header_size, file) != header_size || fclose(file)) {
		return 21;
	}
	if(riscv64_elf_plan(image, header_size, (unsigned long)size,
		USER_LIMIT, &plan) || plan.entry != 0x600078 ||
		plan.phdr != 0x600040 || plan.image_end != 0x600188 ||
		plan.load_count != 1 || plan.load[0].prot != 7) {
		return 22;
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct riscv64_elf64_header *header;
	struct riscv64_elf64_program_header *program;
	struct riscv64_elf_plan plan;

	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	if(riscv64_elf_plan(image, sizeof(image), 0x2000, USER_LIMIT, &plan) ||
		plan.entry != 0x10100 || plan.phdr != 0x10040 ||
		plan.image_end != 0x12500 || plan.load_count != 2 ||
		plan.load[0].prot != 5 || plan.load[1].prot != 3) {
		return 1;
	}

	header->ident[4] = 1;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 2;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	program[1].flags = 7;
	if(riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan) || plan.load[1].prot != 7) {
		return 3;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	program[1].vaddr = 0x10800;
	program[1].align = 0;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 4;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	program[1].type = 3;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 5;
	}
	make_valid();
	if(!riscv64_elf_plan(image, 100, 0x2000, USER_LIMIT, &plan)) {
		return 6;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	header->entry = 0x12100;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 7;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	program[1].offset = 0x1f80;
	program[1].align = 0;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 8;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	program[1].memsz = 0x100000001UL;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 9;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	header->entry = 0x600078;
	header->phnum = 1;
	program[0].flags = 7;
	program[0].offset = 0;
	program[0].vaddr = 0x600000;
	program[0].filesz = 392;
	program[0].memsz = 392;
	program[0].align = 1;
	if(riscv64_elf_plan(image, sizeof(image), 392, USER_LIMIT, &plan) ||
		plan.entry != 0x600078 || plan.phdr != 0x600040 ||
		plan.load_count != 1 || plan.load[0].prot != 7) {
		return 10;
	}
	make_valid();
	header = (struct riscv64_elf64_header *)image;
	program = (struct riscv64_elf64_program_header *)(image + header->phoff);
	header->phnum = 3;
	program[2].type = 6;
	program[2].vaddr = 0x20000;
	if(!riscv64_elf_plan(image, sizeof(image), 0x2000,
		USER_LIMIT, &plan)) {
		return 11;
	}

	if(argc > 2) {
		return 12;
	}
	return argc == 2 ? check_stage0_file(argv[1]) : 0;
}
