/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996, 1997 Robert Leslie
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
 *
 * $Id: low.h,v 1.10 1997/11/08 01:27:11 rob Exp $
 */

typedef struct {
  Integer	sbSig;		/* device signature (should be 0x4552) */
  Integer	sbBlkSize;	/* block size of the device (in bytes) */
  LongInt	sbBlkCount;	/* number of blocks on the device */
  Integer	sbDevType;	/* reserved */
  Integer	sbDevId;	/* reserved */
  LongInt	sbData;		/* reserved */
  Integer	sbDrvrCount;	/* number of driver descriptor entries */
  LongInt	ddBlock;	/* first driver's starting block */
  Integer	ddSize;		/* size of the driver, in 512-byte blocks */
  Integer	ddType;		/* driver operating system type (MacOS = 1) */
  Integer	ddPad[243];	/* additional drivers, if any */
} Block0;

# define HFS_DDR_SIGWORD	0x4552

typedef struct {
  Integer	pmSig;		/* partition signature (0x504d or 0x5453) */
  Integer	pmSigPad;	/* reserved */
  LongInt	pmMapBlkCnt;	/* number of blocks in partition map */
  LongInt	pmPyPartStart;	/* first physical block of partition */
  LongInt	pmPartBlkCnt;	/* number of blocks in partition */
  Char		pmPartName[33];	/* partition name */
  Char		pmParType[33];	/* partition type */
  LongInt	pmLgDataStart;	/* first logical block of data area */
  LongInt	pmDataCnt;	/* number of blocks in data area */
  LongInt	pmPartStatus;	/* partition status information */
  LongInt	pmLgBootStart;	/* first logical block of boot code */
  LongInt	pmBootSize;	/* size of boot code, in bytes */
  LongInt	pmBootAddr;	/* boot code load address */
  LongInt	pmBootAddr2;	/* reserved */
  LongInt	pmBootEntry;	/* boot code entry point */
  LongInt	pmBootEntry2;	/* reserved */
  LongInt	pmBootCksum;	/* boot code checksum */
  Char		pmProcessor[17];/* processor type */
  Integer	pmPad[188];	/* reserved */
} Partition;

# define HFS_PM_SIGWORD		0x504d
# define HFS_PM_SIGWORD_OLD	0x5453

typedef struct {
  Integer	bbID;		/* boot blocks signature */
  LongInt	bbEntry;	/* entry point to boot code */
  Integer	bbVersion;	/* boot blocks version number */
  Integer	bbPageFlags;	/* used internally */
  Str15		bbSysName;	/* System filename */
  Str15		bbShellName;	/* Finder filename */
  Str15		bbDbg1Name;	/* debugger filename */
  Str15		bbDbg2Name;	/* debugger filename */
  Str15		bbScreenName;	/* name of startup screen */
  Str15		bbHelloName;	/* name of startup program */
  Str15		bbScrapName;	/* name of system scrap file */
  Integer	bbCntFCBs;	/* number of FCBs to allocate */
  Integer	bbCntEvts;	/* number of event queue elements */
  LongInt	bb128KSHeap;	/* system heap size on 128K Mac */
  LongInt	bb256KSHeap;	/* used internally */
  LongInt	bbSysHeapSize;	/* system heap size on all machines */
  Integer	filler;		/* reserved */
  LongInt	bbSysHeapExtra;	/* additional system heap space */
  LongInt	bbSysHeapFract;	/* fraction of RAM for system heap */
} BootBlkHdr;

# define HFS_BB_SIGWORD		0x4c4b

# define HFS_BOOTCODE1LEN	(HFS_BLOCKSZ - 148)
# define HFS_BOOTCODE2LEN	HFS_BLOCKSZ

# define HFS_BOOTCODELEN	(HFS_BOOTCODE1LEN + HFS_BOOTCODE2LEN)

int l_getddr(hfsvol *, Block0 *);
int l_putddr(hfsvol *, const Block0 *);

int l_getpmentry(hfsvol *, Partition *, unsigned long);
int l_putpmentry(hfsvol *, const Partition *, unsigned long);

int l_getbb(hfsvol *, BootBlkHdr *, byte *);
int l_putbb(hfsvol *, const BootBlkHdr *, const byte *);

int l_getmdb(hfsvol *, MDB *, int);
int l_putmdb(hfsvol *, const MDB *, int);
