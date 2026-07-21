/*
 * fiwix/arch/riscv64/elf.c
 *
 * Minimal ELF64 loader gate for the first riscv64 user process.
 */

#define EI_NIDENT       16
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EV_CURRENT      1
#define ET_EXEC         2
#define EM_RISCV        243
#define PT_LOAD         1
#define PF_X            1
#define PF_W            2
#define PF_R            4
#define USER_TEXT_VA    0x00400000UL
#define PAGE_SIZE       4096UL

typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef unsigned char u8;

struct elf64_header {
	u8 ident[EI_NIDENT];
	u16 type;
	u16 machine;
	u32 version;
	u64 entry;
	u64 phoff;
	u64 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
};

struct elf64_program_header {
	u32 type;
	u32 flags;
	u64 offset;
	u64 vaddr;
	u64 paddr;
	u64 filesz;
	u64 memsz;
	u64 align;
};

typedef char elf64_header_size_must_be_64[
	(sizeof(struct elf64_header) == 64) ? 1 : -1];
typedef char elf64_program_header_size_must_be_56[
	(sizeof(struct elf64_program_header) == 56) ? 1 : -1];

extern u8 riscv64_user_elf_start[];
extern u8 riscv64_user_elf_end[];
extern u8 *riscv64_user_text_page(void);

static int range_valid(u64 offset, u64 size, u64 limit)
{
	return offset <= limit && size <= limit - offset;
}

static void copy_bytes(u8 *destination, const u8 *source, u64 count)
{
	while(count--) {
		*destination++ = *source++;
	}
}

u64 riscv64_load_user_elf(void)
{
	const struct elf64_header *header;
	const struct elf64_program_header *program;
	u8 *page;
	u64 image_size;
	u64 page_offset;
	u64 n;
	u16 index;
	int loaded;
	int entry_mapped;

	image_size = (u64)(riscv64_user_elf_end - riscv64_user_elf_start);
	if(image_size < sizeof(struct elf64_header)) {
		return 0;
	}
	header = (const struct elf64_header *)riscv64_user_elf_start;
	if(header->ident[0] != 0x7f || header->ident[1] != 'E' ||
		header->ident[2] != 'L' || header->ident[3] != 'F' ||
		header->ident[4] != ELFCLASS64 ||
		header->ident[5] != ELFDATA2LSB ||
		header->ident[6] != EV_CURRENT || header->type != ET_EXEC ||
		header->machine != EM_RISCV || header->version != EV_CURRENT ||
		header->ehsize != sizeof(struct elf64_header) ||
		header->phentsize != sizeof(struct elf64_program_header) ||
		header->phnum == 0 || header->phnum > 16 ||
		!range_valid(header->phoff,
			(u64)header->phnum * header->phentsize, image_size)) {
		return 0;
	}

	page = riscv64_user_text_page();
	for(n = 0; n < PAGE_SIZE; n++) {
		page[n] = 0;
	}
	loaded = 0;
	entry_mapped = 0;
	for(index = 0; index < header->phnum; index++) {
		program = (const struct elf64_program_header *)(
			riscv64_user_elf_start + header->phoff +
			(u64)index * header->phentsize);
		if(program->type != PT_LOAD) {
			continue;
		}
		if(program->filesz > program->memsz ||
			program->vaddr < USER_TEXT_VA ||
			program->vaddr + program->memsz < program->vaddr ||
			program->vaddr + program->memsz > USER_TEXT_VA + PAGE_SIZE ||
			!range_valid(program->offset, program->filesz, image_size) ||
			(program->flags & (PF_R | PF_X)) != (PF_R | PF_X) ||
			(program->flags & PF_W) ||
			(program->align &&
				((program->align & (program->align - 1)) ||
				((program->vaddr - program->offset) &
					(program->align - 1))))) {
			return 0;
		}
		page_offset = program->vaddr - USER_TEXT_VA;
		copy_bytes(page + page_offset,
			riscv64_user_elf_start + program->offset, program->filesz);
		loaded++;
		if(header->entry >= program->vaddr &&
			header->entry < program->vaddr + program->memsz) {
			entry_mapped = 1;
		}
	}
	if(loaded != 1 || !entry_mapped) {
		return 0;
	}
	return header->entry;
}
