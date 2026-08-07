/* Build a deterministic 1 KiB-block, revision-0 ext2 smoke image. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE          1024U
#define BLOCK_COUNT         8192U
#define INODE_COUNT         128U
#define BLOCK_BITMAP_BLOCK  3U
#define INODE_BITMAP_BLOCK  4U
#define INODE_TABLE_BLOCK   5U
#define ROOT_DIR_BLOCK      21U
#define FILE_DATA_BLOCK     22U
#define SBIN_DIR_BLOCK      23U
#define DEV_DIR_BLOCK       24U
#define INIT_DATA_FIRST     25U
#define ROOT_INODE          2U
#define RESERVED_INODES     10U
#define FILE_INODE          12U
#define LINUX_INODE         13U
#define SBIN_INODE          14U
#define DEV_INODE           15U
#define INIT_INODE          16U
#define CONSOLE_INODE       17U
#define INODE_SIZE          128U

static void put16(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
}

static void put32(unsigned char *p, unsigned long value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)(value >> 16);
	p[3] = (unsigned char)(value >> 24);
}

static void mark_bits(unsigned char *bitmap, unsigned int count)
{
	unsigned int n;

	for(n = 0; n < count; n++) {
		bitmap[n / 8] |= (unsigned char)(1U << (n % 8));
	}
}

static void mark_bit(unsigned char *bitmap, unsigned int bit)
{
	bitmap[bit / 8] |= (unsigned char)(1U << (bit % 8));
}

static void mark_padding(unsigned char *bitmap, unsigned int valid_bits)
{
	unsigned int n;

	for(n = valid_bits; n < BLOCK_SIZE * 8; n++) {
		mark_bit(bitmap, n);
	}
}

static void make_inode(unsigned char *inode, unsigned int mode,
	unsigned int size, unsigned int links, unsigned int block)
{
	put16(inode + 0, mode);
	put32(inode + 4, size);
	put16(inode + 26, links);
	put32(inode + 28, 2);
	put32(inode + 40, block);
}

static unsigned int minimum(unsigned int a, unsigned int b)
{
	return a < b ? a : b;
}

static void make_dirent(unsigned char *entry, unsigned int inode,
	unsigned int record_length, const char *name)
{
	unsigned int length;

	length = (unsigned int)strlen(name);
	put32(entry + 0, inode);
	put16(entry + 4, record_length);
	entry[6] = (unsigned char)length;
	entry[7] = 0;
	memcpy(entry + 8, name, length);
}

int main(int argc, char **argv)
{
	static const char sector_marker[] =
		"Fiwix riscv64 virtio sector gate\n";
	static const char file_marker[] =
		"Fiwix riscv64 ext2 file gate passed\n";
	unsigned char *image;
	unsigned char *super;
	unsigned char *group;
	unsigned char *inodes;
	unsigned char *directory;
	unsigned char *sbin_directory;
	unsigned char *dev_directory;
	unsigned char *init_inode;
	unsigned char *linux_inode;
	FILE *output;
	FILE *linux_input;
	FILE *init_input;
	long linux_size;
	long init_size;
	unsigned int init_blocks;
	unsigned int linux_blocks;
	unsigned int single_block;
	unsigned int double_block;
	unsigned int double_children;
	unsigned int child_first;
	unsigned int data_first;
	unsigned int data_block;
	unsigned int logical;
	unsigned int child;
	unsigned int slot;
	unsigned int metadata_blocks;
	unsigned int last_block;
	unsigned int free_blocks;
	size_t written;

	if(argc != 4) {
		fprintf(stderr, "usage: %s OUTPUT LINUX_IMAGE INIT_IMAGE\n",
			argv[0]);
		return 2;
	}
	image = (unsigned char *)calloc(BLOCK_COUNT, BLOCK_SIZE);
	if(!image) {
		fprintf(stderr, "unable to allocate disk image\n");
		return 2;
	}
	memcpy(image, sector_marker, sizeof(sector_marker) - 1);

	super = image + BLOCK_SIZE;
	put32(super + 0, INODE_COUNT);
	put32(super + 4, BLOCK_COUNT);
	put32(super + 16, INODE_COUNT - RESERVED_INODES - 6);
	put32(super + 20, 1);
	put32(super + 24, 0);
	put32(super + 28, 0);
	put32(super + 32, 8192);
	put32(super + 36, 8192);
	put32(super + 40, INODE_COUNT);
	put16(super + 52, 0);
	put16(super + 54, 0xffff);
	put16(super + 56, 0xef53);
	put16(super + 58, 1);
	put16(super + 60, 1);
	put32(super + 72, 0);
	put32(super + 76, 0);

	group = image + 2 * BLOCK_SIZE;
	put32(group + 0, BLOCK_BITMAP_BLOCK);
	put32(group + 4, INODE_BITMAP_BLOCK);
	put32(group + 8, INODE_TABLE_BLOCK);
	put16(group + 14, INODE_COUNT - RESERVED_INODES - 6);
	put16(group + 16, 3);
	mark_bits(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, RESERVED_INODES);
	mark_bit(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, FILE_INODE - 1);
	mark_bit(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, LINUX_INODE - 1);
	mark_bit(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, SBIN_INODE - 1);
	mark_bit(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, DEV_INODE - 1);
	mark_bit(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, INIT_INODE - 1);
	mark_bit(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, CONSOLE_INODE - 1);
	mark_padding(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, INODE_COUNT);

	inodes = image + INODE_TABLE_BLOCK * BLOCK_SIZE;
	make_inode(inodes + (ROOT_INODE - 1) * INODE_SIZE,
		0x41ed, BLOCK_SIZE, 4, ROOT_DIR_BLOCK);
	make_inode(inodes + (FILE_INODE - 1) * INODE_SIZE,
		0x81a4, sizeof(file_marker) - 1, 1, FILE_DATA_BLOCK);
	make_inode(inodes + (SBIN_INODE - 1) * INODE_SIZE,
		0x41ed, BLOCK_SIZE, 2, SBIN_DIR_BLOCK);
	make_inode(inodes + (DEV_INODE - 1) * INODE_SIZE,
		0x41ed, BLOCK_SIZE, 2, DEV_DIR_BLOCK);
	make_inode(inodes + (CONSOLE_INODE - 1) * INODE_SIZE,
		0x21b6, 0, 1, 0);
	put32(inodes + (CONSOLE_INODE - 1) * INODE_SIZE + 28, 0);
	put32(inodes + (CONSOLE_INODE - 1) * INODE_SIZE + 40, 0x501);

	init_input = fopen(argv[3], "rb");
	if(!init_input || fseek(init_input, 0, SEEK_END)) {
		fprintf(stderr, "unable to read %s\n", argv[3]);
		free(image);
		return 2;
	}
	init_size = ftell(init_input);
	if(init_size <= 0 || fseek(init_input, 0, SEEK_SET)) {
		fprintf(stderr, "invalid init fixture size in %s\n", argv[3]);
		fclose(init_input);
		free(image);
		return 2;
	}
	init_blocks = ((unsigned long)init_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	if(init_blocks > 12) {
		fprintf(stderr, "init fixture is too large for direct ext2 blocks\n");
		fclose(init_input);
		free(image);
		return 2;
	}
	if(fread(image + INIT_DATA_FIRST * BLOCK_SIZE, 1,
		(size_t)init_size, init_input) != (size_t)init_size ||
		fclose(init_input)) {
		fprintf(stderr, "unable to copy %s\n", argv[3]);
		free(image);
		return 2;
	}
	init_inode = inodes + (INIT_INODE - 1) * INODE_SIZE;
	make_inode(init_inode, 0x81ed, (unsigned int)init_size, 1,
		INIT_DATA_FIRST);
	put32(init_inode + 28, init_blocks * 2);
	for(logical = 0; logical < init_blocks; logical++) {
		put32(init_inode + 40 + logical * 4, INIT_DATA_FIRST + logical);
	}

	linux_input = fopen(argv[2], "rb");
	if(!linux_input || fseek(linux_input, 0, SEEK_END)) {
		fprintf(stderr, "unable to read %s\n", argv[2]);
		free(image);
		return 2;
	}
	linux_size = ftell(linux_input);
	if(linux_size <= 0 ||
		fseek(linux_input, 0, SEEK_SET)) {
		fprintf(stderr, "invalid Linux fixture size in %s\n", argv[2]);
		fclose(linux_input);
		free(image);
		return 2;
	}
	linux_blocks = ((unsigned long)linux_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	single_block = 0;
	double_block = 0;
	double_children = 0;
	data_first = INIT_DATA_FIRST + init_blocks;
	if(linux_blocks > 12) {
		single_block = data_first++;
	}
	if(linux_blocks > 12 + 256) {
		double_block = data_first++;
		double_children = (linux_blocks - 12 - 256 + 255) / 256;
	}
	child_first = data_first;
	data_first += double_children;
	if(data_first >= BLOCK_COUNT || linux_blocks > BLOCK_COUNT - data_first) {
		fprintf(stderr, "Linux fixture is too large for the ext2 image\n");
		fclose(linux_input);
		free(image);
		return 2;
	}
	if(fread(image + data_first * BLOCK_SIZE, 1,
		(size_t)linux_size, linux_input) != (size_t)linux_size ||
		fclose(linux_input)) {
		fprintf(stderr, "unable to copy %s\n", argv[2]);
		free(image);
		return 2;
	}
	linux_inode = inodes + (LINUX_INODE - 1) * INODE_SIZE;
	make_inode(linux_inode, 0x81a4, (unsigned int)linux_size, 1,
		data_first);
	for(logical = 0; logical < minimum(linux_blocks, 12); logical++) {
		put32(linux_inode + 40 + logical * 4, data_first + logical);
	}
	if(single_block) {
		put32(linux_inode + 40 + 12 * 4, single_block);
		for(logical = 12; logical < minimum(linux_blocks, 12 + 256);
			logical++) {
			put32(image + single_block * BLOCK_SIZE +
				(logical - 12) * 4, data_first + logical);
		}
	}
	if(double_block) {
		put32(linux_inode + 40 + 13 * 4, double_block);
		for(child = 0; child < double_children; child++) {
			put32(image + double_block * BLOCK_SIZE + child * 4,
				child_first + child);
			for(slot = 0; slot < 256; slot++) {
				logical = 12 + 256 + child * 256 + slot;
				if(logical >= linux_blocks) {
					break;
				}
				data_block = data_first + logical;
				put32(image + (child_first + child) * BLOCK_SIZE +
					slot * 4, data_block);
			}
		}
	}
	metadata_blocks = (single_block ? 1 : 0) +
		(double_block ? 1 + double_children : 0);
	put32(linux_inode + 28, (linux_blocks + metadata_blocks) * 2);
	last_block = data_first + linux_blocks - 1;
	free_blocks = BLOCK_COUNT - (last_block + 1);
	put32(super + 12, free_blocks);
	put16(group + 12, free_blocks);
	mark_bits(image + BLOCK_BITMAP_BLOCK * BLOCK_SIZE, last_block);
	mark_padding(image + BLOCK_BITMAP_BLOCK * BLOCK_SIZE, BLOCK_COUNT - 1);

	directory = image + ROOT_DIR_BLOCK * BLOCK_SIZE;
	make_dirent(directory, ROOT_INODE, 12, ".");
	make_dirent(directory + 12, ROOT_INODE, 12, "..");
	make_dirent(directory + 24, FILE_INODE, 20,
		"bootstrap");
	make_dirent(directory + 44, LINUX_INODE, 16, "linux");
	make_dirent(directory + 60, SBIN_INODE, 12, "sbin");
	make_dirent(directory + 72, DEV_INODE, BLOCK_SIZE - 72, "dev");
	sbin_directory = image + SBIN_DIR_BLOCK * BLOCK_SIZE;
	make_dirent(sbin_directory, SBIN_INODE, 12, ".");
	make_dirent(sbin_directory + 12, ROOT_INODE, 12, "..");
	make_dirent(sbin_directory + 24, INIT_INODE, BLOCK_SIZE - 24,
		"init");
	dev_directory = image + DEV_DIR_BLOCK * BLOCK_SIZE;
	make_dirent(dev_directory, DEV_INODE, 12, ".");
	make_dirent(dev_directory + 12, ROOT_INODE, 12, "..");
	make_dirent(dev_directory + 24, CONSOLE_INODE, BLOCK_SIZE - 24,
		"console");
	memcpy(image + FILE_DATA_BLOCK * BLOCK_SIZE, file_marker,
		sizeof(file_marker) - 1);

	output = fopen(argv[1], "wb");
	if(!output) {
		perror(argv[1]);
		free(image);
		return 2;
	}
	written = fwrite(image, BLOCK_SIZE, BLOCK_COUNT, output);
	if(written != BLOCK_COUNT || fclose(output)) {
		fprintf(stderr, "unable to write %s\n", argv[1]);
		free(image);
		return 2;
	}
	free(image);
	return 0;
}
