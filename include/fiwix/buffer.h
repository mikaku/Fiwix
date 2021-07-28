/*
 * fiwix/include/fiwix/buffer.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_BUFFER_H
#define _FIWIX_BUFFER_H

#include <fiwix/types.h>
#include <fiwix/fs.h>

/* buffer flags */
#define BUFFER_VALID	0x0001
#define BUFFER_LOCKED	0x0002
#define BUFFER_DIRTY	0x0004

struct buffer {
	__dev_t dev;			/* device number */
	__blk_t block;			/* block number */
	int size;			/* block size (in bytes) */
	int flags;
	char *data;			/* block contents */
	struct buffer *prev_hash;
	struct buffer *next_hash;
	struct buffer *prev_free;
	struct buffer *next_free;
};
extern struct buffer *buffer_table;
extern struct buffer **buffer_hash_table;

/* values to be determined during system startup */
extern unsigned int buffer_table_size;		/* size in bytes */
extern unsigned int buffer_hash_table_size;	/* size in bytes */

struct buffer * bread(__dev_t, __blk_t, int);
void bwrite(struct buffer *);
void brelse(struct buffer *);
void sync_buffers(__dev_t);
void invalidate_buffers(__dev_t);
int reclaim_buffers(void);
void buffer_init(void);

#endif /* _FIWIX_BUFFER_H */
