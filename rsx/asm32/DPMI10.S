/*****************************************************************************
 * FILE: DPMI10.S							     *
 *									     *
 * DESC:								     *
 *	- DPMI 1.0 functions / for GNU-C (386)				     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/


.text

#define ALIGN_DATA	.align	2
#define ALIGN_TEXT	.align	2,0x90
#define ALIGN_486_TEXT	.align	4,0x90

#define NAME(x) _ ## x
#define LNAME(x) _ ## x ## :

#define ASFUNC(x)   .globl NAME(x); ALIGN_TEXT; LNAME(x)
#define BEGIN	    pushl %ebp ; movl %esp,%ebp
#define END	    leave; ret

#define DPMI(V)     movw V , %ax ; int $0x31
#define CHKERR	    1: ; jc 2f ;
#define CHKERR_1    jnc 1f; movl $-1, %eax; jmp 2f; 1:
#define OKEAX0	    xorl %eax, %eax; 2:

#define PAR1	    8(%ebp)
#define PAR2	    12(%ebp)
#define PAR3	    16(%ebp)
#define PAR4	    20(%ebp)
#define PAR5	    24(%ebp)

#define PUSH1(x)	pushl ##x
#define PUSH2(x,y)	pushl ##x ; pushl ##y
#define PUSH3(x,y,z)	pushl ##x ; pushl ##y ; pushl ##z

#define POP1(x) 	popl ##x
#define POP2(x,y)	popl ##y ; popl ##x
#define POP3(x,y,z)	popl ##z ; popl ##y ; popl ##x


/*
int GetMultipleDescriptors(UINT count, DESCRIPTOR *buffer)
*/
ASFUNC(GetMultipleDescriptors)
BEGIN
	pushl	%edi
	movl	PAR1, %ecx	/* number sel */
	movl	PAR2, %edi	/* buffer */
	DPMI	($0x00E)
	CHKERR
	OKEAX0
	popl	%edi
END

/*
int SetMultipleDescriptors(UINT count, DESCRIPTOR *buffer)
*/
ASFUNC(SetMultipleDescriptors)
BEGIN
	pushl	%edi
	movl	PAR1, %ecx	/* number sel */
	movl	PAR2, %edi	/* buffer */
	DPMI	($0x00F)
	CHKERR
	OKEAX0
	popl	%edi
END

/*
int GetProtModeExceptionVector32(BYTE no, UINT *sel, DWORD *offset)
*/
ASFUNC(GetProtModeExceptionVector32)
BEGIN
	pushl	%ebx
	movb	PAR1, %bl	/* exception no */
	DPMI	($0x210)
	CHKERR
	movl	PAR2, %ebx	/* *sel */
	movl	%ecx, (%ebx)
	movl	PAR3, %ebx	/* *off */
	movl	%edx, (%ebx)
	OKEAX0
	popl	%ebx
END

/*
int GetRealModeExceptionVector32(BYTE no, UINT *sel, DWORD *offset)
*/
ASFUNC(GetRealModeExceptionVector32)
BEGIN
	pushl	%ebx
	movb	PAR1, %bl	/* exception no */
	DPMI	($0x211)
	CHKERR
	movl	PAR2, %ebx	/* *sel */
	movl	%ecx, (%ebx)
	movl	PAR3, %ebx	/* *off */
	movl	%edx, (%ebx)
	OKEAX0
	popl	%ebx
END

/*
int SetProtModeExceptionVector32(BYTE no, UINT sel, DWORD offset)
*/
ASFUNC(SetProtModeExceptionVector32)
BEGIN
	movb	PAR1, %bl	/* exception no */
	movw	PAR2, %cx	/* sel */
	movl	PAR3, %edx	/* off */
	DPMI	($0x212)
	CHKERR
	OKEAX0
END

/*
int SetRealModeExceptionVector32(BYTE no, UINT sel, DWORD offset)
*/
ASFUNC(SetRealModeExceptionVector32)
BEGIN
	movb	PAR1, %bl	/* exception no */
	movw	PAR2, %cx	/* sel */
	movl	PAR3, %edx	/* off */
	DPMI	($0x213)
	CHKERR
	OKEAX0
END

/*
** typedef struct {
**    WORD    bits;
**    WORD    res1;
**    WORD    res2;
**    } DPMICAP ;
**
** int GetDPMICapabilities(DPMICAP *cap, BYTE *buffer)
*/
ASFUNC(GetDPMICapabilities)
BEGIN
	pushl	%ebx
	pushl	%edi
	movl	PAR2, %edi	/* buffer */
	DPMI	($0x401)
	CHKERR
	movl	PAR1, %ebx	/* cap */
	movw	%ax, (%ebx)
	movw	%cx, 2(%ebx)
	movw	%dx, 4(%ebx)
	OKEAX0
	popl	%edi
	popl	%ebx
END

/*
int AllocLinearMemory(DWORD bytes,DWORD linaddress,DWORD flags,DWORD *handle,DWORD *memaddress)
*/
ASFUNC(AllocLinearMemory)
BEGIN
	pushl	%ebx
	pushl	%esi
	movl	PAR1, %ecx	/* size */
	movl	PAR2, %ebx	/* linaddress */
	movl	PAR3, %edx	/* flags */
	DPMI	($0x504)
	CHKERR
	movl	PAR4, %edx	/* *handle */
	movl	%esi, (%edx)
	movl	PAR5, %edx	/* *memaddress */
	movl	%ebx, (%edx)
	OKEAX0
	popl	%esi
	popl	%ebx
END

/*
int ResizeLinearMemory(DWORD bytes,DWORD handle,DWORD flags,DWORD *newhandle,DWORD *newmemaddress)
*/
ASFUNC(ResizeLinearMemory)
BEGIN
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	PAR1, %ecx	/* new size */
	movl	PAR2, %esi	/* handle */
	movl	PAR3, %edx	/* flags */
	xorl	%edi, %edi
	DPMI	($0x505)
	CHKERR
	movl	PAR4, %edx	/* *new handle */
	movl	%esi, (%edx)
	movl	PAR5, %edx	/* *new memaddress */
	movl	%ebx, (%edx)
	OKEAX0
	popl	%edi
	popl	%esi
	popl	%ebx
END

/*
int GetPageAttributes(DWORD handle,DWORD offs,DWORD pages,WORD *attr)
*/
ASFUNC(GetPageAttributes)
BEGIN
	pushl	%ebx
	pushl	%esi
	movl	PAR1, %esi	/* handle */
	movl	PAR2, %ebx	/* offs */
	movl	PAR3, %ecx	/* pages */
	movl	PAR4, %edx	/* pages */
	DPMI	($0x506)
	CHKERR
	OKEAX0
	popl	%esi
	popl	%ebx
END

/*
int ModifyPageAttributes(DWORD handle,DWORD offs,DWORD pages,WORD *attr)
*/
ASFUNC(ModifyPageAttributes)
BEGIN
	pushl	%ebx
	pushl	%esi
	movl	PAR1, %esi	/* handle */
	movl	PAR2, %ebx	/* offs */
	movl	PAR3, %ecx	/* pages */
	movl	PAR4, %edx	/* pages */
	DPMI	($0x507)
	CHKERR
	OKEAX0
	popl	%esi
	popl	%ebx
END

/*
int MapDeviceInMemoryBlock(DWORD handle,DWORD offs,DWORD pages,DWORD device)
*/
ASFUNC(MapDeviceInMemoryBlock)
BEGIN
	pushl	%ebx
	pushl	%esi
	movl	PAR1, %esi	/* handle */
	movl	PAR2, %ebx	/* offs */
	movl	PAR3, %ecx	/* pages */
	movl	PAR4, %edx	/* pages */
	DPMI	($0x508)
	CHKERR
	OKEAX0
	popl	%esi
	popl	%ebx
END

/*
int MapDOSMemInMemoryBlock(DWORD handle,DWORD offs,DWORD pages,DWORD dosmem)
*/
ASFUNC(MapDOSMemInMemoryBlock)
BEGIN
	pushl	%ebx
	pushl	%esi
	movl	PAR1, %esi	/* handle */
	movl	PAR2, %ebx	/* offs */
	movl	PAR3, %ecx	/* pages */
	movl	PAR4, %edx	/* pages */
	DPMI	($0x509)
	CHKERR
	OKEAX0
	popl	%esi
	popl	%ebx
END

/*
int GetMemoryBlockData(DWORD handle,DWORD *addr,DWORD *bytes)
*/
ASFUNC(GetMemoryBlockData)
BEGIN
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	movl	PAR1, %esi	/* handle high */
	movl	%esi, %edi	/* handle low */
	shrl	$16, %esi
	DPMI	($0x50A)
	CHKERR
	movl	PAR2, %edx	/* addr */
	movw	%si, 2(%edx)
	movw	%di, (%edx)
	movl	PAR3, %edx	/* bytes */
	movw	%bx, 2(%edx)
	movw	%cx, (%edx)
	OKEAX0
	popl	%esi
	popl	%edi
	popl	%ebx
END


/*
int GetMemoryInfo(DPMI10INFO *info)
*/
ASFUNC(GetMemoryInfo)
BEGIN
	pushl	%edi
	movl	PAR1, %edi	/* info  */
	DPMI	($0x50B)
	CHKERR
	OKEAX0
	popl	%edi
END

/*
int FreePhysicalMapping(DWORD address)
*/
ASFUNC(FreePhysicalMapping)
BEGIN
	pushl	%ebx
	movl	PAR1, %ebx	/* addr high */
	movl	%ebx, %ecx	/* addr low */
	shrl	$16, %ecx
	DPMI	($0x801)
	CHKERR
	OKEAX0
	popl	%ebx
END

/*
int DpmiGetCoproStatus(UINT *status)
*/
ASFUNC(DpmiGetCoproStatus)
BEGIN
	pushl	%ebx
	DPMI	($0xE00)
	CHKERR
	movzwl	%ax, %eax
	movl	PAR1, %edx	/* addr high */
	movl	%eax, (%edx)
	OKEAX0
	popl	%ebx
END

/*
int DpmiSetCoproStatus(UINT *status)
*/
ASFUNC(DpmiSetCoproStatus)
BEGIN
	pushl	%ebx
	movl	PAR1, %ebx	/* fpu bits */
	DPMI	($0xE01)
	CHKERR
	OKEAX0
	popl	%ebx
END
