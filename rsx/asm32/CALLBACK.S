/*****************************************************************************
 * FILE: CALLBACK.S							     *
 *									     *
 * DESC:								     *
 *	- int 0x21 callback						     *
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
int	AllocRMcallAddress (UINT, UINT, TRANSLATION *, UINT *, UINT *);
*/
ASFUNC(AllocRMcallAddress)
BEGIN
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	PAR3, %edi	/* es:edi = translation */
	movl	PAR2, %esi	/* ds:esi = address of function */
	movl	PAR1, %eax
	pushl	%ds
	movw	%ax, %ds
	DPMI	($0x303)
	popl	%ds
	CHKERR
	movl	PAR4, %ebx	/* return CX:DX real mode */
	movzwl	%cx, %ecx
	movl	%ecx, (%ebx)
	movl	PAR5, %ebx
	movzwl	%dx, %edx
	movl	%edx, (%ebx)
	OKEAX0
	popl	%edi
	popl	%esi
	popl	%ebx
END

/*
int	FreeRMcallAddress (UINT, UINT);
*/
ASFUNC(FreeRMcallAddress)
BEGIN
	movl	PAR1, %ecx
	movl	PAR2, %edx
	DPMI	($0x304)
	CHKERR
	OKEAX0
END

/*
** typedef struct {
**	DWORD edi,esi,ebp,reserved,ebx,edx,ecx,eax ;
**	WORD  flags,es,ds,fs,gs,ip,cs,sp,ss ;
** } TRANSLATION ;
*/

#define TR_FL 32
#define TR_IP 42
#define TR_CS 44
#define TR_SP 46


/*
** CALLBACK HANDLER:
** - interrupt disabled
** - ds:esi = sel/off of realmode stack
** - es:edi = sel/off of translation struct
** - ss:esp = locked dpmi stack
*/


ASFUNC(rm_handler)
	pushl	%ds			/* save realmode stack */
	pushl	%esi

	call	_load_ds
	movw	%ds, %ax
	movw	%ax, %es

	/*
	movw	%es, %ax
	movw	%ax, %ds
	*/

	movw	%ss, dpmi_ss		/* save dpmi stack */
	movl	%esp, dpmi_esp

	movw	%ax, %ss		/* change stack */
	movl	_stackp16, %esp

ALIGN_TEXT
call_our_handler:
	pushl	%edi
	call	_int21_realmode_callback
	popl	%edi

	movw	dpmi_ss, %ss		/* restore stack */
	movl	dpmi_esp, %esp

	or	%eax, %eax
	jnz	chain_to_dos

return_from_int21: /* put values before int21 in struct */
	popl	%esi
	popl	%ds			/* ds:edi = realmode stack */
	cld
	lodsw				/* ip */
	movw	%ax, %es:TR_IP(%edi)
	lodsw				/* cs */
	movw	%ax, %es:TR_CS(%edi)
	lodsw				/* do not use old flag */

	orw	$512, %es:TR_FL(%edi)	/* tr.flag + IF enable */
	addw	$6, %es:TR_SP(%edi)	/* pop off rm stack */
	iret

ALIGN_TEXT
chain_to_dos:
	movl	_rm21_seg, %eax 	/* old handler segment */
	movw	%ax, %es:TR_CS(%edi)
	movl	_rm21_off, %eax 	/* old handler offset */
	movw	%ax, %es:TR_IP(%edi)
	popl	%esi
	popl	%ds
	iret

.data

	.globl _rm21_seg
_rm21_seg:
	.long	0

	.globl _rm21_off
_rm21_off:
	.long	0

dpmi_ss:
	.long	0
dpmi_esp:
	.long	0
