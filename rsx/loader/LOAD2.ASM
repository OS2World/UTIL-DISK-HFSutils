	.286
	.model SMALL, C

; macro to call dpmi in ax
;
DPMI MACRO function
	mov ax,function
	int 31h
	ENDM

; error check for dpmi-functions
; return -1 for dpmi 0.9
; for dpmi 1.0 replace with "jc @@end" , error code = ax
;
CHECKERR MACRO
	jnc	short @@ok
	mov	ax, -1
	jmp	short @@end
	ENDM

getdword MACRO high,low,address
	mov low,word ptr address
        mov high,word ptr address+2
	ENDM

setdword MACRO address,high,low
	mov word ptr address,low
        mov word ptr address+2,high
	ENDM


	.data

	extrn data32sel:DWORD
	extrn code32sel:DWORD
	extrn stackp32:DWORD

	havei387    dw	    0

	.code

;
; **************** DPMI FUNCTIONS ********************************
;

   ;
   ;	int AllocLDT(WORD anzahl,WORD *sel)
   ;
	public	C AllocLDT
AllocLDT PROC C \
	anzahl	:WORD, \
	sel	:PTR

	mov	cx, anzahl
	DPMI 0000h
	CHECKERR
@@ok:
	mov	bx, sel
	mov	word ptr [bx], ax
	xor	ax, ax
@@end:
	ret
AllocLDT	endp

   ;
   ;	int FreeLDT(WORD sel)
   ;
	public	C FreeLDT
FreeLDT PROC C \
	sel	:WORD

	mov	bx, sel
	DPMI 0001h
	CHECKERR
@@ok:
	xor	ax, ax
@@end:
	ret
FreeLDT 	endp

   ;
   ;	WORD SelInc(void)
   ;
	public	C SelInc
SelInc PROC C
	DPMI 0003h
	ret
SelInc		endp

   ;
   ;	int SetBaseAddress(WORD sel,DWORD address)
   ;
	public	C SetBaseAddress
SetBaseAddress PROC C \
	sel	:WORD , \
        basesel :DWORD

        getdword cx,dx,basesel
	mov	bx, sel
	DPMI 0007h
	CHECKERR
@@ok:
	xor	ax, ax
@@end:
	ret
SetBaseAddress endp

   ;
   ;	int SetLimit(WORD sel,DWORD limit)
   ;
	public	C SetLimit
SetLimit PROC C \
	sel	:WORD , \
	limit	:DWORD

	mov	bx,sel
	getdword cx,dx,limit
	DPMI 0008h
	CHECKERR
@@ok:
	xor	ax, ax
@@end:
	ret
SetLimit       endp

   ;
   ;	int SetAccess(WORD sel,BYTE access,BYTE extaccess)
   ;
	public	C SetAccess
SetAccess PROC C \
	sel	:WORD, \
	access	:BYTE, \
	extaccess :BYTE

	mov	bx,sel
	mov	cl,access
	mov	ch,extaccess
	DPMI 0009h
	CHECKERR
@@ok:
	xor	ax, ax
@@end:
	ret
SetAccess      endp

	public C GetCS
GetCS PROC C
	mov	ax,cs
	ret
GetCS endp

	public C GetDS
GetDS PROC C
	mov	ax,ds
	ret
GetDS endp

	public C GetDpmiEntryPoint
GetDpmiEntryPoint PROC C USES ES SI DI , \
	entry	:PTR, \
	dpmipara:PTR, \
	flags	:PTR, \
	dpmiver :PTR, \
	cpu	:PTR

	mov	ax, 1687h
	int	2Fh
	or	ax,ax
	jnz	noDPMI
	mov	ax, bx		; save bx = bits

	mov	bx, entry
	mov	[bx], di
	mov	[bx+2], es
	mov	bx, flags
	mov	[bx], ax
	mov	bx, dpmiver
	mov	[bx], dx
	mov	bx, cpu
	mov	[bx], cl
	mov	bx, dpmipara
	mov	[bx], si
	xor	ax, ax
noDPMI:
	ret
GetDpmiEntryPoint endp

	public C GetDpmiHostParagraph
GetDpmiHostParagraph PROC C \
	dpmipara:WORD

	mov	bx, dpmipara
	mov	ah, 48h
	int	21h
	jnc	@@end
	or	ax,ax
@@end:
	ret
GetDpmiHostParagraph endp

	public C DpmiEnterProtectedMode
DpmiEnterProtectedMode PROC C \
	entry	:DWORD, \
	flags	:WORD, \
	parasegm:WORD

	mov	ax, parasegm
	mov	es, ax
	mov	ax, flags
	call	dword ptr entry
	jnc	@@ok
	mov	ax, -1
	jmp	short @@end
@@ok:
	xor	ax, ax
@@end:
    .386
	movzx	ebp, bp
	movzx	esp, sp
    .286
	ret
DpmiEnterProtectedMode endp

        public C DosReallocParagraph
DosReallocParagraph PROC C USES ES, \
	oldsegm:WORD, \
	newpara:WORD

	mov	es, oldsegm
	mov	bx, newpara
	mov	ah, 4Ah
	int	21h
	jc	@@end
	or	ax,ax
@@end:
	ret
DosReallocParagraph endp

	public	C LockLinRegion
LockLinRegion PROC C USES SI DI, \
	sizeb	:DWORD , \
        address :DWORD

        getdword bx,cx,address
	getdword si,di,sizeb
	DPMI 0600h
	CHECKERR
@@ok:
	xor	ax, ax
@@end:
	ret
LockLinRegion  endp

;
; **************** DOS FUNCTIONS ********************************
;

   ;
   ;	void dos_puts(char *)
   ;
	public	C dos_puts
dos_puts PROC C \
	buffer	:PTR

	mov	dx, buffer
	mov	ah, 09h
	int	021h
	ret
dos_puts endp


   ;
   ;	int dos_open(char * name, int mode)
   ;
	public	C dos_open
dos_open PROC C \
	buffer	:PTR , \
	mode	:BYTE

	mov	dx, buffer
	mov	al, mode
	mov	ah, 3Dh
	int	021h
	jnc	@@end
	mov	ax, -1
@@end:
	ret
dos_open endp


   ;
   ;	int dos_close(int handle)
   ;
	public	C dos_close
dos_close PROC C \
	handle	:WORD

	mov	bx, handle
	mov	ah, 3Eh
	int	021h
	jnc	@@end
	mov	ax, -1
@@end:
	ret
dos_close endp


   ;
   ;	int dos_read(int handle, void * buffer, int bytes)
   ;
	public	C dos_read
dos_read PROC C \
	handle	:WORD, \
	buffer	:PTR, \
	bytes	:WORD

	mov	bx, handle
	mov	cx, bytes
	mov	dx, buffer
	mov	ah, 03Fh
	int	021h
	jnc	@@end
	mov	ax,-1
@@end:
	ret
dos_read endp


   ;
   ;	int dos_read_far(int handle, far *buffer, int bytes)
   ;
	public	C dos_read_far
dos_read_far PROC C USES DS, \
	handle	:WORD, \
	buffer	:FAR PTR, \
	bytes	:WORD

	mov	bx, handle
	mov	cx, bytes
	getdword ds,dx,buffer
	mov	ah, 03Fh
	int	021h
	jnc	@@end
	mov	ax,-1
@@end:
	ret
dos_read_far endp


   ;
   ;	int dos_lseek(int handle, long pos, int orgin)
   ;
	public	C dos_lseek
dos_lseek PROC C \
	handle	:WORD, \
	pos	:DWORD, \
	orgin	:WORD

	mov	bx, handle
	getdword cx,dx,pos
	mov	al, byte ptr orgin
	mov	ah, 42h
	int	021h
	jnc	@@end
	mov	ax, -1
	mov	dx, -1
@@end:
	ret
dos_lseek endp


   ;
   ;	int far_memcpy(void far * dst, void far *src, int bytes)
   ;
	public	C far_memcpy
far_memcpy PROC C USES DS ES SI DI, \
	dst	:FAR PTR, \
	src	:FAR PTR, \
	bytes	:WORD

	les	di, dst
	lds	si, src
	mov	cx, bytes
	shr	cx, 1
	cld
        rep     movsw
	jnb	@@end
	movsb
@@end:
	ret
far_memcpy endp


   ;
   ;	int far_bzero(void far * p, int bytes)
   ;
	public	C far_bzero
far_bzero PROC C USES ES DI, \
	dst	:FAR PTR, \
	bytes	:WORD

	les	di, dst
	mov	cx, bytes
	shr	cx, 1
	xor	ax, ax
	cld
        rep     stosw
	jnb	@@end
	stosb
@@end:
	ret
far_bzero endp



	public C jmp_to_user
jmp_to_user PROC C
	mov	ax, word ptr data32sel
	.386p
	mov	ebx, dword ptr stackp32
	movzx	ecx, word ptr code32sel
	mov	edx, dword ptr 10000h

	mov	ss, ax
	mov	esp, ebx

	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ds, ax

	push	ecx
	push	edx

	db 066H 				; we need 16:32 bit return
	retf

	.186

	nop
	ret
jmp_to_user endp

	public C npx_installed
npx_installed PROC C USES SI,

	fninit					; set SW,CW,..
	mov	si, offset havei387
	mov	word ptr [si],5a5ah
	fnstsw	[si]				; save SW
	cmp	byte ptr [si],0 		; SW==0 ?
	jne	no_i387

	fnstcw	[si]				; save CW
	mov	ax,[si] 			; check
	and	ax,103fh			;
	cmp	ax,3fh				; init ?
	jne	no_i387

	fld1					; check for 387
	fldz
	fdiv
	fld	st
	fchs
	fcompp
	fstsw	[si]
	mov	ax,[si]
	sahf
	je	no_i387
	fninit					; 387 ok ,init
	fnstcw	havei387
	wait
	and	havei387,0fffah
	fldcw	havei387
	mov	ax,1
	jmp	short ende
no_i387:
	mov	ax,0
ende:
	ret
npx_installed endp

	end
