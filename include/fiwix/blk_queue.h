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

struct blk_request {
	int cmd;
	int status;
	int errno;
	__dev_t dev;
	__blk_t block;
	int size;
	char *buffer;
	struct device *device;
	int (*fn)(__dev_t, __blk_t, char *, int);
	struct blk_request *next;
};

int do_blk_request(struct device *, int, __dev_t, __blk_t, char *, int);
void run_blk_request(struct device *);

#endif /* _FIWIX_BLKQUEUE_H */
