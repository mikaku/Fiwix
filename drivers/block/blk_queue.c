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
	unsigned long int flags;
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

int do_blk_request(struct device *d, int cmd, struct buffer *buf)
{
	struct blk_request *br;
	int errno;

	if(!(br = (struct blk_request *)kmalloc(sizeof(struct blk_request)))) {
		printk("WARNING: %s(): no more free memory for block requests.\n", __FUNCTION__);
		return -ENOMEM;
	}

	memset_b(br, 0, sizeof(struct blk_request));
	br->cmd = cmd;
	br->cmd = buf->dev;
	br->block = buf->block;
	br->size = buf->size;
	br->buffer = buf;
	br->device = d;
	if(cmd == BLK_READ) {
		br->fn = d->fsop->read_block;
	} else {
		br->fn = d->fsop->write_block;
	}

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
	unsigned long int flags;
	struct blk_request *br, *brh;
	int errno;

	SAVE_FLAGS(flags); CLI();
	br = (struct blk_request *)d->requests_queue;
	while(br) {
		if(br->status) {
			return;
		}
		br->status = BR_PROCESSING;
		if(!(errno = br->fn(br->buffer->dev, br->buffer->block, br->buffer->data, br->buffer->size))) {
			return;
		}
		br->errno = errno;
		d->requests_queue = (void *)br->next;
		br->status = BR_COMPLETED;
		if(br->head_group) {
			brh = br->head_group;
			brh->left--;
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
