#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/sleep.h>
#include <fiwix/syscalls.h>

static struct proc test_process;
static struct fd test_fd_table[NR_OPENS];

struct proc *current = &test_process;
unsigned int fd_table_size = sizeof(test_fd_table);

void lock_resource(struct resource *resource)
{
	(void)resource;
}

void unlock_resource(struct resource *resource)
{
	(void)resource;
}

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *byte;

	byte = destination;
	while(count--) {
		*byte++ = value;
	}
}

int sys_close(unsigned int ufd)
{
	release_user_fd(ufd);
	return 0;
}

static void clear_process(void)
{
	memset_b(&test_process, 0, sizeof(test_process));
	current->rlim[RLIMIT_NOFILE].rlim_cur = OPEN_MAX;
	current->rlim[RLIMIT_NOFILE].rlim_max = NR_OPENS;
}

int main(void)
{
	unsigned int n;
	int fd;

	fd_table = test_fd_table;
	if(OPEN_MAX != NR_OPENS || OPEN_MAX <= 256) {
		return 1;
	}

	clear_process();
	for(n = 0; n < OPEN_MAX; n++) {
		fd = get_new_user_fd(0);
		if(fd != (int)n) {
			return 2;
		}
	}
	if(get_new_user_fd(0) != -EMFILE) {
		return 3;
	}
	release_user_fd(OPEN_MAX / 2);
	if(get_new_user_fd(0) != OPEN_MAX / 2) {
		return 4;
	}

	clear_process();
	current->rlim[RLIMIT_NOFILE].rlim_cur = 300;
	for(n = 0; n < 300; n++) {
		if(get_new_user_fd(0) != (int)n) {
			return 5;
		}
	}
	if(get_new_user_fd(0) != -EMFILE) {
		return 6;
	}

	clear_process();
	current->fd[3] = 1;
	fd_table[1].count = 1;
	if(sys_dup2(3, OPEN_MAX) != -EINVAL) {
		return 7;
	}
	if(sys_dup2(3, OPEN_MAX - 1) != OPEN_MAX - 1 ||
		current->fd[OPEN_MAX - 1] != 1 || fd_table[1].count != 2) {
		return 8;
	}

	return 0;
}
