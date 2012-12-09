/*****************************************************************************
 * FILE: version.c                                                           *
 *									     *
 * DESC:								     *
 *      - prints version                                                     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *                                                                           *
 *****************************************************************************/

#include "printf.h"
#include "version.h"

static char version[] =
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License version 2 as\n"
"published by the Free Software Foundation.\n"
"\n"
"RSXHFS (32-bit "
            RSX_VERSION
            ") DPMI DOS extender for emx and rsxnt programs.\n"
"Copyright (c) Rainer Schnitker 1993-1996. All rights reserved.\n"
"Modified for hfsutils by Marcus Better\n";

void print_version(void)
{
    puts(version);
}
