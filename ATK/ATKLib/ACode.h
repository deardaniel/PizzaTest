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
/*     File: ACode.h -    Interface to the feature enCoder     */
/* ----------------------------------------------------------- */

/* !HVER!ACode: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)

// 	    TARGETRATE      = 10000*        -- sampling rate (in 100ns units)
// HPARM: TARGETKIND    = MFCC_E_D_A_N* -- coded feature kind
// HPARM: ENORMALISE	= FALSE*        -- dont normalise energy
// HPARM: NUMCHANS	    = 22*	        -- number of filter bank channels
// HPARM: NUMCEPS	    = 12*	        -- number of cepstral coefficients
// HPARM: CALWINDOW     = 30*           -- num frames to calibrate speech det
// HPARM: SILDISCARD    = 10.0*	        -- ignore frames with lower energy
// HPARM: SPCSEQCOUNT   = 10            -- min speech frames in seq to trigger
// HPARM: SILSEQCOUNT   = 100           -- min silence frames in seq to trigger
// HPARM: SPCGLCHCOUNT  = 0             -- max ignorable glitches in speech
// HPARM: SILGLCHCOUNT  = 2             -- max ignorable glitches in silence
// ACODE: TRACE         = 0             -- trace flag
// ACODE: NUMSTREAMS    = 1	            -- number of streams
// ACODE: SHOWDISP      = T             -- display running feature gram
// ACODE: DISPXORIGIN   = 420           -- top-left X coord of display window
// ACODE: DISPYORIGIN   = 80            -- top-left Y coord of display window
// ACODE: DISPHEIGHT    = 220           -- height of volume meter
// ACODE: DISPWIDTH     = 400           -- width of volume meter
// ACODE: MAXFEATS      = 1000          -- max num features to display

#include <stdio.h>
#ifndef _ATK_ACode
#define _ATK_ACode

#include "AComponent.h"
#include "AHmms.h"
#define INBUFSIZE 5000

static Ptr xOpen(Ptr xInfo, char *fn, BufferInfo *info);
static int xNumSamp(Ptr xInfo, Ptr bInfo);
static int xGetData(Ptr xInfo, Ptr bInfo, int n, Ptr data);

class ACode: public AComponent {
public:
  ACode(){}
  ACode(const string & name, ABuffer *inb, ABuffer *outb, char *confName=NULL);
  void Start(HPriority priority=HPRIO_NORM);
  AObsData * GetSpecimen();
  HTime GetSampPeriod();
  void SetCodeHMMSet(AHmms *ahmm);
private:
  friend TASKTYPE TASKMOD ACode_Task(void *p);
  friend Ptr xOpen(Ptr xInfo, char *fn, BufferInfo *info);
  friend int xNumSamp(Ptr xInfo, Ptr bInfo);
  friend int xGetData(Ptr xInfo, Ptr bInfo, int n, Ptr data);
  void PaintCol(int col, int fgIdx);
  void ShowFrame();
  void SetScaling();
  void RedrawDisplay();
  void UpdateDisplay();
  void DisplaySilDetParms();
  void DrawStatus();
  void DrawFG(APacket pkt);
  void AddMkr2FG(APacket pkt);
  Boolean HoldReady(HTime tnow);
  void ForwardMkr();
  void ForwardOldMkrs(HTime tnow);
  void ForwardAllMkrs();
  void HoldPacket(APacket p);
  void DrawButton();
  void DisplayTime(HTime t);
  void ButtonPressed();
  void CalibrateCmd(HTime when=0.0);
  APacket CodePacket();
  void ExecCommand(const string & cmdname);
  MemHeap mem;         // heap for HParm
  short inBuffer[INBUFSIZE];  // array for input samples
  int inBufUsed;       // num samples in the inBuffer
  int numStreams;      // number of observation streams
  BufferInfo info;     // Parameter buffer info record
  ParmBuf pbuf;        // The actual parameter buffer
  Boolean isFlushing;  // True when flushing out frames
  HParmSrcDef ext;     // "external" source defn (ie ASource)
  HWin win;            // FG display window
  Boolean showFG;      // show featuregram if TRUE
  Boolean FGinit;      // true when display initialised
  int fgx0,fgy0;       // origin of FG window
  int width,height;    // width and height of FG window
  int x0,y0,x1,y1,x2,y2,x3,y3,x4,y4;  // FG fixed points
  int cellw,cellh;     // cell width and height
  int maxFrames;       // max frames in FG
  int nFrames;         // num frames in FG
  int nFeats;          // num of features per frame
  int maxFeats;        // max features allowed
  int lastIn;          // index of last frame added
  int lastDisplayTime; // last display time
  APacket *fgo;        // array[0..maxFrames-1] of Packet
  Boolean *spDet;      // array[0..maxFrames-1] of Boolean
  float *scale;        // grey scale value =
  float *offset;       //   fv[i]*scale[i]+offset[i]
  ABuffer *in;         // input buffer
  ABuffer *out;        // output buffer
  int trace;           // trace flag
  HButton calb;        // calibrate button
  char calbname[20];   // name of calibrate button
  HTime timeNow;       // current time
  list<APacket> holdList;  // packet hold list
  //  typedef list<APacket>::iterator PktEntry;
  ChannelInfoLink chan;  //optional channel info
};


#endif
/*  -------------------- End of ACode.h --------------------- */

