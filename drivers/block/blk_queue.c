/*
 * fiwix/drivers/block/blk_queue.c
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/irq.h>
#include <fiwix/blk_queue.h>
#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/* append the request into the queue */
void add_blk_request(struct blk_request *br)
{
	unsigned int flags;
	struct blk_request *h;
	struct device *d;

	d = br->device;
	SAVE_FLAGS(flags); CLI();
	if((h = (struct blk_request *)d->requests_queue)) {
		while(h->next) {
			h = h->next;
		}
		h->next = br;
	} else {
		d->requests_queue = (void *)br;
	}
	RESTORE_FLAGS(flags);
}

int do_blk_request(struct device *d, void *fn, struct buffer *buf)
{
	struct blk_request *br;
	int errno;

	if(!(br = (struct blk_request *)kmalloc(sizeof(struct blk_request)))) {
		printk("WARNING: %s(): no more free memory for block requests.\n", __FUNCTION__);
		return -ENOMEM;
	}

	memset_b(br, 0, sizeof(struct blk_request));
	br->dev = buf->dev;
	br->block = buf->block;
	br->size = buf->size;
	br->buffer = buf;
	br->device = d;
	br->fn = fn;

	add_blk_request(br);
	run_blk_request(d);
	if(br->status != BR_COMPLETED) {
		sleep(br, PROC_UNINTERRUPTIBLE);
	}
	errno = br->errno;
	if(!br->head_group) {
		kfree((unsigned int)br);
	}
	return errno;
}

void run_blk_request(struct device *d)
{
	unsigned int flags;
	struct blk_request *br, *brh;
	int errno;

	SAVE_FLAGS(flags); CLI();
	br = (struct blk_request *)d->requests_queue;
	while(br) {
		if(br->status) {
			if(br->status == BR_COMPLETED) {
				printk("%s(): status marked as BR_COMPLETED, picking the next one ...\n", __FUNCTION__);
				d->requests_queue = (void *)br->next;
				br = br->next;
				continue;
			}
			return;
		}
		br->status = BR_PROCESSING;
		if(!(errno = br->fn(br->dev, br->block, br->buffer->data, br->size))) {
			return;
		}
		br->errno = errno;
		d->requests_queue = (void *)br->next;
		br->status = BR_COMPLETED;
		if(br->head_group) {
			brh = br->head_group;
			brh->left--;
			brh->errno = errno;
			if(!brh->left) {
				wakeup(brh);
			}
		} else {
			wakeup(br);
		}
		br = br->next;
	}
	RESTORE_FLAGS(flags);
}
