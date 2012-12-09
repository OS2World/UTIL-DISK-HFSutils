/*****************************************************************************
 * FILE: process.c							     *
 *									     *
 * DESC:								     *
 *	- process handling						     *
 *	- kernel stacks, schedule, process switch			     *
 *	- syscalls: fork(), spawnve(), wait(), exit()			     *
 *	- execute real mode programs					     *
 *	- shut down rsx 						     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 * Thanks to: (bug-fixes, patches, testing, ..)                              *
 *      Friedbert Widmann <widi@informatik.uni-koblenz.de>                   *
 *                                                                           *
 *****************************************************************************/

/* Modified for hfsutils by Marcus Better, July 1997 */

#include <string.h>
#include <malloc.h>
#include "PRINTF.H"
#include "DPMI.H"
#include "DPMI10.H"
#include "RMLIB.H"
#include "PROCESS.H"
#include "SIGNALS.H"
#include "START32.H"
#include "CDOSX32.H"
#include "ADOSX32.H"
#include "EXCEP32.H"
#include "COPY32.H"
#include "FS.H"
#include "RSX.H"
#include "LOADPRG.H"
#include "DOSERRNO.H"
#include "SYSDEP.H"
#include "DJIO.H"
#include "KDEB.H"
#include "TERMIO.H"

/* local functions */
static NEWPROCESS * find_empty_process(void);
static unsigned long alloc_kernel_stack(void);
static void	    free_kernel_stack(unsigned long);
static int	    switch_to_process(NEWPROCESS *);
static int	    mk_cmdline(char **, char *);
static void	    copy_filedescriptors(NEWPROCESS *, NEWPROCESS *);
static void	    copy_cwd (NEWPROCESS * from, NEWPROCESS * to);
static void	    set_go32_info_block(void);

/* globals */
NEWPROCESS process[N_PRZ + 1];	/* N_PRZ processes, 0 = extender/emu387 */
NEWPROCESS *npz;		/* current running process */
unsigned int current_pid = 1;

/* locals */
static char stack_used[N_PRZ];
static unsigned long kstk[N_PRZ];


#define KERNEL_STACK 2048L

/*
 * kernel stack:
 *
 *	N_PRZ * KERNEL_STACK + init_stack <= RSX stack size
 *	   8	    2KB 	 0xFFF	     0xA000 - 0xFFFF = 0x6000
 */

static unsigned long alloc_kernel_stack(void)
{
    static int inited;
    int i;
    /* swapper needs also stack */
    DWORD stack = (stackp16 - 0x400) & ~0xFFL;

    if (!inited) {
	inited = 1;
	for (i=0; i<N_PRZ; i++)
	    kstk[i] = (unsigned long) stack - i * KERNEL_STACK;
    }
    for (i=0; i<N_PRZ; i++)
	if (!stack_used[i]) {
	    stack_used[i] = 1;
	    return kstk[i];
	}
    return 0;
}

static void free_kernel_stack(unsigned long address)
{
    int i;

    for (i=0; i<N_PRZ; i++)
	if (address == kstk[i]) {
	    stack_used[i] = 0;
	    break;
	}
}

#ifndef __GNUC__

void save_all(unsigned *stack);
void restore_all(unsigned *stack);
void new_stack_return(unsigned *stack);

#ifdef __WATCOMC__
#pragma aux ASM "_*"   \
	parm caller []	\
	value struct float struct routine [ax];
#pragma aux (ASM) save_all;
#pragma aux (ASM) restore_all;
#pragma aux (ASM) new_stack_return;
#endif

#endif

/*
** switch to other process, save current kernel stack
*/
void switch_context(NEWPROCESS *p)
{
#ifdef __GNUC__
    if (npz->p_status != PS_ZOMBIE) {	/* kernel stack needed */
	__asm__("pusha ; pushl %0 ; movl %%esp, %0" : "=m" (npz->kstack));
	npz->p_flags &= ~PF_NEW_KSTACK;
    }

    switch_to_process(p);

    if (npz->p_flags & PF_NEW_KSTACK) {  /* not run before */
	__asm__("movl %0, %%esp" : :"m" (npz->kstack));
	back_from_syscall();
    }
    else
	__asm__("movl %0, %%esp ; popl %0 ; popa ; nop " : :"m" (npz->kstack));

#else /* **not gnu** */

    if (npz->p_status != PS_ZOMBIE) {	/* no kernel stack */
	save_all((unsigned *)& npz->kstack);
	npz->p_flags &= ~PF_NEW_KSTACK;
    }

    switch_to_process(p);

    if (npz->p_flags & PF_NEW_KSTACK)	    /* not run before */
	new_stack_return((unsigned *)& npz->kstack);
    else
	restore_all((unsigned *)& npz->kstack);
#endif
}


/*
** small scheduler (called by from read/write)
*/
int schedule(void)
{
    static NEWPROCESS *last = &FIRST_PROCESS;
    NEWPROCESS *p;
    int found = 0;

    if (!npz->options.opt_schedule)
	return 0;

    if (npz->options.opt_printall) {
	puts("schedule:");
	for (p=&FIRST_PROCESS; p<=&LAST_PROCESS; p++)
	    if (p->pid>0)
		printf("pid = %d status = %d stack %lX\n",
		    p->pid, p->p_status, p->kstack);
    }

    p = last;
    do {
	if (p->p_status == PS_RUN && p != npz) {
	    found = 1;
	    break;
	}
	if (++p > &LAST_PROCESS)
	    p = &FIRST_PROCESS;
    } while (p != last);

    if (found) {
	last = p;
	switch_context(p);
	return 1;
    } else {
	if (npz->options.opt_printall)
	    printf("schedule: no other process found\n");
	return 0;
    }
}

/*
** init processes
*/
void init_this_process()
{
    NEWPROCESS *p;
    int i;

    for (p = &FIRST_PROCESS; p <= &LAST_PROCESS; p++)
	p->p_status = PS_EMPTY;

    npz = &RSX_PROCESS;
    npz->pid = current_pid++;
    npz->p_status = PS_RUN;
    npz->p_flags = PF_EXTENDER;
    npz->kstack = stackp16;
    npz->options = main_options;

    init_rsx_filetab();
    npz->filp[0] = & rsx_filetab[0];
    npz->filp[1] = & rsx_filetab[1];
    npz->filp[2] = & rsx_filetab[2];

    for (i = 0; i < N_DRIVES; i++)
	npz->cwd[i] = NULL;
    npz->cdrive = rm_getdrive ();
    get_cwd(npz->cdrive);
}

/*
** find empty processtable
*/
static NEWPROCESS *find_empty_process(void)
{
    NEWPROCESS *p;

    for (p = &FIRST_PROCESS; p <= &LAST_PROCESS; p++)
	if (p->p_status == PS_EMPTY)
	    return p;
    return NULL;
}

/*
** check illegal arguments
*/
int verify_illegal(NEWPROCESS * p, DWORD where, DWORD lenght)
{
    if (p->p_flags & PF_DJGPP_FILE) { /* 1.12 use first page ?!? */
	if (where < 0x00A8L || where + lenght >= p->membytes)
	    return 1;
    }
    else { /* EMX */
	if (where < 0x1000L || where + lenght >= p->membytes)
	    return 1;
    }
    return 0;
}

/*
** check illegal arguments (write access)
*/
int verify_illegal_write(NEWPROCESS * p, DWORD where, DWORD lenght)
{
    DWORD end = where + lenght;

    if (where >= p->data_start && end < p->data_end)
	return 0;
    if (where >= p->stack_down && end < p->stack_top)
	return 0;
    if (where >= p->init_brk && end < p->brk_value)
	return 0;
    return 1;
}

/*
** find processtable
*/
NEWPROCESS *find_process(unsigned pid)
{
    NEWPROCESS *p;

    for (p = &FIRST_PROCESS; p <= &LAST_PROCESS; p++)
	if (p->pid == pid)
	    return p;
    return (NEWPROCESS *) 0;
}

/*
** get wait_status
*/
unsigned sys_wait(unsigned *status)
{
    NEWPROCESS *p;
    int pid = -1;

#ifdef CONFIG_KDEB
    if (opt_kdeb && npz->pid == 2) {
	if (RSX_PROCESS.p_flags & PF_WAIT_WAIT) {
	    *status = RSX_PROCESS.wait_return;
	    RSX_PROCESS.p_flags &= ~PF_WAIT_WAIT;
	    return 1;
	}
    }
#endif

    do {
	for (p = &LAST_PROCESS; p >= &FIRST_PROCESS; p--)
	    if (p->pptr == npz)
		if (p->p_flags & PF_WAIT_WAIT) {
		    *status = p->wait_return;
		    pid = p->pid;
		    p->p_flags &= ~PF_WAIT_WAIT;
		    if (p->p_status == PS_ZOMBIE)
			clean_processtable(p);
		    return pid;
		}
	if (pid < 0) {
	    npz->p_status = PS_WAIT;	    /* exclude from scheduler */
	    if (!schedule()) {
		npz->p_status = PS_RUN;     /* include if no others */
		break;
	    }
	}
    } while (pid < 0);

    npz->p_status = PS_RUN;

    return pid;
}

/*
** wait for pid
*/
int sys_waitpid(int pid, unsigned *status)
{
    NEWPROCESS *p;
    int ret = -1;

    if (pid == -1)
	return -1;

    for (p = &LAST_PROCESS; p >= &FIRST_PROCESS; p--)
	if ((int) p->pid == pid)
	    do {
		if ((p->p_flags & PF_WAIT_WAIT)) {
		    *status = p->wait_return;
		    ret = p->pid;
		    p->p_flags &= ~PF_WAIT_WAIT;
		    if (p->p_status == PS_ZOMBIE)
			clean_processtable(p);
		    break;
		}
		npz->p_status = PS_WAIT;    /* exclude from scheduler */
		if (!schedule()) {
		    npz->p_status = PS_RUN;
		    break;
		}
	    } while (ret != -1);
    return ret;
}

/*
** free process memory and selectors from DPMI-Server
** close open file handles
*/
void free_process(NEWPROCESS * p)
{
    int i;

    if (p->code32sel == 0)	/* already cleaned ? */
	return;
    FreeMem(p->memhandle);
    FreeLDT(p->code32sel);
    FreeLDT(p->data32sel);
    p->code32sel = 0;

    for (i = 0; i < N_DRIVES; i++)
	if (p->cwd[i])
	    free (p->cwd[i]);

    if (p->p_flags & PF_USEDPMI10)
	rm_close((int) p->filehandle);
}

/*
** clean processtable
*/
void clean_processtable(NEWPROCESS * p)
{
    memset(p, 0, sizeof(NEWPROCESS));
}

/*
** switch to next program, save mathe state, set npz
*/
static int switch_to_process(NEWPROCESS * nextp)
{
    int i;

    /* if math used, save 387 regs */
/*
    if (npz->p_flags & PF_MATH_USED) {
	if (copro == 3)
	    save_emu_state(npz);
	else if (copro == 1)
	    do_fnsave(&(npz->npx));
    }
*/
    /* change process table */
    npz = nextp;
    cbrkcall = 0;

    /* load 387 regs (init 387) */
/*
    if (copro == 3) {
	if (npz->npx.soft.cwd) {
	    /- emulation done ? (check control word) -/
	    npz->p_flags |= PF_MATH_USED;
	    load_emu_state(npz);
	}
	if (npz->p_flags & PF_MATH_USED)
	    emu_switch(MATH_USED, npz->p_flags & PF_DEBUG);
	else
	    emu_switch(MATH_NEW, npz->p_flags & PF_DEBUG);
    } else if (copro == 1) {
	if (npz->p_flags & PF_MATH_USED) {
	    do_frstor(&(npz->npx));
	    npz->p_flags |= PF_MATH_USED;
	}
	else {
	    do_fninit();
	}
    }
*/
    for (i = 0; i < N_DRIVES; i++)
	if (npz->cwd[i] != NULL)
	    rm_chdir (npz->cwd[i]);

    rm_setdrive (npz->cdrive);

    return 0;
}

static int mk_cmdline(char **argv, char *cmd)
{
    int i, j;
    char *s;

    if (!argv[0]) {
	*cmd = 13;
	return 0;
    }
    for (i = 0, j = 0; (s = argv[i]) != NULL; i++) {
	while (*s)
	    cmd[j++] = *s++;
	cmd[j++] = ' ';
    }
    cmd[--j] = 13;
    return j;
}

/*
** build exec block for dos
*/
int execute_realmode_prg(char *filename, char **argv, char **env, int *ret_code)
{
    struct execb eb;
    char cmd_line[128];
    int len;

    /* build one string from arguments */
    len = mk_cmdline(argv + 1, cmd_line + 2);
    /* psp fields */
    cmd_line[0] = (char) (len + 1);
    cmd_line[1] = ' ';

    /* fill exec block */
    eb.psp_2c = ds16real + ((unsigned) env[0] >> 4);
    eb.psp_80_seg = ds16real;
    eb.psp_80_off = (UINT) cmd_line;
    eb.fcb1_seg = eb.fcb1_off = 0;
    eb.fcb2_seg = eb.fcb2_off = 0;

    *ret_code = rm_exec(filename, &eb);

    if (*ret_code == -1)
	return -1;

    *ret_code = rm_get_exit_status() & 0xFF;
    return 0;
}

/*
** execute a real-mode program
**
** - get RealMode callback address from host (host switch from RM to PM)
** - set this address to DOS 0x21
** - dup emx file handles to dos std handles
*/
extern void rm_handler(void);	    /* our asm handler	    */
extern UINT rm21_seg, rm21_off;     /* org real mode 0x21   */

char realmode_filp[N_FILES];	    /* real mode file types */

int realmode_prg(char *filename, char **argv, char **env)
{
    UINT org_filp[3];		    /* real mode std files  */
    int ret_code, ok, i;
    int fCB = 0;		    /* flag Callb installed */
    TRANSLATION tr;		    /* translation regs     */
    UINT cb_seg, cb_off;	    /* callback address     */

    /* dup father std handles if dos file	    */
    /* needed for dos fnct ah<0Ch without handles   */
    for (i = 0; i < 3; i++) {
	if (npz->filp[i] && npz->filp[i]->f_doshandle != i) {
	    if (npz->filp[i]->f_doshandle != -1) {
		org_filp[i] = rm_dup(i);
		rm_dup2(npz->filp[i]->f_doshandle, i);
		realmode_filp[i] = HT_DEV_CON;
	    }
	    else { /* future: others */
		org_filp[i] = -1;
		realmode_filp[i] = HT_UPIPE;
		fCB = 1; /* pipe: we need callbacks */
	    }
	} else {
	    org_filp[i] = -1;
	    realmode_filp[i] = HT_DEV_CON;
	}
    }

    if (fCB) { /* only if pipes used */
	if (AllocRMcallAddress(code16sel, (UINT) rm_handler,
		    &tr, &cb_seg, &cb_off))
	    fCB = 0;
	else {
	    GetRealModeVector(0x21, &rm21_seg, &rm21_off);
	    SetRealModeVector(0x21, cb_seg, cb_off);
	}
    }

    ok = execute_realmode_prg(filename, argv, env, &ret_code);

    if (fCB) {
	SetRealModeVector(0x21, rm21_seg, rm21_off);
	FreeRMcallAddress( cb_seg, cb_off);
    }

    for (i = 0; i < 3; i++)
	if (org_filp[i] != -1) {
	    rm_dup2(org_filp[i], i);
	    rm_close(org_filp[i]);
	}

    if (ok)
	return doserror_to_errno(ret_code);
    else {
	EAX = (long) ret_code & 0xff;	/* return value */
	current_pid++;
	return 0;
    }
}

static void copy_filedescriptors(NEWPROCESS * father, NEWPROCESS * child)
{
    int i;

    for (i = 0; i < N_FILES; i++)
	if (father->filp[i]) {
	    child->filp[i] = father->filp[i];
	    child->filp[i]->f_count ++;
	}
}

static void copy_cwd (NEWPROCESS * from, NEWPROCESS * to)
{
    int i;

    for (i = 0; i < N_DRIVES; i++)
	if (from->cwd[i]) {
	    to->cwd[i] = (char *)malloc (1 + strlen(from->cwd[i]));
	    if (to->cwd[i])
		strcpy (to->cwd[i], from->cwd[i]);
	}
	else
	    to->cwd[i] = NULL;

    to->cdrive = from->cdrive;
}

/*
 * get_cwd (int drive)
 *
 * get the current directory of DRIVE and store it in
 * npz->cwd[drive] of the current process
 */
void get_cwd (int drive)
{
    char cwd_buf[256];

    if (drive < 0 || drive >= N_DRIVES)
	return;

    if (npz->cwd[drive])
	free (npz->cwd[drive]);

    cwd_buf[0] = drive + 'A';
    cwd_buf[1] = ':';
    cwd_buf[2] = '\\';
    rm_getcwd (drive + 1, &cwd_buf[3]);

    npz->cwd[drive] = (char *)malloc(1 + strlen(cwd_buf));
    if (npz->cwd[drive])
	strcpy(npz->cwd[drive], cwd_buf);
}

/*
 * store_cwd (int drive)
 *
 * get the current directory of DRIVE (a=0, b=1, c=2, ...)
 * and store it in p->cwd[drive] of all active processes
 */
void store_cwd (int drive)
{
    char cwd_buf[256];
    int len;
    NEWPROCESS *p;

    if (drive < 0 || drive >= N_DRIVES)
	return;

    cwd_buf[0] = drive + 'A';
    cwd_buf[1] = ':';
    cwd_buf[2] = '\\';
    rm_getcwd (drive + 1, &cwd_buf[3]);
    len = 1 + strlen (cwd_buf);

    for (p = &FIRST_PROCESS; p <= &LAST_PROCESS; p++)
	if (p->p_status != PS_EMPTY && p->p_status != PS_ZOMBIE)
	    if (p->cwd[drive] == NULL) {
		p->cwd[drive] = (char *)malloc(len);
		if (p->cwd[drive])
		    strcpy(p->cwd[drive], cwd_buf);
	    }
}

struct StubInfo {
    BYTE  magic[16];
    DWORD struct_length;
    char  go32[16];
    BYTE  need_version[4];
    DWORD min_stack;
    DWORD keep_on_spawn;
    BYTE  file_to_run[15];
    BYTE  enable_globbing;
    DWORD free_conv_memory;
} stub_info = {
    "StubInfoMagic!!",
    sizeof(struct StubInfo),
    "RSX",
    { 0,'b',0,4 },
    262144L,
    0L,
    "",
    0,
    0L
};

struct {
  DWORD size_structure;
  DWORD linaddr_primary_screen;
  DWORD linaddr_secondary_screen;
  DWORD linaddr_transfer_buffer;
  DWORD size_transfer_buffer; /* >= 4k */
  DWORD pid;
  BYTE	master_pic;
  BYTE	slave_pic;
  WORD	selector_linear_memory;
  DWORD linaddr_stubinfo;
  DWORD linaddr_org_psp;
  WORD	run_mode;
  WORD	run_mode_info;
} go32_info_block;

static void set_go32_info_block()
{
    go32_info_block.size_structure = sizeof(go32_info_block);
    go32_info_block.linaddr_primary_screen = 0xB0000;
    go32_info_block.linaddr_secondary_screen = 0xB8000;
    go32_info_block.linaddr_transfer_buffer =
	(((DWORD) ds16real << 4) + (DWORD) (UINT) iobuf) + 4096L;
    go32_info_block.size_transfer_buffer = 4096L;
    go32_info_block.pid = 42;
    go32_info_block.master_pic = 8;
    go32_info_block.slave_pic = 70;
    go32_info_block.selector_linear_memory = dosmem_sel;
    go32_info_block.linaddr_stubinfo =
	((DWORD) ds16real << 4) + (DWORD) (UINT) & stub_info;
    go32_info_block.linaddr_org_psp =
	((DWORD) cs16real << 4);
    go32_info_block.run_mode = 3;
    go32_info_block.run_mode_info = 0x090;
}

/*
** load, init and switch to another 32bit program
*/
int exec32(unsigned mode, char *name, int argc, char **argp, int envc, char **envp)
{
    NEWPROCESS *child, *father;
    int i, ret;

#ifdef CONFIG_KDEB
    static kernel_trace = 0;

    ret = strlen(name);
    if (opt_kdeb && ret >= 8 && strcmp(name+(ret-8), "rsxd.emx") == 0) {
	if (*kdeb_program) {
	    name = kdeb_program;
	    kernel_trace = 1;
	    printf("KERNEL DEBUG MODE with second prg %s\n", name);
	}
	else {
	    printf("KERNEL DEBUG MODE (but no second prg)\n");
	    return EMX_EINVAL;
	}
    }
#endif

    /* look for a empty slot */
    child = find_empty_process();
    if (child == NULL)
	return EMX_EAGAIN;

    child->options = main_options;
    child->options.opt_schedule = npz->options.opt_schedule;
    child->options.opt_os2 = npz->options.opt_os2;

    /* try load a a.out program */
    if ((ret = load_protected_program(name, child)) != 0) {
	child->p_status = PS_EMPTY;
	return ret;
    }

    if (!(child->kstack = alloc_kernel_stack())) {
	puts("no kernel stack");
	child->p_status = PS_EMPTY;
	return EMX_EAGAIN;
    }
    child->p_flags |= PF_NEW_KSTACK;

    cpy_exename_to_stack(child, name);

    /* copy arguments,environment */
    argvenv(argc, argp, envc, envp, child);

    /* setup new process table */
    child->pid = current_pid++;
    child->pptr = npz;
    if (mode == P_DEBUG)
	child->p_flags |= PF_DEBUG;
    /* start values */
    child->regs.eip = child->entry;
    child->regs.esporg = child->stackp32;
    child->regs.esp = child->stackp32 - 12L;	/* iret frame */
    /* for DJGPP: first MB */
    child->regs.eax = ((DWORD) dosmem_sel << 16) | 0x00007008L;
    child->regs.ebx = 0;
    child->regs.ecx = 0;
    child->regs.edx = ((DWORD) ds16real << 4) + (DWORD) (UINT) &go32_info_block;
    child->regs.esi = 0;
    child->regs.edi = 0;
    child->regs.ebp = 0;
    child->regs.cs = child->code32sel;
    child->regs.ds = child->regs.es = child->regs.ss = child->data32sel;
    child->regs.fs = child->data32sel;
    child->regs.gs = dosmem_sel;
    child->regs.eflags = 0x3202;
    child->time_tic = time_tic;
    npz->cptr = child;

    if (child->p_flags & PF_DJGPP_FILE)
	set_go32_info_block();

    copy_cwd(npz, child);
    copy_filedescriptors(npz, child);
    father = npz;
    npz = child;
    for (i=0; i<N_FILES; i++)
	if (FD_ISSET(i, &father->close_on_exec))
	    sys_close(i);
    npz = father;

    /* if current prg extender, switch to first emx-porgram */
    if (npz->p_flags & PF_EXTENDER) {
	djio_init();	/* before first prg, init djio */
	child->p_status = PS_RUN;
	switch_context(child);
	shut_down(0);	/* should never execute */
    }

#ifdef CONFIG_KDEB
    if (opt_kdeb && kernel_trace == 1) {
	EAX = child->pid = 1;
	child->p_status = PS_STOP;
	RSX_PROCESS.cptr = child;
	RSX_PROCESS.regs.eip = 0x10000;
	RSX_PROCESS.regs.esp = (DWORD) & ret;
    }
#endif

    if (mode == P_WAIT || mode == P_DETACH) {
	MarkPageDemand(npz->memaddress, npz->membytes);
	npz->p_status = PS_STOP;
	child->p_status = PS_RUN;
	switch_context(child);
	npz->p_status = PS_RUN;
	EFLAGS &= ~1L;
	if (child->wait_return & 0xFF)	/* signal */
	    EAX = 3;
	else
	    EAX = (child->wait_return >> 8) & 0xFF;
	child->p_flags &= ~PF_WAIT_WAIT;
	clean_processtable (child);
    }
    else if (mode == P_NOWAIT || mode == P_SESSION) {
	child->p_status = PS_RUN;
	if (!npz->options.opt_schedule || mode == P_SESSION)
	    switch_context(child);
	EFLAGS &= ~1L;
	EAX = (DWORD) child->pid;
    } else if (mode == P_DEBUG) {
	EAX = child->pid;	/* return process no */
	child->p_status = PS_STOP;
    } else if (mode == P_OVERLAY) {
	NEWPROCESS *this = npz;
	npz->p_flags &= ~PF_MATH_USED;	/* don't save mathe state */
	child->pid = npz->pid;		/* take the correct pid */
	for (i = 0; i < N_FILES; i++)
	    if (npz->filp[i] != NIL_FP)
		sys_close(i);
	switch_to_process(npz->pptr);	/* switch to parent */
	free_process(this);		/* free process memory */
	free_kernel_stack(this->kstack);
	clean_processtable(this);	/* free process table */
	npz->cptr = child;		/* new child */
	child->pptr = npz;		/* new parent */
	switch_to_process(child);	/* switch to new child */
	child->p_status = PS_RUN;
    } else
	return EMX_EINVAL;

    return 0;
}

/*
** load, init and exec a 16-bit realmode-program
*/
int execRM(unsigned mode, char *name, char **argp, char **envp)
{
    NEWPROCESS *child = NULL;
    int ret;

    if (mode == P_DEBUG)
	return EMX_EINVAL;

    if (mode == P_NOWAIT || mode == P_SESSION) {
	/* look for a empty slot */
	child = find_empty_process();
	if (child == NULL)
	    return EMX_EAGAIN;

	/* setup new process table */
	child->pid = current_pid;
	child->pptr = npz;
	npz->cptr = child;
	child->p_status = PS_STOP;
    }

    ret = realmode_prg(name, argp, envp);
    if (ret) {
	if (child != NULL)
	    clean_processtable(child);
	return ret;
    }

    if (mode == P_NOWAIT || mode == P_SESSION) {
	child->p_status = PS_ZOMBIE;
	child->wait_return = ret << 8;
	child->p_flags |= PF_WAIT_WAIT; /* allow wait() for father */
	EAX = (DWORD) child->pid;
    } else if (mode == P_OVERLAY) {
	do_exit4c(0);
    }

    send_signal(npz, SIGCLD);
    return 0;
}

/*
** fork current process
*/
int sys_fork(void)
{
    NEWPROCESS *child;
    int pid;

    child = find_empty_process();
    if (child == NULL)
	return -EMX_EAGAIN;

    /* copy process tables */
    memcpy(child, npz, sizeof(NEWPROCESS));

    child->p_status = PS_EMPTY; /* if error, leave empty */

    if (!(child->kstack = alloc_kernel_stack())) {
	puts("no kernel stack");
	return -EMX_EAGAIN;
    }
    child->p_flags |= PF_NEW_KSTACK;

    /* MEMORY per DPMI besorgen */
    if (child->p_flags & PF_USEDPMI10) {
	if (AllocLinearMemory(npz->membytes, 0L, 0L, &child->memhandle, &child->memaddress))
	    return -EMX_ENOMEM;
    } else {
	if (AllocMem(npz->membytes, &child->memhandle, &child->memaddress))
	    return -EMX_ENOMEM;
    }

    if (AllocLDT(2, &(child->code32sel))) {
	FreeMem(child->memhandle);
	return -EMX_EIO;
    }

    child->data32sel = child->code32sel + sel_incr;

    SetBaseAddress(child->code32sel, child->memaddress);
    SetBaseAddress(child->data32sel, child->memaddress);
    SetAccess(child->code32sel, APP_CODE_SEL, DEFAULT_BIT | GRANULAR_BIT);
    SetAccess(child->data32sel, APP_DATA_SEL, DEFAULT_BIT | GRANULAR_BIT);
    SetLimit(child->code32sel, lsl32(npz->code32sel));
    SetLimit(child->data32sel, lsl32(npz->data32sel));

    child->regs.cs = child->code32sel;
    child->regs.ds = child->regs.es = child->regs.ss = child->data32sel;
    child->regs.fs = child->data32sel;
    child->regs.gs = dosmem_sel;/* first Megabyte DOS */

    pid = child->pid = current_pid++;
    child->pptr = npz;
    child->cptr = NULL;
    child->time_alarm = 0;
    child->time_tic = time_tic;
    child->filehandle = 0;		/* for swapper !! */
    npz->cptr = child;

    copy_cwd(npz, child);
    copy_filedescriptors(npz, child);

    /* text segment */
    cpy32_32(npz->data32sel, npz->text_start,
	     child->data32sel, child->text_start,
	     npz->text_end - npz->text_start);

    /* data/bss segment */
    cpy32_32(npz->data32sel, npz->data_start,
	     child->data32sel, child->data_start,
	     npz->data_end - npz->data_start);

    /* heap segment */
    cpy32_32(npz->data32sel, npz->init_brk,
	     child->data32sel, child->init_brk,
	     npz->brk_value - npz->init_brk);

    /* stack segment */
    cpy32_32(npz->data32sel, ESP,
	     child->data32sel, ESP,
	     npz->stack_top - ESP);

    /* child returns null */
    child->regs.ecx = 0;
    child->regs.eax = 0;

    child->p_status = PS_RUN;
    if (!npz->options.opt_schedule)
	switch_context(child);

    return pid;
}

/*
** global clean-up for extender
*/
void shut_down(int exit_code)
{
    NEWPROCESS *p;

    /* free memory,selectors */
    for (p = &FIRST_PROCESS; p != NULL && p <= &LAST_PROCESS; p++)
	free_process(p);

    if (main_options.opt_printall)
	printf("clock: %lu ticks = %lu sec\n", time_tic, time_tic * 10 / 182);

#ifdef CONFIG_KDEB
    if (opt_kdeb)
	KDEB_disable_breakpoints();
#endif

    /* clean_up ints,exceptions,... */
    clean_up();

    /* leave protected mode */
    protected_to_real(exit_code);
}

#define WIDIEXTRA
#ifdef WIDIEXTRA
/*
** a ResizeMem for DPMI-Server with virtual swap-file
**	QDPMI breaks
*/
static int
MyResizeMem (NEWPROCESS *p, DWORD bytes, DWORD handle,
		DWORD *newHandle, DWORD *newAddress)
{
    UINT	memsel;

    if (p->p_flags & PF_USEDPMI10)  /* no resize if dpmi 1.0 */
	return -1;

    if (ResizeMem (bytes, handle, newHandle, newAddress) == 0)
	return 0;

    if (AllocMem (bytes, newHandle, newAddress))
	return -1;

    if (AllocLDT (1, &memsel)) {
	FreeMem (*newHandle);
	return -1;
    }

    SetBaseAddress (memsel, *newAddress);
    SetAccess(memsel, APP_DATA_SEL, BIG_BIT | GRANULAR_BIT);
    SetLimit(memsel, bytes - 1L);

    cpy32_32(	p->data32sel, p->text_start,
		memsel, p->text_start,
		p->text_end - p->text_start);
    cpy32_32(	p->data32sel, p->data_start,
		memsel, p->data_start,
		p->data_end - p->data_start);
    cpy32_32(	p->data32sel, p->init_brk,
		memsel, p->init_brk,
		p->brk_value - p->init_brk);
    cpy32_32(	p->data32sel, p->regs.esp,
		memsel, p->regs.esp,
		p->stack_top - p->regs.esp);

    FreeMem (handle);	/* release old mem */
    FreeLDT (memsel);
    return 0;
}
#define ResizeMem(s,h,nh,na) MyResizeMem(p,s,h,nh,na)
#endif

/*
** get more memory from DPMI-Server
*/
DWORD getmem(DWORD bytes, NEWPROCESS * p)
{				/* ret: old break value */
    DWORD retv, pagealign;
    DWORD newaddress, newhandle;

    if (bytes <= p->pagefree) {
	retv = p->brk_value;
	p->brk_value += bytes;
	p->pagefree -= bytes;
    } else {
	if (p->p_flags & PF_DJGPP_FILE)
	    pagealign = (bytes + 0xFFFF) & 0xFFFF0000L;
	else
	    pagealign = (bytes + 0xFFF) & 0xFFFFF000L;
	MarkPageDemand(p->memaddress, p->membytes);
	if (ResizeMem(p->membytes + pagealign, p->memhandle,
		      &newhandle, &newaddress))
	    return -1L;
	p->membytes += pagealign;
	retv = p->brk_value;
	p->brk_value += bytes;
	p->pagefree += (pagealign - bytes);
	if (!p->options.opt_memaccess)
	    SetLimit(p->data32sel, p->membytes - 1);
	if (p->memhandle != newhandle) {
	    p->memhandle = newhandle;
	}
	if (p->memaddress != newaddress) {
	    p->memaddress = newaddress;
	    SetBaseAddress(p->code32sel, p->memaddress);
	    SetBaseAddress(p->data32sel, p->memaddress);
	    SetBaseAddress(p->data32sel + sel_incr, p->memaddress);
	    if (p->options.opt_memaccess && p->options.opt_printall)
		puts("warning: memaccess pointer is not valid");
	}
	/* dammed! djgpp (gcc.exe) cause gp-fault, if not */
	if ((p->p_flags & PF_DJGPP_FILE) || p->options.opt_zero)
	    bzero32(p->data32sel, retv, pagealign);
    }
    return retv;
}

/*
** exit process and switch to father
*/
int do_exit4c(int bysig)
{
    NEWPROCESS *father;
    int i;
    unsigned retval;

    retval = AX & 0xFF;

    /* close all files */
    for (i = 0; i < N_FILES; i++)
	if (npz->filp[i])
	    sys_close(i);

    /* get parent */
    father = npz->pptr;

    npz->wait_return = (bysig) ? bysig : retval << 8;
    npz->p_status = PS_ZOMBIE;
    npz->p_flags |= PF_WAIT_WAIT;	/* allow wait() for father */
    npz->p_flags &= ~PF_MATH_USED;	/* don't save mathe state */
    free_process(npz);
    free_kernel_stack(npz->kstack);

    /* if father process is Extender */
    if (father->p_flags & PF_EXTENDER)
	shut_down(retval);

    if (father->p_status == PS_WAIT)
	father->p_status = PS_RUN;

    send_signal(father, SIGCLD);

    switch_context(father);

    puts("RSX: end of do_exit4c() should never reached");
    shut_down(0);
    return 0;
}
