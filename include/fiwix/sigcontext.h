/*
 * fiwix/include/fiwix/sigcontext.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SIGCONTEXT_H
#define _FIWIX_SIGCONTEXT_H

struct sigcontext {
	unsigned int gs;
	unsigned int fs;
	unsigned int es;
	unsigned int ds;
	unsigned int edi;
	unsigned int esi;
	unsigned int ebp;
	unsigned int esp;
	int ebx;
	int edx;
	int ecx;
	int eax;
	int err;
	unsigned int eip;
	unsigned int cs;
	unsigned int eflags;
	unsigned int oldesp;
	unsigned int oldss;
};

#endif /* _FIWIX_SIGCONTEXT_H */
