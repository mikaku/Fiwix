/*
 * fiwix/include/fiwix/charq.h
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CHARQ_H
#define _FIWIX_CHARQ_H

#define CBSIZE		32	/* number of characters in cblock */
#define NR_CB_QUEUE	8	/* number of cblocks per queue */

#define LAST_CHAR(q)	((q)->tail ? (q)->tail->data[(q)->tail->end_off - 1] : '\0')

struct clist {
	unsigned short int count;
	unsigned short int cb_num;
	struct cblock *head;
	struct cblock *tail;
};

struct cblock {
	unsigned short int start_off;
	unsigned short int end_off;
	unsigned char data[CBSIZE];
	struct cblock *prev;
	struct cblock *next;
};

int charq_putchar(struct clist *, unsigned char);
int charq_unputchar(struct clist *);
unsigned char charq_getchar(struct clist *);
void charq_flush(struct clist *);
int charq_room(struct clist *q);
void charq_init(void);

#endif /* _FIWIX_CHARQ_H */
