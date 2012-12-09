/*****************************************************************************
 * FILE: sysdj.c							     *
 *									     *
 * DESC:								     *
 *	- int 0x21, ah=0xff djgpp syscall handler			     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "DPMI.H"
#include "RMLIB.H"
#include "PRINTF.H"
#include "TIMEDOS.H"
#include "STATEMX.H"
#include "PROCESS.H"
#include "SIGNALS.H"
#include "CDOSX32.H"
#include "ADOSX32.H"
#include "START32.H"
#include "COPY32.H"
#include "RSX.H"
#include "DOSERRNO.H"
#include "DJIO.H"

/*
**  For DJGPP programs 1.00 - 1.09 (not coff files) with big segments (4 MB)
**
**  move  stack,data,bss,heap
**	|-CCCCC---------------------------------SSSS-DDDD-HHH-|
**	0					    ^0x400000
**
**  to the end of text
**	|-CCCCC-SSSS-DDDD-HHH-|
*/

void compress_memory(void)
{
    DWORD movedmem, newhandle, newaddress;
    DWORD r_esp = ESP & ~4095L;

    /* check djgpp segment, and stack */
    if (npz->data_start != 0x400000L || r_esp > npz->data_start)
	return;

    movedmem = npz->membytes - r_esp;	/* memory to move */
    if (r_esp - npz->stack_down < movedmem)	/* enough space? */
	return;

    cpy32_32(DS, r_esp, DS, npz->stack_down, movedmem);

    if (ResizeMem(npz->stack_down + movedmem, npz->memhandle, &newhandle,
		  &newaddress)) {
	printf("error:resize memory block\n");
	return;
    }
    if (npz->memaddress != newaddress) {
	npz->memaddress = newaddress;
	SetBaseAddress(npz->code32sel, npz->memaddress);
	SetBaseAddress(npz->data32sel, npz->memaddress);
	SetBaseAddress(npz->data32sel + sel_incr, npz->memaddress);
    }
    npz->p_flags |= PF_COMPRESS;
}

void uncompress_memory(void)
{
    DWORD movedmem, newhandle, newaddress;
    DWORD r_esp = ESP & ~4095L;

    /* check djgpp segment, and stack */
    if (npz->data_start != 0x400000L || !(npz->p_flags & PF_COMPRESS))
	return;

    if (ResizeMem(npz->membytes, npz->memhandle, &newhandle, &newaddress)) {
	printf("Can't switch to parant process: ENOMEM\n");
	shut_down(1);
    }
    if (npz->memaddress != newaddress) {
	npz->memaddress = newaddress;
	SetBaseAddress(npz->code32sel, npz->memaddress);
	SetBaseAddress(npz->data32sel, npz->memaddress);
	SetBaseAddress(npz->data32sel + sel_incr, npz->memaddress);
    }
    movedmem = npz->membytes - r_esp;	/* memory to move */
    cpy32_32(DS, npz->stack_down, DS, r_esp, movedmem);
    npz->p_flags &= ~PF_COMPRESS;
}

void def_extention(char *dst, const char *ext)
{
    int dot, sep;

    dot = 0;
    sep = 1;
    while (*dst != 0)
	switch (*dst++) {
	case '.':
	    dot = 1;
	    sep = 0;
	    break;
	case ':':
	case '/':
	case '\\':
	    dot = 0;
	    sep = 1;
	    break;
	default:
	    sep = 0;
	    break;
	}
    if (!dot && !sep) {
	*dst++ = '.';
	strcpy(dst, ext);
    }
}

char *search_path(char *file, char *path)
{
    char *list, *end;
    int i;

    strcpy(path, file);
    if (rm_access(path, 4) == 0)
	return path;
    list = getenv("PATH");
    if (list != NULL)
	for (;;) {
	    while (*list == ' ' || *list == '\t')
		++list;
	    if (*list == 0)
		break;
	    end = list;
	    while (*end != 0 && *end != ';')
		++end;
	    i = end - list;
	    while (i > 0 && (list[i - 1] == ' ' || list[i - 1] == '\t'))
		--i;
	    if (i != 0) {
		memcpy(path, list, i);
		if (list[i - 1] != '/' && list[i - 1] != '\\' && list[i - 1] != ':')
		    path[i++] = '\\';
		strcpy(path + i, file);
		if (rm_access(path, 4) == 0)
		    return path;
	    }
	    if (*end == 0)
		break;
	    list = end + 1;
	}
    path[0] = 0;
    return (char *) 0;
}

struct time32 {
    DWORD secs, usecs;
};

struct tz32 {
    DWORD offset, dst;
};

struct stat32 {
    short st_dev, st_ino, st_mode, st_nlink, st_uid, st_gid, st_rdev,
     st_align_for_DWORD;
    long st_size, st_atime, st_mtime, st_ctime, st_blksize;
};

static int set_systime(unsigned long *gmt_seconds)
{
    struct tm tm;
    struct dos_time dt;

    gmt2tm(gmt_seconds, &tm);
    dt.dtime_hour = (char) tm.tm_hour;
    dt.dtime_minutes = (char) tm.tm_min;
    dt.dtime_seconds = (char) tm.tm_sec;
    dt.dtime_hsec = 0;
    rm_settime(&dt);
    return 0;
}

static int make_tokens(char *argmem, char **argp)
{
    int i;
    int args=0;

    argp[args++] = argmem;
    for (i = 0; argmem[i] != '\0'; i++)
	if (argmem[i] == ' ') {
	    argmem[i] = '\0';
	    if (argmem[i + 1] == ' ')
		continue;
	    else if (argmem[i + 1] != '\0')
		argp[args++] = (argmem + i + 1);
	}
    argp[args] = 0;

    return args;
}

/*
** DJGPP 1.12 gcc compiler doesn't use this call
**
** RSX starts the new process instead of loading the extender + prg
*/
int sys_system(char *command, char **env)
{
    char *argmem;
    char **argp;
    char exe[260];
    int args, r, i;

    r = strlen(command);
    args = 0;
    for ( i = r; i > 0 ; i--) {
	if (command[i] == ' ')
	    args++;
    }

    argmem = (char *) malloc(r+1);
    argp = (char **) malloc((args+3) * sizeof(char **));
    if (argmem == NULL || argp == NULL) {
	if (argmem != NULL)
	    free(argmem);
	if (argp != NULL)
	    free(argp);
	errno = EMX_ENOMEM;
	return -1;
    }

    strcpy(argmem, command);

    /* make strings from command; build argv */
    args = make_tokens(argmem, argp);

    /* check last argument, response file? */
    if (args > 1 && argp[args - 1][0] == '@') {
	int f, bytes;
	char *s = &argmem[i + 2];

	args--;
	f = rm_open(argp[args] + 1, RM_O_RDONLY | RM_O_DENYWR);
	bytes = rm_read(f, s, 900);
	s[bytes] = 0;
	rm_close(f);

	argp[args++] = s;
	for (i = 0; i < bytes; i++)
	    if (*(s + i) == 13 && *(s + i + 1) == 10) {
		*(s + i) = 0;
		if (i + 4 > bytes)
		    break;
		argp[args++] = (s + i + 2);
	    }
	    else if (*(s + i) == ' ' && *(s + i + 1) > ' ') {
		argp[args++] = (s + i + 2);
	    }
	argp[args] = 0;
    }

    if (rm_access(argmem, 4)) {
	if (!search_path(argmem, exe)) {
	    errno = EMX_ENOENT;
	    free(argmem);
	    free(argp);
	    return -1;
	} else {
	    argp[0] = exe;
	}
    }

    /* resize memory to the real used page */
    compress_memory();

    r = exec32(P_WAIT, argp[0], args, argp, org_envc, org_env);

    /* if error, try a real-mode prg */
    if (r == EMX_ENOEXEC)
	r = realmode_prg(argp[0], &(argp[0]), org_env);

    uncompress_memory();

    free(argmem);
    free(argp);

    return r;
}

/*
** DJGPP 1.11 exec/spawn
** uses TRANSLATION services by DPMI to simulate 0x21 ah=9x4b
**
** RSX starts the new process instead of loading the extender + prg
*/
int simulate_go32_exec(void)
{
    TRANSLATION tr;
    DWORD offset;
    WORD *pblock;
    char **env;
    char *name;
    int z, args;
    char *cmd;
    static char cline[128];

    cpy32_16(ES, EDI, & tr, sizeof(tr));

    /* get pointer to program name */
    tr.edx += (DWORD) tr.ds << 4;
    offset = tr.edx - ((DWORD)ds16real << 4);
    name = (char *)(UINT)offset;

    /* get pointer to exec block */
    tr.ebx += (DWORD) tr.es << 4;
    offset = tr.ebx - ((DWORD)ds16real << 4);
    pblock = (WORD *)(UINT)offset;

    /* get args */
    offset = ((DWORD) pblock[2] << 4) + (DWORD) pblock[1];
    offset -= (DWORD) ds16real << 4;
    cmd = (char *) ((UINT) offset + 1);

    /* build comand line */
    strcpy(cline, name);
    z = strlen(cline);
    for ( ; z < sizeof(cline) ; z++) {
	cline[z] = *cmd;
	if (cline[z] == 13)
	    break;
	cmd++;
    }
    cline[z] = '\0';

    /* environment */
    if (pblock[0]) {
	offset = (DWORD) pblock[0] << 4;
	offset -= ((DWORD)ds16real << 4);
	env = (char **)(UINT)offset;
    }
    else
	env = org_env;

    /*** just like go32 **************
    npz->regs.eax = 0x300;
    if (execute_dpmi_function())
	r = -1;
    else
	r = (int) tr.eax & 0xFF;
    *********************************/

    if (strstr(cline, "!proxy")) {
	/* go32 !proxy argc seg off seg off info */
	char *param[12];	/* if more in future */
	WORD *newargs;

	make_tokens(cline, param);
	args = hexstr2int(param[2]);

	offset = (DWORD) (UINT) hexstr2int(param[3]) << 4;
	offset -= ((DWORD)ds16real << 4);
	newargs = (WORD *) ((UINT) offset + (UINT) hexstr2int(param[4]));

	strcpy(iobuf, (char *) ((UINT)newargs[0] + (UINT)offset));
	for ( z = 1; z < args; z++) {
	    strcat(iobuf, " ");
	    strcat(iobuf, (char *)((UINT)newargs[z] + (UINT)offset));
	}
	cmd = iobuf;
    } else
	cmd = cline;

    /* but it's better to handle it self */
    return sys_system(cmd, NULL);
}

static long my_daylight;
static long my_timezone;

int i_21_ff(void)
{
    DWORD p1 = EBX, p2 = ECX;
    int r, i, handle;

    switch (AX & 0xff) {	/* al=? */
    case 1:
	handle = get_empty_proc_filp();
	if (handle < 0) {
	    EAX = EMX_EBADF;
	    return CARRY_ON;
	}
	strcpy32_16(DS, p1, iobuf);	/* p1 -> iobuf */
	r = dj_creat(iobuf);
	if (r >= 0) {
	    npz->filp[handle]->f_mode = FMODE_READ | FMODE_WRITE;
	    npz->filp[handle]->f_doshandle = r;
	    npz->filp[handle]->f_op = & msdos_fop;
	    r = handle;
	}
	break;

    case 2:
	handle = get_empty_proc_filp();
	if (handle < 0) {
	    EAX = EMX_EBADF;
	    return CARRY_ON;
	}
	strcpy32_16(DS, p1, iobuf);	/* p1 -> iobuf */
	i = (int) (UINT) p2;
	r = dj_open(iobuf, i);
	if (r >= 0) {
	    npz->filp[handle]->f_mode = FMODE_READ | FMODE_WRITE;
	    npz->filp[handle]->f_doshandle = r;
	    npz->filp[handle]->f_op = & msdos_fop;
	    r = handle;
	}
	break;

    case 3:			/* fstat */
	{
	    struct stat32 statbuf32;
	    struct stat statbuf;

	    handle = get_dos_handle((WORD) p1);
	    if (handle < 0) {
		EAX = EMX_EBADF;
		return CARRY_ON;
	    } else
		p1 = handle;

	    r = sys_fstat(handle, &statbuf);
	    if (r == -1) {
		errno = doserror_to_errno(_doserrno);
		break;
	    }
	    statbuf32.st_dev = (short) statbuf.st_dev;
	    statbuf32.st_ino = (short) statbuf.st_ino;
	    statbuf32.st_mode = (short) statbuf.st_mode;
	    statbuf32.st_nlink = (short) statbuf.st_nlink;
	    statbuf32.st_uid = 42;
	    statbuf32.st_gid = 42;
	    statbuf32.st_rdev = (short) statbuf.st_rdev;
	    statbuf32.st_size = statbuf.st_size;
	    statbuf32.st_atime = statbuf.st_atime;
	    statbuf32.st_mtime = statbuf.st_mtime;
	    statbuf32.st_ctime = statbuf.st_ctime;
	    statbuf32.st_blksize = 4096;
	    /* stat -> p2 */
	    cpy16_32(DS, p2, &statbuf32, (DWORD) sizeof(statbuf32));
	}
	break;

    case 4:			/* get time & day */
	{
	    struct time32 time32;
	    struct tz32 tz32;
	    struct dos_date dd;
	    struct dos_time dt;

	    if (p2) {
		tz32.offset = my_timezone;
		tz32.dst = my_daylight;
		cpy16_32(DS, p2, &tz32, (DWORD) sizeof(tz32));
	    }
	    if (p1) {
		unsigned long gmt;
		rm_getdate(&dd);
		rm_gettime(&dt);
		gmt = dos2gmt(&dd, &dt);
		time32.secs = gmt;
		time32.usecs = (DWORD) dt.dtime_hsec * 10;
		cpy16_32(DS, p1, &time32, (DWORD) sizeof(time32));
	    }
	    r = 0;
	}
	break;

    case 5:			/* set time & day */
	{
	    struct time32 time32;
	    struct tz32 tz32;

	    if (p2) {
		cpy32_16(DS, p2, &tz32, (DWORD) sizeof(tz32));
		my_timezone = tz32.offset;
		my_daylight = (WORD) tz32.dst;
	    }
	    if (p1) {
		cpy32_16(DS, p1, &time32, (DWORD) sizeof(time32));
		set_systime((unsigned long *) &(time32.secs));
	    }
	    r = 0;
	}
	break;

    case 6:			/* stat */
	{
	    struct stat32 statbuf32;
	    struct stat statbuf;
	    char fname[160];

	    memset(&statbuf, 0, sizeof(statbuf));
	    strcpy32_16(DS, p1, fname);

	    r = sys_stat(fname, &statbuf);
	    if (r == -1) {
		errno = doserror_to_errno(_doserrno);
		break;
	    }
	    statbuf32.st_dev = (short) statbuf.st_dev;
	    statbuf32.st_ino = (short) statbuf.st_ino;
	    statbuf32.st_mode = (short) statbuf.st_mode;
	    statbuf32.st_nlink = (short) statbuf.st_nlink;
	    statbuf32.st_uid = 42;
	    statbuf32.st_gid = 42;
	    statbuf32.st_rdev = (short) statbuf.st_rdev;
	    statbuf32.st_size = statbuf.st_size;
	    statbuf32.st_atime = statbuf.st_atime;
	    statbuf32.st_mtime = statbuf.st_mtime;
	    statbuf32.st_ctime = statbuf.st_ctime;
	    statbuf32.st_blksize = 512;
	    /* stat -> p2 */
	    cpy16_32(DS, p2, &statbuf32, (DWORD) sizeof(statbuf32));
	}
	break;

    case 7:
	strcpy32_16(DS, p1, iobuf);
	r = sys_system(iobuf, NULL);
	if (r) {
	    errno = r;
	    r = -1;
	}
	break;

    case 8:
	handle = get_dos_handle((WORD) p1);
	if (handle < 0) {
	    EAX = EMX_EBADF;
	    return CARRY_ON;
	} else
	    p1 = (long) handle;

	i = rm_ioctl_getattr(handle);
	if (p2 & 0x8000)	/* O_BINARY */
	    i |= 0x20;
	else
	    i &= ~0x20;
	rm_ioctl_setattr(handle, i);
	r = dj_setmode((int) p1, (int) p2);
	break;

    case 9:
	strcpy32_16(DS, p1, iobuf);	/* p1 -> iobuf */
	r = dj_chmod(iobuf, (int) p2);
	break;

    case 10:	/* DOS 0x4B */
	r = simulate_go32_exec();
	break;

    default:
	r = -1;
	errno = EMX_EIO;
	break;
    }				/* switch */

    if (r == -1) {
	EAX = errno;
	return CARRY_ON;
    } else {
	EAX = (long) r;
	return CARRY_OFF;
    }
}

/* go32 debug functions */
#define EXTERNAL_DEBUGGER_EXECUTE	0
#define EXTERNAL_DEBUGGER_GETINFOPTR	1

typedef struct {
  WORD version; 	/* set to version number of interface */
  WORD a_tss_ofs;	/* pointer to TSS for debugged program */
  WORD a_tss_seg;
  WORD filename_ofs;	/* points to name of file being debugged */
  WORD filename_seg;
  WORD filename_len;
  WORD areas_ofs;	/* from paging.c */
  WORD areas_seg;
  DWORD app_base;	/* linear base address of application */
  DWORD dr[8];		/* debug registers, set when a_tss runs */
  DWORD ansi_mode;	/* if set, OK to emit ansi control codes */
} ExternalDebuggerInfo;

int i_21_fe(void)
{
    puts("external debugger not supported");
    do_exit4c(0);
    return 0;

    /*
    ext_debug_info.version = EXTERNAL_DEBUGGER_VERSION;

    ext_debug_info.a_tss_ofs = FP_OFF(&a_tss);
    ext_debug_info.a_tss_seg = ourDSsel;

    ext_debug_info.filename_ofs = FP_OFF(running_fname);
    ext_debug_info.filename_seg = ourDSsel;
    ext_debug_info.filename_len = strlen(running_fname);

    ext_debug_info.areas_ofs = FP_OFF(&areas);
    ext_debug_info.areas_seg = ourDSsel;

    ext_debug_info.app_base = ARENA;
    ext_debug_info.ansi_mode = use_ansi;

    memset(ext_debug_info.dr, 32, 0);
    */
}
