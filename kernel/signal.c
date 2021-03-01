/*
 * fiwix/kernel/signal.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
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

int send_sig(struct proc *p, __sigset_t signum)
{
	struct proc *z;

	if(signum > NSIG || !p) {
		return -EINVAL;
	}

	if(!IS_SUPERUSER && current->euid != p->euid && current->sid != p->sid) {
		return -EPERM;
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
				p->state = PROC_RUNNING;
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

	if(p->sigaction[signum - 1].sa_handler == SIG_IGN && signum != SIGCHLD) {
		return 0;
	}

	/* SIGCHLD is ignored by default */
	if(p->sigaction[signum - 1].sa_handler == SIG_DFL) {
		/*
		 * INIT process is special, it only gets signals that have the
		 * signal handler installed. This avoids to bring down the
		 * system accidentally.
		 */
		if(p->pid == INIT) {
			return 0;
		}
		if(signum == SIGCHLD) {
			return 0;
		}
	}

	/* if SIGCHLD is ignored reap its children (prevent zombies) */
	if(p->sigaction[signum - 1].sa_handler == SIG_IGN) {
		if(signum == SIGCHLD) {
			while((z = get_next_zombie(p))) {
				remove_zombie(z);
			}
			return 0;
		}
	}

	p->sigpending |= 1 << (signum - 1);
	p->usage.ru_nsignals++;

	/* wake up the process only if that signal is not blocked */
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

void psig(unsigned int stack)
{
	int len;
	__sigset_t signum;
	unsigned int mask;
	struct sigcontext *sc;
	struct proc *p;

	sc = (struct sigcontext *)stack;
	for(signum = 1, mask = 1; signum < NSIG; signum++, mask <<= 1) {
		if(current->sigpending & mask) {
			current->sigpending &= ~mask;

			if((unsigned int)current->sigaction[signum - 1].sa_handler) {
				current->sigexecuting = mask;
				if(!(current->sigaction[signum - 1].sa_flags & SA_NODEFER)) {
					current->sigblocked |= mask;
				}

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

				if(current->sigaction[signum - 1].sa_flags & SA_RESETHAND) {
					current->sigaction[signum - 1].sa_handler = SIG_DFL;
				}
				return;
			}
			if(current->sigaction[signum - 1].sa_handler == SIG_DFL) {
				switch(signum) {
					case SIGCONT:
						current->state = PROC_RUNNING;
						need_resched = 1;
						break;
					case SIGSTOP:
					case SIGTSTP:
					case SIGTTIN:
					case SIGTTOU:
						current->exit_code = signum;
						current->state = PROC_STOPPED;
						if(!(current->sigaction[signum - 1].sa_flags & SA_NOCLDSTOP)) {
							if((p = get_proc_by_pid(current->ppid))) {
								send_sig(p, SIGCHLD);
								/* needed for job control */
								wakeup(&sys_wait4);
							}
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

	/* coming from a system call that needs to be restarted */
	if(sc->err > 0) {
		if(sc->eax == -ERESTART) {
			sc->eax = sc->err;	/* syscall was saved in 'err' */
			sc->eip -= 2;		/* point again to 'int 0x80' */
		}
	}
}
