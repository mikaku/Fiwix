/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_elf.h>
#include <fiwix/arm_trap.h>
#include <fiwix/arm_vm.h>
#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/string.h>

#define HOST_RAM_BASE		0x43000000U
#define HOST_RAM_PAGES		128U
#define HOST_PROT_READ		0x1
#define HOST_PROT_WRITE		0x2
#define HOST_MAP_PRIVATE	0x2
#define HOST_MAP_ANONYMOUS	0x20
#define HOST_MAP_FIXED_NOREPLACE 0x100000
#define IMAGE_SIZE		0x1800U
#define BLOCK_SIZE		512U
#define MAX_MAPPINGS		96
#define MAX_VMAS		8

extern void *mmap(void *, unsigned long, int, int, int, long);
extern int munmap(void *, unsigned long);

struct user_mapping {
	unsigned int virtual_address;
	unsigned int physical_address;
	unsigned int prot;
};

struct vma_record {
	unsigned int start;
	unsigned int end;
	unsigned int prot;
	char type;
};

static unsigned char image[IMAGE_SIZE];
static struct user_mapping mappings[MAX_MAPPINGS];
static struct vma_record vmas[MAX_VMAS];
static struct buffer fake_buffer;
static struct superblock fake_superblock;
static struct inode fake_inode;
static struct proc process;
static unsigned int next_page;
static int mapping_count;
static int vma_count;
static int signal_count;
static int instruction_cache_invalidations;

struct proc *current = &process;

void memcpy_b(void *destination, const void *source, unsigned int count)
{
	unsigned char *to;
	const unsigned char *from;

	to = (unsigned char *)destination;
	from = (const unsigned char *)source;
	while(count--) {
		*to++ = *from++;
	}
}

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *to;

	to = (unsigned char *)destination;
	while(count--) {
		*to++ = value;
	}
}

void printk(const char *format, ...)
{
	(void)format;
}

void arm_instruction_cache_invalidate(void)
{
	instruction_cache_invalidations++;
}

int send_sig(struct proc *p, __sigset_t signal)
{
	(void)p;
	(void)signal;
	signal_count++;
	return 0;
}

int bmap(struct inode *inode, __off_t offset, int mode)
{
	(void)inode;
	(void)mode;
	return (int)(offset / BLOCK_SIZE) + 1;
}

struct buffer *bread(__dev_t device, __blk_t block, int size)
{
	unsigned int offset;

	(void)device;
	if(size != BLOCK_SIZE || block <= 0) {
		return 0;
	}
	offset = (unsigned int)(block - 1) * BLOCK_SIZE;
	if(offset >= IMAGE_SIZE) {
		return 0;
	}
	fake_buffer.data = (char *)image + offset;
	fake_buffer.size = size;
	return &fake_buffer;
}

void brelse(struct buffer *buffer)
{
	(void)buffer;
}

void release_binary(void)
{
	mapping_count = 0;
	vma_count = 0;
	current->vma_table = 0;
}

signed long do_mmap(struct inode *inode, __addr_t start, __size_t length,
	unsigned int prot, unsigned int flags, unsigned int offset, char type,
	char mode, void *object)
{
	(void)inode;
	(void)offset;
	(void)mode;
	(void)object;
	if(!(flags & MAP_FIXED) || !length || vma_count == MAX_VMAS) {
		return -EINVAL;
	}
	vmas[vma_count].start = start;
	vmas[vma_count].end = start + length;
	vmas[vma_count].prot = prot;
	vmas[vma_count].type = type;
	vma_count++;
	return start;
}

static unsigned int allocate_page(void)
{
	unsigned int address;

	if(next_page == HOST_RAM_PAGES) {
		return 0;
	}
	address = HOST_RAM_BASE + next_page * PAGE_SIZE;
	next_page++;
	memset_b((void *)(unsigned long)address, 0, PAGE_SIZE);
	return address;
}

__addr_t map_page(struct proc *p, __addr_t virtual_address, __addr_t address,
	unsigned int prot)
{
	int n;

	(void)p;
	for(n = 0; n < mapping_count; n++) {
		if(mappings[n].virtual_address ==
			(virtual_address & PAGE_MASK)) {
			mappings[n].prot = prot;
			return mappings[n].physical_address;
		}
	}
	if(mapping_count == MAX_MAPPINGS) {
		return 0;
	}
	if(!address) {
		address = allocate_page();
	}
	if(!address) {
		return 0;
	}
	mappings[mapping_count].virtual_address =
		virtual_address & PAGE_MASK;
	mappings[mapping_count].physical_address = address;
	mappings[mapping_count].prot = prot;
	mapping_count++;
	return address;
}

int unmap_page(__addr_t virtual_address)
{
	int n;

	for(n = 0; n < mapping_count; n++) {
		if(mappings[n].virtual_address ==
			(virtual_address & PAGE_MASK)) {
			mapping_count--;
			mappings[n] = mappings[mapping_count];
			return 0;
		}
	}
	return 1;
}

static unsigned int user_physical(unsigned int address)
{
	int n;

	for(n = 0; n < mapping_count; n++) {
		if(mappings[n].virtual_address == (address & PAGE_MASK)) {
			return mappings[n].physical_address +
				(address & ~PAGE_MASK);
		}
	}
	return 0;
}

static unsigned int user_word(unsigned int address)
{
	unsigned int physical;

	physical = user_physical(address);
	return physical ?
		*(unsigned int *)(unsigned long)physical : 0xFFFFFFFFU;
}

static int user_string_equal(unsigned int address, const char *expected)
{
	unsigned int physical;

	while(*expected) {
		physical = user_physical(address++);
		if(!physical ||
			*(unsigned char *)(unsigned long)physical !=
				(unsigned char)*expected++) {
			return 0;
		}
	}
	physical = user_physical(address);
	return physical &&
		!*(unsigned char *)(unsigned long)physical;
}

static void make_image(void)
{
	struct arm_elf32_header *header;
	struct arm_elf32_program_header *program;
	unsigned int n;

	memset_b(image, 0, sizeof(image));
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
	header->phnum = 1;
	program = (struct arm_elf32_program_header *)(image + header->phoff);
	program->type = 1;
	program->offset = 0;
	program->vaddr = 0x00100000U;
	program->filesz = 0x1200U;
	program->memsz = 0x1400U;
	program->flags = 7;
	program->align = PAGE_SIZE;
	for(n = 0x1000; n < program->filesz; n++) {
		image[n] = (unsigned char)(n ^ 0xA5U);
	}
	fake_superblock.s_blocksize = BLOCK_SIZE;
	fake_inode.sb = &fake_superblock;
	fake_inode.i_size = IMAGE_SIZE;
}

static int check_stack(const struct arm_trap_frame *frame)
{
	unsigned int argv;
	unsigned int envp;
	unsigned int stack;

	stack = frame->user_sp;
	if(stack & 7U || user_word(stack) != 1) {
		return -1;
	}
	argv = user_word(stack + 4);
	envp = user_word(stack + 12);
	if(!argv || user_word(stack + 8) ||
		!envp || user_word(stack + 16) ||
		!user_string_equal(argv, "/sbin/init") ||
		!user_string_equal(envp, "HOME=/") ||
		user_word(stack + 20) != 3 ||
		user_word(stack + 24) != 0x00100034U ||
		user_word(stack + 28) != 4 ||
		user_word(stack + 32) !=
			sizeof(struct arm_elf32_program_header) ||
		user_word(stack + 36) != 5 ||
		user_word(stack + 40) != 1) {
		return -1;
	}
	return 0;
}

int main(void)
{
	struct arm_trap_frame frame;
	struct binargs barg;
	unsigned int staging;
	unsigned int physical;
	unsigned int n;

	if(mmap((void *)(unsigned long)HOST_RAM_BASE,
			HOST_RAM_PAGES * PAGE_SIZE,
			HOST_PROT_READ | HOST_PROT_WRITE,
			HOST_MAP_PRIVATE | HOST_MAP_ANONYMOUS |
				HOST_MAP_FIXED_NOREPLACE, -1, 0) !=
		(void *)(unsigned long)HOST_RAM_BASE) {
		return 1;
	}
	make_image();
	memset_b(&process, 0, sizeof(process));
	memset_b(&frame, 0xA5, sizeof(frame));
	memset_b(&barg, 0, sizeof(barg));
	staging = allocate_page();
	barg.page[ARG_MAX - 1] = staging;
	barg.offset = 4000;
	barg.argc = 1;
	barg.envc = 1;
	memcpy_b((void *)(unsigned long)(staging + barg.offset),
		"/sbin/init", 11);
	memcpy_b((void *)(unsigned long)(staging + barg.offset + 11),
		"HOME=/", 7);

	if(arm_elf32_load(&fake_inode, &barg, &frame, image,
			BLOCK_SIZE)) {
		return 2;
	}
	if(frame.pc != 0x00101000U || frame.cpsr != 0x50U ||
		frame.user_lr || frame.vector || check_stack(&frame) ||
		current->entry_address != frame.pc ||
		current->end_code != 0x00101400U ||
		current->brk != 0x00102000U ||
		current->brk_lower != current->brk ||
		current->argc != 1 || current->envc != 1 ||
		(unsigned int)(unsigned long)current->argv !=
			frame.user_sp + 4 ||
		(unsigned int)(unsigned long)current->envp !=
			frame.user_sp + 12 ||
		signal_count || instruction_cache_invalidations != 1) {
		return 3;
	}
	for(n = 0; n < 0x1200U; n++) {
		physical = user_physical(0x00100000U + n);
		if(!physical ||
			*(unsigned char *)(unsigned long)physical != image[n]) {
			return 4;
		}
	}
	for(n = 0x1200U; n < 0x1400U; n++) {
		physical = user_physical(0x00100000U + n);
		if(!physical ||
			*(unsigned char *)(unsigned long)physical) {
			return 5;
		}
	}
	if(vma_count != 3 ||
		vmas[0].start != 0x00100000U ||
		vmas[0].end != 0x00102000U ||
		vmas[0].prot != 7 || vmas[0].type != P_TEXT ||
		vmas[1].start != 0x00102000U ||
		vmas[1].prot != (PROT_READ | PROT_WRITE) ||
		vmas[1].type != P_HEAP ||
		vmas[2].end != ARM_VM_USER_LIMIT ||
		vmas[2].prot != (PROT_READ | PROT_WRITE) ||
		vmas[2].type != P_STACK) {
		return 6;
	}
	if(munmap((void *)(unsigned long)HOST_RAM_BASE,
		HOST_RAM_PAGES * PAGE_SIZE)) {
		return 7;
	}
	return 0;
}
