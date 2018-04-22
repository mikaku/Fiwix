/*
 * fiwix/include/fiwix/utime.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_UTIME_H
#define _FIWIX_UTIME_H

#include <fiwix/types.h>

struct utimbuf {
	__time_t actime;	/* access time */
	__time_t modtime;	/* modification time */
};

#endif /* _FIWIX_UTIME_H */
