/*
 * fiwix/tests/arm-linux-image.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_linux.h>

#define TEST_IMAGE_SIZE	64U
#define ZIMAGE_MAGIC	0x016F2818U

static void put32(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)(value >> 16);
	p[3] = (unsigned char)(value >> 24);
}

int main(void)
{
	unsigned char image[TEST_IMAGE_SIZE];
	unsigned int n;

	for(n = 0; n < TEST_IMAGE_SIZE; n++) {
		image[n] = 0;
	}
	put32(image + 0x24, ZIMAGE_MAGIC);
	put32(image + 0x28, 0);
	put32(image + 0x2c, TEST_IMAGE_SIZE);
	if(arm_linux_zimage_header_gate(image, TEST_IMAGE_SIZE) ||
		arm_linux_zimage_header_gate(0, TEST_IMAGE_SIZE) == 0 ||
		arm_linux_zimage_header_gate(image, 0x2f) == 0) {
		return 1;
	}
	image[0x24] ^= 1;
	if(arm_linux_zimage_header_gate(image, TEST_IMAGE_SIZE) == 0) {
		return 1;
	}
	image[0x24] ^= 1;
	put32(image + 0x28, 1);
	if(arm_linux_zimage_header_gate(image, TEST_IMAGE_SIZE) == 0) {
		return 1;
	}
	put32(image + 0x28, 0);
	put32(image + 0x2c, TEST_IMAGE_SIZE - 1);
	if(arm_linux_zimage_header_gate(image, TEST_IMAGE_SIZE) == 0) {
		return 1;
	}

	if(arm_linux_page_reserved(ARM_LINUX_IMAGE_ADDRESS - 1) ||
		!arm_linux_page_reserved(ARM_LINUX_IMAGE_ADDRESS) ||
		!arm_linux_page_reserved(ARM_LINUX_IMAGE_ADDRESS +
		ARM_LINUX_IMAGE_CAPACITY - 1) ||
		arm_linux_page_reserved(ARM_LINUX_IMAGE_ADDRESS +
		ARM_LINUX_IMAGE_CAPACITY) ||
		arm_linux_page_reserved(ARM_LINUX_DTB_ADDRESS - 1) ||
		!arm_linux_page_reserved(ARM_LINUX_DTB_ADDRESS) ||
		!arm_linux_page_reserved(ARM_LINUX_DTB_ADDRESS +
		ARM_LINUX_DTB_CAPACITY - 1) ||
		arm_linux_page_reserved(ARM_LINUX_DTB_ADDRESS +
		ARM_LINUX_DTB_CAPACITY)) {
		return 1;
	}
	return 0;
}
