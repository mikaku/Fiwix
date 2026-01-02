/*
 * fiwix/include/fiwix/string.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _INCLUDE_STRING_H
#define _INCLUDE_STRING_H

#include <fiwix/types.h>

#ifndef NULL
#define NULL	((void *)0)
#endif

#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))

#define IS_NUMERIC(c)	((c) >= '0' && (c) <= '9')
#define IS_SPACE(c)	((c) == ' ')

void swap_asc_word(char *, int);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, __ssize_t);
char * strcpy(char *, const char *);
void strncpy(char *, const char *, int);
char * strcat(char *, const char *);
char * strncat(char *, const char *, __ssize_t);
int strlen(const char *);
char * get_basename(const char *);
char * remove_trailing_slash(char *);
int is_dir(const char *);
int atoi(const char *);
void memcpy_b(void *, const void *, unsigned int);
void memcpy_w(void *, const void *, unsigned int);
void memcpy_l(void *, const void *, unsigned int);
void memset_b(void *, unsigned char, unsigned int);
void memset_w(void *, unsigned short int, unsigned int);
void memset_l(void *, unsigned int, unsigned int);
int memcmp(const void *, const void *, unsigned int);
void *memmove(void *, void const *, int);

#endif /* _INCLUDE_STRING_H */
