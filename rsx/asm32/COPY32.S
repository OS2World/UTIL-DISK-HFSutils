.text
	.align 2,0x90
.globl _read32
_read32:
	movl	2*4(%esp), %edx
	movw	1*4(%esp), %ax
	movw	%ax, %fs
	movl	%fs:(%edx), %eax
	ret

	.align 2,0x90
.globl _store32
_store32:
	movw	1*4(%esp), %ax
	movw	%ax, %fs
	movl	2*4(%esp),%edx
	movl	3*4(%esp),%eax
	movl	%eax, %fs:(%edx)
	ret

	.align 2,0x90
.globl _put_user_byte
_put_user_byte:
	movw	1*4(%esp), %ax
	movw	%ax, %fs
	movl	2*4(%esp),%edx
	movb	3*4(%esp),%al
	movb	%al, %fs:(%edx)
	ret

	.align 2,0x90
.globl _bzero32
_bzero32:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%es
	pushl	%edi
	movw	8(%ebp), %ax		/ seg
	movl	12(%ebp), %edi		/ off
	movl	16(%ebp), %ecx		/ bytes
	movw	%ax, %es
	xorl	%eax, %eax
	shrl	$2, %ecx
	rep
	stosl
	movl	16(%ebp), %ecx		/ bytes
	andl	$3, %ecx
	rep
	stosb
	popl	%edi
	popl	%es
	leave
	ret

	.align 2,0x90
.globl _cpy16_32
_cpy16_32:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%es

	movw	8(%ebp),%ax	/ dest es:user
	movw	%ax, %es
	movl	12(%ebp),%edi	/ dest edi:user
	movl	16(%ebp),%esi	/ src ds:esi
	movl	20(%ebp),%ecx
	cld
	testb	$1,%cl
	je 1f
	movsb
1:	testb	$2,%cl
	je 2f
	movsw
2:	shrl	$2,%ecx
	rep ; movsl

	popl	%es
	popl	%esi
	popl	%edi
	leave
	ret


	.align 2,0x90
.globl _cpy32_16
_cpy32_16:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ds

	movw	8(%ebp),%ax	    / src ds:user
	movw	%ax, %ds
	movl	12(%ebp),%esi	    / src esi:user
	movl	16(%ebp),%edi	    / dest es:edi
	movl	20(%ebp),%ecx	    / bytes
	cld
	testb	$1,%cl
	je 1f
	movsb
1:	testb	$2,%cl
	je 2f
	movsw
2:	shrl	$2,%ecx
	rep ; movsl

	popl	%ds
	popl	%esi
	popl	%edi
	leave
	ret

	.align 2,0x90
.globl _strcpy16_32
_strcpy16_32:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%es
	movw	8(%ebp),%ax	   / dest es:user
	movw	%ax, %es
	movl	12(%ebp),%edi	   / dest edi:user
	movl	16(%ebp),%esi	   / src ds:esi

1:	lodsb
	stosb
	orb	%al, %al
	jnz	1b

	popl	%es
	popl	%esi
	popl	%edi
	leave
	ret

	.align 2,0x90
.globl _strcpy32_16
_strcpy32_16:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ds
	movw	8(%ebp),%ax	    / src ds:user
	movw	%ax, %ds
	movl	12(%ebp),%esi	    / src esi:user
	movl	16(%ebp),%edi	    / dest es:edi

1:	lodsb
	stosb
	orb	%al, %al
	jnz	1b

	popl	%ds
	popl	%esi
	popl	%edi
	leave
	ret

	.align 2,0x90
.globl _cpy32_32
_cpy32_32:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ds
	pushl	%es

	movw	8(%ebp),%ax
	movw	%ax, %ds
	movl	12(%ebp),%esi
	movw	16(%ebp), %ax
	movw	%ax, %es
	movl	20(%ebp),%edi
	movl	24(%ebp),%ecx
	cld
	testb	$1,%cl
	je 1f
	movsb
1:	testb	$2,%cl
	je 2f
	movsw
2:	shrl	$2,%ecx
	rep ; movsl

	popl	%es
	popl	%ds
	popl	%esi
	popl	%edi
	leave
	ret

	.align 2,0x90
.globl _getstr32_16
_getstr32_16:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ds

	movw	8(%ebp),%ax	    / src ds:user
	movw	%ax, %ds
	movl	12(%ebp),%esi	    / src esi:user
	movl	16(%ebp),%edi	    / dest es:edi
	movb	20(%ebp),%cl	    / char

1:	lodsb
	stosb
	xorb	%al, %cl
	jnz	1b

	popl %ds
	popl %esi
	popl %edi
	leave
	ret
