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
/*     File: AIO.h -  High Level IO Interface to ATK           */
/* ----------------------------------------------------------- */

/* !HVER!AIO: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
//
// AIO:

#ifndef _ATK_AIO
#define _ATK_AIO

#include "AComponent.h"
#include "ASource.h"
#include "ACode.h"
#include "ASyn.h"
#include "ARec.h"
#include "AMonitor.h"
#include "ATee.h"
#include "ALog.h"
#include "FliteSynthesiser.h"

// This componenet provides a high level interface to the main ATK
// components.  Inputs are via the SendMessage interface:
//
// tell(text) - provide information
// ask(text)  - ask a question
// setgroup(rgroup) - set resource group in embedded ARec component
// closedown()      - close down subcomponents
//
// Outputs are returned via a buffer and consist of
//
// command packets providing status information:
//      synFinished()
//      synInterrupted(lastIdx,lastWord,percentOutput)
//      asrTimeOut()
//      terminated()
//
// phrase packets - unprocessed arec output packets
//
//
// AIO utilises 2 interlinked state machines
//
enum ASRState {
   asr_idle,        // waiting for a START packet from rec
   asr_listening,   // start received
   asr_filling,     // so far only fillers decoded
   asr_recognising  // user is really saying something
};

enum ASREvent {
   AsrStartEv,  // start packet received
   AsrWordEv,   // word packet received
   AsrEndEv,    // end packet received
   AsrFillEv,   // filler word received
   AsrTimerEv,  // timeout
   AsrMarkerEv, // nonASR string marker to pass thru
   AsrOtherEv   // everything else
};

enum SYNMode {
   syn_telling,     // no response required
   syn_asking,      // response required so enable timer
   syn_yelling,     // disable AIn for duration of utt
   syn_self         // talk to self, ie, no mute/abort
};

enum SYNState {
   syn_idle,        // waiting for tell or ask
   syn_talking,     // syn talking, user quiet
   syn_muted,       // syn talking, user talking
   syn_aborted      // abort sent, waiting for finished
};

typedef pair<string,int> FillerPair;
typedef map<string,int>  FillerMap;

struct TalkCmd {
   string text;
   SYNMode mode;
};

typedef list<TalkCmd> TalkCmdQueue;

class AIO: public AComponent {
public:
   AIO(){}
   // Create an IO subsystem using the resource manager appRman
   // if supplied, else using an internally created default.
   // AIO will invoke the following subsidiary threads:
   //   aud, code, rec, syn plus a local timer thread.
   AIO(const string & name, ABuffer *outChannel, ARMan *appRman=NULL, Boolean useLogging=FALSE);
   // Attach a monitor if required.  This methods simply calls
   // AddComponent for each of the components embedded in AIO.
   void AttachMonitor(AMonitor *amon);
   // Define a filler word, call once for each filler
   void DefineFiller(const string& word);
   // Start the interface thread ( this will also start all of the
   // subcomponents: aud, code, syn, rec, timer )
   void Start(HPriority priority=HPRIO_NORM);
private:
   friend TASKTYPE TASKMOD AIO_Task(void *p);
   friend TASKTYPE TASKMOD TimerTask(void *p);
   friend TASKTYPE TASKMOD DisplayTask(void *p);
   void ExecCommand(const string & cmdname);
   void SendString(const string& s, ABuffer *b);
   void SendCommand(const string& cmd, ABuffer *b, Boolean withIntInfo);
   void ForwardRecPkts();
   void SynthOutput(SYNMode m);
   void StepSynState(APacket p);
   void StepAsrState(APacket p);
   void SetTimer();
   void ClearTimer();
   void UnhandledSynCmd(string cmd);
   void UnhandledAsrCmd(ASREvent e);
   ASREvent MapAsrEvent(APacket p);
   Boolean IsFiller(const string& word);
   void CheckTQ();
   ASource *aud;      // audio input/output
   FSynthesiser *synDevice; // use Flite Synthesiser
   ASyn    *syn;      // tts
   ACode   *code;     // front-end feature encoder
   ARec    *rec;      // recogniser
   ABuffer *waveOut;  // syn -> aud for synth playing
   ABuffer *waveLog;  // tee->log for save
   ABuffer *waveCode;  // tee->coder
   ABuffer *synAck;   // aud -> syn for acks
   ABuffer *waveIn;   // aud -> code for user input
   ABuffer *featIn;   // code -> rec for features
   ABuffer *synRep;   // syn -> aio for synthesiser replies
   ABuffer *asrIn;    // asr -> aio for recognition results
   ABuffer *outChan;  // aio -> application
   HThread timer;     // Timer for asr timeouts
   int timeOutPeriod; // Allowed time before user response
   int timeOut;       // current timeout in msecs
   HSignal tsig;
   HLock   tlock;
   Boolean timing;    // current status of timer
   Boolean toutFlag;  // set after timeout for display use
   ARMan   *rman;     // Resource manager
   ResourceGroup *main;
   AHmms   *hset;     // default resources
   AGram   *gram;
   ADict   *dict;
   ANGram  *ngram;
   ATee *tee;
   ALog *log;             //logging components
   ASRState astate;       // ASR state machine
   SYNState sstate;       // SYN state machine
   SYNMode  smode;
   TalkCmdQueue talkq;    // pending talk commands
   string  text;          // current synthesis text
   FillerMap fillers;     // list of filler words
   int synIntIdx;         // last syn interrupt
   string synIntWord;
   float  synIntPercent;
   list<APacket> recPkts; // temp buffer till filler/word determined
   HWin win;              // AIO display window
   Boolean showDisp;      // show display if TRUE
   int dx0,dy0;           // origin of FG window
   int width,height;      // width and height of FG window
   int x0,y0,x1,y1,y2;    // Display fixed points
   int xstep;             // horizantal step size
   int barHeight;         // height of status bars
   int ticker;            // counter for time ticks
   HThread display;       // display thread - updates approx every 10msecs
   int trace;
   Boolean logData;       // use an ALog component
   Boolean disableBargeIn;
   Boolean PauseTimeOut;
};

#endif

/* -------------------- End of AIO.h ------------------- */

