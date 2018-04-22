/*
 * fiwix/include/fiwix/stdio.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _INCLUDE_STDIO_H
#define _INCLUDE_STDIO_H

void register_console(void (*fn)(char *, unsigned int));
void printk(const char *, ...);
int sprintk(char *, const char *, ...);

#endif /* _INCLUDE_STDIO_H */
