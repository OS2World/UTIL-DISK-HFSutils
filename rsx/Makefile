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

VPATH = asm32

override AS = gcc.exe -x assembler-with-cpp -Zc++-comments
EXTRA_ASFLAGS = -Wall -g -I$(SOURCEASM)
EXTRA_CFLAGS = -I.

BLD=build
SOURCEASM=asm32

$(BLD)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

OBJS =	$(addprefix $(BLD)/, \
	rsx.o \
	start32.o \
	process.o \
	signals.o \
	ptrace.o \
	termio.o \
	doserrno.o \
	sysemx.o \
	sysdj.o \
	cdosx32.o \
	loadprg.o \
	timedos.o \
	rmlib.o \
	statemx.o \
	djio.o \
	sysdep2.o \
	fs.o \
        intrm.o \
        version.o \
	dasd.o \
	dasdrm.o)

KDEB  = $(BLD)/kdeb.o

CRT0  = $(BLD)/crt0.o

AOBJS = $(addprefix $(BLD)/, \
	dpmi.o \
	dpmi10.o \
	excep32.o \
	adosx32.o \
	copy32.o \
	callback.o)

LIBOBJS = $(addprefix $(BLD)/,printf.o libc.o)

include ../Rules.make

all:: $(BLD)/RSX.EMX $(BLD)/RSX.EXE 

clean::
	-del /N build\* 2> NUL:
	-del *~ 2> NUL:

$(BLD)/rsx.emx : $(CRT0) $(AOBJS) $(OBJS) $(LIBOBJS)
	ld -o $@ $(CRT0) @ofiles.rsp 

$(BLD)/rsxd.emx : $(CRT0) $(AOBJS) $(OBJS) $(LIBOBJS)
	ld -o $@ $(CRT0) @ofiles.rsp 

$(BLD)/rsx.exe: $(BLD)/rsx.emx loader\loader.exe
	strip $(BLD)/rsx.emx
	COPY /B LOADER\LOADER.EXE + $(BLD)\RSX.EMX $(BLD)\rsx.exe


#
# ASM-files in asm32 directory
#
$(BLD)/dpmi10.o:   dpmi10.s
	$(AS) -c $(EXTRA_ASFLAGS) -o $@ $^
$(BLD)/excep32.o:  excep32.s
	$(AS) -c $(EXTRA_ASFLAGS) -o $@ $^
$(BLD)/adosx32.o:  adosx32.s
	$(AS) -c $(EXTRA_ASFLAGS) -o $@ $^
$(BLD)/copy32.o:   copy32.s
	$(AS) -c $(EXTRA_ASFLAGS) -o $@ $^
$(BLD)/callback.o: callback.s
	$(AS) -c $(EXTRA_ASFLAGS) -o $@ $^
# startup file
$(BLD)/crt0.o: $(SOURCEASM)/crt0.s
	$(AS) -c $(EXTRA_ASFLAGS) -o $@ $^ 

#
# C-files in asm32 directory
#
$(BLD)/dpmi.o: dpmi.c
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) $< -o $@ -I$(SOURCEASM)

# version
version.o: version.c version.h
sysemx.o: sysemx.c version.h

