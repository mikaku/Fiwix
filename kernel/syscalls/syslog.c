/*
 * fiwix/kernel/syscalls/syslog.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/syslog.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

int sys_syslog(int type, char *buffer, int len)
{
	char c;
	int errno, n, count;

#ifdef __DEBUG__
	printk("(pid %d) sys_syslog(%d, 0x%x, %d)\n", current->pid, type, buffer, len);
#endif /*__DEBUG__ */

	if(type != SYSLOG_READ_ALL && !IS_SUPERUSER) {
		return -EPERM;
	}
	if(type == SYSLOG_CLOSE || type == SYSLOG_OPEN) {
		return 0;
	}
	if(type == SYSLOG_READ ||
	   type == SYSLOG_READ_ALL ||
	   type == SYSLOG_READ_CLEAR) {
		if(!buffer || len < 0) {
			return -EINVAL;
		}
		if(!len) {
			return 0;
		}
		if((errno = check_user_area(VERIFY_WRITE, buffer, len))) {
			return errno;
		}
		count = 0;
		if(type == SYSLOG_READ || type == SYSLOG_READ_CLEAR) {
			while(!log_new_chars) {
				sleep(&sys_syslog, PROC_INTERRUPTIBLE);
				if(current->sigpending & ~current->sigblocked) {
					return -ERESTART;
				}
			}
			n = (log_write - log_new_chars) & (LOG_BUF_LEN - 1);
			while(log_new_chars && count < len) {
				if(!log_buf[n]) {
					break;
				}
				c = log_buf[n];
				log_new_chars--;
				n++;
				n &= LOG_BUF_LEN - 1;
				*buffer = c;
				buffer++;
				count++;
			}
			if(type == SYSLOG_READ_CLEAR) {
				log_new_chars = 0;
			}
		} else {
			len = MIN(LOG_BUF_LEN, len);
			len = MIN(log_size, len);
			n = log_read;
			while(len) {
				if(!log_buf[n]) {
					break;
				}
				c = log_buf[n];
				n++;
				n &= LOG_BUF_LEN - 1;
				*buffer = c;
				buffer++;
				len--;
				count++;
			}
		}
		return count;
	}
	if(type == SYSLOG_CLEAR) {
		log_new_chars = 0;
		return 0;
	}
	if(type == SYSLOG_CONSOLE_OFF) {
		console_loglevel = 1;
		return 0;
	}
	if(type == SYSLOG_CONSOLE_ON) {
		console_loglevel = DEFAULT_CONSOLE_LOGLEVEL;
		return 0;
	}
	if(type == SYSLOG_CONSOLE_LEVEL) {
		if(len < 1 || len > 8) {
			return -EINVAL;
		}
		console_loglevel = len;
		return 0;
	}
	if(type == SYSLOG_SIZE_BUFFER) {
		return LOG_BUF_LEN;
	}

	return -EINVAL;
}
