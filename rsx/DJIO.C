/*****************************************************************************
 * FILE: djio.c 							     *
 *									     *
 * DESC:								     *
 *	- io library for djgpp support					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

/*
** source: libc from emx/gcc (c) Eberhard Mattes
*/

#include "DPMI.H"
#include "RMLIB.H"
#include "DOSERRNO.H"
#include <stdarg.h>
#include <stddef.h>

#ifdef __EMX__

/* from djgpp include files */
#define O_RDONLY	0x0001
#define O_WRONLY	0x0002
#define O_RDWR		0x0004
#define O_CREAT 	0x0100
#define O_TRUNC 	0x0200
#define O_EXCL		0x0400
#define O_APPEND	0x0800
#define O_NDELAY	0x2000
#define O_TEXT		0x4000
#define O_BINARY	0x8000

#define O_ACCMODE	0x07

#define O_NOINHERIT	0x0080
#define O_DENYALL	0x0010
#define O_DENYWRITE	0x0020
#define O_DENYREAD	0x0030
#define O_DENYNONE	0x0040

#define F_EOF	    0x00000020
#define F_TERMIO    0x00000040
#define F_DEV	    0x00000080
#define F_NPIPE     0x00001000

#define S_IREAD 0x0100	/* owner may read */
#define S_IWRITE 0x0080 /* owner may write */
#define S_IEXEC 0x0040	/* owner may execute <directory search> */

#define SH_MASK 	  0x70

#define FALSE 0
#define TRUE  1

int _fmode_bin = O_BINARY;
int _nfiles = 40;
long _files[40];
int _lookahead[40];

typedef int (*OPENFUNC) (char *, WORD);

void djio_init(void)
{
    int j;
    int handle;

    for (handle = 0; handle < 5; ++handle) {
	_files[handle] = 0;
	_lookahead[handle] = -1;
	if ((j = rm_ioctl_getattr(handle)) != -1) {
	    _files[handle] |= O_TEXT;
	    if (j & 128)
		_files[handle] |= F_DEV;
	    if (handle == 0)
		_files[handle] |= O_RDONLY;
	    else if (handle == 1 || handle == 2)
		_files[handle] |= O_WRONLY;
	    else
		_files[handle] |= O_RDWR;
	}
    }
}

void djio_dup_fileattr(int from, int to)
{
    if (to > 40)
	return;
    _files[to] = _files[from];
    _lookahead[to] = _lookahead[from];
}

static void *my_memchr(const void *s, unsigned char c, size_t n)
{
    if (n != 0) {
	register const unsigned char *p = s;
	do {
	    if (*p++ == c)
		return ((void *) (p - 1));
	} while (--n != 0);
    }
    return (void *) 0;
}

static int do_ftruncate(int fh, long pos)
{
    long temp;

    if ((temp = rm_lseek(fh, 0, SEEK_END)) == -1)
	return -1;
    else if (temp > (long) pos)
	if (rm_lseek(fh, pos, SEEK_SET) == -1 || rm_write(fh, 0, 0) == -1)
	    return -1;
    return 0;
}

static int do_open(char *buf, long flags)
{
    int i, dosfile;
    OPENFUNC openfn;
    unsigned p2;

    i = (int) (flags >> 16);	/* 1 = O_CREAT, 2 = O_EXCL, 4 = O_TRUNC */

    if (i & 0x01) {
	if (i & 0x02)		/* O_CREAT | O_EXCL */
	    openfn = rm_creatnew;
	else if (i & 4) 	/* O_CREAT | O_TRUNC */
	    openfn = rm_creat;
	else if (rm_access(buf, 0) == -1)	/* old file there ? */
	    openfn = rm_creat;
	else
	    openfn = rm_open;
    } else			/* not O_CREAT */
	openfn = rm_open;

    if (openfn == rm_open) {
	int rdwr = flags & O_ACCMODE;
	rdwr >>= 1;
	p2 = flags & 0xff;
	p2 &= ~O_ACCMODE;
	p2 |= rdwr;
    }
    else
	p2 = (unsigned) flags >> 8;	/* creat attrib */

    dosfile = (*openfn) (buf, (WORD) p2);

    if (dosfile == -1)
	return -1;
    else {
	if ((i & 0x05) == 0x4) {/* O_TRUNC with open */
	    if (do_ftruncate(dosfile, 0) < 0)
		return -1;
	}
	return dosfile;
    }

}

static int dj_sopen(char *name, int oflag, int shflag, int pmode)
{
    int handle, saved_errno;
    long flags, bits, last;
    char dummy, ctrlz_kludge = FALSE;

    if ((oflag & O_ACCMODE) == O_RDONLY && (oflag & (O_TRUNC | O_CREAT))) {
	emx_errno = EMX_EINVAL;
	return (-1);
    }
    bits = oflag & (O_ACCMODE | O_NDELAY | O_APPEND);
    if (oflag & O_BINARY)
	 /* do nothing */ ;
    else if (oflag & O_TEXT)
	bits |= O_TEXT;
    else if (_fmode_bin == 0)	/* neither O_TEXT nor O_BINARY given */
	bits |= O_TEXT;

    if ((bits & O_TEXT) && (oflag & O_APPEND) && (oflag & O_ACCMODE) == O_WRONLY) {
	/*
	 * The caller requests to open a text file for appending in
	 * write-only.	To remove the Ctrl-Z (if there is any), we have to
	 * temporarily open the file in read/write mode.
	 */

	flags = O_RDWR | (shflag & SH_MASK);
	ctrlz_kludge = TRUE;
    } else
	flags = (oflag & O_ACCMODE) | (shflag & SH_MASK);

    if (oflag & O_CREAT) {
	int attr = 0;
	if (!(pmode & S_IWRITE))
	    attr |= _A_RDONLY;
	flags |= (attr << 8) | 0x10000;
	if (oflag & O_EXCL)
	    flags |= 0x20000;
    }
    if (oflag & O_TRUNC)
	flags |= 0x40000;

    saved_errno = emx_errno;
    handle = do_open(name, flags);
    if (handle < 0 && ctrlz_kludge && emx_errno == EMX_EACCES) {
	/* Perhaps read access is denied.  Try again. */
	emx_errno = saved_errno;
	ctrlz_kludge = FALSE;
	flags = (flags & ~O_ACCMODE) | (oflag & O_ACCMODE);
	handle = do_open(name, flags);
    }
    if (handle < 0)
	return (-1);

    if (handle >= _nfiles) {
	rm_close(handle);
	emx_errno = EMX_EMFILE;
	return (-1);
    }
    if (rm_ioctl_getattr(handle) & 0x80) {
	bits |= F_DEV;
	oflag &= ~O_APPEND;
    }
    if (!(bits & F_DEV) && (bits & O_TEXT)) {
	last = rm_lseek(handle, -1L, SEEK_END);
	if (last != -1 && rm_read(handle, &dummy, 1) == 1 && dummy == 0x1a)
	    do_ftruncate(handle, last); /* Remove Ctrl-Z) */
	rm_lseek(handle, 0L, SEEK_SET);
    }
    if (ctrlz_kludge) {
	/* Reopen the handle in write-only mode. */

	rm_close(handle);
	flags = (flags & ~O_ACCMODE) | (oflag & O_ACCMODE);
	flags &= ~0x20000;	/* Remove O_EXCL */
	handle = do_open(name, flags);
	if (handle < 0)
	    return (-1);
    }
    _files[handle] = bits;
    _lookahead[handle] = -1;

    /*
     * When opening a file for appending, move to the end of the file. This
     * is required for passing the handle to a child process.
     */

    if (!(bits & F_DEV) && (bits & O_APPEND))
	rm_lseek(handle, 0L, SEEK_END);
    emx_errno = saved_errno;

    return (handle);
}

int dj_creat(char *buf) {
    return dj_sopen(buf, O_CREAT | O_WRONLY | O_TRUNC,
		     O_DENYNONE, S_IREAD | S_IWRITE);
}

int dj_open(char *buf, int flags) {
    return dj_sopen(buf, flags, O_DENYNONE, S_IREAD | S_IWRITE);
}

#define CTRL_Z 0x1a

int dj_write(int handle, void *buf, size_t nbyte)
{
    int out_cnt, lf_cnt, i, n, buf_cnt;
    char *src, *p;
    char tmp[1024];

    if (handle < 0 || handle >= _nfiles) {
	emx_errno = EMX_EBADF;
	return (-1);
    }
    if (_files[handle] & O_APPEND)
	rm_lseek(handle, 0L, SEEK_END);
    if (nbyte == 0)		/* Avoid truncation of file */
	return (0);
    src = buf;
    if (_files[handle] & O_TEXT) {
	out_cnt = lf_cnt = 0;
	buf_cnt = 0;
	p = (char *) my_memchr(src, '\n', nbyte);
	if (p == NULL)
	    goto write_bin;
	for (i = 0; i < (int) nbyte; ++i) {
	    if (src[i] == '\n') {
		if (buf_cnt >= sizeof(tmp))
		    do {
			n = rm_write(handle, tmp, buf_cnt);
			if (n == -1)
			    goto error;
			out_cnt += n;
			if (n != buf_cnt) {
			    emx_errno = EMX_ENOSPC;
			    return (-1);
			}
			buf_cnt = 0;
		    } while (0);
		tmp[buf_cnt++] = '\r';
		++lf_cnt;
	    }
	    if (buf_cnt >= sizeof(tmp))
		do {
		    n = rm_write(handle, tmp, buf_cnt);
		    if (n == -1)
			goto error;
		    out_cnt += n;
		    if (n != buf_cnt) {
			emx_errno = EMX_ENOSPC;
			return (-1);
		    }
		    buf_cnt = 0;
		} while (0);
	    tmp[buf_cnt++] = src[i];
	}
	if (buf_cnt != 0)
	    do {
		n = rm_write(handle, tmp, buf_cnt);
		if (n == -1)
		    goto error;
		out_cnt += n;
		if (n != buf_cnt) {
		    emx_errno = EMX_ENOSPC;
		    return (-1);
		}
		buf_cnt = 0;
	    } while (0);
	return (out_cnt - lf_cnt);
    }
  write_bin:
    n = rm_write(handle, src, nbyte);
    if (n == -1)
	goto error;
    if (n == 0 && !((_files[handle] & F_DEV) && *src == CTRL_Z)) {
	emx_errno = EMX_ENOSPC;
	return (-1);
    }
    return (n);

  error:
    return (-1);
}

static int read_lookahead(int handle, void *buf, size_t nbyte)
{
    int i, n, saved_errno;
    char *dst;

    i = 0;
    dst = buf;
    saved_errno = emx_errno;
    if (nbyte > 0 && _lookahead[handle] != -1) {
	*dst = (char) _lookahead[handle];
	_lookahead[handle] = -1;
	++i;
	--nbyte;
    }
    n = rm_read(handle, dst + i, nbyte);
    if (n == -1) {
	if (emx_errno == EMX_EAGAIN && i > 0) { /* lookahead and O_NDELAY */
	    emx_errno = saved_errno;/* hide EAGAIN */
	    return (i); 	/* and be successful */
	}
	return (-1);
    }
    return (i + n);
}

static long filelength(int handle)
{
    long cur, n;

    cur = rm_lseek(handle, 0L, SEEK_CUR);
    if (cur == -1L)
	return (-1L);
    n = rm_lseek(handle, 0L, SEEK_END);
    rm_lseek(handle, cur, SEEK_SET);
    return (n);
}

static long tell(int handle)
{
    long n;

    if (handle < 0 || handle >= _nfiles) {
	emx_errno = EMX_EBADF;
	return (-1L);
    }
    n = (long) rm_lseek(handle, 0L, SEEK_CUR);
    if (n == -1)
	return (n);
    if (_lookahead[handle] >= 0)
	--n;
    return (n);
}

static int _crlf(char *buf, size_t size, size_t * new_size)
{
    size_t src, dst;
    char *p;

    p = my_memchr(buf, '\r', size);     /* Avoid copying until CR reached */
    if (p == NULL) {		/* This is the trivial case */
	*new_size = size;
	return (0);
    }
    src = dst = p - buf;	/* Start copying here */
    while (src < size) {
	if (buf[src] == '\r') { /* CR? */
	    ++src;		/* Skip the CR */
	    if (src >= size) {	/* Is it the final char? */
		*new_size = dst;/* Yes -> don't include in new_size, */
		return (1);	/* notify caller	     */
	    }
	    if (buf[src] != '\n')       /* CR not followed by LF? */
		--src;		/* Yes -> copy the CR */
	}
	buf[dst++] = buf[src++];/* Copy a character */
    }
    *new_size = dst;
    return (0);
}

static int eof(int handle)
{
    long cur, len;

    if (handle < 0 || handle >= _nfiles) {
	emx_errno = EMX_EBADF;
	return (-1);
    }
    if (_files[handle] & F_EOF) /* Ctrl-Z reached */
	return (1);
    cur = tell(handle);
    if (cur < 0)
	return (-1);
    len = filelength(handle);
    if (len < 0)
	return (-1);
    return (cur == len);
}

int dj_read(int handle, void *buf, size_t nbyte)
{
    int n;
    size_t j, k;
    char *dst, c;

    if (handle < 0 || handle >= _nfiles) {
	emx_errno = EMX_EBADF;
	return (-1);
    }
    if (nbyte > 0 && (_files[handle] & F_EOF))
	return (0);
    dst = buf;
    n = read_lookahead(handle, dst, nbyte);
    if (n == -1)
	return (-1);
    if ((_files[handle] & O_TEXT) && !(_files[handle] & F_TERMIO) && n > 0) {
	/* special processing for text mode */
	if (!(_files[handle] & (F_NPIPE | F_DEV)) && dst[n - 1] == 0x1a &&
	    eof(handle)) {
	    /* remove last Ctrl-Z in text files */
	    --n;
	    _files[handle] |= F_EOF;
	    if (n == 0)
		return (0);
	}
	if (n == 1 && dst[0] == '\r') {
	    /* This is the tricky case as we are not allowed to decrement n  */
	    /* by one as 0 indicates end of file. We have to use look ahead. */
	    int saved_errno = emx_errno;
	    j = read_lookahead(handle, &c, 1);	/* look ahead */
	    if (j == -1 && emx_errno == EMX_EAGAIN) {
		_lookahead[handle] = dst[0];
		return (-1);
	    }
	    emx_errno = saved_errno;/* hide error */
	    if (j == 1 && c == '\n')    /* CR/LF ? */
		dst[0] = '\n';  /* yes -> replace with LF */
	    else
		_lookahead[handle] = c; /* no -> save the 2nd char */
	} else {
	    if (_crlf(dst, n, &k))	/* CR/LF conversion */
		_lookahead[handle] = '\r';      /* buffer ends with CR */
	    n = k;
	}
    }
    return (n);
}

int dj_setmode(int handle, int mode)
{
    int old_mode;

    if (handle < 0 || handle >= _nfiles) {
	emx_errno = EMX_EBADF;
	return (-1);
    }
    old_mode = ((_files[handle] & O_TEXT) ? O_TEXT : O_BINARY);
    if (mode == O_BINARY)
	_files[handle] &= ~O_TEXT;
    else if (mode == O_TEXT)
	_files[handle] |= O_TEXT;
    else {
	emx_errno = EMX_EINVAL;
	return (-1);
    }
    return (old_mode);
}

int dj_chmod(char *name, int pmode)
{
    WORD attr;

    if (rm_getfattr(name, &attr) == -1)
	return (-1);
    if (pmode & S_IWRITE)
	attr &= ~1;
    else
	attr |= 1;
    if (rm_setfattr(name, attr) == -1)
	return -1;
    return (0);
}

#else

#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DJO_RDONLY	  0x0001
#define DJO_WRONLY	  0x0002
#define DJO_RDWR	  0x0004
#define DJO_CREAT	  0x0100
#define DJO_TRUNC	  0x0200
#define DJO_EXCL	  0x0400
#define DJO_APPEND	  0x0800
#define DJO_TEXT	  0x4000
#define DJO_BINARY	  0x8000

static int convert_dj_attr(int attr)
{
    int newattr = 0;
    if (attr & DJO_RDONLY) newattr |= O_RDONLY;
    if (attr & DJO_WRONLY) newattr |= O_WRONLY;
    if (attr & DJO_RDWR  ) newattr |= O_RDWR  ;
    if (attr & DJO_CREAT ) newattr |= O_CREAT ;
    if (attr & DJO_TRUNC ) newattr |= O_TRUNC ;
    if (attr & DJO_EXCL  ) newattr |= O_EXCL  ;
    if (attr & DJO_APPEND) newattr |= O_APPEND;
    if (attr & DJO_TEXT  ) newattr |= O_TEXT  ;
    if (attr & DJO_BINARY) newattr |= O_BINARY;
    return newattr;
}

void djio_init(void) {};

void djio_dup_fileattr(int from, int to) {};

int dj_creat(char *buf)
{
    return creat(buf, S_IREAD | S_IWRITE);
}
int dj_open(char *buf, int flags)
{
    return open(buf, convert_dj_attr(flags), S_IREAD | S_IWRITE);
}
int dj_write(int handle, void *buf, size_t nbyte)
{
    return write(handle, buf, nbyte);
}
int dj_read(int handle, void *buf, size_t nbyte)
{
    return read(handle, buf, nbyte);
}
int dj_setmode(int handle, int mode)
{
    return setmode(handle, convert_dj_attr(mode));
}
int dj_chmod(char *name, int pmode)
{
    return chmod(name, pmode);
}
#endif
