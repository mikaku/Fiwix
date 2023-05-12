/*
 * fiwix/include/fiwix/memdev.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_MEMDEV_H
#define _FIWIX_MEMDEV_H

#include <fiwix/fs.h>

#define MEMDEV_MAJOR	1	/* major number */

#define MEMDEV_MEM	1	/* minor for /dev/mem */
#define MEMDEV_KMEM	2	/* minor for /dev/kmem */
#define MEMDEV_NULL	3	/* minor for /dev/null */
#define MEMDEV_PORT	4	/* minor for /dev/port */
#define MEMDEV_ZERO	5	/* minor for /dev/zero */
#define MEMDEV_FULL	7	/* minor for /dev/full */
#define MEMDEV_RANDOM	8	/* minor for /dev/random */
#define MEMDEV_URANDOM	9	/* minor for /dev/urandom */

int mem_open(struct inode *, struct fd *);
int mem_close(struct inode *, struct fd *);
int mem_read(struct inode *, struct fd *, char *, __size_t);
int mem_write(struct inode *, struct fd *, const char *, __size_t);
int mem_llseek(struct inode *, __off_t);

int kmem_open(struct inode *, struct fd *);
int kmem_close(struct inode *, struct fd *);
int kmem_read(struct inode *, struct fd *, char *, __size_t);
int kmem_write(struct inode *, struct fd *, const char *, __size_t);
int kmem_llseek(struct inode *, __off_t);

int null_open(struct inode *, struct fd *);
int null_close(struct inode *, struct fd *);
int null_read(struct inode *, struct fd *, char *, __size_t);
int null_write(struct inode *, struct fd *, const char *, __size_t);
int null_llseek(struct inode *, __off_t);

int port_open(struct inode *, struct fd *);
int port_close(struct inode *, struct fd *);
int port_read(struct inode *, struct fd *, char *, __size_t);
int port_write(struct inode *, struct fd *, const char *, __size_t);
int port_llseek(struct inode *, __off_t);

int zero_open(struct inode *, struct fd *);
int zero_close(struct inode *, struct fd *);
int zero_read(struct inode *, struct fd *, char *, __size_t);
int zero_write(struct inode *, struct fd *, const char *, __size_t);
int zero_llseek(struct inode *, __off_t);

int full_open(struct inode *, struct fd *);
int full_close(struct inode *, struct fd *);
int full_read(struct inode *, struct fd *, char *, __size_t);
int full_write(struct inode *, struct fd *, const char *, __size_t);
int full_llseek(struct inode *, __off_t);

int urandom_open(struct inode *, struct fd *);
int urandom_close(struct inode *, struct fd *);
int urandom_read(struct inode *, struct fd *, char *, __size_t);
int urandom_write(struct inode *, struct fd *, const char *, __size_t);
int urandom_llseek(struct inode *, __off_t);

int memdev_open(struct inode *, struct fd *);
int mem_mmap(struct inode *, struct vma *);
void memdev_init(void);

#endif /* _FIWIX_MEMDEV_H */
