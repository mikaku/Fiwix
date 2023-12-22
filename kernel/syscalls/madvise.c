/*
 * fiwix/kernel/syscalls/madvise.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Copyright 2023, Richard R. Masters. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/process.h>
#include <fiwix/stdio.h>

#ifdef CONFIG_MADVISE_STUB
int sys_madvise(void *addr, __size_t length, int advice)
{
       return 0;
}
#endif
