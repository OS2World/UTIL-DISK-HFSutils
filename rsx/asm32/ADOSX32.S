/*****************************************************************************
 * FILE: adosx32.S							     *
 *									     *
 * DESC:								     *
 *	- asm DOS 0x21 handler						     *
 *	- debug handler for Windows 3.1 				     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "REGS386.INC"

.text

	.globl _printk
	.align 2,144
_printk:
	jmp	_printf


/***************************************/

// Entry point for int 0x10

	.globl _bioscall
	.align 2,144
_bioscall:				// INT 0x10 handler entry
	cmpb	$0x4F, %ah		// vesa call ?
	jne	not_vesa
	pushl	$0x10
	jmp	common_int

not_vesa:				// jmp to orginal handler (DPMI-host)
	.byte	 0xEA

	.globl _int10voff		// label for writing in code
_int10voff:
	.long	 1
	.globl _int10vsel		// label for writing in code
_int10vsel:
	.long	 0

/***************************************/

// Entry point for int 0x33

	.globl _mousecall
	.align 2,144
_mousecall:				// INT 0x33 handler entry
	pushl	$0x33
	jmp	common_int


/***************************************/

common_int:	// we do not save fs & gs
	pushl	%esi
	call	_load_ds			// load 16 bit extender ds
	movl	_npz, %esi			// load process ptr
	popl	R_ESI(%esi)			// save ESI
	popl	R_FAULTNO(%esi) 		// save int no

	movl	%eax, R_EAX(%esi)
	movl	%ebx, R_EBX(%esi)
	movl	%ecx, R_ECX(%esi)
	movl	%edx, R_EDX(%esi)
	movl	%edi, R_EDI(%esi)
	movl	%ebp, R_EBP(%esi)
	movw	%es, %ax
	movw	%ax, R_DS(%esi)
	movw	%ax, R_ES(%esi)
	movw	%ss, %ax
	movw	%ax, R_SS(%esi)
	movl	%esp, R_ESP(%esi)
	movl	%esp, R_ESPORG(%esi)
	addl	$12, R_ESPORG(%esi)

	movl	%esp, %ebp			// others regs are on user stack
	movl	(%ebp), %eax			// EIP
	movl	%eax, R_EIP(%esi)
	movl	4(%ebp), %eax			// CS
	movl	%eax, R_CS(%esi)
	movl	8(%ebp), %eax			// EFLAGS
	movl	%eax, R_EFLAGS(%esi)

	movw	%ds, %ax
	movw	%ax, %es
	movw	%ax, %ss
	movl	R_KSTACK(%esi), %esp		// load 16bit sp

	call	_prot_mode_interrupt

	movl	_npz, %esi			// load process ptr
	movl	R_EBX(%esi), %ebx		// EBX
	movl	R_ECX(%esi), %ecx		// ECX
	movl	R_EDX(%esi), %edx		// EDX
	movl	R_EDI(%esi), %edi		// EDI
	movl	R_EBP(%esi), %ebp		// EBP
	movl	R_SS(%esi), %eax
	movw	%ax, %ss
	movl	R_ESP(%esi), %esp
	movw	R_ES(%esi), %ax
	movw	%ax, %es

	movl	R_EFLAGS(%esi), %eax
	movl	%eax, 8(%esp)			// EFLAGS
	movl	R_CS(%esi), %eax
	movl	%eax, 4(%esp)			// CS
	movl	R_EIP(%esi), %eax
	movl	%eax, (%esp)			// EIP

	movl	R_EAX(%esi), %eax		// EAX
	pushl	R_DS(%esi)
	pushl	R_ESI(%esi)
	popl	%esi
	popl	%ds				// last segment restore
	iret


/***************************************/


// Entry point for int 0x21

	.globl _doscall
	.align 2,144
_doscall:				// INT 0x21 handler entry
	cmpb	$0x4C, %ah		// exit call ?
	jne	int32bit		// no, must 32bit user int

	// ah=4Ch: recursive extender instance or 32bit user call ?
	// next instance reset interrupt handler and do int21/0x4c00

	pushl	%esi
	pushl	%eax
	pushl	%ds
	call	_load_ds			// load 16 bit extender ds
	movl	_npz, %esi			// load process ptr
	movw	16(%esp), %ax			// 0:esi,eax,ds,eip,12:cs
	cmpw	R_CS(%esi), %ax
	popl	%ds
	popl	%eax
	popl	%esi
	je	int32bit

	.align 2,144
					// jmp to orginal handler (DPMI-host)
jmp_inst:
	.byte	 0xEA

	.globl _int21voff		// label for writing in code
_int21voff:
	.long	 1
	.globl _int21vsel		// label for writing in code
_int21vsel:
	.long	 0

/***************************************/


.align 2,144
int32bit:	  // stack: 0=eip 4=cs 8=eflags

	pushl	%esi
	call	_load_ds			// load 16 bit extender ds
	movl	_npz, %esi			// load process ptr
	popl	R_ESI(%esi)			// save ESI

	movl	%eax, R_EAX(%esi)
	movl	%ebx, R_EBX(%esi)
	movl	%ecx, R_ECX(%esi)
	movl	%edx, R_EDX(%esi)
	movl	%edi, R_EDI(%esi)
	movl	%ebp, R_EBP(%esi)
	movw	%es, %ax
	movw	%ax, R_DS(%esi)
	movw	%ax, R_ES(%esi)
	movw	%ss, %ax
	movw	%ax, R_SS(%esi)
	movw	%fs, %ax
	movw	%ax, R_FS(%esi)
	movw	%gs, %ax
	movw	%ax, R_GS(%esi)
	movl	%esp, R_ESP(%esi)
	movl	%esp, R_ESPORG(%esi)
	addl	$12, R_ESPORG(%esi)

	movl	%esp, %ebp			// others regs are on user stack
	movl	(%ebp), %eax			// EIP
	movl	%eax, R_EIP(%esi)
	movl	4(%ebp), %eax			// CS
	movl	%eax, R_CS(%esi)
	movl	8(%ebp), %eax			// EFLAGS
	movl	%eax, R_EFLAGS(%esi)

	movw	%ds, %ax
	movw	%ax, %es
	movw	%ax, %ss
	movl	R_KSTACK(%esi), %esp		// load 16bit sp

	call	_int21		// call C-prg handler for int21h

.globl _back_from_syscall
_back_from_syscall:
	call	_check_signals
	cld

	movl	_npz, %esi			// load process ptr
	movl	R_EBX(%esi), %ebx		// EBX
	movl	R_ECX(%esi), %ecx		// ECX
	movl	R_EDX(%esi), %edx		// EDX
	movl	R_EDI(%esi), %edi		// EDI
	movl	R_EBP(%esi), %ebp		// EBP
	movl	R_SS(%esi), %eax
	movw	%ax, %ss
	movl	R_ESP(%esi), %esp
	movw	R_ES(%esi), %ax
	movw	%ax, %es
	movw	R_FS(%esi), %ax
	movw	%ax, %fs
	movw	R_GS(%esi), %ax
	movw	%ax, %gs

	movl	R_EFLAGS(%esi), %eax
	movl	%eax, 8(%esp)			// EFLAGS
	movl	R_CS(%esi), %eax
	movl	%eax, 4(%esp)			// CS
	movl	R_EIP(%esi), %eax
	movl	%eax, (%esp)			// EIP

	movl	R_EAX(%esi), %eax		// EAX
	pushl	R_DS(%esi)
	pushl	R_ESI(%esi)
	popl	%esi
	popl	%ds				// last segment restore
	iret


	.align 2,144
	.globl _debug_entry
_debug_entry:					// int 0x03 handler
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
	movw	%ss, %ax
	movw	%eax, REGF_SS
	movw	%fs, %ax
	movw	%eax, REGF_FS
	movw	%gs, %ax
	movw	%eax, REGF_GS
	popl	%eax			// get orginal ds
	movl	%eax, REGF_DS

	movl	%esp, REGF_ESP
	addl	$12, REGF_ESP
	movl	%esp, REGF_ESPORG
	addl	$12, REGF_ESPORG
	movl	$1, REGF_FAULTNO

	movl	%esp, %ebp
	movl	(%ebp), %eax
	movl	%eax, REGF_EIP
	movl	4(%ebp), %eax
	movl	%eax, REGF_CS
	movl	8(%ebp), %eax
	movl	%eax, REGF_EFLAGS

	movl	_npz, %esi
	movw	%ds, %ax		// set other segment registers
	movw	%ax, %es		// make ds = es = ss
	movw	%ax, %ss		// load ss
	movl	R_KSTACK(%esi), %esp	// load esp

	call	_myexcep13		// call C-handler

	jmp	_back_from_syscall

	iret


	.align 2,144
	.globl _execute_dpmi_function
_execute_dpmi_function:
	pusha
	pushl	%ds
	pushl	%es

	movl	_npz, %esi
	movl	R_EAX(%esi), %eax
	movl	R_EBX(%esi), %ebx
	movl	R_ECX(%esi), %ecx
	movl	R_EDX(%esi), %edx
	movl	R_EBP(%esi), %ebp
	movl	R_EDI(%esi), %edi
	pushl	R_DS(%esi)
	pushl	R_ES(%esi)
	movl	R_ESI(%esi), %esi
	popl	%es
	popl	%ds
	int	$0x31

	popl	%es
	popl	%ds
	popa
	nop

	pushf
	popl	%eax
	andl	$1, %eax
	ret
