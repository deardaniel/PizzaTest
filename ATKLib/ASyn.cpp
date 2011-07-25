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
/*        File: Asyn.cpp -    Implementation of Synthesiser    */
/* ----------------------------------------------------------- */

char * asyn_version="!HVER!ASyn: 1.6.0 [SJY 01/06/07]";

// Modification history:

#include "ASyn.h"

#define ACKBUFID 1
#define ASynPRBUFSIZE 5

#define T_TOP 0001    // top level tracing
#define T_ABT 0002    // abort tracing

// This component is driven by command messages, status is returned
// via the reply buffer (repbuf).

//  Commands               Normal Reply                  Error Reply
//  --------------         ---------------               -----------
//  talk("text")           started (ss,n)                error(ss)
//  abort()                aborted (ss,idx,word,f)       idle()
//  mute()                 muted   (ss,idx,word,f)       idle()
//  unmute()               unmuted (ss,idx,word,f)       idle()
//  (on completion)        finished(ss)
//
// where ss = seqnumber, n=num samples in utterance, f = % spoken
//       idx = index of last whole word output
//       word = last whole word output
//

// ------------------- ASyn Class --------------------------------

// ASyn constructor
ASyn::ASyn(const string & name, ABuffer *repb, ABuffer *audb,
           ABuffer *ackb, ASource *asink, ASynthesiser *theSyn)
: AComponent(name,(HasRealConsole()?2:ASynPRBUFSIZE))
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i;
   Boolean b;
   char buf[100];

   // create the module
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   audbuf = audb; ackbuf = ackb; repbuf = repb;
   sink = asink; state = synth_idle; syn=theSyn;
   trace = 0; seqnum = 0;
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   // create the output channel to the sink
   asink->OpenOutput(audb,ackb,625.0);
}


// Start the task
TASKTYPE TASKMOD ASyn_Task(void * p);
void ASyn::Start(HPriority priority)
{
   AComponent::Start(priority,ASyn_Task);
}

// ChkAckBuf: check acks coming from sink and update host on status
void ASyn::ChkAckBuf()
{
   if (!ackbuf->IsEmpty()){
      // first unpack the incoming packet which has the form
      //   cmd(ss,playedSamps,totalSamps)
      APacket p = ackbuf->GetPacket();
      if (p.GetKind() != CommandPacket) {
         HRError(12002,"ASyn:ChkAckBuf - non-command pkt in ackbuf\n");
         throw ATK_Error(12002);
      }
      ACommandData * cd = (ACommandData *) p.GetData();
      string ack = cd->GetCommand();
      int ss = cd->GetInt(0);
      int played = cd->GetInt(1);
      int total = cd->GetInt(2);
      // next construct reply command
      cd = new ACommandData(ack);
      // if intermediate state, then figure out what has been played
      string word;
      if (ack=="aborted" || ack=="muted" || ack=="unmuted"){
         int idx;
         float percent;
         if (ss==seqnum) {  // still current output utterance
               //printf("********* p=%d,t=%d,nw=%d,b1=%d,b2=%d\n",
               //played,total,syn->GetNumWords(),
               //syn->GetBoundary(1),syn->GetBoundary(2));
            idx = syn->GetNumWords();
            while (idx>0 && syn->GetBoundary(idx)>played) --idx;
            word = "";
            if (idx>0) word = syn->GetWord(idx);
            percent = 100.0*played/total;
         }else{
            idx=0; percent=100.0; word=".";
         }
         cd->AddArg(ss); cd->AddArg(idx);
         cd->AddArg(word); cd->AddArg(percent);
         if (ack=="aborted") syn->EndUtterance();
         if (trace&T_ABT)
            printf("ASyn: syn interrupted: ss=%d,played=%d,total=%d,idx=%d[%s]\n",
                    ss,played,total,idx,word.c_str());
      }else if (ack == "started") {
         cd->AddArg(ss); cd->AddArg(total);
      }else if (ack == "error" || ack == "finished") {
         cd->AddArg(ss); syn->EndUtterance();
      }
      APacket reppkt(cd);
      reppkt.SetStartTime(GetTimeNow());
      reppkt.SetEndTime(GetTimeNow());
      repbuf->PutPacket(reppkt);
      if (trace&T_TOP){
         printf("ASyn: reply sent - "); cd->Show(); printf("\n");
      }
   }
}

// TalkCmd: synthesise the string and start playing it
void ASyn::TalkCmd()
{
   AWaveData *wd;
   char cbuf[100];
   short *wave,*p;
   int size,n;
   string text;

   if (!GetStrArg(text))
      HPostMessage(HThreadSelf(),"TalkCmd: synthesis string expected\n");
   if (trace&T_TOP)
      printf("ASyn: talk request = %s\n",text.c_str());   // convert text to waveform
   syn->StartUtterance(text);
   n = syn->GetNumSamples();
   if (n==0){
      string err="TalkCmd: cannot synthesise "+text+"\n";
      HPostMessage(HThreadSelf(),err.c_str());
   }
   // packet up the wave and send it to ASource
   // - first create header
   ++seqnum;
   sprintf(cbuf,"%d:%d",seqnum,n);
   AStringData *sd = new AStringData(string(cbuf));
   APacket hdrpkt(sd);
   hdrpkt.SetStartTime(GetTimeNow());
   hdrpkt.SetEndTime(GetTimeNow());
   audbuf->PutPacket(hdrpkt);
   // - then chop wave into packets
   p = syn->GetWave();
   while (n>0){
      size = n;
      if (size>WAVEPACKETSIZE) size = WAVEPACKETSIZE;
      wd = new AWaveData(size,p);
      n -= size; p += size;
      APacket wavpkt(wd);
      audbuf->PutPacket(wavpkt);
   }
   // - finally send command to start playback
   sprintf(cbuf,"startout(%d)",seqnum);
   sink->SendMessage(string(cbuf));
   state = synth_talking;
}


// MuteCmd: quiet the output dont kill it
void ASyn::MuteCmd()
{
   sink->SendMessage("muteout()");
   state = synth_muted;
}

// ResumeCmd: resume the output to full volume
void ASyn::UnmuteCmd()
{
   sink->SendMessage("unmuteout()");
   state = synth_unmuted;
}

// AbortCmd: terminate output
void ASyn::AbortCmd()
{
   sink->SendMessage("abortout()");
   state = synth_aborted;
}

// Implement the command interface
void ASyn::ExecCommand(const string & cmdname)
{
   char buf[100];

   if (cmdname == "talk") {
      TalkCmd();
   } else if (cmdname == "mute") {
      MuteCmd();
   } else if(cmdname == "unmute") {
      UnmuteCmd();
   } else if(cmdname == "abort") {
      AbortCmd();
   } else {
      sprintf(buf,"Unknown command %s\n",cmdname.c_str());
      HPostMessage(HThreadSelf(),buf);
   }
}

// ASyn task
TASKTYPE TASKMOD ASyn_Task(void * p)
{
   ASyn *asp = (ASyn *)p;
   APacket pkt,mkr;
   char buf[100],cname[100];
   HEventRec e;

   try{
      strcpy(cname,asp->cname.c_str());
      asp->ackbuf->RequestBufferEvents(ACKBUFID);
      asp->RequestMessageEvents();
      while (!asp->IsTerminated()){
         e = HGetEvent(0,0);
         switch(e.event){
            case HTBUFFER:
               switch(e.c){
                  case MSGEVENTID:
                     asp->ChkMessage();
                     while (asp->IsSuspended()) asp->ChkMessage(TRUE);
                     break;
                  case ACKBUFID:
                     asp->ChkAckBuf();
		               break;
               } /* switch(e.c) */
               break;
            case HWINCLOSE:
               sprintf(buf, "terminating");
               HPostMessage(HThreadSelf(),buf);
               asp->terminated = TRUE;
               break;
         }
      }
      if (asp->trace&T_TOP) printf("%s synthesiser exiting\n",asp->cname.c_str());
      HExitThread(0);
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}

// ----------------------End of ASyn.cpp ---------------------
