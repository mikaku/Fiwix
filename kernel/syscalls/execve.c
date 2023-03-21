/*
 * fiwix/kernel/syscalls/execve.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/syscalls.h>
#include <fiwix/stat.h>
#include <fiwix/buffer.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/fcntl.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

static int initialize_barg(struct binargs *barg, char *argv[], char *envp[])
{
	int n, errno;

	for(n = 0; n < ARG_MAX; n++) {
		barg->page[n] = 0;
	}
	barg->argv_len = barg->envp_len = 0;

	for(n = 0; argv[n]; n++) {
		if((errno = check_user_area(VERIFY_READ, argv[n], sizeof(char *)))) {
			return errno;
		}
		barg->argv_len += strlen(argv[n]) + 1;
	}
	barg->argc = n;

	for(n = 0; envp[n]; n++) {
		if((errno = check_user_area(VERIFY_READ, envp[n], sizeof(char *)))) {
			return errno;
		}
		barg->envp_len += strlen(envp[n]) + 1;
	}
	barg->envc = n;

	return 0;
}

static void free_barg_pages(struct binargs *barg)
{
	int n;

	for(n = 0; n < ARG_MAX; n++) {
		if(barg->page[n]) {
			kfree(barg->page[n]);
		}
	}
}

static int add_strings(struct binargs *barg, char *filename, char *interpreter, char *args)
{
	int n, p, offset;
	unsigned int ae_str_len;
	char *page;

	/*
	 * For a script we need to substitute the saved argv[0] by the original
	 * 'filename' supplied in execve(), otherwise the interpreter won't be
	 * able to find the script file.
	 */
	p = ARG_MAX - 1;
	ae_str_len = barg->argv_len + barg->envp_len + 4;
	p -= ae_str_len / PAGE_SIZE;
	offset = PAGE_SIZE - (ae_str_len % PAGE_SIZE);
	if(offset == PAGE_SIZE) {
		offset = 0;
		p++;
	}
	page = (char *)barg->page[p];
	while(*(page + offset)) {
		offset++;
		barg->argv_len--;
		if(offset == PAGE_SIZE) {
			p++;
			offset = 0;
			page = (char *)barg->page[p];
		}
	}
	barg->argv_len--;


	p = ARG_MAX - 1;
	barg->argv_len += strlen(interpreter) + 1;
	barg->argv_len += strlen(args) ? strlen(args) + 1 : 0;
	barg->argv_len += strlen(filename) + 1;
	barg->argc++;
	if(*args) {
		barg->argc++;
	}
	ae_str_len = barg->argv_len + barg->envp_len + 4;
	p -= ae_str_len / PAGE_SIZE;
	offset = PAGE_SIZE - (ae_str_len % PAGE_SIZE);
	if(offset == PAGE_SIZE) {
		offset = 0;
		p++;
	}
	barg->offset = offset;
	for(n = p; n < ARG_MAX; n++) {
		if(!barg->page[n]) {
			if(!(barg->page[n] = kmalloc(PAGE_SIZE))) {
				free_barg_pages(barg);
				return -ENOMEM;
			}
		}
	}

	/* interpreter */
	page = (char *)barg->page[p];
	while(*interpreter) {
		*(page + offset) = *interpreter;
		offset++;
		interpreter++;
		if(offset == PAGE_SIZE) {
			p++;
			offset = 0;
			page = (char *)barg->page[p];
		}
	}
	*(page + offset++) = 0;
	if(offset == PAGE_SIZE) {
		p++;
		offset = 0;
	}

	/* args */
	page = (char *)barg->page[p];
	if(*args) {
		while(*args) {
			*(page + offset) = *args;
			offset++;
			args++;
			if(offset == PAGE_SIZE) {
				p++;
				offset = 0;
				page = (char *)barg->page[p];
			}
		}
		*(page + offset++) = 0;
		if(offset == PAGE_SIZE) {
			p++;
			offset = 0;
		}
	}

	/* original script ('filename' with path) at argv[0] */
	page = (char *)barg->page[p];
	while(*filename) {
		*(page + offset) = *filename;
		offset++;
		filename++;
		if(offset == PAGE_SIZE) {
			p++;
			offset = 0;
			page = (char *)barg->page[p];
		}
	}
	*(page + offset) = 0;

	return 0;
}

static int copy_strings(struct binargs *barg, char *argv[], char *envp[])
{
	int n, p, offset;
	unsigned int ae_ptr_len, ae_str_len;
	char *page, *str;

	p = ARG_MAX - 1;
	ae_ptr_len = (1 + (barg->argc + 1) + (barg->envc + 1)) * sizeof(unsigned int);
	/* the last 4 bytes of the stack pages are not used */
	ae_str_len = barg->argv_len + barg->envp_len + 4;
	if (ae_ptr_len + ae_str_len > (ARG_MAX * PAGE_SIZE)) {
		return -E2BIG;
	}
	p -= ae_str_len / PAGE_SIZE;
	offset = PAGE_SIZE - (ae_str_len % PAGE_SIZE);
	if(offset == PAGE_SIZE) {
		offset = 0;
		p++;
	}
	barg->offset = offset;
	for(n = p; n < ARG_MAX; n++) {
		if(!(barg->page[n] = kmalloc(PAGE_SIZE))) {
			free_barg_pages(barg);
			return -ENOMEM;
		}
	}
	for(n = 0; n < barg->argc; n++) {
		str = argv[n];
		page = (char *)barg->page[p];
		while(*str) {
			*(page + offset) = *str;
			offset++;
			str++;
			if(offset == PAGE_SIZE) {
				p++;
				offset = 0;
				page = (char *)barg->page[p];
			}
		}
		*(page + offset++) = 0;
		if(offset == PAGE_SIZE) {
			p++;
			offset = 0;
		}
	}
	for(n = 0; n < barg->envc; n++) {
		str = envp[n];
		page = (char *)barg->page[p];
		while(*str) {
			*(page + offset) = *str;
			offset++;
			str++;
			if(offset == PAGE_SIZE) {
				p++;
				offset = 0;
				page = (char *)barg->page[p];
			}
		}
		*(page + offset++) = 0;
		if(offset == PAGE_SIZE) {
			p++;
			offset = 0;
		}
	}

	return 0;
}

static int do_execve(const char *filename, char *argv[], char *envp[], struct sigcontext *sc)
{
	char interpreter[NAME_MAX + 1], args[NAME_MAX + 1], name[NAME_MAX + 1];
	__blk_t block;
	struct buffer *buf;
	struct inode *i;
	struct binargs barg;
	char *data;
	int errno;

	if((errno = initialize_barg(&barg, &(*argv), &(*envp))) < 0) {
		return errno;
	}

	/* save 'argv' and 'envp' into the kernel address space */
	if((errno = copy_strings(&barg, &(*argv), &(*envp)))) {
		return errno;
	}

	if(!(data = (void *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	strcpy(name, filename);

loop:
	if((errno = namei(name, &i, NULL, FOLLOW_LINKS))) {
		free_barg_pages(&barg);
		kfree((unsigned int)data);
		return errno;
	}

	if(!S_ISREG(i->i_mode)) {
		iput(i);
		free_barg_pages(&barg);
		kfree((unsigned int)data);
		return -EACCES;
	}
	if(check_permission(TO_EXEC, i) < 0) {
		iput(i);
		free_barg_pages(&barg);
		kfree((unsigned int)data);
		return -EACCES;
	}

	if((block = bmap(i, 0, FOR_READING)) < 0) {
		iput(i);
		free_barg_pages(&barg);
		kfree((unsigned int)data);
		return block;
	}
	if(!(buf = bread(i->dev, block, i->sb->s_blocksize))) {
		iput(i);
		free_barg_pages(&barg);
		kfree((unsigned int)data);
		return -EIO;
	}

	/*
	 * The contents of the buffer is copied and then freed immediately to
	 * make sure that it won't conflict while zeroing the BSS fractional
	 * page, in case that the same block is requested during the page fault.
	 */
	memcpy_b(data, buf->data, i->sb->s_blocksize);
	brelse(buf);

	errno = elf_load(i, &barg, sc, data);
	if(errno == -ENOEXEC) {
		/* OK, looks like it was not an ELF binary; let's see if it is a script */
		memset_b(interpreter, 0, NAME_MAX + 1);
		memset_b(args, 0, NAME_MAX + 1);
		errno = script_load(interpreter, args, data);
		if(!errno) {
			/* yes, it is! */
			iput(i);
			if((errno = add_strings(&barg, name, interpreter, args))) {
				free_barg_pages(&barg);
				kfree((unsigned int)data);
				return errno;
			}
			strcpy(name, interpreter);
			goto loop;
		}
	}

	if(!errno) {
		if(i->i_mode & S_ISUID) {
			current->euid = i->i_uid;
		}
		if(i->i_mode & S_ISGID) {
			current->egid = i->i_gid;
		}
	}

	iput(i);
	free_barg_pages(&barg);
	kfree((unsigned int)data);
	return errno;
}

#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_execve(const char *filename, char *argv[], char *envp[], int arg4, int arg5, int arg6, struct sigcontext *sc)
#else
int sys_execve(const char *filename, char *argv[], char *envp[], int arg4, int arg5, struct sigcontext *sc)
#endif /* CONFIG_SYSCALL_6TH_ARG */
{
	char *tmp_name;
	int n, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_execve('%s', ...)\n", current->pid, filename);
#endif /*__DEBUG__ */

	if((errno = malloc_name(filename, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = do_execve(tmp_name, &(*argv), &(*envp), sc))) {
		free_name(tmp_name);
		return errno;
	}

	strncpy(current->argv0, tmp_name, NAME_MAX);
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
	free_name(tmp_name);
	return 0;
}
