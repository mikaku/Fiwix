#include <fiwix/riscv64_fdt.h>

#include <stdio.h>
#include <stdlib.h>

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
	if(riscv64_fdt_memory_pages(invalid, 0x80000000UL,
		0x80000000UL)) {
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	unsigned char *blob;
	unsigned long actual, expected;
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
