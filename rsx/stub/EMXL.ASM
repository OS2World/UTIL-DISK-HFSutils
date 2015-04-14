;
; Modified for hfsutils by Marcus Better, July 1997
;

;
; modified for rsx and rsxnt:
; Copyright (c) 1995-1996 by Rainer Schnitker
;
; 1) search for a DPMI server - load cwsdpmi if not (rename to CSX.EXE!!)
; 2) always use RSX.EXE
; 3) work with Watcom/wasm and Borland/tasm
;

;
; EMXL.ASM -- emx loader (emxl.exe)
;
; Copyright (c) 1991-1996 by Eberhard Mattes
;
; This file is part of emx.
;
; emx is free software; you can redistribute it and/or modify it
; under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2, or (at your option)
; any later version.
;
; emx is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with emx; see the file COPYING.  If not, write to
; the Free Software Foundation, 59 Temple Place - Suite 330,
; Boston, MA 02111-1307, USA.
;
; As a special exception, if you bind emxl.exe to an executable file
; (using emxbind), this does not cause the resulting executable file
; to be covered by the GNU General Public License.  The source code
; for emxl.exe must be distributed with the executable file.  This
; exception does not however invalidate any other reasons why the
; executable file might be covered by the GNU General Public License.
;

		INCLUDE EMX.INC
		INCLUDE HEADERS.INC
		INCLUDE VERSION.INC

		.8086

;
; Data (the name SV_DATA stems from emx, we're using EMX.INC!)
;
SV_DATA 	SEGMENT

PSP_SEG 	DW	?			; Program prefix segment
ENV_SEG 	DW	?			; Environment segment
NAME_OFF	DW	?			; Offset of prog name (ENV_SEG)
NAME_LEN	DW	?			; Length of prog name

;
; Parameter block for INT 21H, AX=4B00H
;
PAR_BLOCK	DW	0			; Copy parent's environment
		DW	OFFSET CMD_LINE 	; Command line
		DW	SEG CMD_LINE
		DW	5CH			; FCB1
PB_SEG1 	DW	?
		DW	6CH			; FCB2
PB_SEG2 	DW	?

;
; DPMI_FLAG is non-zero if a DPMI server has been found, but no
; VCPI server
;
DPMI_FLAG	DB	FALSE			; Initially FALSE

;
; Messages
;
$RSX_NOT_FOUND	DB	"rsxhfs not found", CR, LF, "$"
$USE_EMXBIND	DB	"Use emxbind", CR, LF, "$"
$BAD_ENV	DB	"Bad environment", CR, LF, "$"

;
; Names of environment variables
;
;$RSX		DB	"RSX", 0
$RSX		DB	"RSXHFS", 0
$PATH		DB	"HFSUTILS", 0

;
; Name of rsx.exe for searching in the current working directory and PATH
;
;$RSX_EXE	DB	"rsx.exe", 0
$RSX_EXE	DB	"rsxhfs.exe", 0
RSX_EXE_LEN	=	$-$RSX_EXE

;
; This command line is passed to rsx.exe
;
CMD_LINE	DB	7			; 7 bytes
		DB	"-/"                    ; Special marker
CMD_LINE_PSP	DB	"0000"                  ; PSP_SEG, hexadecimal
		DB	"/"                     ; Another special marker
		DB	CR			; End of command line

;
; Build path name here when using directory from PATH environment variable
;
PGM_NAME	DB	128 DUP (?)
PGM_NAME_END    LABEL   BYTE

SV_DATA 	ENDS


;
; This area will be patched by emxbind
;
HDR_SEG 	SEGMENT
PATCH		LABEL	BIND_HEADER
                DB      "emx ", "0.9c", 0
		DB	(SIZE BIND_HEADER - HDR_VERSION_LEN) DUP (0)
HDR_SEG 	ENDS

;
; Code
;
INIT_CODE	SEGMENT

		ASSUME	CS:INIT_CODE
		ASSUME	DS:NOTHING, ES:NOTHING

;
; Setup data segment.  Note: This macro modifies AX.
;
SET_DS		MACRO
		MOV	AX, SV_DATA
		MOV	DS, AX
		ASSUME	DS:SV_DATA
		ENDM

;
; The program starts here
;
ENTRY:		SET_DS				; Setup data segment
		MOV	PSP_SEG, ES		; Save program prefix segment
		CALL	INIT			; Initialize
		CALL	CHECK_DPMI		; Check for DPMI
		CALL	GET_NAME		; Get program name
; MB: Remove this to enable running ntbind-converted program under RSX.
;		CALL	BIND_HDR		; Check patch area
		CALL	TRY_RSX_ENV		; Use RSX environment variable
		CALL	TRY_CWD 		; Search current directory
		CALL	TRY_PATH		; Use PATH environment variable
		LEA	DX, $RSX_NOT_FOUND	; Give up
FAIL:		JMP	ABORT

;
; Initialization
;
INIT		PROC	NEAR
		MOV	AX, PSP_SEG
		MOV	PB_SEG1, AX		; Fill in parameter block
		MOV	PB_SEG2, AX		; for DOS function 4BH (EXEC)
		MOV	ES, AX
		MOV	AX, ES:[2CH]		; Get environment segment
		MOV	ENV_SEG, AX
		LEA	BX, CMD_LINE_PSP	; Insert PSP_SEG, hexadecimal
		MOV	CX, 0404H		; 4 digits, shift by 4 bits
		MOV	DX, PSP_SEG
INIT_1: 	ROL	DX, CL
		MOV	AL, DL
		AND	AL, 0FH
		ADD	AL, "0"
		CMP	AL, "9"
		JBE	INIT_2
		ADD	AL, "A" - ("0" + 10)
INIT_2: 	MOV	[BX], AL
		INC	BX
		DEC	CH
		JNZ	INIT_1
		RET
INIT		ENDP


;
; Check for DPMI server
;
CHECK_DPMI	PROC	NEAR
		MOV	AX, 1687H		; Check for DPMI server
		INT	2FH
		OR	AX, AX			; Server present?
                JNZ     LOAD_CWSDPMI            ; No -> try cwsdpmi
		TEST	BX, 1			; 32-bit programs supported?
                JZ      LOAD_CWSDPMI            ; No -> try cwsdpmi
                MOV     DPMI_FLAG, NOT FALSE    ; Set flag
                JMP     DPMI_RET

LOAD_CWSDPMI:   MOV     AX, "sc"                ; First 2 letters of "csx.exe"
                MOV     WORD PTR $RSX_EXE, AX   ; Patch name of executable
                LEA     BX, CMD_LINE            ; patch cmdline
                MOV     BYTE PTR [BX+1], CR
		CALL	TRY_CWD 		; Search current directory
                JNC     RETURN_CWSDPMI          ; Success
		CALL	TRY_PATH		; Use PATH environment variable
RETURN_CWSDPMI:
                LEA     BX, CMD_LINE            ; patch cmdline
                MOV     BYTE PTR [BX+1], 2DH
                MOV     AX, "sr"                ; First 2 letters of "rsx.exe"
                MOV     WORD PTR $RSX_EXE, AX   ; Patch name of executable
DPMI_RET:	RET
CHECK_DPMI	ENDP

;
; Find program name
;
GET_NAME	PROC	NEAR
		MOV	ES, ENV_SEG		; Scan environment
		XOR	DI, DI			; starting at offset 0
		MOV	CX, 8000H		; Maximum environment size
		XOR	AL, AL			; Search for bytes of zeros
		CLD				; Incrementing
GET_NAME_1:     REPNE   SCAS BYTE PTR ES:[DI]   ; Skip string
		JNE	GET_NAME_ERR		; Count exhausted -> error
                SCAS    BYTE PTR ES:[DI]        ; End of environment?
		JE	GET_NAME_9		; Yes -> program name found
		LOOP	GET_NAME_1		; Next string, 1st char skipped
GET_NAME_ERR:	LEA	DX, $BAD_ENV		; Bad environment
		JMP	ABORT

GET_NAME_9:	ADD	DI, 2			; Skip count
		MOV	NAME_OFF, DI		; Offset of program name
		CALL	STRLEN			; Compute length
		MOV	NAME_LEN, CX
		RET
GET_NAME	ENDP


;
; Check emxbind patch area
;
BIND_HDR	PROC	NEAR
		MOV	AX, HDR_SEG
		MOV	ES, AX			; Access emxbind patch area
		ASSUME	ES:HDR_SEG
		CMP	PATCH.BND_BIND_FLAG, FALSE ; Bound?
		JNE	BIND_HDR_1		; Yes -> continue
		LEA	DX, $USE_EMXBIND	; Must be bound to a.out
		JMP	ABORT
BIND_HDR_1:	RET
BIND_HDR	ENDP

		ASSUME	ES:NOTHING

;
; Display error message and abort program
;
; In:	DX	Offset of dollar-delimited error message in SV_DATA
;
ABORT:		SET_DS				; This can't hurt
		MOV	AH, 09H 		; Display string
		INT	21H
		MOV	AX, 4C01H		; Terminate process, rc=1
		INT	21H

;
; Try to find rsx.exe using RSX environment variable
;
TRY_RSX_ENV	PROC	NEAR
		LEA	BX, $RSX
		CALL	GETENV
		JC	TRY_RSX_ENV_RET
		MOV	DX, DI
		MOV_DS_ES
		ASSUME	DS:NOTHING
		CALL	SPAWN
		ASSUME	DS:SV_DATA
TRY_RSX_ENV_RET:RET
TRY_RSX_ENV	ENDP

;
; Try to find rsx.exe in current working directory
;
TRY_CWD 	PROC	NEAR
		LEA	DX, $RSX_EXE
		CALL	SPAWN
		RET
TRY_CWD 	ENDP

;
; Try to find rsx.exe in the directories listed in the PATH environment
; variable
;
TRY_PATH	PROC	NEAR
		LEA	BX, $PATH
		CALL	GETENV
		JC	TRY_PATH_RET
		MOV	SI, DI
FIND_2: 	LEA	DI, PGM_NAME
FIND_3: 	MOV	AL, ES:[SI]
		INC	SI
		OR	AL, AL
		JZ	TRY_PATH_RET
		CMP	AL, " "
		JE	FIND_3
		CMP	AL, TAB
		JE	FIND_3
		CMP	AL, ";"
		JE	FIND_3
FIND_4: 	CMP	DI, OFFSET PGM_NAME_END
		JAE	FIND_5
		MOV	[DI], AL
		MOV	AH, AL
		INC	DI
FIND_5: 	MOV	AL, ES:[SI]
		INC	SI
		OR	AL, AL
		JZ	FIND_6
		CMP	AL, " "
		JE	FIND_5
		CMP	AL, TAB
		JE	FIND_5
		CMP	AL, ";"
		JNE	FIND_4
FIND_6: 	DEC	SI
		CMP	DI, OFFSET PGM_NAME_END - RSX_EXE_LEN
		JAE	FIND_2
;
; We don't support DBCS in rsxl.exe: PATH should not contain directories
; whose name ends with a DBCS characters whose 2nd byte is 2F, 3AH, or 5CH.
;
		CMP	AH, "\"
		JE	FIND_7
		CMP	AH, "/"
		JE	FIND_7
		CMP	AH, ":"
		JE	FIND_7
		CMP	DI, OFFSET PGM_NAME_END
		JAE	FIND_7
                MOV     BYTE PTR DS:[DI], "\"
		INC	DI
FIND_7: 	PUSH	ES
		PUSH	SI
		LEA	SI, $RSX_EXE
		MOV	CX, RSX_EXE_LEN
FIND_8: 	LODSB
		MOV	[DI], AL
		INC	DI
		LOOP	FIND_8
		LEA	DX, PGM_NAME
		CALL	SPAWN
		POP	SI
		POP	ES
		JMP	FIND_2

TRY_PATH_RET:	RET

TRY_PATH	ENDP



;
; Find environment entry
;
; In:	BX	Points to zero-terminated name of environment variable
;
; Out:	CY	Not found
;	NC	Found
;	DI	Points to value of environment variable
;

		ASSUME	DS:SV_DATA

GETENV		PROC	NEAR
		MOV	ES, ENV_SEG
		XOR	DI, DI
		CLD
GETENV_NEXT:    CMP     BYTE PTR ES:[DI], 0     ; Empty environment?
		JE	GETENV_FAILURE		; Yes -> not found
		PUSH	BX			; Save pointer to name
GETENV_COMPARE: MOV	AL, [BX]
		CMP	AL, ES:[DI]		; Compare names
		JNE	GETENV_DIFF		; Mismatch -> try next one
		OR	AL, AL			; Shouldn't happen (`='!)
		JZ	GETENV_DIFF		; (name matches completely)
		INC	BX
		INC	DI
		JMP	GETENV_COMPARE		; Compare next character
GETENV_DIFF:	POP	BX			; Restore pointer to name
		OR	AL, AL			; End of name reached?
		JE	GETENV_EQUAL		; Yes -> candidate found
GETENV_SKIP:	XOR	AL, AL
		MOV	CX, 32767		; Search for next entry
                REPNE   SCAS BYTE PTR ES:[DI]
		JMP	GETENV_NEXT		; Check that entry

GETENV_EQUAL:   CMP     BYTE PTR ES:[DI], "="   ; Exact match?
		JNE	GETENV_SKIP		; No  -> go to next entry
		INC	DI			; Skip `='
		CLC
		JMP	GETENV_RET		; Return pointer to value

GETENV_FAILURE: STC				; Not found
GETENV_RET:	RET

GETENV		ENDP

;
; Compute the length of a zero-terminated string
;
; In:	ES:SI	Points to string
;
; Out:	CX	Length
;
STRLEN		PROC	NEAR
		XOR	CX, CX
		XOR	AL, AL
STRLEN_1:	SCASB
		JE	STRLEN_2
		INC	CX
		JMP	STRLEN_1
STRLEN_2:	RET
STRLEN		ENDP

;
; Try to run a program. Exit if successful, return if failed.
;
; In:	DS:DX	Points to path name of program
;
; Out:	DS	SV_DATA
;

		ASSUME	DS:NOTHING

SPAWN		PROC	NEAR
		MOV	AX, SV_DATA
		MOV	ES, AX
		ASSUME	ES:SV_DATA
		LEA	BX, PAR_BLOCK		; ES:BX -> parameter block
		MOV	AX, 4B00H		; Load and execute program
		INT	21H
		SET_DS				; Restore data segment
		JNC	DONE			; Success -> terminate
		RET

DONE:		MOV	AH, 4DH 		; Get return code of child
		INT	21H			; process
                TEST    AX, 0300H               ; ah=3 TSR, al=code (success)
                JZ      DONE_2
                RET                             ; CWSDPMI.EXE loaded
DONE_2:
		MOV	AH, 4CH 		; terminate process
		INT	21H

SPAWN           ENDP

		ASSUME	ES:NOTHING


INIT_CODE	ENDS


RM_STACK	SEGMENT
		DW	256 DUP (?)
RM_STACK	ENDS

		END	ENTRY
