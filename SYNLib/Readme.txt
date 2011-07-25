SYNLib

This is part of the flite speech synthesis package distributed by 

    Language Technologies Institute   
       Carnegie Mellon University     
          Copyright (c) 2001          
          All Rights Reserved.        

The interface functions provided by flite.h have been limited to just

cst_utterance *flite_synth_text(const char *text,cst_voice *voice);
cst_utterance *flite_synth_phones(const char *phones,cst_voice *voice);

The direct audio support provided by the festival library is not
used by ATK.   Instead the waveform is extracted from a
cst_utterance using the utt_wave(utt) function and then passed
in packets to ASource (see ASyn.cpp).