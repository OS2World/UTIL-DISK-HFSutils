/*****************************************************************************
 * FILE: fs.c								     *
 *									     *
 * DESC:								     *
 *	- file system functions 					     *
 *	- sys_close, sys_dup, sys_sup2, sys_lseek			     *
 *	- msdos file operations 					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 * Thanks to: (bug-fixes, patches, testing, ..)                              *
 *      Friedbert Widmann <widi@informatik.uni-koblenz.de>                   *
 *                                                                           *
 *****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "DPMI.H"
#include "PROCESS.H"
#include "DOSERRNO.H"
#include "CDOSX32.H"
#include "EXCEP32.H"
#include "SIGNALS.H"
#include "RMLIB.H"
#include "PRINTF.H"
#include "FS.H"
#include "COPY32.H"
#include "TERMIO.H"
#include "RSX.H"

#ifdef __EMX__
#define RSX
#else
#define RSXWIN
#endif

#ifdef RSXWIN
#include "conwin.h"
#define TIME_TIC time_tic()
#else
#define TIME_TIC time_tic
#endif

struct file rsx_filetab[RSX_NFILES];

/*
** make entries for standard dos_files
*/
void init_rsx_filetab()
{
    rsx_filetab[0].f_mode = FMODE_READ;
    rsx_filetab[0].f_count = 1;
    rsx_filetab[0].f_doshandle = 0;
    rsx_filetab[0].f_op = & msdos_fop;
    rsx_filetab[0].f_flags = (rm_isatty(0)) ? FCNTL_CONSOLE : 0;

    rsx_filetab[1].f_mode = FMODE_WRITE;
    rsx_filetab[1].f_count = 1;
    rsx_filetab[1].f_doshandle = 1;
    rsx_filetab[1].f_op = & msdos_fop;

    rsx_filetab[2].f_mode = FMODE_WRITE;
    rsx_filetab[2].f_count = 1;
    rsx_filetab[2].f_doshandle = (opt_stderr) ? 1 : 2;
    rsx_filetab[2].f_op = & msdos_fop;

    rsx_filetab[3].f_mode = FMODE_WRITE;
    rsx_filetab[3].f_count = 1;
    rsx_filetab[3].f_doshandle = 3;
    rsx_filetab[3].f_op = & msdos_fop;

    rsx_filetab[4].f_mode = FMODE_WRITE;
    rsx_filetab[4].f_count = 1;
    rsx_filetab[4].f_doshandle = 4;
    rsx_filetab[4].f_op = & msdos_fop;
}

/*
** search for free file pointer
*/
int get_empty_proc_filp(void)
{
    int fd, i;

    /* search for entry in proc tab */
    for (fd = 0 ; fd < N_FILES ; fd++)
	if (!npz->filp[fd])
	    break;
    if (fd >= N_FILES)
	return -EMX_EMFILE;

    /* search for entry in rsx_filetab */
    for (i = 0; i < RSX_NFILES; i++)
	if (! rsx_filetab[i].f_count)
	    break;
    if (rsx_filetab[i].f_count)
	return -EMX_EMFILE;

    rsx_filetab[i].f_count = 1;
    rsx_filetab[i].f_mode = FMODE_READ | FMODE_WRITE;
    npz->filp[fd] = & rsx_filetab[i];
    return fd;
}

/*
** convert handle to real DOS file handle
*/
int get_dos_handle(int handle)
{
    if (handle >= N_FILES || !npz->filp[handle])
	return -1;
    else if (npz->filp[handle]->f_doshandle == -1)
	return -2;
    else
	return npz->filp[handle]->f_doshandle;
}

/*
** close a file handle
*/
int sys_close(int fd)
{
    struct file * filp;

    if (fd >= N_FILES)
	return -EMX_EBADF;
    if (!(filp = npz->filp[fd]))
	return -EMX_EBADF;
    npz->filp[fd] = NIL_FP;

    if (filp->f_count == 0) {
	printf("FS error: file %d count is 0\n", fd);
	return 0;
    }
    if (filp->f_count > 1) {
	filp->f_count--;
	return 0;
    }
    if (filp->f_op && filp->f_op->release)
	filp->f_op->release(filp);
    filp->f_count--;
    return 0;
}

/*
** dup a file handle
*/
static int dupfd(unsigned int fd, unsigned int arg)
{
    if (fd >= N_FILES || !npz->filp[fd])
	return -EMX_EBADF;
    if (arg >= N_FILES)
	return -EMX_EINVAL;
    while (arg < N_FILES)
	if (npz->filp[arg])
	    arg++;
	else
	    break;
    if (arg >= N_FILES)
	    return -EMX_EMFILE;
    npz->filp[arg] = npz->filp[fd];
    npz->filp[arg]->f_count++;
    return arg;
}

int sys_dup2(unsigned int oldfd, unsigned int newfd)
{
    if (oldfd >= N_FILES || !npz->filp[oldfd])
	return -EMX_EBADF;
    if (newfd == oldfd)
	return newfd;
    if (newfd > N_FILES)
	return -EMX_EBADF;
    if (newfd == N_FILES)
	return -EMX_EBADF;
    sys_close(newfd);
    return dupfd(oldfd, newfd);
}

int sys_dup(unsigned int fildes)
{
    return dupfd(fildes,0);
}

/*
** read from file handle
*/
ARGUSER sys_read(int fd, ARGUSER buf, ARGUSER bytes)
{
    struct file * file;

    if (fd >= N_FILES)
	return -EMX_EBADF;
    if (!(file = npz->filp[fd]))
	return -EMX_EBADF;
    if (! file->f_op || ! file->f_op->read)
	return -EMX_EBADF;
    /* tested in cdosx32 **
    if (verify_illegal_write(npz, buf, bytes)) {
	send_signal(npz, SIGSEGV);
	return -EMX_EINTR;
    } */
    return file->f_op->read(file, buf, bytes);
}

/*
** write to file handle
*/
ARGUSER sys_write(int fd, ARGUSER buf, ARGUSER bytes)
{
    struct file * file;

    if (fd >= N_FILES)
	return -EMX_EBADF;
    if (!(file = npz->filp[fd]))
	return -EMX_EBADF;
    if (! file->f_op || ! file->f_op->write)
	return -EMX_EBADF;
    /* tested in cdosx32 **
    if (verify_illegal(npz, buf, bytes)) {
	send_signal(npz, SIGSEGV);
	return -EMX_EINTR;
    }
    */
    return file->f_op->write(file, buf, bytes);
}

/*
** seek file handle
*/
long sys_lseek(int fd, long off, int orgin)
{
    struct file * file;

    if (fd >= N_FILES)
	return -EMX_EBADF;
    if (!(file = npz->filp[fd]))
	return -EMX_EBADF;
    if (! file->f_op || ! file->f_op->lseek)
	return -EMX_EBADF;
    return file->f_op->lseek(file, off, orgin);
}

int sys_select(ARGUSER u_select)
{
    DWORD time_val[2];
    DWORD time_end = TIME_TIC;      /* initialise with current time */
    struct _select select;
    struct _fd_set fd_in, fd_out;
    struct file *file;
    int nhandles;
    int i;
#ifdef RSXWIN
    unsigned long test;
#endif

    /* get select struct */
    cpy32_16(DS, u_select, &select, sizeof(select));

    if (select.time != 0) {	    /* get time struct */
	cpy32_16(DS, select.time, time_val, sizeof(time_val));
	time_end += ((time_val[0] * 182) / 10);     /* timer tics  sec */
	time_end += ((time_val[1] * 182) / 10000);		/* usec */
    } else {
	time_end = 0xffffffff;
    }

    if (select.readfds)     /* get read fd struct */
	cpy32_16(DS, select.readfds, &fd_in, sizeof(fd_in));
    else
	return -EMX_EINVAL;

    memset( &fd_out, 0, sizeof(fd_out));    /* FD_ZERO */

    if (select.nfds > N_FILES)
	select.nfds = N_FILES;

#ifdef RSXWIN
    /* test if only stdin */
    test = 0;
    for (i = 0; i < 7; i++) {
	test += (fd_in.bits[i]);
    }
    if (test != 1 || !(FD_ISSET(0, &fd_in)))
	test = 0;

    if (test) {
	win_cursor_on();
	time_end = 0xffffffff;
    }
#endif
    nhandles = 0;
    for (;;) {
	for (i = 0; i < (int)select.nfds; i++) {
	    if (!(FD_ISSET(i, &fd_in)))
		continue;
	    file = npz->filp[i];
	    if (!(file = npz->filp[i]))
		continue;
	    if (! file->f_op || ! file->f_op->select)
		continue;
	    if (file->f_op->select(file)) {
		FD_SET(i, &fd_out);
		nhandles ++;
	    }
	}
	if (nhandles > 0)
	    break;
        if (time_end != 0xFFFFFFFF && TIME_TIC >= time_end)
	    break;
	if (npz->sig_raised)	/* if SIGINT or SIGQUIT */
	    break;
	schedule();		/* use time for other processes */
    }
#ifdef RSXWIN
    if (test)
	win_cursor_off();
#endif

    if (select.readfds)     /* put read fd struct */
	cpy16_32(DS, select.readfds, &fd_out, sizeof(fd_out));

    if (select.writefds)
	bzero32(DS, select.writefds, sizeof(struct _fd_set));

    if (select.excepfds)
	bzero32(DS, select.excepfds, sizeof(struct _fd_set));

    return nhandles;
}

int sys_ioctl(int fd, ARGUSER request, ARGUSER arg)
{
    struct file * file;

    if (fd >= N_FILES)
	return -EMX_EBADF;
    if (!(file = npz->filp[fd]))
	return -EMX_EBADF;
    if (! file->f_op || ! file->f_op->ioctl)
	return -EMX_EBADF;
    return file->f_op->ioctl(file, request, arg);
}

int sys_pipe(ARGUSER size, ARGUSER arg)
{
#if 0
    return -EMX_EMSDOS;
#else
    int fin, fout;
    struct pipe_info *pi;
    struct file *file;

    fin = get_empty_proc_filp();
    fout = get_empty_proc_filp();
    if (fin < 0 || fout < 0) {
	npz->filp[fin]->f_count = 0;
	npz->filp[fout]->f_count = 0;
	return -EMX_EMFILE;
    }
    pi = (struct pipe_info *) malloc (sizeof(struct pipe_info));
    if (!pi) {
	npz->filp[fin]->f_count = 0;
	npz->filp[fout]->f_count = 0;
	return -EMX_ENOMEM;
    }
    if (AllocMem(size, &(pi->memhandle), &(pi->memaddress))) {
	npz->filp[fin]->f_count = 0;
	npz->filp[fout]->f_count = 0;
	free(pi);
	return -EMX_ENOMEM;
    }
    AllocLDT(1, & pi->sel);
    SetBaseAddress(pi->sel, pi->memaddress);
    SetAccess(pi->sel, APP_DATA_SEL, 0);
    SetLimit(pi->sel, size-1);

    pi->pos = 0;
    pi->len = 0;
    pi->size = size;
    pi->rd_openers = 0;
    pi->wr_openers = 0;
    pi->readers = 1;
    pi->writers = 1;

    file = npz->filp[fin];
    file->f_mode = FMODE_READ;
    file->f_doshandle = -1;
    file->f_op = & pipe_fop;
    file->f_offset = 0;
    file->f_info.pipe_i = pi;

    file = npz->filp[fout];
    file->f_mode = FMODE_WRITE;
    file->f_doshandle = -1;
    file->f_op = & pipe_fop;
    file->f_offset = 0;
    file->f_info.pipe_i = pi;

    store32 (DS, arg, (long) fin);
    store32 (DS, arg + 4, (long) fout);

    return 0;
#endif
}

/* ----------------------------------------------------------------- */

/*
** MS-DOS
** fileoperations
*/

static long msdos_lseek(struct file *filp, long off, int orgin)
{
    long pos;

    if ((pos = rm_lseek(filp->f_doshandle, off, orgin)) == -1L)
	return -emx_errno;
    else
	return pos;
}

static ARGUSER msdos_read(struct file *filp, ARGUSER buf, ARGUSER bytes)
{
    /* test termio on stdin */
    if (filp->f_doshandle == 0 && (npz->p_flags & PF_TERMIO))
	return termio_read(DS, buf, (int)bytes);
    else
	return cdosx_read(filp->f_doshandle, buf, bytes);
}

static ARGUSER msdos_write(struct file *filp, ARGUSER buf, ARGUSER bytes)
{
    return cdosx_write(filp->f_doshandle, buf, bytes);
}

static int msdos_select(struct file *filp)
{
    /* test termio on stdin */
#ifdef RSX
    if (filp->f_doshandle == 0 && (npz->p_flags & PF_TERMIO))
	return (do_fionread()) ? 1 : 0;
#else
    if (filp->f_doshandle == 0) 	/* rsxio */
	return win_kbhit();
    else if (filp->f_doshandle <= 2)	/* rsxio */
	return 1;
#endif
    else
	return rm_ioctl_select_in(filp->f_doshandle);
}

extern int sysemx_ioctl(int handle, ARGUSER request, ARGUSER arg);
static int msdos_ioctl(struct file *filp, ARGUSER request, ARGUSER arg)
{
    /* test console on stdin */
    if (filp->f_doshandle == 0 && (filp->f_flags & FCNTL_CONSOLE))
	return kbd_ioctl(request, arg);
    else
	return sysemx_ioctl(filp->f_doshandle, request, arg);
}

static void msdos_release(struct file *filp)
{
    if (filp->f_doshandle <= 4)
	printf("FS error: closing doshandle %d\n", filp->f_doshandle);
    else
	rm_close(filp->f_doshandle);
}

struct file_operations msdos_fop =  {
    msdos_lseek,
    msdos_read,
    msdos_write,
    msdos_select,
    msdos_ioctl,
    NULL,   /* special open */
    msdos_release
};

/* ----------------------------------------------------------------- */

/*
** pipe 		!! JUST A FIRST TEST !!
** fileoperations
*/

#define PIPE_EMPTY(pipe)	((pipe)->len == 0)
#define PIPE_FULL(pipe) 	((pipe)->len == (pipe)->size)
#define PIPE_FREE(pipe) 	((pipe)->size - (pipe)->len)
#define PIPE_END(pipe)		(((pipe)->pos+(pipe)->len)&((pipe)->size-1))
#define PIPE_MAX_READ(pipe)	((pipe)->size - (pipe)->pos)
#define PIPE_MAX_WRITE(pipe)	((pipe)->size - PIPE_END(pipe))

static long pipe_lseek(struct file *filp, long off, int orgin)
{
    emx_errno = EMX_ESPIPE;
    return -1;
}

static ARGUSER pipe_read(struct file *filp, ARGUSER buf, ARGUSER bytes)
{
    struct pipe_info *pi = filp->f_info.pipe_i;
    long chars, size, read = 0;

    if (filp->f_flags & FCNTL_NDELAY) {
	if (PIPE_EMPTY(pi))
	    if (pi->writers)
		return -EMX_EAGAIN;
	    else
		return 0;
    }
    while (PIPE_EMPTY(pi)) {
	if (!pi->writers)
	    return 0;
	if (!schedule())    /* no other processes */
	    return -EMX_EPIPE;
    }
    while (bytes > 0 && (size = pi->len)) {
	chars = PIPE_MAX_READ(pi);
	if (chars > bytes)
	    chars = bytes;
	if (chars > size)
	    chars = size;
	cpy32_32(pi->sel, pi->pos, DS, buf, chars);
	pi->len -= chars;
	pi->pos += chars;
	pi->pos &= (pi->size-1);
	buf += chars;
	bytes -= chars;
	read += chars;
	schedule();
    }
    if (read)
	return read;
    if (pi->writers)
	return -EMX_EAGAIN;
    return 0;
}

static ARGUSER pipe_write(struct file *filp, ARGUSER buf, ARGUSER bytes)
{
    struct pipe_info *pi = filp->f_info.pipe_i;
    long chars, free, written = 0;

    if (!pi->readers) {
	send_signal(npz, SIGPIPE);
	return -EMX_EPIPE;
    }
    if (bytes <= pi->size)
	free = bytes;
    else
	free = 1;

    while (bytes > 0) {
	while (PIPE_FREE(pi) < free) {	    /* not enough space */
	    if (!pi->readers) {
		send_signal(npz, SIGPIPE);
		return written ? written : -EMX_EPIPE;
	    }
	    if (filp->f_flags & FCNTL_NDELAY)
		return written ? written : -EMX_EAGAIN;
	    if (!schedule())
		return -EMX_EPIPE;
	}
	while (bytes > 0 && (free = PIPE_FREE(pi))) {
	    chars = PIPE_MAX_WRITE(pi);
	    if (chars > bytes)
		chars = bytes;
	    if (chars > free)
		chars = free;
	    cpy32_32(DS, buf, pi->sel, PIPE_END(pi), chars);
	    pi->len += chars;
	    written += chars;
	    bytes -= chars;
	    buf += chars;
	}
	schedule();
    }
    return written;
}

static int pipe_select(struct file *filp)
{
    struct pipe_info *pi = filp->f_info.pipe_i;

    if (filp->f_mode == FMODE_READ)
	return (pi->len != 0L);
    else if (filp->f_mode == FMODE_WRITE)
	return (pi->len != pi->size);
    else
	return 0;
}

static int pipe_ioctl(struct file *filp, ARGUSER request, ARGUSER arg)
{
    long temp;
    struct pipe_info *pi = filp->f_info.pipe_i;

    if (request == FIONREAD)
	temp = pi->len;
    else if (request == FGETHTYPE)
	temp = HT_UPIPE;
    else
	return -EMX_EINVAL;

    store32(DS, arg, temp);
    return 0;
}

static void pipe_release(struct file *filp)
{
    struct pipe_info *pi = filp->f_info.pipe_i;

    if (filp->f_mode & FMODE_READ)
	pi->readers--;
    else if (filp->f_mode & FMODE_WRITE)
	pi->writers--;
    else
	puts("FS error: pipe with no r/w mode");

    if (!pi->readers && !pi->writers) {
	FreeMem(pi->memhandle);
	FreeLDT(pi->sel);
	free(pi);
	filp->f_info.pipe_i = NULL;
    }
}

struct file_operations pipe_fop =  {
    pipe_lseek,
    pipe_read,
    pipe_write,
    pipe_select,
    pipe_ioctl,
    NULL,   /* special open */
    pipe_release
};

/* ----------------------------------------------------------------- */

/*
** Do all filename-conversions.
** These are:
**	-Rp:	if the first char of a filename is the dot, convert it
**		to exclamation-mark (.emacs ==> !emacs)
**	-t:	truncate filenames to 8.3 convention
**	-Rt803	truncate filenames to 8.3 convention
**	-Rt553	truncate filenames to 5-3.3 convention
**	-r*	add default diskdrive
*/

#define MAX(a,b) ((a) > (b) ? (a) : (b))

char *convert_filename (char *name)
{
    struct options *o = &npz->options;
    register char *dst, *src, *p;
    enum { FIRSTCHAR, BASENAME, FIRSTEXT, MOREEXT } state;
    char *part;
    int first, last, ext;

    /* work within the original string, because result is shorter */
    src = dst = name;

    if (*src == '/' || *src == '\\') {
	if (src[1] == '/' || src[1] == '\\') {  /* name starts with '//' */
	    strcpy (src+1, src+2);		/*     ==> strip one '/' */
	} else
	if (memcmp (src+1, "dev/", 4) == 0) {   /* the unix-devices '/dev/' */
	    if (strcmp (src+5, "tty") == 0)
		return strcpy (name, "con");
	    else if (strcmp (src+5, "null") == 0)
		return strcpy (name, "nul");
	} else
	if (memcmp (src+1, "pipe", 4) == 0) {   /* see /emx/doc/emxdev.doc */
	} else
	if (o->opt_default_disk) {
	    dst = name + strlen (name) + 1;	/* create space for X: */
	    src = dst+2;
	    while (dst > name)
		*--src = *--dst;
					/* now: dst = name, src = name+2 */
	    *dst++ = o->opt_default_disk;
	    *dst++ = ':';
	}
    }

    /* is the rest neccessary ? */
    if (!o->opt_convert_dot && !o->opt_convert_filename)
	return name;

    switch (o->opt_convert_filename) {
	case 803:
		first = 8;
		last = 0;
		ext = 3;
		break;
	case 533:
		first = 5;
		last = 3;
		ext = 3;
		break;
	default:
		first = 256;
		last = 0;
		ext = 256;
		break;
    }

    part = src;
    state = FIRSTCHAR;

    for ( ; *src; src++) {
	if (*src == '/' || *src == '\\') {
	    if (state == BASENAME) {		/* copy tail of name */
		p = MAX (part + first, src - last);
		while (p < src)
		    *dst++ = *p++;
	    }
	    *dst++ = *src;			/* copy '/' */
	    part = src+1;
	    state = FIRSTCHAR;
	} else
	if (*src == '.') {              /* start of extension */
	    switch (state) {
		case FIRSTCHAR:
		    state = BASENAME;
		    /* do special things with "." and ".." */
		    if (src[1] == '\0' || src[1] == '/' || src[1] == '\\') {
			*dst++ = *src;
			break;
		    } else
		    if (src[1] == '.'  &&
			(src[2] == '\0' || src[2] == '/' || src[2] == '\\')) {
			*dst++ = *src++;
			*dst++ = *src;
			break;
		    } else
		    if (o->opt_convert_dot) {
			*dst++ = '!';
			break;
		    }
		    /* else fall through */
		case BASENAME:
		    p = MAX (part + first, src - last);
		    while (p < src)
			*dst++ = *p++;
		    state = FIRSTEXT;	/* this ist the first extension */
		    *dst++ = *src;
		    part = src+1;
		    break;

		case FIRSTEXT:
		    state = MOREEXT;
		case MOREEXT:
		    if (!o->opt_convert_filename)
			*dst++ = *src;
		    break;
	    }
	} else {
	    if (o->opt_convert_dot)
		switch (*src) { 		/* convert unix-like	     */
		    case '+':                   /* special characters to '!' */
			*src = '!';
			break;
		}
	    switch (state) {
		case FIRSTCHAR:
		    state = BASENAME;
		case BASENAME:
		    if (src < part+first)
			*dst++ = *src;
		    break;
		case FIRSTEXT:
		    if (src < part+ext) {
			*dst++ = *src;
			break;
		    }
		    /* else */
		case MOREEXT:
		    if (!o->opt_convert_filename)
			*dst++ = *src;
		    break;
	    }
	}
    }
    if (state == BASENAME) {
	p = MAX (part + first, src - last);
	while (p < src)
	    *dst++ = *p++;
    }
    *dst = '\0';

    return name;
}
