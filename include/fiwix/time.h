/*
 * fiwix/include/fiwix/time.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_TIME_H
#define _FIWIX_TIME_H

#define ITIMER_REAL	0
#define ITIMER_VIRTUAL	1
#define ITIMER_PROF	2

struct timespec {
	int tv_sec;		/* seconds since 00:00:00, 1 Jan 1970 UTC */
	int tv_nsec;		/* nanoseconds (1000000000ns = 1sec) */
};

struct timeval {
	int tv_sec;		/* seconds since 00:00:00, 1 Jan 1970 UTC */
	int tv_usec;		/* microseconds	(1000000us = 1sec) */
};

struct timezone {
	int tz_minuteswest;	/* minutes west of GMT */
	int tz_dsttime;		/* type of DST correction */
};

struct itimerval {
	struct timeval it_interval;
	struct timeval it_value;
};

struct tm {
	int tm_sec;	/* seconds */
	int tm_min;	/* minutes */
	int tm_hour;	/* hour */
	int tm_mday;	/* day of the month */
	int tm_month;	/* month */
	int tm_year;	/* year */
	int tm_wday;	/* day of the week */
	int tm_yday;	/* day of the year */
	int tm_isdst;	/* daylight savings flag */
};

unsigned int tv2ticks(const struct timeval *);
void ticks2tv(int, struct timeval *);
int setitimer(int, const struct itimerval *, struct itimerval *);
unsigned int mktime(struct tm *);

#endif /* _FIWIX_TIME_H */
