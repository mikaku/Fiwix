/*
 * fiwix/include/fiwix/arm_linux.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_LINUX_H
#define _FIWIX_ARM_LINUX_H

#define ARM_LINUX_IMAGE_ADDRESS		0x42000000U
#define ARM_LINUX_IMAGE_CAPACITY	0x00800000U
#define ARM_LINUX_DTB_ADDRESS		0x48000000U
#define ARM_LINUX_DTB_CAPACITY		0x00200000U

int arm_linux_page_reserved(unsigned long);
int arm_linux_zimage_header_gate(const void *, unsigned int);
int arm_linux_prepare(void);
unsigned int arm_linux_image_entry(void);
unsigned int arm_linux_dtb_entry(void);

#endif /* _FIWIX_ARM_LINUX_H */
