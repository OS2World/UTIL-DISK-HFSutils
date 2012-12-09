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
 * $Id: version.c,v 1.10 1997/11/11 23:13:30 rob Exp $
 */

/* Modified for emx by Marcus Better, November 1997 */

# include "version.h"

char hfsutils_rcsid[] = "$Id: version.c,v 1.10 1997/11/11 23:13:30 rob Exp $";

#ifdef __EMX__
char hfsutils_version[]   = "hfsutils for DOS, Windows and OS/2 version 3.1a\n";
char hfsutils_copyright[] = "Copyright (C) 1996, 1997 Robert Leslie\n" \
                            "Copyright (C) 1997 Marcus Better";
char hfsutils_author[]    = "Robert Leslie <rob@mars.org>";
#else
char hfsutils_version[]   = "hfsutils version 3.1";
char hfsutils_copyright[] = "Copyright (C) 1996, 1997 Robert Leslie";
char hfsutils_author[]    = "Robert Leslie <rob@mars.org>";
#endif

char hfsutils_license[] =
  "This program is free software; you can redistribute it and/or modify it\n"
  "under the terms of the GNU General Public License as published by the\n"
  "Free Software Foundation; either version 2 of the License, or (at your\n"
  "option) any later version.\n\n"

  "This program is distributed in the hope that it will be useful, but\n"
  "WITHOUT ANY WARRANTY; without even the implied warranty of\n"
  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
  "General Public License for more details.\n\n"

  "You should have received a copy of the GNU General Public License along\n"
  "with this program; if not, write to the Free Software Foundation, Inc.,\n"
  "675 Mass Ave, Cambridge, MA 02139, USA.\n\n";
