/*
 * fiwix/arch/riscv64/linux.c
 *
 * RISC-V Linux Image header and load-address gate.
 */

#define IMAGE_CAPACITY      (8UL * 1024 * 1024)
#define IMAGE_TEXT_OFFSET   0x00200000UL
#define IMAGE_LOAD_ADDRESS  0x80200000UL
#define IMAGE_MAGIC         0x0000005643534952UL
#define IMAGE_MAGIC2        0x05435352U

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;

static u8 linux_image[IMAGE_CAPACITY]
	__attribute__((section(".linux_image"), aligned(0x200000)));

extern int riscv64_ext2_load_file(const char *, void *, u64, u64 *);

static u32 get32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) |
		((u32)p[3] << 24);
}

static u64 get64(const u8 *p)
{
	return (u64)get32(p) | ((u64)get32(p + 4) << 32);
}

int riscv64_linux_image_gate(void)
{
	u64 file_size;
	u64 image_size;
	u64 n;

	if((u64)linux_image != IMAGE_LOAD_ADDRESS ||
		riscv64_ext2_load_file("linux", linux_image,
		IMAGE_CAPACITY, &file_size) < 0 || file_size < 64) {
		return -1;
	}
	image_size = get64(linux_image + 16);
	if(get64(linux_image + 8) != IMAGE_TEXT_OFFSET || !image_size ||
		image_size > IMAGE_CAPACITY || get64(linux_image + 24) != 0 ||
		get32(linux_image + 32) != 2 ||
		get64(linux_image + 48) != IMAGE_MAGIC ||
		get32(linux_image + 56) != IMAGE_MAGIC2) {
		return -1;
	}
	for(n = file_size; n < image_size; n++) {
		linux_image[n] = 0;
	}
	return 0;
}

u64 riscv64_linux_image_entry(void)
{
	return (u64)linux_image;
}
