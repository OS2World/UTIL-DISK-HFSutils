/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 1997 Marcus Better
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
** Direct access file operations.
** These allow RSX programs to open a drive as if it were a file.
*/

#include <stdlib.h>

#include "copy32.h"
#include "dpmi.h"
#include "process.h"
#include "fs.h"
#include "rmlib.h"
#include "cdosx32.h"
#include "doserrno.h"
#include "dasdrm.h"

static long dasd_lseek(struct file *, long, int);
static ARGUSER dasd_read(struct file *, ARGUSER, ARGUSER);
static ARGUSER dasd_write(struct file *, ARGUSER, ARGUSER);
static void dasd_release(struct file *);

static long dasd_lseek(struct file *filp, long off, int origin)
{
    switch (origin) {
    case SEEK_SET:
	filp->f_offset = off;
	break;
    case SEEK_CUR:
	filp->f_offset += off;
	break;
    case SEEK_END:
	filp->f_offset = filp->f_info.dasd_i->bytes+off;
	break;
    }

    return filp->f_offset;
}

static ARGUSER dasd_read(struct file *filp, ARGUSER buf, ARGUSER count)
{
    long org_bytes, read_bytes, rc;

    org_bytes = count;

    /* first partial sector */
    if (filp->f_offset % SECTOR_SIZE) {
	int sector_offset = filp->f_offset % SECTOR_SIZE;
	
	read_bytes = SECTOR_SIZE - sector_offset;
	if (read_bytes > count)
	    read_bytes = count;

	rc = absolute_read(filp->f_info.dasd_i, filp->f_offset / SECTOR_SIZE, 
			   1, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	cpy16_32(DS, buf, iobuf+sector_offset, read_bytes);
	buf += read_bytes;
	filp->f_offset += read_bytes;
	count -= read_bytes;
    }

    /* rest of the sectors */
    while (count > 0) {
	int sectors;
	read_bytes = (count < IOBUF_SIZE) ? count : IOBUF_SIZE;

	sectors = read_bytes / SECTOR_SIZE;
	if (read_bytes % SECTOR_SIZE)
	    ++sectors;

	rc = absolute_read(filp->f_info.dasd_i, filp->f_offset/SECTOR_SIZE, 
			   sectors, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	cpy16_32(DS, buf, iobuf, read_bytes);
	buf += read_bytes;
	filp->f_offset += read_bytes;
	count -= read_bytes;
    }

    return org_bytes - count;
}

static ARGUSER dasd_write(struct file *filp, ARGUSER buf, ARGUSER count)
{
    long org_bytes, write_bytes, rc;

    org_bytes = count;

    /* first partial sector */
    if (filp->f_offset % SECTOR_SIZE) {
	int sector_offset = filp->f_offset % SECTOR_SIZE;
	int sector = filp->f_offset / SECTOR_SIZE;
	
	write_bytes = SECTOR_SIZE - sector_offset;
	if (write_bytes > count)
	    write_bytes = count;

	rc = absolute_read(filp->f_info.dasd_i, sector, 1, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	cpy32_16(DS, buf, iobuf+sector_offset, write_bytes);
	rc = absolute_write(filp->f_info.dasd_i, sector, 1, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	buf += write_bytes;
	filp->f_offset += write_bytes;
	count -= write_bytes;
    }

    /* rest of the sectors */
    while (count >= SECTOR_SIZE) {
	int sectors;
	write_bytes = (count < IOBUF_SIZE) ? count : IOBUF_SIZE;
	write_bytes -= write_bytes % SECTOR_SIZE;
	sectors = write_bytes / SECTOR_SIZE;

	cpy32_16(DS, buf, iobuf, write_bytes);
	rc = absolute_write(filp->f_info.dasd_i, filp->f_offset/SECTOR_SIZE, 
			    sectors, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	buf += write_bytes;
	filp->f_offset += write_bytes;
	count -= write_bytes;
    }

    /* last partial sector */
    if (count) {
	int sector = filp->f_offset / SECTOR_SIZE;
	
	rc = absolute_read(filp->f_info.dasd_i, sector, 1, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	cpy32_16(DS, buf, iobuf, count);
	rc = absolute_write(filp->f_info.dasd_i, sector, 1, iobuf);
	if (rc)
	    return -doserror_to_errno(rc);

	filp->f_offset += count;
	count = 0;
    }

    return org_bytes - count;
}

static void dasd_release(struct file *filp)
{
    free(filp->f_info.dasd_i);
}

struct file_operations dasd_fop = {
    dasd_lseek,
    dasd_read,
    dasd_write,
    NULL,
    NULL,
    NULL,
    dasd_release
};
