/*****************************************************************************
 * FILE: cdosx32.c							     *
 *									     *
 * DESC:								     *
 *	- int 0x21 dos call handler					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 * Thanks to: (bug-fixes, patches, testing, ..)                              *
 *      Friedbert Widmann <widi@informatik.uni-koblenz.de>                   *
 *                                                                           *
 *****************************************************************************/

#include "DPMI.H"
#include "PRINTF.H"
#include "RMLIB.H"
#include "PROCESS.H"
#include "FS.H"
#include "SIGNALS.H"
#include "START32.H"
#include "CDOSX32.H"
#include "COPY32.H"
#include "TERMIO.H"
#include "DOSERRNO.H"
#include "DJIO.H"
#include "RSX.H"

/* define segment offset usage */
#ifdef __EMX__
#define SET_SEG_OFF(pointer, seg, off) \
    { seg = (((unsigned) (pointer) & ~0xFFF) >> 4) + ds16real; \
	off = (unsigned) (pointer) & 0xFFF; }

#else				/* 16 bit compiler */
#define SET_SEG_OFF(pointer, seg, off) \
    { seg = ds16real; off = (WORD) (pointer) ;}
#endif

#ifdef __EMX__
#define SET_SEG_OFF_64(pointer, seg, off) \
    { seg = (((unsigned) (pointer) & ~0xFFFF) >> 4) + ds16real; \
	off = (unsigned) (pointer) & 0xFFFF; }

#else				/* 16 bit compiler */
#define SET_SEG_OFF_64(pointer, seg, off) \
    { seg = ds16real; off = (WORD) (pointer) ;}
#endif

extern WORD _psp;		/* PSP after protected mode switch */

static char iobuffer[IOBUF_SIZE + 3];
char *iobuf = iobuffer; 	/* I/O buffer for real-mode operations */
DWORD iobuf_addr;

DWORD iobuf_linear;		/* linear address I/O for djgpp */

void align_iobuf(void)
{
    iobuf = (char *) ((unsigned) (iobuf + 3) & ~3);
    iobuf_addr = ((DWORD) ds16real << 4) + (DWORD) (UINT) iobuf;
    iobuf_linear = iobuf_addr + 4096L;	/* if we use this buffer */

    if (main_options.opt_printall)
	printf("lin address of iobuf %lX\n", iobuf_linear);
}


#define CONVERT_BX_DOSHANDLE {	\
    int h = get_dos_handle(BX); \
    if (h < 0) {		\
	EAX = EMX_EBADF;	\
	return CARRY_ON;	\
    }				\
    else BX = h; }


static void put_regs(TRANSLATION *tr)
{
    tr->eax = EAX;
    tr->ebx = EBX;
    tr->ecx = ECX;
    tr->edx = EDX;
}

static void get_regs(TRANSLATION *tr)
{
    EAX = tr->eax;
    EBX = tr->ebx;
    ECX = tr->ecx;
    EDX = tr->edx;
    FLAGS = tr->flags;
}

static char rm_stack[512];
unsigned real_mode_stack = (unsigned) (rm_stack + 510);

/* call Real-Mode INT0x21 */
#if 0
static int realdos(TRANSLATION *tr)
{
    SET_SEG_OFF_64(real_mode_stack, tr->ss, tr->sp);

    tr->flags = 0x3200;
    tr->eax &= 0xFFFF;
    tr->cs = cs16real;
    tr->es = tr->ds;

    /* DPMI-Call to Real-Mode DOS */
    SimulateRMint(0x21, 0, 0, tr);

    /* return Carry-bit */
    return (tr->flags & 1);
}
#else
static int realdos(TRANSLATION *tr)
{
    SET_SEG_OFF_64(real_mode_stack, tr->ss, tr->sp);

    tr->flags = 0x3200;
    tr->eax &= 0xFFFF;
    tr->cs = int21rmv.sel;
    tr->ip = int21rmv.off;
    tr->es = tr->ds;

    /* DPMI-Call to Real-Mode DOS (without real mode interrupt) */
    CallRMprocIret(0, 0, tr);

    /* return Carry-bit */
    return (tr->flags & 1);
}
#endif

static int illegal_syscall()
{
    puts("Illegal Parameter in syscall");
    send_signal(npz, SIGSEGV);
    EAX = EMX_EINTR;
    return CARRY_ON;
}

#define TEST_ILLEGAL( OFFSET, LENGHT ) \
if (verify_illegal(npz, OFFSET, LENGHT)) return illegal_syscall();

#define TEST_ILLEGAL_WRITE( OFFSET, LENGHT ) \
if (verify_illegal_write(npz, OFFSET, LENGHT)) return illegal_syscall();

/*
** INT 0x21 handler returns:
**
** CARRY_ON :	error, set carry-bit, errno in eax
** CARRY_OFF:	no error, clear carry-bit
** CARRY_NON:	carry-bit set by realdos -> dos error code in ax,
**		translate ax to errno
*/
int int21normal(void)
{
    static DWORD user_dta;	/* prot-mode DTA address */
    TRANSLATION tr;		/* registers set for mode-switching */
    DWORD saveEBX, saveEDX;	/* save EBX while DOS-calls */
    int err, new_handle, drive, len;
    unsigned char rAH, rAL;

    rAH = (BYTE) (AX >> 8);
    rAL = (BYTE) EAX;

    switch (rAH) {
	/*
	** register based functions
	** no changes needed - only ax,dx,flags (bx,cx) used
	*/
    case 0x01:			/* read char */
    case 0x02:			/* write char */
    case 0x03:			/* read char stdaux */
    case 0x04:			/* write char stdaux */
    case 0x05:			/* read char prn */
    case 0x06:			/* con output&input */
    case 0x07:			/* char input & echo */
    case 0x08:			/* char output & echo */
    case 0x0b:			/* get stdin stat */
    case 0x0c:			/* flush buffer & read std input */
    case 0x0d:			/* disk reset */
    case 0x0e:			/* select drive */
    case 0x19:			/* get drive */
    case 0x2a:			/* get system date (cx) */
    case 0x2b:			/* set system date (cx) */
    case 0x2c:			/* get time (cx) */
    case 0x2d:			/* set time (cx) */
    case 0x2e:			/* set verify */
    case 0x30:			/* get DOS version (bx,cx) */
    case 0x33:			/* extended break check */
    case 0x36:			/* get free disk space (bx,cx) */
    case 0x37:			/* get&set switch char */
    case 0x54:			/* get verify flag */
    case 0x58:			/* get/set UMB link state */
    case 0x5c:			/* un-&lock file */
    case 0x66:			/* code page */
	put_regs(&tr);
	realdos(&tr);
	get_regs(&tr);
	if (rAH == 0x0e) {
	    npz->cdrive = rm_getdrive();
	}
	return CARRY_NON;
    case 0x44:			/* IOCTL */
	switch (rAL) {
	case 0x00:		/* get device info */
	case 0x01:		/* set device info */
	case 0x06:		/* get input status */
	case 0x07:		/* get output status */
	case 0x0a:		/* check handle remote */
	    saveEBX = EBX;	/* save BX to restore after
				 * CONVERT_BX_DOSHANDLE, because all
				 * 0x44-functions leave the value of
				 * BX unchanged
				 */
	    CONVERT_BX_DOSHANDLE;
	    put_regs(&tr);
	    realdos(&tr);
	    get_regs(&tr);
	    EBX = saveEBX;
	    return CARRY_NON;
	case 0x08:		/* check block device remove */
	case 0x09:		/* check block device remote */
	case 0x0b:		/* set sharing */
	case 0x0e:		/* get log drive map */
	case 0x0f:		/* set log drive map */
	case 0x10:		/* query ioctl handle */
	case 0x11:		/* query ioctl drive */
	    put_regs(&tr);
	    realdos(&tr);
	    get_regs(&tr);
	    return CARRY_NON;
	default:
	    EAX = EMX_EIO;
	    return CARRY_ON;
	}

	/*
	** simple file handle functions
	** convert handle
	*/
    case 0x57:			/* get file state (bx,cx) */
    case 0x68:			/* fflush file (bx) */
	CONVERT_BX_DOSHANDLE;
	put_regs(&tr);
	realdos(&tr);
	get_regs(&tr);
	return CARRY_NON;

    case 0x3e:			/* close file (bx) */
	err = sys_close(BX);
	if (err < 0) {
	    EAX = emx_errno;
	    return CARRY_ON;
	} else {
	    EAX = 0;
	    return CARRY_OFF;
	}

    case 0x42:			/* lseek file */
	{
	    long pos;

	    if (npz->p_flags & PF_EMX_FILE)
		pos = EDX;
	    else
		pos = (ECX << 16) | (EDX & 0xFFFF);

	    if ((EAX = sys_lseek(BX, pos, rAL))== -1L) {
		EAX = emx_errno;
		return CARRY_ON;
	    }
	    if (npz->p_flags & PF_DJGPP_FILE) {
		EDX = EAX >> 16;
	    }
	    return CARRY_OFF;
	}

    case 0x45:			/* dup file (bx) */
	if ((new_handle = sys_dup(BX)) < 0) {
	    EAX = (long) -new_handle;
	    return CARRY_ON;
	}
	EAX = (long) new_handle;
	return CARRY_OFF;

    case 0x46:			/* dup2 file (bx,cx) */
	if ((new_handle = sys_dup2(BX, CX)) < 0) {
	    EAX = (long) -new_handle;
	    return CARRY_ON;
	}
	EAX = (long) new_handle;
	return CARRY_OFF;

	/*
	** ASCiiZero functions
	**
	** copy name -> real_buffer , call realdos
	*/

    case 0x09:			/* string to output */
	TEST_ILLEGAL(EDX, 2);
	getstr32_16(DS, EDX, iobuf, '$');
	tr.eax = EAX;
	SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	realdos(&tr);
	return CARRY_NON;

    case 0x39:			/* MKDIR name */
    case 0x3a:			/* RMDIR name */
    case 0x41:			/* UNLINK name */
    case 0x43:			/* ATTRIB name */
	TEST_ILLEGAL(EDX, 2);
        strcpy32_16(DS, EDX, iobuf);
        convert_filename(iobuf);

	put_regs(&tr);
        if (lfn) {
            tr.eax = 0x7100 | rAH;
            if (rAH == 0x41)
                tr.esi = 0;
            if (rAH == 0x43)
                tr.ebx = rAL;
        }
	SET_SEG_OFF(iobuf, tr.ds, tr.edx);
        realdos(&tr);
        EAX = tr.eax;
        FLAGS = tr.flags;
        return CARRY_NON;

    case 0x3b:			/* CHDIR name */
	TEST_ILLEGAL(EDX, 2);
	saveEDX = EDX;
	strcpy32_16(DS, EDX, iobuf);
	convert_filename(iobuf);

	if (iobuf[1] == ':') {
	    if (iobuf[0] >= 'A' && iobuf[0] <= 'Z')
		drive = iobuf[0] - 'A';
	    else if (iobuf[0] >= 'a' && iobuf[0] <= 'z')
		drive = iobuf[0] - 'a';
	    else
		drive = npz->cdrive;
	}
	else
	    drive = npz->cdrive;
	/* save cwd[drive] in all processes */
	if (npz->cwd[drive] == NULL)
	    store_cwd (drive);

	put_regs(&tr);
	SET_SEG_OFF(iobuf, tr.ds, tr.edx);
        if (lfn)
            tr.eax = 0x713b;
	realdos(&tr);
	get_regs(&tr);
	EDX = saveEDX;
	/* save cwd in this process */
	get_cwd (drive);
	return CARRY_NON;

    case 0x3c:			/* CREATE name */
    case 0x3d:			/* OPEN name */
    case 0x5b:			/* CREATE New file */
	/*
	** only DOS handles !!
	** others with EMX: 0x7F ; DJGPP 0xFF
	*/
	if (rAH == 0x3c)	/* fix umask bug */
	    ECX &= ~1L;
	TEST_ILLEGAL(EDX, 2);
	saveEDX = EDX;
	strcpy32_16(DS, EDX, iobuf);
	convert_filename(iobuf);
	put_regs(&tr);

        if (lfn) {
            tr.eax = 0x716C;
            if (rAH == 0x3c) {
                tr.edx = CREAT_NEW;
                tr.ebx = 0;
            }
            else if (rAH == 0x3d) {
                tr.edx = OPEN_EXISTING;
                tr.ebx = tr.eax;
            }
            else if (rAH == 0x5b) {
                tr.edx = OPEN_ALWAYS;
                tr.ebx = 0;
            }
            SET_SEG_OFF(iobuf, tr.ds, tr.esi);
        }
        else
            SET_SEG_OFF(iobuf, tr.ds, tr.edx);

	if (!realdos(&tr)) {
	    new_handle = get_empty_proc_filp();
	    npz->filp[new_handle]->f_mode = FMODE_READ | FMODE_WRITE;
	    npz->filp[new_handle]->f_doshandle = (BYTE) tr.eax;
	    npz->filp[new_handle]->f_op = & msdos_fop;
	    tr.eax = (long) new_handle;
	}
	get_regs(&tr);
	EDX = saveEDX;
	return CARRY_NON;

    case 0x56:			/* RENAME oldname name (strlen < 1024) */
	TEST_ILLEGAL(EDX, 2);
	TEST_ILLEGAL(EDI, 2);
	saveEDX = EDX;
	strcpy32_16(DS, EDX, iobuf);
	strcpy32_16(DS, EDI, iobuf + 1024);
	convert_filename(iobuf);
	convert_filename(iobuf + 1024);
	put_regs(&tr);
	SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	tr.es = tr.ds;
	tr.edi = tr.edx + 1024;
        if (lfn)
            tr.eax = 0x7156;
	realdos(&tr);
	get_regs(&tr);
	EDX = saveEDX;
	return CARRY_NON;

    case 0x5a:			/* CREATE TEMP name */
	TEST_ILLEGAL_WRITE(EDX, 2);
	put_regs(&tr);
	strcpy32_16(DS, EDX, iobuf);
	convert_filename(iobuf);
	SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	if (!realdos(&tr)) {
	    strcpy16_32(DS, EDX, iobuf);
	    new_handle = get_empty_proc_filp();
	    npz->filp[new_handle]->f_mode = FMODE_READ | FMODE_WRITE;
	    npz->filp[new_handle]->f_doshandle = (BYTE) tr.eax;
	    npz->filp[new_handle]->f_op = & msdos_fop;
	    tr.eax = (long) new_handle;
	}
	get_regs(&tr);
	return CARRY_NON;

    case 0x6c:			/* EXTENDED OPEN/CREATE file */
	TEST_ILLEGAL(ESI, 2);
	strcpy32_16(DS, ESI, iobuf);
	convert_filename(iobuf);
	put_regs(&tr);
	SET_SEG_OFF(iobuf, tr.ds, tr.esi);
	if (!realdos(&tr)) {
	    new_handle = get_empty_proc_filp();
	    npz->filp[new_handle]->f_mode = FMODE_READ | FMODE_WRITE;
	    npz->filp[new_handle]->f_doshandle = (BYTE) tr.eax;
	    npz->filp[new_handle]->f_op = & msdos_fop;
	    tr.eax = (long) new_handle;
	}
	get_regs(&tr);
	return CARRY_NON;

	/*
	** buffer functions
	**
	** copy data before or after realdos
	*/
    case 0x0a:			/* BUFFERED INPUT */
	{
	    BYTE no_chars;

	    TEST_ILLEGAL_WRITE(EDX, 4);
	    put_regs(&tr);
	    no_chars = (BYTE) read32(DS, EDX);
            iobuf[0] = no_chars;                    /* no of chars with CR */
	    SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	    realdos(&tr);
            no_chars = iobuf[1];                    /* no of chars without CR */
            cpy16_32(DS, EDX, iobuf, (DWORD) no_chars + 3);
	    return CARRY_OFF;
	}

    case 0x3f:			/* READ from file */
	if (EDX != iobuf_linear) {
	    TEST_ILLEGAL_WRITE(EDX, ECX);
	}
	if (ECX == 0) {
	    EAX = 0;
	    return CARRY_OFF;
	}
	if ((npz->p_flags & PF_DJGPP_FILE) && EDX == 0) {
	    printf("read.o not ok ; run DPMIFIX from DJGPP 1.10\n");
	    EAX = EMX_EIO;
	    return CARRY_ON;
	}

	EAX = sys_read(BX, EDX, ECX);
	if ((long) EAX < 0) {
	    EAX = - (long)EAX;
	    return CARRY_ON;
	} else
	    return CARRY_OFF;

    case 0x40:			/* WRITE to file */
	if (EDX != iobuf_linear) {
	    TEST_ILLEGAL(EDX, ECX);
	}
	EAX = sys_write(BX, EDX, ECX);
	if ((long)EAX < 0) {
	    EAX = - (long)EAX;
	    return CARRY_ON;
	} else
	    return CARRY_OFF;

    case 0x38:			/* GET COUNTRY INFO */
	if (DX != 0xFFFF) {
	    TEST_ILLEGAL_WRITE(EDX, 34);
	    saveEDX = EDX;
	    put_regs(&tr);
	    SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	    if (!realdos(&tr))
		cpy16_32(DS, EDX, iobuf, 34L);
	    get_regs(&tr);
	    EDX = saveEDX;
	} else {
	    put_regs(&tr);
	    realdos(&tr);
	    get_regs(&tr);
	}
	return CARRY_NON;

    case 0x47:			/* GET CURR DIRECTORY */
	TEST_ILLEGAL_WRITE(ESI, 64);
	put_regs(&tr);
	SET_SEG_OFF(iobuf, tr.ds, tr.esi);
        if (lfn)
            tr.eax = 0x7147;
	if (!realdos(&tr))
            if (lfn)
                strcpy16_32(DS, ESI, iobuf);
            else
                cpy16_32(DS, ESI, iobuf, 64L);
	get_regs(&tr);
	return CARRY_NON;

    case 0x4e:			/* FINDFIRST */
        /* DTA: iobuf byte 0-42 , wild _string: iobuf + 512 */
	if (npz->p_flags & PF_EMX_FILE) {
	    /* set dta address to iobuf */
	    user_dta = ESI;
	    SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	    tr.eax = 0x1a00;
	    realdos(&tr);
	}
        if (lfn)
            len = 30 + 257;
        else
            len = 30 + 13;
        TEST_ILLEGAL_WRITE(user_dta, len);
	TEST_ILLEGAL(EDX, 2);
        strcpy32_16(DS, EDX, iobuf + 512);
        convert_filename(iobuf + 512);

        err = rm_findfirst(iobuf + 512, CX, (struct find_t *) iobuf);

        if (err == -1) {
            EAX = emx_errno;
            return CARRY_ON;
        }
        else {
            cpy16_32(DS, user_dta, iobuf, len);
            return CARRY_OFF;
        }

    case 0x4f:			/* FINDNEXT */
	/* DTA: iobuf byte 0-42 */
	if (npz->p_flags & PF_EMX_FILE) {
	    user_dta = ESI;
	    SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	    tr.eax = 0x1a00;
	    /* set dta address */
	    realdos(&tr);
	}
        /* put user dta in iobuf */
        if (lfn)
            len = 30 + 257;
        else
            len = 30 + 13;
        TEST_ILLEGAL_WRITE(user_dta, len);
        cpy32_16(DS, user_dta, iobuf, len);

        err = rm_findnext((struct find_t *) iobuf);

        if (err == -1) {
            EAX = emx_errno;
            return CARRY_ON;
        }
        else {
            cpy16_32(DS, user_dta, iobuf, len);
            return CARRY_OFF;
        }

	/*
	** some special handling
	*/
    case 0x1a:			/* SET DTA */
	TEST_ILLEGAL(EDX, 2);
	user_dta = EDX;
	tr.eax = 0x1a00;
	SET_SEG_OFF(iobuf, tr.ds, tr.edx);
	realdos(&tr);
	return CARRY_OFF;

    case 0x2f:			/* GET DTA */
	EBX = user_dta;
	return CARRY_OFF;

    case 0x62:			/* GET PSP */
	EBX = (DWORD) _psp;
	return CARRY_OFF;

	/*
	** functions complete changed
	** need to call DPMI-functions
	*/
    case 0x48:			/* ALLOC MEM */
    case 0x49:			/* FREE MEM */
	EAX = EMX_EIO;
	return CARRY_ON;

    case 0x4a:			/* RESIZE MEM */
	if (npz->p_flags & PF_EMX_FILE) {
	    EAX = EMX_EIO;
	    return CARRY_ON;
	} else {
	    if (EAX & 0xff)
		EAX = getmem(EBX, npz);
	    else
		EAX = npz->brk_value;
	    if (EAX == -1)
		EAX = 0;
	    return CARRY_OFF;	/* sbrk.s didn't check carry */
	}

    case 0x4c:
	return do_exit4c(0);

    case 0x4d:
	{
	    unsigned status = 0;
	    /* sys_wait(&status); */
	    AX = status >> 8;	/* al = return code */
	    return CARRY_OFF;
	}

    default:
	printf("Warning: Not implemented DOS function ah=%02X\n", rAH);
	EAX = EMX_EIO;
	return CARRY_ON;

    }				/* switch R_AH */
}

#include "DJIO.H"

/*
** read for dos handles
*/
ARGUSER cdosx_read(int handle, ARGUSER buf, ARGUSER count)
{
    long org_bytes;
    int iob_bytes;
    int ret_bytes;

    if ((npz->p_flags & PF_DJGPP_FILE) && buf == (ARGUSER)iobuf_linear)
	return dj_read(handle, iobuf+4096, (unsigned)count);

    org_bytes = count;
    while (count > 0) {
	iob_bytes = (count <= IOBUF_SIZE) ? (int) count : IOBUF_SIZE;

	if (npz->p_flags & PF_DJGPP_FILE) {
	    if ((ret_bytes = dj_read(handle, iobuf, iob_bytes)) == -1)
		return -(long)errno_djgpp(emx_errno);
	} else {
	    if ((ret_bytes = rm_read(handle, iobuf, iob_bytes)) == -1)
		return -(long)emx_errno;
	}
	cpy16_32(DS, buf, iobuf, (long) ret_bytes);
	count -= (long) ret_bytes;
	if (ret_bytes < iob_bytes)  /* EOF */
	    break;
	buf += ret_bytes;
    }
    return (org_bytes - count);
}

/*
** write for dos handles
*/
ARGUSER cdosx_write(int handle, ARGUSER buf, ARGUSER count)
{
    long org_bytes;
    int iob_bytes;
    int ret_bytes;

    if (!count)
	return (ARGUSER) rm_write(handle, &iobuf, 0);

    if ((npz->p_flags & PF_DJGPP_FILE) && buf == (ARGUSER) iobuf_linear)
	return dj_write(handle, iobuf+4096, (unsigned)count);

    org_bytes = count;
    while (count > 0) {
	iob_bytes = (count <= IOBUF_SIZE) ? (int) count : IOBUF_SIZE;
	cpy32_16(DS, buf, iobuf, (long)iob_bytes);

	if (npz->p_flags & PF_DJGPP_FILE) {
	    if ((ret_bytes = dj_write(handle, iobuf, iob_bytes)) == -1)
		return -(long)errno_djgpp(emx_errno);
	} else {
	    if ((ret_bytes = rm_write(handle, iobuf, iob_bytes)) == -1)
		return -(long)emx_errno;
	}
	count -= (long) ret_bytes;
	if (ret_bytes < iob_bytes)  /* disk full */
	    break;
	buf += ret_bytes;
    }
    return (org_bytes - count);
}

typedef union {
    unsigned long flat;
    struct FarPtr {
	unsigned short off16;
	unsigned short seg16;
    } farptr;
} PTR;

#pragma pack(1)
typedef struct {
    char    signatur[4];
    char    version[2];
    PTR     oem_name;
    char    capabilities[4];
    PTR     modes;
    char    blocks[2];
} VESAINFO;
#pragma pack()

static unsigned real_mode_vio10(TRANSLATION *pTrans)
{
    pTrans->sp = pTrans->ss = 0;
    pTrans->flags = 0x3200;
    return SimulateRMint(0x10, 0, 0, pTrans);
}
static unsigned real_mode_mou33(TRANSLATION *pTrans)
{
    pTrans->sp = pTrans->ss = 0;
    pTrans->flags = 0x3200;
    return SimulateRMint(0x33, 0, 0, pTrans);
}

/*
** support for vesa modes ah = 0x4F, al = 00 - 08
*/
static void vio10(void)
{
    unsigned char rAH, rAL;
    TRANSLATION tr;
    VESAINFO *pInfo;
    DWORD offset;

    rAH = (BYTE) (AX >> 8);
    rAL = (BYTE) EAX;

    if (rAH != 0x4F)
	return;

    switch (rAL) {
	case 0x00:
	    SET_SEG_OFF(iobuf, tr.es, tr.edi);
	    put_regs(&tr);
	    real_mode_vio10(&tr);
	    if ((tr.eax & 0xFF) != 0x4f)
		break;
	    else
		EAX = tr.eax;	    /* success */

	    pInfo = (VESAINFO *) iobuf;
	    offset = ((DWORD) pInfo->oem_name.farptr.seg16 << 4)
		     + (DWORD) pInfo->oem_name.farptr.off16;
	    pInfo->oem_name.flat = (dpmi10) ?
		offset + DPMI_PRG_DATA :
		offset - npz->memaddress;
	    offset = ((DWORD) pInfo->modes.farptr.seg16 << 4)
		     + (DWORD) pInfo->modes.farptr.off16;
	    pInfo->modes.flat = (dpmi10) ?
		offset + DPMI_PRG_DATA :
		offset - npz->memaddress;

	    cpy16_32(ES, EDI, iobuf, 256);
	    break;

	case 0x01:
	    SET_SEG_OFF(iobuf, tr.es, tr.edi);
	    put_regs(&tr);
	    real_mode_vio10(&tr);
	    if ((tr.eax & 0xFF) != 0x4f)
		break;
	    else
		EAX = tr.eax;

	    * (DWORD *) (iobuf + 0x0C) = 0; /* zero far function */
	    cpy16_32(ES, EDI, iobuf, 256);
	    break;

	case 0x02: /* set SVGA mode */
	case 0x03: /* get SVGA mode */
	case 0x05: /* return ax */
	case 0x06: /* return ax,bx,cx,dx */
	case 0x07: /* return ax */
	case 0x08: /* return ax,bx */
	    put_regs(&tr);
	    real_mode_vio10(&tr);
	    get_regs(&tr);
	    break;

	case 0x04: /* unsupported */
	default:
	    printf("Not supported Vesa call %X", AX);
	    return;
    }
}

/*
** support for mouse calls
*/
static void mou33(void)
{
    TRANSLATION tr;
    unsigned char rAH = (BYTE) EAX;

    if (rAH > 0x35) /* not supported */
	return;

    switch (rAH) {
	case 0x0C:	/* Define IRQ routine	    */
	case 0x14:	/* Exchange IRQ routines    */
	case 0x16:	/* Save state		    */
	case 0x17:	/* Restore state	    */
	case 0x18:	/* Set event handler	    */
	case 0x19:	/* Return event handler     */
	case 0x29:	/* Enumerate video modes    */
	case 0x2B:	/* Load acceleration prof.  */
	case 0x2C:	/* Get acceleration prof.   */
	case 0x2D:	/* Select acc. profile	    */
	case 0x2E:	/* Set acc. profile names   */
	case 0x33:	/* Switch settings etc.     */
	case 0x34:	/* Get initialization file  */
	    break; /* not supported */

	case 0x09:	/* define graphic cursor es:dx */
	    put_regs(&tr);
	    cpy32_16(ES, EDX, iobuf, 32);
	    SET_SEG_OFF(iobuf, tr.es, tr.edx);
	    real_mode_mou33(&tr);
	    EAX = tr.eax;
	    break;

	case 0x12:	/* graphic cursor block es:dx ; bh*ch*4 bytes */
	    put_regs(&tr);
	    cpy32_16(ES, EDX, iobuf, BH * CH * 4);
	    SET_SEG_OFF(iobuf, tr.es, tr.edx);
	    real_mode_mou33(&tr);
	    get_regs(&tr);
	    break;

	default:
	    put_regs(&tr);
	    real_mode_mou33(&tr);
	    get_regs(&tr);
	    break;
    }
}

void prot_mode_interrupt(void)
{
    int int_no = (int) npz->regs.faultno;

    if (int_no == 0x10)
	return vio10();
    else if (int_no == 0x33)
	return mou33();
}
