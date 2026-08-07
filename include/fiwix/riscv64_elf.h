/* ELF64/RISC-V executable planning and generic-kernel loading. */

#ifndef _FIWIX_RISCV64_ELF_H
#define _FIWIX_RISCV64_ELF_H

#define RISCV64_ELF_MAX_SEGMENTS	16

struct riscv64_elf64_header {
	unsigned char ident[16];
	unsigned short type;
	unsigned short machine;
	unsigned int version;
	unsigned long entry;
	unsigned long phoff;
	unsigned long shoff;
	unsigned int flags;
	unsigned short ehsize;
	unsigned short phentsize;
	unsigned short phnum;
	unsigned short shentsize;
	unsigned short shnum;
	unsigned short shstrndx;
};

struct riscv64_elf64_program_header {
	unsigned int type;
	unsigned int flags;
	unsigned long offset;
	unsigned long vaddr;
	unsigned long paddr;
	unsigned long filesz;
	unsigned long memsz;
	unsigned long align;
};

struct riscv64_elf_segment {
	unsigned long offset;
	unsigned long vaddr;
	unsigned long filesz;
	unsigned long memsz;
	unsigned int prot;
};

struct riscv64_elf_plan {
	unsigned long entry;
	unsigned long phdr;
	unsigned long image_end;
	unsigned short phnum;
	unsigned short load_count;
	struct riscv64_elf_segment load[RISCV64_ELF_MAX_SEGMENTS];
};

struct inode;
struct binargs;
struct riscv64_trap_frame;

int riscv64_elf_plan(const void *, unsigned long, unsigned long,
	unsigned long, struct riscv64_elf_plan *);
int riscv64_elf_load(struct inode *, struct binargs *,
	struct riscv64_trap_frame *, const void *, unsigned long);

#endif /* _FIWIX_RISCV64_ELF_H */
