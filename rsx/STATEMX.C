/*****************************************************************************
 * FILE: statemx.c							     *
 *									     *
 * DESC:								     *
 *	- stat, fstat functions 					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "DPMI.H"
#include "RMLIB.H"
#include "TIMEDOS.H"
#include "STATEMX.H"
#include "DOSERRNO.H"

#define ACC(x) (((x)>>6)*0111)

static long ino = 0x10000;

int sys_stat(char *filename, struct stat * statbuf)
{
    struct find_t find;
    unsigned long gmt;

    if (filename == NULL)
	return -1;

    /* find : dir, system, hidden */
    if (rm_findfirst(filename, 0x16, &find) == -1) {
	/* error: must be a root dir  or illegal drive */
	char path[260];
	unsigned olddrive, newdrive;
	int ret;

	if (rm_get_exterror_code() == 0x53) /* fail on int24 */
	    return -1;

	olddrive = rm_getdrive();
	if (filename[1] == ':') {
	    newdrive = (unsigned) *filename;
	    if (newdrive >= 'a')
		newdrive -= 0x20;
	    if (newdrive < 'A' || newdrive > 'Z')
		return -1;
	    newdrive -= 'A';
	    if (newdrive != olddrive)
		rm_setdrive(newdrive);
	} else
	    newdrive = olddrive;
	rm_getcwd(0, path);
	ret = rm_chdir(filename);
	rm_chdir(path); 	/* restore path */
	if (newdrive != olddrive)	/* restore drive */
	    rm_setdrive(olddrive);
	if (ret == -1)
	    return -1;		/* no root, error */
	statbuf->st_mode = S_IFDIR | ACC(S_IREAD | S_IWRITE | S_IEXEC);
	statbuf->st_size = 0L;
	statbuf->st_dev = (long) newdrive;
	statbuf->st_attr = 0;
	gmt = 0;
    } else {
	struct file_time ft;

	statbuf->st_attr = (long) find.attrib;
	if (find.attrib & 0x10) {	/* directory */
	    statbuf->st_mode = S_IFDIR | ACC(S_IREAD | S_IWRITE | S_IEXEC);
	    statbuf->st_size = 0L;
	} else {
	    statbuf->st_mode = S_IFREG; /* file */
	    statbuf->st_mode |= ACC(S_IREAD);
	    if (!(find.attrib & 0x1))	/* test read only */
		statbuf->st_mode |= ACC(S_IWRITE);	/* Read-write access */
	    statbuf->st_size = (DWORD) find.size_hi << 16 | find.size_lo;
	}
	ft.ft_date = find.wr_date;
	ft.ft_time = find.wr_time;
	gmt = filetime2gmt(&ft);
	if (find.name[1] == ':')
	    statbuf->st_dev = (long) (find.name[0] - 'A');
	else
	    statbuf->st_dev = rm_getdrive();
    }
    /* all */
    statbuf->st_atime = gmt;
    statbuf->st_ctime = gmt;
    statbuf->st_mtime = gmt;
    statbuf->st_rdev = statbuf->st_dev;
    statbuf->st_uid = 0L;
    statbuf->st_gid = 0L;
    statbuf->st_ino = ino++;
    statbuf->st_nlink = 1L;
    statbuf->st_reserved = 0L;
    return 0;
}

int sys_fstat(int handle, struct stat * statbuf)
{
    struct file_time ft;
    unsigned long gmt;
    int info;

    if ((info = rm_ioctl_getattr(handle)) == -1)
	return -1;

    if (!(info & 0x80)) {	/* file */
	long oldpos;
	statbuf->st_mode = S_IFREG;
	oldpos = rm_lseek(handle, 0L, 1);
	statbuf->st_size = rm_lseek(handle, 0L, 2);
	rm_lseek(handle, oldpos, 0);
	statbuf->st_dev = 0;
    } else {			/* device */
	statbuf->st_mode = S_IFCHR;
	statbuf->st_size = 0L;
	statbuf->st_dev = (long) (info & 0x3F);
    }
    rm_getftime(handle, &ft.ft_date, &ft.ft_time);
    gmt = filetime2gmt(&ft);
    statbuf->st_atime = gmt;
    statbuf->st_ctime = gmt;
    statbuf->st_mtime = gmt;
    statbuf->st_mode |= ACC(S_IREAD | S_IWRITE);
    statbuf->st_uid = 0L;
    statbuf->st_gid = 0L;
    statbuf->st_rdev = statbuf->st_dev;
    statbuf->st_nlink = 1L;
    statbuf->st_ino = ino++;
    statbuf->st_attr = 0L;
    statbuf->st_reserved = 0L;
    return 0;
}
