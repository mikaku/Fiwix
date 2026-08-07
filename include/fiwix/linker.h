/*
 * fiwix/include/fiwix/linker.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_LINKER_H
#define _FIWIX_LINKER_H

#include <fiwix/config.h>

#ifdef CONFIG_ARCH_RISCV64
#define PAGE_OFFSET	0x0000004000000000UL /* top of the Sv39 user half */
#define KERNEL_ADDR	0x80000000UL
#define RISCV64_MEMORY_FALLBACK	0x10000000UL	/* 256 MiB without a DTB */
#define RISCV64_MEMORY_LIMIT	0x80000000UL	/* two Sv39 leaves: 2 GiB */
#define GDT_BASE	RISCV64_MEMORY_LIMIT
#elif defined(CONFIG_ARCH_ARM)
#define PAGE_OFFSET	0x40000000U	/* top of the ARMv7 user range */
#define KERNEL_ADDR	0x40010000U
#define ARM_MEMORY_FALLBACK	0x08000000U	/* 128 MiB without a DTB */
#define ARM_MEMORY_LIMIT	0x40000000U	/* 1 GiB identity-map gate */
#define GDT_BASE	0x50000000U
#elif defined(CONFIG_VM_SPLIT22)
#define PAGE_OFFSET	0x80000000	/* VM split: 2GB user / 2GB kernel */
#else
#define PAGE_OFFSET	0xC0000000	/* VM split: 3GB user / 1GB kernel */
#endif

#if !defined(CONFIG_ARCH_RISCV64) && !defined(CONFIG_ARCH_ARM)
#define KERNEL_ADDR	0x100000
#define GDT_BASE	(0xFFFFFFFF - (PAGE_OFFSET - 1))
#endif
#define KERNEL_STACK	4096

#endif /* _FIWIX_LINKER_H */
