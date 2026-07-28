/*
 * fiwix/tests/arm-fdt.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_fdt.h>

#include <stdio.h>
#include <stdlib.h>

static void write_be32(unsigned char *data, unsigned int value)
{
	data[0] = (unsigned char)(value >> 24);
	data[1] = (unsigned char)(value >> 16);
	data[2] = (unsigned char)(value >> 8);
	data[3] = (unsigned char)value;
}

static int check_invalid_inputs(void)
{
	unsigned char invalid[40];
	struct arm_fdt_info info;
	int n;

	for(n = 0; n < (int)sizeof(invalid); n++) {
		invalid[n] = 0;
	}
	if(!arm_fdt_parse(NULL, 0x40000000U, 0x10000000U, &info) ||
		arm_fdt_size(NULL) ||
		!arm_fdt_parse(invalid, 0x40000000U, 0x10000000U, &info) ||
		arm_fdt_size(invalid)) {
		return 1;
	}
	write_be32(invalid, 0xD00DFEEDU);
	write_be32(invalid + 4, 39);
	if(arm_fdt_size(invalid)) {
		return 1;
	}
	write_be32(invalid + 4, (2U * 1024U * 1024U) + 1U);
	if(arm_fdt_size(invalid)) {
		return 1;
	}
	return 0;
}

static int check_reservation(unsigned char *blob, unsigned long size)
{
	unsigned long address;
	unsigned long first_page;
	unsigned long last_page;

	address = (unsigned long)blob;
	first_page = address & ~4095UL;
	last_page = (address + size - 1) & ~4095UL;
	arm_fdt_set_boot_blob(blob, first_page,
		(last_page - first_page) + 4096);
	if(!arm_boot_fdt_selected() ||
		!arm_boot_page_reserved(first_page) ||
		!arm_boot_page_reserved(last_page) ||
		arm_boot_page_reserved(first_page - 4096) ||
		arm_boot_page_reserved(last_page + 4096)) {
		return 1;
	}
	arm_fdt_set_boot_blob(blob, first_page, size - 1);
	return arm_boot_fdt_selected() ||
		arm_boot_page_reserved(first_page);
}

int main(int argc, char **argv)
{
	unsigned char *blob;
	struct arm_fdt_info info;
	unsigned int expected_pages;
	unsigned int expected_regions;
	unsigned int n;
	unsigned int size;
	long length;
	FILE *file;

	if(argc != 4 || check_invalid_inputs()) {
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
	expected_pages = (unsigned int)strtoul(argv[2], NULL, 0);
	expected_regions = (unsigned int)strtoul(argv[3], NULL, 0);
	size = arm_fdt_size(blob);
	if(size < 40 || size > (unsigned int)length ||
		arm_fdt_parse(blob, 0x40000000U, 0x10000000U, &info) ||
		info.memory_pages != expected_pages ||
		info.virtio_count != expected_regions ||
		check_reservation(blob, size)) {
		fprintf(stderr, "ARM DTB parse or reservation check failed\n");
		free(blob);
		return 1;
	}
	for(n = 0; n < info.virtio_count; n++) {
		if(info.virtio[n].address != 0x0A000000U + n * 0x200U ||
			info.virtio[n].size != 0x200U) {
			fprintf(stderr,
				"ARM DTB virtio region %u is %x+%x\n", n,
				info.virtio[n].address, info.virtio[n].size);
			free(blob);
			return 1;
		}
	}
	free(blob);
	return 0;
}
