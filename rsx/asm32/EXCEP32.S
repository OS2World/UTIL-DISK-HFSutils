/*****************************************************************************
 * FILE: excep32.s							     *
 *									     *
 * DESC:								     *
 *	- Exception handler entry					     *
 *	- Ints that came from real-mode 				     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "REGS386.INC"

.data

	.globl _time_tic
_time_tic:
	.long 0

	.globl _cbrkcall
_cbrkcall:
	.long 0

dpmiss:
	.long 0
dpmiesp:
	.long 0


.text

	.align 2,144
	.globl _extender_ds
_extender_ds:
	.long 0 			// DataSegment for ExceptionHandler

	.align 2,144
	.globl _load_ds
_load_ds:				// load our DataSegment
	pushl	%cs:_extender_ds
	popl	%ds
	ret


// this proc is called after exception handler return from DPMI-server
// cs:eip and ss:esp set
	.align 2,144
exception_after_return:
	movw	%ss, %ax		// set segment registers
	movw	%ax, %ds
	movw	%ax, %es

	call	_myexcep13		// call C-handler

	jmp	_back_from_syscall


/***
// EXCEPTIONS
//
//  DPMI-rules:
// - return with far return , org SS:ESP,CS:EIP,EFLAGS(i-flag!) will restored
//   (note: iret doesn't restore i-flag, because IOPL < DPL )
// - all fault have error code (only valid for 08,0A-0E)
// - handler must preserve and restore all registers
// - handler will be called on a locked stack with interrupts disabled
// - handler must return or jump to the next handler
// - handler can modify exception stack, but not return cs,eip
//   it must return to the orginal handler
// - called only for protected mode exceptions
//
//	  exception STACK
// EBP	   0
// EIP	   4
// CS	   8
// ERROR   12
// EIP	   16
// CS	   20
// EFLAGS  24
// ESP	   28
// SS	   32
***/

	.align 2,144
	.globl _excep13_386
_excep13_386:
	pushl	$13
	jmp	1f

	.align 2,144
	.globl _excep14_386
_excep14_386:
	pushl	$14
	jmp	1f

	.align 2,144
	.globl _excep0_386
_excep0_386:
	pushl	$0
	jmp	1f

	.align 2,144
	.globl _excep1_386
_excep1_386:
	pushl	$1
	jmp	1f

	.align 2,144
	.globl _excep2_386
_excep2_386:
	pushl	$2
	jmp	1f

	.align 2,144
	.globl _excep3_386
_excep3_386:
	pushl	$3
	jmp	1f

	.align 2,144
	.globl _excep4_386
_excep4_386:
	pushl	$4
	jmp	1f

	.align 2,144
	.globl _excep5_386
_excep5_386:
	pushl	$5
	jmp	1f

	.align 2,144
	.globl _excep6_386
_excep6_386:
	pushl	$6
	jmp	1f

	.align 2,144
	.globl _excep7_386
_excep7_386:
	pushl	$7
	jmp	1f

	.align 2,144
	.globl _excep8_386
_excep8_386:
	pushl	$8
	jmp	1f

	.align 2,144
	.globl _excep9_386
_excep9_386:
	pushl	$9
	jmp	1f

	.align 2,144
	.globl _excep10_386
_excep10_386:
	pushl	$10
	jmp	1f

	.align 2,144
	.globl _excep11_386
_excep11_386:
	pushl	$11
	jmp	1f

	.align 2,144
	.globl _excep12_386
_excep12_386:
	pushl	$12
	jmp	1f

	.align 2,144
	.globl _excep15_386
_excep15_386:
	pushl	$15
	jmp	1f

	.align 2,144
	.globl _excep16_386
_excep16_386:
	pushl	$16
	jmp	1f

	.align 2,144
	.globl _excep17_386
_excep17_386:
	pushl	$17
	jmp	1f

	.align 2,144
1:
exceptionhandler:
	pushl	%ds
	call	_load_ds

	movl	%eax, REGF_EAX
	movl	%ebx, REGF_EBX
	movl	%ecx, REGF_ECX
	movl	%edx, REGF_EDX
	movl	%ebp, REGF_EBP
	movl	%edi, REGF_EDI
	movl	%esi, REGF_ESI
	xorl	%eax, %eax
	movw	%es, %ax
	movw	%eax, REGF_ES
	movw	%fs, %ax
	movw	%eax, REGF_FS
	movw	%gs, %ax
	movw	%eax, REGF_GS

	popl	%eax			// get orginal ds
	movzwl	%ax, %eax
	movl	%eax, REGF_DS
	popl	%eax			// get fault number
	movl	%eax, REGF_FAULTNO

	// the rest regs are on stack
	pushl	%ebp
	movl	%esp, %ebp

	// get error code from stack
	movl	12(%ebp), %eax
	movl	%eax, REGF_ERR

	// get eip from stack
	movl	16(%ebp), %eax
	movl	%eax, REGF_EIP

	// get cs from stack
	movl	20(%ebp), %eax
	movl	%eax, REGF_CS

	// get eflags from stack
	movl	24(%ebp), %eax
	andl	$0xFFFFFEFF, %eax		// clear trace flag
	orl	$0x200, %eax			// set iret flag
	movl	%eax, REGF_EFLAGS		// else trap after fret
	movl	%eax, 24(%ebp)			// back on dpmi-stack

	// get %esp from stack
	movl	28(%ebp), %eax
	movl	%eax, REGF_ESP
	movl	%eax, REGF_ESPORG

	// get ss from stack
	movl	32(%ebp), %eax
	movl	%eax, REGF_SS


	// set new return address cs:eip
	// to exception_after_return

	movl	$exception_after_return, %eax
	movl	%eax, 16(%ebp)				// set new eip

	movw	%cs, %ax				// bzw _code16sel
	movw	%ax, 20(%ebp)				// set cs


	// set new ss:%esp
	// to our C gp_fault handler

	movl	_npz, %esi
	movl	R_KSTACK(%esi), %eax
	movl	%eax, 28(%ebp)				// set %esp

	movw	%ds, %ax				// bzw _stack16sel
	movw	%ax, 32(%ebp)				// set ss

	popl	%ebp

	// restore changed regs
	movl	REGF_ESI, %esi
	movl	REGF_EAX, %eax
	pushl	REGF_DS
	popl	%ds

	lret


/
// EXCEPTIONS DPMI 1.0
//
// extended info at esp + 32
//	   32 : ebp  (our,not default)
//	   36 : return eip (to host)
//	   40 : return cs  (to host)
//	   44 : error code
//	   48 : EIP (orginal)
//	   52 : CS (orginal)
//	   56 : EFLAGS
//	   60 : ESP
//	   64 : SS
//	   68 : DS
//	   72 : ES
//	   76 : FS
//	   80 : GS
//	   84 : CR2
//	   88 : PTE

//
// only page fault (exception 14) use this
//
	.align 2,144
	.globl _page_fault
_page_fault:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ds
	pushl	%es
	pusha

	call	_load_ds
	movl	44(%ebp), %eax				 // error code
	movl	%eax, REG_INFO_ERR
	movl	48(%ebp), %eax				 // EIP
	movl	%eax, REG_INFO_EIP
	movzwl	52(%ebp), %eax				 // CS
	movl	%eax, REG_INFO_CS
	movl	84(%ebp), %eax				 // CR2
	movl	%eax, REG_INFO_CR2
	movl	88(%ebp), %eax				 // PTE
	movl	%eax, REG_INFO_PTE

	movw	%ss, dpmiss
	movl	%esp, dpmiesp
	movw	%ds, %ax
	movw	%ax, %es
	movw	%ax, %ss
	movl	_stackp16, %esp
	movl	$0x0901, %ax				// enable ints
	int	$0x31

	call	_swapper				// call swapper
	orw	%ax, %ax				// uncommit fault?
	jz	swapper_return				// yes, return

	movw	dpmiss, %ss
	movl	dpmiesp, %esp
	popa
	nop
	popl	%es
	popl	%ds
	popl	%ebp
	pushl	$14					// no, generic fault
	jmp	exceptionhandler

	.align 2,144
swapper_return:
	movw	dpmiss, %ss
	movl	dpmiesp, %esp
	popa
	nop
	popl	%es
	popl	%ds
	popl	%ebp
	lret

/*
// INTs that came form real mode (timer 0x1C,^C 0x23,crit error 0x24)
//
//  DPMI-rules:
// - handler must return ( don't terminate )
// - interrupts are disabled (?)
// - stack: locked protected mode stack from host (4 KB)
*/

// Control-C handler
	.align 2,144
	.globl _prot_cbrk
_prot_cbrk:
	pushl	%ds
	pushl	%esi
	call	_load_ds
	movl	$1, _cbrkcall
	movl	_npz, %esi			// load process ptr
	bts	$1, R_SIG_RAISED(%esi)		// set SIGINT
	popl	%esi
	popl	%ds

//      pushl   %eax
//      movw    $0x0901, %ax
//      int     $0x31
//      popl    %eax

        sti
	iret


// Timer handler
	.align 2,144
	.globl _timer_handler
_timer_handler:
	pushl	%ds
	call	_load_ds
	addl	$1, _time_tic
	popl	%ds

        //sti
        //ret

       .byte   0xEA                    // jmp to original handler

       .globl _int1Cvoff
_int1Cvoff:
       .long   1
       .globl _int1Cvsel
_int1Cvsel:
       .long   0

	iret
