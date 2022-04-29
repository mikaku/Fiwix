/*
 * fiwix/include/fiwix/signal.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SIGNAL_H
#define _FIWIX_SIGNAL_H

#define NSIG		32

#define SIGHUP		1	/* Hangup or Reset */
#define SIGINT		2	/* Interrupt */
#define SIGQUIT		3	/* Quit */
#define SIGILL		4	/* Illegal Instruction */
#define SIGTRAP		5	/* Trace Trap */
#define SIGABRT		6	/* Abort Instruction */
#define SIGIOT		SIGABRT	/* I/O Trap Instruction */
#define SIGBUS		7	/* Bus Error */
#define SIGFPE		8	/* Floating Point Exception */
#define SIGKILL		9	/* Kill */
#define SIGUSR1		10	/* User Defined #1 */
#define SIGSEGV		11	/* Segmentation Violation */
#define SIGUSR2		12	/* User Defined #2 */
#define SIGPIPE		13	/* Broken Pipe */
#define SIGALRM		14	/* Alarm Clock */
#define SIGTERM		15	/* Software Termination */
#define SIGSTKFLT	16	/* Stack Fault */
#define SIGCHLD		17	/* Child Termination */
#define SIGCONT		18	/* Continue */
#define SIGSTOP		19	/* Stop */
#define SIGTSTP		20	/* Terminal Stop */
#define SIGTTIN		21	/* Background Read */
#define SIGTTOU		22	/* Background Write */
#define SIGURG		23	/* Urgent Data */
#define SIGXCPU		24	/* CPU eXceeded */
#define SIGXFSZ		25	/* File Size eXceeded */
#define SIGVTALRM	26	/* Virtual Time Alarm */
#define SIGPROF		27	/* Profile Alarm */
#define SIGWINCH	28	/* Window Change */
#define SIGIO		29	/* I/O Asyncronous */
#define SIGPOLL		SIGIO
#define SIGPWR		30	/* Power Fault */
#define SIGUNUSED	31

typedef unsigned long int __sigset_t;
typedef void (*__sighandler_t)(int);

struct sigaction {
	__sighandler_t sa_handler;
	__sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer)(void);
};

#define SIG_DFL		((__sighandler_t)  0)
#define SIG_IGN		((__sighandler_t)  1)
#define SIG_ERR		((__sighandler_t) -1)

/* bits in sa_flags */
#define SA_NOCLDSTOP	0x00000001	/* don't send SIGCHLD when children stop */
#define SA_NOCLDWAIT	0x00000002	/* don't create zombie on child death */
#define SA_ONSTACK	0x08000000	/* invoke handler on alternate stack */
#define SA_RESTART	0x10000000	/* automatically restart system call */
#define SA_INTERRUPT	0x20000000	/* unused */

/* don't automatically block signal when the handler is executing */
#define SA_NODEFER	0x40000000
#define SA_NOMASK	SA_NODEFER

/* reset signal disposition to SIG_DFL before invoking handler */
#define SA_RESETHAND	0x80000000
#define SA_ONESHOT	SA_RESETHAND

/* bits in the third argument to 'waitpid/wait4' */
#define WNOHANG		1	/* don't block waiting */
#define WUNTRACED	2	/* report status of stopped children */

#define SIG_BLOCK	0	/* for blocking signals */
#define SIG_UNBLOCK	1	/* for unblocking signals */
#define SIG_SETMASK	2	/* for setting the signal mask */

/* SIGKILL and SIGSTOP can't ever be set as blockable signals */
#define SIG_BLOCKABLE	(~(1 << (SIGKILL - 1)) | (1 << (SIGSTOP - 1)))

#define SIG_MASK(sig)	(~(1 << ((sig) - 1)))

#define	FROM_KERNEL	1	/* kernel is who sent the signal */
#define	FROM_USER	2	/* user is who sent the signal */

int issig(void);
void psig(unsigned int);
int kill_pid(__pid_t, __sigset_t, int);
int kill_pgrp(__pid_t, __sigset_t, int);

#endif /* _FIWIX_SIGNAL_H */
