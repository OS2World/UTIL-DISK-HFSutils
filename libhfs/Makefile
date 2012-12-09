###############################################################################
#
# hfsutils - tools for reading and writing Macintosh HFS volumes
# Copyright (C) 1997 Marcus Better
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
###############################################################################

EXTRA_CFLAGS = -DHAVE_CONFIG_H

ARFLAGS =	rcs

###############################################################################

NAME =		libhfs
TARGET =	$(NAME).a
OBJS =		block btree data file hfs low medium node record unicode \
		volume version emx
OBJS :=         $(addsuffix .o,$(OBJS))

###############################################################################

include ../Rules.make

all :: $(TARGET)

clean ::
	-del *.o 2> NUL:
	-del $(TARGET) 2> NUL:
	-del *~ 2> NUL:

###############################################################################

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

### DEPENDENCIES FOLLOW #######################################################

block.o: block.c config.h libhfs.h hfs.h block.h data.h os.h
btree.o: btree.c config.h libhfs.h hfs.h btree.h data.h file.h block.h \
 node.h
data.o: data.c config.h data.h
file.o: file.c config.h libhfs.h hfs.h file.h btree.h record.h \
 volume.h
hfs.o: hfs.c config.h libhfs.h hfs.h data.h block.h low.h medium.h \
 file.h btree.h node.h record.h volume.h
low.o: low.c config.h libhfs.h hfs.h low.h data.h block.h file.h
medium.o: medium.c config.h libhfs.h hfs.h block.h low.h medium.h
memcmp.o: memcmp.c config.h
node.o: node.c config.h libhfs.h hfs.h node.h data.h btree.h
os.o: os.c config.h libhfs.h hfs.h os.h
record.o: record.c config.h libhfs.h hfs.h record.h data.h
unicode.o: unicode.c config.h libhfs.h hfs.h sequoia.h unicode.h
version.o: version.c version.h
volume.o: volume.c config.h libhfs.h hfs.h volume.h data.h block.h \
 low.h medium.h file.h btree.h record.h os.h
