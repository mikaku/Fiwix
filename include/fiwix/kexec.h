/*
 * fiwix/include/fiwix/kexec.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_KEXEC

#ifndef _FIWIX_KEXEC_H
#define _FIWIX_KEXEC_H

#define KEXEC_MULTIBOOT1	0x01
#define KEXEC_LINUX		0x02

#define KEXEC_BOOT_ADDR		0x9D000

void kexec_multiboot1(void);
void kexec_linux(void);

#endif /* _FIWIX_KEXEC_H */

#endif /* CONFIG_KEXEC */
