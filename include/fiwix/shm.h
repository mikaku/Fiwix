/*
 * fiwix/include/fiwix/shm.h
 */

#ifdef CONFIG_IPC

#ifndef _FIWIX_SHM_H
#define _FIWIX_SHM_H

#include <fiwix/types.h>
#include <fiwix/ipc.h>

#define SHM_DEST	01000		/* destroy segment on last detach */
#define	SHM_RDONLY	010000		/* attach a read-only segment */
#define	SHM_RND		020000		/* round attach address to SHMLBA */
#define	SHM_REMAP	040000		/* take-over region on attach */

/* super user shmctl commands */
#define SHM_LOCK 	11
#define SHM_UNLOCK 	12

/* system-wide limits */
/*
 * Since the current kernel memory allocator has a granularity of page size,
 * it's not possible to go beyond 4096 pages in the array of pointers to page
 * frames (*shm_pages). Hence SHMMAX must stay to 0x1000000 (4096 * 4096).
 *
 * It will be 0x2000000 (or more) when the new kernel memory allocator be
 * implemented.
 */
#define SHMMAX		0x1000000	/* max. segment size (in bytes) */

#define SHMMIN		1		/* min. segment size (in bytes) */
#define SHMMNI		128		/* max. number of shared segments */
#define SHMSEG		SHMMNI		/* max. segments per process */
#define SHMLBA		PAGE_SIZE	/* low boundary address (in bytes) */
#define SHMALL		524288		/* max. total segments (in pages) */

#define SHM_STAT 	13
#define SHM_INFO 	14

#define NUM_ATTACHES_PER_SEG	(PAGE_SIZE / sizeof(struct vma))

struct shmid_ds {
	struct ipc_perm shm_perm;	/* access permissions */
	__size_t shm_segsz;		/* size of segment (in bytes) */
	__time_t shm_atime;		/* time of the last shmat() */
	__time_t shm_dtime;		/* time of the last shmdt() */
	__time_t shm_ctime;		/* time of the last change */
	unsigned short shm_cpid;	/* pid of creator */
	unsigned short shm_lpid;	/* pid of last shm operation */
	unsigned short shm_nattch;	/* num. of current attaches */
	/* the following are for kernel only */
	unsigned short shm_npages;	/* size of segment (in pages) */
	unsigned int *shm_pages;	/* array of ptrs to frames -> SHMMAX */
	struct vma *shm_attaches;	/* ptr to array of attached regions */
};

struct shminfo {
    int shmmax;
    int shmmin;
    int shmmni;
    int shmseg;
    int shmall;
};

struct shm_info {
	int used_ids;
	unsigned long int shm_tot;	/* total allocated shm */
	unsigned long int shm_rss;	/* total resident shm */
	unsigned long int shm_swp;	/* total swapped shm */
	unsigned long int swap_attempts;
	unsigned long int swap_successes;
};

extern struct shmid_ds *shmseg[];
extern unsigned int num_segs;
extern unsigned int max_segid;
extern unsigned int shm_seq;
extern unsigned int shm_tot;
extern unsigned int shm_rss;

void shm_init(void);
struct shmid_ds *shm_get_new_seg(void);
void shm_release_seg(struct shmid_ds *);
void free_seg(int);
struct vma *shm_get_new_attach(struct shmid_ds *);
void shm_release_attach(struct vma *);
int shm_map_page(struct vma *, unsigned int);
int sys_shmat(int, char *, int, unsigned long int *);
int sys_shmdt(char *);
int sys_shmget(key_t, __size_t, int);
int sys_shmctl(int, int, struct shmid_ds *);

#endif /* _FIWIX_SHM_H */

#endif /* CONFIG_IPC */
