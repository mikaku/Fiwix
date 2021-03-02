/*
 * fiwix/drivers/char/tty_queue.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/tty.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

/*
 * tty_queue.c implements a queue using a static-sized doubly linked list of a
 * central pool of buffers which covers all ttys.
 *
 *  head                                     tail
 * +--------------+  +--------------+  ...  +--------------+
 * |prev|data|next|  |prev|data|next|  ...  |prev|data|next|
 * | /  |    |  -->  <--  |    |  -->  ...  <--  |    |  / |
 * +--------------+  +--------------+  ...  +--------------+
 *  (cblock)          (cblock)               (cblock)
 */

struct cblock cblock_pool[CB_POOL_SIZE];
struct cblock *cblock_pool_head;

static struct cblock *get_free_cblock(void)
{
	struct cblock *new = NULL;

	if(cblock_pool_head) {
		new = cblock_pool_head;
		cblock_pool_head = cblock_pool_head->next;
		new->prev = new->next = NULL;
	}
	return new;
}

static void put_free_cblock(struct cblock *old)
{
	old->prev = NULL;
	old->next = cblock_pool_head;
	cblock_pool_head = old;
}

static struct cblock *insert_cblock_in_head(struct clist *q)
{
	struct cblock *cb;

	if(q->cb_num >= NR_CB_QUEUE) {
		return NULL;
	}
	if(!(cb = get_free_cblock())) {
		return NULL;
	}

	/* initialize cblock */
	cb->start_off = cb->end_off = 0;
	memset_b(cb->data, 0, CBSIZE);
	cb->prev = cb->next = NULL;
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

static struct cblock *insert_cblock_in_tail(struct clist *q)
{
	struct cblock *cb;

	if(q->cb_num >= NR_CB_QUEUE) {
		return NULL;
	}
	if(!(cb = get_free_cblock())) {
		return NULL;
	}

	/* initialize cblock */
	cb->start_off = cb->end_off = 0;
	memset_b(cb->data, 0, CBSIZE);
	cb->prev = cb->next = NULL;
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
	put_free_cblock(tmp);
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
	put_free_cblock(tmp);
}

int tty_queue_putchar(struct tty *tty, struct clist *q, unsigned char ch)
{
	unsigned long int flags;
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
		tty_queue_putchar(tty, q, ch);
		errno = 0;
	} else {
		errno = -EAGAIN;
	}

	RESTORE_FLAGS(flags);
	return errno;
}

int tty_queue_unputchar(struct clist *q)
{
	unsigned long int flags;
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

unsigned char tty_queue_getchar(struct clist *q)
{
	unsigned long int flags;
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

void tty_queue_flush(struct clist *q)
{
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();

	while(q->head != NULL) {
		delete_cblock_from_head(q);
	}

	RESTORE_FLAGS(flags);
}

int tty_queue_room(struct clist *q)
{
	return (NR_CB_QUEUE * CBSIZE) - q->count;
}

void tty_queue_init(void)
{
	int n;
	struct cblock *cb;

	memset_b(cblock_pool, NULL, sizeof(cblock_pool));

	/* cblock free list initialization */
	cblock_pool_head = NULL;
	n = CB_POOL_SIZE;
	while(n--) {
		cb = &cblock_pool[n];
		put_free_cblock(cb);
	}
}
