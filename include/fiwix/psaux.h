/*
 * fiwix/include/fiwix/psaux.h
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_PSAUX

#ifndef _FIWIX_PSAUX_H
#define _FIWIX_PSAUX_H

#include <fiwix/fs.h>
#include <fiwix/charq.h>
#include <fiwix/sigcontext.h>

#define PSAUX_IRQ	12

#define PSAUX_MAJOR	10	/* major number for /dev/psaux */
#define PSAUX_MINOR	1	/* minor number for /dev/psaux */

struct psaux {
	int count;
	struct clist read_q;
	struct clist write_q;
};
extern struct psaux psaux_table;

int psaux_open(struct inode *, struct fd *);
int psaux_close(struct inode *, struct fd *);
int psaux_read(struct inode *, struct fd *, char *, __size_t);
int psaux_write(struct inode *, struct fd *, const char *, __size_t);
int psaux_select(struct inode *, int);

void irq_psaux(int num, struct sigcontext *);
void psaux_init(void);

#endif /* _FIWIX_PSAUX_H */

#endif /* CONFIG_PSAUX */
