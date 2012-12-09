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

### PROJECT INFORMATION #######################################################

export NAME = hfsutils
export VERMAJOR =	3
export VERMINOR =	1
export RELEASE =	a

# Define WIN32 to build the Win32 version.
export WIN32=YES

# Define DEBUG to get debug information.
#export DEBUG=1

# Define to build Tcl/Tk programs.
#WITH_TCLTK=YES

### PATHS and other local settings ############################################

# Path to Tcl/Tk include files 
TCLINCLUDES=/prog/tcltk/include

# Path to Tcl/Tk DLL files
ifdef WIN32
  TCLDLL_DIR=tcllibs
else
  TCLDLL_DIR=/dll
endif

# Tcl/Tk version (i.e., "76" for "7.6")
TCL_VERSION = 76
TK_VERSION = 42

### TRANSLATOR DEFINITIONS ####################################################

export CC		= gcc
export AR		= ar
export EMXDOC		= emxdoc
export IPFC		= /prog/ddk/tools/ipfc
export HARDLINK		= copy
export EMXBIND		= emxbind
export NTBIND		= ntbind
export MAKELIB		= makelib
export SED		= /tools/bin/sed

ifdef WIN32
  CMD_EXT = bat
else
  CMD_EXT = cmd
endif

#
# Options
#

ifdef DEBUG
 export COPTS=-Wall -g
 export DEFINES+=-DDEBUG
else
 export COPTS=-Wall -O2
 export EMXBINDOPT=-s
 export NTBINDOPT=-strip
endif

INCLUDES=-Ilibhfs -I$(TCLINCLUDES)
LIBS=-llibhfs

TCLLIB=tcl$(TCL_VERSION)
TKLIB=tk$(TK_VERSION)
TCLDLL=$(TCLDLL_DIR)/$(TCLLIB).dll
TKDLL=$(TCLDLL_DIR)/$(TKLIB).dll

TCLLIB_A=$(TCLLIB).a
TKLIB_A=$(TKLIB).a

export CFLAGS=$(COPTS) $(DEFINES)
EXTRA_CFLAGS = $(INCLUDES) -DHAVE_CONFIG_H
EXTRA_LDFLAGS+=-Llibhfs -L$(BINDIR)

ifdef WIN32
 export CFLAGS+=-Zrsx32
 export LDFLAGS+=-Zrsx32
 WIN32_LDFLAGS = -Zwin32
 BINDIR=bin/win32
else
 BINDIR=bin/os2
endif

### DIRECTORIES ###############################################################

DOCDIR =	doc
LIBHFS_DIR =	libhfs
RSXDIR =	rsx

SUB_DIRS = $(LIBHFS_DIR) $(RSXDIR) $(DOCDIR)

### TARGETS ###################################################################

CLITARGETS_BASE = hattrib hcd hcopy hdel hdir hformat hls hmkdir \
		  hmount hpwd hrename hrmdir humount hvol
CLITARGETS =	$(addprefix $(BINDIR)/,$(addsuffix .exe,$(CLITARGETS_BASE)))

TCLTARGETS =	$(BINDIR)/hfssh.exe
TKTARGETS =	$(BINDIR)/xhfs.exe

LIBHFS =	$(LIBHFS_DIR)/libhfs.a

RSX_EXE =	$(RSXDIR)/build/rsx.exe
EMXL =		$(RSXDIR)/stub/emxldpmi.exe
RSXHFS =	$(BINDIR)/rsxhfs.exe

HFSUTIL_AOUT =  $(BINDIR)/hfsutil
HFSUTIL =	$(BINDIR)/hfsutil.exe
SPAWN_AOUT =	$(BINDIR)/spawn
SPAWN =		$(BINDIR)/spawn.exe

CLIOBJS =	$(addprefix $(BINDIR)/, hattrib.o hcd.o hcopy.o hdel.o \
		hformat.o hls.o hmkdir.o  \
		hmount.o hpwd.o hrename.o hrmdir.o humount.o hvol.o)
UTILOBJS =	$(addprefix $(BINDIR)/, crc.o binhex.o charset.o copyin.o \
		copyout.o darray.o dlist.o dstring.o glob.o suid.o version.o)
TCLOBJS =       $(addprefix $(BINDIR)/, hfssh.o tclhfs.o)
TKOBJS =	$(addprefix $(BINDIR)/, hfswish.o xhfs.o tclhfs.o)

### RULES #####################################################################

include Rules.make

all :: $(CLITARGETS) $(HFSUTIL) $(RSXHFS)

ifdef WIN32
ifdef WITH_TCLTK
all :: $(TCLTARGETS) $(TKTARGETS)
endif
endif

tcl: $(TCLTARGETS)

tk: $(TKTARGETS)

.PHONY: doc
doc:
	 $(MAKE) -C $(DOCDIR)

clean ::
	-del /N $(subst /,\,$(BINDIR)\*) 2> NUL:

###############################################################################

$(LIBHFS):
	$(MAKE) -C $(LIBHFS_DIR)

$(BINDIR)/%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

$(HFSUTIL_AOUT): $(LIBHFS) $(BINDIR)/hfsutil.o $(BINDIR)/hcwd.o $(CLIOBJS) $(UTILOBJS)
	$(CC) $(LDFLAGS) $(EXTRA_LDFLAGS) $(BINDIR)/hfsutil.o $(BINDIR)/hcwd.o \
		$(CLIOBJS) $(UTILOBJS)  \
		$(LIBS) -o $@

$(HFSUTIL): $(HFSUTIL_AOUT) $(EMXL)
ifdef WIN32
	$(NTBIND) $(NTBINDOPT) -s $(EMXL) $<
else
	$(EMXBIND) -b $(EMXBINDOPT) $(EMXL) $<
endif

$(SPAWN_AOUT): spawn.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $^

$(SPAWN): $(SPAWN_AOUT) $(EMXL)
ifdef WIN32
	$(NTBIND) $(NTBINDOPT) -s $(EMXL) $<
else
	$(EMXBIND) -b $(EMXBINDOPT) $(EMXL) $<
endif

$(CLITARGETS): $(SPAWN)
	$(HARDLINK) $(subst /,\,$(SPAWN) $@)

$(RSXHFS): $(RSX_EXE)
	copy $(subst /,\,$(RSX_EXE) $(RSXHFS))

ifdef WIN32
$(BINDIR)/$(TCLLIB_A): $(TCLDLL)
	$(MAKELIB) $(TCLDLL) -o $@

$(BINDIR)/$(TKLIB_A): $(TKDLL)
	$(MAKELIB) $(TKDLL) -o $@
endif

$(BINDIR)/hfssh.exe: $(LIBHFS) $(UTILOBJS) $(TCLOBJS) $(BINDIR)/$(TCLLIB_A)
ifdef WIN32
	$(CC) $(WIN32_LDFLAGS) $(EXTRA_LDFLAGS) \
		$(TCLOBJS) $(UTILOBJS)  \
		$(LIBS) -l$(TCLLIB) -o $@
else
	$(CC) $(LDFLAGS) $(EXTRA_LDFLAGS) \
		$(TCLOBJS) $(UTILOBJS)  \
		$(LIBS) -l$(TCLLIB) -o $@
endif

xhfs.c: mkxhfs.sed xhfs.tcl
	echo # include "xhfs.h" > $@
	echo char xhfs[] = \>> $@
	$(SED) -n -f $^
	copy $@ + tmp.sed $@
	echo ""; >> $@
	del tmp.sed

$(BINDIR)/xhfs.exe: $(LIBHFS) $(TKOBJS) $(UTILOBJS)
ifdef WIN32
	$(CC) $(WIN32_LDFLAGS) $(EXTRA_LDFLAGS) \
		$(TKOBJS) $(UTILOBJS)  \
		$(LIBS) -l$(TCLLIB) -l$(TKLIB) -o $@
endif

### FILE LISTS ################################################################

ARCHIVE_BASE 	= hfsu
ARCHIVE_EXT 	= zip
VER		= $(VERMAJOR)$(VERMINOR)$(RELEASE)
ifdef WIN32
DIST_ARCHIVE	= dist/$(ARCHIVE_BASE)w$(VER).$(ARCHIVE_EXT)
else
DIST_ARCHIVE	= dist/$(ARCHIVE_BASE)o$(VER).$(ARCHIVE_EXT)
endif
SRC_ARCHIVE	= dist/src.$(ARCHIVE_EXT)
ZIP		= /tools/bin/zip
CREATE_DIRS	= bin bin/os2 bin/win32

DIST_FILES = $(addprefix $(BINDIR)/, \
	hattrib.exe hcd.exe hcopy.exe hdel.exe \
	hdir.exe hformat.exe hfsutil.exe hls.exe hmkdir.exe \
	hmount.exe hpwd.exe hrename.exe hrmdir.exe humount.exe \
	hvol.exe) \
	bin/csxhfs.exe $(RSXHFS) \
	$(SRC_ARCHIVE) \
	$(addprefix $(DOCDIR)/, COPYING README hfsutils.txt)

ifndef WIN32
DIST_FILES += $(DOCDIR)/hfsutils.inf
endif

LIBHFS_SRC = $(addprefix $(LIBHFS_DIR)/, \
	block.c block.h btree.c btree.h config.h data.c data.h \
	emx.c file.c file.h hfs.c hfs.h libhfs.h low.c low.h \
	medium.c medium.h node.c node.h os.h record.c record.h \
	sequoia.h unicode.c unicode.h version.c version.h volume.c volume.h \
	Makefile)

DOC_SRC = $(addprefix $(DOCDIR)/, \
	COPYING hfsutils.src readme.os2.src readme.win.src Makefile)

RSX_SRC = $(addprefix $(RSXDIR)/, \
	adosx32.h cdosx32.c cdosx32.h copy32.h dasd.c dasdrm.c dasdrm.h \
	djio.c djio.h doserrno.c doserrno.h dpmi.h dpmi10.h dpmitype.h \
	excep32.h externa.h fs.c fs.h gnuaout.h intrm.c kdeb.c kdeb.h \
	libc.c loadprg.c loadprg.h Makefile ofiles.rsp printf.c printf.h \
	process.c process.h ptrace.c ptrace.h rmlib.c rmlib.h rsx.c rsx.h \
	signals.c signals.h start32.c start32.h statemx.c statemx.h \
	sysdep.h sysdep2.c sysdj.c sysemx.c termio.c termio.h timedos.c \
	timedos.h version.c version.h \
	$(addprefix asm32/, \
	adosx32.s callback.s copy32.s crt0.s dpmi.c dpmi10.s \
	excep32.s fpu.c regs386.inc) \
	build \
	$(addprefix loader/, \
	crt1.asm load2.asm loader.c loader.h Makefile) \
	$(addprefix stub/, \
	emx.inc emxl.asm headers.inc version.inc Makefile))

IMAGES_SRC = $(addprefix images/, \
	application.xbm application_mask.xbm caution.xbm cdrom.xbm \
	cdrom_mask.xbm document.xbm document_mask.xbm floppy.xbm \
	floppy_mask.xbm folder.xbm folder_mask.xbm harddisk.xbm \
	harddisk_mask.xbm help.xbm macdaemon.xbm macdaemon_mask.xbm \
	note.xbm padlock.xbm sm_application.xbm sm_cdrom.xbm \
	sm_document.xbm sm_floppy.xbm sm_folder.xbm sm_harddisk.xbm \
	stop.xbm)

SRC_FILES = binhex.c charset.c copyin.c copyout.c crc.c darray.c dlist.c \
	dstring.c glob.c hattrib.c hcd.c hcopy.c hcwd.c hdel.c hformat.c \
	hfsutil.c hls.c hmkdir.c hmount.c hpwd.c hrename.c \
	hrmdir.c humount.c hvol.c spawn.c suid.c version.c \
	binhex.h charset.h config.h copyin.h copyout.h \
	crc.h darray.h dlist.h dstring.h glob.h hattrib.h hcd.h \
	hcopy.h hcwd.h hdel.h hformat.h hfsutil.h hls.h hmkdir.h \
	hmount.h hpwd.h hrename.h hrmdir.h humount.h hvol.h \
	suid.h version.h \
	Rules.make \
	$(LIBHFS_SRC) $(DOC_SRC) $(RSX_SRC) $(CREATE_DIRS) 
ifdef WITH_TCLTK
  SRC_FILES += hfswish.c hfssh.c images.h tclhfs.c tclhfs.h xhfs.h xhfs.tcl mkxhfs.sed $(IMAGES_SRC)
endif

# Makefile is included separately to prevent unneccessary remake of
# source archive when platform is changed
 
dist: $(DIST_ARCHIVE)

$(SRC_ARCHIVE): $(SRC_FILES)
	$(ZIP) $@ $^ Makefile

$(DIST_ARCHIVE): $(DIST_FILES)
	$(ZIP) -j $@ $^

### DEPENDENCIES FOLLOW #######################################################

$(BINDIR)/binhex.o: binhex.c config.h binhex.h crc.h
$(BINDIR)/charset.o: charset.c config.h charset.h
$(BINDIR)/copyin.o: copyin.c config.h libhfs/hfs.h libhfs/data.h copyin.h \
 charset.h binhex.h crc.h
$(BINDIR)/copyout.o: copyout.c config.h libhfs/hfs.h libhfs/data.h copyout.h \
 charset.h binhex.h crc.h
$(BINDIR)/crc.o: crc.c config.h crc.h
$(BINDIR)/darray.o: darray.c config.h darray.h
$(BINDIR)/dlist.o: dlist.c config.h dlist.h
$(BINDIR)/dstring.o: dstring.c config.h dstring.h
$(BINDIR)/glob.o: glob.c config.h dlist.h dstring.h libhfs/hfs.h glob.h
$(BINDIR)/hattrib.o: hattrib.c config.h libhfs/hfs.h hcwd.h hfsutil.h hattrib.h
$(BINDIR)/hcd.o: hcd.c config.h libhfs/hfs.h hcwd.h hfsutil.h hcd.h
$(BINDIR)/hcopy.o: hcopy.c config.h libhfs/hfs.h hcwd.h hfsutil.h hcopy.h \
 copyin.h copyout.h
$(BINDIR)/hcwd.o: hcwd.c config.h libhfs/hfs.h hcwd.h
$(BINDIR)/hdel.o: hdel.c config.h libhfs/hfs.h hcwd.h hfsutil.h hdel.h
$(BINDIR)/hformat.o: hformat.c config.h libhfs/hfs.h hcwd.h hfsutil.h suid.h \
 hformat.h
$(BINDIR)/hfssh.o: hfssh.c config.h tclhfs.h suid.h
$(BINDIR)/hfsutil.o: hfsutil.c config.h libhfs/hfs.h hcwd.h hfsutil.h suid.h \
 glob.h version.h hattrib.h hcd.h hcopy.h hdel.h hformat.h hls.h \
 hmkdir.h hmount.h hpwd.h hrename.h hrmdir.h humount.h hvol.h
$(BINDIR)/hfswish.o: hfswish.c config.h tclhfs.h xhfs.h suid.h images.h \
 images/macdaemon.xbm images/macdaemon_mask.xbm images/stop.xbm \
 images/caution.xbm images/note.xbm images/floppy.xbm \
 images/harddisk.xbm images/cdrom.xbm images/floppy_mask.xbm \
 images/harddisk_mask.xbm images/cdrom_mask.xbm images/sm_floppy.xbm \
 images/sm_harddisk.xbm images/sm_cdrom.xbm images/folder.xbm \
 images/document.xbm images/application.xbm images/folder_mask.xbm \
 images/document_mask.xbm images/application_mask.xbm \
 images/sm_folder.xbm images/sm_document.xbm images/sm_application.xbm \
 images/help.xbm images/padlock.xbm
$(BINDIR)/hls.o: hls.c config.h libhfs/hfs.h hcwd.h hfsutil.h darray.h dlist.h \
 dstring.h hls.h
$(BINDIR)/hmkdir.o: hmkdir.c config.h libhfs/hfs.h hcwd.h hfsutil.h hmkdir.h
$(BINDIR)/hmount.o: hmount.c config.h libhfs/hfs.h hcwd.h hfsutil.h suid.h \
 hmount.h
$(BINDIR)/hpwd.o: hpwd.c config.h libhfs/hfs.h hcwd.h hfsutil.h hpwd.h
$(BINDIR)/hrename.o: hrename.c config.h libhfs/hfs.h hcwd.h hfsutil.h hrename.h
$(BINDIR)/hrmdir.o: hrmdir.c config.h libhfs/hfs.h hcwd.h hfsutil.h hrmdir.h
$(BINDIR)/humount.o: humount.c config.h libhfs/hfs.h hcwd.h hfsutil.h humount.h
$(BINDIR)/hvol.o: hvol.c config.h libhfs/hfs.h hcwd.h hfsutil.h hvol.h
$(BINDIR)/suid.o: suid.c config.h suid.h
$(BINDIR)/tclhfs.o: tclhfs.c config.h tclhfs.h libhfs/hfs.h glob.h copyin.h \
 copyout.h charset.h suid.h version.h
$(BINDIR)/version.o: version.c version.h
