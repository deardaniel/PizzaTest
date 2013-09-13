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
/*     File: ASource.h -    Interface to the Audio Source      */
/* ----------------------------------------------------------- */

/* !HVER!ASource: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
//         SOURCEFORMAT = HAUDIO  -- source is direct audio
// ASOURCE: DISPSHOW    = T       -- create a volume meter
// ASOURCE: DISPXORIGIN = 30      -- top-left X coord of VM Window
// ASOURCE: DISPYORIGIN = 10      -- top-left Y coord of VM Window
// ASOURCE: DISPHEIGHT  = 30      -- height of volume meter
// ASOURCE: DISPWIDTH   = 120     -- width of volume meter
// ASOURCE: WAVEFILE    = ""      -- source an audio file
// ASOURCE: WAVELIST    = ""      -- list of audio source files
// ASOURCE: FLUSHMARGIN = 0.0     -- duration of zeros inserted on Stop
// ASOURCE: NORMALVOLUME = 100    -- default volume for output
// ASOURCE: MUTEDVOLUME = 50      -- muted volume
// ASOURCE: EXTRABUTTON = ''      -- define to create extra control button

#ifndef _ATK_ASource
#define _ATK_ASource

#include "AComponent.h"

class ASource: public AComponent {
public:
  ASource(){}
  // general graphical interface version
  ASource(const string & name, ABuffer *outb, HWin cntlWin = NULL,
          int cx0 = 0, int cy0 = 0, int cx1 = 0, int cy1 = 0);
  // version for use by AVite to read only from filelist
  ASource(const string & name, ABuffer *outb, const string &filelist);
  // Attach an output channel for piping waveforms to the audio output device
  // the ackchan is a reply channel to report status.
  void OpenOutput(ABuffer *spoutb, ABuffer *ackchanb, HTime sampPeriod);
  // Start the task operating - needs to be highest priority to avoid
  // audio input buffer over-run, and pauses in output
  void Start(HPriority priority=HPRIO_HIGH);
  // Get the input sampling period
  HTime GetSampPeriod();
  AudioOut ao;         // output device
  AudioIn ain;         // HTK audio input object
private:
  friend TASKTYPE TASKMOD ASource_Task(void *p);
  void CommonInit(const string & name, ABuffer *outb);
  void ReadWaveList(char *fn);
  void DrawVM(int level);
  void DrawButton();
  void ButtonPressed(int butid);
  void StartCmd();
  void StopCmd();
  void SendMarkerPkt(string marker);
  APacket MakePacket(Boolean &isEmpty);
  void ExecCommand(const string & cmdname);
  MemHeap mem;         // heap for HWave input
  FileFormat fmt;      // source format (default HAUDIO)
  Wave w;              // alternative audio source file
  list<string> wfnList; // list of file names
  short *wbuf;         //  ... its data
  long wSamps;         //  ... num samps in wbuf
  long widx;           //  ... index into wbuf
  long sampSent;       // total samples sent
  HTime flushmargin;   // duration of zeros to send when STOP cmd rxed (default=0)
  long flushsamps;     // number of flush samples left
  HTime timeNow;       // used to track time
  HTime sampPeriod;    // Sample period
  Boolean stopping;    // TRUE when stopping sampling
  Boolean stopped;     // TRUE when sampling is temporarily stopped
  int mutevolume;      // muted volume level
  int normvolume;      // normal volume level
  HWin win;            // display window
  Boolean showVM;      // show volmeter if TRUE
  int oldLevel;        // old display level
  int vmx0,vmy0;       // origin of VM window
  int width,height;    // width and height of VM window
  int x0,y0,x1,y1,x2;  // VM fixed points
  ABuffer *out;        // output buffer
  HButton ssb;         // start/stop button
  HButton xtb;         // extra button
  char xtbname[12];    // extra button name
  char ssbname[12];    // button name
  Boolean hasXbt;      // display has an extra button
  int trace;           // tracing
  // -------------- non standard SJY extension ------
  // --------------- speech output channel ----------
  //  NB only 16 bit wave packets supported
  void StartOutCmd();  // command messages
  void AbortOutCmd();
  void MuteOutCmd();
  void UnmuteOutCmd();
  void StatusOutCmd();
  Boolean GetOutPktHdr();     // get next pkt header
  Boolean CheckPlaying();     // check if output still playing and
                              // if so, send finished message
  void AckOutCmd(string cmd); // send ack to outack
  int WavePktToAudioOut(int remaining);
                       // out next pkt and return size
  ABuffer *spout;      // speech output channel
  ABuffer *outack;     // acknowledgement channel

  HTime outSampP;      // output sample period
  int outSeqNum;       // current output seq number
  int outSamples;      // num samples in current utterance
  int outSampLeft;     // num samples left to play
  Boolean isPlaying;   // true when output in progress
};



#endif

/*  -------------------- End of ASource.h --------------------- */

