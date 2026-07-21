/*
 * fiwix/arch/riscv64/ext2_gate.c
 *
 * Read-only revision-0 ext2 lookup gate over the bring-up virtio queue.
 */

#define BLOCK_SIZE          1024UL
#define EXT2_MAGIC          0xef53U
#define ROOT_INODE          2UL
#define INODE_SIZE          128UL
#define INODE_TABLE_OFFSET  8UL
#define INODE_MODE_OFFSET   0UL
#define INODE_SIZE_OFFSET   4UL
#define INODE_BLOCK_OFFSET  40UL
#define MODE_TYPE_MASK      0xf000U
#define MODE_DIRECTORY      0x4000U
#define MODE_REGULAR        0x8000U

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

static u8 block_buffer[BLOCK_SIZE] __attribute__((aligned(16)));

extern int riscv64_virtio_read_sector(u64, void *);

static u16 get16(const u8 *p)
{
	return (u16)p[0] | ((u16)p[1] << 8);
}

static u32 get32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) |
		((u32)p[3] << 24);
}

static int read_block(u32 block)
{
	if(riscv64_virtio_read_sector((u64)block * 2, block_buffer) < 0 ||
		riscv64_virtio_read_sector((u64)block * 2 + 1,
			block_buffer + 512) < 0) {
		return -1;
	}
	return 0;
}

static int name_matches(const u8 *name, u8 length, const char *expected)
{
	u8 n;

	for(n = 0; n < length; n++) {
		if(!expected[n] || name[n] != (u8)expected[n]) {
			return 0;
		}
	}
	return expected[length] == 0;
}

static int read_inode(u32 table_block, u32 inode, u8 *result)
{
	u32 offset;
	u32 block;

	offset = (inode - 1) * INODE_SIZE;
	block = table_block + offset / BLOCK_SIZE;
	offset %= BLOCK_SIZE;
	if(read_block(block) < 0 || offset + INODE_SIZE > BLOCK_SIZE) {
		return -1;
	}
	for(block = 0; block < INODE_SIZE; block++) {
		result[block] = block_buffer[offset + block];
	}
	return 0;
}

static int load_root_directory(u32 *inode_table)
{
	u8 inode[INODE_SIZE];
	u32 root_block;

	if(read_block(1) < 0 || get16(block_buffer + 56) != EXT2_MAGIC ||
		get32(block_buffer + 24) != 0 || get32(block_buffer + 76) != 0 ||
		read_block(2) < 0) {
		return -1;
	}
	*inode_table = get32(block_buffer + INODE_TABLE_OFFSET);
	if(!*inode_table || read_inode(*inode_table, ROOT_INODE, inode) < 0 ||
		(get16(inode + INODE_MODE_OFFSET) & MODE_TYPE_MASK) !=
		MODE_DIRECTORY || get32(inode + INODE_SIZE_OFFSET) != BLOCK_SIZE) {
		return -1;
	}
	root_block = get32(inode + INODE_BLOCK_OFFSET);
	if(!root_block || read_block(root_block) < 0) {
		return -1;
	}
	return 0;
}

static int find_root_file(const char *name, u32 *inode)
{
	u32 offset;
	u16 record_length;
	u8 name_length;

	*inode = 0;
	for(offset = 0; offset + 8 <= BLOCK_SIZE; offset += record_length) {
		record_length = get16(block_buffer + offset + 4);
		name_length = block_buffer[offset + 6];
		if(record_length < 8 || record_length > BLOCK_SIZE - offset ||
			name_length > record_length - 8) {
			return -1;
		}
		if(name_matches(block_buffer + offset + 8, name_length, name)) {
			*inode = get32(block_buffer + offset);
			return *inode ? 0 : -1;
		}
	}
	return -1;
}

static u32 file_block(const u8 *inode, u32 logical)
{
	u32 indirect;
	u32 child;

	if(logical < 12) {
		return get32(inode + INODE_BLOCK_OFFSET + logical * 4);
	}
	logical -= 12;
	if(logical < 256) {
		indirect = get32(inode + INODE_BLOCK_OFFSET + 12 * 4);
		if(!indirect || read_block(indirect) < 0) {
			return 0;
		}
		return get32(block_buffer + logical * 4);
	}
	logical -= 256;
	if(logical >= 256 * 256) {
		return 0;
	}
	indirect = get32(inode + INODE_BLOCK_OFFSET + 13 * 4);
	if(!indirect || read_block(indirect) < 0) {
		return 0;
	}
	child = get32(block_buffer + (logical / 256) * 4);
	if(!child || read_block(child) < 0) {
		return 0;
	}
	return get32(block_buffer + (logical % 256) * 4);
}

int riscv64_ext2_gate(void)
{
	static const char expected[] =
		"Fiwix riscv64 ext2 file gate passed\n";
	u8 inode_data[INODE_SIZE];
	u32 inode_table;
	u32 file_inode;
	u32 file_block;
	u32 file_size;
	u64 n;

	if(load_root_directory(&inode_table) < 0 ||
		find_root_file("bootstrap", &file_inode) < 0 ||
		read_inode(inode_table, file_inode, inode_data) < 0 ||
		(get16(inode_data + INODE_MODE_OFFSET) & MODE_TYPE_MASK) !=
		MODE_REGULAR) {
		return -1;
	}
	file_size = get32(inode_data + INODE_SIZE_OFFSET);
	file_block = get32(inode_data + INODE_BLOCK_OFFSET);
	if(file_size != sizeof(expected) - 1 || !file_block ||
		read_block(file_block) < 0) {
		return -1;
	}
	for(n = 0; n < sizeof(expected) - 1; n++) {
		if(block_buffer[n] != (u8)expected[n]) {
			return -1;
		}
	}
	return 0;
}

int riscv64_ext2_load_file(const char *name, void *destination,
	u64 capacity, u64 *size)
{
	u8 inode_data[INODE_SIZE];
	u8 *output;
	u32 inode_table;
	u32 inode;
	u32 file_size;
	u32 block;
	u32 chunk;
	u32 copied;
	u32 n;

	if(!name || !destination || !size ||
		load_root_directory(&inode_table) < 0 ||
		find_root_file(name, &inode) < 0 ||
		read_inode(inode_table, inode, inode_data) < 0 ||
		(get16(inode_data + INODE_MODE_OFFSET) & MODE_TYPE_MASK) !=
		MODE_REGULAR) {
		return -1;
	}
	file_size = get32(inode_data + INODE_SIZE_OFFSET);
	if(!file_size || file_size > capacity) {
		return -1;
	}
	output = (u8 *)destination;
	for(copied = 0; copied < file_size; copied += chunk) {
		block = file_block(inode_data, copied / BLOCK_SIZE);
		if(!block || read_block(block) < 0) {
			return -1;
		}
		chunk = file_size - copied;
		if(chunk > BLOCK_SIZE) {
			chunk = BLOCK_SIZE;
		}
		for(n = 0; n < chunk; n++) {
			output[copied + n] = block_buffer[n];
		}
	}
	*size = file_size;
	return 0;
}
