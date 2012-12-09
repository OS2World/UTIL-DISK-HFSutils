/* Copyright    Rainer Schnitker 92,93 */

#define EMX_EPERM	    1
#define EMX_ENOENT	    2
#define EMX_ESRCH	    3
#define EMX_EINTR	    4
#define EMX_EIO 	    5
#define EMX_E2BIG	    7
#define EMX_ENOEXEC	    8
#define EMX_EBADF	    9
#define EMX_ECHILD	    10
#define EMX_EAGAIN	    11
#define EMX_ENOMEM	    12
#define EMX_EACCES	    13
#define EMX_EEXIST	    17
#define EMX_EXDEV	    18
#define EMX_ENOTDIR	    20
#define EMX_EISDIR	    21
#define EMX_EINVAL	    22
#define EMX_EMFILE	    24
#define EMX_ENOSPC	    28
#define EMX_ESPIPE	    29
#define EMX_EROFS	    30
#define EMX_EPIPE	    32
#define EMX_EDOM	    33
#define EMX_ERANGE	    34
#define EMX_EMSDOS	    37
#define EMX_ENAMETOOLONG    38

extern int _doserrno;

unsigned int doserror_to_errno(int);
unsigned int errno_djgpp(int);
