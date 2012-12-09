/*****************************************************************************
 * FILE: sysdep2.c							     *
 *									     *
 * DESC:								     *
 *	- system functions for EMX/GCC					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

/* Modified for hfsutils by Marcus Better, July 1997 */

#include "DPMI.H"
#include "PROCESS.H"
#include "PRINTF.H"
#include "SYSDEP.H"

DWORD copro_struct;		/* address of soft-387 struct */
UINT emu_sel = 0;		/* emu data-sel */
DWORD emu_esp;			/* emu esp entry */
POINTER16_32 emu_entry;		/* entry address emu (for init) */

void save_emu_state(NEWPROCESS * proc)
{
    /*
    ** buildin emu uses proc->npx
    */
}

void load_emu_state(NEWPROCESS * proc)
{
    /*
    ** buildin emu uses proc->npx
    */
}

int install_rsx387()
{
/*    emu_init();*/
    return 0;
}

/* convert PSP to command line */
void get_emxl_psp(unsigned emxl_psp)
{
    /* done by loader */
}

/* make command string to argv */
void build_emx_args(int *argc, char ***argv)
{
    /* done by loader */
}

void build_dj_args(int *argc, char ***argv)
{
    /* done by loader */
}

void flush_all_buffers(void)
{
}
