/*****************************************************************************
 * FILE: signals.c							     *
 *									     *
 * DESC:								     *
 *	- signal handling						     *
 *	- exception handler						     *
 *	- DPMI 1.0 page fault handler					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include <string.h>
#include "PRINTF.H"
#include "DPMI.H"
#include "DPMI10.H"
#include "RMLIB.H"
#include "PROCESS.H"
#include "SIGNALS.H"
#include "START32.H"
#include "CDOSX32.H"
#include "EXCEP32.H"
#include "ADOSX32.H"
#include "COPY32.H"
#include "RSX.H"
#include "LOADPRG.H"
#include "KDEB.H"
#include "DOSERRNO.H"

#define SIGSA(no)   ((no)-1)
#define SIGMASK(no) (1L<<((no)-1))
#define SIGBLOCK    (~(SIGSA(SIGKILL)))

#define SA_NOTBSD   (SA_SYSV | SA_ACK)

/* local functions */
static int exception2signal(WORD);
static int do_signal(int);
static void print_regs_exception(void);

extern char *sigtext[];

/*
** give back a signal no from a hardware exception fault no
*/
static int exception2signal(WORD faultno)
{
    int signal;

    switch (faultno) {
    case 0:
    case 2:
    case 4:
    case 5:
    case 6:
    case 8:
    case 10:
    case 15:
	signal = SIGILL;
	break;
    case 1:
    case 3:
	signal = SIGTRAP;
	break;
    case 7:
    case 16:
	signal = SIGFPE;
	break;
    case 9:
    case 11:
    case 12:
    case 13:
    case 14:
    case 17:
	signal = SIGSEGV;
	break;
    default:
	signal = SIGSEGV;
	break;
    }

    return signal;
}

/*
** set signal for one process
*/
int send_signal(NEWPROCESS * p, int signal)
{
    if (!p || signal < 0 || signal >= MAX_SIGNALS)
	return EMX_EINVAL;
    if (signal != 0)		/* kill(pid,0): check if pid is ok */
	p->sig_raised |= SIGMASK(signal);
    return 0;
}

static void check_pending(int signum)
{
    struct sigaction *p;

    p = signum - 1 + npz->sigaction;
    if (p->sa_handler == SIG_IGN) {
	if (signum == SIGCHLD)
	    return;
	npz->sig_raised &= ~ SIGMASK(signum);
	    return;
    }
    if (p->sa_handler == SIG_DFL) {
	if (signum != SIGCHLD)
	    return;
	npz->sig_raised &= ~ SIGMASK(signum);
	return;
    }
}

int sys_sigaction(int signum, DWORD action, DWORD oldaction)
{
    struct sigaction new_sa, *p;

    if (signum<1 || signum>=MAX_SIGNALS || signum==SIGKILL)
	return -EMX_EINVAL;
    p = signum - 1 + npz->sigaction;
    if (action) {
	if (verify_illegal(npz, action, sizeof(action)))
	    return -EMX_EINVAL;
	cpy32_16(npz->data32sel, action, &new_sa, sizeof(struct sigaction));
	new_sa.sa_mask |= SIGMASK(signum);
	new_sa.sa_mask &= SIGBLOCK;
    }
    if (oldaction) {
	if (verify_illegal_write(npz, oldaction, sizeof(oldaction)))
	    return -EMX_EINVAL;
	cpy16_32(npz->data32sel, oldaction, p, sizeof(struct sigaction));
	}
    if (action) {
	*p = new_sa;
	check_pending(signum);
    }
    return 0;
}

int sys_sigpending(DWORD set_addr)
{
    DWORD set = npz->sig_blocked & npz->sig_raised;

    if (verify_illegal_write(npz, set, 4))
	return -EMX_EINVAL;
    store32(npz->data32sel, set_addr, set);
    return 0;
}

int sys_sigprocmask(int how, DWORD set, DWORD oset)
{
    sigset_t new_set, old_set = npz->sig_blocked;

    if (set) {
	if (verify_illegal(npz, set, sizeof(sigset_t)))
	    return -EMX_EINVAL;
	new_set = read32(npz->data32sel, set) & SIGBLOCK;

	switch (how) {
	    case SIG_BLOCK:
		npz->sig_blocked |= new_set;
		break;
	    case SIG_UNBLOCK:
		npz->sig_blocked &= ~new_set;
		break;
	    case SIG_SETMASK:
		npz->sig_blocked = new_set;
		break;
	    default:
		return -EMX_EINVAL;
	}
    }
    if (oset) {
	if (verify_illegal_write(npz, oset, sizeof(sigset_t)))
	    return -EMX_EINVAL;
	store32(npz->data32sel, oset, old_set);
    }
    return 0;
}

/*
** signal handler returned via syscall 0x10
*/
int signal_handler_returned(void)
{
    DWORD signal, sig_blocked;
    DWORD r;

    ESPORG += 4; /* old return address */

    signal = read32(npz->data32sel, ESPORG);
    if (npz->options.opt_printall)
	printf("return signal handler: %lX\n", signal);

    sig_blocked = read32(npz->data32sel, ESPORG + 4);

    /* unblock old signal mask, if not emx ack */
    if (!(npz->sigaction[SIGSA(signal)].sa_flags & SA_ACK))
	npz->sig_blocked = sig_blocked & ~SIGMASK(signal);

    r = read32(npz->data32sel, ESPORG + 8);
    if (r != 0x7F07b866) {
	puts("sigreturn: illegal frame");
	return 1;
    }

    cpy32_16(npz->data32sel, ESPORG + 20, &(npz->regs), sizeof(REG386));

    if (AX == 0x7f0E) { 	/* back from raise() */
	EAX = ECX = 0;		/* put return values */
	back_from_syscall();
	return 1;
    }
    /*
    ** if we had a hardware exception, we zero the user handler, to
    ** prevent a exception loop to handler (may be changed in future)
    */
    if (signal == SIGSEGV || signal == SIGILL ||
	signal == SIGFPE || signal == SIGTRAP) {
	(npz->sigaction[SIGSA(signal)]).sa_handler = 0;
	npz->sig_blocked &= ~SIGMASK(signal);
    }

    return 1;
}

/*
** called before return to user process
*/
int check_signals(void)
{
    int i;
    DWORD sigs;

    sigs = ~npz->sig_blocked & npz->sig_raised;
    if (!sigs)
	return 0;

    while (sigs) {
	for (i = 1; i <= MAX_SIGNALS; i++) {
	    if (sigs & 1) {
		if (do_signal(i))
		    break;	    /* restart loop, if handler called */
	    }
	    sigs >>= 1;
	}
	sigs = ~npz->sig_blocked & npz->sig_raised;
    }
    return 0;
}

#define SIGDFL_IGNORE	    0
#define SIGDFL_TERMINATE    1
#define SIGDFL_CORE	    2

static struct {
    char *text;
    char action;
} sigdfl[MAX_SIGNALS] = {
    { "SIGHUP",     SIGDFL_TERMINATE },
    { "SIGINT",     SIGDFL_TERMINATE },
    { "SIGQUIT",    SIGDFL_CORE },
    { "SIGILL",     SIGDFL_CORE },
    { "SIGTRAP",    SIGDFL_CORE },
    { "SIGABRT",    SIGDFL_CORE },
    { "SIGEMT",     SIGDFL_CORE },
    { "SIGFPE",     SIGDFL_CORE },
    { "SIGKILL",    SIGDFL_TERMINATE },
    { "SIGBUS",     SIGDFL_CORE },
    { "SIGSEGV",    SIGDFL_CORE },
    { "SIGSYS",     SIGDFL_CORE },
    { "SIGPIPE",    SIGDFL_TERMINATE },
    { "SIGALRM",    SIGDFL_TERMINATE },
    { "SIGTERM",    SIGDFL_TERMINATE },
    { "SIGUSR1",    SIGDFL_IGNORE },
    { "SIGUSR2",    SIGDFL_IGNORE },
    { "SIGCHLD",    SIGDFL_IGNORE },
    { "SIG19",      SIGDFL_IGNORE },
    { "SIG20",      SIGDFL_IGNORE },
    { "SIGBREAK",   SIGDFL_TERMINATE }
};

void setup_frame(DWORD address, DWORD oldmask, int signal)
{
    DWORD code;
    static unsigned char runcode[12] = {
	0x66, 0xb8, 0x07, 0x7f, 	/* mov $0x7f07, %ax */
	0x68, 0x0C, 0x00, 0x01, 0x00,	/* push ___syscall  */
	0xc3,				/* ret		    */
	0x90, 0x90
    } ;

    if (ESP == ESPORG)		/* build stack-frame,if not */
	ESP -= 12;

    /* save regs for handler return, put reg values on user stack */
    cpy16_32(npz->data32sel, ESP - sizeof(REG386),
	     &(npz->regs), sizeof(REG386));
    ESP -= sizeof(REG386);	/* sub register-frame */

    EIP = address;		/* sighandler address */

    ESP -= sizeof(runcode);	/* put return code */
    code = ESP;
    cpy16_32(npz->data32sel, ESP, runcode, sizeof(runcode));

    ESP -= sizeof(long);	/* put signal mask on user stack */
    store32(npz->data32sel, ESP, oldmask);
    ESP -= sizeof(long);	/* put signalno on user stack */
    store32(npz->data32sel, ESP, signal);
    ESP -= sizeof(long);	/* put return address: stack code */
    store32(npz->data32sel, ESP, code);

    ESPORG = ESP;
    ESP -= 12;			/* sub iret frame */
}

/*
** check signals settings , change eip to signal handler
*/
static int do_signal(int signal)
{
    DWORD address;
    DWORD mask;

#ifdef CONFIG_KDEB
    if (!opt_kdeb)
#endif
    /* if debugger: switch first */
    if ((npz->p_flags & PF_DEBUG) && signal != SIGKILL && signal != SIGCLD) {
	npz->wait_return = (signal << 8) | 127;
	npz->p_flags |= PF_WAIT_WAIT;

	npz->p_status = PS_STOP;
	npz->pptr->p_status = PS_RUN;	    /* run debugger */
	switch_context(npz->pptr);

	npz->p_status = PS_RUN; 	    /* continue child */
	npz->wait_return = 0;

	if (signal == SIGTRAP) {
	    mask = ~ SIGMASK(signal);
	    npz->sig_raised &= mask;
	    npz->sig_blocked &= mask;
	    return 0;
	}
    }

    address = npz->sigaction[SIGSA(signal)].sa_handler;
    if (npz->options.opt_printall)
	printf("do_signal %d handler %lX\n", signal, address);

    mask = SIGMASK(signal);
    npz->sig_raised &= ~mask;	    /* clear sig_raised */

    if (address == 1L)		    /* ignore sig */
	return 0;

    if (address == 0L) {
	/* emx ignores SIGCLD, SIGCHLD, SIGUSR */

	if (sigdfl[SIGSA(signal)].action == SIGDFL_IGNORE)
	    return 0;
	else if (sigdfl[SIGSA(signal)].action == SIGDFL_CORE) {
	    if (!npz->options.opt_nocore)
		write_core_file(npz);
	}
	printf("\nProcess terminated by %s\n", sigdfl[SIGSA(signal)].text);

	do_exit4c(signal);
	return 1;
    }

    /* ok, do user handler */
    if (npz->sigaction[SIGSA(signal)].sa_flags & SA_SYSV)
	npz->sigaction[SIGSA(signal)].sa_handler = 0L;
    else
	npz->sig_blocked |= mask;   /* set blocked */

    setup_frame(address, npz->sig_blocked, signal);

    /* BSD block others */
    npz->sig_blocked |= npz->sigaction[SIGSA(signal)].sa_mask;

    return 1;
}

long sys_signal(int signum, long handler)
{
    long old_handler;

    if (signum < 1 || signum >= MAX_SIGNALS || signum == SIGKILL)
	return -1;

    old_handler = npz->sigaction[SIGSA(signum)].sa_handler;

    if (handler == SIG_ACK) {
	npz->sig_blocked &= ~ SIGMASK(signum);
	return old_handler;
    }
    else if (handler != SIG_DFL && handler != SIG_IGN)
	if (verify_illegal(npz, handler, 4))
	    return -1;

    npz->sigaction[SIGSA(signum)].sa_handler = handler;
    npz->sigaction[SIGSA(signum)].sa_flags = 0;
    npz->sigaction[SIGSA(signum)].sa_mask = 0;

    if ((npz->uflags & 3) == 1) 	/* system V */
	npz->sigaction[SIGSA(signum)].sa_flags = SA_SYSV;
    else if ((npz->uflags & 3) == 2)	/* BSD */
	npz->sigaction[SIGSA(signum)].sa_flags = 0;
    else				/* old EMX */
	npz->sigaction[SIGSA(signum)].sa_flags = SA_ACK;

    return old_handler;
}

char *exceptext[] =
{
    "division by zero",
    "debug",
    "NMI",
    "breakpoint",
    "overflow",
    "bound check",
    "invalid opcode",
    "copro not availble",
    "double fault",
    "copro exception",
    "invalid TSS",
    "segment not present",
    "stack fault",
    "general protection",
    "page fault",
    "reserved",
    "copro error",
    "alignment error"
};


/*
** this function is called after hardware exceptions
*/

/* regs after exceptions */
REG386 regf;
EXCEPTION_10 reg_info;

static int emulate_os2call(void);

void myexcep13(void)
{				/* C exception handler */
    int signal;

#ifdef CONFIG_KDEB
    if (opt_kdeb && (WORD) regf.cs == code16sel && regf.faultno == 1) {
	memcpy(&(RSX_PROCESS.regs), &regf, sizeof(REG386));
	return KDEB_debug_handler();
    }
#endif

    if (npz->options.opt_printall)
	printf("Exception %d\n", (WORD) regf.faultno);

    /* test if we have a error in kernel, abort rsx */
    /* future versions will just terminate the running process */

    if ((WORD) regf.cs == code16sel || (WORD) regf.ds == data16sel) {
	printf("Kernel fault at %X %lX\n", (WORD) regf.cs, regf.eip);
	printf("EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX\n"
	       "EBP=%08lX ESP=%08lX  ESI=%08lX EDI=%08lX\n"
	       "CS=%04X DS=%04X ES=%04X SS=%04X\n",
	       regf.eax, regf.ebx, regf.ecx, regf.edx,
	       regf.ebp, regf.esp, regf.esi, regf.edi,
	 (WORD) regf.cs, (WORD) regf.ds, (WORD) regf.es, (WORD) regf.ss);
	if (dpmi10) {
	    printf("cr2 = %lX\n", reg_info.cr2);
	    printf("pte = %lX\n", reg_info.pte);
	}

        /*
        printf("User Registers:\n");
        npz->regs.faultno = regf.faultno;
        printf("PROTECTION FAULT  %d :\n", FAULTNO);
        print_regs_exception();
        */

        /* clean_up ints,exceptions,... */
        clean_up();

        /* leave protected mode */
        protected_to_real(0);
    }

    /* user fault, copy saved regs to process table */
    memcpy(&(npz->regs), &regf, sizeof(REG386));

    signal = exception2signal(FAULTNO);

    if (signal != SIGTRAP) {
	printf("process %d get hardware fault %d (%s) at %lX\n",
	       npz->pid, FAULTNO, exceptext[FAULTNO], EIP);
	if (npz->options.opt_printall) {
	    print_regs_exception();
	    if (dpmi10) {
		printf("cr2 = %lX\n", reg_info.cr2);
		printf("offset = %lX\n", reg_info.cr2 - npz->memaddress);
		printf("pte = %lX\n", reg_info.pte);
	    }
	}
    }
    else if (EIP <= 1) {
	if (emulate_os2call())
	    return;		/* no error on success */
    }

    send_signal(npz, signal);

    /* then, check_signal() is called (see excep32.asm) */
}

static void print_regs_exception(void)
{
    printf("selector=%lX  errbits: %X\n"
	   "cs:eip=%04X:%08lX eflags=%08lX\n"
	   "eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX\n"
	   "ebp=%08lX esp=%08lX  esi=%08lX edi=%08lX\n"
	   "cs=%04X ds=%04X es=%04X ss=%04X fs=%04X gs=%04X\n",
	   ERR & ~7L, (WORD) ERR & 7,
	   CS, EIP, EFLAGS,
	   EAX, EBX, ECX, EDX,
	   EBP, ESP, ESI, EDI,
	   CS, DS, ES, SS, FS, GS);
    printf("[ss:esp]   = %lX\n", read32(SS,ESP));
    printf("[ss:esp+4] = %lX\n", read32(SS,ESP+4));
}


/*
** DPMI 1.0 support, damand paging
**
** only called, if start32.c sets page_fault() function
*/

/*
** commit page, if legal address
** page in text, data
** return 1, if real page-fault
*/

static unsigned char pagein_buffer[4096];

int swapper(void)
{
    DWORD offset;
    NEWPROCESS *proc;
    WORD page = 1 + 8;		/* commit & read/write */
    int handle;

    if ((WORD) reg_info.cs == code16sel) {
	/* copy in kernel, find current process */
	for (proc = &FIRST_PROCESS; proc <= &LAST_PROCESS; proc++) {
	    if (!proc->code32sel)
		continue;
	    if ((reg_info.cr2 > proc->memaddress) &&
		(reg_info.cr2 < proc->memaddress + proc->membytes))
		break;
	}
	if (proc > &LAST_PROCESS) {
	    if (proc->options.opt_printall) {
		puts("swapper: cannot find process");
		printf("pagefault in %04X\n", (WORD) reg_info.cs);
		printf("cr2 %08lX\n", reg_info.cr2);
		printf("pte %X err %X\n", (WORD) reg_info.pte, (WORD) reg_info.error_code);
	    }
	    return 1;
	}
    } else
	proc = npz;

    offset = (reg_info.cr2 - proc->memaddress) & ~0xFFFL;

#if 0
    if (proc->options.opt_printall) {
	printf("process %d : pagefault in %04X\n", proc->pid, (WORD) reg_info.cs);
	printf("cr2 %08lX, pageoffset %08lX\n", reg_info.cr2, offset);
	printf("pte %X err %X\n", (WORD) reg_info.pte, (WORD) reg_info.error_code);
	printf("memaddress = %lX handle = %lX\n", proc->memaddress, proc->memhandle);
    }
#endif

    if (proc->pid == 0)
	return 1;

    handle = (int) proc->filehandle;

    /* text */
    if (offset >= proc->text_start && offset < proc->text_end) {
	if ((WORD) reg_info.cs != code16sel && (reg_info.error_code & 2))
	    return 1;
	if (ModifyPageAttributes(proc->memhandle, offset, 1, &page))
	    return 1;		/* better:readonly */
	if (handle == 0)	/* forked process */
	    return 0;
	rm_lseek(handle, proc->text_off + (offset - proc->text_start), SEEK_SET);
	if (rm_read(handle, pagein_buffer, 4096) != 4096)
	    return 1;
	cpy16_32(proc->data32sel, offset, pagein_buffer, 4096L);
	page = 1;
	if (ModifyPageAttributes(proc->memhandle, offset, 1, &page))
	    return 1;
	return 0;
    } else
     /* bss */ if (offset >= proc->bss_start && offset < proc->bss_end) {
	if (ModifyPageAttributes(proc->memhandle, offset, 1, &page))
	    return 1;
	if (handle == 0)	/* forked process */
	    return 0;
	bzero32(proc->data32sel, offset, 4096L);
	return 0;
    } else
     /* data */ if (offset >= proc->data_start && offset < proc->data_end) {
	if (ModifyPageAttributes(proc->memhandle, offset, 1, &page))
	    return 1;
	if (handle == 0)	/* forked process */
	    return 0;
	rm_lseek(handle, proc->data_off + (offset - proc->data_start), SEEK_SET);
	if (rm_read(handle, pagein_buffer, 4096) != 4096)
	    return 1;
	cpy16_32(proc->data32sel, offset, pagein_buffer, 4096L);
	return 0;
    } else
     /* heap */ if (offset >= proc->init_brk && offset < proc->brk_value) {
	if (ModifyPageAttributes(proc->memhandle, offset, 1, &page))
	    return 1;
	if (handle == 0)	/* forked process */
	    return 0;
	if (proc->p_flags & PF_DJGPP_FILE)
	    bzero32(proc->data32sel, offset, 4096L);
	return 0;
    } else
     /* stack */ if (offset >= proc->brk_value && offset <= proc->membytes) {
	if (ModifyPageAttributes(proc->memhandle, offset, 1, &page))
	    return 1;
	return 0;
    } else
	return 1;
}

static int emulate_os2call(void)
{
    DWORD called = read32(SS, ESPORG);
    DWORD p1 = read32(SS, ESPORG + 4);
    DWORD p2 = read32(SS, ESPORG + 8);
    DWORD p3 = read32(SS, ESPORG + 12);

    if (ESP == ESPORG)
	ESP -= 12;

    if (called == 0x7751d) {
	/*
	    rc = DosQueryCp(ulLength, pCodePageList, pDataLength);

	    ulLength (ULONG) - input
		The length, in bytes, of CodePageList.

	    pCodePageList (PULONG) - output
		The returned data list, in which the first doubleword
		is the current code page identifier of the calling process.

		If one or two code pages have been prepared for the system,
		then the second doubleword is the first prepared code page,
		and the third doubleword is the second prepared code page.

		If the data length is less than the number of bytes needed
		to return all of the prepared system code pages, then the
		returned list is truncated.

		The code page identifiers have the following values:
		    Value     Description
		    437       United States
		    850       Multilingual
		    852       Latin 2 (Czechoslovakia, Hungary, Poland, Yugoslavia)
		    857       Turkish
		    860       Portuguese
		    861       Iceland
		    863       Canadian French
		    865       Nordic
		    932       Japan
		    934       Korea
		    936       People's Republic of China
		    938       Taiwan
		    942       Japan  SAA
		    944       Korea  SAA
		    946       People's Republic of China  SAA
		    948       Taiwan  SAA

		Note:  Code pages 932, 934, 936, 938, 942, 944, 946, and 948
		    are supported only with the Asian version of the
		    operating system on Asian hardware.

	    pDataLength (PULONG) - output
		     The length, in bytes, of the returned data.

	*/
	EAX = 474;
	return 1;
    }
    else if (called == 0x76fb2) {
	/*
	 DosGetInfoBlocks returns the address
	 of the Thread Information Block (TIB)
	 of the current thread.
	 This function also returns the address
	 of the Process Information Block (PIB)
	 of the current process.

	rc = DosGetInfoBlocks(pptib, pppib);

	pptib (PTIB *) - output
	    Address of a doubleword in which the address of the Thread
	    Information Block (TIB) of the current thread is returned.

	    Refer to the Notes section for a description of the Thread
	    Information Block.

	pppib (PPIB *) - output
	    Address of a doubleword in which the address of the Process
	    Information Block (PIB) of the current process is returned.

	    Refer to the Notes section for a description of the Process
	    Information Block.
	*/

	/* fill some dummies */
	store32(DS, p1, npz->data_start);
	store32(DS, p2, npz->data_end);
	EAX = -1;
	return 1;
    }
    else if (called == 0x76fcc) {
	/*
	 rc = DosQueryModuleName(hmodModHandle, ulBufferLength, pNameBuffer);
	    HMODULE  hmodModHandle;
	    ULONG    ulBufferLength;
	    PCHAR    pNameBuffer;
	*/
	strcpy16_32(DS, p2, "\\emacs\\19.27\\bin\\emacs.exe");
	EAX = 6;
	return 1;
    } else
	return 0;
}
