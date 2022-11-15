/*
 * fiwix/include/fiwix/system.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SYSTEM_H
#define _FIWIX_SYSTEM_H

#define UTS_SYSNAME	"Fiwix"
#define UTS_NODENAME	"(none)"
#define UTS_RELEASE	"1.4.0"
#define UTS_DOMAINNAME	"(none)"

struct sysinfo {
	long int uptime;		/* seconds since boot */
	unsigned long int loads[3];	/* load average (1, 5 and 15 minutes) */
	unsigned long int totalram;	/* total usable main memory size */
	unsigned long int freeram;	/* available memory size */
	unsigned long int sharedram;	/* amount of shared memory */
	unsigned long int bufferram;	/* amount of memory used by buffers */
	unsigned long int totalswap;	/* total swap space size */
	unsigned long int freeswap;	/* available swap space */
	unsigned short int procs;	/* number of current processes */
	char _f[22];			/* pads structure to 64 bytes */
};

#endif /* _FIWIX_SYSTEM_H */
