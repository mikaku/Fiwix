/*
 * fiwix/kernel/syscalls/execve.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/fs.h>
#include <fiwix/syscalls.h>
#include <fiwix/stat.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_execve(const char *filename, char *argv[], char *envp[], int arg4, int arg5, struct sigcontext *sc)
{
	int n, errno;
	struct inode *i;
	char *tmp_name;

#ifdef __DEBUG__
	printk("(pid %d) sys_execve('%s', ...)\n", current->pid, filename);
#endif /*__DEBUG__ */

	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	if(!S_ISREG(i->i_mode)) {
		iput(i);
		free_name(tmp_name);
		return -EACCES;
	}
	if(check_permission(TO_EXEC, i) < 0) {
		iput(i);
		free_name(tmp_name);
		return -EACCES;
	}

	if((errno = elf_load(i, &(*argv), &(*envp), sc))) {
		iput(i);
		free_name(tmp_name);
		return errno;
	}

	if(i->i_mode & S_ISUID) {
		current->euid = i->i_uid;
	}
	if(i->i_mode & S_ISGID) {
		current->egid = i->i_gid;
	}

	for(n = 0; n < OPEN_MAX; n++) {
		if(current->fd[n] && (current->fd_flags[n] & FD_CLOEXEC)) {
			sys_close(n);
		}
	}

	current->suid = current->euid;
	current->sgid = current->egid;
	current->sigpending = 0;
	current->sigexecuting = 0;
	for(n = 0; n < NSIG; n++) {
		current->sigaction[n].sa_mask = 0;
		current->sigaction[n].sa_flags = 0;
		if(current->sigaction[n].sa_handler != SIG_IGN) {
			current->sigaction[n].sa_handler = SIG_DFL;
		}
	}
	current->sleep_address = NULL;
	current->flags |= PF_PEXEC;
	iput(i);
	free_name(tmp_name);
	return 0;
}
