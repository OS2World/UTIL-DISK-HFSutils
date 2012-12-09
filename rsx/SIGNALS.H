/* Copyright	Rainer Schnitker 92,93 */

#ifndef _SIGNALS_H
#define _SIGNALS_H

/* signal no */
#define SIGHUP	  1
#define SIGINT	  2
#define SIGQUIT   3
#define SIGILL	  4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGEMT	  7
#define SIGFPE	  8
#define SIGKILL   9
#define SIGBUS	 10
#define SIGSEGV  11
#define SIGSYS	 12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGUSR1  16
#define SIGUSR2  17
#define SIGCLD	 18
#define SIGBREAK 21
#define MAX_SIGNALS 22

#define SIGCHLD SIGCLD

#define SIG_DFL  0
#define SIG_IGN  1
#define SIG_ACK  4
#define SIG_ERR  -1

#define SA_NOCLDSTOP	0x0001
#define SA_SYSV 	0x0002	/* Reset to SIG_DFL */
#define SA_ACK		0x0004	/* Don't unblock automatically */

#define SIG_BLOCK	1
#define SIG_UNBLOCK	2
#define SIG_SETMASK	3

typedef unsigned long sigset_t;

struct sigaction {
    DWORD	sa_handler;
    sigset_t	sa_mask;
    int 	sa_flags;
};

int	signal_handler_returned(void);
long	sys_signal( int, long);
int	sys_sigaction(int, DWORD, DWORD);
int	sys_sigpending(DWORD);
int	sys_sigprocmask(int, DWORD, DWORD);
void	myexcep13 ( void );

#endif
