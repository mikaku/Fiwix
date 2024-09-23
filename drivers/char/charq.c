/*
 * fiwix/drivers/char/charq.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/charq.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

/*
static struct cblock *insert_cblock_in_head(struct clist *q)
{
	struct cblock *cb;

	if(q->cb_num >= NR_CB_QUEUE) {
		return NULL;
	}
	if(!(cb = (struct cblock *)kmalloc(sizeof(struct cblock)))) {
		return NULL;
	}

	memset_b(cb, 0, sizeof(struct cblock));
	q->cb_num++;

	if(!q->head) {
		q->head = q->tail = cb;
	} else {
		cb->prev = NULL;
		cb->next = q->head;
		q->head->prev = cb;
		q->head = cb;
	}
	return cb;
}
*/

static struct cblock *insert_cblock_in_tail(struct clist *q)
{
	struct cblock *cb;

	if(q->cb_num >= NR_CB_QUEUE) {
		return NULL;
	}
	if(!(cb = (struct cblock *)kmalloc(sizeof(struct cblock)))) {
		return NULL;
	}

	memset_b(cb, 0, sizeof(struct cblock));
	q->cb_num++;

	if(!q->tail) {
		q->head = q->tail = cb;
	} else {
		cb->prev = q->tail;
		cb->next = NULL;
		q->tail->next = cb;
		q->tail = cb;
	}
	return cb;
}

static void delete_cblock_from_head(struct clist *q)
{
	struct cblock *tmp;

	if(!q->head) {
		return;
	}

	tmp = q->head;
	if(q->head == q->tail) {
		q->head = q->tail = NULL;
	} else {
		q->head = q->head->next;
		q->head->prev = NULL;
	}

	q->count -= tmp->end_off - tmp->start_off;
	q->cb_num--;
	kfree((unsigned int)tmp);
}

static void delete_cblock_from_tail(struct clist *q)
{
	struct cblock *tmp;

	if(!q->tail) {
		return;
	}

	tmp = q->tail;
	if(q->head == q->tail) {
		q->head = q->tail = NULL;
	} else {
		q->tail = q->tail->prev;
		q->tail->next = NULL;
	}

	q->count -= tmp->end_off - tmp->start_off;
	q->cb_num--;
	kfree((unsigned int)tmp);
}

int charq_putchar(struct clist *q, unsigned char ch)
{
	unsigned int flags;
	struct cblock *cb;
	int errno;

	SAVE_FLAGS(flags); CLI();

	cb = q->tail;
	if(!cb) {
		cb = insert_cblock_in_tail(q);
		if(!cb) {
			RESTORE_FLAGS(flags);
			return -EAGAIN;
		}
	}

	if(cb->end_off < CBSIZE) {
		cb->data[cb->end_off] = ch;
		cb->end_off++;
		q->count++;
		errno = 0;
	} else if(insert_cblock_in_tail(q)) {
		charq_putchar(q, ch);
		errno = 0;
	} else {
		errno = -EAGAIN;
	}

	RESTORE_FLAGS(flags);
	return errno;
}

int charq_unputchar(struct clist *q)
{
	unsigned int flags;
	struct cblock *cb;
	unsigned char ch;

	SAVE_FLAGS(flags); CLI();

	ch = 0;
	cb = q->tail;
	if(cb) {
		if(cb->end_off > cb->start_off) {
			ch = cb->data[cb->end_off - 1];
			cb->end_off--;
			q->count--;
		}
		if(cb->end_off - cb->start_off == 0) {
			delete_cblock_from_tail(q);
		}
	}

	RESTORE_FLAGS(flags);
	return ch;
}

unsigned char charq_getchar(struct clist *q)
{
	unsigned int flags;
	struct cblock *cb;
	unsigned char ch;

	SAVE_FLAGS(flags); CLI();

	ch = 0;
	cb = q->head;
	if(cb) {
		if(cb->start_off < cb->end_off) {
			ch = cb->data[cb->start_off];
			cb->start_off++;
			q->count--;
		}
		if(cb->end_off - cb->start_off == 0) {
			delete_cblock_from_head(q);
		}
	}

	RESTORE_FLAGS(flags);
	return ch;
}

void charq_flush(struct clist *q)
{
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();

	while(q->head != NULL) {
		delete_cblock_from_head(q);
	}

	RESTORE_FLAGS(flags);
}

int charq_room(struct clist *q)
{
	return (NR_CB_QUEUE * CBSIZE) - q->count;
}
