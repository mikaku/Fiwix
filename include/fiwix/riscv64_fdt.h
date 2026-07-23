/* Fiwix riscv64 flattened-device-tree memory discovery. */

#ifndef _FIWIX_RISCV64_FDT_H
#define _FIWIX_RISCV64_FDT_H

unsigned long riscv64_fdt_memory_pages(const void *, unsigned long,
	unsigned long);
unsigned long riscv64_boot_memory_pages(void);

#endif /* _FIWIX_RISCV64_FDT_H */
