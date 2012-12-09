/*****************************************************************************
 * FILE: kdeb.c                                                              *
 *									     *
 * DESC:								     *
 *      - support for kernel debugging                                       *
 *      - kernel ptrace                                                      *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "DPMI.H"
#include "KDEB.H"

#include "PROCESS.H"
#include "PTRACE.H"
#include "DOSERRNO.H"
#include "START32.H"
#include "ADOSX32.H"
#include "SIGNALS.H"
#include "PRINTF.H"

#define SINGLE_STEP	0x100
#define RESUME_FLAG	0x10000L
#define FLAG_MASK	0x00000dd9L
#define BREAKPOINT	0xcc

#define MAX_HARD	4
#define BP_FLAG_MASK	0x0FFF
#define BP_ENABLE	0x1000
#define BP_DISABLE	0x2000

typedef struct {
    DWORD   address;
    WORD    handle;
    WORD    flags;
    DWORD   data;
    DWORD   pt_address;
} HardBreakPoint;

static HardBreakPoint hbp[MAX_HARD];
static DWORD memaddress;

static int KDEB_set_hard_break(DWORD at_address, WORD flags)
{
    int i;

    if (!memaddress)
	GetBaseAddress(GetCS(), & memaddress);

    for (i = 1; i < MAX_HARD; i++)
	if (hbp[i].flags == 0)
	    break;
    if (i >= MAX_HARD)
	return -1;
    if (SetDebugWatchpoint(memaddress + at_address,
			   flags & BP_FLAG_MASK, &hbp[i].handle)) {
	puts("set_hard_break() error");
	return -1;
    }
    else {
	hbp[i].address = at_address;
	hbp[i].flags = flags | BP_ENABLE;
	return i;
    }
}

static int KDEB_del_hard_break(int no)
{
    if (no >= MAX_HARD || hbp[no].flags == 0)
	return -1;

    if (hbp[no].flags & BP_DISABLE) {
	/* already clear */
	hbp[no].address = 0;
	hbp[no].flags = 0;
	hbp[no].handle = 0;
	return 0;
    }

    if (ClearDebugWatchpoint(hbp[no].handle)) {
	puts("del_hard_break() error");
	return -1;
    }
    else {
	hbp[no].address = 0;
	hbp[no].flags = 0;
	hbp[no].handle = 0;
	return 0;
    }
}

static int KDEB_disable_hard_break(int no)
{
    if (no <= 1 && no >= MAX_HARD)
	return -1;
    if (hbp[no].flags & BP_DISABLE)
	return -1;
    if (ClearDebugWatchpoint(hbp[no].handle)) {
	puts("disable_hard_break() error");
	return -1;
    }
    else {
	hbp[no].flags &= BP_FLAG_MASK;
	hbp[no].flags |= BP_DISABLE;
	return 0;
    }
}

static int KDEB_enable_hard_break(int no)
{
    if (no <= 1 && no >= MAX_HARD)
	return -1;
    if (hbp[no].flags & BP_ENABLE)
	return -1;
    if (SetDebugWatchpoint(memaddress + hbp[no].address,
			hbp[no].flags & BP_FLAG_MASK, &hbp[no].handle)) {
	puts("enable_hard_break() error");
	return -1;
    }
    else {
	hbp[no].flags &= BP_FLAG_MASK;
	hbp[no].flags |= BP_ENABLE;
	return 0;
    }
}


int KDEB_check_breakpoints(DWORD pc)
{
    int i;
    WORD j;

    for (i = 0; i < MAX_HARD; i++)
	if (hbp[i].flags & BP_ENABLE) {
	    j = 0;
	    GetStateDebugWatchpoint(hbp[i].handle, &j);
	    if (j)
		ResetDebugWatchpoint(hbp[i].handle);
	}
    return ((i >= MAX_HARD) ? -1 : i);
}

void KDEB_enable_breakpoints(void)
{
    int i;
    for (i = 0; i < MAX_HARD; i++)
	if ((hbp[i].flags & BP_DISABLE) && hbp[i].pt_address)
	    KDEB_enable_hard_break(i);
}

void KDEB_disable_breakpoints(void)
{
    int i;
    for (i = 0; i < MAX_HARD; i++)
	if ((hbp[i].flags & BP_ENABLE) && hbp[i].pt_address)
	    KDEB_disable_hard_break(i);
}



/* +++++++++++++ PTRACE in KERNEL +++++++++++++++++ */

/* crt0.s */
extern unsigned long rsx_data_start;
extern unsigned long rsx_data_ends;

/*
 * simulate ptrace(pid,POKETEXT,address,BREAKPOINT/ORGINAL)
 */
static int KDEB_ptrace_poketext(DWORD address, DWORD value)
{
    int i;
    DWORD bp = value;
    DWORD bp_addr = address;

    for (i = 0 ; i <= 3 ; i++) {
	if ((bp & 0xff) == BREAKPOINT)
	    break;
	bp >>= 8;
	bp_addr ++;
    }

    /* ptrace: set breakpoint in text */
    if (i <= 3) {
	printf("POKETEXT new break %lX\n", address);
	if ((i = KDEB_set_hard_break(bp_addr, BREAK_CODE)) > 0) {
	    KDEB_disable_hard_break(i);
	    hbp[i].data = value;
	    hbp[i].pt_address = address;
	    return 0;
	}
	else {
	    printf("POKETEXT break fail %lX\n", address);
	    return -1;
	}
    }
    /* ptrace: delete breakpoint in text */
    else {
	for (i = 0; i < MAX_HARD; i++)
	    if (address == hbp[i].pt_address)
		break;
	if (i >= MAX_HARD) {
	    printf("POKETEXT address %lX\n", address);
	    * (DWORD *) address = value;
	}
	else {
	    printf("POKETEXT delete break %lX\n", address);
	    hbp[i].data = hbp[i].pt_address = 0;
	    KDEB_del_hard_break(i);
	}
	return 0;
    }
}

/*
 * simulate ptrace(pid,PEEKTEXT,address, 0)
 */
static int KDEB_ptrace_peektext(DWORD address, DWORD * value)
{
    int i;

    for (i = 0; i < MAX_HARD; i++)
	if (address == hbp[i].pt_address)
	    break;
    if (i >= MAX_HARD) {
	/* printf("PEEKTEXT address %lX\n", address); */
	*value = * (DWORD *) address;
    }
    else {
	/* printf("PEEKTEXT peek break %lX\n", address); */
	*value = hbp[i].data;
    }
    return 0;
}

/*
 * simulate ptrace(pid,POKEDATA,address,value)
 */
static int KDEB_ptrace_pokedata(DWORD address, DWORD value)
{
    if (address < 4096 || address >= rsx_data_ends) {
	puts("KDEB: illegal address");
	return EMX_EINVAL;
    }
    if (address == (DWORD) & npz) {
	puts("KDEB: don't touch npz");
	return EMX_EINVAL;
    }
    * (DWORD *) address = value;
    return 0;
}

/*
 * simulate ptrace(pid,PEEKDATA,address,0)
 */
static int KDEB_ptrace_peekdata(DWORD address, DWORD * value)
{
    if (address < 4096 || address >= rsx_data_ends) {
	puts("KDEB: illegal address");
	return EMX_EINVAL;
    }
    if (address == (DWORD) & npz) {
	*value = (DWORD) RSX_PROCESS.cptr;
	return 0;
    }
    *value = * (DWORD *) address;
    return 0;
}

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */


int KDEB_ptrace(int request, DWORD addr, DWORD data, DWORD * ret)
{
    /* printf("KDEB %d %lX %lX\n", request, addr, data); */

    *ret = 0;

    switch (request) {

    case PTRACE_TRACEME:
    case PTRACE_SESSION:
	return 0;

    case PTRACE_PEEKTEXT:
    case PTRACE_PEEKDATA:
	if (addr > 0x10000L && addr < rsx_data_start)
	    return KDEB_ptrace_peektext(addr, ret);
	else
	    return KDEB_ptrace_peekdata(addr, ret);

    case PTRACE_POKETEXT:
	if (addr > 0x10000L && addr < rsx_data_start)
	    return KDEB_ptrace_poketext(addr, data);
	else
	    return KDEB_ptrace_pokedata(addr, data);

    case PTRACE_EXIT:
	KDEB_disable_breakpoints();
	return 0;

    case PTRACE_PEEKUSER:
	if (addr == 0x30) {	/* u_ar0  */
	    *ret = 0xE0000000 + ((DWORD) (UINT) & (RSX_PROCESS.regs));
	    return 0;
	} else {		/* peek regs */
	    DWORD *peekat;
	    peekat = (DWORD *) (UINT) (addr);

	    if (peekat < &(RSX_PROCESS.regs.gs) || peekat > &(RSX_PROCESS.regs.ss))
		return EMX_EIO;

	    *ret = *peekat;

	    if (peekat == &(RSX_PROCESS.regs.eip)) {
		int i;
		for (i = 0; i < MAX_HARD; i++)
		    if (RSX_PROCESS.regs.eip == hbp[i].address)
			break;
		if (i < MAX_HARD)
		    (*ret)++;
	    }

	    return 0;
	}

    case PTRACE_POKEUSER:
	/* don't change kernel registers ! */
	*ret = data;
	return 0;

    case PTRACE_STEP:
	/* printf("PTRACE step\n"); */
	RSX_PROCESS.regs.eflags |= SINGLE_STEP;
	npz = RSX_PROCESS.cptr;
	KDEB_rsx32_orgjmp();
	return 0;

    case PTRACE_RESUME:
	/* printf("PTRACE resume\n"); */
	RSX_PROCESS.regs.eflags &= ~SINGLE_STEP;
	npz = RSX_PROCESS.cptr;
	KDEB_enable_breakpoints();
	/* start first program */
	if (npz == & process[2] && npz->regs.eip == 0x10000)
	    back_from_syscall();
	KDEB_rsx32_orgjmp();
	return 0;

    default:
	return EMX_EIO;
    }
}

struct POPAD {
    unsigned edi;
    unsigned esi;
    unsigned ebp;
    unsigned esp;
    unsigned ebx;
    unsigned edx;
    unsigned ecx;
    unsigned eax;
};

#define RSXREG RSX_PROCESS.regs

static unsigned rsx_eip;
static unsigned rsx_flags;
static unsigned rsx_esp;

void KDEB_debug_handler(void)
{
    KDEB_check_breakpoints(RSXREG.eip);
    KDEB_disable_breakpoints();

    /*
    printf("Debug fault at %04X:%08lX  efl=%08lX\n",
	RSXREG.cs & 0xFFFF, RSXREG.eip, RSXREG.eflags);
    */

    rsx_eip = RSXREG.eip;
    RSX_PROCESS.p_status = PS_STOP;
    RSX_PROCESS.wait_return = (SIGTRAP << 8) | 127;
    RSX_PROCESS.p_flags |= PF_WAIT_WAIT;
    RSX_PROCESS.cptr = npz;
    npz = & process[1];
    EAX = ECX = 0;
    npz->p_status = PS_RUN;

    /* ptrace should go on other stack */
    stackp16 -= 8000;

    back_from_syscall();
}

volatile void KDEB_rsx32_orgjmp(void)
{
    struct POPAD pop;

    /* ptrace should go on other stack */
    stackp16 += 8000;

    pop.eax = RSXREG.eax;
    pop.ecx = RSXREG.ecx;
    pop.edx = RSXREG.edx;
    pop.ebx = RSXREG.ebx;
    pop.esp = RSXREG.esp;
    pop.ebp = RSXREG.ebp;
    pop.esi = RSXREG.esi;
    pop.edi = RSXREG.edi;

    rsx_eip = RSXREG.eip;
    rsx_flags = RSXREG.eflags;
    rsx_esp = RSXREG.esporg;

loop:
    __asm__ __volatile (
	"movl %0, %%esp \n\t"
	"popa \n\t"
	"nop \n\t"
	"movl  _rsx_esp, %%esp \n\t"
	"pushl _rsx_flags\n\t"
	"pushl %%cs \n\t"
	"pushl _rsx_eip \n\t"
	"iret \n\t"
	:
	: "r" ((unsigned)&pop)
	);
    goto loop;
}
