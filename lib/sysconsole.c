/*
 * fiwix/lib/sysconsole.c
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/sysconsole.h>
#include <fiwix/tty.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct sysconsole sysconsole_table[NR_SYSCONSOLES];

int add_sysconsoledev(__dev_t dev)
{
	int n;

	for(n = 0; n < NR_SYSCONSOLES; n++) {
		if(!sysconsole_table[n].dev) {
			sysconsole_table[n].dev = dev;
			return 1;
		}
	}

	return 0;
}

void register_console(struct tty *tty)
{
	int n;

	for(n = 0; n < NR_SYSCONSOLES; n++) {
		if(sysconsole_table[n].dev == tty->dev) {
			sysconsole_table[n].tty = tty;
			break;
		}
	}
}

void sysconsole_init(void)
{
	memset_b(sysconsole_table, 0, sizeof(sysconsole_table));
}
