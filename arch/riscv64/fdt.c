/* Minimal flattened-device-tree memory discovery for early RV64 boot. */

#include <fiwix/mm.h>

#define FDT_MAGIC	0xd00dfeedUL
#define FDT_BEGIN_NODE	1
#define FDT_END_NODE	2
#define FDT_PROP	3
#define FDT_NOP	4
#define FDT_END	9
#define FDT_HEADER_SIZE	40
#define FDT_MAX_SIZE	(2 * 1024 * 1024)

static unsigned long read_be32(const unsigned char *data)
{
	return ((unsigned long)data[0] << 24) |
		((unsigned long)data[1] << 16) |
		((unsigned long)data[2] << 8) |
		(unsigned long)data[3];
}

static int range_valid(unsigned long offset, unsigned long length,
	unsigned long total)
{
	return offset <= total && length <= total - offset;
}

static unsigned long align4(unsigned long value)
{
	return (value + 3) & ~3UL;
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

static unsigned long read_cells(const unsigned char *data, int cells)
{
	unsigned long value;
	int n;

	value = 0;
	for(n = 0; n < cells; n++) {
		value = (value << 32) | read_be32(data + n * 4);
	}
	return value;
}

static unsigned long memory_reg_pages(const unsigned char *reg,
	unsigned long length, int address_cells, int size_cells,
	unsigned long physical_base, unsigned long memory_limit)
{
	unsigned long address, size, end, limit_end, pages, best;
	unsigned long tuple_size, offset;

	if(address_cells < 1 || address_cells > 2 ||
		size_cells < 1 || size_cells > 2) {
		return 0;
	}
	tuple_size = (address_cells + size_cells) * 4;
	best = 0;
	for(offset = 0; offset + tuple_size <= length; offset += tuple_size) {
		address = read_cells(reg + offset, address_cells);
		size = read_cells(reg + offset + address_cells * 4, size_cells);
		end = address + size;
		if(end < address) {
			end = 0xffffffffffffffffUL;
		}
		limit_end = physical_base + memory_limit;
		if(limit_end < physical_base) {
			limit_end = 0xffffffffffffffffUL;
		}
		if(address > physical_base || end <= physical_base) {
			continue;
		}
		if(end > limit_end) {
			end = limit_end;
		}
		pages = (end - physical_base) >> PAGE_SHIFT;
		if(pages > best) {
			best = pages;
		}
	}
	return best;
}

unsigned long riscv64_fdt_memory_pages(const void *blob,
	unsigned long physical_base, unsigned long memory_limit)
{
	const unsigned char *bytes, *structure, *strings, *node_name;
	const unsigned char *property, *candidate_reg;
	unsigned long total, structure_offset, structure_size;
	unsigned long strings_offset, strings_size, cursor, token, length;
	unsigned long name_offset, node_available, candidate_reg_length;
	unsigned long pages, candidate_pages;
	int depth, address_cells, size_cells, candidate_memory;

	if(!blob || !memory_limit) {
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
	structure_offset = read_be32(bytes + 8);
	strings_offset = read_be32(bytes + 12);
	strings_size = read_be32(bytes + 32);
	structure_size = read_be32(bytes + 36);
	if(!range_valid(structure_offset, structure_size, total) ||
		!range_valid(strings_offset, strings_size, total)) {
		return 0;
	}
	structure = bytes + structure_offset;
	strings = bytes + strings_offset;
	cursor = 0;
	depth = -1;
	address_cells = 2;
	size_cells = 1;
	candidate_memory = 0;
	candidate_reg = 0;
	candidate_reg_length = 0;
	pages = 0;

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
			if(length == node_available ||
				!range_valid(cursor, align4(length + 1), structure_size)) {
				return 0;
			}
			depth++;
			if(depth == 1) {
				candidate_memory = memory_node_name(node_name,
					length + 1);
				candidate_reg = 0;
				candidate_reg_length = 0;
			}
			cursor += align4(length + 1);
		} else if(token == FDT_END_NODE) {
			if(depth == 1 && candidate_memory && candidate_reg) {
				candidate_pages = memory_reg_pages(candidate_reg,
					candidate_reg_length, address_cells, size_cells,
					physical_base, memory_limit);
				if(candidate_pages > pages) {
					pages = candidate_pages;
				}
			}
			if(depth < 0) {
				return 0;
			}
			depth--;
		} else if(token == FDT_PROP) {
			if(!range_valid(cursor, 8, structure_size)) {
				return 0;
			}
			length = read_be32(structure + cursor);
			name_offset = read_be32(structure + cursor + 4);
			cursor += 8;
			if(!range_valid(cursor, align4(length), structure_size) ||
				name_offset >= strings_size) {
				return 0;
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
					strings_size - name_offset, "reg")) {
				candidate_reg = property;
				candidate_reg_length = length;
			}
			cursor += align4(length);
		} else if(token == FDT_NOP) {
			continue;
		} else if(token == FDT_END) {
			return depth == -1 ? pages : 0;
		} else {
			return 0;
		}
	}
	return 0;
}
