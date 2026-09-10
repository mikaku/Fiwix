/*
 * fiwix/lib/rand.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/rand.h>
#include <fiwix/kernel.h>

unsigned int rand(void) {
	kstat.random_seed = kstat.random_seed * 1103515245 + 12345;
	return (unsigned int)(kstat.random_seed / 65536) % 256;
}
