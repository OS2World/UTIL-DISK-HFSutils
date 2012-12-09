/*****************************************************************************
 * FILE: start32							     *
 *									     *
 * DESC:								     *
 *	- DPMI-switches for DPMI 0.9					     *
 *	- init some protected mode interrupts				     *
 *	- init exception handlers					     *
 *	- clean up for exit						     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 * Thanks to: (bug-fixes, patches, testing, ..)                              *
 *      Friedbert Widmann <widi@informatik.uni-koblenz.de>                   *
 *                                                                           *
 *****************************************************************************/

#include "DPMI.H"
#include "DPMI10.H"
#include "PRINTF.H"
#include "PROCESS.H"
#include "RSX.H"
#include "ADOSX32.H"
#include "CDOSX32.H"
#include "EXCEP32.H"
#include "START32.H"
#include "COPY32.H"
#include "SYSDEP.H"

/* global extender segments,selectors */
UINT cs16real, ds16real;	/* 16-bit segments for extender */
UINT code16sel, data16sel;	/* 16-bit cs,ds for extender */
UINT stack16sel;		/* 16-bit stack sel */
DWORD stackp16; 		/* 16-bit stack offset (use long!) */
UINT sel_incr;			/* increment to next selector */
UINT dosmem_sel;		/* selector for the first MB */
UINT fpu_status;
WORD DPMIversion = 0;
WORD previous_client_cs;

char dpmi10;			/* DPMI 1.0 server */
char dpmi10_map;		/* DPMI 1.0 mapping supported */
char dpmi10_page;		/* DPMI 1.0 access/dirty supported */
char dpmi10_zero;		/* DPMI 1.0 demand zero supported */

/* private vars */
static WORD DPMIdata_para_needed;
static WORD DPMIdata_segm_address;
static char fpu_status_init;

/*
** back to real-mode, terminate ( int0x21 must set to orginal )
*/
#ifndef __EMX__
extern void ProtectedModeSwitch(void);
extern void RealModeSwitch(void);
extern void _cexit(void);
#endif

void protected_to_real(WORD errorlevel)
{
    if (fpu_status_init)
	DpmiSetCoproStatus(fpu_status & 3);

#ifdef _MSC_VER
    RealModeSwitch();		/* clean up in real mode */
    _cexit();
    ProtectedModeSwitch();
#endif
#ifdef _TURBOC_
    RealModeSwitch();		/* clean up interrupt vector */
    _restorezero();
    ProtectedModeSwitch();
#endif

    dos_exit(errorlevel);
    /* program ends here */
}

void init_real_mode(void)
{
#ifdef __EMX__
    DWORD base_addr;	/* rsx32 is in protected mode */

    code16sel = GetCS();
    data16sel = stack16sel = GetDS();

    GetBaseAddress(code16sel, &base_addr);
    cs16real = (UINT) (base_addr >> 4);
    GetBaseAddress(data16sel, &base_addr);
    ds16real = (UINT) (base_addr >> 4);

#else			/* rsx16 is in real mode */
    cs16real = GetCS();
    ds16real = GetDS();
#endif
}

/*
** switch to protected-mode via DPMI-host
*/
int real_to_protected(WORD mode)
{
#ifndef __EMX__
    WORD DPMIflags;
    BYTE processor;
    DWORD PM_jump;		/* switch to protmode jump */

    clearregs();
    /* setbuf(stdout,NULL); */

    if (GetDpmiEntryPoint(&PM_jump, &DPMIdata_para_needed,
			  &DPMIflags, &DPMIversion, &processor)) {
	puts("No DPMI-host found!");
	return -1;
    }
    if (mode == 1 && !(DPMIflags & 1)) {
	puts("32bit programs not supported\n");
	return -1;
    }
    if (DPMIdata_para_needed) { /* get DPMI ring 0 stack */
	DPMIdata_segm_address = GetDpmiHostParagraph(DPMIdata_para_needed);
	if (!DPMIdata_segm_address) {
	    puts("Can't alloc memory for the DPMI-host-stack");
	    return -1;
	}
    }
    if (DpmiEnterProtectedMode(PM_jump, mode, DPMIdata_segm_address)) {
	puts("can't switch to Protected Mode");
	return -1;
    }
    /* Now we are in Protected Mode */

    /* lock DPMI-stack */
    if (DPMIdata_para_needed) {
	LockLinRegion((DWORD) DPMIdata_segm_address << 4,
		      (DWORD) DPMIdata_para_needed << 4);
    }
    /* get prot-mode extender segments */
    code16sel = GetCS();
    data16sel = stack16sel = GetDS();
#endif
    return 0;
}

/*
** test DPMI server
*/
int test_dpmi_capabilities(void)
{
    if (!DPMIversion) {
	DPMIVERSION ver;

	GetDPMIVersion(&ver);
	DPMIversion = ((WORD)ver.major << 8) | ver.minor;

	if (main_options.opt_printall)
	    printf("DPMI version %d.%d\n", ver.major, ver.minor);
    }

    if (!main_options.opt_force_dpmi09) {
	DPMICAP cap;
	if ((DPMIversion >> 8) >= 1)
	    dpmi10 = 1;
	if (!GetDPMICapabilities(&cap, iobuf)) {
	    if (cap.bits & 1)
		dpmi10_page = 1;
	    if (cap.bits & 8)
		dpmi10_map = 1;
	    if (cap.bits & 16)
		dpmi10_zero = 1;
	    if (dpmi10_page & dpmi10_map)	/* give 0.9x a chance */
		dpmi10 = 1;
	}
    }

    sel_incr = SelInc();

    /* build selector for first megabyte */
    AllocLDT(1, &dosmem_sel);
    SetBaseAddress(dosmem_sel, 0L);
    SetAccess(dosmem_sel, APP_DATA_SEL, BIG_BIT | GRANULAR_BIT);
    SetLimit(dosmem_sel, 1024L * 1024L - 1L);

    return 0;
}

static POINTER16_32 int21v;	/* old int21h handler address */
static POINTER16_32 int10v;	/* old int10h handler address */
static POINTER16_32 int33v;	/* old int33h handler address */
static POINTER16_32 ctrlcv;	/* old control-c handler */
static POINTER16_32 timerv;	/* old timer handler */
static POINTER16_32 debintv;	/* old int3 handler */
static POINTER16_32 excep0v, excep1v, excep2v, excep3v, excep4v, excep5v,
 excep6v, excep7v, excep8v, excep9v, excep10v, excep11v, excep12v, excep13v, excep14v,
 excep15v, excep16v, excep17v;

int hangin21;

POINTER16_16 int21rmv;	 /* dos int21h realmode handler */

static POINTER16_16 int24rmv;	/* dos int24h realmode handler */
static unsigned char int24_code[] =
    {	0xb0, 0x03,	/* mov al, 3	*/
	0xcf		/* iret 	*/
    };

#ifdef __EMX__
#define INT24_SEG(pointer) ((((unsigned)(pointer) & ~0xFFF) >> 4) + ds16real)
#define INT24_OFF(pointer) ((unsigned)(pointer) & 0xFFF)
#else /* 16 bit compiler */
#define INT24_SEG(pointer) (ds16real)
#define INT24_OFF(pointer) ((unsigned) (pointer))
#endif

int hangin_extender(void)
{
    UINT alias_cs;
    int stackp;

    /* entry stack for interrupts, exceptions */
    stackp16 = (UINT) & stackp;

    /* store ds-sel in code for some interrupt handler */
    if (CreatAlias(code16sel, &alias_cs))
	return -1;

    /* store 16bit data selector in load_ds function */
    store32(alias_cs, (DWORD) (UINT) extender_ds, (DWORD) data16sel);

    /* hang in int 0x21 */
    if (GetProtModeVector32(0x21, &int21v.sel, &int21v.off) == -1) {
	puts("error:can't get int21");
	return -1;
    }
    if (SetProtModeVector32(0x21, code16sel, (DWORD) (UINT) doscall) == -1) {
	puts("error:can't set int21");
	return -1;
    }
    hangin21 = 1;
    previous_client_cs = int21v.sel;
    align_iobuf();

    /* chain to default int 0x21 */
    store32(alias_cs, (DWORD) (unsigned) &int21vsel, (DWORD) int21v.sel);
    store32(alias_cs, (DWORD) (unsigned) &int21voff, (DWORD) int21v.off);

    /* set new bios int10 for vesa calls */
    if (GetProtModeVector32(0x10, &int10v.sel, &int10v.off)<0)
	puts("int10 fail:get");
    if (SetProtModeVector32(0x10, code16sel, (DWORD) (UINT) bioscall)<0)
	puts("int10 fail:set");
    store32(alias_cs, (DWORD) (unsigned) &int10vsel, (DWORD) int10v.sel);
    store32(alias_cs, (DWORD) (unsigned) &int10voff, (DWORD) int10v.off);

    /* TIMER */
    GetProtModeVector32(0x1C, &timerv.sel, &timerv.off);
    store32(alias_cs, (DWORD) (unsigned) &int1Cvsel, (DWORD) timerv.sel);
    store32(alias_cs, (DWORD) (unsigned) &int1Cvoff, (DWORD) timerv.off);
    SetProtModeVector32(0x1C, code16sel, (DWORD) (UINT) timer_handler);

    FreeLDT(alias_cs);

    /* MOUSE */
    GetProtModeVector32(0x33, &int33v.sel, &int33v.off);
    SetProtModeVector32(0x33, code16sel, (DWORD) (UINT) mousecall);


    /* get all exception vectors */
    GetExceptionVector32(0, &excep0v.sel, &excep0v.off);
    GetExceptionVector32(1, &excep1v.sel, &excep1v.off);
    GetExceptionVector32(2, &excep2v.sel, &excep2v.off);
    GetExceptionVector32(3, &excep3v.sel, &excep3v.off);
    GetExceptionVector32(4, &excep4v.sel, &excep4v.off);
    GetExceptionVector32(5, &excep5v.sel, &excep5v.off);
    GetExceptionVector32(6, &excep6v.sel, &excep6v.off);
    GetExceptionVector32(7, &excep7v.sel, &excep7v.off);
    GetExceptionVector32(8, &excep8v.sel, &excep8v.off);
    GetExceptionVector32(9, &excep9v.sel, &excep9v.off);
    GetExceptionVector32(10, &excep10v.sel, &excep10v.off);
    GetExceptionVector32(11, &excep11v.sel, &excep11v.off);
    GetExceptionVector32(12, &excep12v.sel, &excep12v.off);
    GetExceptionVector32(13, &excep13v.sel, &excep13v.off);
    GetExceptionVector32(14, &excep14v.sel, &excep14v.off);
    GetExceptionVector32(15, &excep15v.sel, &excep15v.off);
    GetExceptionVector32(16, &excep16v.sel, &excep16v.off);
    GetExceptionVector32(17, &excep17v.sel, &excep17v.off);

    /* set all exception vectors */
    if (!opt_debugrsx) {
	SetExceptionVector32(1, code16sel, (DWORD) (UINT) (excep1_386));
	SetExceptionVector32(3, code16sel, (DWORD) (UINT) (excep3_386));
    }
    SetExceptionVector32(0, code16sel, (DWORD) (UINT) (excep0_386));
    SetExceptionVector32(2, code16sel, (DWORD) (UINT) (excep2_386));
    SetExceptionVector32(4, code16sel, (DWORD) (UINT) (excep4_386));
    SetExceptionVector32(5, code16sel, (DWORD) (UINT) (excep5_386));
    SetExceptionVector32(6, code16sel, (DWORD) (UINT) (excep6_386));
    SetExceptionVector32(7, code16sel, (DWORD) (UINT) (excep7_386));
    SetExceptionVector32(8, code16sel, (DWORD) (UINT) (excep8_386));
    SetExceptionVector32(9, code16sel, (DWORD) (UINT) (excep9_386));
    SetExceptionVector32(10, code16sel, (DWORD) (UINT) (excep10_386));
    SetExceptionVector32(11, code16sel, (DWORD) (UINT) (excep11_386));
    SetExceptionVector32(12, code16sel, (DWORD) (UINT) (excep12_386));
    SetExceptionVector32(13, code16sel, (DWORD) (UINT) (excep13_386));
    SetExceptionVector32(15, code16sel, (DWORD) (UINT) (excep15_386));
    SetExceptionVector32(16, code16sel, (DWORD) (UINT) (excep16_386));
    SetExceptionVector32(17, code16sel, (DWORD) (UINT) (excep17_386));
    if (dpmi10)
	SetExceptionVector32(14, code16sel, (DWORD) (UINT) (page_fault));
    else
	SetExceptionVector32(14, code16sel, (DWORD) (UINT) (excep14_386));

    /* CTRL-C */
    GetProtModeVector32(0x23, &ctrlcv.sel, &ctrlcv.off);
    SetProtModeVector32(0x23, code16sel, (DWORD) (UINT) prot_cbrk);

    /* debug int3 */
    if (!opt_debugrsx) {
	GetProtModeVector32(0x03, &debintv.sel, &debintv.off);
	SetProtModeVector32(0x03, code16sel, (DWORD) (UINT) debug_entry);
    }

    GetRealModeVector(0x21, &int21rmv.sel, &int21rmv.off);

    GetRealModeVector(0x24, &int24rmv.sel, &int24rmv.off);
    SetRealModeVector(0x24, INT24_SEG(int24_code), INT24_OFF(int24_code));
    return 0;
}

/*
** init fpu
*/
int init_fpu(void)
{
    /* Get FPU status, if user allow this (not -e0) */
    if (opt_force_copro != 1) {
	if (DpmiGetCoproStatus(&fpu_status))
	    fpu_status = 0;
	if (main_options.opt_printall)
	    printf("fpu_status = 0x%X\n", fpu_status);
    }

    /*
    ** copro = 0 , with -e option : if no 387, set copro = 2
    ** copro = 1 , default : enable FPU
    ** copro = 3 , if no FPU : enable kernel FPU emulation
    */
    if (copro == 0 && !npx_present)	/* catch missing 387 */
	if (! opt_force_copro)
	    copro = 2;
    if (copro) {
	fpu_status_init = 1;
	if (DpmiSetCoproStatus(copro))
	    puts("Warning: No DPMI-server FPU support");
    }
    return 0;
}

static void free_emu_mem(void)
{
    UnlockLinRegion(RSX_PROCESS.memaddress, RSX_PROCESS.membytes);
    if (rsx387_in_dosmem)
	FreeDosMem((WORD) RSX_PROCESS.memhandle);
    else
	FreeMem(RSX_PROCESS.memhandle);
    FreeLDT(RSX_PROCESS.code32sel);
    FreeLDT(RSX_PROCESS.data32sel);
    FreeLDT(RSX_PROCESS.data32sel + sel_incr);
}

/* clean up interrupt handlers, exceptions, memory ... */
void clean_up(void)
{
    flush_all_buffers();
    clearregs();

    if (main_options.opt_printall)
	puts("cleanup now");

    if (DPMIdata_para_needed)
	UnlockLinRegion((DWORD) DPMIdata_segm_address << 4,
			(DWORD) DPMIdata_para_needed << 4);

    /* unset 387-usage (otherwise: crash) */
    if (fpu_status_init) {
	DpmiSetCoproStatus(fpu_status & 3);
	if (copro == 3 && emu_sel != 0)
	    free_emu_mem();
    }
    fpu_status_init = 0;
    copro = 0;

    SetProtModeVector32(0x21, int21v.sel, int21v.off);
    SetProtModeVector32(0x10, int10v.sel, int10v.off);
    SetProtModeVector32(0x33, int33v.sel, int33v.off);
    SetProtModeVector32(0x1C, timerv.sel, timerv.off);
    SetProtModeVector32(0x23, ctrlcv.sel, ctrlcv.off);
    SetProtModeVector32(0x03, debintv.sel, debintv.off);

    if (!opt_debugrsx) {
	SetExceptionVector32(1, excep1v.sel, excep1v.off);
	SetExceptionVector32(3, excep3v.sel, excep3v.off);
    }
    SetExceptionVector32(0, excep0v.sel, excep0v.off);
    SetExceptionVector32(2, excep2v.sel, excep2v.off);
    SetExceptionVector32(4, excep4v.sel, excep4v.off);
    SetExceptionVector32(5, excep5v.sel, excep5v.off);
    SetExceptionVector32(6, excep6v.sel, excep6v.off);
    SetExceptionVector32(7, excep7v.sel, excep7v.off);
    SetExceptionVector32(8, excep8v.sel, excep8v.off);
    SetExceptionVector32(9, excep9v.sel, excep9v.off);
    SetExceptionVector32(10, excep10v.sel, excep10v.off);
    SetExceptionVector32(11, excep11v.sel, excep11v.off);
    SetExceptionVector32(12, excep12v.sel, excep12v.off);
    SetExceptionVector32(13, excep13v.sel, excep13v.off);
    SetExceptionVector32(14, excep14v.sel, excep14v.off);
    SetExceptionVector32(15, excep15v.sel, excep15v.off);
    SetExceptionVector32(16, excep16v.sel, excep16v.off);
    SetExceptionVector32(17, excep17v.sel, excep17v.off);

    SetRealModeVector(0x24, int24rmv.sel, int24rmv.off);
}
