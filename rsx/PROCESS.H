/* Copyright	Rainer Schnitker 92,93 */

#ifndef _RSX_PROCESS_H
#define _RSX_PROCESS_H

#define N_SIGA	    30		/* signal actions per process */
#define N_FILES     40		/* open files per process */
#define N_FD	     2		/* bits field for close_on_exec */
#define RSX_NFILES 100		/* RSX files for all processes */
#define N_DRIVES    26		/* watched drives for cwd */

#define DPMI_PRG_DATA 60*1024L*1024L
#define DPMI_PRG_ROOM 64*1024L*1024L

/* emx swawnve */
#define P_WAIT		0
#define P_NOWAIT	1
#define P_OVERLAY	2
#define P_DEBUG 	3
#define P_SESSION	4
#define P_DETACH	5
#define P_PM		6

/* process status */
#define PS_EMPTY	0	/* free process */
#define PS_ZOMBIE	1	/* process exit, wait for father wait() */
#define PS_RUN		2	/* process running */
#define PS_STOP 	3	/* process stopped */
#define PS_WAIT 	4	/* process is waiting */

/* process flags */
#define PF_DEBUG	1	/* process debugged by ptrace */
#define PF_MATH_USED	2	/* 387 state saving */
#define PF_TERMIO	4	/* termio used */
#define PF_WAIT_WAIT	8	/* wait for father wait() */
#define PF_COMPRESS	16	/* compress memory for djgpp-system() call */
#define PF_SPAWN_ASYNC	32	/* async spawn */
#define PF_EMX_FILE	64	/* emx program */
#define PF_DJGPP_FILE	128	/* djgpp program */
#define PF_EXTENDER	256	/* dos extender process */
#define PF_USEDPMI10	512	/* uses DPMI 1.0 paging, etc */
#define PF_TNT_FILE    1024	/* TNT executable */
#define PF_NEW_KSTACK  2048	/* kstack isn't pushed to stack */

struct fpu_reg {
	char sign;
	char tag;
	short align_fill_gcc;
	long exp;
	unsigned long sigl;
	unsigned long sigh;
};

union i387_union {
	struct i387_hard_struct {
		long	cwd;
		long	swd;
		long	twd;
		long	fip;
		long	fcs;
		long	foo;
		long	fos;
		long	st_space[20];
	} hard;
	struct i387_soft_struct {
		long	cwd;
		long	swd;
		long	twd;
		long	fip;
		long	fcs;
		long	foo;
		long	fos;
		long	top;
		struct fpu_reg	regs[8];
		long res1 ;
		long res2 ;
		long res3 ;
	} soft;
};

#include "RSX.H"
#include "SIGNALS.H"
#include "FS.H"

typedef struct p_tab
{
	   REG386  regs;		/* process registers */
    unsigned long  sig_raised;		/* signal raised y/n  (bits) */
    unsigned long  sig_blocked; 	/* signal en/disabled (bits) */
    unsigned long  kstack;		/* kernel stack */
 struct sigaction  sigaction[N_SIGA];	/* signal action */
     struct p_tab  *pptr;		/* pointer to org. parent */
     struct p_tab  *cptr;		/* pointer to last child */
	     UINT  pid; 		/* process id */
	     UINT  p_status;		/* process status */
	     UINT  p_flags;		/* process flags */
	     UINT  wait_return; 	/* return value for wait() */
	     UINT  code32sel;		/* code selector */
	     UINT  data32sel;		/* data selector */
    unsigned long  membytes;		/* memory for process */
    unsigned long  memhandle;		/* dpmi handle for memory block */
    unsigned long  memaddress;		/* lin. address of memory block */
    unsigned long  text_start;		/* first text address */
    unsigned long  text_end;		/* last text address */
    unsigned long  data_start;		/* first data address */
    unsigned long  data_end;		/* last data address */
    unsigned long  bss_start;		/* first bss address */
    unsigned long  bss_end;		/* last bss address */
	     long  filehandle;		/* swapper file handle (dpmi 1.0) */
    unsigned long  text_off;		/* offset text in file */
    unsigned long  data_off;		/* offset data in file */
    unsigned long  stacksize;		/* stack size */
    unsigned long  stack_top;		/* stack top address */
    unsigned long  stack_down;		/* stack bottom address */
    unsigned long  stackp32;		/* stackpointer at prg start */
    unsigned long  init_brk;		/* first brk value */
    unsigned long  brk_value;		/* current brk value */
    unsigned long  entry;		/* prg entry */
    unsigned long  pagefree;		/* free memory in current page */
    unsigned long  uflags;		/* uflags bits */
    struct options options;		/* options bits */
    unsigned long  time_tic;		/* clock at start */
    unsigned long  time_alarm;		/* alarm time */
    struct file *  filp[N_FILES];	/* file descriptors */
    char *	   cwd[N_DRIVES];	/* the cwd's of the drives */
    char	   cdrive;		/* current drive */
    p_fdset	   close_on_exec;	/* (bits) */
    union i387_union npx;
} NEWPROCESS ;

typedef struct
{
    unsigned long  arg_off;
    unsigned long  env_off;
    unsigned long  fname_off;
    unsigned short arg_sel;
    unsigned short env_sel;
    unsigned short fname_sel;
    unsigned short arg_count;
    unsigned short arg_size;
    unsigned short env_count;
    unsigned short env_size;
    unsigned short mode;
} PROCESS_ENV ;

#define EAX (npz->regs.eax)
#define EBX (npz->regs.ebx)
#define ECX (npz->regs.ecx)
#define EDX (npz->regs.edx)
#define ESI (npz->regs.esi)
#define EDI (npz->regs.edi)
#define EBP (npz->regs.ebp)
#define ESP (npz->regs.esp)
#define EIP (npz->regs.eip)
#define ERR (npz->regs.err)
#define ESPORG (npz->regs.esporg)
#define EFLAGS (npz->regs.eflags)
#define FAULTNO ((WORD)(npz->regs.faultno))
#define CS  ((WORD)(npz->regs.cs))
#define DS  ((WORD)(npz->regs.ds))
#define ES  ((WORD)(npz->regs.es))
#define SS  ((WORD)(npz->regs.ss))
#define FS  ((WORD)(npz->regs.fs))
#define GS  ((WORD)(npz->regs.gs))

#define AX (* (WORD *) & npz->regs.eax)
#define BX (* (WORD *) & npz->regs.ebx)
#define CX (* (WORD *) & npz->regs.ecx)
#define DX (* (WORD *) & npz->regs.edx)
#define SI (* (WORD *) & npz->regs.esi)
#define DI (* (WORD *) & npz->regs.edi)
#define BP (* (WORD *) & npz->regs.ebp)
#define SP (* (WORD *) & npz->regs.esp)
#define IP (* (WORD *) & npz->regs.eip)
#define FLAGS (* (WORD *) & npz->regs.eflags)

#define AH (* ((BYTE *) (&npz->regs.eax) + 1))
#define BH (* ((BYTE *) (&npz->regs.ebx) + 1))
#define CH (* ((BYTE *) (&npz->regs.ecx) + 1))
#define DH (* ((BYTE *) (&npz->regs.edx) + 1))

#define AL (* (BYTE *) &npz->regs.eax)
#define BL (* (BYTE *) &npz->regs.ebx)
#define CL (* (BYTE *) &npz->regs.ecx)
#define DL (* (BYTE *) &npz->regs.edx)

#define N_PRZ	8
#define RSX_PROCESS	process[0]
#define FIRST_PROCESS	process[1]
#define LAST_PROCESS	process[N_PRZ]
extern NEWPROCESS process[N_PRZ+1] ;
extern NEWPROCESS *npz; 	       /* current running process */
extern unsigned current_pid;

/* process.c functions */
int	send_signal ( NEWPROCESS *, int );
void	switch_context(NEWPROCESS *p);
int	schedule(void);
void	init_this_process ( void );
int	verify_illegal ( NEWPROCESS *, DWORD, DWORD );
int	verify_illegal_write ( NEWPROCESS *, DWORD, DWORD );
NEWPROCESS * find_process ( unsigned );
unsigned sys_wait ( unsigned * );
int	sys_waitpid(int, unsigned *);
void	free_process ( NEWPROCESS * );
void	clean_processtable( NEWPROCESS * );
int	realmode_prg ( char *, char **, char ** );
int	sys_fork ( void );
int	exec32 ( unsigned int, char *, int, char **, int, char **);
int	execRM ( unsigned int, char *, char **, char **);
void	shut_down ( int );
DWORD	getmem ( DWORD ,NEWPROCESS *);
int	do_exit4c ( int );

void	store_cwd (int drive);
void	get_cwd (int drive);

#endif /* _RSX_PROCESS_H */
