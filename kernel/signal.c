/*
 * fiwix/kernel/signal.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/signal.h>
#include <fiwix/sigcontext.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/syscalls.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#ifdef CONFIG_ARCH_RISCV64
#include <fiwix/riscv64_signal.h>
#endif

/* can process 'current' send a signal to process 'p'? */
int can_signal(struct proc *p)
{
	if(!IS_SUPERUSER && current->euid != p->euid && current->sid != p->sid) {
		return 0;
	}

	return 1;
}

int send_sig(struct proc *p, __sigset_t signum)
{
	if(signum >= NSIG || !p) {
		return -EINVAL;
	}

	/* kernel processes can't receive signals */
	if(p->flags & PF_KPROC) {
		return 0;
	}

	switch(signum) {
		case 0:
			return 0;
		case SIGKILL:
		case SIGCONT:
			if(p->state == PROC_STOPPED) {
				runnable(p);
				need_resched = 1;
			}
			/* discard all pending stop signals */
			p->sigpending &= SIG_MASK(SIGSTOP);
			p->sigpending &= SIG_MASK(SIGTSTP);
			p->sigpending &= SIG_MASK(SIGTTIN);
			p->sigpending &= SIG_MASK(SIGTTOU);
			break;
		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			/* discard all pending SIGCONT signals */
			p->sigpending &= SIG_MASK(SIGCONT);
			break;
		default:
			break;
	}

	/*
	 * Force some signals, that a process cannot ignore, by changing its
	 * signal disposition to SIG_DFL.
	 */
	switch(signum) {
		case SIGFPE:
		case SIGSEGV:
			if(p->sigaction[signum - 1].sa_handler == SIG_IGN) {
				p->sigaction[signum - 1].sa_handler = SIG_DFL;
			}
			break;
	}

	if(p->sigaction[signum - 1].sa_handler == SIG_DFL) {
		/*
		 * INIT process is special, it only gets signals that have the
		 * signal handler installed. This avoids to bring down the
		 * system accidentally.
		 */
		if(p->pid == INIT) {
			return 0;
		}

		/* these signals with SIG_DFL are ignored */
		switch(signum) {
			case SIGCONT:
			case SIGWINCH:
			case SIGCHLD:
			case SIGURG:
				return 0;
		}
	}

	if(p->sigaction[signum - 1].sa_handler == SIG_IGN) {
		/* if SIGCHLD is ignored reap its children (prevent zombies) */
		if(signum == SIGCHLD) {
			while(sys_waitpid(-1, NULL, WNOHANG) > 0) {
				continue;
			}
		}
		return 0;
	}

	p->sigpending |= 1 << (signum - 1);
	p->usage.ru_nsignals++;

	/* wake up the process only if the signal is not blocked */
	if(!(p->sigblocked & (1 << (signum - 1)))) {
		wakeup_proc(p);
	}

	return 0;
}

int issig(void)
{
	__sigset_t signum;
	unsigned int mask;
	struct proc *p;

	if(!(current->sigpending & ~current->sigblocked)) {
		return 0;
	}

	for(signum = 1, mask = 1; signum < NSIG; signum++, mask <<= 1) {
		if(current->sigpending & mask) {
			if(signum == SIGCHLD) {
				if(current->sigaction[signum - 1].sa_handler == SIG_IGN) {
					/* this process ignores SIGCHLD */
					while((p = get_next_zombie(current))) {
						remove_zombie(p);
					}
				} else {
					if(current->sigaction[signum - 1].sa_handler != SIG_DFL) {
						return signum;
					}
				}
			} else {
				if(current->sigaction[signum - 1].sa_handler != SIG_IGN) {
					return signum;
				}
			}
			current->sigpending &= ~mask;
		}
	}
	return 0;
}

void psig(__addr_t stack)
{
#ifndef CONFIG_ARCH_RISCV64
	int len;
#endif
	__sigset_t signum;
#ifdef CONFIG_ARCH_RISCV64
	__sigset_t oldmask;
#endif
	unsigned int mask;
#ifndef CONFIG_ARCH_RISCV64
	struct sigcontext *sc;
#else
	struct riscv64_trap_frame *frame;
#endif
	struct proc *p;

#ifndef CONFIG_ARCH_RISCV64
	sc = (struct sigcontext *)stack;
#else
	frame = (struct riscv64_trap_frame *)stack;
#endif
	for(signum = 1, mask = 1; signum < NSIG; signum++, mask <<= 1) {
		if(current->sigpending & mask) {
			current->sigpending &= ~mask;

			if(current->sigaction[signum - 1].sa_handler != SIG_DFL) {

#ifndef CONFIG_ARCH_RISCV64
				/*
				 * page_not_present() may have raised a SIGSEGV if it
				 * detected that this process doesn't have an stack
				 * region in vma_table.
				 *
				 * So, this check is needed to make sure to terminate the
				 * process now, otherwise the kernel would panic when using
				 * 'sc->oldesp' during the 'memcpy_b' below.
				 */
				if(!find_vma_region(sc->oldesp)) {
					printk("WARNING: %s(): no stack region in vma table for process %d. Terminated.\n", __FUNCTION__, current->pid);
					do_exit(signum);
				}
#endif

#ifdef CONFIG_ARCH_RISCV64
				oldmask = current->sigblocked;
#endif
				current->sigexecuting = mask;
				if(!(current->sigaction[signum - 1].sa_flags & SA_NODEFER)) {
					current->sigblocked |= mask;
				}

#ifdef CONFIG_ARCH_RISCV64
				current->sigblocked |=
					current->sigaction[signum - 1].sa_mask & SIG_BLOCKABLE;
				if(riscv64_signal_deliver(frame, signum, oldmask)) {
					printk("WARNING: %s(): unable to build signal frame for process %d. Terminated.\n",
						__FUNCTION__, current->pid);
					do_exit(SIGSEGV);
				}
#else
				/* save the current sigcontext */
				memcpy_b(&current->sc[signum - 1], sc, sizeof(struct sigcontext));
				/* setup the jump to the user signal handler */
				len = ((int)end_sighandler_trampoline - (int)sighandler_trampoline);
				sc->oldesp -= len;
				sc->oldesp -= 4;
				sc->oldesp &= ~3;	/* round up */
				memcpy_b((void *)sc->oldesp, sighandler_trampoline, len);
				sc->ecx = (unsigned int)current->sigaction[signum - 1].sa_handler;
				sc->eax= signum;
				sc->eip = sc->oldesp;
#endif

				if(current->sigaction[signum - 1].sa_flags & SA_RESETHAND) {
					current->sigaction[signum - 1].sa_handler = SIG_DFL;
				}
				return;
			}
			if(current->sigaction[signum - 1].sa_handler == SIG_DFL) {
				switch(signum) {
					case SIGCONT:
						runnable(current);
						need_resched = 1;
						break;
					case SIGSTOP:
					case SIGTSTP:
					case SIGTTIN:
					case SIGTTOU:
						current->exit_code = signum;
						not_runnable(current, PROC_STOPPED);
						if(!(current->sigaction[signum - 1].sa_flags & SA_NOCLDSTOP)) {
							p = current->ppid;
							send_sig(p, SIGCHLD);
							/* needed for job control */
							wakeup(&sys_wait4);
						}
						need_resched = 1;
						break;
					case SIGCHLD:
						break;
					default:
						do_exit(signum);
				}
			}
		}
	}

	/* coming from an i386 system call that needs to be restarted */
#ifndef CONFIG_ARCH_RISCV64
	if(sc->err > 0) {
		if(sc->eax == -ERESTART) {
			sc->eax = sc->err;	/* syscall was saved in 'err' */
			sc->eip -= 2;		/* point again to 'int 0x80' */
		}
	}
#endif
}

int kill_pid(__pid_t pid, __sigset_t signum, int sender)
{
	struct proc *p;

	FOR_EACH_PROCESS(p) {
		if(p->pid == pid && p->state != PROC_ZOMBIE) {
			if(sender == USER) {
				if(!can_signal(p)) {
					return -EPERM;
				}
			}
			return send_sig(p, signum);
		}
		p = p->next;
	}
	return -ESRCH;
}

int kill_pgrp(__pid_t pgid, __sigset_t signum, int sender)
{
	struct proc *p;
	int found;

	found = 0;
	FOR_EACH_PROCESS(p) {
		if(p->pgid == pgid && p->state != PROC_ZOMBIE) {
			if(sender == USER) {
				if(!can_signal(p)) {
					p = p->next;
					continue;
				}
			}
			send_sig(p, signum);
			found = 1;
		}
		p = p->next;
	}

	if(!found) {
		return -ESRCH;
	}
	return 0;
}
