#ifndef _DPMI10_H
#define _DPMI10_H

/* struct DESCRIPTOR needed */
#ifndef _DPMI_H
#include "DPMI.H"
#endif

#define LIN_MEM_UNCOMMIT    0
#define LIN_MEM_COMMIT	    1

#define PAGE_UNCOMMIT	    0
#define PAGE_COMMIT	    1
#define PAGE_MAPPED	    2
#define PAGE_RDWR	    8
#define PAGE_ACCDIRTY	   16
#define PAGE_ACCESS	   32
#define PAGE_DIRTY	   64

#define FPU_ENABLE	    1
#define FPU_EMU_CLIENT	    2
#define FPU_PRESENT	    4
#define FPU_EMU_HOST	    8
#define FPU_GET_NO(type)    (((type) >> 4 ) & 0xF)

typedef struct {
    DWORD   HostPhysicalMemory ;
    DWORD   HostVirtuelMemory ;
    DWORD   HostFreeVirtuelMemory ;
    DWORD   VMVirtuelMemory ;
    DWORD   VMFreeVirtuelMemory ;
    DWORD   ClientVirtuelMemory ;
    DWORD   ClientFreeVirtuelMemory ;
    DWORD   LockedMemory ;
    DWORD   ClientMaxLockedMemory ;
    DWORD   ClientHighestLinearAddress ;
    DWORD   LargestMemoryBlock	;
    DWORD   MinimumUnitValue ;
    DWORD   MemoryAlignValue ;
    BYTE    res[76];
    } DPMI10INFO;

typedef struct {
    WORD    bits;
    WORD    res1;
    WORD    res2;
    } DPMICAP ;

typedef struct {
    DWORD   error_code;
    DWORD   eip;
    DWORD   cs;
    DWORD   eflags;
    DWORD   esp;
    DWORD   ss;
    DWORD   ds;
    DWORD   es;
    DWORD   fs;
    DWORD   gs;
    DWORD   cr2;
    DWORD   pte;
    } EXCEPTION_10 ;

int	GetMultipleDescriptors(UINT, DESCRIPTOR *);
int	SetMultipleDescriptors(UINT, DESCRIPTOR *);
int	GetProtModeExceptionVector32(BYTE, UINT *, DWORD *);
int	GetRealModeExceptionVector32(BYTE, UINT *, DWORD *);
int	SetProtModeExceptionVector32(BYTE, UINT, DWORD);
int	SetRealModeExceptionVector32(BYTE, UINT, DWORD);
int	GetDPMICapabilities(DPMICAP *,BYTE *);
int	AllocLinearMemory(DWORD bytes,DWORD linaddress,DWORD flags,DWORD *handle,DWORD *memaddress);
int	ResizeLinearMemory(DWORD bytes,DWORD handle,DWORD flags,DWORD *newhandle,DWORD *newmemaddress);
int	GetPageAttributes(DWORD handle,DWORD offs,DWORD pages,WORD *attr);
int	ModifyPageAttributes(DWORD handle,DWORD offs,DWORD pages,WORD *attr);
int	MapDeviceInMemoryBlock(DWORD handle,DWORD offs,DWORD pages,DWORD device);
int	MapDOSMemInMemoryBlock(DWORD handle,DWORD offs,DWORD pages,DWORD dosmem);
int	GetMemoryBlockData(DWORD handle,DWORD *addr,DWORD *bytes);
int	GetMemoryInfo(DPMI10INFO *);
int	FreePhysicalMapping(DWORD address);
int	DpmiGetCoproStatus(UINT *status);
int	DpmiSetCoproStatus(UINT status);

#endif /* _DPMI10_H */
