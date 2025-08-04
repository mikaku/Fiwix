/*
 * fiwix/include/fiwix/syscalls.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SYSCALLS_H
#define _FIWIX_SYSCALLS_H

#include <fiwix/types.h>
#include <fiwix/system.h>
#include <fiwix/time.h>
#include <fiwix/times.h>
#include <fiwix/timeb.h>
#include <fiwix/utime.h>
#include <fiwix/statbuf.h>
#include <fiwix/ustat.h>
#include <fiwix/signal.h>
#include <fiwix/utsname.h>
#include <fiwix/resource.h>
#include <fiwix/dirent.h>
#include <fiwix/statfs.h>
#include <fiwix/sigcontext.h>
#include <fiwix/mman.h>
#include <fiwix/ipc.h>

#define NR_SYSCALLS	(sizeof(syscall_table) / sizeof(unsigned int))

#ifdef CONFIG_SYSCALL_6TH_ARG
int do_syscall(unsigned int, int, int, int, int, int, int, struct sigcontext);
#else
int do_syscall(unsigned int, int, int, int, int, int, struct sigcontext);
#endif /* CONFIG_SYSCALL_6TH_ARG */

int sys_exit(int);
void do_exit(int);
#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_fork(int, int, int, int, int, int, struct sigcontext *);
#else
int sys_fork(int, int, int, int, int, struct sigcontext *);
#endif /* CONFIG_SYSCALL_6TH_ARG */
int sys_read(unsigned int, char *, int);
int sys_write(unsigned int, const char *, int);
int sys_open(const char *, int, __mode_t);
int sys_close(unsigned int);
int sys_waitpid(__pid_t, int *, int);
int sys_creat(const char *, __mode_t);
int sys_link(const char *, const char *);
int sys_unlink(const char *);
#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_execve(const char *, char **, char **, int, int, int, struct sigcontext *);
#else
int sys_execve(const char *, char **, char **, int, int, struct sigcontext *);
#endif /* CONFIG_SYSCALL_6TH_ARG */
int sys_chdir(const char *);
int sys_time(__time_t *);
int sys_mknod(const char *, __mode_t, __dev_t);
int sys_chmod(const char *, __mode_t);
int sys_lchown(const char *, __uid_t, __gid_t);
int sys_stat(const char *, struct old_stat *);
int sys_lseek(unsigned int, __off_t, unsigned int);
int sys_getpid(void);
int sys_mount(const char *, const char *, const char *, unsigned int, const void *);
int sys_umount(const char *);
int sys_setuid(__uid_t);
int sys_getuid(void);
int sys_stime(__time_t *);
int sys_alarm(unsigned int);
int sys_fstat(unsigned int, struct old_stat *);
int sys_pause(void);
int sys_utime(const char *, struct utimbuf *);
int sys_access(const char *, __mode_t);
int sys_ftime(struct timeb *);
void sys_sync(void);
int sys_kill(__pid_t, __sigset_t);
int sys_rename(const char *, const char *);
int sys_mkdir(const char *, __mode_t);
int sys_rmdir(const char *);
int sys_dup(unsigned int);
int sys_pipe(int *);
int sys_times(struct tms *);
int sys_brk(unsigned int);
int sys_setgid(__gid_t);
int sys_getgid(void);
unsigned int sys_signal(__sigset_t, void(*sighandler)(int));
int sys_geteuid(void);
int sys_getegid(void);
int sys_umount2(const char *, int);
int sys_ioctl(unsigned int, int, unsigned int);
int sys_fcntl(unsigned int, int, unsigned int);
int sys_setpgid(__pid_t, __pid_t);
int sys_olduname(struct oldold_utsname *);
int sys_umask(__mode_t);
int sys_chroot(const char *);
int sys_ustat(__dev_t, struct ustat *);
int sys_dup2(unsigned int, unsigned int);
int sys_getppid(void);
int sys_getpgrp(void);
int sys_setsid(void);
int sys_sigaction(__sigset_t, const struct sigaction *, struct sigaction *);
int sys_sgetmask(void);
int sys_ssetmask(int);
int sys_setreuid(__uid_t, __uid_t);
int sys_setregid(__gid_t, __gid_t);
int sys_sigsuspend(__sigset_t *);
int sys_sigpending(__sigset_t *);
int sys_sethostname(const char *, int);
int sys_setrlimit(int, const struct rlimit *);
int sys_getrlimit(int, struct rlimit *);
int sys_getrusage(int, struct rusage *);
int sys_gettimeofday(struct timeval *, struct timezone *);
int sys_settimeofday(const struct timeval *, const struct timezone *);
int sys_getgroups(__ssize_t, __gid_t *);
int sys_setgroups(__ssize_t, const __gid_t *);
int old_select(unsigned int *);
int sys_symlink(const char *, const char *);
int sys_lstat(const char *, struct old_stat *);
int sys_readlink(const char *, char *, __size_t);
int sys_reboot(int, int, int);
int old_mmap(struct mmap *);
int sys_munmap(unsigned int, __size_t);
int sys_truncate(const char *, __off_t);
int sys_ftruncate(unsigned int, __off_t);
int sys_fchmod(unsigned int, __mode_t);
int sys_fchown(unsigned int, __uid_t, __gid_t);
int sys_statfs(const char *, struct statfs *);
int sys_fstatfs(unsigned int, struct statfs *);
int sys_ioperm(unsigned int, unsigned int, int);
int sys_socketcall(int, unsigned int *);
int sys_syslog(int, char *, int);
int sys_setitimer(int, const struct itimerval *, struct itimerval *);
int sys_getitimer(int, struct itimerval *);
int sys_newstat(const char *, struct new_stat *);
int sys_newlstat(const char *, struct new_stat *);
int sys_newfstat(unsigned int, struct new_stat *);
int sys_uname(struct old_utsname *);
#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_iopl(int, int, int, int, int, int, struct sigcontext *);
#else
int sys_iopl(int, int, int, int, int, struct sigcontext *);
#endif /* CONFIG_SYSCALL_6TH_ARG */
int sys_wait4(__pid_t, int *, int, struct rusage *);
int sys_sysinfo(struct sysinfo *);
#ifdef CONFIG_SYSVIPC
int sys_ipc(unsigned int, struct sysvipc_args *);
#endif /* CONFIG_SYSVIPC */
int sys_fsync(unsigned int);
#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_sigreturn(unsigned int, int, int, int, int, int, struct sigcontext *);
#else
int sys_sigreturn(unsigned int, int, int, int, int, struct sigcontext *);
#endif /* CONFIG_SYSCALL_6TH_ARG */
int sys_setdomainname(const char *, int);
int sys_newuname(struct new_utsname *);
int sys_mprotect(unsigned int, __size_t, int);
int sys_sigprocmask(int, const __sigset_t *, __sigset_t *);
int sys_getpgid(__pid_t);
int sys_fchdir(unsigned int);
int sys_personality(unsigned int);
int sys_setfsuid(__uid_t);
int sys_setfsgid(__gid_t);
int sys_llseek(unsigned int, unsigned int, unsigned int, __loff_t *, unsigned int);
int sys_getdents(unsigned int, struct dirent *, unsigned int);
int sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int sys_flock(unsigned int, int);
int sys_readv(int, struct iovec *, int);
int sys_writev(int, struct iovec *, int);
int sys_getsid(__pid_t);
int sys_fdatasync(int);
int sys_nanosleep(const struct timespec *, struct timespec *);
int sys_chown(const char *, __uid_t, __gid_t);
int sys_getcwd(char *, __size_t);
#ifdef CONFIG_MMAP2
int sys_mmap2(unsigned int, unsigned int, unsigned int, unsigned int, int, unsigned int);
#endif /* CONFIG_MMAP2 */
int sys_truncate64(const char *, __loff_t);
int sys_ftruncate64(unsigned int, __loff_t);
int sys_stat64(const char *, struct stat64 *);
int sys_lstat64(const char *, struct stat64 *);
int sys_fstat64(unsigned int, struct stat64 *);
int sys_chown32(const char *, unsigned int, unsigned int);
int sys_getdents64(unsigned int, struct dirent64 *, unsigned int);
int sys_fcntl64(unsigned int, int, unsigned int);
int sys_utimes(const char *, struct timeval times[2]);

#endif /* _FIWIX_SYSCALLS_H */
