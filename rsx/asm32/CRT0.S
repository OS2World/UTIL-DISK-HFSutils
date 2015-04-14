	.text

	.globl	__text
	.globl	___main

__text:
	call	__entry1
	call	_main
	addl	$12,%esp

	movw	$0x4c00,%ax
	int	$0x21
	ret

___main:
	ret

__entry1:
	popl	%esi
	cld
	xorl	%ebp, %ebp	/* End of stack frames		*/
	leal	(%esp), %edi	/* argv[]			*/
	movl	%edi, _environ
	pushl	%edi		/* envp 			*/
	call	L_ptr_tbl
	pushl	%edi		/* argv 			*/
	call	L_ptr_tbl
	pushl	%ecx		/* argc 			*/
	jmp	*%esi

L_ptr_tbl:
	xorl	%eax, %eax
	movl	$-1, %ecx
1:	incl	%ecx
	scasl
	jne	1b
	ret


	.data

	.globl	__data
	.globl	_rsx_data_start
	.globl	_rsx_data_ends
	.globl	_environ

__data:

	.long	__text		/* text begin		*/
	.long	__etext 	/* text end		*/
	.long	__data		/* data begin		*/
	.long	__edata 	/* data end		*/
	.long	__edata 	/* bss begin		*/
	.long	__end		/* bss end		*/

_rsx_data_start:
	.long	__data

_rsx_data_ends:
	.long	__end

_environ:
	.long	0

