/*****************************************************************************
 * FILE: doserrno.c							     *
 *									     *
 * DESC:								     *
 *	- msdos error no to EMX errno					     *
 *	- EMX errno to DJGPP errno					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "DOSERRNO.H"

#define N_DOS_ERRORS 0x5A

#define EMX_RESERVED	    EMX_EIO

static unsigned char dos_errors[N_DOS_ERRORS + 1] =
{
    0,				/* 0x00  no error							     */
    EMX_EINVAL,			/* 0x01  function number invalid			     */
    EMX_ENOENT,			/* 0x02  file not found 					     */
    EMX_ENOENT,			/* 0x03  path not found 					     */
    EMX_EMFILE,			/* 0x04  too many open files					     */
    EMX_EACCES,			/* 0x05  access denied						     */
    EMX_EBADF,			/* 0x06  invalid handle 					     */
    EMX_EIO,			/* 0x07  memory control block destroyed 	     */
    EMX_ENOMEM,			/* 0x08  insufficient memory					     */
    EMX_EIO,			/* 0x09  memory block address invalid			     */
    EMX_EINVAL,			/* 0x0A  environment invalid					     */
    EMX_ENOEXEC,		/* 0x0B  format invalid 					     */
    EMX_EINVAL,			/* 0x0C  access code invalid					     */
    EMX_EINVAL,			/* 0x0D  data invalid							     */
    EMX_RESERVED,		/* 0x0E  reserved							     */
    EMX_EACCES,			/* 0x0F  invalid drive						     */
    EMX_EACCES,			/* 0x10  attempted to remove current directory	 */
    EMX_EXDEV,			/* 0x11  not same device					     */
    EMX_ENOENT,			/* 0x12  no more files						     */
    EMX_EIO,			/* 0x13  disk write-protected					     */
    EMX_EIO,			/* 0x14  unknown unit							     */
    EMX_EIO,			/* 0x15  drive not ready					     */
    EMX_EIO,			/* 0x16  unknown command					     */
    EMX_EIO,			/* 0x17  data error							     */
    EMX_EIO,			/* 0x18  bad request structure length			     */
    EMX_EIO,			/* 0x19  seek error							     */
    EMX_EIO,			/* 0x1A  unknown media type					     */
    EMX_EIO,			/* 0x1B  sector not found					     */
    EMX_ENOSPC,			/* 0x1C  printer out of paper					     */
    EMX_EIO,			/* 0x1D  write fault							     */
    EMX_EIO,			/* 0x1E  read fault							     */
    EMX_EIO,			/* 0x1F  general failure					     */
    EMX_EACCES,			/* 0x20  sharing violation					     */
    EMX_EACCES,			/* 0x21  lock violation 					     */
    EMX_EIO,			/* 0x22  disk change invalid					     */
    EMX_EIO,			/* 0x23  FCB unavailable					     */
    EMX_EIO,			/* 0x24  sharing buffer overflow			     */
    EMX_EIO,			/* 0x25  code page mismatch					     */
    EMX_EIO,			/* 0x26  cannot complete file operation 	     */
    EMX_ENOSPC,			/* 0x27  insufficient disk space			     */
    EMX_RESERVED,		/* 0x28  reserved							     */
    EMX_RESERVED,		/* 0x29  reserved							     */
    EMX_RESERVED,		/* 0x2A  reserved							     */
    EMX_RESERVED,		/* 0x2B  reserved							     */
    EMX_RESERVED,		/* 0x2C  reserved							     */
    EMX_RESERVED,		/* 0x2D  reserved							     */
    EMX_RESERVED,		/* 0x2E  reserved							     */
    EMX_RESERVED,		/* 0x2F  reserved							     */
    EMX_RESERVED,		/* 0x30  reserved							     */
    EMX_RESERVED,		/* 0x31  reserved							     */
    EMX_EIO,			/* 0x32  network request not supported		     */
    EMX_EIO,			/* 0x33  remote computer not listening		     */
    EMX_EIO,			/* 0x34  duplicate name on network			     */
    EMX_EIO,			/* 0x35  network name not found 			     */
    EMX_EIO,			/* 0x36  network busy							     */
    EMX_EIO,			/* 0x37  network device no longer exists	     */
    EMX_EIO,			/* 0x38  network BIOS command limit exceeded	     */
    EMX_EIO,			/* 0x39  network adapter hardware error 	     */
    EMX_EIO,			/* 0x3A  incorrect response from network	     */
    EMX_EIO,			/* 0x3B  unexpected network error			     */
    EMX_EIO,			/* 0x3C  incompatible remote adapter			     */
    EMX_ENOSPC,			/* 0x3D  print queue full					     */
    EMX_ENOSPC,			/* 0x3E  queue not full 					     */
    EMX_EIO,			/* 0x3F  not enough space to print file 	     */
    EMX_EIO,			/* 0x40  network name was deleted			     */
    EMX_EACCES,			/* 0x41  network: Access denied 			     */
    EMX_EIO,			/* 0x42  network device type incorrect		     */
    EMX_EIO,			/* 0x43  network name not found 			     */
    EMX_EIO,			/* 0x44  network name limit exceeded			     */
    EMX_EIO,			/* 0x45  network BIOS session limit exceeded	     */
    EMX_EIO,			/* 0x46  temporarily paused					     */
    EMX_EIO,			/* 0x47  network request not accepted			     */
    EMX_EIO,			/* 0x48  network print/disk redirection paused	 */
    EMX_EACCES,			/* 0x49  invalid network version			     */
    EMX_EIO,			/* 0x4A  account expired					     */
    EMX_EIO,			/* 0x4B  password expired					     */
    EMX_EIO,			/* 0x4C  login attempt invalid at this time	     */
    EMX_EIO,			/* 0x4D  disk limit exceeded on network node	     */
    EMX_EIO,			/* 0x4E  not logged in to network node		     */
    EMX_RESERVED,		/* 0x4F  reserved							     */
    EMX_EEXIST,			/* 0x50  file exists							     */
    EMX_RESERVED,		/* 0x51  reserved							     */
    EMX_ENOENT,			/* 0x52  cannot make directory				     */
    EMX_EIO,			/* 0x53  fail on INT 24h					     */
    EMX_EIO,			/* 0x54  too many redirections				     */
    EMX_EIO,			/* 0x55  duplicate redirection				     */
    EMX_EIO,			/* 0x56  invalid password					     */
    EMX_EINVAL,			/* 0x57  invalid parameter					     */
    EMX_EIO,			/* 0x58  network write fault					     */
    EMX_EIO,			/* 0x59  function not supported on network	     */
    EMX_EIO			/* 0x5A  required system component not installed */
};

static unsigned char emx2djgpp[] =
{
    0,
    5,				/* 1  EPERM	 Operation not permitted	      */
    2,				/* 2  ENOENT	 No such file or directory	      */
    19,				/* 3  ESRCH	 No such process		      */
    100,			/* 4  EINTR	 Interrupted system call	      */
    101,			/* 5  EIO 	 I/O error			      */
    101,
    10,				/* 7  E2BIG	 Arguments or environment too big     */
    21,				/* 8  ENOEXEC	 Invalid executable file format       */
    6,				/* 9  EBADF	 Bad file number		      */
    200,			/* 10 ECHILD	 No child processes		      */
    103,			/* 11 EAGAIN	 No more processes		      */
    8,				/* 12 ENOMEM	 Not enough memory		      */
    5,				/* 13 EACCES	 Permission denied		      */
    101,
    101,
    101,
    36,				/* 17 EEXIST	 File exists			      */
    17,				/* 18 EXDEV	 Cross-device link		      */
    101,
    3,				/* 20 ENOTDIR	 Not a directory		      */
    19,				/* 21 EISDIR	 Is a directory 		      */
    19,				/* 22 EINVAL	 Invalid argument		      */
    101,
    4,				/* 24 EMFILE	 Too many open files		      */
    101,
    101,
    101,
    102,			/* 28 ENOSPC	 Disk full			      */
    19,				/* 29 ESPIPE	 Illegal seek			      */
    5,				/* 30 EROFS	 Read-only file system		      */
    101,
    32,				/* 32 EPIPE	 Broken pipe			      */
    33,				/* 33 EDOM	 Domain error			      */
    34,				/* 34 ERANGE	 Result too large		      */
    101,
    101,
    5,				/* 37 EMSDOS	 Not supported under MS-DOS	      */
    19				/* 38 ENAMETOOLONG File name too long		      */
};

unsigned int doserror_to_errno(int error_dos)
{
    if (error_dos >= N_DOS_ERRORS)
	return EMX_EIO;
    else
	return (unsigned) dos_errors[error_dos];
}

unsigned int errno_djgpp(int emx_errno)
{
    if (emx_errno > 38)
	emx_errno = EMX_EIO;
    return (unsigned) emx2djgpp[emx_errno];
}
