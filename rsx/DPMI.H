/*
** This file is DPMI.H
** Copyright (c) Rainer Schnitker 1991-1994
*/

#ifndef _DPMI_H
#define _DPMI_H

#ifndef _DPMI_TYPES
#define _DPMI_TYPES

/* always defined in windows.h */
#ifndef __WINDOWS_H
#ifndef _WINDOWS_16_
#ifndef _INC_WINDOWS
typedef unsigned char	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;
typedef unsigned int	UINT;
typedef unsigned long	ULONG;
#endif
#endif
#endif

#endif /* _DPMI_TYPES */

#ifdef __GNUC__     /* disable far, near statements for 32bit */
#define far
#define near
#endif

typedef struct {
		DWORD off ;
		UINT sel ;
		} POINTER16_32;

typedef struct {
		UINT off ;
		UINT sel ;
		} POINTER16_16;

typedef struct {
		WORD lim_lo,base_lo;
		BYTE base_mi,access;
		BYTE lim_hi,base_hi;
		} DESCRIPTOR ;
typedef DESCRIPTOR near *NPDESCRIPTOR;
typedef DESCRIPTOR far *LPDESCRIPTOR;

typedef struct {
	WORD limit,lo,hi ;
	   } GDTR ;
typedef GDTR far *LPGDTR;

typedef struct {
	WORD off_lo;
	WORD sel;
	BYTE count;
	BYTE type;
	WORD off_hi;
	} GATE;
typedef GATE far *LPGATE;

/* dpmi: GetFreeMemInfo */
typedef struct { DWORD LargestFree;
		 DWORD MaxUnlockedPages;
		 DWORD MaxLockedPages;
		 DWORD AddressSpacePages;
		 DWORD UnlockedPages;
		 DWORD UnusedPages;
		 DWORD PhysicalPages;
		 DWORD FreeAddressSpacePages;
		 DWORD PagingFilePages;  /* or partition size */
		 DWORD reserved[3];
	       } FREEMEMINFO ;
typedef FREEMEMINFO near *NPFREEMEMINFO;
typedef FREEMEMINFO far  *LPFREEMEMINFO;


/* dpmi: SimulateRMint,CallRMprocFar,CallRMprocIret */

typedef struct { DWORD edi,esi,ebp,reserved,ebx,edx,ecx,eax ;
		 WORD  flags,es,ds,fs,gs,ip,cs,sp,ss ;
		} TRANSLATION ;
typedef TRANSLATION near *NPTRANSLATION ;
typedef TRANSLATION far  *LPTRANSLATION ;

typedef struct { WORD	di,edi,
			si,esi,
			bp,ebp,
			resl,resh,
			bx,ebx,
			dx,edx,
			cx,ecx,
			ax,eax,
			flags,es,ds,fs,gs,ip,cs,sp,ss;
	       } TRANS286 ;
typedef TRANS286 far *LPTRANS286;

/* dpmi: 16bit stack frame for exceptions */
typedef struct {
     unsigned int err,flags,ip,
	  ax,bx,cx,dx,bp,sp,
	  cs,ds,es,ss,di,si ;
	  } REG286;

/* dpmi: 32bit stack frame for exceptions */
typedef struct {
     unsigned long gs,fs,es,ds,
		edi,esi,ebp,esporg,ebx,edx,ecx,eax,
		faultno,err,eip,cs,eflags,esp,ss;
	  } REG386;

/* dpmi: GetDPMIVersion */
typedef struct {
	BYTE major,minor ;
	WORD flags ;
	BYTE cpu,master,slave ;
	} DPMIVERSION ;

/* descriptor makros */
#define DESC_BASE(d)  (((((DWORD)d.base_hi<<8)|(DWORD)d.base_mi)<<16)|(DWORD)d.base_lo)
#define DESC_LIMIT(d) ((((DWORD)(d.lim_hi&15))<<16)|(DWORD)d.lim_lo)
#define GATE_OFFSET(d) (((((DWORD)d.base_hi<<8)|(DWORD)d.lim_hi)<<16)|(DWORD)d.lim_lo)

/* descriptor fields */
#define ACCESS_BIT		1
#define READ_BIT		2
#define WRITE_BIT		2
#define CONFIRMING_BIT		4
#define EXPAND_BIT		4
#define CODE_BIT		8
#define SEGMENT_BIT		16
#define PRESENT_BIT		128
#define AVL_BIT 		16
#define DEFAULT_BIT		64
#define BIG_BIT 		64
#define GRANULAR_BIT		128
#define DPL_MASK		96
#define TYPE_MASK		15
#define LIMIT_HI_MASK		15

#define APP_CODE_SEL		250   /* dpl=3 , read */
#define APP_DATA_SEL		242   /* dpl=3 ,write */

#define TYPE_CODE		10
#define TYPE_CONFIRM_CODE	14
#define TYPE_DATA		2
#define TYPE_EXPAND_DATA	6
#define TYPE_286TSS		1
#define TYPE_LDT		2
#define TYPE_BUSY286TSS 	3
#define TYPE_286CALLGATE	4
#define TYPE_TASKGATE		5
#define TYPE_286INTGATE 	6
#define TYPE_286TRAPGATE	7
#define TYPE_386TSS		9
#define TYPE_BUSY386TSS 	11
#define TYPE_386CALLGATE	12
#define TYPE_386INTGATE 	14
#define TYPE_386TRAPGATE	15

/* breakpoints-types */
#define BREAK_CODE		0x0001
#define BREAK_DATA_W1		0x0101
#define BREAK_DATA_RW1		0x0201
#define BREAK_DATA_W2		0x0102
#define BREAK_DATA_RW2		0x0202
#define BREAK_DATA_W4		0x0104
#define BREAK_DATA_RW4		0x0204

/* memory strategy for extmalloc */
#define NOLIMIT  0
#define XMSLIMIT 1		/* only alloc memory <= mxslimit */
#define LOCKMEM  2		/* locked memory */


/* INTERFACE for DPMI SPECIFICATION 0.9 */

/* LDT Descriptor management services  DPMI 0.9 */
int	AllocLDT (UINT, UINT *);
int	FreeLDT (UINT);
int	SegToSel (UINT, UINT *);
UINT	SelInc (void);
int	LockSel (UINT);
int	UnlockSel (UINT);
int	GetBaseAddress (UINT, DWORD *);
int	SetBaseAddress (UINT, DWORD);
int	SetLimit (UINT, DWORD);
int	SetAccess ( UINT, BYTE, BYTE);
int	CreatAlias (UINT, UINT *);
int	GetDescriptor (UINT, NPDESCRIPTOR);
int	SetDescriptor (UINT, NPDESCRIPTOR);
int	AllocSpecialLDT (UINT);

/* DOS Memory Services DPMI 0.9 */
int	AllocDosMem (UINT, UINT *, UINT *);
int	FreeDosMem (UINT);
int	ResizeDosMem (UINT, UINT, UINT *);

/* Interrupt Services  DPMI 0.9 */
int	GetRealModeVector (BYTE, UINT *, UINT *);
int	SetRealModeVector (BYTE, UINT, UINT);
int	GetExceptionVector (BYTE, UINT *, UINT *);
int	SetExceptionVector (BYTE, UINT, UINT);
int	GetProtModeVector (BYTE, UINT *, UINT *);
int	SetProtModeVector (BYTE, UINT, UINT);
/* 32 bit extentions */
int	GetExceptionVector32 (BYTE, UINT *, DWORD *);
int	SetExceptionVector32 (BYTE, UINT, DWORD);
int	GetProtModeVector32 (BYTE, UINT *, DWORD *);
int	SetProtModeVector32 (BYTE, UINT, DWORD);

/* Translation services DPMI 0.9 */
int	SimulateRMint (BYTE, BYTE, UINT, NPTRANSLATION, ...);
int	CallRMprocFar (BYTE, UINT, NPTRANSLATION, ...);
int	CallRMprocIret (BYTE, UINT, NPTRANSLATION, ...);
int	AllocRMcallAddress (UINT, UINT, NPTRANSLATION, UINT *, UINT *);
int	FreeRMcallAddress (UINT, UINT);
int	GetStateSaveAddress (UINT *, POINTER16_16 *, POINTER16_16 *);
int	SaveState (UINT *, POINTER16_16);
int	RestoreState (UINT *, POINTER16_16);

/* Get Version DPMI 0.9 */
void	GetDPMIVersion (DPMIVERSION *);

/* Memory managment services DPMI 0.9 */
int	GetFreeMemInfo (NPFREEMEMINFO);
int	AllocMem (DWORD, DWORD *, DWORD *);
int	FreeMem (DWORD);
int	ResizeMem (DWORD, DWORD, DWORD *, DWORD *);
int	GetFreeMemInfo32 (NPFREEMEMINFO);

/* Page locking services  DPMI 0.9 */
int	LockLinRegion(DWORD, DWORD);
int	UnlockLinRegion(DWORD, DWORD);
int	MarkRealModePageable(DWORD, DWORD);
int	RelockRealModeRegion(DWORD, DWORD);
int	GetPageSize(DWORD *);

/* Damand page performance tuning services */
int	MarkPageDemand(DWORD, DWORD);
int	DiscardPage(DWORD, DWORD);

/* Physical Address Mapping DPMI 0.9 */
int	PhysicalMap(DWORD, DWORD, DWORD *);

/* Virtuel interrupt state functions DPMI 0.9 */
BYTE	DisableVirtuelInterruptState (void);
BYTE	EnableVirtuelInterruptState (void);
BYTE	GetVirtuelInterruptState (void);

/* Get Vendor Specific Api Entry Point */
int	GetVendorEntry (BYTE *, UINT *, UINT *);

/* Debug Register support DPMI 0.9 */
int SetDebugWatchpoint (DWORD, WORD, WORD *);
int ClearDebugWatchpoint (WORD);
int GetStateDebugWatchpoint (WORD, WORD *);
int ResetDebugWatchpoint (WORD);


/* UTILITIES for DPMI SPECIFICATION 0.9 */

UINT	GetCS (void);
UINT	GetDS (void);
UINT	GetES (void);
void	dos_exit(WORD);
int	DpmiEnableFpu(WORD);
int	DpmiDisableFpu(void);
int	GetDpmiEntryPoint(DWORD *entry,WORD *para,WORD *flags,WORD *v,BYTE *cpu);
int	GetDpmiHostParagraph(WORD paragr);
int	DpmiEnterProtectedMode(DWORD entry,WORD mode,WORD hostsegm);

/* switching between real and protected mode */
int	real_to_protected(WORD);
void	protected_to_real(WORD);

/* c-functions using DPMI */
int	FreeManyLDT(WORD,WORD);
void	PrintFreeMemInfo(NPFREEMEMINFO);
void	PrintDescriptor(LPDESCRIPTOR);
void	far * IncFP(void far *);
void	far * DecFP(void far *);

/* high level c-functions for DPMI */
void	farcopy(void far *, void far *,DWORD);
int	extmovedata(void far *,void far *,DWORD);
void	far *extmalloc(DWORD);
void	extfree(void far *);
void	set_memory_type(WORD);

/* others */
int	IsWindowsEnhanced(void);
int	IsPM(void);
void yield(void);


/* INTERFACE FOR PROTECTED MODE 16/32 BIT */

DWORD lsl32(UINT);
WORD lsl16(UINT);
DWORD lar32(UINT);
WORD lar16(UINT);
WORD verr16(UINT);
WORD verw16(UINT);
void sgdt16(LPGDTR);
void sidt16(LPGDTR);
WORD sldt16(void);
WORD str16(void);

#ifdef __WATCOMC__
#include "externa.h"
#endif

#endif /* _DPMI_H */
