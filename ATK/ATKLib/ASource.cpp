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
/*    File: ASource.cpp -    Implementation of Audio Source    */
/* ----------------------------------------------------------- */

char * asource_version="!HVER!ASource: 1.6.0 [SJY 01/06/07]";

// Modification history:
//   9/12/02 - added PRBUFSIZE for printf display monitor
//   5/08/03 - standardised display config var names
//  28/07/04 - included dc mean removal
//  11/08/04 - static variables removed
//  19/08/05 - speech output added
//  18/03/06 - speech output flushing modified

#include "ASource.h"

#define T_TOP 0001    // top level tracing
#define T_OUT 0002    // trace output packets
#define T_SPO 0004    // trace speech output

#define ASOURCEPRBUFSIZE 4

void ASource::CommonInit(const string & name, ABuffer *outb)
{
   int numParm;
   int i; double f;
   string wfname;
   Boolean b;
   char buf[100];
   ConfParam *cParm[MAXGLOBS];       /* config parameters */

   strcpy(buf,name.c_str());

   stopped = TRUE; out = outb;
   normvolume = 100; mutevolume = 50;
   width = 120; height=30; showVM = FALSE; sampSent = 0;
   timeNow = 0.0; flushmargin = 0.0; oldLevel = -1;
   sampPeriod = 0.0;  // ie default is to use device setting
   outSeqNum = 0; isPlaying = FALSE; trace=0;
   ain = NULL; ao = NULL;
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   hasXbt = FALSE;
   if (numParm>0){
      if (GetConfBool(cParm,numParm,"DISPSHOW",&b)) showVM = b;
      if (GetConfInt(cParm,numParm,"DISPXORIGIN",&i)) vmx0 = i;
      if (GetConfInt(cParm,numParm,"DISPYORIGIN",&i)) vmy0 = i;
      if (GetConfInt(cParm,numParm,"DISPWIDTH",&i)) width = i;
      if (GetConfInt(cParm,numParm,"DISPHEIGHT",&i)) height = i;
      if (GetConfInt(cParm,numParm,"NORMALVOLUME",&i)) normvolume = i;
      if (GetConfInt(cParm,numParm,"MUTEDVOLUME",&i)) mutevolume = i;
      if (GetConfStr(cParm,numParm,"EXTRABUTTON",buf)){
         buf[11] = '\0';   // max length is 11 chars
         strcpy(xtbname,buf);
         hasXbt = TRUE;
      }
      if (GetConfStr(cParm,numParm,"WAVEFILE",buf)) {
         wfname = buf; wfnList.push_back(wfname);
      }
      if (GetConfStr(cParm,numParm,"WAVELIST",buf))ReadWaveList(buf);
      if (GetConfFlt(cParm,numParm,"FLUSHMARGIN",&f)) flushmargin = f;

      if (GetConfStr(cParm,numParm,"SOURCEFORMAT",buf))
         fmt = Str2Format(buf);
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
}

// ASource constructor, if cntlWin is supplied, then the volume control
// is drawn in the supplied window, otherwise ain dedicated window is
// generated with ain start/stop button
ASource::ASource(const string & name, ABuffer *outb, HWin cntlWin,
                 int cx0, int cy0, int cx1, int cy1)
                 : AComponent(name,(HasRealConsole()?2:ASOURCEPRBUFSIZE))
{
   vmx0 = 30;   vmy0 = 10;  win = cntlWin;
   fmt = HAUDIO;      // default is to use direct audio input
   CommonInit(name,outb);
   x1 = cx0; x2 = cx1; y0 = cy0; y1 = cy1;    // used when drawing VM in ext win
}

// ASource constructor for specific file list
ASource::ASource(const string & name, ABuffer *outb, const string &filelist)
: AComponent(name,ASOURCEPRBUFSIZE)
{
   char buf[100];
   vmx0 = vmy0 = 0;  win = NULL;
   fmt = HTK;      // default is HTK file format
   CommonInit(name,outb);
   strcpy(buf,filelist.c_str());
   ReadWaveList(buf);
   x1 = x2 = y1 = 0;
}

// Read ain list of file names from given file and store in wfnList
void ASource::ReadWaveList(char *fn)
{
   char listfn[512],buf[512],*s;
   int len;
   Boolean ok;
   FILE *f;

   strcpy(listfn,fn);
   ok = ((f = fopen(listfn,"r")) != NULL)?TRUE:FALSE;
   if (!ok){
      if (listfn[0]=='/') {
         listfn[0] = listfn[1]; listfn[1]=':';
         ok = ((f = fopen(listfn,"r")) != NULL)?TRUE:FALSE;
      }
   }
   if (! ok){
      HRError(10110,"Cannot open wavelist %s",fn);
      throw ATK_Error(10110);
   }
   while (fgets(buf,511,f) != NULL){
      for (s=buf; *s==' '; s++);
      if (*s != '\n'){
         len = strlen(s);
         if (s[len-1]=='\n') s[len-1] = '\0';
         wfnList.push_back(string(s));
      }
   }
}

// Start the task
TASKTYPE TASKMOD ASource_Task(void * p);
void ASource::Start(HPriority priority)
{
   AComponent::Start(priority,ASource_Task);
}

// Return SampPeriod
HTime ASource::GetSampPeriod()
{
   return sampPeriod;
}

// Draw Button
void ASource::DrawButton()
{
   if (stopping)
      strcpy(ssbname,"Start");
   else
      strcpy(ssbname,"Stop");
   RedrawHButton(&ssb);
}

// Button Press
void ASource::ButtonPressed(int butid)
{
   if (butid==ssb.id){
      if (stopped)
         StartCmd();
      else
         StopCmd();
   }else if (butid==xtb.id){
      SendMarkerPkt(string(xtb.str));
   }
}

// Draw VolMeter - level = 0 to 100, note that volMeter is normally
// drawn inside ASource's own windown.  But if DISPSHOW is false but
// win is not NULL, then win belongs to some other thread which has
// requested ain volume meter.   In this latter case, x0 and y0
// are set by DISPXORIGIN and DISPYORIGIN ie relative to supplied window
//
void ASource::DrawVM(int level)
{
   const int margin = 4;
   const int bmargin = 6;
   int sbw,xbw;

   if (oldLevel < 0){
      // set up position info
      if (showVM) {
         x0 = margin; y0 = margin;
         x2 = width-margin;   y1 = height-margin;
         sbw = HTextWidth(win,"Start") + bmargin;
         x1 = x0 + sbw + margin;
         if (hasXbt) {
            xbw = HTextWidth(win,xtbname) + bmargin;
            x1 += xbw + margin;
         }
         // paint background
         HSetGrey(win,40);
         HFillRectangle(win,0,0,width,height);
         // create start/stop button
         ssb.x = x0; ssb.y = y0; ssb.w = sbw; ssb.h = height-2*margin;
         ssb.fg = DARK_GREY; ssb.bg = LIGHT_GREY; ssb.lit = FALSE;
         ssb.active = TRUE; ssb.toggle = TRUE;  ssb.fontSize = 0;
         strcpy(ssbname,"Start"); ssb.str = ssbname;
         ssb.id = 1; ssb.win = win;
         ssb.next = 0; ssb.action = 0;
         RedrawHButton(&ssb);
         if (hasXbt){
            // create xtra button
            xtb.x = x0+sbw+margin; xtb.y = y0; xtb.w = xbw; xtb.h = height-2*margin;
            xtb.fg = DARK_GREY; xtb.bg = LIGHT_GREY; xtb.lit = FALSE;
            xtb.active = TRUE; xtb.toggle = FALSE;  xtb.fontSize = 0;
            xtb.str = xtbname;
            xtb.id = 2; xtb.win = win;
            xtb.next = 0; xtb.action = 0;
            RedrawHButton(&xtb);
         }
         // draw outline for volume control
         HSetGrey(win,20);
         HDrawRectangle(win,x1-1,y0-1,x2+1,y1+1);
      }
   }
   if (level != oldLevel){
      float fx = float(level/100.0);
      int x = x1 + int((x2-x1)*fx);
      int xmax = x1 + int(0.7*(x2-x1));

      HSetColour(win,YELLOW);
      HFillRectangle(win,x1,y0,x2,y1);
      HSetColour(win,DARK_GREEN);
      HFillRectangle(win,x1,y0,x<xmax?x:xmax,y1);
      if (x>xmax){
         HSetColour(win,RED);
         HFillRectangle(win,xmax,y0,x<x2?x:x2,y1);
      }
   }
   oldLevel = level;
}

// Start the audio sampling
void ASource::StartCmd()
{
   string wfn;
   char buf[512];

   if (fmt == HAUDIO){
      timeNow = GetTimeNow();
      SendMarkerPkt("START");
      ain = OpenAudioInput(&sampPeriod);
      if (ain == NULL){
         HRError(10106,"ASource::StartCmd: Cannot open audio device");
         throw HTK_Error(10106);
      }
      StartAudioInput(ain);
   }else {
      timeNow = 0.0;
      if (wfnList.size()==0){
         SendMarkerPkt("ENDOFLIST");
         return;
      }
      wfn = wfnList.front(); wfnList.pop_front();
      SendMarkerPkt("START ("+wfn+")");
      strcpy(buf,wfn.c_str());
      w = OpenWaveInput(&mem, buf, fmt, 0.0, 0.0, &sampPeriod);
      if (w==NULL){
         HRError(10110,"ASource::StartCmd: Cannot open wave %s",wfn.c_str());
         throw HTK_Error(10100);
      }
      wbuf = GetWaveDirect(w,&wSamps);
      widx = 0;
   }
   stopped = FALSE; stopping = FALSE;
   flushsamps = (long int) (flushmargin/sampPeriod);
   if (showVM) DrawButton();
}

// Stop the audio sampling
void ASource::StopCmd()
{
   if (fmt == HAUDIO){
      StopAudioInput(ain);
      SendMarkerPkt("PAUSE");
   }
   stopping = TRUE;
   if (showVM) DrawButton();
}

// Create and fill ain wave data packet
APacket ASource::MakePacket(Boolean &isEmpty)
{
   int sampsAvail,sampsInAudio,pktsInAudio;

   // create ain wavedata container and fill it
   AWaveData *wd = new AWaveData();
   isEmpty = TRUE;
   if (fmt == HAUDIO){
      //  The normal source for ATK ie raw audio input
      if (stopping) {
         if (PacketsInAudio(ain) > 0) {
            GetAudio(ain,wd->data); isEmpty = FALSE;
         }else if (flushsamps>0) {
            for (int i=0; i<WAVEPACKETSIZE; i++){
               wd->data[i] = (short int) FakeSilenceSample();
               --flushsamps;
            }
            isEmpty = FALSE;
         }
         if (stopping && (PacketsInAudio(ain)==0) && (flushsamps<=0)){
            stopped = TRUE;
            CloseAudioInput(ain);  ain=NULL;
         }
      } else {
         GetAudio(ain,wd->data); isEmpty = FALSE;
      }
   } else {
      // The alternate source for testing - ain file
      sampsAvail = wSamps-widx + flushsamps;
      if (sampsAvail>WAVEPACKETSIZE) sampsAvail=WAVEPACKETSIZE;
      if (sampsAvail < WAVEPACKETSIZE) {
         stopping = TRUE; stopped = TRUE;
         if (showVM) DrawButton();
      }
      for (int i=0; i<sampsAvail; i++,widx++)
         wd->data[i] = (widx<wSamps)?wbuf[widx]:0;
      if (stopped) CloseWaveInput(w);
      if (sampsAvail<WAVEPACKETSIZE)  // pad with zeros
         for (int i=sampsAvail; i<WAVEPACKETSIZE; i++)
            wd->data[i] = (short int) FakeSilenceSample();
      isEmpty = FALSE;
   }
   wd->wused = WAVEPACKETSIZE;
   // wrap it with ain packet and return it.
   APacket newpkt(wd);
   newpkt.SetStartTime(timeNow);
   timeNow +=sampPeriod*WAVEPACKETSIZE;
   newpkt.SetEndTime(timeNow);
   return newpkt;
}

// Send ain marker packet to output
void ASource::SendMarkerPkt(string marker)
{
   AStringData *sd = new AStringData(cname+"::"+marker);
   APacket mkrpkt(sd);
   mkrpkt.SetStartTime(timeNow);
   mkrpkt.SetEndTime(timeNow);
   out->PutPacket(mkrpkt);
   if (trace&T_OUT)mkrpkt.Show();
}

// Implement the command interface
void ASource::ExecCommand(const string & cmdname)
{
   char buf[100];

   if (cmdname == "start")
      StartCmd();
   else if (cmdname == "stop")
      StopCmd();
   else if (cmdname == "startout")
      StartOutCmd();
   else if (cmdname == "abortout")
      AbortOutCmd();
   else if (cmdname == "muteout")
      MuteOutCmd();
   else if (cmdname == "unmuteout")
      UnmuteOutCmd();
   else if (cmdname == "statusout")
      StatusOutCmd();
   else {
      sprintf(buf,"Unknown command %s\n",cmdname.c_str());
      HPostMessage(HThreadSelf(),buf);
   }
}

// --------------------- parallel output facility ---------------

// Input packets are either string or waveform packets.
// String packets are of the form ss:nnnn  where ss is
// ain sequence number and nnnn is ain sample count. The
// nnnn samples then follow in one or more waveform pkts.
// Input packets are left in the input buffer until
// ASource receives command messages:
//
//   startout(ss) - start outputting message ss.  Any incoming
//                  waves with seq num < ss are discarded.
//                  If next wave has seq num > ss, then reply
//                  "error()" else start output and reply
//                  "started()".
//   muteout()   -  If output is active reduce volume & reply
//                  "muted()" else reply "idle()".
//   unmuteout()  - If output is active restore volume & reply
//                  "unmuted()" else reply "idle()".
//   abortout()   - If output is active, stop it, flush remainder
//                  of input and reply "aborted()"
//                  else reply "idle()".
//   statusout()  - Returns "playing()" or "idle()"
//
// whenever an output finishes normally ain "finished()" cmd is returned.
// All replies contain 3 args  (seqnum, samplesPlayed, total Samples).
// When finish is normal, then finished() has samplesPlayed=0.
//

// GetOutPktHdr: get next output packet header.  Any intervening
// wave packets are discarded.
Boolean ASource::GetOutPktHdr()
{
   APacket pkt;
   PacketKind kind;
   do {
      if (spout->IsEmpty()) return FALSE;
      pkt = spout->GetPacket(); kind = pkt.GetKind();
      if (trace&T_SPO && kind != StringPacket)
         printf("SpOut: skipping data packet\n");
   } while (kind != StringPacket);
   AStringData * sd = (AStringData *)pkt.GetData();
   string::size_type len = sd->data.length();
   if (len > 99) {
      HRError(10201,"ASource::GetOutPktHdr - pkt hdr too long");
      throw ATK_Error(10201);
   }
   string::size_type colonpos = sd->data.find(":");
   if (colonpos==string::npos) {
      HRError(10201,"ASource::GetOutPktHdr - bad pkt hdr format");
      throw ATK_Error(10201);
   }
   string ss = sd->data.substr(0,colonpos);
   string nn = sd->data.substr(colonpos+1,len-colonpos-1);
   outSeqNum = atoi(ss.c_str()); outSamples = atoi(nn.c_str());
   if (trace&T_SPO)
      printf("SpOut: Header [%d:%d] rxed\n",outSeqNum,outSamples);
   return TRUE;
}

// AckOutCmd: send ack packet to outack buffer
void ASource::AckOutCmd(string cmd)
{
   char buf[100];
   string acks;
   buf[0]='\0';
   int played = outSamples - outSampLeft;
   if (played<0) played = 0;
   if (played>outSamples) played = outSamples;
   ACommandData * cd = new ACommandData(cmd);
   cd->AddArg(outSeqNum);
   cd->AddArg(played);
   cd->AddArg(outSamples);
   APacket ackpkt(cd);
   ackpkt.SetStartTime(timeNow); ackpkt.SetEndTime(timeNow);
   outack->PutPacket(ackpkt);
   if (trace&T_SPO){
      printf("SpOut: ack sent:"); cd->Show(); printf("\n");
   }
}

// WavePktToAudioOut: get next wavepkt from spout buffer and play it
int ASource::WavePktToAudioOut(int remaining)
{
   APacket pkt;
   PacketKind kind;
   if (spout->IsEmpty()) return 0;
   pkt = spout->GetPacket(); kind = pkt.GetKind();
   if (kind != WavePacket){
      HRError(10204,"ASource::WavePktToAudioOut - wave pkt expected [%d]",kind);
      throw ATK_Error(10204);
   }
   AWaveData *wp = (AWaveData *)pkt.GetData();
   Boolean lastblock = FALSE;
   if (remaining <= wp->wused) lastblock = TRUE;
   PlayAudioOutput(ao,wp->wused,wp->data,lastblock,ain);
   return wp->wused;
}

// Attach an input channel for piping to the audio output device
void ASource::OpenOutput(ABuffer *spoutb, ABuffer *ackchanb, HTime sampPeriod)
{
   outSampP = sampPeriod;
   spout = spoutb;
   outack = ackchanb;
   ao =  OpenAudioOutput(&outSampP);
   if (ao==NULL) {
      HRError(10200,"ASource::OpenOutput - cant open out device");
      throw ATK_Error(10200);
   }
   outSeqNum = 0;  outSamples = 0; outSampLeft = 0;
}

// StartOutCmd: message interface startout command
void ASource::StartOutCmd()
{
   int ss;

   if (! GetIntArg(ss, 0, -1)) {
      HRError(10201,"ASource::StartOutCmd - invalid seq number");
      throw ATK_Error(10201);
   }
   if (trace&T_SPO)
      printf("SpOut: startout(%d) rxed\n",ss);
   // ensure volume is normal
   SetOutVolume(ao,normvolume);
   // first get header packet ss
   do {
      if (! GetOutPktHdr()) {
         HRError(10202,"ASource::StartOutCmd - invalid out pkt header");
         throw ATK_Error(10202);
      }
   }while (outSeqNum<ss);
   // if not found then ack it and return
   if (ss<outSeqNum) { AckOutCmd("error"); return; }
   // otherwise pipe the whole waveform to the output device
   int toPlay = outSamples;
   outSampLeft = outSamples;
   AckOutCmd("started"); isPlaying=TRUE;
   SendMarkerPkt("SYNTHSTART");
   while (toPlay > 0) {
      int n = WavePktToAudioOut(toPlay);
      if (n==0){
         HRError(10202,"ASource::StartOutCmd - data truncated");
         throw ATK_Error(10202);
      }
      toPlay -= n;
   }
}

// AbortOutCmd: message interface abortout command
void ASource::AbortOutCmd()
{
   if (trace&T_SPO)
      printf("SpOut: abortout() rxed\n");
   if (isPlaying){
      outSampLeft = SamplesToPlay(ao);
      FlushAudioOutput(ao);
      AckOutCmd("aborted");
      isPlaying=FALSE;
   }else{
      AckOutCmd("idle");
   }
}

// MuteOutCmd: message interface muteout command
void ASource::MuteOutCmd()
{
   if (trace&T_SPO)
      printf("SpOut: muteout() rxed\n");
   if (isPlaying){
      outSampLeft = SamplesToPlay(ao);
      SetOutVolume(ao,mutevolume);
      AckOutCmd("muted");
   }else{
      AckOutCmd("idle");
   }
}

// UnmuteOutCmd: message interface unmuteout command
void  ASource::UnmuteOutCmd()
{
   if (trace&T_SPO)
      printf("SpOut: unmuteout() rxed\n");
   if (isPlaying){
      outSampLeft = SamplesToPlay(ao);
      SetOutVolume(ao,normvolume);
      AckOutCmd("unmuted");
   }else{
      AckOutCmd("idle");
   }
}

// StatusOutCmd: message interface statusout command
void  ASource::StatusOutCmd()
{
   if (isPlaying){
      outSampLeft = SamplesToPlay(ao);
      AckOutCmd("playing");
   }else{
      AckOutCmd("idle");
   }
}

// -------------------- ASource task ----------------------------
TASKTYPE TASKMOD ASource_Task(void * p)
{
   ASource *asp = (ASource *)p;
   APacket pkt;
   int i;
   char buf[100],cname[100];
   HEventRec e;
   AWaveData *wd;
   Boolean isEmpty;

   try{
      CreateHeap(&(asp->mem), "ASourceStack", MSTAK, 1, 1.0, 10000, 50000);
      strcpy(cname,asp->cname.c_str());
      if (asp->showVM && asp->win == NULL){
         asp->win = MakeHWin(cname,asp->vmx0,asp->vmy0,asp->width,asp->height,1);
         if (asp->win==NULL) HError(999,"ASource cannot create volume window\n");
      }
      if (asp->win != NULL) asp->DrawVM(0);
      asp->RequestMessageEvents();
      if (asp->trace&T_TOP)
         printf("ASource: starting main loop\n");
      while (!asp->IsTerminated()){
         if (!asp->stopped){
            pkt = asp->MakePacket(isEmpty);
            if (!isEmpty){
               wd = (AWaveData *)pkt.GetData();
               if (asp->win != NULL){
                  asp->DrawVM((int)GetCurrentVol(asp->ain));
               }
               asp->out->PutPacket(pkt);
               if (asp->trace&T_OUT)pkt.Show();
            }
            if (asp->stopped)asp->SendMarkerPkt("STOP");
         }
         if (asp->stopped || HEventsPending(0)){
            e = HGetEvent(0,0);
            switch(e.event){
            case HAUDOUT:
               asp->isPlaying = FALSE;
               asp->AckOutCmd("finished");
               asp->SendMarkerPkt("SYNTHSTOP");
               break;
            case HTBUFFER:
               if (e.c == MSGEVENTID) {
                  asp->ChkMessage();
                  while (asp->IsSuspended()) asp->ChkMessage(TRUE);
               }
               break;
            case HWINCLOSE:
               sprintf(buf, "terminating");
               HPostMessage(HThreadSelf(),buf);
               asp->terminated = TRUE;
               break;
            case HMOUSEDOWN:
               if (TrackButtons(&(asp->ssb),e)>0)
                  asp->ButtonPressed(asp->ssb.id);
               if (asp->hasXbt && TrackButtons(&(asp->xtb),e)>0)
                  asp->ButtonPressed(asp->xtb.id);
            }
         }
      }
      asp->SendMarkerPkt("TERMINATED");
      if (!asp->stopped) asp->StopCmd();
      if (asp->trace&T_TOP) printf("%s source exiting\n",asp->cname.c_str());
      HExitThread(0);
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}

// ----------------------End of ASource.cpp ---------------------
