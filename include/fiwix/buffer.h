/*
 * fiwix/include/fiwix/buffer.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_BUFFER_H
#define _FIWIX_BUFFER_H

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/blk_queue.h>

/* buffer flags */
#define BUFFER_VALID	0x01
#define BUFFER_LOCKED	0x02
#define BUFFER_DIRTY	0x04

#define BLK_READ	1
#define BLK_WRITE	2

struct buffer {
	__dev_t dev;			/* device number */
	__blk_t block;			/* block number */
	int size;			/* block size (in bytes) */
	int flags;
	char *data;			/* block contents */
	unsigned int mark;		/* a mark to identify a buffer */
	struct buffer *prev;
	struct buffer *next;
	struct buffer *prev_hash;
	struct buffer *next_hash;
	struct buffer *prev_free;
	struct buffer *next_free;
	struct buffer *prev_dirty;
	struct buffer *next_dirty;
	struct buffer *first_sibling;
	struct buffer *next_sibling;
	struct buffer *next_retained;
};
extern struct buffer *buffer_table;
extern struct buffer **buffer_hash_table;

/* value to be determined during system startup */
extern unsigned int buffer_hash_table_size;	/* size in bytes */

int gbread(struct device *, struct blk_request *);
struct buffer *bread(__dev_t, __blk_t, int);
void bwrite(struct buffer *);
void brelse(struct buffer *);
void sync_buffers(__dev_t);
void invalidate_buffers(__dev_t);
int reclaim_buffers(void);
int kbdflushd(void);
void buffer_init(void);

#endif /* _FIWIX_BUFFER_H */
