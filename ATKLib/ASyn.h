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
/*     File: ASyn.h - Interface to the Synthesiser             */
/* ----------------------------------------------------------- */

/* !HVER!ASyn: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)

// ASYN: TRACE         = 0             -- trace flag

#ifndef _ATK_ASyn
#define _ATK_ASyn

#include "ASource.h"

// -------------- Abstract Interface to actual Synthesiser -----------
//
// a concrete implementation of this class must be passed to ASyn on
// creation
class ASynthesiser {
public:
   virtual void StartUtterance(const string& text)=0;
   // start new utterance with given text
   virtual short *GetWave()=0;
   // get synthesised waveform corresponding to current utterance
   virtual int GetNumSamples()=0;
   // get number of samples in current utterance
   virtual int GetNumWords()=0;
   // get number of words in current utterance
   virtual string GetWord(int i)=0;
   // return i'th word in utterance (first is i=1)
   virtual int GetBoundary(int i)=0;
   // get sample number of end of i'th word
   virtual void EndUtterance()=0;
   // release any storage allocated for current utterance
   friend class ASyn;
};

// ---------------------- ASyn Application Interface -----------------

enum Synth_State {
   synth_idle,
   synth_talking,
   synth_muted,
   synth_unmuted,
   synth_aborted
};

class ASyn: public AComponent {
public:
  ASyn(){}
  ASyn(const string & name, ABuffer *repb, ABuffer *audb, ABuffer *ackb,
       ASource *asink, ASynthesiser *theSyn);
  void Start(HPriority priority=HPRIO_NORM);
private:
  friend TASKTYPE TASKMOD ASyn_Task(void *p);
  void ExecCommand(const string & cmdname);
  void ChkAckBuf();
  void TalkCmd();       // Interface command messages
  void MuteCmd();
  void UnmuteCmd();
  void AbortCmd();
  ASynthesiser *syn;    // the actual synthesiser to use
  ABuffer *audbuf;      // output wavs to ASource
  ABuffer *ackbuf;      // acks from ASource
  ABuffer *repbuf;      // reply to host
  ASource *sink;        // sink for audio output
  int seqnum;           // seqnum for ASource
  Synth_State state;    // synthesiser state
  int trace;            // trace flag
};


#endif

/*  -------------------- End of ASyn.h --------------------- */

