/*
 * fiwix/arch/arm/fdt.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_fdt.h>
#include <fiwix/mm.h>

#define FDT_MAGIC	0xD00DFEEDU
#define FDT_BEGIN_NODE	1U
#define FDT_END_NODE	2U
#define FDT_PROP	3U
#define FDT_NOP		4U
#define FDT_END		9U
#define FDT_HEADER_SIZE	40U
#define FDT_MAX_SIZE	(2U * 1024U * 1024U)

typedef unsigned long long arm_fdt_u64;

static unsigned long boot_blob_address;
static unsigned long boot_blob_size;
static struct arm_fdt_info boot_info;

static unsigned int read_be32(const unsigned char *data)
{
	return ((unsigned int)data[0] << 24) |
		((unsigned int)data[1] << 16) |
		((unsigned int)data[2] << 8) |
		(unsigned int)data[3];
}

static int range_valid(unsigned long offset, unsigned long length,
	unsigned long total)
{
	return offset <= total && length <= total - offset;
}

static int align4(unsigned long value, unsigned long *aligned)
{
	if(value > 0xFFFFFFFCUL) {
		return -1;
	}
	*aligned = (value + 3UL) & ~3UL;
	return 0;
}

static int bounded_string_equal(const unsigned char *text,
	unsigned long available, const char *expected)
{
	unsigned long n;

	for(n = 0; n < available; n++) {
		if(text[n] != (unsigned char)expected[n]) {
			return 0;
		}
		if(!expected[n]) {
			return 1;
		}
	}
	return 0;
}

static int string_list_contains(const unsigned char *list,
	unsigned long length, const char *expected)
{
	unsigned long available;
	unsigned long offset;
	unsigned long size;

	offset = 0;
	while(offset < length) {
		available = length - offset;
		for(size = 0; size < available && list[offset + size]; size++) {
			/* Find this bounded string's terminator. */
		}
		if(size == available) {
			return 0;
		}
		if(bounded_string_equal(list + offset, size + 1, expected)) {
			return 1;
		}
		offset += size + 1;
	}
	return 0;
}

static void clear_info(struct arm_fdt_info *info)
{
	unsigned int n;

	info->memory_pages = 0;
	info->virtio_count = 0;
	for(n = 0; n < ARM_FDT_MAX_VIRTIO_REGIONS; n++) {
		info->virtio[n].address = 0;
		info->virtio[n].size = 0;
	}
}

static void copy_info(struct arm_fdt_info *destination,
	const struct arm_fdt_info *source)
{
	unsigned int n;

	destination->memory_pages = source->memory_pages;
	destination->virtio_count = source->virtio_count;
	for(n = 0; n < ARM_FDT_MAX_VIRTIO_REGIONS; n++) {
		destination->virtio[n].address = source->virtio[n].address;
		destination->virtio[n].size = source->virtio[n].size;
	}
}

unsigned int arm_fdt_size(const void *blob)
{
	const unsigned char *bytes;
	unsigned int total;

	if(!blob) {
		return 0;
	}
	bytes = (const unsigned char *)blob;
	if(read_be32(bytes) != FDT_MAGIC) {
		return 0;
	}
	total = read_be32(bytes + 4);
	if(total < FDT_HEADER_SIZE || total > FDT_MAX_SIZE) {
		return 0;
	}
	return total;
}

void arm_fdt_set_boot_blob(const void *blob, unsigned long physical_base,
	unsigned long memory_limit)
{
	unsigned long address;
	unsigned long memory_end;
	unsigned long size;

	boot_blob_address = 0;
	boot_blob_size = 0;
	size = arm_fdt_size(blob);
	if(!size || size > memory_limit) {
		return;
	}
	address = (unsigned long)blob;
	memory_end = physical_base + memory_limit;
	if(memory_end < physical_base || address < physical_base ||
		address > memory_end - size) {
		return;
	}
	boot_blob_address = address;
	boot_blob_size = size;
}

int arm_boot_fdt_selected(void)
{
	return boot_blob_size != 0;
}

int arm_boot_page_reserved(unsigned long address)
{
	unsigned long blob_end;

	if(!boot_blob_size) {
		return 0;
	}
	blob_end = boot_blob_address + boot_blob_size;
	if(address >= blob_end) {
		return 0;
	}
	if(address >= boot_blob_address) {
		return 1;
	}
	return boot_blob_address - address < PAGE_SIZE;
}

static int memory_node_name(const unsigned char *name,
	unsigned long available)
{
	static const char prefix[] = "memory";
	unsigned long n;

	for(n = 0; n < sizeof(prefix) - 1; n++) {
		if(n >= available || name[n] != (unsigned char)prefix[n]) {
			return 0;
		}
	}
	return n < available && (!name[n] || name[n] == '@');
}

static arm_fdt_u64 read_cells(const unsigned char *data, int cells)
{
	arm_fdt_u64 value;
	int n;

	value = 0;
	for(n = 0; n < cells; n++) {
		value = (value << 32) | (arm_fdt_u64)read_be32(data + n * 4);
	}
	return value;
}

static unsigned int memory_reg_pages(const unsigned char *reg,
	unsigned long length, int address_cells, int size_cells,
	unsigned int physical_base, unsigned int memory_limit)
{
	arm_fdt_u64 address;
	arm_fdt_u64 base;
	arm_fdt_u64 end;
	arm_fdt_u64 limit_end;
	arm_fdt_u64 pages;
	arm_fdt_u64 size;
	unsigned int best;
	unsigned long offset;
	unsigned long tuple_size;

	if(address_cells < 1 || address_cells > 2 ||
		size_cells < 1 || size_cells > 2) {
		return 0;
	}
	tuple_size = (unsigned long)(address_cells + size_cells) * 4UL;
	base = physical_base;
	limit_end = base + memory_limit;
	best = 0;
	for(offset = 0; offset + tuple_size <= length; offset += tuple_size) {
		address = read_cells(reg + offset, address_cells);
		size = read_cells(reg + offset + address_cells * 4, size_cells);
		end = address + size;
		if(end < address) {
			end = ~(arm_fdt_u64)0;
		}
		if(address > base || end <= base) {
			continue;
		}
		if(end > limit_end) {
			end = limit_end;
		}
		pages = (end - base) >> PAGE_SHIFT;
		if(pages > best) {
			best = (unsigned int)pages;
		}
	}
	return best;
}

static void add_virtio_regions(struct arm_fdt_info *info,
	const unsigned char *reg, unsigned long length, int address_cells,
	int size_cells)
{
	arm_fdt_u64 address;
	arm_fdt_u64 end;
	arm_fdt_u64 size;
	unsigned int n;
	unsigned long offset;
	unsigned long tuple_size;

	if(address_cells < 1 || address_cells > 2 ||
		size_cells < 1 || size_cells > 2) {
		return;
	}
	tuple_size = (unsigned long)(address_cells + size_cells) * 4UL;
	for(offset = 0; offset + tuple_size <= length; offset += tuple_size) {
		if(info->virtio_count >= ARM_FDT_MAX_VIRTIO_REGIONS) {
			return;
		}
		address = read_cells(reg + offset, address_cells);
		size = read_cells(reg + offset + address_cells * 4, size_cells);
		end = address + size;
		if(!size || address > 0xFFFFFFFFULL || size > 0xFFFFFFFFULL ||
			end > 0x100000000ULL || end < address) {
			continue;
		}
		for(n = 0; n < info->virtio_count; n++) {
			if(info->virtio[n].address == (unsigned int)address) {
				break;
			}
		}
		if(n != info->virtio_count) {
			continue;
		}
		info->virtio[n].address = (unsigned int)address;
		info->virtio[n].size = (unsigned int)size;
		info->virtio_count++;
	}
}

int arm_fdt_parse(const void *blob, unsigned int physical_base,
	unsigned int memory_limit, struct arm_fdt_info *info)
{
	const unsigned char *bytes;
	const unsigned char *candidate_reg;
	const unsigned char *node_name;
	const unsigned char *property;
	const unsigned char *strings;
	const unsigned char *structure;
	unsigned int candidate_pages;
	unsigned int strings_offset;
	unsigned int strings_size;
	unsigned int structure_offset;
	unsigned int structure_size;
	unsigned int total;
	unsigned long aligned;
	unsigned long candidate_reg_length;
	unsigned long cursor;
	unsigned long length;
	unsigned long name_offset;
	unsigned long node_available;
	unsigned long token;
	int address_cells;
	int candidate_memory;
	int candidate_virtio;
	int depth;
	int size_cells;

	if(!info || !memory_limit) {
		return -1;
	}
	clear_info(info);
	bytes = (const unsigned char *)blob;
	total = arm_fdt_size(blob);
	if(!total) {
		return -1;
	}
	structure_offset = read_be32(bytes + 8);
	strings_offset = read_be32(bytes + 12);
	strings_size = read_be32(bytes + 32);
	structure_size = read_be32(bytes + 36);
	if(!range_valid(structure_offset, structure_size, total) ||
		!range_valid(strings_offset, strings_size, total)) {
		return -1;
	}
	structure = bytes + structure_offset;
	strings = bytes + strings_offset;
	cursor = 0;
	depth = -1;
	address_cells = 2;
	size_cells = 1;
	candidate_memory = 0;
	candidate_virtio = 0;
	candidate_reg = 0;
	candidate_reg_length = 0;

	while(range_valid(cursor, 4, structure_size)) {
		token = read_be32(structure + cursor);
		cursor += 4;
		if(token == FDT_BEGIN_NODE) {
			node_name = structure + cursor;
			node_available = structure_size - cursor;
			for(length = 0; length < node_available && node_name[length];
				length++) {
				/* Locate the bounded node-name terminator. */
			}
			if(length == node_available || align4(length + 1, &aligned) ||
				!range_valid(cursor, aligned, structure_size)) {
				clear_info(info);
				return -1;
			}
			depth++;
			if(depth == 1) {
				candidate_memory = memory_node_name(node_name,
					length + 1);
				candidate_virtio = 0;
				candidate_reg = 0;
				candidate_reg_length = 0;
			}
			cursor += aligned;
		} else if(token == FDT_END_NODE) {
			if(depth == 1 && candidate_reg) {
				if(candidate_memory) {
					candidate_pages = memory_reg_pages(candidate_reg,
						candidate_reg_length, address_cells,
						size_cells, physical_base, memory_limit);
					if(candidate_pages > info->memory_pages) {
						info->memory_pages = candidate_pages;
					}
				}
				if(candidate_virtio) {
					add_virtio_regions(info, candidate_reg,
						candidate_reg_length, address_cells,
						size_cells);
				}
			}
			if(depth < 0) {
				clear_info(info);
				return -1;
			}
			depth--;
		} else if(token == FDT_PROP) {
			if(!range_valid(cursor, 8, structure_size)) {
				clear_info(info);
				return -1;
			}
			length = read_be32(structure + cursor);
			name_offset = read_be32(structure + cursor + 4);
			cursor += 8;
			if(align4(length, &aligned) ||
				!range_valid(cursor, aligned, structure_size) ||
				name_offset >= strings_size) {
				clear_info(info);
				return -1;
			}
			property = structure + cursor;
			if(depth == 0 && length == 4 &&
				bounded_string_equal(strings + name_offset,
					strings_size - name_offset, "#address-cells")) {
				address_cells = (int)read_be32(property);
			} else if(depth == 0 && length == 4 &&
				bounded_string_equal(strings + name_offset,
					strings_size - name_offset, "#size-cells")) {
				size_cells = (int)read_be32(property);
			} else if(depth == 1 &&
				bounded_string_equal(strings + name_offset,
					strings_size - name_offset, "device_type") &&
				bounded_string_equal(property, length, "memory")) {
				candidate_memory = 1;
			} else if(depth == 1 &&
				bounded_string_equal(strings + name_offset,
					strings_size - name_offset, "compatible") &&
				string_list_contains(property, length,
					"virtio,mmio")) {
				candidate_virtio = 1;
			} else if(depth == 1 &&
				bounded_string_equal(strings + name_offset,
					strings_size - name_offset, "reg")) {
				candidate_reg = property;
				candidate_reg_length = length;
			}
			cursor += aligned;
		} else if(token == FDT_NOP) {
			continue;
		} else if(token == FDT_END) {
			if(depth == -1) {
				return 0;
			}
			clear_info(info);
			return -1;
		} else {
			clear_info(info);
			return -1;
		}
	}
	clear_info(info);
	return -1;
}

unsigned int arm_fdt_boot_discover(const void *blob,
	unsigned int physical_base, unsigned int memory_limit)
{
	struct arm_fdt_info discovered;
	unsigned long selected_memory;

	clear_info(&boot_info);
	arm_fdt_set_boot_blob(0, 0, 0);
	if(arm_fdt_parse(blob, physical_base, memory_limit, &discovered) ||
		!discovered.memory_pages) {
		return 0;
	}
	copy_info(&boot_info, &discovered);
	selected_memory = (unsigned long)discovered.memory_pages << PAGE_SHIFT;
	arm_fdt_set_boot_blob(blob, physical_base, selected_memory);
	return discovered.memory_pages;
}

const struct arm_fdt_info *arm_boot_fdt_info(void)
{
	return boot_info.memory_pages ? &boot_info : 0;
}
