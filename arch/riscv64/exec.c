/* Eager ELF64 loader for the generic RV64 process path. */

#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/fcntl.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_elf.h>
#include <fiwix/riscv64_signal.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define AT_NULL		0
#define AT_PHDR		3
#define AT_PHENT	4
#define AT_PHNUM	5
#define AT_PAGESZ	6
#define AT_ENTRY	9
#define AT_UID		11
#define AT_EUID		12
#define AT_GID		13
#define AT_EGID		14
#define RISCV64_ALIGN16_MASK	0xFFFFFFFFFFFFFFF0UL
#define RISCV64_SSTATUS_NO_SPP	0xFFFFFFFFFFFFFEFFUL

static int riscv64_read_inode(struct inode *inode, unsigned long offset,
	void *destination, unsigned long count)
{
	struct buffer *buffer;
	__blk_t block;
	unsigned long block_offset, bytes;
	unsigned char *output;

	output = (unsigned char *)destination;
	while(count) {
		block = bmap(inode, (__off_t)offset, FOR_READING);
		if(block < 0) {
			return block;
		}
		block_offset = offset & (inode->sb->s_blocksize - 1);
		bytes = inode->sb->s_blocksize - block_offset;
		if(bytes > count) {
			bytes = count;
		}
		if(!block) {
			memset_b(output, 0, bytes);
		} else {
			buffer = bread(inode->dev, block, inode->sb->s_blocksize);
			if(!buffer) {
				return -EIO;
			}
			memcpy_b(output, buffer->data + block_offset, bytes);
			brelse(buffer);
		}
		offset += bytes;
		output += bytes;
		count -= bytes;
	}
	return 0;
}

static int riscv64_map_range(__addr_t start, __addr_t end,
	unsigned int prot, char type)
{
	__addr_t address, page;
	signed long result;

	result = do_mmap(NULL, start, end - start, prot,
		MAP_PRIVATE | MAP_FIXED, 0, type, 0, NULL);
	if(result < 0) {
		return result;
	}
	for(address = start; address < end; address += PAGE_SIZE) {
		page = map_page(current, address, 0, prot);
		if(!page) {
			return -ENOMEM;
		}
		memset_b((void *)page, 0, PAGE_SIZE);
	}
	return 0;
}

static int riscv64_load_segment(struct inode *inode,
	const struct riscv64_elf_segment *segment)
{
	__addr_t start, end, address, page;
	unsigned long copied, page_offset, bytes;
	char type;
	int error;

	start = segment->vaddr & PAGE_MASK;
	end = PAGE_ALIGN(segment->vaddr + segment->memsz);
	type = segment->prot & PROT_EXEC ? P_TEXT : P_DATA;
	if((error = riscv64_map_range(start, end, segment->prot, type))) {
		return error;
	}
	copied = 0;
	while(copied < segment->filesz) {
		address = segment->vaddr + copied;
		page = map_page(current, address & PAGE_MASK, 0, segment->prot);
		if(!page) {
			return -ENOMEM;
		}
		page_offset = address & ~PAGE_MASK;
		bytes = PAGE_SIZE - page_offset;
		if(bytes > segment->filesz - copied) {
			bytes = segment->filesz - copied;
		}
		error = riscv64_read_inode(inode, segment->offset + copied,
			(void *)(page + page_offset), bytes);
		if(error) {
			return error;
		}
		copied += bytes;
	}
	return 0;
}

static unsigned long riscv64_barg_start(struct binargs *barg)
{
	int n;

	for(n = 0; n < ARG_MAX; n++) {
		if(barg->page[n]) {
			return RISCV64_USER_STACK_TOP -
				((ARG_MAX - n) * PAGE_SIZE) +
				barg->offset;
		}
	}
	return RISCV64_USER_STACK_TOP - 4;
}

static unsigned char riscv64_barg_byte(struct binargs *barg,
	unsigned long address)
{
	unsigned long page_base;
	int n;

	n = ARG_MAX - (int)((RISCV64_USER_STACK_TOP - address + PAGE_SIZE - 1) /
		PAGE_SIZE);
	if(n < 0 || n >= ARG_MAX || !barg->page[n]) {
		return 0;
	}
	page_base = RISCV64_USER_STACK_TOP - ((ARG_MAX - n) * PAGE_SIZE);
	return *((unsigned char *)barg->page[n] + address - page_base);
}

static unsigned long riscv64_next_string(struct binargs *barg,
	unsigned long address)
{
	while(riscv64_barg_byte(barg, address)) {
		address++;
	}
	return address + 1;
}

static int riscv64_store_word(unsigned long address, unsigned long value)
{
	__addr_t page;

	page = map_page(current, address & PAGE_MASK, 0,
		PROT_READ | PROT_WRITE);
	if(!page) {
		return -ENOMEM;
	}
	*(unsigned long *)(page + (address & ~PAGE_MASK)) = value;
	return 0;
}

static int riscv64_create_stack(struct binargs *barg,
	const struct riscv64_elf_plan *plan, unsigned long *stack_pointer)
{
	unsigned long string, stack, slots, address, page;
	int n, error;

	string = riscv64_barg_start(barg);
	slots = 1 + barg->argc + 1 + barg->envc + 1 + 20;
	stack = (string & RISCV64_ALIGN16_MASK) -
		slots * sizeof(unsigned long);
	stack &= RISCV64_ALIGN16_MASK;
	if((error = riscv64_map_range(stack & PAGE_MASK,
		RISCV64_USER_STACK_TOP,
		PROT_READ | PROT_WRITE, P_STACK))) {
		return error;
	}
	for(n = 0; n < ARG_MAX; n++) {
		if(!barg->page[n]) {
			continue;
		}
		address = RISCV64_USER_STACK_TOP -
			((ARG_MAX - n) * PAGE_SIZE);
		page = map_page(current, address, 0, PROT_READ | PROT_WRITE);
		if(!page) {
			return -ENOMEM;
		}
		memcpy_b((void *)page, (void *)barg->page[n], PAGE_SIZE);
	}
	address = stack;
#define PUSH_WORD(value) \
	do { \
		if((error = riscv64_store_word(address, (unsigned long)(value)))) { \
			return error; \
		} \
		address += sizeof(unsigned long); \
	} while(0)
	PUSH_WORD(barg->argc);
	current->argc = barg->argc;
	current->argv = (char **)address;
	for(n = 0; n < barg->argc; n++) {
		PUSH_WORD(string);
		string = riscv64_next_string(barg, string);
	}
	PUSH_WORD(0);
	current->envc = barg->envc;
	current->envp = (char **)address;
	for(n = 0; n < barg->envc; n++) {
		PUSH_WORD(string);
		string = riscv64_next_string(barg, string);
	}
	PUSH_WORD(0);
	PUSH_WORD(AT_PHDR);   PUSH_WORD(plan->phdr);
	PUSH_WORD(AT_PHENT);  PUSH_WORD(sizeof(struct riscv64_elf64_program_header));
	PUSH_WORD(AT_PHNUM);  PUSH_WORD(plan->phnum);
	PUSH_WORD(AT_PAGESZ); PUSH_WORD(PAGE_SIZE);
	PUSH_WORD(AT_ENTRY);  PUSH_WORD(plan->entry);
	PUSH_WORD(AT_UID);    PUSH_WORD(current->uid);
	PUSH_WORD(AT_EUID);   PUSH_WORD(current->euid);
	PUSH_WORD(AT_GID);    PUSH_WORD(current->gid);
	PUSH_WORD(AT_EGID);   PUSH_WORD(current->egid);
	PUSH_WORD(AT_NULL);   PUSH_WORD(0);
#undef PUSH_WORD
	*stack_pointer = stack;
	return 0;
}

int riscv64_elf_load(struct inode *inode, struct binargs *barg,
	struct riscv64_trap_frame *frame, const void *header_data,
	unsigned long header_size)
{
	struct riscv64_elf_plan plan;
	const char *stage;
	unsigned long heap, stack, sstatus;
	unsigned short n;
	int error;

	if(riscv64_elf_plan(header_data, header_size, inode->i_size,
		RISCV64_SIGNAL_TRAMPOLINE, &plan)) {
		return -ENOEXEC;
	}

	if(!current->vma_table) {
		unmap_page(RISCV64_SIGNAL_TRAMPOLINE);
		unmap_page(RISCV64_SIGNAL_TRAMPOLINE - PAGE_SIZE);
	}
	release_binary();
	stage = "load segment";
	for(n = 0; n < plan.load_count; n++) {
		if((error = riscv64_load_segment(inode, &plan.load[n]))) {
			goto failed;
		}
	}
	stage = "map signal trampoline";
	if((error = riscv64_signal_map())) {
		goto failed;
	}
	heap = PAGE_ALIGN(plan.image_end);
	stage = "map heap";
	if((error = riscv64_map_range(heap, heap + PAGE_SIZE,
		PROT_READ | PROT_WRITE, P_HEAP))) {
		goto failed;
	}
	current->brk_lower = current->brk = heap;
	stage = "create stack";
	if((error = riscv64_create_stack(barg, &plan, &stack))) {
		goto failed;
	}

	current->entry_address = plan.entry;
	current->end_code = plan.image_end;
	sstatus = frame->sstatus;
	memset_b(frame, 0, sizeof(*frame));
	frame->sp = stack;
	frame->sepc = plan.entry;
	frame->sstatus = (sstatus & RISCV64_SSTATUS_NO_SPP) | 0x20UL;
	return 0;

failed:
	printk("WARNING: riscv64 ELF load failed for pid %d at %s: error %d.\n",
		current->pid, stage, error);
	release_binary();
	send_sig(current, SIGKILL);
	return error;
}
