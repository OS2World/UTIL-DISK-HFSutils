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
 * $Id: hformat.c,v 1.12 1997/11/11 21:27:48 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "suid.h"
# include "hformat.h"

# define O_FORCE	0x01

extern char *optarg;
extern int optind;

/*
 * NAME:	usage()
 * DESCRIPTION:	display usage message
 */
static
int usage(void)
{
  fprintf(stderr, "Usage: %s [-f] [-l label] path [partition-no]\n", argv0);

  return 1;
}

/*
 * NAME:	do_format()
 * DESCRIPTION:	call hfs_format() with necessary privileges
 */
static
hfsvol *do_format(const char *path, int partno, const char *vname)
{
  hfsvol *vol = 0;

  suid_enable();

  if (hfs_format(path, partno, vname, 0, 0) != -1)
    vol = hfs_mount(path, partno, HFS_MODE_ANY);

  suid_disable();

  return vol;
}

/*
 * NAME:	hformat->main()
 * DESCRIPTION:	implement hformat command
 */
int hformat_main(int argc, char *argv[])
{
  const char *vname, *path;
  hfsvol *vol;
  hfsvolent ent;
  int nparts, partno, options = 0, result = 0;

  vname = "Untitled";

  while (1)
    {
      int opt;

      opt = getopt(argc, argv, "fl:");
      if (opt == EOF)
	break;

      switch (opt)
	{
	case '?':
	  return usage();

	case 'f':
	  options |= O_FORCE;
	  break;

	case 'l':
	  vname = optarg;
	  break;
	}
    }

  if (argc - optind < 1 || argc - optind > 2)
    return usage();

  path = argv[optind];

  nparts = hfs_nparts(path);

  if (argc - optind == 2)
    {
      partno = atoi(argv[optind + 1]);

      if (nparts != -1 && partno == 0)
	{
	  if (options & O_FORCE)
	    {
	      fprintf(stderr,
		      "%s: warning: erasing partition information\n", argv0);
	    }
	  else
	    {
	      fprintf(stderr, "%s: medium is partitioned; "
		      "select partition > 0 or use -f\n", argv0);
	      return 1;
	    }
	}
    }
  else
    {
      if (nparts > 1)
	{
	  fprintf(stderr,
		  "%s: must specify partition number (%d available)\n",
		  argv0, nparts);
	  return 1;
	}
      else if (nparts == -1)
	partno = 0;
      else
	partno = 1;
    }

  vol = do_format(path, partno, vname);
  if (vol == 0)
    {
      hfsutil_perror(path);
      return 1;
    }

  hfs_vstat(vol, &ent);
  hfsutil_pinfo(&ent);

  if (hcwd_mounted(ent.name, ent.crdate, path, partno) == -1)
    {
      perror("Failed to record mount");
      result = 1;
    }

  hfsutil_unmount(vol, &result);

  return result;
}
