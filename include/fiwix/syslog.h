/*
 * fiwix/include/fiwix/syslog.h
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SYSLOG_H
#define _FIWIX_SYSLOG_H

#define SYSLOG_CLOSE		0	/* close the log */
#define SYSLOG_OPEN		1	/* open the log */
#define SYSLOG_READ		2	/* read the log */
#define SYSLOG_READ_ALL		3	/* read all messages */
#define SYSLOG_READ_CLEAR	4	/* read and clear all messages */
#define SYSLOG_CLEAR		5	/* clear the buffer */
#define SYSLOG_CONSOLE_OFF	6	/* disable printk to console */
#define SYSLOG_CONSOLE_ON	7	/* enable printk to console */
#define SYSLOG_CONSOLE_LEVEL	8	/* set printk level to console */
#define SYSLOG_SIZE_UNREAD	9	/* get the number of unread chars */
#define SYSLOG_SIZE_BUFFER	10	/* get the size of the buffer */

#define DEFAULT_MESSAGE_LOGLEVEL 6	/* KERN_INFO */
#define DEFAULT_CONSOLE_LOGLEVEL 7 	/* KERN_DEBUG */

#define LOG_BUF_LEN	4096

extern char log_buf[LOG_BUF_LEN];	/* circular buffer */
extern unsigned int log_read, log_write, log_size, log_new_chars;
extern int console_loglevel;

#endif /* _FIWIX_SYSLOG_H */
