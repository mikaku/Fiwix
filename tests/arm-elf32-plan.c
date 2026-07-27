/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_elf.h>

#define HEADER_BYTES	512
#define USER_LIMIT	0x40000000U

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
	struct arm_elf32_header *header;
	struct arm_elf32_program_header *program;

	clear_image();
	header = (struct arm_elf32_header *)image;
	header->ident[0] = 0x7f;
	header->ident[1] = 'E';
	header->ident[2] = 'L';
	header->ident[3] = 'F';
	header->ident[4] = 1;
	header->ident[5] = 1;
	header->ident[6] = 1;
	header->type = 2;
	header->machine = 40;
	header->version = 1;
	header->entry = 0x00101000U;
	header->phoff = sizeof(*header);
	header->ehsize = sizeof(*header);
	header->phentsize = sizeof(*program);
	header->phnum = 2;

	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[0].type = 1;
	program[0].offset = 0;
	program[0].vaddr = 0x00100000U;
	program[0].filesz = 0x2000;
	program[0].memsz = 0x2000;
	program[0].flags = 5;
	program[0].align = 0x1000;
	program[1].type = 1;
	program[1].offset = 0x2000;
	program[1].vaddr = 0x00103000U;
	program[1].filesz = 0x100;
	program[1].memsz = 0x500;
	program[1].flags = 6;
	program[1].align = 0x1000;
}

int main(void)
{
	struct arm_elf32_header *header;
	struct arm_elf32_program_header *program;
	struct arm_elf_plan plan;

	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	if(arm_elf32_plan(image, sizeof(image), 0x3000,
			USER_LIMIT, &plan) ||
		plan.entry != 0x00101000U ||
		plan.phdr != 0x00100034U ||
		plan.image_end != 0x00103500U ||
		plan.load_count != 2 ||
		plan.load[0].prot != 5 || plan.load[1].prot != 3) {
		return 1;
	}
	header->ident[4] = 2;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 2;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	header->machine = 3;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 3;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[1].vaddr = 0x00101800U;
	program[1].align = 0;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 4;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[1].type = 3;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 5;
	}
	make_valid();
	if(!arm_elf32_plan(image, 80, 0x3000, USER_LIMIT, &plan)) {
		return 6;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	header->entry = 0x00103100U;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 7;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[1].offset = 0x2F80U;
	program[1].align = 0;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 8;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[1].vaddr = USER_LIMIT - 0x100U;
	program[1].memsz = 0x500U;
	program[1].align = 0;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 9;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[0].offset = 0x1000U;
	program[0].filesz = 0x1000U;
	program[0].align = 0;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 10;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program[0].align = 24;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 11;
	}
	make_valid();
	header = (struct arm_elf32_header *)image;
	header->entry++;
	if(!arm_elf32_plan(image, sizeof(image), 0x3000,
		USER_LIMIT, &plan)) {
		return 12;
	}
	return 0;
}
