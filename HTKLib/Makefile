# UNIX HTK LIBRARY MAKEFILE

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

UNAME := $(shell uname -s)

hlib = .

HLIBS = $(hlib)/HTKLib.$(CPU).a

ifeq "$(HTKCC)" ""
CC = gcc
else
CC = $(HTKCC)
endif

ifeq "$(HTKCF)" ""
CFLAGS = -O2 -ansi -g -D'ARCH="$(CPU)"' -DXGRAFIX -I.\
          -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else
CFLAGS = $(HTKCF) -D'ARCH="$(CPU)"' -DXGRAFIX -I. -DUNIX -DATK\
          -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

ifeq ($(UNAME),Darwin)
    CFLAGS += -Wno-write-strings -DAPPLE
endif

#option for making HTKTools:
ifneq "$(HTKNOATK)" ""
CFLAGS = -O2 -ansi -DOSS_AUDIO -D'ARCH="$(CPU)"' -DXGRAFIX -I.\
        -DUNIX -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

modules = HShell.o HMath.o  HSigP.o  HWave.o HAudio.o HParm.o HVQ.o  HGraf.o\
          HLabel.o HModel.o HUtil.o HTrain.o HDict.o  HLM.o   \
          HLat.o   HNBest.o HRec.o HNet.o \
          HAdapt.o HFB.o HMem.o HThreads.o 

all:    HTKLib.$(CPU).a

HTKLib.$(CPU).a: $(modules)
	-rm HTKLib.$(CPU).a
	ar rv HTKLib.$(CPU).a $(modules)
	-ranlib HTKLib.$(CPU).a

.PHONY: clean cleanup depend
clean:
	-rm -f *.o HTKLib.$(CPU).a *.cpu
	touch $(CPU).cpu

cleanup:
	-rm -f *.o *.cpu

depend:
	-makedepend -Y *.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

HAdapt.o: HShell.h HMem.h HMath.h HSigP.h HWave.h HAudio.h HParm.h HLabel.h
HAdapt.o: HModel.h HTrain.h HUtil.h HAdapt.h
HAudio.o: HShell.h HMem.h HMath.h HWave.h HAudio.h
HDict.o: HShell.h HMem.h HMath.h HWave.h HLabel.h HDict.h
HFB.o: HShell.h HMem.h HMath.h HSigP.h HAudio.h HWave.h HVQ.h HParm.h
HFB.o: HLabel.h HModel.h HTrain.h HUtil.h HAdapt.h HFB.h
HGraf.o: HShell.h HMem.h HMath.h HGraf.h HThreads.h 
HGraf.null.o: HShell.h HMem.h HMath.h HGraf.h
HGraf_WIN32.o: HShell.h HMem.h HMath.h HGraf.h
HLM.o: HShell.h HMem.h HMath.h HWave.h HLabel.h HLM.h
HLabel.o: HShell.h HMem.h HMath.h HWave.h HLabel.h
HMath.o: HShell.h HMem.h HMath.h
HMem.o: HShell.h HMem.h
HModel.o: HShell.h HMem.h HMath.h HWave.h HAudio.h HParm.h HLabel.h HModel.h
HModel.o: HUtil.h HTrain.h HAdapt.h
HLat.o: HShell.h HMem.h HMath.h HWave.h HAudio.h HParm.h HLabel.h HModel.h
HLat.o: HUtil.h HDict.h HNet.h HLM.h HLat.h
HNet.o: HShell.h HMem.h HMath.h HWave.h HAudio.h HParm.h HLabel.h HModel.h
HNet.o: HUtil.h HDict.h HNet.h
HNBest.o: HShell.h HMem.h HMath.h HSigP.h HWave.h HAudio.h HVQ.h HParm.h
HNBest.o: HLabel.h HModel.h HDict.h HLM.h HNet.h HRec.h HNBest.h
HParm.o: HShell.h HMem.h HMath.h HSigP.h HAudio.h HWave.h HVQ.h HParm.h
HParm.o: HLabel.h HModel.h 
HRec.o: HShell.h HMem.h HMath.h HWave.h HAudio.h HParm.h HLabel.h HModel.h
HRec.o: HDict.h HNet.h HRec.h HUtil.h HLM.h HLat.h HNBest.h
HShell.o: HShell.h
HSigP.o: HShell.h HMem.h HMath.h HSigP.h
HThreads.o: HShell.h HThreads.h
HTrain.o: HShell.h HMem.h HMath.h HSigP.h HAudio.h HWave.h HVQ.h HParm.h
HTrain.o: HLabel.h HModel.h HUtil.h HTrain.h
HUtil.o: HShell.h HMem.h HMath.h HSigP.h HAudio.h HWave.h HVQ.h HParm.h
HUtil.o: HLabel.h HModel.h HUtil.h
HVQ.o: HShell.h HMem.h HMath.h HVQ.h
HWave.o: HShell.h HMem.h HMath.h HWave.h HAudio.h HParm.h

HThreadTest :  HTKLib.$(CPU).a HThreadTest.o
	$(CC) HThreadTest.o $(HLIBS) -lpthread -lm -lX11 -L/usr/X11R6/lib $(HTKLF)
	mv a.out HThreadTest

HThreadXTest :  HTKLib.$(CPU).a HThreadXTest.o
	$(CC) HThreadXTest.o $(HLIBS) -lpthread -lm -lX11 -L/usr/X11R6/lib  $(HTKLF)
	mv a.out HThreadXTest

HGrafTest :  HTKLib.$(CPU).a HGrafTest.o
	$(CC) HGrafTest.o $(HLIBS) -lm -lX11 -L/usr/X11R6/lib  $(HTKLF)
	mv a.out HGrafTest








