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

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
PLATLIBS = -framework AudioToolbox -framework CoreFoundation
else
PLATLIBS = -lasound
endif

CXX = g++

ifeq "$(HTKCF)" ""
CXXFLAGS = $(HTKCF) -Wno-write-strings -ansi -g -O2 -DOSS_AUDIO -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(alib) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else 
CXXFLAGS = $(HTKCF) -Wno-write-strings -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(alib) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

all:    SSDS

ATKLib.$(CPU).a: $(modules)
	-rm ATKLib.$(CPU).a
	ar rv ATKLib.$(CPU).a $(modules)
	-ranlib ATKLib.$(CPU).a

.PHONY: clean cleanup depend
clean:
	-rm -f *.o SSDS

cleanup:
	-rm -f *.o SSDS

depend:
	-makedepend -Y *.cpp

# DO NOT DELETE THIS LINE -- make depend depends on it.

SSDS :  SSDS.o
	$(CXX) SSDS.o $(ALIBS) $(HLIBS) $(PLATLIBS) -lpthread -lm -lX11 -L/usr/X11R6/lib  $(HTKLF)
	mv a.out SSDS
