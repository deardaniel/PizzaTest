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

modules = us_aswd.o  us_dur_stats.o  us_durz_cart.o  usenglish.o \
	  us_expand.o  us_f0lr.o  us_f0_model.o  us_ffeatures.o \
	  us_gpos.o  us_int_accent_cart.o  us_int_tone_cart.o \
	  us_nums_cart.o  us_phoneset.o  us_phrasing_cart.o \
	  us_postlex.o  us_text.o

all:   US_English.$(CPU).a

US_English.$(CPU).a: $(modules)
	-rm US_English.$(CPU).a
	ar rv US_English.$(CPU).a $(modules)
	-ranlib US_English.$(CPU).a

.PHONY: clean cleanup depend
clean:
	-rm -f *.o US_English.$(CPU).a *.cpu
	touch $(CPU).cpu

cleanup:
	-rm -f *.o *.cpu

depend:
	-makedepend -Y *.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

us_aswd.o: usenglish.h us_text.h
us_durz_cart.o: us_durz_cart.h
usenglish.o: usenglish.h us_f0.h us_text.h us_ffeatures.h
us_expand.o: us_text.h
us_f0lr.o: us_f0.h
us_f0_model.o: us_f0.h
us_ffeatures.o: us_ffeatures.h
us_int_accent_cart.o: us_int_accent_cart.h
us_int_tone_cart.o: us_int_tone_cart.h
us_nums_cart.o: us_nums_cart.h
us_phrasing_cart.o: us_phrasing_cart.h
us_text.o: usenglish.h us_text.h
