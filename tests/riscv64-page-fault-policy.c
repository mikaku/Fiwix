#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/string.h>

static struct proc process;
struct proc *current = &process;
static struct vma region;
static struct vma stack_region;
static struct vma trampoline_region;
static int use_region;
static int use_stack;
static int copy_result;
static int map_calls;
static __addr_t last_map_address;
static unsigned char mapped_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static struct page pages[1];
struct page *page_table = pages;

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *byte;

	byte = (unsigned char *)destination;
	while(count--) {
		*byte++ = value;
	}
}

struct vma *find_vma_region(__addr_t address)
{
	if(use_region && address >= region.start && address < region.end) {
		return &region;
	}
	if(use_stack && address >= stack_region.start &&
		address < stack_region.end) {
		return &stack_region;
	}
	return 0;
}

int copy_on_write_page(struct vma *vma, __addr_t address)
{
	if(vma != &region || address < region.start || address >= region.end) {
		return -1;
	}
	return copy_result;
}

__addr_t map_page(struct proc *owner, __addr_t address, __addr_t physical,
	unsigned int prot)
{
	(void)physical;
	(void)prot;
	if(owner != current) {
		return 0;
	}
	map_calls++;
	last_map_address = address;
	return (__addr_t)mapped_page;
}

struct page *search_page_hash(struct inode *inode, __off_t offset)
{
	(void)inode;
	(void)offset;
	return 0;
}

void page_lock(struct page *page)
{
	(void)page;
}

void page_unlock(struct page *page)
{
	(void)page;
}

int bread_page(struct page *page, struct inode *inode, __off_t offset,
	char prot, char flags)
{
	(void)page;
	(void)inode;
	(void)offset;
	(void)prot;
	(void)flags;
	return 0;
}

int unmap_page(__addr_t address)
{
	(void)address;
	return 0;
}

int shm_map_page(struct vma *vma, unsigned int address)
{
	(void)vma;
	(void)address;
	return 0;
}

void printk(const char *format, ...)
{
	(void)format;
}

static void reset(void)
{
	memset_b(&process, 0, sizeof(process));
	memset_b(&region, 0, sizeof(region));
	memset_b(&stack_region, 0, sizeof(stack_region));
	memset_b(&trampoline_region, 0, sizeof(trampoline_region));
	memset_b(mapped_page, 0xa5, sizeof(mapped_page));
	region.start = 0x4000;
	region.end = 0x5000;
	region.prot = PROT_READ | PROT_WRITE;
	region.flags = MAP_PRIVATE | ZERO_PAGE;
	stack_region.start = PAGE_OFFSET - (2 * PAGE_SIZE);
	stack_region.end = PAGE_OFFSET - PAGE_SIZE;
	stack_region.prot = PROT_READ | PROT_WRITE;
	stack_region.flags = MAP_PRIVATE | ZERO_PAGE;
	stack_region.s_type = P_STACK;
	use_region = 1;
	use_stack = 0;
	copy_result = 0;
	map_calls = 0;
	last_map_address = 0;
}

int main(void)
{
	int result;

	reset();
	result = resolve_page_fault(0x4100, PFAULT_U | PFAULT_V, 0x4800);
	if(result != PFAULT_SIGSEGV || map_calls) {
		return 1;
	}

	reset();
	result = resolve_page_fault(0x4100,
		PFAULT_U | PFAULT_V | PFAULT_W, 0x4800);
	if(result != PFAULT_RESOLVED || map_calls) {
		return 2;
	}
	copy_result = -1;
	if(resolve_page_fault(0x4100, PFAULT_U | PFAULT_V | PFAULT_W,
		0x4800) != PFAULT_SIGSEGV) {
		return 3;
	}
	copy_result = 1;
	if(resolve_page_fault(0x4100, PFAULT_U | PFAULT_V | PFAULT_W,
		0x4800) != PFAULT_SIGKILL) {
		return 4;
	}

	reset();
	result = resolve_page_fault(0x4100, PFAULT_U, 0x4800);
	if(result != PFAULT_RESOLVED || map_calls != 1 ||
		last_map_address != 0x4100 || process.usage.ru_minflt != 1 ||
		mapped_page[0] || mapped_page[PAGE_SIZE - 1]) {
		return 5;
	}

	reset();
	use_region = 0;
	use_stack = 1;
	process.vma_table = &stack_region;
	stack_region.prev = &trampoline_region;
	stack_region.next = &trampoline_region;
	trampoline_region.start = PAGE_OFFSET - PAGE_SIZE;
	trampoline_region.end = PAGE_OFFSET;
	trampoline_region.s_type = P_TEXT;
	trampoline_region.prev = &stack_region;
	result = resolve_page_fault(PAGE_OFFSET - (2 * PAGE_SIZE) - 16,
		PFAULT_U, PAGE_OFFSET - (2 * PAGE_SIZE));
	if(result != PFAULT_RESOLVED || map_calls != 1 ||
		stack_region.start != PAGE_OFFSET - (3 * PAGE_SIZE)) {
		return 6;
	}

	reset();
	use_region = 0;
	if(resolve_page_fault(0x2000, PFAULT_U, 0x8000) != PFAULT_SIGSEGV) {
		return 7;
	}
	if(resolve_page_fault(0x2000, PFAULT_U, 16) != PFAULT_SIGSEGV) {
		return 8;
	}

	reset();
	if(resolve_page_fault(0x4100, 0, 0) != PFAULT_RESOLVED ||
		map_calls != 1) {
		return 9;
	}
	if(resolve_page_fault(0x4100, PFAULT_V, 0) != PFAULT_FATAL) {
		return 10;
	}

	reset();
	use_region = 0;
	if(resolve_page_fault(0x2000, 0, 0x2010) != PFAULT_SIGSEGV) {
		return 11;
	}
	if(resolve_page_fault(0x2000, PFAULT_V, 0x2010) != PFAULT_FATAL) {
		return 12;
	}

	return 0;
}
