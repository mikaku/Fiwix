/*
 * fiwix/lib/printk.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/syslog.h>
#include <fiwix/tty.h>
#include <fiwix/sysconsole.h>
#include <fiwix/syscalls.h>
#include <fiwix/sleep.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/stdarg.h>

#define MAX_BUF		1024	/* printk() and sprintk() size limit */

static char buf[MAX_BUF];
static char newline = 1;
char log_buf[LOG_BUF_LEN];	/* circular buffer */
unsigned int log_read, log_write, log_size, log_new_chars;
int console_loglevel = DEFAULT_CONSOLE_LOGLEVEL;

static void puts(char *buffer, int msg_level)
{
	struct tty *tty;
	int n;
	char *p, *l;

#ifdef CONFIG_QEMU_DEBUGCON
	if(kstat.flags & KF_HAS_DEBUGCON) {
		p = buffer;
		while(*p) {
			if(p == buffer && strlen(buffer) > 3) {
				if(p[0] == '<' &&
				   p[1] >= '0' && p[1] <= '7' &&
				   p[2] == '>') {
					p = p + 3;
				}
			}
			if(msg_level < console_loglevel) {
				outport_b(QEMU_DEBUG_PORT, *(p++));
			}
		}
	}
#endif /* CONFIG_QEMU_DEBUGCON */

	tty = NULL;
	for(n = 0; n < NR_SYSCONSOLES; n++) {
		if(!sysconsole_table[n].dev) {
			continue;
		}

		if(sysconsole_table[n].dev == MKDEV(VCONSOLES_MAJOR, 0)) {
			tty = get_tty(MKDEV(VCONSOLES_MAJOR, 0));
		} else {
			tty = sysconsole_table[n].tty;
		}
	}

	l = p = buffer;
	while(*l) {
		if(tty && *p) {
			if(p == buffer && strlen(buffer) > 3) {
				if(p[0] == '<' &&
				   p[1] >= '0' && p[1] <= '7' &&
				   p[2] == '>') {
					p = p + 3;
				}
			}
			if(msg_level < console_loglevel) {
				charq_putchar(&tty->write_q, *p);
			}
			tty->output(tty);
			p++;
		}
		log_write &= LOG_BUF_LEN - 1;
		log_buf[log_write++] = *l;
		if(log_size < LOG_BUF_LEN) {
			log_size++;
		} else {
			log_read++;
			log_read &= LOG_BUF_LEN - 1;
		}
		log_new_chars = log_new_chars < LOG_BUF_LEN ? log_new_chars + 1 : log_new_chars;
		l++;
	}
	wakeup(&sys_syslog);
	wakeup(&do_select);
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
 * flags (e.g: %05d or %-6s)
 * --------------------------------------------------------
 *	0	result is padded with zeros (e.g.: '%06d')
 *		(maximum value is 32)
 *	blank	result is padded with spaces (e.g.: '% 6d')
 *		(maximum value is 32)
 *	-	the numeric result is left-justified
 *		(default is right-justified)
 *		the string is right-justified
 *		(default is left-justified)
 */
static int do_printk(char *buffer, const char *format, va_list args)
{
	char sw_neg, in_identifier, n_pad, lf, sw_l;
	char str[32 + 1], ch_pad, basecase, c;
	char nullstr[7] = { '<', 'N', 'U', 'L', 'L', '>', '\0' };
	char *ptr_s, *p;
	int num, count, level_found;
	char simplechar;
	unsigned int unum, digit;
	long long int lnum;
	unsigned long long int lunum;
	static char msg_level = -1;

	sw_neg = in_identifier = n_pad = lf = sw_l = 0;
	count = 0;
	basecase = 'A';
	ch_pad = ' ';
	p = NULL;

	/*
	 * Checks the log level (e.g: <4>) on every new line and adds the
	 * level mark (if needed), but only if the caller function was printk().
	 */
	if(newline == 1) {
		if(msg_level < 0 && buffer == buf) {
			level_found = 0;
			if(strlen(format) > 3) {
				if(format[0] == '<' &&
				   format[1] >= '0' && format[1] <= '7' &&
				   format[2] == '>') {
					msg_level = format[1] - '0';
					level_found = 1;
				}
			}
			if(!level_found) {
				msg_level = DEFAULT_MESSAGE_LOGLEVEL;
				*(buffer++) = '<';
				*(buffer++) = msg_level + '0';
				*(buffer++) = '>';
			}
		}
		newline = 0;
	}

	/* assumes buffer has a maximum size of MAX_BUF */
	while((c = *(format++)) && count < MAX_BUF) {
		if(!in_identifier) {
			memset_b(str, 0, sizeof(str));
		}
		if((c != '%') && !in_identifier) {
			if(c == '\n') {
				newline = 1;
				msg_level = -1;
			}
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
					if(lf) {
						while(num-- > 0 && count < MAX_BUF) {
							*(buffer++) = ' ';
							count++;
						}
					}
					while((c = *(ptr_s++)) && count < MAX_BUF) {
						*(buffer++) = c;
						count++;
					}
					while(num-- > 0 && count < MAX_BUF) {
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
	return msg_level;
}

void flush_log_buf(struct tty *tty)
{
	int n;
	static char msg_level = -1;

	n = log_read;
	while(n < log_size) {
		if(msg_level < 0) {
			msg_level = log_buf[n + 1] - '0';
			n = n + 3;
		}
		if(msg_level < console_loglevel) {
			if(charq_putchar(&tty->write_q, log_buf[n]) < 0) {
				tty->output(tty);
				continue;
			}
		}
		if(log_buf[n] == '\n') {
			msg_level = -1;
		}
		n++;
	}
	tty->output(tty);
}

void printk(const char *format, ...)
{
	va_list args;
	int msg_level;

	va_start(args, format);
	msg_level = do_printk(buf, format, args);
	puts(buf, msg_level);
	va_end(args);
}

int sprintk(char *buffer, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	do_printk(buffer, format, args);
	newline = 1;
	va_end(args);
	return strlen(buffer);
}

int snprintk(char *str, unsigned int size, const char *format, ...)
{
        va_list args;
        char *buffer;

	if(size > MAX_BUF) {
		PANIC("size too big.");
	}
        if(!(buffer = (char *)kmalloc(MAX_BUF))) {
                return;
        }
        va_start(args, format);
        sprintk(buffer, format, args);
        va_end(args);
        strncpy(str, buffer, size);
        str[size - 1] = '\0';
        kfree((unsigned int)buffer);
	return strlen(str);
}
