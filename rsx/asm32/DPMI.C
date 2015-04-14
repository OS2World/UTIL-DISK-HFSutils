/*****************************************************************************
 * FILE: DPMI.C 							     *
 *									     *
 * DESC:								     *
 *	- DPMI 0.9 functions for GNUC					     *
 *									     *
 * Copyright (C) 1993,1994						     *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "dpmi.h"

/*
** this file could also be used for a include file with
** #define INLINE extern inline
*/

#ifndef INLINE
#define INLINE
#endif

#define CHECKERR "jnc    1f\n\tmovl   $-1, %0 \n\tjmp    2f \n\t1: \n\t"
#define CHECK_ERR_DPMI10 "jc    2f\n\t"
#define OKEAX0 "xorl   %0, %0 \n\t2: \n\t"

INLINE int AllocLDT(UINT n_sel, UINT * first_sel)
{
    register int _v;

    __asm__ __volatile__(
	 "int    $0x31 \n\t"
	 CHECKERR
	 "movzwl %%ax, %%eax\n\t"
	 "movl   %%eax, %2 \n\t"
	 OKEAX0
	:"=a"(_v)
	:"c"(n_sel), "m"(*first_sel), "0"(0x000)
	:"ax", "cx"
    );
    return _v;
}

INLINE int FreeLDT(UINT sel)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "0"(0x001)
	:"ax", "bx"
    );

    return _v;
}

INLINE int SegToSel(UINT segm, UINT * sel)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movzwl %%ax, %%eax \n\t"
	"movl   %%ax, %2 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"(segm), "m"(*sel), "0"(0x002)
	:"ax", "bx"
    );
    return _v;
}

INLINE UINT SelInc(void)
{
    register unsigned short _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	:"=a"((unsigned short) _v)
	:"0"((short) 0x003)
	:"ax"
    );
    return (unsigned int) _v;
}

INLINE int GetBaseAddress(UINT sel, ULONG * base)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movw   %%dx, %2 \n\t"
	"movw   %%cx, %3 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "m"(*base), "m"(*((short *) base + 1)), "0"(0x006)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SetBaseAddress(UINT sel, DWORD base)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "c"((short) (base >> 16)), "d"((short) base), "0"(0x007)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SetLimit(UINT sel, DWORD limit)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "c"((short) (limit >> 16)), "d"((short) limit), "0"(0x008)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SetAccess(UINT sel, BYTE acc, BYTE acc_hi)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "c"(((short) acc_hi << 8) | acc), "0"(0x009)
	:"ax", "bx", "cx"
    );
    return _v;
}

INLINE int CreatAlias(UINT sel, UINT * alias)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movzwl %%ax, %%eax \n\t"
	"movl   %%eax, %2 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "m"(*alias), "0"(0xa)
	:"ax", "bx"
    );
    return _v;
}

INLINE int GetDescriptor(UINT sel, NPDESCRIPTOR desc)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "D"((long) desc), "0"(0x00B)
	:"ax", "bx", "di"
    );
    return _v;
}

INLINE int SetDescriptor(UINT sel, NPDESCRIPTOR desc)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "D"((long) desc), "0"(0x00C)
	:"ax", "bx", "di"
    );
    return _v;
}

INLINE int AllocSpecialLDT(UINT sel)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(sel), "0"(0x00D)
	:"ax", "bx"
    );
    return _v;
}

/*
** on error : return value = DOS error
** segm = maximaler request
*/

INLINE int AllocDosMem(UINT paragr, UINT * segm, UINT * sel)
{
    register short _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	"jnc    1f \n\t"
	"movzwl %%bx, %%ebx \n\t"
	"movl   %%ebx, %2 \n\t"
	"jmp    2f \n\t"
	"1: \n\t"
	"movzwl %%ax, %%eax \n\t"
	"movl   %%eax, %2 \n\t"
	"movzwl %%dx, %%edx \n\t"
	"movl   %%edx, %3 \n\t"
	"xorl   %0, %0 \n\t"
	"2: \n\t"
	:"=a"((short) _v)
	:"b"(paragr), "m"(*segm), "m"(*sel), "0"(0x100)
	:"ax", "bx", "dx"
    );
    return (int) _v;
}

INLINE int FreeDosMem(UINT sel)
{
    register char _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"((char) _v)
	:"d"(sel), "0"(0x101)
	:"ax", "dx"
    );
    return (int) _v;
}

INLINE int ResizeDosMem(UINT sel, UINT bytes, UINT * newsel)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	"jc     1f \n\t"
	"movl   $-1, %0 \n\t"
	"jmp    2f \n\t"
	"1: \n\t"
	"movzwl %%dx, %%edx \n\t"
	"movl   %%edx, %3 \n\t"
	"xorl   %0, %0 \n\t"
	"2: \n\t"
	:"=a"(_v)
	:"d"(sel), "b"(bytes), "m"(*newsel), "0"(0x102)
	:"ax", "bx", "dx"
    );
    return _v;
}

INLINE int GetRealModeVector(BYTE inum, UINT * segm, UINT * offs)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movzwl %%cx, %%ecx \n\t"
	"movl   %%ecx, %2 \n\t"
	"movzwl %%dx, %%edx \n\t"
	"movl   %%edx, %3 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"((char) inum), "m"(*segm), "m"(*offs), "0"(0x200)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SetRealModeVector(BYTE inum, UINT segm, UINT offs)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((char) inum), "c"((short) segm), "d"((short) offs), "0"(0x201)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int GetExceptionVector32(BYTE inum, UINT * sel, DWORD * offs)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movzwl %%cx, %%ecx \n\t"
	"movl   %%ecx, %2 \n\t"
	"mov    %%edx, %3 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"((char) inum), "m"(*sel), "m"(*offs), "0"(0x202)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SetExceptionVector32(BYTE inum, UINT sel, DWORD offs)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((char) inum), "c"(sel), "d"(offs), "0"(0x203)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int GetProtModeVector32(BYTE inum, UINT * sel, DWORD * offs)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movzwl %%cx, %%ecx \n\t"
	"movl   %%ecx, %2 \n\t"
	"mov    %%edx, %3 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"((char) inum), "m"(*sel), "m"(*offs), "0"(0x204)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SetProtModeVector32(BYTE inum, UINT sel, DWORD offs)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((char) inum), "c"(sel), "d"(offs), "0"(0x205)
	:"ax", "bx", "cx", "dx"
    );
    return _v;
}

INLINE int SimulateRMint(BYTE intno, BYTE r, UINT w, NPTRANSLATION rmcall,...)
{
    register int _v;

    __asm__ __volatile__(
	"xorb   %%bh, %%bh \n\t"
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((BYTE) intno), "c"(0), "D"((long) rmcall), "0"(0x300)
	:"ax", "bx", "cx", "di"
    );
    return _v;
}

INLINE int CallRMprocFar(BYTE r, UINT w, NPTRANSLATION rmcall,...)
{
    register int _v;

    __asm__ __volatile__(
	"xorb   %%bh, %%bh \n\t"
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"c"(0), "D"((long) rmcall), "0"(0x301)
	:"ax", "bx", "cx", "di"
    );
    return _v;
}

INLINE int CallRMprocIret(BYTE b, UINT w, NPTRANSLATION rmcall,...)
{
    register int _v;

    __asm__ __volatile__(
	"xorb   %%bh, %%bh \n\t"
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"c"(0), "D"((long) rmcall), "0"(0x302)
	:"ax", "bx", "cx", "di"
    );
    return _v;
}

INLINE void GetDPMIVersion(DPMIVERSION * version)
{
    __asm__ __volatile__(
	"int    $0x31 \n\t"
	"movb   %%ah, (%0) \n\t"
	"movb   %%al, 1(%0) \n\t"
	"movw   %%bx, 2(%0) \n\t"
	"movb   %%cl, 4(%0) \n\t"
	"movb   %%dh, 5(%0) \n\t"
	"movb   %%dl, 6(%0) \n\t"
	:
	:"D"((long) version), "a"(0x400)
	: "ax","bx","cx","dx"
    );
    return ;
}

INLINE int GetFreeMemInfo(NPFREEMEMINFO info)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"D"((long) info), "0"(0x500)
	:"ax", "di"
    );
    return _v;
}

INLINE int AllocMem(DWORD size, DWORD * handle, DWORD * base)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movl   %3 , %%edx \n\t"
	"movw   %%si, 2(%%edx) \n\t"
	"movw   %%di, (%%edx) \n\t"
	"movl   %4 , %%edx \n\t"
	"movw   %%bx, 2(%%edx) \n\t"
	"movw   %%cx, (%%edx) \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"((short) (size >> 16)), "c"((short) size)
	,"m"(handle), "m"(base)
	,"0"(0x501)
	:"ax", "bx", "cx", "dx", "di", "si"
    );
    return _v;
}

INLINE int FreeMem(DWORD handle)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"S"((short) (handle >> 16)), "D"((short) handle), "0"(0x502)
	:"ax", "si", "di"
    );
    return _v;
}

INLINE int ResizeMem(DWORD size, DWORD handle, DWORD * newhandle, DWORD * newbase)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movl   %5 , %%edx \n\t"
	"movw   %%si, 2(%%edx) \n\t"
	"movw   %%di, (%%edx) \n\t"
	"movl   %6 , %%edx \n\t"
	"movw   %%bx, 2(%%edx) \n\t"
	"movw   %%cx, (%%edx) \n\t"
	OKEAX0
	:"=a"(_v)
	:"b"((short) (size >> 16)), "c"((short) size),
	 "S"((short) (handle >> 16)), "D"((short) handle),
	 "m"(newhandle), "m"(newbase), "0"(0x503)
	:"ax", "bx", "cx", "dx", "di", "si"
    );
    return _v;
}

int LockLinRegion(DWORD size, DWORD address)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((short) (address >> 16)), "c"((short) address),
	 "S"((short) (size >> 16)), "D"((short) size),
	 "0"(0x600)
	:"ax", "bx", "cx", "si", "di"
    );
    return _v;
}

int UnlockLinRegion(DWORD size, DWORD address)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((short) (address >> 16)), "c"((short) address),
	 "S"((short) (size >> 16)), "D"((short) size),
	 "0"(0x601)
	:"ax", "bx", "cx", "si", "di"
    );
    return _v;
}

int MarkRealModePageable(DWORD size, DWORD address)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((short) (address >> 16)), "c"((short) address),
	 "S"((short) (size >> 16)), "D"((short) size),
	 "0"(0x602)
	:"ax", "bx", "cx", "si", "di"
    );
    return _v;
}

int RelockRealModeRegion(DWORD size, DWORD address)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((short) (address >> 16)), "c"((short) address),
	 "S"((short) (size >> 16)), "D"((short) size),
	 "0"(0x602)
	:"ax", "bx", "cx", "si", "di"
    );
    return _v;
}

INLINE int GetPageSize(DWORD * size)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movw   %%cx, %1 \n\t"
	"movw   %%bx, %2 \n\t"
	OKEAX0
	:"=a"(_v)
	:"m"(*size), "m"(*((char *) size + 2)), "0"(0x604)
	:"ax", "bx", "cx"
    );
    return _v;
}

int MarkPageDemand(DWORD size, DWORD address)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((short) (address >> 16)), "c"((short) address),
	 "S"((short) (size >> 16)), "D"((short) size),
	 "0"(0x702)
	:"ax", "bx", "cx", "si", "di"
    );
    return _v;
}

int SetDebugWatchpoint(DWORD address, WORD type, WORD *handle)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movw   %%bx, %4 \n\t"
	OKEAX0
	:"=a"(_v)
	:"b" ((short) (address >> 16)), "c" ((short) address),
	 "d" ((short) (type)), "m" (*(short *)handle),"0" (0xB00)
	:"ax", "bx", "cx"
    );
    return _v;
}

int ClearDebugWatchpoint(WORD handle)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a" (_v)
	:"b" ((short) (handle)),"0" (0xB01)
	:"ax", "bx"
    );
    return _v;
}

int GetStateDebugWatchpoint(WORD handle, WORD *state)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	"movw   %%ax, %2 \n\t"
	OKEAX0
	: "=a" (_v)
	: "b" ((short) (handle)), "m" (*(short *)state),"0" (0xB02)
	: "ax", "bx"
    );
    return _v;
}

int ResetDebugWatchpoint(WORD handle)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((short) (handle)), "0"(0xB03)
	:"ax", "bx"
    );
    return _v;
}

INLINE void get_segment(int sel, void *from, void *to, int size)
{
    __asm__("pushw  %%fs \n\t"
	    "movw   %%ax,%%fs \n\t"
	    "cld \n\t"
	    "testb  $1,%%cl \n\t"
	    "je     1f \n\t"
	    "fs ; movsb \n\t"
	    "1: \n\t"
	    "testb  $2,%%cl \n\t"
	    "je     2f \n\t"
	    "fs ; movsw \n\t"
	    "2: \n\t"
	    "shrl   $2,%%ecx \n\t"
	    "rep ; fs ; movsl \n\t"
	    "popw   %%fs \n\t"
	    ::"a"(sel), "c"(size), "D"((long) to), "S"((long) from)
	    :"cx", "di", "si");
}

UINT unsigned GetCS()
{
    register short _v;
  __asm__("movw   %%cs, %0 ":"=r"((short) _v));
    return (UINT) _v;
}

UINT unsigned GetDS()
{
    register short _v;
  __asm__("movw   %%ds, %0 ":"=r"((short) _v));
    return (UINT) _v;
}

UINT GetES()
{
    register unsigned short _v;
  __asm__("movw   %%es, %0 ":"=r"((short) _v));
    return (UINT) _v;
}

void volatile _dos_exit(WORD exit)
{
    __asm__ __volatile__(
	"int    $0x21"
	:   /* void */
	:"a"((exit & 0xFF) | 0x4C00)
    );
}

DWORD lsl32(UINT sel)
{
    register unsigned _v;

    __asm__ __volatile__(
	"lsl %1,%0 "
	:"=r"(_v)
	:"r"(sel)
    );
    return _v;
}

int DpmiEnableFpu(WORD bits)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"((WORD)bits), "0"(0xE01)
	:"ax", "bx"
    );
    return _v;
}

int DpmiDisableFpu(void)
{
    register int _v;

    __asm__ __volatile__(
	"int    $0x31 \n\t"
	CHECKERR
	OKEAX0
	:"=a"(_v)
	:"b"(0), "0"(0xE01)
	:"ax", "bx"
    );
    return _v;
}

void dos_exit(WORD exit)
{
    __asm__ __volatile__(
	"movb   $0x4C, %%ah \n\t"
	"int    $0x21 \n\t"
	:
	:"a"((unsigned char) exit)
    );
}

void clearregs(void)
{
}
