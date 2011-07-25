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
/*        File: ARec.h -    Interface to the recogniser        */
/* ----------------------------------------------------------- */

/* !HVER!ARec: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
//
// AREC: SHOWDISP      = T         -- display running feature gram
// AREC: DISPXORIGIN   = 420       -- top-left X coord of display Window
// AREC: DISPYORIGIN   = 280       -- top-left Y coord of display Window
// AREC: DISPHEIGHT    = 120       -- height of arec display
// AREC: DISPWIDTH     = 400       -- width of arec display
// AREC: RUNMODE       = 01222     -- set run mode
// AREC: NTOKS         = 0         -- number of tokens (0 == std 1-best decode)
// AREC: WORDPEN       = 0.0       -- word insertion penalty
// AREC: LMSCALE       = 1.0       -- link lm scale factor
// AREC: NGSCALE       = 0.0       -- ngram lm scale factor
// AREC: PRSCALE       = 1.0       -- pronunciation scale factor
// AREC: GENBEAM       = 225.0     -- general beam width
// AREC: WORDBEAM      = 200.0     -- word beam width
// AREC: NBEAM         = 225.0     -- nbest beam width
// AREC: MAXBEAM       = 0         -- max number of active nodes in beam
// AREC: GRPNAME       = ""        -- default resource group name
// AREC: TRBAKFREQ     = 5         -- default traceback freq for RD
// AREC: NBEST         = 0         -- number N-Best hyps to compute
//       TARGETRATE    = 10000.0   -- target sample rate
// HREC: FORCEOUT      = F         -- force output

#include <stdio.h>
#ifndef _ATK_ARec
#define _ATK_ARec

#include "ARMan.h"
#include "AComponent.h"

// Recogniser run mode: flags
enum RunMode {
  ONESHOT_MODE     =00001,
  CONTINUOUS_MODE  =00002,
  RUN_IMMED        =00010,
  FLUSH_TOMARK     =00020,
  FLUSH_TOSPEECH   =00040,
  STOP_IMMED       =00100,
  STOP_ATMARK      =00200,
  STOP_ATSIL       =00400,
  RESULT_ATEND     =01000,
  RESULT_IMMED     =02000,
  RESULT_ASAP      =04000,
  RESULT_ALL       =07000,
  MAXMODEVAL       =04442
};

// Recogniser state
enum RunState {
  WAIT_STATE,   // idle, waiting to do something
  PRIME_STATE,  // prime recogniser, update network if necessary
  FLUSH_STATE,  // flushing incoming packets
  RUN_STATE,    // recognising incoming packets
  ANS_STATE     // computing results
};

typedef map<Path*, int, less<Path*> > PathMap;
typedef list<APacket> PacketList;

class ARec: public AComponent {
public:
  ARec(const string & name, ABuffer *inb, ABuffer *outb, ARMan *armgr, int nnToks=0);
  ~ARec();
  void Start(HPriority priority=HPRIO_NORM);
  float lmScale;       // link lm scaling
  float ngScale;       // ngram lm scaling
  float prScale;       // pron model scaling
  float wordPen;       // word insertion penalty
  float genBeam;       // general beam width
  float nBeam;         // n-token beam
  float wordBeam;      // word end beam width
  int maxActive;       // max active models
  int nToks;           // number of tokens
  int nBest;           //n-best decoding

private:
  friend TASKTYPE TASKMOD ARec_Task(void *p);
  void InitDrawRD();
  void DrawStatus();
  void DrawOutLine(PhraseType k, HTime t, string wrd, string tag);
  void ScrollOutBox();
  // Runtime commands
  void ExecCommand(const string & cmdname);
  void SetModeCmd();
  void StartCmd();
  void StopCmd();
  void UseGrpCmd();
  // State Machine operations
  void InitRecogniser();
  void PrimeRecogniser();
  void SendMarkerPkt(string marker);
  Boolean FlushObservation();    // TRUE when flushing complete
  Boolean RecObservation();      // TRUE when recognition complete
  void OutPathElement(int n, PartialPath pp);
  void OutTranscription(Transcription *trans);
  void TraceBackRecogniser();    // Do traceback for RD
  void OutPacket(PhraseType k, string wrd, string tag,
		int pred, int alt, float ac, float lm, float score,
		float confidence, float nact, HTime start, HTime end);
  void StoreMarker(APacket p);    // Store unrecognised markers for onward transmission
  void OutMarkers(HTime t);       // Forward stored markers stamped < t, all if t -ve
  void ComputeAnswer();
  // Recogniser global data

  int trace;           // trace control
  PSetInfo *psi;       // Model related recognition data
  PRecInfo *pri;       // Private recognition data
  ARMan *rmgr;         // Resource manager
  RunState runstate;   // current state
  RunState laststate;  // previous state
  RunMode runmode;     // current mode
  string grpName;      // current resource group
  HMMSet *hset;        // Current HMMSet
  HTime sampPeriod;    // Sample period
  HTime stTime;        // Abs time of first frame
  HTime enTime;        // Abs time of last frame
  int outseqnum;       // sequence number of outgoing packets
  int trbakFreq;       // trace back frequency
  int frameCount;      // frame counter
  int tact;            // active model count
  string trbak;        // trace back text
  int trbakFrame;      // last traceback frame;
  float trbakAc;       // last traceback acoustic score
  int trbakCount;      // traceback counter
  int trbakLastWidth;  // size of last written trbak display
  PathMap opMap;       // set of output packets to avoid dups
  PacketList markers;  // list of packets to pass thru
  float score;         // current average score
  string fname;        // source file name (if any)
  HWin win;            // display window
  Boolean showRD;      // show display if TRUE
  string rdline;       // current output line of RD
  int inlevel;         // input buffer level
  int rdly,rdlyinc;    // y position of rdline and increment
  int rdx0,rdy0;       // origin of RD window
  int width,height;    // width and height of RD window
  int x0,x1,x2,x3,x4,x5,x6;  // RD display anchor points
  int x7,x8,x9,x10,x11,x12,x13,x14;
  int y0,y1,y2,y3,y4,y5,y6,y7,y8,y9;
  ABuffer *in;         // input buffer
  ABuffer *out;        // output buffer

  MemHeap ansHeap;     //for lattice generation
  MemHeap altHeap;
};


#endif
/*  -------------------- End of ARec.h --------------------- */

