/*****************************************************************************
 * FILE: intrm.c							     *
 *									     *
 * DESC:								     *
 *	- int 0x21 callback handler (for rm prg)			     *
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
#include "PROCESS.H"
#include "PRINTF.H"
#include "RMLIB.H"
#include "FS.H"
#include "SIGNALS.H"
#include "START32.H"
#include "COPY32.H"
#include "CDOSX32.H"
#include "TERMIO.H"

extern char realmode_filp[N_FILES];

/* lokals */
static void con_read(char *c);
static void con_write(char c);
static int con_avail(void);
static int pipe_avail(struct file *filp);
static int pipe_read_dos(struct file *filp, long dosbuf, int bytes);
static int pipe_write_dos(struct file *filp, long dosbuf, int bytes);


/*  DOS call without callback */
static void call_dos_handler(TRANSLATION *tr)
{
    tr->cs = int21rmv.sel;
    tr->ip = int21rmv.off;
    tr->flags = 0;
    tr->ss = 0;
    tr->sp = 0;
    CallRMprocIret(0, 0, tr);
}


/*
    int int21_realmode_callback(TRANSLATION *tr);

    return:
	0	return the interrupt event
	other	chain to previous handler

    tr.cs and tr.ip  will be restored to caller cs:ip
*/

int int21_realmode_callback(TRANSLATION *tr)
{
    char rAH, rAL;
    DWORD buf_addr;
    int ret;

    rAH = (BYTE) ((tr->eax & 0xFF00) >> 8);
    rAL = (BYTE) tr->eax;

    if (rAH == 0x0C)	    /* flush buffer and run funktion al */
	rAH = rAL;

    switch (rAH) {
	case 0x01:	    /* read char in AL, echo stdout */
	    con_read((char *) &tr->eax);
	    con_write(tr->eax);
	    return 0;
	case 0x02:	    /* write char in dl */
	    con_write(tr->edx);
	    return 0;
	case 0x06:	    /* con output in dl & input dl=ff al ZF */
	    if ((tr->edx & 0xFF) == 0xFF) {
		if (con_avail()) {
		    con_read((char *) &tr->eax);
		    tr->flags &= ~128;
		} else
		    tr->flags |= 128;
	    } else
		con_write(tr->edx);
	    return 0;
	case 0x07:	    /* read char in AL without echo */
	case 0x08:	    /* read char in AL without echo*/
	    con_read((char *) &tr->eax);
	    return 0;
	case 0x0b:	    /* get stdin stat */
	    tr->eax = con_avail();
	    return 0;

	case 0x09:	    /* write string with $ ds:dx */
	    if (realmode_filp[1] != HT_DEV_CON) {
		int i = 0;
		getstr32_16(dosmem_sel, EDX & 0xFFFF, iobuf, '$');
		while (iobuf[i] != '$')
		    i++;
		buf_addr = ((DWORD)tr->ds << 4) + (tr->edx & 0xFFFF);
		ret = pipe_write_dos(npz->filp[1], buf_addr, tr->ecx & 0xFFFF);
	    } else break;
#if 0
	case 0x0a:	    /* input ds:dx 0:bytes 1:ret 2:chars */
#endif

	case 0x3F:  /* read file */
	    if (tr->ebx == 0 && realmode_filp[tr->ebx] != HT_DEV_CON) {
		buf_addr = ((DWORD)tr->ds << 4) + (tr->edx & 0xFFFF);
		ret = pipe_read_dos(npz->filp[tr->ebx],
				buf_addr, tr->ecx & 0xFFFF);
		if (ret < 0) {
		    tr->flags |= 1;
		    tr->eax = -ret;
		} else {
		    tr->flags &= ~1;
		    tr->eax = ret;
		}
		return 0;
	    } else
		return -1;

	case 0x40:  /* write file */
	    if (tr->ebx <= 2 && realmode_filp[tr->ebx] != HT_DEV_CON) {
		buf_addr = ((DWORD)tr->ds << 4) + (tr->edx & 0xFFFF);
		ret = pipe_write_dos(npz->filp[tr->ebx],
				buf_addr, tr->ecx & 0xFFFF);
		if (ret < 0) {
		    tr->flags |= 1;
		    tr->eax = -ret;
		} else {
		    tr->flags &= ~1;
		    tr->eax = ret;
		}
		return 0;
	    } else
		return -1;
    }
    return -1;
}

static void con_read(char *c)
{
    if (realmode_filp[0] != HT_DEV_CON) {
	pipe_read_dos(npz->filp[0], ((DWORD)cs16real<<4) + (DWORD) c, 1);
    } else {
	TRANSLATION tr2;
	tr2.eax = 0x0700;
	tr2.flags = 0;
	call_dos_handler(&tr2);
	*c = tr2.eax;
    }
}

static void con_write(char c)
{
    if (realmode_filp[1] != HT_DEV_CON) {
	pipe_write_dos(npz->filp[1], (ds16real<<4) + (DWORD) &c, 1);
    } else {
	TRANSLATION tr2;
	tr2.eax = 0x0200;
	tr2.edx = c;
	call_dos_handler(&tr2);
    }
}

static int con_avail(void)  /* read */
{
    if (realmode_filp[0] != HT_DEV_CON) {
	return pipe_avail(npz->filp[0]);
    } else {
	TRANSLATION tr2;
	tr2.eax = 0x0B00;
	call_dos_handler(&tr2);
	return (tr2.eax & 0xFF);
    }
}

#define PIPE_EMPTY(pipe)	((pipe)->len == 0)
#define PIPE_FULL(pipe) 	((pipe)->len == (pipe)->size)
#define PIPE_FREE(pipe) 	((pipe)->size - (pipe)->len)
#define PIPE_END(pipe)		(((pipe)->pos+(pipe)->len)&((pipe)->size-1))
#define PIPE_MAX_READ(pipe)	((pipe)->size - (pipe)->pos)
#define PIPE_MAX_WRITE(pipe)	((pipe)->size - PIPE_END(pipe))


static int pipe_avail(struct file *filp)
{
    struct pipe_info *pi = filp->f_info.pipe_i;
    return pi->len;
}

static int pipe_read_dos(struct file *filp, long dosbuf, int bytes)
{
    struct pipe_info *pi = filp->f_info.pipe_i;
    long chars, size, read = 0;

    if (PIPE_EMPTY(pi)) {
	puts("!!pipe read full!!");
	return -30; /* dos read-fault error code */
    }

    while (bytes > 0 && (size = pi->len)) {
	chars = PIPE_MAX_READ(pi);
	if (chars > bytes)
	    chars = bytes;
	if (chars > size)
	    chars = size;
	cpy32_32(pi->sel, pi->pos, dosmem_sel, dosbuf, chars);
	pi->len -= chars;
	pi->pos += chars;
	pi->pos &= (pi->size-1);
	dosbuf += chars;
	bytes -= chars;
	read += chars;
    }

    if (read)
	return read;
    else
	return 0;
}

static int pipe_write_dos(struct file *filp, long dosbuf, int bytes)
{
    struct pipe_info *pi = filp->f_info.pipe_i;
    long chars, free, written = 0;

    while (bytes > 0) {
	if (PIPE_FULL(pi))  {	    /* not enough space */
	    puts("!!pipe write full!!");
	    return (written) ? written : -29;
	}

	while (bytes > 0 && (free = PIPE_FREE(pi))) {
	    chars = PIPE_MAX_WRITE(pi);
	    if (chars > bytes)
		chars = bytes;
	    if (chars > free)
		chars = free;
	    cpy32_32(dosmem_sel, dosbuf, pi->sel, PIPE_END(pi), chars);
	    pi->len += chars;
	    written += chars;
	    bytes -= chars;
	    dosbuf += chars;
	}
    }
    return written;
}
