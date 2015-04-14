/*****************************************************************************
 * FILE: fpu.c								     *
 *									     *
 * DESC:								     *
 *	- FPU saving, FPU-EMU install					     *
 *									     *
 * Copyright (C) 1993,1994						     *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "DPMI.H"
#include "DPMI10.H"
#include "PROCESS.H"
#include "START32.H"
#include "FPU.H"

extern int used_math;
extern int debugged;
extern union i387_union *process_npx;

DWORD emu_init(void)
{
    int _ret;
    __asm__(
	"pushl  %%ebx \n\t"
	"pushl  %%edi \n\t"
	"pushl  %%esi \n\t"
	"pushl  _data16sel \n\t"
	"popl   %%gs \n\t"
	"pushl  %%cs \n\t"
	"call   _fpu_entry \n\t"
	"popl   %%esi \n\t"
	"popl   %%edi \n\t"
	"popl   %%ebx \n\t"
	: "=a"(_ret)
	: "d"(stackp16), "0"(0x12345678)
	: "ax", "dx"
    );
    return _ret;
}

void emu_switch(int x, int y)
{
    used_math = x;
    debugged = y;
    process_npx = &(npz->npx);
}

void do_fnsave(union i387_union * fpu_struct)
{
    __asm__(
	"fnsave %0"
	:"=m"( *fpu_struct)
    );
}

void do_frstor(union i387_union * fpu_struct)
{
    __asm__(
	"frstor %0"
	::"m"( *fpu_struct)
    );
}

void do_fninit(void)
{
    __asm__("fninit");
}

extern char * getenv(char *);

int npx_installed(void)
{
    char *s = getenv("RSX_x87");
    if (!s)
	return 0;
    else
	return (int) (*s - '0');
}
