/* Copyright    Rainer Schnitker 92,93 */

/* ptrace */
#define PTRACE_TRACEME      0
#define PTRACE_PEEKTEXT     1
#define PTRACE_PEEKDATA     2
#define PTRACE_PEEKUSER     3
#define PTRACE_POKETEXT     4
#define PTRACE_POKEDATA     5
#define PTRACE_POKEUSER     6
#define PTRACE_RESUME       7
#define PTRACE_EXIT         8
#define PTRACE_STEP         9
#define PTRACE_SESSION      10

int	do_ptrace ( int, int, DWORD, DWORD, DWORD * );
