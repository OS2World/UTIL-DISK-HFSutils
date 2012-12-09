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

#define SECTOR_SIZE 512

unsigned long get_drive_size(int);
int get_dasd_info(int, struct dasd_info *);
int absolute_read(struct dasd_info *, unsigned long,  unsigned short, void *);
int absolute_write(struct dasd_info *, unsigned long,  unsigned short, void *);

