    .SEQ

HDR_SEG segment para use16 public 'HEADER'
HDR_SEG ends

_TEXT	segment para use16 public 'CODE'
_TEXT	ends

_CONST	segment para public 'CONST'
_CONST	ends

_BSS	segment para public 'BSS'
_BSS	ends

_DATA	segment para use16 public 'DATA'
_DATA	ends

_STACK	segment para use16 STACK 'STACK'
_STACK	ends

DGROUP group  _CONST, _BSS, _DATA, _STACK

    .8086

HDR_SEG segment
	db	"rsx (c) Rainer Schnitker '93 '94"
HDR_SEG ends

_STACK_SIZE = 2048		; Default stack size = 2K

_STACK	segment
	db	_STACK_SIZE dup (?)    ; stack space
	stack_top label word
_STACK	ends

_DATA	segment
	public __psp
	__psp  dw  ?
	public __acrtused
	__acrtused  dw	9876h
_DATA	ends

; *******************************************************

extrn _main:near

_TEXT	segment
assume cs:_TEXT
assume ds:DGROUP
assume es:DGROUP
assume ss:DGROUP

ENTRY:
	mov	dx, es			; save psp
	mov	ax, DGROUP		; make ds=es=ss
	mov	ds, ax
	mov	es, ax
	mov	ss, ax			; reload stack
	mov	sp, offset DGROUP:stack_top

	mov	__psp, dx		; store psp
	call	_main

	mov	ah, 04CH
	int	021H

_TEXT ends
	end	ENTRY
