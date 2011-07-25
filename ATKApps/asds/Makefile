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
synlib = ../../SYNLib
cmu_us_kal16 = ../../CMU_US_KAL16
us_english= ../../US_English
cmu_lexicon= ../../CMU_Lexicon

HLIBS = $(hlib)/HTKLib.$(CPU).a
ALIBS = $(alib)/ATKLib.$(CPU).a
CMU_US_KAL16 = $(cmu_us_kal16)/CMU_US_KAL16.$(CPU).a
SYNLIB = $(synlib)/SYNLib.$(CPU).a
US_ENGLISH = $(us_english)/US_English.$(CPU).a
CMU_LEXICON = $(cmu_lexicon)/CMU_Lexicon.$(CPU).a

CXX = g++

ifeq "$(HTKCF)" ""
CXXFLAGS = $(HTKCF) -Wno-write-strings -ansi -g -O2 -DOSS_AUDIO -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(synlib) -I$(cmu_us_kal16) -I$(alib) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else 
CXXFLAGS = $(HTKCF) -Wno-write-strings -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib)  -I$(synlib) -I$(cmu_us_kal16) -I$(alib) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif


all:    ASDS

.PHONY: clean cleanup depend
clean:
	-rm -f *.o ASDS


cleanup:
	-rm -f *.o *.cpu

depend:
	-makedepend -Y *.cpp

# DO NOT DELETE THIS LINE -- make depend depends on it.

ASDS : ASDS.o
	$(CXX) ASDS.o $(ALIBS) $(HLIBS) $(SYNLIB) $(CMU_US_KAL16) $(US_ENGLISH) $(CMU_LEXICON) $(SYNLIB) -lpthread  -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out ASDS
