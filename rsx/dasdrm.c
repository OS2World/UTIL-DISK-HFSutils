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

#include "copy32.h"
#include "dpmi.h"
#include "process.h"
#include "fs.h"
#include "cdosx32.h"
#include "start32.h"
#include "dasdrm.h"

#define SET_SEG_OFF(pointer, seg, off) \
{ seg = (((unsigned) (pointer) & ~0xFFF) >> 4) + ds16real; \
							       off = (unsigned) (pointer) & 0xFFF; }

#define SET_SEG_OFF_64(pointer, seg, off) \
{ seg = (((unsigned) (pointer) & ~0xFFFF) >> 4) + ds16real; \
								off = (unsigned) (pointer) & 0xFFFF; }

#define LV_WORD *(WORD *)&

#pragma pack(1)
struct {
    char SpecFunc;
    unsigned short Head;
    unsigned short Cylinder;
    unsigned short FirstSector;
    unsigned short Sectors;
    unsigned short BufferOfs;
    unsigned short BufferSeg;
} rwblock;

struct {
    unsigned char SpecFunc;
    unsigned char DevType;
    unsigned short DevAttr;
    unsigned short Cylinders;
    unsigned char MediaType;
    unsigned short BytesPerSec;
    unsigned char SecPerClust;
    unsigned short ResSectors;
    unsigned char FATs;
    unsigned short RootDirEnts;
    unsigned short Sectors;
    unsigned char Media;
    unsigned short FATsecs;
    unsigned short SecPerTrack;
    unsigned short Heads;
    unsigned long HiddenSecs;
    unsigned long HugeSectors;
} deviceparams;

struct {
    unsigned long start_sector;
    unsigned short sectors;
    unsigned short buf_ofs;
    unsigned short buf_seg;
} diskio;

static int absolute_io(struct dasd_info *, unsigned long, 
		       unsigned short, void *, int);
static void call_rm_dos(TRANSLATION *);

/* Perform real-mode interrupt 21h call */
static void call_rm_dos(TRANSLATION *ts)
{
    ts->flags = 0x3200;
    SET_SEG_OFF_64(real_mode_stack, ts->ss, ts->sp);
    SimulateRMint(0x21, 0, 0, ts);
}

/* Return drive size (drive 1=A:, 2=B:, etc.) */
/* This function is not currently used anywhere. */
unsigned long get_drive_size(int drive)
{
    TRANSLATION ts;

    LV_WORD ts.eax = 0x3600;
    LV_WORD ts.edx = drive-1;
    ts.flags = 0x3200;
    SET_SEG_OFF_64(real_mode_stack, ts.ss, ts.sp);
    SimulateRMint(0x21, 0, 0, &ts);
    if (ts.flags & 1)
	return 0;

    return (ts.eax & 0xFFFF) * (ts.edx & 0xFFFF) * SECTOR_SIZE;
}

/* Fill in the dasd_i structure. */
int get_dasd_info(int drive, struct dasd_info *dasd_i)
{
    TRANSLATION tr;

    /* Get device parameters */

    LV_WORD tr.eax = 0x440d;
    LV_WORD tr.ebx = drive;
    LV_WORD tr.ecx = 0x0860;
    SET_SEG_OFF(&deviceparams, tr.ds, tr.edx);
    call_rm_dos(&tr);
    if (tr.flags & 1)
	return tr.eax & 0xFFFF;

    dasd_i->drive = drive;
    dasd_i->bytes = (deviceparams.BytesPerSec * deviceparams.SecPerTrack
		     * deviceparams.Heads * deviceparams.Cylinders);
    dasd_i->cyls = deviceparams.Cylinders;
    dasd_i->heads = deviceparams.Heads;
    dasd_i->secs = deviceparams.SecPerTrack;

    /* Detect presence of OS/2. This should be done once somewhere else. */
    LV_WORD tr.eax = 0x3000;
    call_rm_dos(&tr);
    if ((tr.eax & 0xff) >= 20)
      dasd_i->os2 = 1;
    else
      dasd_i->os2 = 0;
    
    return 0;
}

int absolute_read(struct dasd_info *dasd_i, unsigned long start_sector,
		  unsigned short sectors, void *buf)
{
    return absolute_io(dasd_i, start_sector, sectors, buf, 0);
}

int absolute_write(struct dasd_info *dasd_i, unsigned long start_sector,
		   unsigned short sectors, void *buf)
{
    return absolute_io(dasd_i, start_sector, sectors, buf, 1);
}

/* Transfer logical sectors.
   There are two ways to do this: the old method with interrupt 25h and 26h,
   or the newer Generic IOCtl call (interrupt 21h, AX=440Dh).

   Under OS/2 we use int 25h and 26h because Generic IOCtl seems to be
   broken. Under Warp 4, sectors 16 and higher (physical) on each track
   cannot be read.

   Otherwise, we use Generic IOCtl, because interrupts 25h and 26h seem
   to be broken in Windows 95.
*/
static int absolute_io(struct dasd_info *dasd_i, unsigned long start_sector,
		       unsigned short sectors, void *buf, int rwflag)
{
    TRANSLATION tr;

    if (dasd_i->os2) {
	diskio.start_sector = start_sector;
	diskio.sectors = sectors;
	SET_SEG_OFF(buf, diskio.buf_seg, diskio.buf_ofs);
	
	LV_WORD tr.eax = dasd_i->drive - 1;
	LV_WORD tr.ecx = 0xFFFF;
	SET_SEG_OFF(&diskio, tr.ds, tr.ebx);
	
	tr.flags = 0x3200;
	SET_SEG_OFF_64(real_mode_stack, tr.ss, tr.sp);
	
	if (rwflag == 0)
	    SimulateRMint(0x25, 0, 0, &tr);
	else
	    SimulateRMint(0x26, 0, 0, &tr);
	
	if (tr.flags & 1)
	    return tr.eax & 0xFFFF;
	
	return 0;
    }
    else {
	unsigned sectors_per_track;
	
	sectors_per_track = dasd_i->heads * dasd_i->secs;
	rwblock.Cylinder = start_sector / sectors_per_track;
	rwblock.Head = (start_sector % sectors_per_track) / dasd_i->secs;
	rwblock.FirstSector = start_sector % dasd_i->secs;
	rwblock.Sectors = sectors;
	SET_SEG_OFF(buf, rwblock.BufferSeg, rwblock.BufferOfs);

	LV_WORD tr.eax = 0x440d;
	LV_WORD tr.ebx = dasd_i->drive;
	LV_WORD tr.ecx = 0x0800;
	if (rwflag == 0)
	    LV_WORD tr.ecx += 0x61;
	else
	    LV_WORD tr.ecx += 0x41;
	SET_SEG_OFF(&rwblock, tr.ds, tr.edx);
	
	call_rm_dos(&tr);
	if (tr.flags & 1)
	    return tr.eax & 0xFFFF;
	
	return 0;
    }
}
