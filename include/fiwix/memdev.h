/*
 * fiwix/include/fiwix/memdev.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_MEMDEV_H
#define _FIWIX_MEMDEV_H

#include <fiwix/fs.h>

#define MEMDEV_MAJOR	1	/* major number */
#define MEMDEV_MINORS	5	/* number of supported minors */

#define MEMDEV_MEM	1	/* minor for /dev/mem */
#define MEMDEV_KMEM	2	/* minor for /dev/kmem */
#define MEMDEV_NULL	3	/* minor for /dev/null */
#define MEMDEV_ZERO	5	/* minor for /dev/zero */

int mem_open(struct inode *, struct fd *);
int mem_close(struct inode *, struct fd *);
int mem_read(struct inode *, struct fd *, char *, __size_t);
int mem_write(struct inode *, struct fd *, const char *, __size_t);
int mem_lseek(struct inode *, __off_t);

int kmem_open(struct inode *, struct fd *);
int kmem_close(struct inode *, struct fd *);
int kmem_read(struct inode *, struct fd *, char *, __size_t);
int kmem_write(struct inode *, struct fd *, const char *, __size_t);
int kmem_lseek(struct inode *, __off_t);

int null_open(struct inode *, struct fd *);
int null_close(struct inode *, struct fd *);
int null_read(struct inode *, struct fd *, char *, __size_t);
int null_write(struct inode *, struct fd *, const char *, __size_t);
int null_lseek(struct inode *, __off_t);

int zero_open(struct inode *, struct fd *);
int zero_close(struct inode *, struct fd *);
int zero_read(struct inode *, struct fd *, char *, __size_t);
int zero_write(struct inode *, struct fd *, const char *, __size_t);
int zero_lseek(struct inode *, __off_t);

int memdev_open(struct inode *, struct fd *);
int mem_mmap(struct inode *, struct vma *);
void memdev_init(void);

#endif /* _FIWIX_MEMDEV_H */
