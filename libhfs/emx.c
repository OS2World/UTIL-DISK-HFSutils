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

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libhfs.h"
#include "os.h"

#define CD01 0x31304443L
#define HSG_ADDRESS 0
#define CDROM_SECTOR_SIZE       2048
#define CDROM_LONG_SECTOR_SIZE  2352

#pragma pack(1)
struct ReadLong {
  ULONG  ID_code;
  UCHAR  address_mode;
  USHORT transfer_count;
  ULONG  start_sector;
  UCHAR  reserved;
  UCHAR  interleave_size;
  UCHAR  interleave_skip_factor;
};

/*
 * NAME:	os->open()
 * DESCRIPTION:	open and lock a new descriptor from the given path and mode
 */
int os_open(void **priv, const char *path, int mode)
{
  int fd;
  char new_path[10];
  
  switch (mode)
    {
    case HFS_MODE_RDONLY:
      mode = O_RDONLY;
      break;

    case HFS_MODE_RDWR:
    default:
      mode = O_RDWR;
      break;
    }
  mode |= O_BINARY;

  /* Handle drive specifications for Win32 */
  if(_emx_env & 0x1000) { /* Check if running under rsx */
    if(_emx_rev >> 16 == 2) { /* Check for RSXNT */
      if(isalpha(path[0]) && path[1]==':' && path[2]==0) {
	strcpy(new_path, "\\\\.\\");
	strcat(new_path, path);
	path = new_path;
      }
    }
  }

  /* If path is a drive letter and we are under OS/2, 
     open in DASD mode and use _imphandle() to convert the handle to emx.
     */
  if(_emx_env & 0x0200 
     && isalpha(path[0]) && path[1]==':' && path[2]==0) {
    HFILE hf;
    ULONG action, open_mode;
    APIRET rc;
    int handle;

    open_mode = OPEN_FLAGS_DASD | OPEN_SHARE_DENYREADWRITE;
    if (mode & O_WRONLY || mode & O_RDWR)
      open_mode |= OPEN_ACCESS_READWRITE;
    else
      open_mode |= OPEN_ACCESS_READONLY;
    rc = DosOpen(path, &hf, &action, 0, 0, 
		 OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
		 open_mode, NULL);
    if (rc != NO_ERROR)
      ERROR(EACCES, "error opening medium");
    
    /* If opening for write, lock the drive. */
    if((open_mode & OPEN_ACCESS_WRITEONLY) |
       (open_mode & OPEN_ACCESS_READWRITE)) {
      char lock_param[1] = {0}, lock_data[1] = {0};
      ULONG paramlen=1, datalen=1;
      
      rc=DosDevIOCtl(hf, IOCTL_DISK, DSK_LOCKDRIVE,
		     lock_param, 1, &paramlen,
		     lock_data, 1, &datalen);
      if(rc != NO_ERROR) {
	DosClose(hf);
	ERROR(EACCES, "error opening medium");
      }
    }

    handle = _imphandle(hf);

    if (setmode(handle, O_BINARY) < 0)
      ERROR(errno, "error opening medium");

    fd = handle;
  }
  else
    fd = open(path, mode);
    /* here we should lock the descriptor against concurrent access */

  if (fd == -1) {
    hfs_error = "error opening medium";
    return -1;
  }

  *priv = (void *) fd;

  return 0;
fail:
  return -1;
}

/*
 * NAME:	os->close()
 * DESCRIPTION:	close an open descriptor
 */
int os_close(void **priv)
{
  int fd = (int) *priv;

  *priv = (void *) -1;

  /* If we are under OS/2, check if it is a DASD handle. If yes, then unlock
     the drive first.
     */
  if (_emx_env & 0x0200) {
    ULONG rc, open_mode;

    rc = DosQueryFHState(fd, &open_mode);
    if(rc == NO_ERROR) {
      if(open_mode & OPEN_FLAGS_DASD &&
	 ((open_mode & OPEN_ACCESS_WRITEONLY) |
	  (open_mode & OPEN_ACCESS_READWRITE))) {
	char lock_param[1] = {0}, lock_data[1] = {0};
	ULONG paramlen=1, datalen=1;
	
	DosDevIOCtl(fd, IOCTL_DISK, DSK_UNLOCKDRIVE,
		    lock_param, 1, &paramlen,
		    lock_data, 1, &datalen);
      }
    }
    else 
      ERROR(EACCES, "error closing medium");
  }

  if (close(fd) == -1)
    ERROR(errno, "error closing medium");

  return 0;

fail:
  return -1;
}

/*
 * NAME:	os->same()
 * DESCRIPTION:	return 1 iff path is same as the open descriptor
 */
int os_same(void **priv, const char *path)
{
  int fd = (int) *priv;
  struct stat fdev, dev;

  if (fstat(fd, &fdev) == -1 ||
      stat(path, &dev) == -1)
    ERROR(errno, "can't get path information");

  return fdev.st_dev == dev.st_dev &&
         fdev.st_ino == dev.st_ino;
fail:
  return -1;
}

/*
 * NAME:	os->seek()
 * DESCRIPTION:	set a descriptor's seek pointer
 */
long os_seek(void **priv, long offset, int from)
{
  long result;

  switch (from)
    {
    case HFS_SEEK_CUR:
      from = SEEK_CUR;
      break;

    case HFS_SEEK_END:
      from = SEEK_END;
      break;

    case HFS_SEEK_SET:
    default:
      from = SEEK_SET;
      break;
    }

  result = lseek((int) *priv, offset, from);

  if (result == -1)
    ERROR(errno, "error seeking medium");

  return result;
fail:
  return -1;
}

/*
 * NAME:	os->read()
 * DESCRIPTION:	read bytes from an open descriptor
 */
long os_read(void **priv, void *buf, unsigned long len)
{
  long result;
  int fd = (int)*priv;

  /* Some hybrid CD-ROMs have incorrect volume size, and the HFS part
     cannot be accessed with normal file operations. On OS/2, we use
     IOCTLs to read raw sectors instead. */
  if(_emx_env & 0x0200) {
    ULONG param, data, plen, dlen;
    APIRET rc;

    /* Check for CDROM */
    param = CD01;
    data = 0;
    plen  = sizeof(ULONG);
    dlen  = sizeof(ULONG);

    rc = DosDevIOCtl(fd, IOCTL_CDROMDISK, CDROMDISK_GETDRIVER,
		     &param, plen, &plen,
		     &data, dlen, &dlen);
    if(data != CD01)
      result = read(fd, buf, len);
    else {
      /* NOTE: This code assumes that libhfs reads whole blocks only. */

      UCHAR buffer[CDROM_LONG_SECTOR_SIZE];
      struct ReadLong pp;
      long pos;
      unsigned long bytes;

      pos = tell(fd);
      if (pos < 0)
	ERROR(errno, "error reading from medium");

      pp.ID_code                = CD01;
      pp.address_mode           = HSG_ADDRESS;
      pp.transfer_count         = 1;             /* 1 sector @ 2352 bytes */
      pp.reserved               = 0;
      pp.interleave_size        = 0;
      pp.interleave_skip_factor = 0;

      for (bytes = len; bytes > 0; ) {
	unsigned long count;

	pp.start_sector = pos / CDROM_SECTOR_SIZE;
	plen = sizeof(struct ReadLong);
	dlen = sizeof(buffer);
	rc = DosDevIOCtl(fd, IOCTL_CDROMDISK, CDROMDISK_READLONG,
			 &pp, plen, &plen, buffer, dlen, &dlen);
	if (rc != NO_ERROR || dlen != sizeof(buffer))
	  ERROR(EIO, "error reading from medium");

	count = CDROM_SECTOR_SIZE - pos % CDROM_SECTOR_SIZE;
	if (count > bytes)
	  count = bytes;

	memcpy(buf, buffer + 16 + pos % CDROM_SECTOR_SIZE, count);
	buf += count;
	pos += count;
	bytes -= count;
	os_seek(priv, pos, HFS_SEEK_SET);
      }	
      result = len;
    }
  }
  else
    result = read(fd, buf, len);

  if (result == -1)
    ERROR(errno, "error reading from medium");

  return result;
fail:
  return -1;
}

/*
 * NAME:	os->write()
 * DESCRIPTION:	write bytes to an open descriptor
 */
long os_write(void **priv, const void *buf, unsigned long len)
{
  long result;

  result = write((int) *priv, buf, len);

  if (result == -1)
    ERROR(errno, "error writing to medium");

  return result;
fail:
  return -1;
}
