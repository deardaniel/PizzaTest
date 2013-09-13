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
alib = ../ATKLib
ALIBS = $(alib)/ATKLib.$(CPU).a
synlib = ../SYNLib
cmu_us_kal16 = ../CMU_US_KAL16
us_english= ../US_English
cmu_lexicon= ../CMU_Lexicon

CMU_US_KAL16 = $(cmu_us_kal16)/CMU_US_KAL16.$(CPU).a
SYNLIB = $(synlib)/SYNLib.$(CPU).a
US_ENGLISH = $(us_english)/US_English.$(CPU).a
CMU_LEXICON = $(cmu_lexicon)/CMU_Lexicon.$(CPU).a



ifeq "$(HTKCC)" ""
CC = gcc
else
CC = $(HTKCC)
endif

ifeq "$(HTKCF)" ""
CFLAGS = -O2 -ansi -g -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib)\
         -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else
CFLAGS = $(HTKCF) -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -DUNIX -DATK\
         -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

ifeq "$(HTKCF)" ""
CXXFLAGS = $(HTKCF) -ansi -g -O2 -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(alib) -I$(cmu_us_kal16) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
else
CXXFLAGS = $(HTKCF) -D'ARCH="$(CPU)"' -DXGRAFIX -I. -I$(hlib) -I$(alib) -I$(cmu_us_kal16) -DUNIX -DATK -D_cplusplus -D_XOPEN_SOURCE=500 -D_REENTRANT
endif

CXX = g++

modules = audio.o cst_alloc.o cst_args.o cst_cart.o cst_clunits.o \
	  cst_diphone.o cst_endian.o cst_error.o cst_features.o \
	  cst_ffeature.o cst_file_stdio.o  cst_item.o cst_lexicon.o \
	  cst_lpcres.o cst_lts.o cst_lts_rewrites.o cst_mmap_win32.o \
	  cst_phoneset.o cst_reflpc.o cst_regex.o cst_relation.o \
	  cst_rel_io.o cst_sigpr.o cst_sigprFP.o cst_ss.o cst_string.o  \
	  cst_sts.o cst_synth.o cst_tokenstream.o cst_track.o \
	  cst_track_io.o cst_units.o cst_utterance.o cst_utt_utils.o \
	  cst_val.o cst_val_const.o cst_val_user.o cst_viterbi.o \
	  cst_voice.o cst_wave.o cst_wave_io.o cst_wave_utils.o \
	  flite.o rateconv.o regexp.o regsub.o FliteSynthesiser.o

all:    SYNLib.$(CPU).a

SYNLib.$(CPU).a: $(modules)
	-rm SYNLib.$(CPU).a
	ar rv SYNLib.$(CPU).a $(modules)
	-ranlib SYNLib.$(CPU).a

.PHONY: clean cleanup depend
clean:
	-rm -f *.o SYNLib.$(CPU).a *.cpu
	touch $(CPU).cpu

cleanup:
	-rm -f *.o *.cpu

depend:
	-makedepend -Y *.c*

# DO NOT DELETE THIS LINE -- make depend depends on it.

audio.o: cst_string.h cst_wave.h cst_error.h cst_alloc.h cst_endian.h
audio.o: cst_file.h cst_val.h cst_val_defs.h cst_val_const.h cst_audio.h
audio.o: cst_hrg.h cst_features.h cst_item.h cst_relation.h cst_utterance.h
cst_alloc.o: cst_alloc.h cst_error.h
cst_args.o: cst_tokenstream.h cst_alloc.h cst_string.h cst_file.h
cst_args.o: cst_features.h cst_val.h cst_error.h cst_val_defs.h
cst_args.o: cst_val_const.h
cst_cart.o: cst_regex.h cst_string.h cst_cart.h cst_val.h cst_file.h
cst_cart.o: cst_error.h cst_alloc.h cst_val_defs.h cst_val_const.h
cst_cart.o: cst_features.h cst_item.h cst_relation.h cst_utterance.h
cst_clunits.o: cst_hrg.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_clunits.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_clunits.o: cst_item.h cst_relation.h cst_utterance.h cst_utt_utils.h
cst_clunits.o: cst_wave.h cst_endian.h cst_track.h cst_viterbi.h
cst_clunits.o: cst_clunits.h cst_cart.h cst_sts.h cst_units.h cst_sigpr.h
cst_diphone.o: cst_hrg.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_diphone.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_diphone.o: cst_item.h cst_relation.h cst_utterance.h cst_utt_utils.h
cst_diphone.o: cst_wave.h cst_endian.h cst_track.h cst_diphone.h cst_sts.h
cst_diphone.o: cst_units.h cst_sigpr.h
cst_endian.o: cst_alloc.h cst_endian.h
cst_error.o: cst_error.h
cst_features.o: cst_error.h cst_features.h cst_alloc.h cst_val.h cst_file.h
cst_features.o: cst_val_defs.h cst_val_const.h cst_string.h
cst_ffeature.o: cst_alloc.h cst_item.h cst_features.h cst_val.h cst_file.h
cst_ffeature.o: cst_error.h cst_val_defs.h cst_val_const.h cst_string.h
cst_ffeature.o: cst_relation.h cst_utterance.h cst_tokenstream.h
cst_file_stdio.o: cst_file.h cst_error.h cst_alloc.h
cst_item.o: cst_alloc.h cst_item.h cst_features.h cst_val.h cst_file.h
cst_item.o: cst_error.h cst_val_defs.h cst_val_const.h cst_string.h
cst_item.o: cst_relation.h cst_utterance.h
cst_lexicon.o: cst_features.h cst_alloc.h cst_val.h cst_file.h cst_error.h
cst_lexicon.o: cst_val_defs.h cst_val_const.h cst_string.h cst_lexicon.h
cst_lexicon.o: cst_item.h cst_lts.h
cst_lpcres.o: cst_string.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_lpcres.o: cst_val_defs.h cst_val_const.h cst_sts.h
cst_lts.o: cst_features.h cst_alloc.h cst_val.h cst_file.h cst_error.h
cst_lts.o: cst_val_defs.h cst_val_const.h cst_string.h cst_lts.h cst_endian.h
cst_lts_rewrites.o: cst_string.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_lts_rewrites.o: cst_val_defs.h cst_val_const.h cst_lts_rewrites.h
cst_phoneset.o: cst_val.h cst_file.h cst_error.h cst_alloc.h cst_val_defs.h
cst_phoneset.o: cst_val_const.h cst_utterance.h cst_features.h cst_string.h
cst_phoneset.o: cst_item.h cst_relation.h cst_phoneset.h
cst_reflpc.o: cst_alloc.h
cst_regex.o: cst_alloc.h cst_regex.h cst_string.h
cst_relation.o: cst_item.h cst_features.h cst_alloc.h cst_val.h cst_file.h
cst_relation.o: cst_error.h cst_val_defs.h cst_val_const.h cst_string.h
cst_relation.o: cst_relation.h cst_utterance.h
cst_rel_io.o: cst_file.h cst_tokenstream.h cst_alloc.h cst_string.h
cst_rel_io.o: cst_item.h cst_features.h cst_val.h cst_error.h cst_val_defs.h
cst_rel_io.o: cst_val_const.h cst_relation.h cst_utterance.h
cst_sigpr.o: cst_hrg.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_sigpr.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_sigpr.o: cst_item.h cst_relation.h cst_utterance.h cst_wave.h
cst_sigpr.o: cst_endian.h cst_sigpr.h cst_sts.h
cst_sigprFP.o: cst_hrg.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_sigprFP.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_sigprFP.o: cst_item.h cst_relation.h cst_utterance.h cst_wave.h
cst_sigprFP.o: cst_endian.h cst_sigpr.h cst_sts.h
cst_ss.o: cst_alloc.h cst_ss.h
cst_string.o: cst_alloc.h cst_string.h
cst_sts.o: cst_string.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_sts.o: cst_val_defs.h cst_val_const.h cst_sts.h
cst_synth.o: cst_hrg.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_synth.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_synth.o: cst_item.h cst_relation.h cst_utterance.h cst_cart.h
cst_synth.o: cst_tokenstream.h cst_utt_utils.h cst_wave.h cst_endian.h
cst_synth.o: cst_track.h cst_lexicon.h cst_lts.h cst_units.h cst_sts.h
cst_synth.o: cst_synth.h cst_voice.h cst_phoneset.h
cst_tokenstream.o: cst_tokenstream.h cst_alloc.h cst_string.h cst_file.h
cst_track.o: cst_string.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_track.o: cst_val_defs.h cst_val_const.h cst_track.h cst_endian.h
cst_track_io.o: cst_string.h cst_endian.h cst_tokenstream.h cst_alloc.h
cst_track_io.o: cst_file.h cst_track.h cst_error.h cst_val.h cst_val_defs.h
cst_track_io.o: cst_val_const.h
cst_units.o: cst_hrg.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_units.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_units.o: cst_item.h cst_relation.h cst_utterance.h cst_utt_utils.h
cst_units.o: cst_wave.h cst_endian.h cst_track.h cst_units.h cst_sts.h
cst_units.o: cst_sigpr.h
cst_utterance.o: cst_alloc.h cst_utterance.h cst_val.h cst_file.h cst_error.h
cst_utterance.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_utterance.o: cst_item.h cst_relation.h
cst_utt_utils.o: cst_synth.h cst_hrg.h cst_val.h cst_file.h cst_error.h
cst_utt_utils.o: cst_alloc.h cst_val_defs.h cst_val_const.h cst_features.h
cst_utt_utils.o: cst_string.h cst_item.h cst_relation.h cst_utterance.h
cst_utt_utils.o: cst_voice.h cst_utt_utils.h cst_wave.h cst_endian.h
cst_utt_utils.o: cst_track.h
cst_val.o: cst_file.h cst_val.h cst_error.h cst_alloc.h cst_val_defs.h
cst_val.o: cst_val_const.h cst_string.h
cst_val_const.o: cst_val.h cst_file.h cst_error.h cst_alloc.h cst_val_defs.h
cst_val_const.o: cst_val_const.h cst_features.h cst_string.h
cst_val_user.o: cst_val.h cst_file.h cst_error.h cst_alloc.h cst_val_defs.h
cst_val_user.o: cst_val_const.h cst_string.h
cst_viterbi.o: cst_viterbi.h cst_utterance.h cst_val.h cst_file.h cst_error.h
cst_viterbi.o: cst_alloc.h cst_val_defs.h cst_val_const.h cst_features.h
cst_viterbi.o: cst_string.h cst_item.h cst_relation.h
cst_voice.o: cst_alloc.h cst_voice.h cst_val.h cst_file.h cst_error.h
cst_voice.o: cst_val_defs.h cst_val_const.h cst_features.h cst_string.h
cst_voice.o: cst_utterance.h cst_item.h cst_relation.h
cst_wave.o: cst_string.h cst_val.h cst_file.h cst_error.h cst_alloc.h
cst_wave.o: cst_val_defs.h cst_val_const.h cst_wave.h cst_endian.h
cst_wave_io.o: cst_string.h cst_wave.h cst_error.h cst_alloc.h cst_endian.h
cst_wave_io.o: cst_file.h cst_val.h cst_val_defs.h cst_val_const.h
flite.o: cst_tokenstream.h cst_alloc.h cst_string.h cst_file.h flite.h
flite.o: cst_regex.h cst_val.h cst_error.h cst_val_defs.h cst_val_const.h
flite.o: cst_features.h cst_item.h cst_relation.h cst_utterance.h cst_wave.h
flite.o: cst_endian.h cst_track.h cst_cart.h cst_phoneset.h cst_voice.h
flite.o: cst_utt_utils.h cst_lexicon.h cst_lts.h cst_synth.h cst_hrg.h
flite.o: cst_units.h cst_sts.h
FliteSynthesiser.o: FliteSynthesiser.h flite.h cst_string.h cst_regex.h
FliteSynthesiser.o: cst_val.h cst_file.h cst_error.h cst_alloc.h
FliteSynthesiser.o: cst_val_defs.h cst_val_const.h cst_features.h cst_item.h
FliteSynthesiser.o: cst_relation.h cst_utterance.h cst_wave.h cst_endian.h
FliteSynthesiser.o: cst_track.h cst_cart.h cst_phoneset.h cst_voice.h
FliteSynthesiser.o: cst_utt_utils.h cst_lexicon.h cst_lts.h cst_synth.h
FliteSynthesiser.o: cst_hrg.h cst_units.h cst_sts.h cst_tokenstream.h 
rateconv.o: cst_alloc.h cst_error.h cst_wave.h cst_endian.h cst_file.h
rateconv.o: cst_val.h cst_val_defs.h cst_val_const.h
regexp.o: cst_alloc.h cst_string.h cst_error.h cst_regex.h
regsub.o: cst_regex.h cst_string.h cst_error.h
TFlite.o: flite.h cst_string.h cst_regex.h cst_val.h cst_file.h cst_error.h
TFlite.o: cst_alloc.h cst_val_defs.h cst_val_const.h cst_features.h
TFlite.o: cst_item.h cst_relation.h cst_utterance.h cst_wave.h cst_endian.h
TFlite.o: cst_track.h cst_cart.h cst_phoneset.h cst_voice.h cst_utt_utils.h
TFlite.o: cst_lexicon.h cst_lts.h cst_synth.h cst_hrg.h cst_units.h cst_sts.h
TFlite.o: cst_tokenstream.h


TFlite : TFlite.o
	$(CXX) TFlite.o $(ALIBS) $(HLIBS) $(CMU_US_KAL16) $(US_ENGLISH) $(CMU_LEXICON) -lpthread -lm -lX11  -L/usr/X11R6/lib  $(HTKLF)
	mv a.out TFlite

