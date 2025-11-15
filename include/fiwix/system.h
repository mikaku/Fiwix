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
#define UTS_RELEASE	"1.7.0"
#define UTS_DOMAINNAME	"(none)"

struct sysinfo {
	int uptime;			/* seconds since boot */
	unsigned int loads[3];		/* load average (1, 5 and 15 minutes) */
	unsigned int totalram;		/* total usable main memory size */
	unsigned int freeram;		/* available memory size */
	unsigned int sharedram;		/* amount of shared memory */
	unsigned int bufferram;		/* amount of memory used by buffers */
	unsigned int totalswap;		/* total swap space size */
	unsigned int freeswap;		/* available swap space */
	unsigned short int procs;	/* number of current processes */
	char _f[22];			/* pads structure to 64 bytes */
};


#ifdef CUSTOM_SYSTEM_H
#include <fiwix/custom_system.h>
#endif

#endif /* _FIWIX_SYSTEM_H */
