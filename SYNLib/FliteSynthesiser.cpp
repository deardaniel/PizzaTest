/* ----------------------------------------------------------- */
/*                                                             */
/*                        _ ___                                */
/*                       /_\ | |_/                             */
/*                       | | | | \                             */
/*                       =========                             */
/*                                                             */
/*        Real-time API for HTK-base Speech Recognition        */
/*                                                             */
/*       Machine Intelligence Laboratory (Speech Group)        */
/*        Cambridge University Engineering Department          */
/*                  http://mi.eng.cam.ac.uk/                   */
/*                                                             */
/*               Copyright CUED 2000-2006                      */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*     File: FliteSynthesiser.cpp - FLite Synth Class          */
/* ----------------------------------------------------------- */

char * flitesynthesiser_version="!HVER!FliteSynthesiser: 1.5.1 [SJY 02/03/07]";

#include "ASyn.h"
#include "FliteSynthesiser.h"
#include "cmu_us_kal16.h"


// Constructor for FSynthesiser
FSynthesiser::FSynthesiser()
{
   // initialise Edinburgh cst lib
   cst_regex_init();
   // setup the voice
   voice = register_cmu_us_kal16(NULL);
   // clear the current utterance
   utt = NULL; cstwave = NULL;
}

// Flite Synthesis 
void FSynthesiser::StartUtterance(const string& text)
{
   if (utt != NULL) EndUtterance();
   ctext = text;
   utt = flite_synth_text(text.c_str(),voice);
   if (utt==NULL) {
      HRError(12002,"FSynth::StartUtterance: cant synthesise %s\n",text.c_str());
      throw ATK_Error(12002);
   }
   cstwave = utt_wave(utt);
   // collect word timing info
   const cst_item *it, *itlast = NULL;
   float x,y;
   int i;
   string lastword="0"; x = 0;
   tokens.clear(); endtimes.clear();
   for (i=0,it = relation_head(utt_relation(utt, "Segment")); 
        it!=NULL; it = item_next(it),i++)
   {
      y = item_feat_float(it,"end");
      string wd = string(ffeature_string(it,"R:SylStructure.parent.parent.name"));
      if (wd != lastword){
         tokens.push_back(lastword); endtimes.push_back(int(x*16000.0));
         lastword=wd;
      }
      x = y;
   }
}

// Get pointer to Waveform
short *FSynthesiser::GetWave()
{
   if (cstwave == NULL) return NULL;
   return cstwave->samples;
}

// Get number of samples in synthed waveform
int FSynthesiser::GetNumSamples()
{
   if (cstwave == NULL) return 0;
   return cstwave->num_samples;
}

// Get number of words in current utterance
int FSynthesiser::GetNumWords()
{
   if (tokens.size()<1) return 0;
   return tokens.size()-1;
}

// return i'th word in utterance (first is i=1)
string FSynthesiser::GetWord(int i)
{
   if (i<1 || i>tokens.size()-1) return "";
   return tokens[i];
}
   
// get sample number of end of i'th word
int FSynthesiser::GetBoundary(int i)
{
   if (i<1) return 0;
   if (i > tokens.size()-1) return GetNumSamples();
   return endtimes[i];
}

// Delete the synthed utterance
void FSynthesiser::EndUtterance()
{
   if (utt==NULL) return;
   delete_utterance(utt);
   utt = NULL; cstwave = NULL;
   tokens.clear(); endtimes.clear();
   ctext.clear();
}

// ----------------------End of FliteSynthesiser.cpp ---------------------
