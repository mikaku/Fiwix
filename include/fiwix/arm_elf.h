/*
 * fiwix/include/fiwix/arm_elf.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_ELF_H
#define _FIWIX_ARM_ELF_H

#define ARM_ELF_MAX_SEGMENTS	16

struct arm_elf32_header {
	unsigned char ident[16];
	unsigned short type;
	unsigned short machine;
	unsigned int version;
	unsigned int entry;
	unsigned int phoff;
	unsigned int shoff;
	unsigned int flags;
	unsigned short ehsize;
	unsigned short phentsize;
	unsigned short phnum;
	unsigned short shentsize;
	unsigned short shnum;
	unsigned short shstrndx;
};

struct arm_elf32_program_header {
	unsigned int type;
	unsigned int offset;
	unsigned int vaddr;
	unsigned int paddr;
	unsigned int filesz;
	unsigned int memsz;
	unsigned int flags;
	unsigned int align;
};

struct arm_elf_segment {
	unsigned int offset;
	unsigned int vaddr;
	unsigned int filesz;
	unsigned int memsz;
	unsigned int prot;
};

struct arm_elf_plan {
	unsigned int entry;
	unsigned int phdr;
	unsigned int image_end;
	unsigned short phnum;
	unsigned short load_count;
	struct arm_elf_segment load[ARM_ELF_MAX_SEGMENTS];
};

struct arm_trap_frame;
struct binargs;
struct inode;

int arm_elf32_plan(const void *, unsigned int, unsigned int, unsigned int,
	struct arm_elf_plan *);
int arm_elf32_load(struct inode *, struct binargs *, struct arm_trap_frame *,
	const void *, unsigned int);

#endif /* _FIWIX_ARM_ELF_H */
