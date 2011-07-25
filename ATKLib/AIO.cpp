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
/*      File: AIO.cpp - High Level IO Interface to ATK         */
/* ----------------------------------------------------------- */

char * AIO_version="!HVER!AIO: 1.6.0 [SJY 01/06/07]";

#include "AIO.h"

#define TIMEOUTPERIOD 3000
#define DISPLAYPERIOD 100
#define AIOPRBUFSIZE 12
#define synRepID 20
#define asrInID 21

#define T_TOP 0001    // top level tracing
#define T_AIN 0002    // display incoming packets from asrIn
#define T_SIN 0004    // display incoming packets from synRep
#define T_TIM 0010    // trace timer
#define T_STC 0020    // display state changes
#define T_HAN 0040    // trace unhandled cmds/events

// predeclare the internal threads
TASKTYPE TASKMOD TimerTask(void *p);
TASKTYPE TASKMOD DisplayTask(void *p);

// Constructor
AIO::AIO(const string & name, ABuffer *outChannel, ARMan *appRman, Boolean useLogging)
: AComponent(name,(HasRealConsole()?2:AIOPRBUFSIZE))
{
   outChan = outChannel;
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm,i;
   Boolean b;
   char ngramFN[100],buf[100];
   ngramFN[0] = '\0';
   // Read configuration parms for ANGRAM to see if NGram used
   numParm = GetConfig("ANGRAM", TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfStr(cParm,numParm,"NGRAMFILE",buf)) strcpy(ngramFN,buf);
   }
   // Read configuration parms for AIO
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   timeOutPeriod = TIMEOUTPERIOD;  // set default timeout
   width = 400; height=220; showDisp = FALSE;
   toutFlag = FALSE;   timeOut = 0; timing = FALSE; logData=useLogging;
   dx0 = 420;   dy0 = 80; trace = 0; xstep = 5;disableBargeIn=FALSE;
   if (numParm>0){
      if (GetConfBool(cParm,numParm,"DISPSHOW",&b)) showDisp = b;
      if (GetConfBool(cParm,numParm,"LOGDATA",&b)) logData = b;
      if (GetConfBool(cParm,numParm,"DISABLEBARGEIN",&b)) disableBargeIn = b;
      if (GetConfInt(cParm,numParm,"DISPXORIGIN",&i)) dx0 = i;
      if (GetConfInt(cParm,numParm,"DISPYORIGIN",&i)) dy0 = i;
      if (GetConfInt(cParm,numParm,"DISPWIDTH",&i)) width = i;
      if (GetConfInt(cParm,numParm,"DISPHEIGHT",&i)) height = i;
      if (GetConfInt(cParm,numParm,"DISPSTEP",&i)) xstep = i;
      if (GetConfInt(cParm,numParm,"TIMEOUTPERIOD",&i))timeOutPeriod = i;
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }

   // Create the buffers
   waveOut = new ABuffer("waveOut");  // syn -> aud for synth playing
   synAck  = new ABuffer("synAck");   // aud -> syn for acks
   waveIn  = new ABuffer("waveIn");   // aud -> code for user input
   featIn  = new ABuffer("featIn");   // code -> rec for features
   synRep  = new ABuffer("synRep");   // syn -> aio for synthesiser replies
   asrIn   = new ABuffer("asrIn");    // asr -> aio for recognition results

   // Install the supplied resource manager or create a new one.
   if (appRman != NULL)
      rman = appRman;
   else {
      rman = new ARMan();
      // create default resources
      hset = new AHmms("HmmSet");
      dict = new ADict("ADict");
      gram = new AGram("AGram");
      rman->StoreHMMs(hset); rman->StoreDict(dict); rman->StoreGram(gram);
      main = rman->NewGroup("main");
      main->AddHMMs(hset); main->AddDict(dict); main->AddGram(gram);
      if (strlen(ngramFN)>0){
         ngram  = new ANGram("ANGram");
         rman->StoreNGram(ngram);
         main->AddNGram(ngram);
      }
   }
   // Create the embedded ATK components
   if(logData) {
      waveLog  = new ABuffer("waveLog");  // tee->ALog
      waveCode = new ABuffer("waveLog");  // tee->ACode
      tee = new ATee("tee",waveIn, waveCode, waveLog);
      code= new ACode("code",waveCode,featIn);   // coder
      log= new ALog("log", waveLog, "logdir", "user", "host"); //Logger
   } else {
      code= new ACode("code",waveIn,featIn);                // coder
   }
   aud = new ASource("aud",waveIn);                      // audio
   synDevice = new FSynthesiser();
   syn = new ASyn("syn", synRep, waveOut, synAck, aud, synDevice);  // tts
   rec = new ARec("rec",featIn,asrIn,rman);         // recogniser
   // Initialise the state machines
   astate = asr_idle; sstate = syn_idle; smode=syn_telling;

   PauseTimeOut=FALSE;
}

// AttachMonitor: load internal components for the application
void AIO::AttachMonitor(AMonitor *amon)
{
   amon->AddComponent(aud);
   amon->AddComponent(syn);
   amon->AddComponent(code);
   amon->AddComponent(rec);
   amon->AddComponent(this);
   if(logData) {
      amon->AddComponent(log);
      amon->AddComponent(tee);
   }
}

// Draw/Update display - this is a thread that runs every 10msecs
TASKTYPE TASKMOD DisplayTask(void *p)
{
   AIO *aio = (AIO *)p;
   const int xmargin = 5, ymargin = 3, bmargin = 2;

   aio->x0 = xmargin;
   aio->x1 = aio->width - xmargin;
   aio->y2 = aio->height - ymargin;
   int rowHeight = (aio->height - 2*ymargin)/3;
   aio->barHeight = rowHeight - bmargin;
   aio->y0 = rowHeight + ymargin;
   aio->y1 = aio->y0 + rowHeight;
   while (!aio->IsTerminated()){
      // scroll current contents left
      HCopyArea(aio->win,aio->x0 + aio->xstep, 1,
                     aio->x1 - aio->x0 - aio->xstep, aio->height-2,
                     aio->x0,1);
      HSetGrey(aio->win,50);
      HFillRectangle(aio->win,aio->x1-aio->xstep,0,aio->x1+1,aio->height-1);
      // draw scrolling x axis
      HSetColour(aio->win,BLACK);
      HSetLineWidth(aio->win, 2);
      HDrawLine(aio->win,aio->x1 - aio->xstep,aio->y2,aio->x1,aio->y2);
      if (aio->ticker==0)
         HDrawLine(aio->win,aio->x1,aio->y2,aio->x1,aio->y2-aio->barHeight);
      else
         HDrawLine(aio->win,aio->x1,aio->y2,aio->x1,aio->y2-(aio->barHeight/2));
      if (++aio->ticker==10) aio->ticker=0;
      HSetLineWidth(aio->win, 1);
      // draw syn state
      HColour c;
      switch (aio->sstate) {
         case syn_idle:
            c = WHITE; break;
         case syn_talking:
            c = (aio->smode==syn_asking)? DARK_GREEN : LIGHT_GREEN;
            break;
         case syn_muted: c = YELLOW;
            break;
         case syn_aborted: c = RED;
            break;
      }
      if (c != WHITE){
         HSetColour(aio->win,c);
         HFillRectangle(aio->win,aio->x1 - aio->xstep,aio->y0 - aio->barHeight,
                        aio->x1,aio->y0);
      }
      HSetColour(aio->win,BLACK);
      HDrawLine(aio->win,aio->x1 - aio->xstep,aio->y0,aio->x1,aio->y0);
      // draw asr state
      if (aio->toutFlag){
         c = RED; aio->toutFlag = FALSE;
      }else{
         switch (aio->astate) {
         case asr_idle:
            c = WHITE; break;
         case asr_listening:
            c = YELLOW; break;
         case asr_filling:
            c = ORANGE; break;
         case asr_recognising:
            c = DARK_GREEN; break;
         }
      }
      if (c != WHITE){
         HSetColour(aio->win,c);
         HFillRectangle(aio->win,aio->x1 - aio->xstep,aio->y1 - aio->barHeight,
            aio->x1,aio->y1);
      }
      HSetColour(aio->win,BLACK);
      HDrawLine(aio->win,aio->x1 - aio->xstep,aio->y1,aio->x1,aio->y1);
      // wait for display period
      HPauseThread(DISPLAYPERIOD);
   }
   if (aio->trace&T_TOP) printf("AIO Display task exiting\n");
   HExitThread(0);
   return 0;
}

// CheckTQ: if talk q not empty, remove front and say it
void AIO::CheckTQ()
{
   if (talkq.size()>0 && sstate == syn_idle){
      TalkCmd tc = talkq.front();
      talkq.pop_front();
      if(smode==syn_yelling || disableBargeIn)
	aud->SendMessage("stop()"), printf("Stopped!!!\n");
      syn->SendMessage("talk("+tc.text+")");
      smode = tc.mode;
      sstate = syn_talking;
      ClearTimer();
   }
}

// SynthOut: output to synthesiser via cmd queue
void AIO::SynthOutput(SYNMode m)
{
   TalkCmd tc;

   if (!GetStrArg(text))
      HPostMessage(HThreadSelf(),"TalkCmd: synthesis string expected\n");
   tc.text = text;
   tc.mode = m;
   talkq.push_back(tc);
   CheckTQ();
}

// SendString: make string pkt and send to b
void AIO::SendString(const string& s, ABuffer *b)
{
   AStringData *sd = new AStringData(s);
   APacket p(sd);
   p.SetStartTime(GetTimeNow());
   p.SetEndTime(GetTimeNow());
   b->PutPacket(p);
}

// SendCommand: send command back to application
void AIO::SendCommand(const string& cmd, ABuffer *b, Boolean withIntInfo)
{
   ACommandData *cd = new ACommandData(cmd);
   if (withIntInfo){
      cd->AddArg(synIntIdx);
      cd->AddArg(synIntWord);
      cd->AddArg(synIntPercent);
   }
   APacket p(cd);
   p.SetStartTime(GetTimeNow());
   p.SetEndTime(GetTimeNow());
   b->PutPacket(p);
}

// ExecCommand: Implement the command interface
void AIO::ExecCommand(const string & cmdname)
{
   char buf[100];
   string strarg, str;

   if (cmdname == "tell") {
      SynthOutput(syn_telling);
   } else if (cmdname == "ask") {
      SynthOutput(syn_asking);
   } else if (cmdname == "yell") {
      SynthOutput(syn_yelling);
   } else if (cmdname == "self") {
      SynthOutput(syn_self);
   } else if (cmdname == "setgroup") {
      string rgroup;
      if (!GetStrArg(rgroup))
         HPostMessage(HThreadSelf(),"SetGroupCmd: group name expected\n");
      rec->SendMessage("usegrp("+rgroup+")");
   } else if (cmdname == "closedown") {
      aud->SendMessage("terminate()");
      code->SendMessage("terminate()");
      syn->SendMessage("terminate()");
      rec->SendMessage("terminate()");
      if(logData) {
         log->SendMessage("terminate()");
         tee->SendMessage("terminate()");
      }
   } else if(cmdname=="logstr" || cmdname=="setuserid" ||
             cmdname=="sethostid" || cmdname=="setlogdir" ) {
      if (!logData)
         HPostMessage(HThreadSelf(),"logcmd, logging not enabled\n");
      ForwardMessage(log);
   } else if(cmdname == "setnbest") {
      int nbest;
      if (!GetIntArg(nbest, 1, 100000))
         HPostMessage(HThreadSelf(),"Setnbest, n-best num expected\n");
      sprintf(buf,"setnbest(%d)",nbest);
      rec->SendMessage(buf);
   }else if(cmdname == "enablebargein") {
      disableBargeIn=FALSE;
   }else if(cmdname == "disablebargein") {
      disableBargeIn=TRUE;
   }else {
      sprintf(buf,"Unknown command %s\n",cmdname.c_str());
      HPostMessage(HThreadSelf(),buf);
   }
}

// Define add given word to list of fillers
void AIO::DefineFiller(const string& word)
{
   fillers[word] = 1;
}

// Check if given word is a filler
Boolean AIO::IsFiller(const string& word)
{
   FillerMap::iterator i=fillers.find(word);
   if (i==fillers.end()) return FALSE;
   return TRUE;
}


// Start:  start subsidiary threads, then aio thread
TASKTYPE TASKMOD AIO_Task(void *p);
void AIO::Start(HPriority priority)
{
   aud->Start(priority);
   code->Start(priority);
   syn->Start(priority);
   rec->Start(priority);
   if(logData) {
      log->Start(priority);
      tee->Start(priority);
   }
   AComponent::Start(priority,AIO_Task);
}

// UnhandledCmds
void AIO::UnhandledSynCmd(string cmd)
{
   char *synStateStr[] = {
      "idle","talking","muted","aborted"
   };
   printf("WARNING: cmd %s not handled in SYN state %s\n",
      cmd.c_str(),synStateStr[sstate]);
}

void AIO::UnhandledAsrCmd(ASREvent e)
{
   char *asrStateStr[] = {
      "idle","listening","filling","recognising"
   };
   char *asrEventStr[] = {
      "start","word","end","fill","timer","other"
   };
   printf("WARNING: event %s not handled in ASR state %s\n",
      asrEventStr[e],asrStateStr[astate]);
}

// StepSynState: use given event (ie cmd pkt) to update syn state machine
void AIO::StepSynState(APacket p)
{
   if (trace&T_SIN){
      printf("-------- Packet from Synthesiser -------------\n");
      p.Show();
      printf("----------------------------------------------\n");
   }
   assert(p.GetKind() == CommandPacket);
   ACommandData * cd = (ACommandData *)p.GetData();
   string cmd = cd->GetCommand();
   switch(sstate){
      case syn_idle:
         if (cmd == "finished"){
            SendCommand("synFinished",outChan,FALSE);
	    if(smode==syn_yelling || disableBargeIn)
	      aud->SendMessage("start()"),printf("Started!!!\n");
         } else
            if (trace&T_HAN) UnhandledSynCmd(cmd);
         break;
      case syn_talking:
         if (cmd == "muted") {
            // syn has been muted - so remember where
            synIntIdx = cd->GetInt(1);
            synIntWord = cd->GetString(2);
            synIntPercent = cd->GetFloat(3);
            sstate = syn_muted;
         }else if (cmd == "finished") {
            // if syn output was a question and asr not active,
            // set timer to ensure some kind of response even
            // if its just a timeout
            if (smode == syn_asking && timeOutPeriod>0 && astate != asr_recognising) {
               if (trace&T_TIM) printf(">>> Starting timer after normal ask\n");
               SetTimer();
            }
	    SendCommand("synFinished",outChan,FALSE);
	    if(smode==syn_yelling || disableBargeIn)
	      aud->SendMessage("start()"),printf("Started!!!\n");
            sstate = syn_idle; CheckTQ();
         }else if (cmd == "started") {
            // do nothing
         }else
            if (trace&T_HAN) UnhandledSynCmd(cmd);
         break;
      case syn_muted:
         if (cmd == "unmuted"){
            sstate = syn_talking;
         }else if (cmd == "finished"){
            // if syn output was a question and asr not active,
            // set timer to ensure some kind of response even
            // if its just a timeout
            if (smode == syn_asking && timeOutPeriod>0 && astate != asr_recognising) {
               if (trace&T_TIM) printf(">>> Starting timer after muted ask\n");
               SetTimer();
            }
            SendCommand("synFinished",outChan,FALSE);
	    if(smode==syn_yelling || disableBargeIn)
	      aud->SendMessage("start()"),printf("Started!!!\n");
            sstate = syn_idle; CheckTQ();
         }else if (cmd == "aborted"){
            SendCommand("synInterrupted",outChan,TRUE);
            sstate = syn_aborted; talkq.clear();
         }else
            if (trace&T_HAN) UnhandledSynCmd(cmd);
         break;
      case syn_aborted:
         if (cmd == "finished"){
            SendCommand("synFinished",outChan,FALSE);
	    if(smode==syn_yelling || disableBargeIn)
	      aud->SendMessage("start()"),printf("Started!!!\n");
            sstate = syn_idle; CheckTQ();
         }else
            if (trace&T_HAN) UnhandledSynCmd(cmd);
         break;
   }
}

// MapAsrEvent: decode current event
ASREvent AIO::MapAsrEvent(APacket p)
{
   PacketKind kind = p.GetKind();
   ACommandData *cd;
   APhraseData *pd;
   AStringData *ad;
   string cmd;
   switch(kind){
      case CommandPacket:
         cd = (ACommandData *)p.GetData();
         cmd = cd->GetCommand();
         if (cmd != "timeout") {
            HRError(13003,"MapAsrEvent: %s cmd unexpected",cmd.c_str());
            throw ATK_Error(13003);
         }
         return AsrTimerEv;
         break;
      case PhrasePacket:
         recPkts.push_back(p);
         pd = (APhraseData *)p.GetData();
         if (pd->ptype == Start_PT ) return AsrStartEv;
         if (pd->ptype == End_PT ) return AsrEndEv;
         if (pd->ptype == Word_PT ){
            if (IsFiller(pd->word))
               return AsrFillEv;
            else
               return AsrWordEv;
         }else{
            return AsrOtherEv;
         }
         break;
      case StringPacket:
		 ad=(AStringData *)p.GetData();
		 cmd=ad->GetSource();
		 if(cmd=="aud"){
			 cmd=ad->GetMarker();
			 if(cmd=="PAUSE") PauseTimeOut=TRUE;
			 if(cmd=="START" && PauseTimeOut) PauseTimeOut=FALSE;
		 }
         return AsrMarkerEv;
         break;
      default:
         return AsrOtherEv;
   }
}

// ForwardRecPkts: pass on any buffered rec packets to application
void AIO::ForwardRecPkts(){
	while (recPkts.size()>0){
		APacket p = recPkts.front();
		recPkts.pop_front();
		outChan->PutPacket(p);
		if(logData)
		{
			APhraseData *pd;
			pd = (APhraseData *)p.GetData();
			if(pd->ptype==Word_PT)
				log->SendMessage("appendstr("+pd->word+")");
		}
	}
}

// StepAsrState: use given event (ie cmd pkt) to update asr state machine
void AIO::StepAsrState(APacket p)
{
   if (trace&T_AIN){
      printf("============== Packet from ASR ===============\n");
      p.Show();
      printf("==============================================\n");
   }
   ASREvent e = MapAsrEvent(p);
   // filter out any string marker packets and pass them on
   if (e==AsrMarkerEv) {
      outChan->PutPacket(p);
		if(logData) {
			AStringData *sd = (AStringData *)p.GetData();
         log->SendMessage("appendstr(<<"+sd->data+">>)");
		}
      return;
   }
   APacket pd;
   switch (astate){
   case asr_idle:        // waiting for a START packet from rec
      if (e==AsrStartEv){
         if (sstate == syn_talking){
            syn->SendMessage("mute()");
         }
         astate = asr_listening;
      }else if (e==AsrTimerEv){
         SendCommand("asrTimeOut",outChan,FALSE);
         recPkts.clear();
         astate = asr_idle;
      }else
         if (trace&T_HAN) UnhandledAsrCmd(e);
      break;
   case asr_listening:   // start received
      if (e==AsrFillEv){
         astate = asr_filling;
      }else if (e==AsrWordEv){
         if(logData){
            if(recPkts.size()>0){
               pd=recPkts.front();
               log->starttime=pd.GetStartTime();
            }else log->starttime=0;
            log->SendMessage("startrec()");
         }
         ForwardRecPkts();
         astate = asr_recognising;
         if (sstate == syn_muted || sstate == syn_talking){
            syn->SendMessage("abort()");
         }
         ClearTimer();
      }else if (e==AsrTimerEv){
         SendCommand("asrTimeOut",outChan,FALSE);
         recPkts.clear();
         astate = asr_idle;
      }else
         if (trace&T_HAN) UnhandledAsrCmd(e);
      break;
   case asr_filling:     // so far only fillers decoded
      if (e==AsrWordEv){
         if(logData){
            if(recPkts.size()>0){
               pd=recPkts.front();
               log->starttime=pd.GetStartTime();
            }else log->starttime=0;
            log->SendMessage("startrec()");
         }
         ForwardRecPkts();
         astate = asr_recognising;
         if (sstate == syn_muted || sstate == syn_talking){
            syn->SendMessage("abort()");
         }
         ClearTimer();
      } else if (e==AsrFillEv){
         recPkts.push_back(p);
      }else if (e==AsrEndEv){
         if (sstate == syn_muted){
            syn->SendMessage("unmute()");
         }
         recPkts.clear();
         astate = asr_idle;
      }else if (e==AsrTimerEv){
         SendCommand("asrTimeOut",outChan,FALSE);
         recPkts.clear();
         astate = asr_idle;
      }else
         if (trace&T_HAN) UnhandledAsrCmd(e);
      break;
   case asr_recognising:  // user is really saying something
      if (e==AsrWordEv){
         ForwardRecPkts();
      }else if (e==AsrEndEv){
         ForwardRecPkts();
         astate = asr_idle;
		 if(logData){
			 log->endtime=p.GetEndTime();
			 log->SendMessage("stoprec()");
		 }
      }else
         if (trace&T_HAN) UnhandledAsrCmd(e);
      break;
   }
}

// ----------------------------- Timer Thread -----------------------

void AIO::SetTimer()
{
   HEnterSection(tlock);
   timeOut = timeOutPeriod; timing = TRUE;
   HLeaveSection(tlock);
   HSendSignal(tsig);
}

void AIO::ClearTimer()
{
   HEnterSection(tlock);
   timeOut = 0; timing = FALSE;
   HLeaveSection(tlock);
}

// TimerTask:  timer thread
TASKTYPE TASKMOD TimerTask(void *p)
{
   AIO *aio = (AIO *)p;
   int t;

   while (!aio->IsTerminated()){
      // idle until timeout set by signalling tsig
      if (aio->trace&T_TIM) printf(">>> Timer Primed\n");

      HEnterSection(aio->tlock);
      while (!aio->timing) HWaitSignal(aio->tsig,aio->tlock);
      HLeaveSection(aio->tlock);

      while (aio->timing) {
         HPauseThread(10);

         HEnterSection(aio->tlock);
		 aio->timeOut -= 10*(!aio->PauseTimeOut);
         if (aio->timeOut<=0) {
            aio->timeOut = 0;
            if (aio->timing) { // check cos it might have been cancelled
               if (aio->trace&T_TIM) printf(">>> TimeOut!!!\n");
               aio->SendCommand("timeout",aio->asrIn,FALSE);
               aio->toutFlag = TRUE;  aio->timing = FALSE;
            }
         }
         HLeaveSection(aio->tlock);
      }
   }
   HExitThread(0);
   return 0;
}

// ----------------------- Main AIO Thread ------------------------------

static char *astates[] = {
   "asr_idle","asr_listening","asr_filling","asr_recognising"
};
static char *sstates[] = {
   "syn_idle","syn_talking","syn_muted","syn_aborted"
};
static char *smodes[] = {
  "syn_telling","syn_asking", "syn_yelling", "syn_self"
};

// AIO_Task:  the main interface control thread
TASKTYPE TASKMOD AIO_Task(void *p)
{
   HEventRec e;
   char buf[100],cname[100];
   ASRState as;   // only used for tracing
   SYNState ss;   //
   SYNMode  sm;   //

   AIO *aio = (AIO *)p;
   try{
   // Create the Timer
   aio->tsig = HCreateSignal("tsig");
   aio->tlock = HCreateLock("tlock");
   aio->timer = HCreateThread("timer",1,HPRIO_NORM,TimerTask,p);
   if (aio->showDisp){
      strcpy(cname,aio->cname.c_str());
      aio->win = MakeHWin(cname,aio->dx0,aio->dy0,aio->width,aio->height,1);
      HSetGrey(aio->win,50);
      HFillRectangle(aio->win,0,0,aio->width,aio->height);
      // Create the display thread
      aio->display = HCreateThread("display",1,HPRIO_NORM,DisplayTask,p);
      aio->ticker = 0;
   }
   aio->synRep->RequestBufferEvents(synRepID);
   aio->asrIn->RequestBufferEvents(asrInID);
   aio->RequestMessageEvents();
   aio->rec->SendMessage("usegrp(main)");
   aio->rec->SendMessage("start()");
   aio->aud->SendMessage("start()");
   if(aio->disableBargeIn)
     aio->aud->SendMessage("stop()");
   while (!aio->IsTerminated()) {
      e = HGetEvent(0,0);
      if (e.event == HTBUFFER) {
         if (aio->trace&T_STC){
            as = aio->astate; ss = aio->sstate; sm = aio->smode;
         }
         switch(e.c){
            case synRepID:
               if (!aio->synRep->IsEmpty()){
                  aio->StepSynState(aio->synRep->GetPacket());
               }
               break;
            case asrInID:
               if (!aio->asrIn->IsEmpty()){
                  aio->StepAsrState(aio->asrIn->GetPacket());
               }
               break;
            case MSGEVENTID:
               aio->ChkMessage();
               while (aio->IsSuspended()) aio->ChkMessage(TRUE);
               break;
         }
         if ( aio->trace&T_STC &&
              (as!=aio->astate || ss!=aio->sstate || sm!=aio->smode)){
            printf(" aio_state: %s : %s[%s]\n",
               astates[aio->astate], sstates[aio->sstate], smodes[aio->smode]);
         }
      }else if (e.event == HWINCLOSE){
         sprintf(buf, "terminating");
         HPostMessage(HThreadSelf(),buf);
         aio->terminated = TRUE;
      }
   }
   if (aio->trace&T_TOP) printf("AIO main task exiting\n");
   aio->SendCommand("terminated",aio->outChan,FALSE);
   HExitThread(0);
   return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}

// ------------------------ End AIO.cpp ---------------------


