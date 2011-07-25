# UNIX SYNLIB LIBRARY MAKEFILE

# This makefile will build an archive file called HTKLib.XXX.a
# and/or a specific library test program
# containing the HTK library object modules where XXX is the
# type of machine.  
# 
# When invoked it expects to have the following environment variables
# set
#       CPU             - set to the machine name
#       HTKCC           - name of C compiler (either cc or gcc)
#       HTKCF           - compiler flags
#
# Suggested settings are
#		CPU	      HTKCC  HTKCF
# SGI:		sgi           cc     -O -DSGI_AUDIO
# HP:		hp700         cc     +e -O -I/usr/include/X11R5 -I/usr/include/audio 
#                                    -I/opt/audio/include -D_HPUX_SOURCE 
#                                    -Aa -DHPUX_AUDIO -DUSE_TIMER 
# Sun:		sun4_solaris  gcc    -ansi -I/usr/openwin/include -O2 
#                                    -DSOLARIS16_AUDIO -DBSD_COMP
# Linux:	linux	      gcc    -ansi -O2 -DOSS_AUDIO
#
# When running in a multiple machine environment, type
#        make clean
# before recompiling for a new machine.
hlib = ../HTKLib
HLIBS = $(hlib)/HTKLib.$(CPU).a

synlib= ../SYNLib
SYNLIBS = $(synlib)/SYNLib.$(CPU).a

ifeq "$(HTKCC)" ""
CC = gcc
else
CC = $(HTKCC)
endif

ifeq "$(HTKCF)" ""
CFLAGS = -O2 -ansi -g -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(synlib) -I$(hlib)\
         -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else
CFLAGS = $(HTKCF) -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(synlib)  -I$(hlib) -DUNIX -DATK\
         -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

modules = cmu6_lts_rules.o  cmu_lex.o  cmu_lex_entry.o  cmu_lex_phone.o

all:   CMU_Lexicon.$(CPU).a

CMU_Lexicon.$(CPU).a: $(modules)
	-rm CMU_Lexicon.$(CPU).a
	ar rv CMU_Lexicon.$(CPU).a $(modules)
	-ranlib CMU_Lexicon.$(CPU).a

.PHONY: clean cleanup depend
clean:
	-rm -f *.o CMU_Lexicon.$(CPU).a *.cpu
	touch $(CPU).cpu

cleanup:
	-rm -f *.o *.cpu

depend:
	-makedepend -Y *.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

cmu6_lts_rules.o: cmu6_lts_rules.h
