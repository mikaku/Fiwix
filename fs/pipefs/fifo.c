/*
 * fiwix/fs/pipefs/fifo.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/sleep.h>
#include <fiwix/fcntl.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>

int fifo_open(struct inode *i, struct fd *f)
{
	/* first open */
	if(i->count == 1) {
		if(!(i->u.pipefs.i_data = (void *)kmalloc(PAGE_SIZE))) {
			return -ENOMEM;
		}
		i->u.pipefs.i_readoff = 0;
		i->u.pipefs.i_writeoff = 0;
	}

	if((f->flags & O_ACCMODE) == O_RDONLY) {
		i->u.pipefs.i_readers++;
		wakeup(&pipefs_write);
		if(!(f->flags & O_NONBLOCK)) {
			while(!i->u.pipefs.i_writers) {
				if(sleep(&pipefs_read, PROC_INTERRUPTIBLE)) {
					if(!--i->u.pipefs.i_readers) {
						wakeup(&pipefs_write);
					}
					return -EINTR;
				}
			}
		}
	}

	if((f->flags & O_ACCMODE) == O_WRONLY) {
		if((f->flags & O_NONBLOCK) && !i->u.pipefs.i_readers) {
			return -ENXIO;
		}

		i->u.pipefs.i_writers++;
		wakeup(&pipefs_read);
		if(!(f->flags & O_NONBLOCK)) {
			while(!i->u.pipefs.i_readers) {
				if(sleep(&pipefs_write, PROC_INTERRUPTIBLE)) {
					if(!--i->u.pipefs.i_writers) {
						wakeup(&pipefs_read);
					}
					return -EINTR;
				}
			}
		}
	}

	if((f->flags & O_ACCMODE) == O_RDWR) {
		i->u.pipefs.i_readers++;
		i->u.pipefs.i_writers++;
		wakeup(&pipefs_write);
		wakeup(&pipefs_read);
	}

	return 0;
}
