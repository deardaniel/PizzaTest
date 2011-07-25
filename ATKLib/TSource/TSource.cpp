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
/*   File: TASource.cpp -     Test the Audio Input component   */
/* ----------------------------------------------------------- */

static const char * version="!HVER!TASource: 1.6.0 [SJY 01/06/07]";

// ----- Tests the ASource/HAudio i/o functions ------

// Basic operation is to simply consume input packets
// options exist to log to a file, playback input and
// emit a repeating tone burst.

#include "AMonitor.h"
#include "ASource.h"
#include "ACode.h"
#include "ARec.h"

#define INBUFID 10
#define ACKBUFID 11

static int seqnum = 0;     // current output message seq number
static ASource *asrc;      // the source component
static ABuffer *inBuf;     // sampled input data from ADC
static ABuffer *outBuf;    // sampled output data to DAC
static ABuffer *ackBuf;    // ack channel from output controller

#define OUTBUFSIZE 1600000
static short outbuf[OUTBUFSIZE];
static long outsize;       // num samples in outbuf

static Boolean isBeeping;  // true when emitting a tone
static float freq=1.0;     // tone freq in kHz
static float tdur=1.0;     // tone duration in secs
static float trep=3.0;     // tone repetition period in secs
static short *tonebuf;     // tone buffer
static long  tonesize;     // num samps in tone buf
static HTime beepTime;     // next time to play tone
static HTime tonePeriod;   // period in HTK units
static int maxcycles=0;    // max num cycles
static int numcycles=0;

static Boolean isPrompting;  // true if prompting rather than beeping
static char *promptFile;     // name of prompt file
static short *promptbuf;     // prompt buffer
static long promptsize;      // num samples in prompt buffer

static Boolean startImm = FALSE;  // set true to start sampling immediately
static Boolean isReplaying = FALSE;    // set to replay input utterances
static Boolean isLogging = FALSE; // set to enable logging of audio input
static char *logfileName;         // name of log file
static FILE *logfile;             // log file

// LoadPromptFile: load the htk wave file stored in promptFile
void LoadPromptFile()
{
   Wave w;
   HTime samp;
   short *p;

   w = OpenWaveInput(&gstack, promptFile, WAV, 0, 0, &samp);
   if (w==NULL)
      HError(999,"Cannot open WAV prompt file %s",promptFile);
   p = GetWaveDirect(w,&promptsize);
   if (p==NULL)
      HError(999,"Cannot access prompt file %s",promptFile);
   promptbuf = (short *)malloc(promptsize*sizeof(short));
   memcpy(promptbuf,p,promptsize*sizeof(short));
}

// InitTone:
void InitTone()
{
   float sampP = 625.0/10000000.0;
   tonesize= tdur/sampP+0.5;
   printf("SampP=%f  toneSize=%d\n",sampP,tonesize);
   tonebuf = (short *)malloc(tonesize*2);
   if (tonebuf==NULL) HError(9999,"Cannot malloc prompt buffer");
   for (int i = 0; i<tonesize; i++)
      tonebuf[i] = 10000*cos(TPI*freq*1000.0*i*sampP);
}

// SendOutput: send contents of buf to speech output channel
void SendOutput(short *buf, int n)
{
   AWaveData *wd;
   char cbuf[100];
   short *p;
   int size;

   ++seqnum;
   printf("TASrc: sending %d:%d\n",seqnum,n);
   sprintf(cbuf,"%d:%d",seqnum,n);
   AStringData *sd = new AStringData(string(cbuf));
   APacket hdrpkt(sd);
   hdrpkt.SetStartTime(GetTimeNow());
   hdrpkt.SetEndTime(GetTimeNow());
   outBuf->PutPacket(hdrpkt);
   p = buf;
   while (n>0){
      size = n;
      if (size>WAVEPACKETSIZE) size = WAVEPACKETSIZE;
      wd = new AWaveData(size,p);
      n -= size; p += size;
      APacket wavpkt(wd);
      outBuf->PutPacket(wavpkt);
   }
   sprintf(cbuf,"startout(%d)",seqnum);
   asrc->SendMessage(string(cbuf));
}

// GetWavePacket: get incoming wave packet and store/log it as necessary
void GetWavePacket()
{
   AWaveData *wp;
   APacket p = inBuf->GetPacket();
   if (p.GetKind() != WavePacket)
      HError(999,"GetWavePkt: Wave packet expected");
   wp = (AWaveData *)p.GetData();
   if (isReplaying){
      for (int i=0; i<wp->wused; i++) {
         if (outsize<OUTBUFSIZE)
            outbuf[outsize++] = wp->data[i];
      }
   }
   if (isLogging){
      int n = fwrite(wp->data,sizeof(short),wp->wused,logfile);
      if (n!=wp->wused)
         HError(999,"GetWavePacket: write failed to %s\n",logfileName);
   }
}

// IsStringPacket: return true if next packet is a string packet = s
Boolean IsStringPacket()
{
   APacket p = inBuf->PeekPacket();
   if (p.GetKind() == StringPacket) return TRUE;
   return FALSE;
}

// GetStringPacket: return string
string GetStringPacket()
{
   APacket p = inBuf->PeekPacket();
   if (p.GetKind() == StringPacket){
      AStringData *sd = (AStringData *) p.GetData();
      inBuf->PopPacket();
      return sd->data;
   }
   return "";
}

// GetAck: get ack packet and return command
void GetAck(string& ack)
{
   APacket p = ackBuf->GetPacket();
   if (p.GetKind() != CommandPacket)
      HError(999,"GetAck: Command packet expected");
   ACommandData * cd = (ACommandData *) p.GetData();
   ack = cd->GetCommand();
   printf("TASrc: ack rxed:"); cd->Show(); printf("\n");
}

void ReportUsage()
{
   printf("TASource -C config [options]\n");
   printf("   -l file     - log to file\n");
   printf("   -r          - replay each input\n");
   printf("   -s          - start sampling immediately\n");
   printf("   -t freq     - output a tone of given freq (in kHz)\n");
   printf("   -p file     - output HTK file instead of tone\n");
   printf("   -j time     - tone duration in seconds\n");
   printf("   -k time     - tone burst period in seconds\n");
   printf("   -m cycles   - stop after m cycles\n");
   exit(0);
}

int main(int argc, char *argv[])
{
   HEventRec e;
   string ack;
   int n;
   char *s;

   try {
      if (InitHTK(argc,argv,version)<SUCCESS){
         // if (NCInitHTK("TSource.cfg",version)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      // get options
      isPrompting = isBeeping = startImm = isLogging = FALSE;
      while (NextArg() == SWITCHARG) {
         s = GetSwtArg();
         if (strlen(s) !=1 )
            HError(999,"TSource: Bad switch %s; must be single letter",s);
         switch(s[0]){
            case 'l':
               logfileName = GetStrArg();
               logfile = fopen(logfileName,"wb");
               if (logfile==NULL)
                  HError(999,"TSource: cannot create logfile %s",logfileName);
               isLogging = TRUE;
               break;
            case 'p':
               isPrompting = TRUE;
               promptFile = GetStrArg();
            case 'r':
               isReplaying = TRUE; outsize = 0; break;
            case 's':
               startImm = TRUE; break;
            case 't':
               isBeeping = TRUE; freq = GetFltArg(); break;
            case 'j':
               tdur = GetFltArg(); break;
            case 'k':
               trep = GetFltArg(); break;
            case 'm':
               maxcycles = GetIntArg(); break;
            default:
               printf("Unknown option %s\n\n",s);
               ReportUsage(); exit(0);
         }
      }
      // Check settings
      if (isPrompting && isBeeping)
         HError(999,"Cannot set both tone and prompt output");
      if (isPrompting) LoadPromptFile();
      beepTime = GetTimeNow();
      tonePeriod = trep*1.0E7;

      // Create Buffers
      inBuf = new ABuffer("auInputChan");
      outBuf = new ABuffer("auOutputChan");
      ackBuf = new ABuffer("ackChan");

      inBuf->RequestBufferEvents(INBUFID);
      ackBuf->RequestBufferEvents(ACKBUFID);

      // Create Source
      asrc = new ASource("ASource",inBuf);

      // Attach an output channel
      asrc->OpenOutput(outBuf,ackBuf,625.0);

      // Setup tone output if needed
      if (isBeeping) InitTone();

      // Create Monitor and Start it
      AMonitor amon;
      amon.AddComponent(asrc);
      amon.Start();

      // Prompt for input, and replay it
      printf("Starting Audio Source Test\n");
      asrc->Start();
      if (startImm) asrc->SendMessage("start()");
      while (!asrc->IsTerminated()){
         if (isBeeping||isPrompting){
            HTime tnow = GetTimeNow();
            if (tnow > beepTime) {
               if (maxcycles>0) {
                  ++numcycles;
                  if (numcycles>maxcycles) {
                     asrc->SendMessage("stop()");
                     asrc->SendMessage("terminate()");
                     isBeeping=FALSE; isPrompting=FALSE;
                  }
               }
               if (isBeeping)
                  SendOutput(tonebuf,tonesize);
               if (isPrompting)
                  SendOutput(promptbuf,promptsize);
               beepTime = tnow + tonePeriod;
            }
         }
         e = HGetEvent(0,0);
         if (e.event == HTBUFFER) {
            // printf("    HTBUFFER %d, state=%d, inpkts=%d, ackpkts=%d\n",
            //   e.c,state,inBuf->NumPackets(),ackBuf->NumPackets());
            switch(e.c){
            case INBUFID:
               if (IsStringPacket()) {
                  string s = GetStringPacket();
                  if (s=="ASource::START"){
                  }else if (s=="ASource::STOP") {
                     SendOutput(outbuf,outsize);
                     outsize = 0;
                  }else if (s=="ASource::TERMINATE") {
                     printf("Source terminated\n");
                  }else {
                     printf("Custom string marker: %s\n",s.c_str());
                  }
               } else {
                  GetWavePacket();
               }
               break;
            case ACKBUFID:
               if (! ackBuf->IsEmpty()){
                  GetAck(ack);
               }
               break;
            }
         }
      }
      asrc->Join();
      // Shutdown
      printf("Waiting for monitor\n");fflush(stdout);
      amon.Terminate();
      HJoinMonitor();
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); }
   catch (HTK_Error e){ ReportErrors("HTK",e.i); }
   return 0;
}
