/*
 * fiwix/include/fiwix/ctype.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CTYPE_H
#define _FIWIX_CTYPE_H

#define _U	0x01    /* upper case */
#define _L	0x02    /* lower case */
#define _N	0x04    /* numeral (digit) */
#define _S	0x08    /* spacing character */
#define _P	0x10    /* punctuation */
#define _C	0x20    /* control character */
#define _X	0x40    /* hexadecimal */
#define _B	0x80    /* blank */

extern unsigned char _ctype[];

#define ISALPHA(ch)	((_ctype + 1)[ch] & (_U | _L))
#define ISUPPER(ch)	((_ctype + 1)[ch] & _U)
#define ISLOWER(ch)	((_ctype + 1)[ch] & _L)
#define ISDIGIT(ch)	((_ctype + 1)[ch] & _N)
#define ISALNUM(ch)	((_ctype + 1)[ch] & (_U | _L | _N))
#define ISSPACE(ch)	((_ctype + 1)[ch] & _S)
#define ISPUNCT(ch)	((_ctype + 1)[ch] & _P)
#define ISCNTRL(ch)	((_ctype + 1)[ch] & _C)
#define ISXDIGIT(ch)	((_ctype + 1)[ch] & (_N | _X))
#define ISPRINT(ch)	((_ctype + 1)[ch] & (_P | _U | _L | _N | _S))

#define ISASCII(ch)	((unsigned) ch <= 0x7F)
#define TOASCII(ch)	((unsigned) ch & 0x7F)

#define TOUPPER(ch)	((ch) & ~32)
#define TOLOWER(ch)	((ch) | 32)

#endif /* _FIWIX_CTYPE_H */
