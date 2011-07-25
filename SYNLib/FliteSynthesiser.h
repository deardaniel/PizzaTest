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
/*               Copyright CUED 2000-2007                      */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*     File: FliteSynthesiser.h - FLite Synth Class            */
/* ----------------------------------------------------------- */

/* !HVER!FliteSynthesiser: 1.6.0 [SJY 01/06/07] */

#ifndef _ATK_FliteSynthesiser
#define _ATK_FliteSynthesiser

#include "flite.h"
#include "ASyn.h"

class FSynthesiser : public ASynthesiser {
public:
   FSynthesiser();
   void StartUtterance(const string& text);
   // start new utterance with given text
   short *GetWave();
   // get synthesised waveform corresponding to current utterance
   int GetNumSamples();
   // get number of samples in current utterance
   int GetNumWords();
   // get number of words in current utterance
   string GetWord(int i);
   // return i'th word in utterance (first is i=1)
   int GetBoundary(int i);
   // get sample number of end of i'th word
   void EndUtterance();
   // release any storage allocated for current utterance
private:
  string ctext;         // current output text
  cst_voice *voice;     // synthesis voice
  cst_utterance *utt;   // current utterance
  cst_wave *cstwave;    // synthesised wave
  vector<string> tokens; // word end info
  vector<int> endtimes;
};

#endif

/*  -------------------- End of FliteSynthesiser.h --------------------- */
