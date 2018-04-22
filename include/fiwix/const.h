/*
 * fiwix/include/fiwix/const.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CONST_H
#define _FIWIX_CONST_H

#define KERNEL_BASE_ADDR	0xC0000000
#define KERNEL_ENTRY_ADDR	0x100000

#define KERNEL_CS	0x08	/* kernel code segment */
#define KERNEL_DS	0x10	/* kernel data segment */
#define USER_CS		0x18	/* user code segment */
#define USER_DS		0x20	/* user data segment */
#define TSS		0x28	/* TSS segment */

#endif /* _FIWIX_CONST_H */
