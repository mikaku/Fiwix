/*
 * fiwix/include/fiwix/reboot.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_REBOOT_H
#define _FIWIX_REBOOT_H

#define BMAGIC_HARD	0x89ABCDEF
#define BMAGIC_SOFT	0
#define BMAGIC_REBOOT	0x01234567
#define BMAGIC_HALT	0xCDEF0123
#define BMAGIC_POWEROFF	0x4321FEDC

#define BMAGIC_1	0xFEE1DEAD
#define BMAGIC_2	672274793

extern char ctrl_alt_del;
void reboot(void);

#endif /* _FIWIX_REBOOT_H */
