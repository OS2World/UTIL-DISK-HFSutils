/*****************************************************************************
 * FILE: sysemx.c							     *
 *									     *
 * DESC:								     *
 *	- int 0x21 32 bit handler					     *
 *	- int 0x21 emx syscall handler					     *
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
#include <time.h>
#include <ctype.h>
#include "DPMI.H"
#include "PRINTF.H"
#include "RMLIB.H"
#include "TIMEDOS.H"
#include "PROCESS.H"
#include "SIGNALS.H"
#include "GNUAOUT.H"
#include "START32.H"
#include "CDOSX32.H"
#include "COPY32.H"
#include "EXCEP32.H"
#include "RSX.H"
#include "STATEMX.H"
#include "PTRACE.H"
#include "LOADPRG.H"
#include "TERMIO.H"
#include "DOSERRNO.H"
#include "version.h"
#include "dasdrm.h"

#ifdef __EMX__
#define RSX
#else
#define RSXWIN
#endif

#ifdef RSX
#define TIME_TIC time_tic
#define OPTIONS main_options
#else
#define TIME_TIC time_tic()
#define OPTIONS npz->options
#include "conwin.h"
#endif

#define CONVERT1_BX_HANDLE		\
	int h = get_dos_handle(BX);	\
	if (h < 0) {			\
	    EAX = EMX_EBADF;		\
	    return CARRY_ON;		\
	}				\
	else BX = h;

#define CONVERT2_BX_HANDLE		\
	int h = get_dos_handle(BX);	\
	if (h < 0) {			\
	    set_ecx_error(EMX_EBADF);	\
	    return CARRY_ON;		\
	}				\
	else BX = h;

static char emx_version[] = "emx_version=c9.0";
#define EMX_VERSION (*(DWORD *)&emx_version[12])

static int time_reached(unsigned long time)
{
    if (time <= TIME_TIC)
	return 1;
    else
	return 0;
}

static void set_ecx_error(int err)
{
    EAX = -1L;
    if (npz->p_flags & PF_DJGPP_FILE)
	err = errno_djgpp(err);
    ECX = (long) err;
}

static void set_no_error(void)
{
    EAX = ECX = 0L;
}

static void set_eax_return(DWORD reg_eax)
{
    EAX = reg_eax;
    ECX = 0L;
}

/*
** SYSEMX functions returns:
**
** CARRY_ON :	error, set carry-bit
** CARRY_OFF:	no error, clear carry-bit
** CARRY_NON:	do nothing
*/

/* oldbrk_eax sbrk(inc_edx) ; err:eax=-1 */
static int sys_emx00_sbrk(void)
{
    EAX = getmem(EDX, npz);
    return CARRY_NON;
}

/* eax=0 brk(newbrk_edx) ; err:eax=-1 */
static int sys_emx01_brk(void)
{
    if (EDX <= npz->brk_value)
	EAX = -1L;
    else if ((EAX = getmem(EDX - npz->brk_value, npz)) != -1L)
	EAX = 0L;
    return CARRY_NON;
}

/* maxbrk_eax ulimit(cmd_ecx,newlimit_edx) errno:ecx */
static int sys_emx02_ulimit(void)
{
    if (ECX == 3L) {
	FREEMEMINFO fm;
	GetFreeMemInfo(&fm);
	set_eax_return(fm.LargestFree + npz->brk_value);
    } else
	set_ecx_error(EMX_EINVAL);
    return CARRY_NON;
}

/* emx special: void vmstat() */
static int sys_emx03_vmstat(void)
{
    return CARRY_NON;
}

/* eax=filepermmask umask(edx) ; err:- */
static int sys_emx04_umask(void)
{
    EAX = EDX;
    return CARRY_NON;
}

/* eax getpid(void) err:- */
static int sys_emx05_getpid(void)
{
    EAX = (DWORD) npz->pid;
    return CARRY_NON;
}

/*
** eax = spawnve(proc_env *edx) err:carry errno:eax
*/
static int sys_emx06_spawn(void)
{
    int i, ret;
    PROCESS_ENV pe;
    static char filename[260];
    char **argp = NULL, **envp = NULL;
    char *envmem = NULL, *argmem = NULL;
    char *s;

    /* get process data, check mode */
    cpy32_16(DS, EDX, &pe, (DWORD) sizeof(PROCESS_ENV));
    pe.mode &= 0xff;
    if (pe.mode >= P_SESSION) {     /* OS/2 modes */
        if (!(OPTIONS.opt_os2 &&
		(pe.mode == P_SESSION || pe.mode == P_DETACH))) {
	    EAX = EMX_EINVAL;
	    return CARRY_ON;
	}
    }

    /* get memory for args and environment (env must para aligned) */
    if ((argp = (char **) malloc((pe.arg_count + 1) * sizeof(char *))) == NULL
	|| (envp = (char **) malloc((pe.env_count + 1) * sizeof(char *))) == NULL
	|| (argmem = (char *) malloc((pe.arg_size + 1) & ~1)) == NULL
	|| (envmem = (char *) malloc((pe.env_size + 20) & ~1)) == NULL
	) {
	printf("argmem,envmem to large a:%d e:%d\n", pe.arg_size, pe.env_size);
	if (argp != NULL)
	    free(argp);
	if (envp != NULL)
	    free(envp);
	if (argmem != NULL)
	    free(argmem);
	if (envmem != NULL)
	    free(envmem);
	EAX = EMX_E2BIG;
	return CARRY_ON;
    }

    /* get args from user ds, built arg-vector */
    cpy32_16(DS, pe.arg_off, argmem, (DWORD) pe.arg_size);
    s = argmem;
    for (i = 0; i < (int) pe.arg_count; i++) {
	s++;					/* skip flag bits */
	argp[i] = s;
	s += (strlen(argp[i]) + 1);
    }
    argp[i] = NULL;

    /* get env from user ds, built env-vector */
    s = (char *) (((int)envmem + 15) & ~15);	/* segment align */
    cpy32_16(DS, pe.env_off, s, (DWORD) pe.env_size);
    s[pe.env_size] = 0; 			/* for dos exec */
    s[pe.env_size+1] = 0;

    for (i = 0; i < (int) pe.env_count; i++) {
	envp[i] = s;
	s += (strlen(envp[i]) + 1);
    }
    envp[i] = NULL;

    /* get filename */
    strcpy32_16(DS, pe.fname_off, filename);
    convert_filename (filename);

    /* load a.out prg */
    ret = exec32(pe.mode, filename, pe.arg_count, argp, pe.env_count, envp);

    /* if error, try a real-mode prg */
    if (ret == EMX_ENOEXEC) {
#ifdef RSX
        ret = execRM(pe.mode, filename, argp, envp);
#else
        ret = realmode_prg(filename, argp, envp);
        if (pe.mode == P_OVERLAY && ret != -1)
            do_exit4c(0);
#endif
    }
    free(argmem);
    free(envmem);
    free(argp);
    free(envp);

    /* check error and return */
    if (ret) {
	EAX = (DWORD) ret;
	return CARRY_ON;
    } else
	return CARRY_OFF;
}

static int sys_emx07_sigreturn(void)
{
    signal_handler_returned();
    return CARRY_NON;
}

/* eax ptrace(ebx,edi,edx,ecx) err:ecx!=0 eax=-1 */
static int sys_emx08_ptrace(void)
{
    int error;
    DWORD data;

    if ((error = do_ptrace(BX, DI, EDX, ECX, &data)) != 0)
	set_ecx_error(error);
    else
	set_eax_return(data);
    return CARRY_NON;
}

/* eax=child_id wait(status_edx) ; errno:ecx */
static int sys_emx09_wait(void)
{
    int ret;
    unsigned status;

    if ((ret = sys_wait(&status)) != -1) {
	EDX = (DWORD) status;
	set_eax_return((DWORD) (unsigned) ret);
    } else
	set_ecx_error(EMX_ECHILD);
    return CARRY_NON;
}

#define RUN_VCPI	0x0001L
#define RUN_XMS 	0x0002L
#define RUN_VDISK	0x0004L
#define RUN_DESQVIEW	0x0008L
#define RUN_287 	0x0010L
#define RUN_387 	0x0020L
#define RUN_486 	0x0040L
#define RUN_DPMI09	0x0080L
#define RUN_DPMI10	0x0100L
#define RUN_OS2 	0x0200L
#define RUN_T_OPT	0x0400L
#define RUN_AC_OPT	0x0800L
#define RUN_RSX 	0x1000L

/* get emx-version ; err:- */
static int sys_emx0a_version(void)
{
    DWORD run_info;

    run_info = RUN_RSX | RUN_AC_OPT;	  /* RSX | data executable bit */
    if (copro)
	run_info |= RUN_387;	/* with emu */
    if (dpmi10)
	run_info |= RUN_DPMI10;
    else
	run_info |= RUN_DPMI09;

    /* enable gcc -pipe for emx 0.8x */
    if (npz->options.opt_os2)
	run_info |= RUN_OS2;

    EAX = EMX_VERSION;
    EBX = run_info;
#ifdef RSX
    ECX = REV_RSX;
#else
    ECX = REV_RSXWIN;
#endif
    EDX = 0L;
    return CARRY_NON;
}

/* eax_pages memavail(void) ; err:- */
static int sys_emx0b_memavail(void)
{
    FREEMEMINFO fm;

    GetFreeMemInfo(&fm);
    EAX = fm.MaxUnlockedPages;
    return CARRY_NON;
}

/* (*prevh) signal(signo_ecx,address_edx) err:eax=-1 */
static int sys_emx0c_signal(void)
{
    EAX = sys_signal ((int)ECX, EDX);
    return CARRY_NON;
}

static int sys_kill(NEWPROCESS *p, long sig)
{
    if (sig < 0) {			/* send SIG to P and all childs */
	NEWPROCESS *cld;
        if (send_signal(p, -sig))       /* send SIG to P */
	    return -EMX_EINVAL;
	for (cld = &FIRST_PROCESS; cld <= &LAST_PROCESS; cld++)
	    if (cld->pptr->pid == p->pid)	/* and recursive to childs */
		sys_kill(cld, sig);
        return 0;
    }

    if (send_signal(p, sig))
        return -EMX_EINVAL;

    /* switch, if child */
    if (p->pptr->pid == npz->pid) {
        p->p_status = PS_RUN;
        switch_context(p);
    }
    return 0;
}

/* eax=0 kill(id_edx,signo_ecx) errno:ecx eax=-1 */
static int sys_emx0d_kill(void)
{
    NEWPROCESS *p;
    int ret;

    if (!(p = find_process((unsigned) EDX)))
	set_ecx_error(EMX_ESRCH);
    else if ((int)(long)ECX == 0)       /* check whether PID is valid */
	set_no_error();
    else if ((ret = sys_kill(p, ECX)) < 0)
	set_ecx_error(-ret);
    else
        set_no_error();
    return CARRY_NON;
}

/* eax raise(ecx) errno:ecx eax=-1 */
static int sys_emx0e_raise(void)
{
    if (send_signal(npz, CX))
	set_ecx_error(EMX_EINVAL);
    return CARRY_NON;
}

/* oldflags uflags(mask=ecx,new=edx) ;; bits 0-1 emx,SysV,BSD signals */
static int sys_emx0f_uflags(void)
{
    EAX = npz->uflags;
    npz->uflags &= ~ECX;
    npz->uflags |= EDX;
    return CARRY_NON;
}

/* void unwind(void) err:no */
static int sys_emx10_unwind(void)
{
    return CARRY_NON;
}

/* core(handle_ebx) err:carry errno */
static int sys_emx11_core(void)
{
    CONVERT1_BX_HANDLE ;

    write_core(BX, npz);
    EAX = 0;
    return CARRY_OFF;
}

/* portaccess(ecx,edx) ecx=first edx=last, err:cy errno:eax */
static int sys_emx12_portaccess(void)
{
    /* dpmi-server must allow this */
    ECX = 0L;			/* first port */
    EDX = 0x3ffL;		/* last port */
    EAX = 0L;
    return CARRY_OFF;
}

/* eax memaccess(Start=ebx,End=ecx,Flags=edx) err:carry errno:eax */
/* under DPMI it's better to used a different segment */
/* memaccess destroy protection -> limit 0xffffffff */
/* must use wrap-around to access memory */

static int sys_emx13_memaccess(void)
{
#ifdef RSXWIN
    send_signal(npz, SIGSEGV);
    return CARRY_ON;
#else
    if (npz->options.opt_memaccess
	&& EBX <= 0xFFFFFL && ECX <= 0xFFFFFL) {

	if (!dpmi10) {
	    /*
	    UINT sel;
	    DWORD screen_base;

	    if (SegToSel((UINT)(EBX >> 4), &sel))
		screen_base = EBX;
	    else
		GetBaseAddress(sel, &screen_base);
	    EAX = screen_base - npz->memaddress;
	    */
	    EAX = EBX - npz->memaddress;
	}
	else
	    EAX = EBX + DPMI_PRG_DATA;
	return CARRY_OFF;
    } else {
	EAX = EMX_EINVAL;
	return CARRY_ON;
    }
#endif
}


int sysemx_ioctl(int dos_handle, ARGUSER request, ARGUSER arg)
{
    int attr, ret;

    if ((attr = rm_ioctl_getattr(dos_handle)) == -1)
	return -EMX_EBADF;

    if (request == FIONREAD) {
	ret = rm_ioctl_select_in(dos_handle);
	store32(npz->data32sel, arg, (long) ret);
	return 0;
    }
    else if (request == FGETHTYPE) {
	long temp;
	if (!(attr & 128))
	    temp = HT_FILE;
	else if (attr & 3)
	    temp = HT_DEV_CON;
	else if (attr & 4)
	    temp = HT_DEV_NUL;
	else if (attr & 8)
	    temp = HT_DEV_CLK;
	else
	    temp = HT_DEV_OTHER;
	store32(DS, arg, temp);
	return 0;
    }
    else
	return -EMX_EINVAL;
}

/*
** eax = ioctl2(ebx,ecx,edx) errno:ecx eax=-1
*/
static int sys_emx14_ioctl(void)
{
    int ret = sys_ioctl((int)EBX, ECX, EDX);

    if (ret < 0)
	set_ecx_error(-ret);
    else
	set_eax_return(ret);
    return CARRY_NON;
}

/* eax=sec alarm(sec_edx) err:no */
static int sys_emx15_alarm(void)
{
    if (TIME_TIC < npz->time_alarm)     /* there seconds left */
        EAX = (npz->time_alarm - TIME_TIC) * 182 / 10;
    else
	EAX = 0;

    if (EDX == 0)		/* clear alarm */
	npz->time_alarm = 0;
    else			/* set alarm */
        npz->time_alarm = TIME_TIC + EDX * 182 / 10;
    return CARRY_NON;
}

/*
** return ramaining clock tics
*/
static DWORD sleep_until(DWORD timel)
{
    for (;;) {
	if (time_reached(timel))
	    return 0;
        if (npz->time_alarm != 0 && time_reached(npz->time_alarm)) {
            npz->time_alarm = 0;
            send_signal(npz, SIGALRM);
        }
	if (npz->sig_raised & ~npz->sig_blocked)
            return (timel - TIME_TIC - 1);
	schedule();
    }
}

/* eax=0 sleep(edx) err:no */
static int sys_emx17_sleep(void)
{
    EAX = sleep_until(TIME_TIC + EDX * 182 / 10) * 10 / 182;
    return CARRY_NON;
}

/*
** chsize(handle_ebx, lenght_edx) err:carry eax=errno
*/
static int sys_emx18_chsize(void)
{
    CONVERT1_BX_HANDLE;

    if (rm_lseek(BX, EDX, SEEK_SET) == -1L || rm_write(BX, iobuf, 0) == -1) {
	EAX = (DWORD) doserror_to_errno(_doserrno);
	return CARRY_ON;
    }
    EAX = 0;
    return CARRY_OFF;
}

/* eax fcntl(handle ebx,req ecx,arg edx) errno:ecx */
static int sys_emx19_fcntl(void)
{
    int fd = (int) (long) EBX;
    struct file *filp;

    if (fd >= N_FILES)
	return -EMX_EBADF;
    if (!(filp = npz->filp[fd]))
	return -EMX_EBADF;

    switch ((int)(long)ECX) {
	case F_GETFL:
	    set_eax_return(filp->f_flags);
	    break;
	case F_SETFL:
	    filp->f_flags &= ~(FCNTL_NDELAY | FCNTL_APPEND);
	    filp->f_flags |= EDX & (FCNTL_NDELAY | FCNTL_APPEND);
	    set_no_error();
	    break;
	case F_GETFD:
	    set_eax_return(FD_ISSET(fd, &npz->close_on_exec));
	    break;
	case F_SETFD:
	    if (EDX & 1)
		FD_SET(fd, &npz->close_on_exec);
	    else
		FD_CLR(fd, &npz->close_on_exec);
	    set_no_error();
	    break;
	default:
	    set_ecx_error(EMX_EINVAL);
	    break;
    }
    return CARRY_NON;
}

/* eax pipe(edx,ecx) errno:ecx*/
static int sys_emx1a_pipe(void)
{
    int ret = sys_pipe(ECX, EDX);

    if (ret)
	set_ecx_error(-ret);
    else
	set_no_error();
    return CARRY_NON;
}

/* eax fsync(ebx) errno:ecx */
static int sys_emx1b_fsync(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

/* eax fork(void) errno:ecx */
static int sys_emx1c_fork(void)
{
    int ret;

    if ((ret = sys_fork()) < 0)
	set_ecx_error(-ret);
    else
	set_eax_return(ret);
    return CARRY_NON;
}

/* void scrsize(EDX) */
static int sys_emx1d_scrsize(void)
{
#ifdef RSX
    store32(DS, EDX + 0L, read32(bios_selector, 0x4AL) & 0xFFFFL);
    store32(DS, EDX + 4L, (read32(bios_selector, 0x84L) & 0xFFL) + 1L);
#else
    store32(DS, EDX + 0L, 80);
    store32(DS, EDX + 4L, 25);
#endif
    return CARRY_NON;
}

/* void select(edx) errno:ecx */
static int sys_emx1e_select(void)
{
    int ret = sys_select(EDX);

    if (ret < 0)
	set_ecx_error(-ret);
    else
	set_eax_return(ret);
    return CARRY_NON;
}

/* eax syserrno(void) */
static int sys_emx1f_syserrno(void)
{
    EAX = doserror_to_errno(_doserrno);
    return CARRY_NON;
}

/*
** eax stat(name_edx,struc_edi) errno:ecx
*/
static int sys_emx20_stat(void)
{
    struct stat st;

    strcpy32_16(DS, EDX, iobuf);
    convert_filename(iobuf);
    if (!sys_stat(iobuf, &st)) {
	cpy16_32(DS, EDI, &st, sizeof(st));
	set_no_error();
    } else
	set_ecx_error(doserror_to_errno(_doserrno));
    return CARRY_NON;
}

/*
** eax fstat(ebx, edi) errno:ecx
*/
static int sys_emx21_fstat(void)
{
    struct stat st;

    CONVERT2_BX_HANDLE;

    if (!sys_fstat(BX, &st)) {
	cpy16_32(DS, EDI, &st, sizeof(st));
	set_no_error();
    } else
	set_ecx_error(doserror_to_errno(_doserrno));
    return CARRY_NON;
}

/* filesys(edx,edi,ecx) errno:ecx */
static int sys_emx23_filesys(void)
{
    char drive[4];
    char *res;
    int ret;

    *(DWORD *) drive = read32(DS, EDX);
    if (drive[0] >= 'a')
	drive[0] -= 0x20;
    if (drive[0] < 'A' || drive[0] > 'Z' || drive[1] != ':' || drive[2] != 0) {
	set_ecx_error(EMX_EINVAL);
	return -1;
    }
    drive[0] -= ('A' - 1);
    ret = rm_ioctl_remotedrive(drive[0]);
    if (ret == 1)
	res = "LAN";
    else
	res = "FAT";
    strcpy16_32(DS, EDI, res);
    set_no_error();
    return CARRY_NON;
}

/* eax utimes(name_edx,struct tval esi) errno:ecx */
/* set access and modif time of file  */
static int sys_emx24_utimes(void)
{
    struct file_time ft;
    DWORD utimes[2];
    int handle;

    strcpy32_16(DS, EDX, iobuf);
    convert_filename(iobuf);
    cpy32_16(DS, ESI, utimes, sizeof(utimes));

    if ((handle = rm_open(iobuf, RM_O_WRONLY)) == -1) {
	set_ecx_error(doserror_to_errno(_doserrno));
	return CARRY_NON;
    }
    gmt2filetime(utimes[0], &ft);
    if (rm_setftime(handle, ft.ft_date, ft.ft_time) == -1)
	set_ecx_error(doserror_to_errno(_doserrno));
    else
	set_eax_return(0);
    rm_close(handle);
    return CARRY_NON;
}

/* eax ftruncate(ebx,edx) errno:ecx */
static int sys_emx25_ftruncate(void)
{
    long temp;

    CONVERT2_BX_HANDLE;

    if ((temp = rm_lseek(BX, 0, SEEK_END)) == -1) {
	set_ecx_error(doserror_to_errno(_doserrno));
	return CARRY_NON;
    } else if (temp > (long) EDX) {
	if (rm_lseek(BX, EDX, SEEK_SET) == -1 || rm_write(BX, iobuf, 0) == -1) {
	    set_ecx_error(doserror_to_errno(_doserrno));
	    return CARRY_NON;
	}
    }
    set_no_error();
    return CARRY_NON;
}

/* eax clock(void) err:no */
static int sys_emx26_clock(void)
{
    /* clk_tck = 100 ; timer 18.2 pre sec */
    EAX = (TIME_TIC - npz->time_tic) * 500 / 91;
    EDX = 0;
    return CARRY_NON;
}

/* void ftime(edx) err:no */
static int sys_emx27_ftime(void)
{
    struct emx_timeb {
	long time, millitm, timezone, dstflag;
    } emx_timeb;
    struct dos_date dd;
    struct dos_time dt;
    unsigned long gmt;

    rm_getdate(&dd);
    rm_gettime(&dt);
    gmt = dos2gmt(&dd, &dt);

    emx_timeb.time = gmt;
    emx_timeb.millitm = (DWORD) dt.dtime_hsec * 10;
    emx_timeb.timezone = 0;
    emx_timeb.dstflag = 0;

    cpy16_32(DS, EDX, &emx_timeb, sizeof(emx_timeb));
    return CARRY_NON;
}

/* eax umask(edx) err:no */
static int sys_emx28_umask(void)
{
    EAX = EDX;
    return CARRY_NON;
}

/* eax getppid(void) err:no */
static int sys_emx29_getppid(void)
{
    EAX = (DWORD) npz->pptr->pid;
    return CARRY_NON;
}

static int sys_emx2a_nlsmemupr(void)
{
    int size = CX;
    struct country_info cinfo;

    if (size < IOBUF_SIZE && !verify_illegal (npz, EDX, size)) {
	int i;
        if (rm_get_country_info(&cinfo))
            set_ecx_error(emx_errno);
	cpy32_16(npz->data32sel, EDX, iobuf, size);
	for (i = 0; i < size; i++) {
	    if (iobuf[i] >= 'a' && iobuf[i] <= 'z')
		iobuf[i] -= 'a' - 'A';
	    else if ((unsigned char) iobuf[i] >= 128)
		iobuf[i] = rm_country_casemap(iobuf[i], cinfo.cd_casemap_func);
	}
	cpy16_32(npz->data32sel, EDX, iobuf, size);
    }
    else
	set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

/*
**  eax open(edx, ecx) errno:ecx
**
**  pathname <= sizeof(iobuf) !
**  creat: CH = creat file attr
**  open:  CL = access & sharing mode
*/

static int sys_emx2b_open(void)
{
    int dosfile, handle;
    unsigned mode;

    strcpy32_16(DS, EDX, iobuf);
    convert_filename(iobuf);

    /* 1 = O_CREAT, 2 = O_EXCL, 4 = O_TRUNC */
    /* 8 = O_INHERIT, 16 = SYNC */
    mode = (unsigned) (ECX >> 16);

    if (strlen(iobuf) == 2 && iobuf[1] == ':'
	&& toupper(iobuf[0]) >= 'A' && toupper(iobuf[0]) <= 'Z') {
	int drive = toupper(iobuf[0])-'A'+1;
	struct dasd_info dasd_i;
	
	/* Direct open */
	if (get_dasd_info(drive, &dasd_i) != 0)
	  return -EMX_EACCES;

	handle = get_empty_proc_filp();
	npz->filp[handle]->f_mode = FMODE_READ | FMODE_WRITE;
	npz->filp[handle]->f_doshandle = -1;
	npz->filp[handle]->f_op = & dasd_fop;
	npz->filp[handle]->f_offset = 0;
	npz->filp[handle]->f_info.dasd_i = malloc(sizeof(struct dasd_info));
	if (npz->filp[handle]->f_info.dasd_i == NULL) {
	    npz->filp[handle]->f_count = 0;
	    return -EMX_ENOMEM;
	}
	*npz->filp[handle]->f_info.dasd_i = dasd_i;
	set_eax_return(handle);
	
	return CARRY_NON;
    }

    dosfile = rm_open(iobuf, CX & 0xFF);

    if (dosfile != -1) {            /* file exist */
        if ((mode & 0x03) == 0x03) { /* O_CREATE | O_EXCL */
            rm_close(dosfile);
            set_ecx_error(EMX_EEXIST);
            return CARRY_NON;
        }
        if (mode & 0x04) {          /* O_TRUNC */
            if (rm_lseek(dosfile, 0, SEEK_SET) != -1L)
                rm_write(dosfile, iobuf, 0);
	}
    } else if (mode & 0x01) {       /* try create, if create attr */
        WORD attr = CX >> 8;
        attr &= ~1;
        dosfile = rm_creat(iobuf, attr);
    }

    if (dosfile == -1) {
        set_ecx_error(doserror_to_errno(_doserrno));
        return CARRY_NON;
    }

    handle = get_empty_proc_filp();
    npz->filp[handle]->f_mode = FMODE_READ | FMODE_WRITE;
    npz->filp[handle]->f_doshandle = dosfile;
    npz->filp[handle]->f_op = & msdos_fop;
    if (mode & 16)  /* inherit file handle */
        FD_CLR(handle, &npz->close_on_exec);
    set_eax_return(handle);

    return CARRY_NON;
}

/* eax newthread(edx) errno:ecx */
static int sys_emx2c_newthread(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

/* eax endthread(void) errno:ecx */
static int sys_emx2d_endthread(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

/* waitpid(pid_edx, options_ecx) ret: eax,ecx edx=status*/
static int sys_emx2e_waitpid(void)
{
    if (!npz->options.opt_schedule)
	set_ecx_error(EMX_EMSDOS);
    else {
	int ret;
	unsigned status;
	if ((ret = sys_waitpid((int)EDX, &status)) >= 0) {
	    EDX = (DWORD) status;
	    set_eax_return((DWORD) (unsigned) ret);
	} else
	    set_ecx_error(EMX_ECHILD);
    }
    return CARRY_NON;
}

#ifdef RSX
static int sys_emx2f_readkbd()
{
    static unsigned char next_key = 0;
    int key;
    unsigned char ascii, scan;

    if (next_key) {
	EAX = (DWORD) next_key;
	next_key = 0;
	return CARRY_NON;
    }
    if (DX & 2) {		/* wait */
	while (rm_bios_read_keybrd(kready))
	    rm_bios_read_keybrd(kread);
	key = rm_bios_read_keybrd(kread);
    } else if ((key = rm_bios_read_keybrd(kready)) != 0)
	key = rm_bios_read_keybrd(kread);
    else {
	EAX = -1;
	return CARRY_NON;
    }

    ascii = (BYTE) (key & 0xFF);
    scan = (BYTE) (key >> 8);

    if (ascii == 0xE0)
	ascii = 0;

    if (ascii == 0)
	next_key = scan;

    if (DX & 1) 		/* echo */
	if (ascii >= 32)
	    /* change to: sys_write() */
	    rm_write((int) npz->filp[1]->f_doshandle, &ascii, 1);

    if ((DX & 4) && ascii == 3) /* signal */
	send_signal(npz, SIGINT);

    EAX = (DWORD) (ascii);

    return CARRY_NON;
}
#else
static int sys_emx2f_readkbd()
{
    static unsigned char next_key = 0;
    int echo, wait, sign;
    int c;

    if (next_key) {
        EAX = (DWORD) next_key;
        next_key = 0;
        return CARRY_NON;
    }
    echo = EDX & 1;
    wait = (EDX & 2) >> 1;
    sign = (EDX & 4) >> 2;

    c = win_read_kbd(echo, wait, sign);
    if ((c & 0xff) == 0x00) {
        next_key = c>>8;
        c = 0;
    }
    EAX = (DWORD) (c);

    if (c == 3 && sign)
        send_signal(npz, SIGINT);

    return CARRY_NON;
}
#endif

/* sleep2 ; err:-*/
static int sys_emx30_sleep2(void)
{
    sleep_until(TIME_TIC + EDX / 55);
    EAX = 0;
    return CARRY_NON;
}

/* void unwind2(); not DOS */
static int sys_emx31_unwind2(void)
{
    return CARRY_NON;
}

/* void pause(); err: eax=-1, return until signal handler */
static int sys_emx32_pause(void)
{
    for (;;) {
	if (npz->sig_raised & ~npz->sig_blocked) {
	    EAX = 0;
	    break;
	}
	if (!schedule()) {     /* no other processes */
	    EAX = -1L;
	    break;
	}
    }
    return CARRY_NON;
}

/* int execname(edx=buf, ecx=size); err=-1 */
#ifdef RSX
static int sys_emx33_execname(void)
{
    DWORD pos = read32(npz->data32sel, npz->stack_top - 8L);
    DWORD len = read32(npz->data32sel, npz->stack_top - 12L);

    if (ECX < len || verify_illegal_write(npz, EDX, len)
	    || verify_illegal(npz, pos, len)) {
	EAX = -1;
	return CARRY_NON;
    }
    cpy32_32(npz->data32sel, pos, npz->data32sel, EDX,	npz->stack_top - pos);
    EAX = 0;
    return CARRY_NON;
}
#else
static int sys_emx33_execname(void)
{
    int len = strlen(npz->execname) + 1;

    if (verify_illegal_write(npz, EDX, ECX) && ECX < len) {
        EAX = -1;
        return CARRY_NON;
    }
    cpy16_32(npz->data32sel, EDX, npz->execname, len);
    EAX = 0;
    return CARRY_NON;
}
#endif

/* int initthread() ; not DOS */
static int sys_emx34_initthread(void)
{
    EAX = -1;
    return CARRY_NON;
}

/* int sigaction(no ECX, in EDX, out EBX) */
static int sys_emx35_sigaction(void)
{
    int err = sys_sigaction((int)ECX, EDX, EBX);
    if (err < 0)
	set_ecx_error(-err);
    else
	set_eax_return(err);
    return CARRY_NON;
}

/* int sigpending(in EDX) */
static int sys_emx36_sigpending(void)
{
    int err = sys_sigpending(EDX);
    if (err < 0)
	set_ecx_error(-err);
    else
	set_eax_return(err);
    return CARRY_NON;
}

/* int sigprocmask(how ECX, in EDX, out EBX) */
static int sys_emx37_sigprocmask(void)
{
    int err = sys_sigprocmask((int)ECX, EDX, EBX);
    if (err < 0)
	set_ecx_error(-err);
    else
	set_eax_return(err);
    return CARRY_NON;
}

/* int sigsuspend(sigset_t EDX) */
static int sys_emx38_sigsuspend(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

/* os/2 only */
static int sys_emx39_imphandle(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

static int sys_emx57_ttyname(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

static int sys_emx58_settime(void)
{
    DWORD timeval[2];
    struct dos_date dd;
    struct dos_time dt;
    unsigned long gmt;
    struct tm stm;

    cpy32_16(DS, EDX, timeval, sizeof(timeval));
    gmt = timeval[0];
    gmt2tm(&gmt, &stm);

    dd.ddate_year = stm.tm_year + 1900;
    dd.ddate_month = stm.tm_mon;
    dd.ddate_day = stm.tm_mday;
    if (rm_setdate(&dd)) {
	set_ecx_error(emx_errno);
	return CARRY_NON;
    }

    dt.dtime_hour = stm.tm_hour;
    dt.dtime_minutes = stm.tm_min;
    dt.dtime_seconds = stm.tm_sec;
    dt.dtime_hsec = 0;
    if (rm_settime(&dt))
	set_ecx_error(emx_errno);

    return CARRY_NON;
}

static int sys_emx59_profil(void)
{
    set_ecx_error(EMX_EMSDOS);
    return CARRY_NON;
}

#define EMX_FNCT_MAX 0x59
typedef int (*EMXFNCT) (void);

static EMXFNCT emx_fnct[EMX_FNCT_MAX + 1] =
{
    sys_emx00_sbrk,
    sys_emx01_brk,
    sys_emx02_ulimit,
    sys_emx03_vmstat,
    sys_emx04_umask,
    sys_emx05_getpid,
    sys_emx06_spawn,
    sys_emx07_sigreturn,
    sys_emx08_ptrace,
    sys_emx09_wait,
    sys_emx0a_version,
    sys_emx0b_memavail,
    sys_emx0c_signal,
    sys_emx0d_kill,
    sys_emx0e_raise,
    sys_emx0f_uflags,
    sys_emx10_unwind,
    sys_emx11_core,
    sys_emx12_portaccess,
    sys_emx13_memaccess,
    sys_emx14_ioctl,
    sys_emx15_alarm,
    (EMXFNCT) 0,	    /* 0x16 emx internal */
    sys_emx17_sleep,
    sys_emx18_chsize,
    sys_emx19_fcntl,
    sys_emx1a_pipe,
    sys_emx1b_fsync,
    sys_emx1c_fork,
    sys_emx1d_scrsize,
    sys_emx1e_select,
    sys_emx1f_syserrno,
    sys_emx20_stat,
    sys_emx21_fstat,
    (EMXFNCT) 0,	    /* unused */
    sys_emx23_filesys,
    sys_emx24_utimes,
    sys_emx25_ftruncate,
    sys_emx26_clock,
    sys_emx27_ftime,
    sys_emx28_umask,
    sys_emx29_getppid,
    sys_emx2a_nlsmemupr,
    sys_emx2b_open,
    sys_emx2c_newthread,
    sys_emx2d_endthread,
    sys_emx2e_waitpid,
    sys_emx2f_readkbd,
    sys_emx30_sleep2,
    sys_emx31_unwind2,
    sys_emx32_pause,
    sys_emx33_execname,
    sys_emx34_initthread,
    sys_emx35_sigaction,
    sys_emx36_sigpending,
    sys_emx37_sigprocmask,
    sys_emx38_sigsuspend,
    sys_emx39_imphandle,
    (EMXFNCT) 0,	    /* 0x3a __fpuemu() */
    (EMXFNCT) 0,	    /* 0x3b __getsockhandle() */
    (EMXFNCT) 0,	    /* 0x3c __socket() */
    (EMXFNCT) 0,	    /* 0x3d __bind() */
    (EMXFNCT) 0,	    /* 0x3e __listen() */
    (EMXFNCT) 0,	    /* 0x3f __recv() */
    (EMXFNCT) 0,	    /* 0x40 __send() */
    (EMXFNCT) 0,	    /* 0x41 __accept() */
    (EMXFNCT) 0,	    /* 0x42 __connect() */
    (EMXFNCT) 0,	    /* 0x43 __getsockopt() */
    (EMXFNCT) 0,	    /* 0x45 __setsockopt() */
    (EMXFNCT) 0,	    /* 0x46 __getsockname() */
    (EMXFNCT) 0,	    /* 0x47 __getpeername() */
    (EMXFNCT) 0,	    /* 0x48 __gethostbyname */
    (EMXFNCT) 0,	    /* 0x49 __gethostbyaddr */
    (EMXFNCT) 0,	    /* 0x4a __getservbyport */
    (EMXFNCT) 0,	    /* 0x4b __getprotobyname() */
    (EMXFNCT) 0,	    /* 0x4c __getprotobynumber() */
    (EMXFNCT) 0,	    /* 0x4d __getnetbyname() */
    (EMXFNCT) 0,	    /* 0x4e __getnetbyaddr() */
    (EMXFNCT) 0,	    /* 0x4f __gethostname */
    (EMXFNCT) 0,	    /* 0x50 __gethostid */
    (EMXFNCT) 0,	    /* 0x51 __shutdown() */
    (EMXFNCT) 0,	    /* 0x52 __recvform() */
    (EMXFNCT) 0,	    /* 0x53 __sendto() */
    (EMXFNCT) 0,	    /* 0x54 __impsockhandle() */
    (EMXFNCT) 0,	    /* 0x55 __recvmsg() */
    (EMXFNCT) 0,	    /* 0x56 __sendmsg() */
    sys_emx57_ttyname,
    sys_emx58_settime,
    sys_emx59_profil
};

static int i_21_7f(void)
{
    WORD i = AX & 0xFF;

    /* emx fnct = al */
    if (i <= EMX_FNCT_MAX && emx_fnct[i])
	return (emx_fnct[i]) ();
    else {
	set_ecx_error(EMX_EMSDOS);
	if (npz->options.opt_printall)
	    printf("Warning: unknown emx syscall: %04X\n", i);
	return CARRY_ON;
    }
}

int i_21_ff(void);
int i_21_fe(void);

void int21(void)
{
    WORD ret;

    if (OPTIONS.opt_print_syscalls)
	printf("%2d 21h:a=%08X b=%08lX c=%08lX d=%08lX D=%08lX S=%08lX\n"
		, npz->pid, AX, EBX, ECX, EDX, EDI, ESI);

    /* emx functions */
    if ((AX & 0xff00) == 0x7f00)
	ret = i_21_7f();

#ifdef RSX
    /* go32 functions */
    else if ((AX & 0xff00) == 0xff00)
	ret = i_21_ff();

    /* go32 debug */
    else if ((AX & 0xff00) == 0xfe00)
	ret = i_21_fe();
#endif

    /* DOS functions */
    else {
	ret = int21normal();
	/* if DOS indicates an error (carry-flag set), convert error code */
	if (ret == CARRY_NON && (FLAGS & 1)) {
	    EAX = (DWORD) doserror_to_errno(AX);
	    ret = CARRY_ON;
	}
    }

    if (ret == CARRY_ON) {
	EFLAGS |= 1;
	if (npz->p_flags & PF_DJGPP_FILE)
	    EAX = (DWORD) errno_djgpp(AX);
    } else if (ret == CARRY_OFF)
	EFLAGS &= ~1L;

    if (OPTIONS.opt_print_syscalls)
	printf("%2d    :a=%08lX b=%08lX c=%08lX d=%08lX D=%08lX S=%08lX F:%04X\n"
		,npz->pid , EAX, EBX, ECX, EDX, EDI, ESI, FLAGS);

    if (npz->time_alarm != 0 && time_reached(npz->time_alarm)) {
	npz->time_alarm = 0;
	send_signal(npz, SIGALRM);
    }
    /* stack check */
    if (npz->stack_down + 0x1000L >= ESP) {
	puts("stack too small!!");
	printf("ESP %lX  stack : min %lX max %lX\n",
	       ESP, npz->stack_down, npz->stack_top);
	send_signal(npz, SIGKILL);
    }
}
