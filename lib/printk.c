/*
 * fiwix/lib/printk.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/tty.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/stdarg.h>

#define LOG_BUF_LEN	4096
#define MAX_BUF		1024	/* printk() and sprintk() size limit */

static char log_buf[LOG_BUF_LEN];
static unsigned int log_count;

static void puts(char *buffer)
{
	struct tty *tty;
	unsigned short int count;
	char *b;

	tty = get_tty(_syscondev);
	count = strlen(buffer);
	b = buffer;

	while(count--) {
		if(!tty) {
			if(log_count < LOG_BUF_LEN) {
		 		log_buf[log_count++] = *(b++);
			}
		} else {
		 	tty_queue_putchar(tty, &tty->write_q, *(b++));

			/* kernel messages must be shown immediately */
			tty->output(tty);
		}
	}
}

/*
 * format identifiers
 * --------------------------------------------------------
 *	%d	decimal conversion
 *	%u	unsigned decimal conversion
 *	%x	hexadecimal conversion (lower case)
 *	%X	hexadecimal conversion (upper case)
 *	%b	binary conversion
 *	%o	octal conversion
 *	%c	character
 *	%s	string
 *
 * flags
 * --------------------------------------------------------
 *	0	result is padded with zeros (e.g.: '%06d')
 *		(maximum value is 32)
 *	blank	result is padded with spaces (e.g.: '% 6d')
 *		(maximum value is 32)
 *	-	the numeric result is left-justified
 *		(default is right-justified)
 */
static void do_printk(char *buffer, const char *format, va_list args)
{
	char sw_neg, in_identifier, n_pad, lf;
	char ch_pad, basecase, c;
	char str[] = {
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL
	};
	char nullstr[7] = { '<', 'N', 'U', 'L', 'L', '>', '\0' };
	char *ptr_s, *p;
	int num, count;
	char simplechar;
	unsigned int unum, digit;

	sw_neg = in_identifier = n_pad = lf = 0;
	count = 0;
	basecase = 'A';
	ch_pad = ' ';
	p = NULL;

	/* assumes buffer has a maximum size of MAX_BUF */
	while((c = *(format++)) && count < MAX_BUF) {
		if((c != '%') && !in_identifier) {
			*(buffer++) = c;
			memset_b(str, NULL, 32);
		} else {
			in_identifier = 1;
			switch(c = *(format)) {
				case 'd':
					num = va_arg(args, int);
					if(num < 0) {
						num *= -1;
						sw_neg = 1;
					}
					ptr_s = str;
					do {
						*(ptr_s++) = '0' + (num % 10);
					} while(num /= 10);
					if(lf) {
						p = ptr_s;
					} else {
						while(*ptr_s) {
							ptr_s++;
						}
					}
					if(sw_neg) {
						sw_neg = 0;
						*(ptr_s++) = '-';
					}
					do {
						*(buffer++) = *(--ptr_s);
						count++;
					} while(ptr_s != str && count < MAX_BUF);
					if(lf) {
						while(*p && count < MAX_BUF) {
							*(buffer++) = *(p++);
							count++;
						}
					}
					format++;
					ch_pad = ' ';
					n_pad = 0;
					in_identifier = 0;
					lf = 0;
					break;

				case 'u':
					unum = va_arg(args, unsigned int);
					ptr_s = str;
					do {
						*(ptr_s++) = '0' + (unum % 10);
					} while(unum /= 10);
					if(lf) {
						p = ptr_s;
					} else {
						while(*ptr_s) {
							ptr_s++;
						}
					}
					do {
						*(buffer++) = *(--ptr_s);
						count++;
					} while(ptr_s != str && count < MAX_BUF);
					if(lf) {
						while(*p && count < MAX_BUF) {
							*(buffer++) = *(p++);
							count++;
						}
					}
					format++;
					ch_pad = ' ';
					n_pad = 0;
					in_identifier = 0;
					lf = 0;
					break;

				case 'x':
					basecase = 'a';
				case 'X':
					unum = va_arg(args, unsigned int);
					ptr_s = str;
					do {
						*(ptr_s++) = (digit = (unum & 0x0F)) > 9 ? basecase + digit - 10 : '0' + digit;
					} while(unum /= 16);
					if(lf) {
						p = ptr_s;
					} else {
						while(*ptr_s) {
							ptr_s++;
						}
					}
					do {
						*(buffer++) = *(--ptr_s);
						count++;
					} while(ptr_s != str && count < MAX_BUF);
					if(lf) {
						while(*p && count < MAX_BUF) {
							*(buffer++) = *(p++);
							count++;
						}
					}
					format++;
					ch_pad = ' ';
					n_pad = 0;
					in_identifier = 0;
					lf = 0;
					break;

				case 'b':
					num = va_arg(args, unsigned int);
					if(num < 0) {
						num *= -1;
					}
					ptr_s = str;
					do {
						*(ptr_s++) = '0' + (num % 2);
					} while(num /= 2);
					if(lf) {
						p = ptr_s;
					} else {
						while(*ptr_s) {
							ptr_s++;
						}
					}
					do {
						*(buffer++) = *(--ptr_s);
						count++;
					} while(ptr_s != str && count < MAX_BUF);
					if(lf) {
						while(*p && count < MAX_BUF) {
							*(buffer++) = *(p++);
							count++;
						}
					}
					format++;
					ch_pad = ' ';
					n_pad = 0;
					in_identifier = 0;
					lf = 0;
					break;

				case 'o':
					num = va_arg(args, unsigned int);
					if(num < 0) {
						num *= -1;
					}
					ptr_s = str;
					do {
						*(ptr_s++) = '0' + (num % 8);
					} while(num /= 8);
					if(lf) {
						p = ptr_s;
					} else {
						while(*ptr_s) {
							ptr_s++;
						}
					}
					do {
						*(buffer++) = *(--ptr_s);
						count++;
					} while(ptr_s != str && count < MAX_BUF);
					if(lf) {
						while(*p && count < MAX_BUF) {
							*(buffer++) = *(p++);
							count++;
						}
					}
					format++;
					ch_pad = ' ';
					n_pad = 0;
					in_identifier = 0;
					lf = 0;
					break;

				case 'c':
					simplechar = va_arg(args, int);
					*(buffer++) = simplechar;
					format++;
					in_identifier = 0;
					lf = 0;
					break;

				case 's':
					num = 0;
					ptr_s = va_arg(args, char *);
					if(n_pad) {
						num = n_pad - strlen(ptr_s);
						if(num < 0) {
							num *= -1;
						}
					}
					/* if it's a NULL then show "<NULL>" */
					if(ptr_s == NULL) {
					  	ptr_s = (char *)nullstr;
					}
					while((c = *(ptr_s++)) && count < MAX_BUF) {
						*(buffer++) = c;
						count++;
					}
					while(num-- && count < MAX_BUF) {
						*(buffer++) = ' ';
						count++;
					}
					format++;
					n_pad = 0;
					in_identifier = 0;
					lf = 0;
					break;

				case ' ':
					ch_pad = ' ';
					break;

				case '0':
					if(!n_pad) {
						ch_pad = '0';
					}
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					n_pad = !n_pad ? c - '0': ((n_pad * 10) + (c - '0'));
					n_pad = n_pad > 32 ? 32 : n_pad;
					for(unum = 0; unum < n_pad; unum++) {
						str[unum] = ch_pad;
					}
					break;

				case '-':
					lf = 1;
					break;
				case '%':
					*(buffer++) = c;
					format++;
					in_identifier = 0;
					break;
			}
		}
		count++;
	}
	*buffer = NULL;
}

void register_console(void (*fn)(char *, unsigned int))
{
	(*fn)(log_buf, log_count);
}

void printk(const char *format, ...)
{
	va_list args;
	char buffer[MAX_BUF];

	va_start(args, format);
	do_printk(buffer, format, args);
	puts(buffer);
	va_end(args);
}

int sprintk(char *buffer, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	do_printk(buffer, format, args);
	va_end(args);
	return strlen(buffer);
}
