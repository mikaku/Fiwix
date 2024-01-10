/*
 * fiwix/include/fiwix/sysconsole.h
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SYSCONSOLE_H
#define _FIWIX_SYSCONSOLE_H

#include <fiwix/config.h>
#include <fiwix/types.h>
#include <fiwix/tty.h>

struct sysconsole {
	__dev_t dev;
	struct tty *tty;
};

extern struct sysconsole sysconsole_table[NR_SYSCONSOLES];

int add_sysconsoledev(__dev_t);
void register_console(struct tty *);
void sysconsole_init(void);

#endif /* _FIWIX_SYSCONSOLE_H */
