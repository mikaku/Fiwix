/*
 * fiwix/lib/string.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/tty.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/* convert from big-endian to little-endian (word swap) */
void swap_asc_word(char *str, int len)
{
	int n, n2;
	short int *ptr;
	char *buf;

	if(!(buf = (void *)kmalloc(PAGE_SIZE))) {
		return;
	}

	ptr = (short int *)str;

	for(n = 0, n2 = 0; n < len; n++) {
		buf[n2++] = *ptr >> 8;
		buf[n2++] = *ptr & 0xFF;
		ptr++;
	}
	for(n = len - 1; n > 0; n--) {
		if(buf[n] == '\0' || buf[n] == ' ') {
			buf[n] = 0;
		} else {
			break;
		}
	}
	memcpy_b(str, buf, len);
	kfree((unsigned int)buf);
}

int strcmp(const char *str1, const char *str2)
{
	while(*str1) {
		if(*str1 != *str2) {
			return 1;
		}
		str1++;
		str2++;
	}
	if(!(*str2)) {
		return 0;
	}
	return 1;
}

int strncmp(const char *str1, const char *str2, __ssize_t n)
{
	while(n > 0) {
		if(*str1 != *str2) {
			return 1;
		}
		str1++;
		str2++;
		n--;
	}
	return 0;
}

char *strcpy(char *dest, const char *src)
{
	if(!dest || !src) {
		return NULL;
	}

	while(*src) {
		*dest = *src;
		dest++;
		src++;
	}
	*dest = 0;		/* NULL-terminated */
	return dest;
}

void strncpy(char *dest, const char *src, int len)
{
	if(!dest || !src) {
		return;
	}

	while((*src) && len) {
		*dest = *src;
		dest++;
		src++;
		len--;
	}
	*dest = 0;		/* NULL-terminated */
}

char *strcat(char *dest, const char *src)
{
	char *orig;

	orig = dest;
	while(*dest) {
		dest++;
	}
	while(*src) {
		*dest = *src;
		dest++;
		src++;
	}
	*dest = 0;		/* NULL-terminated */
	return orig;
}

char *strncat(char *dest, const char *src, __ssize_t len)
{
	char *orig;

	orig = dest;
	while(*dest) {
		dest++;
	}
	while(*src && len) {
		*dest = *src;
		dest++;
		src++;
		len--;
	}
	*dest = 0;		/* NULL-terminated */
	return orig;
}

int strlen(const char *str)
{
	int n;

	n = 0;
	while(str && *str) {
		n++;
		str++;
	}
	return n;
}

char *strchr(const char *str, int c)
{
	while(*str) {
		if(*str == (char)c) {
			break;
		}
		str++;
	}
	return (char *)str;
}

char *strrchr(const char *str, int c)
{
	int len;

	if((len = strlen(str))) {
		while(--len) {
			if(str[len] == (char)c) {
				break;
			}
		}
	}
	return (char *)(str + len);
}

char *get_basename(const char *path)
{
	char *basename;
	char c;

	basename = NULL;

	while(path) {
		while(*path == '/') {
			path++;
		}
		if(*path != '\0') {
			basename = (char *)path;
		}
		while((c = *(path++)) && (c != '/'));
		if(!c) {
			break;
		}
	}
	return basename;
}

char *remove_trailing_slash(char *path)
{
	char *p;

	p = path + (strlen(path) - 1);
	while(p > path && *p == '/') {
		*p = 0;
		p--;
	}
	return path;
}

int is_dir(const char *path)
{
	while(*(path + 1)) {
		path++;
	}
	if(*path == '/') {
		return 1;
	}
	return 0;
}

int atoi(const char *str)
{
	int n;

	n = 0;
	while(IS_SPACE(*str)) {
		str++;
	}
	while(IS_NUMERIC(*str)) {
		n = (n * 10) + (*str++ - '0');
	}
	return n;
}

void memcpy_b(void *dest, const void *src, unsigned int count)
{
	unsigned char *d;
	unsigned char *s;

	d = (unsigned char *)dest;
	s = (unsigned char *)src;
	while(count--) {
		*d = *s;
		d++;
		s++;
	}
}

void memcpy_w(void *dest, const void *src, unsigned int count)
{
	unsigned short int *d;
	unsigned short int *s;

	d = (unsigned short int *)dest;
	s = (unsigned short int *)src;
	while(count--) {
		*d = *s;
		d++;
		s++;
	}
}

void memcpy_l(void *dest, const void *src, unsigned int count)
{
	unsigned int *d;
	unsigned int *s;

	d = (unsigned int *)dest;
	s = (unsigned int *)src;
	while(count--) {
		*d = *s;
		d++;
		s++;
	}
}

void memset_b(void *dest, unsigned char value, unsigned int count)
{
	unsigned char *d;
	
	d = (unsigned char *)dest;
	while(count--) {
		*d = value;
		d++;
	}
}

void memset_w(void *dest, unsigned short int value, unsigned int count)
{
	unsigned short int *d;
	
	d = (unsigned short int *)dest;
	while(count--) {
		*d = value;
		d++;
	}
}

void memset_l(void *dest, unsigned int value, unsigned int count)
{
	unsigned int *d;
	
	d = (unsigned int *)dest;
	while(count--) {
		*d = value;
		d++;
	}
}

int memcmp(const void *str1, const void *str2, unsigned int count)
{
	unsigned char *s1;
	unsigned char *s2;

	s1 = (unsigned char *)str1;
	s2 = (unsigned char *)str2;
	while(count--) {
		if(*s1 != *s2) {
			return s1 < s2 ? -1 : 1;
		}
		s1++;
		s2++;
	}
	return 0;
}

void *memmove(void *dest, void const *src, int count)
{
	if (dest < src) {
		memcpy_b (dest, src, count);
		return dest;
	} else {
		char *p = dest;
		char const *q = src;
		count = count - 1;
		while (count >= 0) {
			p[count] = q[count];
			count = count - 1;
		}
	}
	return dest;
}
