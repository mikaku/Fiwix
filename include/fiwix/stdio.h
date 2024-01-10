/*
 * fiwix/include/fiwix/stdio.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _INCLUDE_STDIO_H
#define _INCLUDE_STDIO_H

#include <fiwix/tty.h>

void flush_log_buf(struct tty *);
void printk(const char *, ...);
int sprintk(char *, const char *, ...);

#endif /* _INCLUDE_STDIO_H */
