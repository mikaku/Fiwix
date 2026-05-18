#ifndef SYS_ARCH_H
#define SYS_ARCH_H

#include <lwip/err.h>
#include <fiwix/sleep.h>
#include <fiwix/rand.h>

typedef struct semaphore {
	struct resource sem;
	int valid;
} sys_sem_t;

struct mail {
	void *msg;
	struct mail *next;
};

typedef struct mailbox {
	struct mail *queue;
	int length;
	int max_length;
	struct resource lock;
	struct resource full;
	struct resource empty;
	int valid;
} sys_mbox_t;

typedef struct mutex sys_mutex_t;
typedef struct proc *sys_thread_t;
typedef int sys_prot_t; /* not actually used */

#endif /* SYS_ARCH_H */
