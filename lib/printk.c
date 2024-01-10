/*
 * fiwix/lib/printk.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/tty.h>
#include <fiwix/sysconsole.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/stdarg.h>

#define LOG_BUF_LEN	4096
#define MAX_BUF		1024	/* printk() and sprintk() size limit */

static char buf[MAX_BUF];
static char log_buf[LOG_BUF_LEN];
static unsigned int log_count;

static void puts(char *buffer)
{
	struct tty *tty;
	int n, syscon;

	while(*buffer) {
#ifdef CONFIG_QEMU_DEBUGCON
		if(kstat.flags & KF_HAS_DEBUGCON) {
			outport_b(QEMU_DEBUG_PORT, *buffer);
		}
#endif /* CONFIG_QEMU_DEBUGCON */

		for(n = 0, syscon = 0; n < NR_SYSCONSOLES; n++) {
			if(!sysconsole_table[n].dev) {
				continue;
			}

			if(sysconsole_table[n].dev == MKDEV(VCONSOLES_MAJOR, 0)) {
				tty = get_tty(MKDEV(VCONSOLES_MAJOR, 0));
			} else {
				tty = sysconsole_table[n].tty;
			}
			if(tty) {
				tty_queue_putchar(tty, &tty->write_q, *buffer);

				/* kernel messages must be shown immediately */
				tty->output(tty);
				syscon = 1;
			}
		}
		if(!syscon) {
			if(log_count < LOG_BUF_LEN) {
				log_buf[log_count++] = *buffer;
			}
		}
		buffer++;
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
 * length modifiers (e.g: %ld or %lu)
 * --------------------------------------------------------
 *  	l	long long int arguments
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
	char sw_neg, in_identifier, n_pad, lf, sw_l;
	char ch_pad, basecase, c;
	char str[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0
	};
	char nullstr[7] = { '<', 'N', 'U', 'L', 'L', '>', '\0' };
	char *ptr_s, *p;
	int num, count;
	char simplechar;
	unsigned int unum, digit;
	long long int lnum;
	unsigned long long int lunum;

	sw_neg = in_identifier = n_pad = lf = sw_l = 0;
	count = 0;
	basecase = 'A';
	ch_pad = ' ';
	p = NULL;

	/* assumes buffer has a maximum size of MAX_BUF */
	while((c = *(format++)) && count < MAX_BUF) {
		if(!in_identifier) {
			memset_b(str, 0, 32);
		}
		if((c != '%') && !in_identifier) {
			*(buffer++) = c;
		} else {
			in_identifier = 1;
			switch(c = *(format)) {
				case 'd':
#ifdef CONFIG_PRINTK64
					if(sw_l) {
						lnum = va_arg(args, long long int);
						if(lnum < 0) {
							lnum *= -1;
							sw_neg = 1;
						}
						ptr_s = str;
						do {
							*(ptr_s++) = '0' + (lnum % 10);
						} while(lnum /= 10);
					} else {
#endif /* CONFIG_PRINTK64 */
						num = va_arg(args, int);
						if(num < 0) {
							num *= -1;
							sw_neg = 1;
						}
						ptr_s = str;
						do {
							*(ptr_s++) = '0' + (num % 10);
						} while(num /= 10);
#ifdef CONFIG_PRINTK64
					}
#endif /* CONFIG_PRINTK64 */

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
					sw_l = 0;
					break;

				case 'u':
#ifdef CONFIG_PRINTK64
					if(sw_l) {
						lunum = va_arg(args, unsigned long long int);
						ptr_s = str;
						do {
							*(ptr_s++) = '0' + (lunum % 10);
						} while(lunum /= 10);
					} else {
#endif /* CONFIG_PRINTK64 */
						unum = va_arg(args, unsigned int);
						ptr_s = str;
						do {
							*(ptr_s++) = '0' + (unum % 10);
						} while(unum /= 10);
#ifdef CONFIG_PRINTK64
					}
#endif /* CONFIG_PRINTK64 */

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
					sw_l = 0;
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
					sw_l = 0;
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
					sw_l = 0;
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
					sw_l = 0;
					break;

				case 'c':
					simplechar = va_arg(args, int);
					*(buffer++) = simplechar;
					format++;
					in_identifier = 0;
					lf = 0;
					sw_l = 0;
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
					sw_l = 0;
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
				case 'l':
					sw_l = 1;
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
	*buffer = 0;
}

void flush_log_buf(struct tty *tty)
{
	char *buffer;

	buffer = &log_buf[0];
	while(log_count) {
		if(tty_queue_putchar(tty, &tty->write_q, *buffer) < 0) {
			tty->output(tty);
			continue;
		}
		log_count--;
		buffer++;
	}
	tty->output(tty);
}

void printk(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	do_printk(buf, format, args);
	puts(buf);
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
