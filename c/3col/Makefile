# Makefile for 3col, version 2.07 (17th October 1996)
#
# See the INSTALL file if you need to know what any of it means.

# Compiler and its flags.
#
CC=gcc		# An ANSI C compiler
CFLAGS=-O	# or -g to include debugging info, or whatever you like

# Other things you need to use.
#
COPY=cp		# should take source filename and dest filename
UNPROTECTx=chmod og+x	# what to do before installing so people can use it
UNPROTECTr=chmod og+r	# ditto, for things to be read rather than executed
UNPROTECTd=chmod og+rx	# ditto, for directories
MKDIR=mkdir -p	# make sure a given directory exists
DELETE=rmdir -f	# delete things, not moaning if they aren't there

# Where to install it.
#
# Directory to which the following 3 things are relative
# (make it say just "INSTALLDIR=" if they're absolute pathnames):
INSTALLDIR=/opt/local
# Binaries, non-manpage docs, manpage:
BINDIR=$(INSTALLDIR)/bin
DOCDIR=$(INSTALLDIR)/doc
MANDIR=$(INSTALLDIR)/man
MANSECT=1
# Where does this actually put things?
MANFILE=$(MANDIR)/man$(MANSECT)/3col.$(MANSECT)
DOCPLACE=$(DOCDIR)/3col

# Global config file location.
#
GLOBAL_CF=/usr/lib/3col-config
_GLOBAL_CF=\"$(GLOBAL_CF)\"

# User config file location.
#
USER_CF=~/.3col-config
_USER_CF=\"$(USER_CF$)\"

# -DNEED_EXPANSION or -UNEED_EXPANSION: the former if your system
# requires you to expand filenames by hand.
#
NE_DEF=-UNEED_EXPANSION

#----------------------------------------------------------------------

3col: 3col.c
	$(CC) $(CFLAGS) -DGLOBAL_CONFIG_FILE="$(_GLOBAL_CF)" \
	-DUSER_CONFIG_FILE="$(_USER_CF)" -DDOCS="\"$(DOCPLACE)\"" $(NE_DEF) \
	-o 3col 3col.c

3col.1: 3col.man
	sed -e 's#!SYSCONFIG!#$(GLOBAL_CF)#' \
	-e 's#!USERCONFIG!#$(USER_CF)#' \
	-e 's#!DOCDIR!#$(DOCPLACE)#' 3col.man > 3col.1

install: 3col 3col.1
	$(COPY) 3col $(BINDIR)/3col
	$(COPY) 3col.1 $(MANFILE)
	$(MKDIR) $(DOCPLACE)
	$(COPY) Docs $(DOCPLACE)
	$(COPY) Summary $(DOCPLACE)
	$(COPY) README $(DOCPLACE)
	$(UNPROTECTx) $(BINDIR)/3col
	$(UNPROTECTr) $(MANFILE)
	$(UNPROTECTd) $(DOCPLACE)
	$(UNPROTECTr) $(DOCPLACE)/Docs
	$(UNPROTECTr) $(DOCPLACE)/Summary
	$(UNPROTECTr) $(DOCPLACE)/README

clean:
	$(DELETE) 3col 3col.1 3col.o
