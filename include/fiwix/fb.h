/*
 * fiwix/include/fiwix/fb.h
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FB_H
#define _FIWIX_FB_H

#include <fiwix/fs.h>

#define FB_MAJOR	29	/* major number */
#define FB_MINOR	0	/* minor number */

int fb_open(struct inode *, struct fd *);
int fb_close(struct inode *, struct fd *);
int fb_read(struct inode *, struct fd *, char *, __size_t);
int fb_write(struct inode *, struct fd *, const char *, __size_t);
int fb_mmap(struct inode *, struct vma *);
int fb_ioctl(struct inode *, struct fd *, int, unsigned int);
__loff_t fb_llseek(struct inode *, __loff_t);

void fb_init(void);

#endif /* _FIWIX_FB_H */
