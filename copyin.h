/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
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
 * $Id: copyin.h,v 1.4 1997/08/25 03:53:09 rob Exp $
 */

extern const char *cpi_error;

typedef int (*cpifunc)(const char *, hfsvol *, const char *);

int cpi_macb(const char *, hfsvol *, const char *);
int cpi_binh(const char *, hfsvol *, const char *);
int cpi_text(const char *, hfsvol *, const char *);
int cpi_raw(const char *, hfsvol *, const char *);
