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


INFFILE = $(NAME).inf
TXTFILE = $(NAME).txt
README  = README
ifdef WIN32
README_SRC = readme.win.src
else
README_SRC = readme.os2.src
endif

include ../Rules.make

all:: doc

clean::
	-del $(INFFILE) 2> NUL:
	-del $(TXTFILE) 2> NUL:
	-del $(README) 2> NUL:
	-del $(NAME).ipf 2> NUL:
	-del *~ 2> NUL:

doc: $(TXTFILE) $(INFFILE) $(README)

.PRECIOUS:: $(NAME).ipf

$(README): $(README_SRC)
	$(EMXDOC) -t -o $@ $<
