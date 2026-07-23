#include <fiwix/riscv64_fdt.h>

#include <stdio.h>
#include <stdlib.h>

static void write_be32(unsigned char *data, unsigned long value)
{
	data[0] = (unsigned char)(value >> 24);
	data[1] = (unsigned char)(value >> 16);
	data[2] = (unsigned char)(value >> 8);
	data[3] = (unsigned char)value;
}

static int check_invalid_inputs(void)
{
	unsigned char invalid[40];
	int n;

	for(n = 0; n < (int)sizeof(invalid); n++) {
		invalid[n] = 0;
	}
	if(riscv64_fdt_memory_pages(NULL, 0x80000000UL,
		0x80000000UL)) {
		return 1;
	}
	if(riscv64_fdt_size(NULL)) {
		return 1;
	}
	if(riscv64_fdt_memory_pages(invalid, 0x80000000UL,
		0x80000000UL)) {
		return 1;
	}
	if(riscv64_fdt_size(invalid)) {
		return 1;
	}
	write_be32(invalid, 0xd00dfeedUL);
	write_be32(invalid + 4, 39);
	if(riscv64_fdt_size(invalid)) {
		return 1;
	}
	write_be32(invalid + 4, (2 * 1024 * 1024) + 1);
	if(riscv64_fdt_size(invalid)) {
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	unsigned char *blob;
	unsigned long actual, address, expected, first_page, last_page, size;
	long length;
	FILE *file;

	if(argc != 3 || check_invalid_inputs()) {
		return 1;
	}
	file = fopen(argv[1], "rb");
	if(!file || fseek(file, 0, SEEK_END)) {
		return 1;
	}
	length = ftell(file);
	if(length <= 0 || fseek(file, 0, SEEK_SET)) {
		return 1;
	}
	blob = (unsigned char *)malloc((size_t)length);
	if(!blob || fread(blob, 1, (size_t)length, file) != (size_t)length) {
		return 1;
	}
	fclose(file);
	expected = strtoul(argv[2], NULL, 0);
	size = riscv64_fdt_size(blob);
	if(size < 40 || size > (unsigned long)length) {
		fprintf(stderr, "invalid DTB total size: %lu\n", size);
		free(blob);
		return 1;
	}
	address = (unsigned long)blob;
	first_page = address & ~4095UL;
	last_page = (address + size - 1) & ~4095UL;
	riscv64_fdt_set_boot_blob(blob, first_page,
		(last_page - first_page) + 4096);
	if(!riscv64_boot_blob_selected() ||
		!riscv64_boot_page_reserved(first_page) ||
		!riscv64_boot_page_reserved(last_page) ||
		riscv64_boot_page_reserved(first_page - 4096) ||
		riscv64_boot_page_reserved(last_page + 4096)) {
		fprintf(stderr, "DTB page reservation range is incorrect\n");
		free(blob);
		return 1;
	}
	riscv64_fdt_set_boot_blob(blob, first_page, size - 1);
	if(riscv64_boot_blob_selected() ||
		riscv64_boot_page_reserved(first_page)) {
		fprintf(stderr, "rejected DTB retained a stale reservation\n");
		free(blob);
		return 1;
	}
	actual = riscv64_fdt_memory_pages(blob, 0x80000000UL,
		0x80000000UL);
	free(blob);
	if(actual != expected) {
		fprintf(stderr, "DTB memory pages: expected %lu, got %lu\n",
			expected, actual);
		return 1;
	}
	return 0;
}
