/*
 * fiwix/include/fiwix/blk_queue.h
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_BLKQUEUE_H
#define _FIWIX_BLKQUEUE_H

#include <fiwix/config.h>
#include <fiwix/types.h>
#include <fiwix/devices.h>

#define BR_PROCESSING	1
#define BR_COMPLETED	2

#define BRF_NOBLOCK	1

struct blk_request {
	int status;
	int errno;
	__dev_t dev;
	__blk_t block;
	int size;
	int flags;
	struct buffer *buffer;
	struct device *device;
	int (*fn)(__dev_t, __blk_t, char *, int);
	int left;
	struct blk_request *next;
	struct blk_request *next_group;
	struct blk_request *head_group;
};

void add_blk_request(struct blk_request *);
int do_blk_request(struct device *, void *, struct buffer *);
void run_blk_request(struct device *);

#endif /* _FIWIX_BLKQUEUE_H */
