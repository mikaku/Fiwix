/*
 * fiwix/include/fiwix/termios.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_TERMIOS_H
#define _FIWIX_TERMIOS_H

#include <fiwix/termbits.h>

struct winsize {
	unsigned short int ws_row;
	unsigned short int ws_col;
	unsigned short int ws_xpixel;
	unsigned short int ws_ypixel;
};

#define NCC	8

struct termio {
	unsigned short int c_iflag;	/* input mode flags */
	unsigned short int c_oflag; 	/* output mode flags */
	unsigned short int c_cflag;	/* control mode flags */
	unsigned short int c_lflag;	/* local mode flags */
	unsigned char c_line;		/* line discipline */
	unsigned char c_cc[NCC];	/* control characters */
};

#endif /* _FIWIX_TERMIOS_H */
