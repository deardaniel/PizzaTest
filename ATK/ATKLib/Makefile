# This is the makefile for the ATK libraries.
# to build, use make all to make the archive
# When invoked it expects to have the following environment variables
# set
#       CPU             - set to the machine name
#       HTKCC           - name of C compiler (either cc or gcc)
#       HTKCF           - compiler flags
#

hlib = ../HTKLib
alib = ../ATKLib
synlib = ../SYNLib
cmu_us_kal16 = ../CMU_US_KAL16
us_english= ../US_English
cmu_lexicon= ../CMU_Lexicon

HLIBS = $(hlib)/HTKLib.$(CPU).a
ALIBS = $(alib)/ATKLib.$(CPU).a
CMU_US_KAL16 = $(cmu_us_kal16)/CMU_US_KAL16.$(CPU).a
SYNLIB = $(synlib)/SYNLib.$(CPU).a
US_ENGLISH = $(us_english)/US_English.$(CPU).a
CMU_LEXICON = $(cmu_lexicon)/CMU_Lexicon.$(CPU).a

CXX = g++

ifeq "$(HTKCF)" ""
CXXFLAGS = $(HTKCF) -Wno-write-strings -ansi -g -O2 -DOSS_AUDIO -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(synlib) -I$(cmu_us_kal16) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else 
CXXFLAGS = $(HTKCF) -Wno-write-strings -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib)  -I$(synlib) -I$(cmu_us_kal16) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

modules = ABuffer.o ACode.o AComponent.o ADict.o AGram.o AHTK.o \
	  AHmms.o AMonitor.o ANGram.o APacket.o ARMan.o ARec.o \
	  AResource.o ASource.o AIO.o ASyn.o ATee.o ALog.o ASplash.o

all:    ATKLib.$(CPU).a

ATKLib.$(CPU).a: $(modules)
	-rm ATKLib.$(CPU).a
	ar rv ATKLib.$(CPU).a $(modules)
	-ranlib ATKLib.$(CPU).a

.PHONY: clean cleanup depend
clean:
	-rm -f *.o */*.o ATKLib.$(CPU).a *.cpu
	-rm TSyn/TSyn TSource/TSource TIO/TIO TBase/TBase
	-rm TRec/TRec TCode/TCode
	touch $(CPU).cpu

cleanup:
	-rm -f *.o *.cpu

depend:
	-makedepend -Y *.cpp

# DO NOT DELETE THIS LINE -- make depend depends on it.

ABuffer.o: APacket.h
ACode.o: AComponent.h
AComponent.o: ABuffer.h
ADict.o: AHTK.h AResource.h 
AGram.o: AHTK.h AResource.h ADict.h
AHTK.o: AHTK.h
AHmms.o: AHTK.h AResource.h
AMonitor.o: AHTK.h AComponent.h
ANGram.o: AHTK.h AResource.h ADict.h
APacket.o: AHTK.h
ARMan.o: AHTK.h AHmms.h ADict.h AGram.h ANGram.h
ARec.o: ARMan.h AComponent.h
AResource.o: AHTK.h
ASource.o: AComponent.h
AIO.o: AIO.h
ASyn.o: ASyn.h
ATee.o: AComponent.h
ALog: AComponent.h
AScript: AScript.h

TPacket :  ATKLib.$(CPU).a MiscTests/TPacket.o
	$(CXX) MiscTests/TPacket.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out MiscTests/TPacket

TBuffer :  ATKLib.$(CPU).a MiscTests/TBuffer.o
	$(CXX) MiscTests/TBuffer.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out MiscTests/TBuffer

TADict :  ATKLib.$(CPU).a MiscTests/TADict.o
	$(CXX) MiscTests/TADict.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out MiscTests/TADict

TAGram :  ATKLib.$(CPU).a MiscTests/TAGram.o
	$(CXX) MiscTests/TAGram.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out MiscTests/TAGram

TAHmms :  ATKLib.$(CPU).a MiscTests/TAHmms.o
	$(CXX) MiscTests/TAHmms.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out MiscTests/TAHmms

TARMan :  ATKLib.$(CPU).a MiscTests/TARMan.o
	$(CXX) MiscTests/TARMan.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out MiscTests/TARMan

TBase :  ATKLib.$(CPU).a TBase/TBase.o
	$(CXX) TBase/TBase.o $(ALIBS) $(HLIBS) -lpthread -lm  -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TBase/TBase

TSource :  ATKLib.$(CPU).a TSource/TSource.o
	$(CXX) TSource/TSource.o $(ALIBS) $(HLIBS) -lpthread  -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TSource/TSource

TCode :  ATKLib.$(CPU).a TCode/TCode.o
	$(CXX) TCode/TCode.o $(ALIBS) $(HLIBS) -lpthread  -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TCode/TCode

TRec :  ATKLib.$(CPU).a TRec/TRec.o
	$(CXX) TRec/TRec.o $(ALIBS) $(HLIBS) -lpthread  -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TRec/TRec

TSyn : ATKLib.$(CPU).a TSyn/TSyn.o
	$(CXX) TSyn/TSyn.o $(ALIBS) $(HLIBS) $(SYNLIB) $(CMU_US_KAL16) $(US_ENGLISH) $(CMU_LEXICON) $(SYNLIB) -lpthread  -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TSyn/TSyn

TIO : ATKLib.$(CPU).a TIO/TIO.o
	$(CXX) TIO/TIO.o $(ALIBS) $(HLIBS) $(SYNLIB) $(CMU_US_KAL16) $(US_ENGLISH) $(CMU_LEXICON) $(SYNLIB) -lpthread  -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TIO/TIO
