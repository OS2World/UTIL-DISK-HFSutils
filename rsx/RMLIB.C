/*****************************************************************************
 * FILE: rmlib.c							     *
 *									     *
 * DESC:								     *
 *	- Interface to Real Mode calls					     *
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
#include "START32.H"
#include "CDOSX32.H"
#include "RMLIB.H"
#include "DOSERRNO.H"

#define LV_WORD *(WORD *)&

static int rm_lfn_findfirst(char *name, WORD attr, struct find_t * ft);
static int rm_lfn_findnext(struct find_t * ft);

unsigned rm_bios_read_keybrd(unsigned mode)
{
    TRANSLATION tr;
    unsigned key;

    LV_WORD tr.eax = mode << 8;
    tr.sp = tr.ss = 0;
    tr.flags = 0x3200;
    tr.cs = cs16real;
    tr.ds = cs16real;
    SimulateRMint(0x16, 0, 0, &tr);
    key = (unsigned) (tr.eax & 0xFFFF);

    if ((tr.flags & 64) && (mode == 0x01 || mode == 0x11))
	return 0;
    else
	return key;
}

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

unsigned emx_errno;

#if 0
static int call_rm_dos(TRANSLATION *ts)
{
    ts->flags = 0x3200;
    ts->cs = cs16real;

    SET_SEG_OFF_64(real_mode_stack, ts->ss, ts->sp);

    /* DPMI-Call to Real-Mode DOS */
    SimulateRMint(0x21, 0, 0, ts);

    /* return Carry-bit */
    if (ts->flags & 1) {
	_doserrno = (unsigned) ts->eax & 0xFFFF;
	emx_errno = doserror_to_errno(_doserrno);
	return -1;
    } else
	return (int) (ts->eax & 0xFFFF);
}
#else
static int call_rm_dos(TRANSLATION *ts)
{
    ts->flags = 0x3200;

    SET_SEG_OFF_64(real_mode_stack, ts->ss, ts->sp);

    /* DPMI-Call to Real-Mode DOS (without real mode interrupt) */
    if (int21rmv.sel) {
	ts->cs = int21rmv.sel;
	ts->ip = int21rmv.off;
	CallRMprocIret(0, 0, ts);
    }
    else
	SimulateRMint(0x21, 0, 0, ts);

    /* return Carry-bit */
    if (ts->flags & 1) {
	_doserrno = (unsigned) ts->eax & 0xFFFF;
	emx_errno = doserror_to_errno(_doserrno);
	return -1;
    } else
	return (int) (ts->eax & 0xFFFF);
}
#endif

/* ********** DISK OPERATIONS ********** */

/*
** AH = 0x0E
** DL = drive (a=0,b=1,c=2)
** return: AL max drive
*/
int rm_setdrive(WORD drive)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x0E00;
    LV_WORD ts.edx = drive;
    return (call_rm_dos(&ts) & 0xFF);
}

/*
** AH = 0x19
** return: AL this drive (a=0,b=1,c=2)
*/
int rm_getdrive(void)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x1900;
    return (call_rm_dos(&ts) & 0xFF);
}

/*
** AH = 0x1A
** DS:DX dta address
*/
void rm_setdta(void *buf)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x1A00;
    SET_SEG_OFF(buf, ts.ds, ts.edx);
    call_rm_dos(&ts);
}

/* ********** DATE/TIME OPERATIONS ********** */

/*
** AH = 0x2A
** return: CX=year DX=month/day
*/
void rm_getdate(struct dos_date * dd)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x2A00;
    call_rm_dos(&ts);
    dd->ddate_year = (WORD) ts.ecx;
    dd->ddate_month = (BYTE) ((WORD) ts.edx >> 8);
    dd->ddate_day = (BYTE) ts.edx;
    dd->ddate_dayofweek = (BYTE) ts.eax;
}

/*
** AH = 0x2B
** CX = year
** DX = month/day
*/
int rm_setdate(struct dos_date * dd)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x2B00;
    LV_WORD ts.ecx = (WORD) dd->ddate_year;
    LV_WORD ts.edx = ((WORD) dd->ddate_month << 8) | dd->ddate_day;
    call_rm_dos(&ts);
    if ((ts.eax & 0xFF) == 0xFF)
	return -1;
    else
	return 0;
}

/*
** AH = 0x2C
** return CX=hour/min DX=sec/hsec
*/
void rm_gettime(struct dos_time * dt)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x2C00;
    call_rm_dos(&ts);
    dt->dtime_hour = (BYTE) ((WORD) ts.ecx >> 8);
    dt->dtime_minutes = (BYTE) ts.ecx;
    dt->dtime_seconds = (BYTE) ((WORD) ts.edx >> 8);
    dt->dtime_hsec = (BYTE) ts.edx;
}

/*
** AH = 0x2D
** CX = hour/min
** DX = sec/hsec
*/
int rm_settime(struct dos_time * dt)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x2D00;
    LV_WORD ts.ecx = ((WORD) dt->dtime_hour << 8) | dt->dtime_minutes;
    LV_WORD ts.edx = ((WORD) dt->dtime_seconds << 8) | dt->dtime_hsec;
    call_rm_dos(&ts);
    if ((ts.eax & 0xFF) == 0xFF)
	return -1;
    else
	return 0;
}

/* ********** DIRECTORY OPERATIONS ********** */

/*
** AH = 0x3B / 0x713B
** DS:DX name (64 bytes)
*/
int rm_chdir(char *name)
{
    TRANSLATION ts;

    LV_WORD ts.eax = (lfn) ? 0x713B : 0x3B00;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

/*
** AH = 0x41 / 0x7141
** DS:DX name
*/
int rm_unlink(char *name)
{
    TRANSLATION ts;

    if (lfn) {
        LV_WORD ts.eax = 0x4100;
        LV_WORD ts.esi = 0x0000;    /* no wild */
    }
    else
        LV_WORD ts.eax = 0x4100;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

/*
** AH = 0x43 ; AL = 0 / AX = 7143
** DS:DX name
** return CX: attr
*/
int rm_getfattr(char *name, WORD * attr)
{
    TRANSLATION ts;

    if (lfn) {
        LV_WORD ts.ebx = 0;             /* function code bl = al */
        LV_WORD ts.eax = 0x7143;
    }
    else
        LV_WORD ts.eax = 0x4300;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    if (call_rm_dos(&ts) == -1)
	return -1;
    else {
	*attr = (WORD) ts.ecx;
	return 0;
    }
}

int rm_access(char *name, WORD mode)
{
    WORD attr;

    if (rm_getfattr(name, &attr) == -1)
	return -1;
    if ((attr & 1) && (mode & 2))	/* RDONLY and try WRITE access */
	return -1;
    else
	return 0;
}

/*
** AH = 0x43 ; AL = 1  / AX = 7143
** CX = attr
** DS:DX name
*/
int rm_setfattr(char *name, WORD attr)
{
    TRANSLATION ts;

    if (lfn) {
        LV_WORD ts.ebx = 1;             /* function code bl = al */
        LV_WORD ts.eax = 0x7143;
    }
    else
        LV_WORD ts.eax = 0x4301;
    LV_WORD ts.ecx = attr;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

/*
** AH = 0x47 / AX = 7147
** DS:SI name
*/
int rm_getcwd(WORD drive, char *name)
{
    TRANSLATION ts;

    LV_WORD ts.eax = (lfn) ? 0x7147 : 0x4700;
    LV_WORD ts.edx = drive;
    SET_SEG_OFF(name, ts.ds, ts.esi);
    return call_rm_dos(&ts);
}

/*
** AH = 0x4E / AX = 714E
** CX = attr
** DS:DX name
*/
int rm_findfirst(char *name, WORD attr, struct find_t * ft)
{
    TRANSLATION ts;

    if (lfn)
        return rm_lfn_findfirst(name, attr, ft);

    rm_setdta(ft);
    SET_SEG_OFF(name, ts.ds, ts.edx);
    LV_WORD ts.eax = 0x4E00;
    LV_WORD ts.ecx = attr;
    return call_rm_dos(&ts);
}

/*
** AH = 0x4F / AX = 714F
*/
int rm_findnext(struct find_t * ft)
{
    TRANSLATION ts;

    if (lfn)
        return rm_lfn_findnext(ft);

    rm_setdta(ft);
    LV_WORD ts.eax = 0x4F00;
    return call_rm_dos(&ts);
}

/*
** AH = 0x3C / AX = 716C
** CX = attr
** DS:DX = name
** return: AX fileno
*/
int rm_creat(char *name, WORD attr)
{
    TRANSLATION ts;

    if (lfn) {
        LV_WORD ts.eax = 0x716C;
        LV_WORD ts.ebx = 2;
        LV_WORD ts.ecx = attr;
        LV_WORD ts.edx = CREAT_ALWAYS;
        SET_SEG_OFF(name, ts.ds, ts.esi);
    }
    else {
        LV_WORD ts.eax = 0x3C00;
        LV_WORD ts.ecx = attr;
        SET_SEG_OFF(name, ts.ds, ts.edx);
    }
    return call_rm_dos(&ts);
}

/*
** AH = 0x3D / AX = 716C
** AL = mode
** DS:DX = name
** return: AX fileno
*/
int rm_open(char *name, WORD modes)
{
    TRANSLATION ts;

    if (lfn) {
        LV_WORD ts.eax = 0x716C;
        LV_WORD ts.ebx = modes;
        LV_WORD ts.ecx = 0;
        LV_WORD ts.edx = OPEN_EXISTING;
        SET_SEG_OFF(name, ts.ds, ts.esi);
    }
    else {
        LV_WORD ts.eax = 0x3D00 | modes;
        SET_SEG_OFF(name, ts.ds, ts.edx);
    }
    return call_rm_dos(&ts);
}

/*
** AH = 0x6C / AX = 716C
** BL = open mode
** CX = creat mode
** DS:DX = name
** return: AX fileno, CX status
*/
int rm_extopen(char *name, WORD openmode, WORD creatmode, WORD action)
{
    TRANSLATION ts;

    LV_WORD ts.eax = (lfn) ? 0x716C : 0x6C00;
    LV_WORD ts.ebx = openmode;
    LV_WORD ts.ecx = creatmode;
    LV_WORD ts.edx = action;
    SET_SEG_OFF(name, ts.ds, ts.esi);
    return call_rm_dos(&ts);
}

/*
** AH = 0x3E
** BX = file handle
** return: -1 on error
*/
int rm_close(WORD handle)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x3E00;
    LV_WORD ts.ebx = handle;
    if (call_rm_dos(&ts) == -1)
	return -1;
    else
	return 0;
}

/*
** AH = 0x3F
** BX = handle
** CX = bytes
** DS:DX = buf
** return: AX bytes
*/
int rm_read(WORD handle, void *buf, WORD bytes)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x3F00;
    LV_WORD ts.ebx = (WORD) handle;
    LV_WORD ts.ecx = (WORD) bytes;
    SET_SEG_OFF(buf, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

/*
** AH = 0x40
** BX = handle
** CX = bytes
** DS:DX = buf
** return: AX bytes
*/
int rm_write(WORD handle, void *buf, WORD bytes)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4000;
    LV_WORD ts.ebx = (WORD) handle;
    LV_WORD ts.ecx = (WORD) bytes;
    SET_SEG_OFF(buf, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

/*
** AH = 0x42
** AL = orgin
** BX = handle
** CX:DX = pos
** return: new pos
*/
long rm_lseek(WORD handle, DWORD offset, WORD orgin)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4200 | orgin;
    LV_WORD ts.ebx = (WORD) handle;
    LV_WORD ts.ecx = (WORD) (offset >> 16);
    LV_WORD ts.edx = (WORD) offset;
    if (call_rm_dos(&ts) == -1)
	return -1L;
    else
	return (ts.edx << 16) | (ts.eax & 0xFFFF);
}

/*
** AH = 0x45
** BX = handle
** return: AX new handle
*/
int rm_dup(WORD handle)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4500;
    LV_WORD ts.ebx = handle;
    return call_rm_dos(&ts);
}

/*
** AH = 0x46
** BX = handle
** CX = new handle
** return: -1 on error
*/
int rm_dup2(WORD handle, WORD newhandle)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4600;
    LV_WORD ts.ebx = handle;
    LV_WORD ts.ecx = newhandle;
    return call_rm_dos(&ts);
}

/*
** AX = 0x5700
** BX = handle
** return: CX:DX
*/
int rm_getftime(WORD handle, WORD * date, WORD * time)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x5700;
    LV_WORD ts.ebx = handle;
    if (call_rm_dos(&ts) == -1)
	return -1;
    else {
	*date = (WORD) ts.edx;
	*time = (WORD) ts.ecx;
	return 0;
    }
}

/*
** AX = 0x5701
** BX = handle
** CX:DX = time/date
*/
int rm_setftime(WORD handle, WORD date, WORD time)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x5701;
    LV_WORD ts.ebx = handle;
    LV_WORD ts.ecx = time;
    LV_WORD ts.edx = date;
    return call_rm_dos(&ts);
}

/*
** AH = 0x5B
** CX = attr
** DS:DX = name
** return: AX fileno
*/
int rm_creatnew(char *name, WORD attr)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x5B00;
    LV_WORD ts.ecx = attr;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

/*
** AX = 0x67
** BX = handles
*/
int rm_sethandles(WORD handles)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x6700;
    LV_WORD ts.ebx = handles;
    return call_rm_dos(&ts);
}

/* ********** IOCTL OPERATIONS ********** */

/*
** AH = 0x44 ; AL = 0
** BX = handle
*/
int rm_ioctl_getattr(WORD handle)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4400;
    LV_WORD ts.ebx = handle;
    if (call_rm_dos(&ts) == -1)
	return -1;
    else
	return (WORD) ts.edx;
}

int rm_isatty(WORD handle)
{
    int i = rm_ioctl_getattr(handle);
    if (i == -1)
	return 0;
    else
	return ((i & 0x83) == 0x83) ? 1 : 0;
}

/*
** AH = 0x44 ; AL = 01
** BX = handle
** DX = mode
*/
int rm_ioctl_setattr(WORD handle, WORD mode)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4401;
    LV_WORD ts.ebx = handle;
    LV_WORD ts.edx = mode;
    if (call_rm_dos(&ts) == -1)
	return -1;
    else
	return 0;
}

/*
** AH = 0x44 ; AL = 06
** BX = handle
*/
int rm_ioctl_select_in(WORD handle)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4406;
    LV_WORD ts.ebx = handle;
    if (call_rm_dos(&ts) == -1)
	return 0;
    return ((BYTE)ts.eax == 0xFF) ? 1 : 0;
}

/*
** AH = 0x44 ; AL = 09
** BL = drive
** ret: 1/0 remote or -1
*/
int rm_ioctl_remotedrive(BYTE drive)
{
    TRANSLATION ts;

    LV_WORD ts.eax = (WORD) 0x4409;
    LV_WORD ts.ebx = (WORD) drive;
    if (call_rm_dos(&ts) == -1)
	return -1;
    else
	return (int) (ts.edx & 0x1000) >> 12;
}

/* ********** PROCESS CONTROL ********** */

/*
** AX = 0x4B00
** DS:DX = name
** ES:BX = exec block
** ret: -1 on error
*/
int rm_exec(char *name, struct execb * eb)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4B00;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    SET_SEG_OFF(eb, ts.es, ts.ebx);
    return call_rm_dos(&ts);
}

/*
** AX = 0x4D00
** ret: exit value
*/
int rm_get_exit_status(void)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x4D00;
    return call_rm_dos(&ts);
}

/*
** AX = 0x5900
** ret: exit value
*/
int rm_get_exterror_code(void)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x5900;
    return call_rm_dos(&ts);
}

/*
** AX = 0x3800
** DS:DX = info struct
** ret: exit value
*/
int rm_get_country_info(struct country_info *cinfo)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x3800;
    SET_SEG_OFF(cinfo, ts.ds, ts.edx);
    return call_rm_dos(&ts);
}

unsigned char rm_country_casemap(unsigned char ascii, DWORD casemap_func)
{
    TRANSLATION ts;

    ts.eax = ascii;
    ts.flags = 0x3200;
    SET_SEG_OFF_64(real_mode_stack, ts.ss, ts.sp);

    ts.cs = (unsigned short) (casemap_func >> 16);
    ts.ip = (unsigned short) (casemap_func & 0xffff);

    CallRMprocIret(0, 0, &ts);

    return ((unsigned char)ts.eax);
}

/***
  Windows 95 extended DOS API
***/

static char * strchr(char *str, int c)
{
  char *s = str;
  char cc = c;

  while (*s)
  {
    if (*s == cc)
      return s;
    s++;
  }
  if (cc == 0)
    return s;

  return (char *) 0;
}

int rm_test_lfn(void)
{
    TRANSLATION ts;
    const char *path = "C:\\";
    char filesys[20];

    ts.eax = 0x71A0;
    ts.ebx = 0;
    ts.ecx = sizeof(filesys);
    ts.flags = 0;
    SET_SEG_OFF_64(path, ts.ds, ts.edx);
    SET_SEG_OFF_64(filesys, ts.es, ts.edi);

    if (call_rm_dos(&ts) == -1 || !(ts.ebx & 0x4000))
        return 0;
    else
        return 1;
}

typedef struct win32_find_data {
    unsigned long attrib;           /* bits: 0-6 DOS 8 temp file  */
    unsigned long ctime[2];	    /* creation time (low/high)   */
    unsigned long atime[2];	    /* access time		  */
    unsigned long mtime[2];	    /* modification time	  */
    unsigned long size_hi;	    /* length of file (high dword)*/
    unsigned long size_lo;	    /* length of file (low dword) */
    unsigned long reserved[2];
    char longname[260]; 	    /* null-terminated filename   */
    char shortname[14]; 	    /* null-terminated filename   */
} WIN32_FIND_DATA;

static void convert_dta(struct find_t *dos, WIN32_FIND_DATA *win32)
{

    dos->attrib = (unsigned char) win32->attrib;
    dos->wr_time = (unsigned short) win32->mtime[0];
    dos->wr_date = (unsigned short) (win32->mtime[0] >> 16);
    dos->size_lo = win32->size_lo & 0xffff;
    dos->size_hi = win32->size_lo >> 16;

    memcpy(dos->magic, "LFN", 3);
    strcpy(dos->shortname, win32->shortname);
    strcpy(dos->name, win32->longname);
}

static int rm_findclose(unsigned short lfn_handle)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x71A1;
    LV_WORD ts.ebx = lfn_handle;

    return call_rm_dos(&ts);
}

static int rm_lfn_findfirst(char *name, WORD attr, struct find_t * ft)
{
    WIN32_FIND_DATA win32_dta;
    TRANSLATION ts;
    int ret;

    rm_setdta(ft);

    LV_WORD ts.eax = 0x714E;
    LV_WORD ts.ecx = attr;
    LV_WORD ts.esi = 1;
    SET_SEG_OFF(name, ts.ds, ts.edx);
    SET_SEG_OFF(&win32_dta, ts.es, ts.edi);

    ret = call_rm_dos(&ts);
    ft->lfn_handle = (unsigned short) ts.eax;

    if (ret != -1)
        convert_dta(ft, &win32_dta);

    if (ret == -1 || (!strchr(name, '*') && !strchr(name, '?')))
        rm_findclose(ft->lfn_handle);

    return ret;
}

static int rm_lfn_findnext(struct find_t * ft)
{
    WIN32_FIND_DATA win32_dta;
    TRANSLATION ts;
    int ret;

    rm_setdta(ft);

    LV_WORD ts.eax = 0x714F;
    LV_WORD ts.esi = 1;
    LV_WORD ts.ebx = ft->lfn_handle;
    SET_SEG_OFF(&win32_dta, ts.es, ts.edi);

    ret = call_rm_dos(&ts);
    if (ret == -1)
        rm_findclose(ft->lfn_handle);
    else
        convert_dta(ft, &win32_dta);
    return ret;
}
