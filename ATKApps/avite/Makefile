# This is the makefile for the ATK libraries.
# to build, use make all to make the archive
# When invoked it expects to have the following environment variables
# set
#       CPU             - set to the machine name
#       HTKCC           - name of C compiler (either cc or gcc)
#       HTKCF           - compiler flags
#

hlib = ../../HTKLib
alib = ../../ATKLib

HLIBS = $(hlib)/HTKLib.$(CPU).a
ALIBS = $(alib)/ATKLib.$(CPU).a

CXX = g++
ifeq "$(HTKCF)" ""
CXXFLAGS = $(HTKCF) -Wno-write-strings -ansi -g -O2 -DOSS_AUDIO -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(alib) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else 
CXXFLAGS = $(HTKCF) -Wno-write-strings -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(alib) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

all:    AVite

.PHONY: clean cleanup depend
clean:
	-rm -f *.o AVite

cleanup:
	-rm -f *.o AVite

depend:
	-makedepend -Y *.cpp

# DO NOT DELETE THIS LINE -- make depend depends on it.


AVite :  AVite.o
	$(CXX) AVite.o $(ALIBS) $(HLIBS) -lpthread -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out AVite




