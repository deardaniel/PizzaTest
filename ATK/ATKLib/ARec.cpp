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
/*      File: ARec.cpp -    Implementation of recogniser       */
/* ----------------------------------------------------------- */

char * arec_version="!HVER!ARec: 1.6.0 [SJY 01/06/07]";

// Modification history:
//  09/12/02 - added PRBUFSIZE for printf display monitor
//  13/12/02 - modified FlushObservation and RecObservation
//             to allow string marker packets to be propagated
//  05/08/03 - standardised display config var names
//  03/01/04 - ngram support added
//  07/07/04 - input buffer indicator added to display
//  11/08/04 - static variables removed
//  17/05/05 - added nbest output options, added a ansheap
//             for lattices, added a destructor - MNS
//  11/08/05 - support for class-based LMs added

#include "ARec.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define T_TOP 0001     /* Top level tracing */
#define T_RST 0002     /* Trace run state */
#define T_FAN 0004     /* Final complete answer */
#define T_PAN 0010     /* Partial answer */
#define T_IAN 0020     /* Immediate answer */
#define T_OUT 0040     /* Output packets */
#define T_OBS 0100     /* Observations */
#define T_FRS 0200     /* frame by frame best token */

#define INBUFID 1
#define ARECPRBUFSIZE 12

#define PANELGREY0 40
#define PANELGREY1 50
#define PANELGREY2 60

// ------------------- ARec Class --------------------------------

// ARec constructor
ARec::ARec(const string & name, ABuffer *inb, ABuffer *outb,
           ARMan *armgr, int nnToks) : AComponent(name,(HasRealConsole()?2:ARECPRBUFSIZE))
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i; double f;
   Boolean b;
   char buf[100];

   in = inb; out = outb; rmgr = armgr;
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   trace=0;
   width = 400; height=120; showRD = FALSE;
   sampPeriod = 100000.0; stTime = 0.0;
   rdx0 = 420;  rdy0 =280;
   runmode = RunMode(CONTINUOUS_MODE+FLUSH_TOMARK+STOP_ATMARK+RESULT_ATEND);
   runstate = WAIT_STATE; laststate = RunState(-1);
   nToks = nnToks;
   wordPen = 0.0; lmScale = 1.0; ngScale=0.0; prScale = 1.0;
   genBeam = nBeam = 225.0; wordBeam = 200.0; maxActive = 0;
   grpName = "";  trbakFreq = 5; outseqnum = 0; inlevel = -1;
   trbakFrame = 0; trbakAc = 0.0; trbakCount = 0; trbakLastWidth=0;
   nBest=0;
   if (numParm>0){
      if (GetConfBool(cParm,numParm,"DISPSHOW",&b)) showRD = b;
      if (GetConfInt(cParm,numParm,"DISPXORIGIN",&i)) rdx0 = i;
      if (GetConfInt(cParm,numParm,"DISPYORIGIN",&i)) rdy0 = i;
      if (GetConfInt(cParm,numParm,"DISPWIDTH",&i)) width = i;
      if (GetConfInt(cParm,numParm,"DISPHEIGHT",&i)) height = i;
      if (GetConfInt(cParm,numParm,"RUNMODE",&i)) runmode = RunMode(i);
      if (GetConfInt(cParm,numParm,"NTOKS",&i)) nToks = i;
      if (GetConfFlt(cParm,numParm,"WORDPEN",&f)) wordPen = float(f);
      if (GetConfFlt(cParm,numParm,"LMSCALE",&f)) lmScale = float(f);
      if (GetConfFlt(cParm,numParm,"NGSCALE",&f)) ngScale = float(f);
      if (GetConfFlt(cParm,numParm,"PRSCALE",&f)) prScale = float(f);
      if (GetConfFlt(cParm,numParm,"GENBEAM",&f)) genBeam = float(f);
      if (GetConfFlt(cParm,numParm,"WORDBEAM",&f)) wordBeam = float(f);
      if (GetConfFlt(cParm,numParm,"NBEAM",&f)) nBeam = float(f);
      if (GetConfInt(cParm,numParm,"MAXBEAM",&i)) maxActive = i;
      if (GetConfStr(cParm,numParm,"GRPNAME",buf)) grpName=string(buf);
      if (GetConfInt(cParm,numParm,"TRBAKFREQ",&i)) trbakFreq = i;
      if (GetConfFlt(cParm,numParm,"TARGETRATE",&f)) sampPeriod = float(f);
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
      if (GetConfInt(cParm,numParm,"NBEST",&i)) nBest = i;
   }
   if (nBeam < genBeam) nBeam = genBeam;
   if ((runmode&RESULT_ASAP) || (runmode&RESULT_IMMED))
      runmode = RunMode(runmode|RESULT_ATEND);
   if(nBest>0){
      CreateHeap(&ansHeap,"Lattice heap",MSTAK,1,0.0,4000,4000);
      CreateHeap(&altHeap,"Lattice heap",MSTAK,1,0.0,4000,4000);
   }
}

// Destructor
ARec::~ARec()
{
  /*  if(nBest>0) {
    assert(&ansHeap!=NULL);
    DeleteHeap(&ansHeap);
    }*/
}

// Start the task
TASKTYPE TASKMOD ARec_Task(void * p);
void ARec::Start(HPriority priority)
{
   AComponent::Start(priority,ARec_Task);
}

// Draw current status
void ARec::DrawStatus()
{
   if (laststate != runstate){
      switch(runstate){
      case WAIT_STATE: HSetColour(win,RED); break;
      case PRIME_STATE: HSetColour(win,ORANGE); break;
      case FLUSH_STATE: HSetColour(win,YELLOW); break;
      case RUN_STATE: HSetColour(win,DARK_GREEN); break;
      case ANS_STATE: HSetColour(win,MAUVE); break;
      }
      HFillRectangle(win,x2,y4+4,x3,y5+1);
      laststate = runstate;
   }
   int npkts = in->NumPackets();
   if (npkts != inlevel){
      inlevel = npkts;
      int x = npkts+x0;
      if (x>x1) x = x1;
      HSetGrey(win,PANELGREY1);
      HFillRectangle(win,x0,y8,x1,y9);
      HSetColour(win,RED);
      HFillRectangle(win,x0+1,y8,x,y9);
   }
}

void ARec::ScrollOutBox()
{
   int w,h;

   if ( (rdly+rdlyinc) > y7) {   // scroll
      h = y7-(y0+1)-rdlyinc+3;
      w = x1-x0-2;
      HCopyArea(win,x0+1,rdly+3-h,w,h,x0+1,y0+1);
      HSetGrey(win,PANELGREY1);
      HFillRectangle(win,x0+1,y1-rdlyinc-1,x1-1,y1-1);
      rdly = y7;
   } else
      rdly += rdlyinc;
}

// Draw output line in main output box - char by char
void ARec::DrawOutLine(PhraseType k, HTime t, string wrd, string tag)
{
   char buf[1000];

   HSetFontSize(win,-14);
   switch(k){
   case Start_PT:
      ScrollOutBox();
      sprintf(buf,"%4.1f: ",t/1e7);
      rdline = string(buf);
      break;
   case OpenTag_PT:
      rdline = rdline + " (";
      break;
   case CloseTag_PT:  rdline = rdline + " )";
      if (tag != "") rdline = rdline + tag;
      break;
   case Null_PT:
      if (tag != "") rdline = rdline + " <"+tag+">";
      break;
   case Word_PT:
      rdline = rdline + " " + wrd;
      if (tag != "") rdline = rdline + "<"+tag+">";
      break;
   case End_PT:
      HSetColour(win,BLACK);
      HPrintf(win,x0+3,rdly,"%s",rdline.c_str());
      break;
   }
   int len,w = x1-x0-6;
   if (HTextWidth(win,rdline.c_str()) > w) {
      strcpy(buf,rdline.c_str());
      do {
         len = strlen(buf); --len;
         assert(len>5);
         buf[len] = '\0';
      } while (HTextWidth(win,buf) > w);
      HSetColour(win,BLACK);
      HPrintf(win,x0+3,rdly,"%s",buf);
      ScrollOutBox();
      rdline.erase(0,len);
      rdline = "   " + rdline;
   }
   HSetFontSize(win,0);
}

// Initialise sizes for display window
void ARec::InitDrawRD()
{
   const int margin = 4;
   int textheight;
   int nlines;

   textheight = HTextHeight(win,"ABCD");

   x0 = margin; y0 = margin; x1 = width-margin;
   x2 = x0 + HTextWidth(win,"Status") + 2;
   x3 = x2+20;
   x4 = x3+ 2*margin;
   x5 = x4 + HTextWidth(win,"Time") + 2;
   x6 = x5 + HTextWidth(win,"9999.9s") + 2;
   x7 = x6 + HTextWidth(win,"Score") + 2;
   x8 = x7 + 40;
   x9 = x8 + 4*margin;
   x10 = x9 + HTextWidth(win,"HMM") + 4;
   x11 = x10 + HTextWidth(win,"aa-yy+bb") + 4;
   x12 = x11 + HTextWidth(win,"Nact") + 6;
   x13 = x12 + HTextWidth(win,"9999") + 6;
   x14 = x13 + HTextWidth(win,"Mode") + 4;
   y9 = height - margin;
   y8 = y9 - margin;
   y5 = y8 - margin;
   y4 = y5 - textheight - 1;
   y3 = y4 - margin;
   y6 = (int) (y3 - textheight*1.5);
   y1 = y6 - margin;
   y7 = y1 - margin-2;
   HSetFontSize(win,-14);
   textheight = HTextHeight(win,"ABCD");
   rdlyinc = textheight+2;
   HSetFontSize(win,0);
   nlines = (y7-y0) / rdlyinc; --nlines;
   rdly = y7 - nlines*rdlyinc;
   // paint background
   HSetGrey(win,PANELGREY2);
   HFillRectangle(win,0,0,width,height);
   // paint output panel
   HDrawPanel(win,x0,y0,x1,y1,PANELGREY1);
   // paint traceback panel
   HDrawPanel(win,x0,y6,x1,y3,PANELGREY1);
   // write labels
   HSetGrey(win,PANELGREY0);
   HPrintf(win,x0,y5,"Status");
   HPrintf(win,x4,y5,"Time");
   HPrintf(win,x6,y5,"Score");
   HPrintf(win,x9,y5,"HMM");
   HPrintf(win,x11,y5,"NAct");
   HPrintf(win,x13,y5,"Mode");
}

// Interface commands
void ARec::SetModeCmd()
{
   int i;

   if (GetIntArg(i,1,MAXMODEVAL)) {
      runmode = RunMode(i);
      if ((runmode&RESULT_ASAP) || (runmode&RESULT_IMMED))
         runmode = RunMode(runmode|RESULT_ATEND);
   }
   else
      HPostMessage(HThreadSelf(),"SetMode: value out of range\n");
}

void ARec::StartCmd()
{
   if (runstate == WAIT_STATE){
      runstate = PRIME_STATE;
      HBufferEvent(HThreadSelf(),INBUFID);
   } else
      HPostMessage(HThreadSelf(),"Start: rec not idle!\n");
}

void ARec::StopCmd()
{
   if (runstate == FLUSH_STATE)
      runstate = (runmode&CONTINUOUS_MODE)?PRIME_STATE:WAIT_STATE;
   else if (runstate == RUN_STATE)
      runstate = ANS_STATE;
   else
      HPostMessage(HThreadSelf(),"Stop: rec not running\n");
}

void ARec::UseGrpCmd()
{
   if (!GetStrArg(grpName))
      HPostMessage(HThreadSelf(),"UseGrp: resource group name expected\n");
   // if asr is running, abort to ensure it is reprimed
   printf("Usegroup: %s %d",grpName.c_str(),runstate);
   if (runstate == RUN_STATE) { // NB - this might be a source of errors
      runstate = ANS_STATE;     // SJY 21/4/07
      printf("-> %d",runstate);
   }
   printf("\n");
   if (trace&T_TOP)
      printf("Rec switching to resource group %s\n",grpName.c_str());
}

// Send a marker packet to output
void ARec::SendMarkerPkt(string marker)
{
  AStringData *sd = new AStringData(cname+"::"+marker);
  APacket mkrpkt(sd);
  mkrpkt.SetStartTime(GetTimeNow());
  mkrpkt.SetEndTime(GetTimeNow());
  out->PutPacket(mkrpkt);
  if (trace&T_OUT)mkrpkt.Show();
}



// Implement the command interface
void ARec::ExecCommand(const string & cmdname)
{
   char buf[100];

   if (cmdname == "start")
	   StartCmd();
   else if (cmdname == "stop")
	   StopCmd();
   else if (cmdname == "setmode")
	   SetModeCmd();
   else if (cmdname == "usegrp")
	   UseGrpCmd();
   else if (cmdname == "setnbest") {
	   if(nBest==0) {
		   CreateHeap(&ansHeap,"Lattice heap",MSTAK,1,0.0,4000,4000);
		   CreateHeap(&altHeap,"Lattice heap",MSTAK,1,0.0,4000,4000);
	   }
	   int nb;
	   if (!GetIntArg(nb, 1, 100000))
		   HPostMessage(HThreadSelf(),"Setnbest, n-best num expected\n");
	   nBest=nb;
   }
   else {
	   sprintf(buf,"Unknown command %s\n", cmdname.c_str());
	   HPostMessage(HThreadSelf(),buf);
   }
}

// Initialise the recogniser
void ARec::InitRecogniser()
{
   ResourceGroup *main = rmgr->MainGroup();
   hset = main->MakeHMMSet();
   psi=InitPSetInfo(hset);
   pri=InitPRecInfo(psi,nToks);
}

// Prime the recogniser ready to process an utterance
void ARec::PrimeRecogniser()
{
   ResourceGroup *g;

   g = (grpName=="")?rmgr->MainGroup():rmgr->FindGroup(grpName);
   if (g == NULL){
      if (grpName=="") HRError(0,"ARec: cant find main resource group\n");
      else HRError(0,"ARec: cant find resource group %s\n",grpName.c_str());
      throw ATK_Error(11001);
   }
   Network *net = g->MakeNetwork();
   LModel *lm = g->MakeNGram();
   opMap.clear();   // forget all previously output packets
   StartRecognition(pri,net,lmScale,wordPen,prScale,ngScale,lm);
   SetPruningLevels(pri,maxActive,genBeam,wordBeam,nBeam,10.0);
   frameCount = 0; tact = 0;
   if (showRD){
      string gn = (grpName=="")?"main":grpName;
      string s = "Primed with "+  gn + "\n";
      HPostMessage(HThreadSelf(),s.c_str());
   }
}

// Flush  a single observation
Boolean ARec::FlushObservation()
{
   APacket pkt;
   AStringData *sd;
   AObsData *od;
   PacketKind kind;
   Boolean flushing = TRUE;

   stTime = -1.0;  // ensure -ve before switching to runstate
   if (runmode&RUN_IMMED) // no flushing
      return TRUE;

   while (in->NumPackets()>0){
      pkt = in->PeekPacket(); kind = pkt.GetKind();

      if (kind==StringPacket) {
         in->PopPacket(); StoreMarker(pkt);
         sd = (AStringData *)pkt.GetData();
         if (sd->data.find("TERMINATED")!=string::npos) terminated=TRUE;
         if (runmode&FLUSH_TOMARK) {
            // if start or endoflist marker found, flushing complete
            string::size_type posn = sd->data.find("ENDOFLIST");
            if (posn != string::npos ) {
               OutPacket(Start_PT, "","ENDOFLIST",0,0,0.0,0.0,0.0,-1,0.0,0.0,0.0);
               terminated = TRUE;
               return TRUE;
            }
            posn = sd->data.find("START");
            if (posn != string::npos) {
               string::size_type pos1 = sd->data.find("(");
               string::size_type pos2 = sd->data.find(")");
               if (pos1<1 || pos2<pos1) return TRUE;
               fname = string(sd->data,pos1+1,pos2-pos1-1);
               return TRUE;
            }
         }
         OutMarkers(GetTimeNow());
      } else
         if (kind==ObservationPacket){
            // if speech flagged observation, flushing complete
            if (runmode&FLUSH_TOSPEECH) {
               od = (AObsData *)pkt.GetData();
               if (od->data.vq[0]) return TRUE;
            }
            in->PopPacket();
         } else {
            in->PopPacket();	// ignore non-string or -obs packets
         }
   }
   return FALSE;
}

// Recognise a single observation
Boolean ARec::RecObservation()
{
   APacket pkt;
   AStringData *sd;
   AObsData *od;
   PacketKind kind;
   Boolean stopDetected = FALSE;

   if (runmode&STOP_IMMED) // stop immediately
      return TRUE;

   while (in->NumPackets()>0){
      pkt = in->GetPacket(); kind = pkt.GetKind();
      if (kind==StringPacket) {
         // check if stop marker
         sd = (AStringData *)pkt.GetData();
         if ((runmode&STOP_ATMARK) && (sd->data.find("STOP") != string::npos))
            stopDetected = TRUE;
         StoreMarker(pkt);
      }
      if (kind != StringPacket || stopDetected){
         if (stTime<0) {  // just entered runmode
            stTime = pkt.GetStartTime(); enTime = stTime;
            score = -1e6;
            trbak = "";  trbakCount = 0;
            if (runmode&RESULT_ALL)
               OutPacket(Start_PT, "", fname, 0,0,0.0,0.0,0.0,-1.0,0.0,stTime,stTime);
         }
         if (stopDetected) return TRUE;
         if (kind==ObservationPacket){
            od = (AObsData *)pkt.GetData();
            if ((runmode&STOP_ATSIL) && !od->data.vq[0])  // stop if silence
               return TRUE;
            if (trace&T_OBS) PrintObservation(frameCount,&(od->data),13);
            // recognise observation
            ProcessObservation(pri,&(od->data),-1,hset->curXForm);

            if (trace & T_FRS) {
               char *p;  MLink m;
               NetNode *node = pri->genMaxNode;
               printf("Frame %-4d ",pri->frame);
               if ( node == NULL){
                  printf("null\n");
               }else{
                  char *qm="?";
                  p = (node->wordset==NULL)?qm:node->wordset->name;
                  m=FindMacroStruct(hset,'h',node->info.hmm);
                  printf("HMM: %s (%s)  %d %5.3f\n",m->id->name,p,
                     pri->nact,pri->genMaxTok.like/pri->frame);
               }
            }
            ++frameCount; tact+=pri->nact;
            enTime += sampPeriod;
            if ((showRD || (runmode&(RESULT_IMMED|RESULT_ASAP)))
               && (++trbakCount == trbakFreq)) {
                  TraceBackRecogniser(); trbakCount = 0;
               }
         }
      }
   }
   return FALSE;
}

// Do traceback and if showRD then update display
void ARec::TraceBackRecogniser()
{
   int i,st,en;
   char *p,buf[10];
   MLink m;
   Path *pp;
   float tnow,confidence,ac;
   int iscore;
   PartialPath pptb,ppcb;

   ppcb = CurrentBestPath(pri);

   // get the word that the current best path is within
   p="";
   if (ppcb.node!=NULL && ppcb.node->wordset!=NULL)
      p = ppcb.node->wordset->name;

   // update time
   tnow = float((pri->frame*sampPeriod + stTime)/1e7);
   if (showRD){
      HSetColour(win,BLACK);
      HSetGrey(win,PANELGREY2);
      HFillRectangle(win,x5-1,y4-1,x6+1,y5+1);
      HSetColour(win,BLACK);
      HPrintf(win,x5,y5,"%.1fs",tnow);
   }

   // update HMM, nactive, and score (but ignore silence)
   if (showRD){
      m=FindMacroStruct(hset,'h',pri->genMaxNode->info.hmm);
      assert(m!=NULL);
      if (strcmp(m->id->name,"sil") != 0 && strcmp(m->id->name,"sp") != 0) {
         st = trbakFrame; en = pri->frame;
         ac = float(pri->genMaxTok.like - trbakAc);
         confidence = GetConfidence(pri,st+1,en,ac,"");
         //printf("CONFA %d->%d = %f [%f - %f = %f]\n",st,en,confidence,pri->genMaxTok.like,trbakAc,ac);
         trbakFrame = pri->frame;
         trbakAc = float(pri->genMaxTok.like);
         iscore = (int) (float(x7) + confidence*float(x8-x7));
         if (iscore<=x7) {
            HSetColour(win,RED); HFillRectangle(win,x7,y4+4,x8,y5+1);
         }else if (iscore>=x8) {
            HSetColour(win,DARK_GREEN); HFillRectangle(win,x7,y4+4,x8,y5+1);
         }else {
            HSetColour(win,RED); HFillRectangle(win,iscore,y4+4,x8,y5+1);
            HSetColour(win,DARK_GREEN); HFillRectangle(win,x7,y4+4,iscore,y5+1);
         }
      }
      // update HMM
      HSetGrey(win,PANELGREY2);
      HFillRectangle(win,x10,y4-1,x11,y5+3);
      HSetColour(win,BLACK);
      HPrintf(win,x10,y5,"%s",m->id->name);
      // update nactive
      HSetGrey(win,PANELGREY2);
      HFillRectangle(win,x12,y4-1,x13,y5+1);
      HSetColour(win,BLACK);
      HPrintf(win,x12,y5,"%d", pri->nact);
      // update mode
      HSetGrey(win,PANELGREY2);
      HFillRectangle(win,x14,y4-1,x1,y5+1);
      HSetColour(win,BLACK);
      strcpy(buf,"    ");
      if (runmode&ONESHOT_MODE) buf[0] = '1';
      if (runmode&CONTINUOUS_MODE) buf[0] = 'C';
      if (runmode&RUN_IMMED) buf[1] = 'I';
      if (runmode&FLUSH_TOMARK) buf[1] = 'M';
      if (runmode&FLUSH_TOSPEECH) buf[1] = 'S';
      if (runmode&STOP_IMMED) buf[2] = 'I';
      if (runmode&STOP_ATMARK) buf[2] = 'M';
      if (runmode&STOP_ATSIL) buf[2] = 'S';
      if (runmode&RESULT_ATEND) buf[3] = 'E';
      if (runmode&RESULT_IMMED) buf[3] = 'I';
      if (runmode&RESULT_ASAP) buf[3] = 'A';
      if ((runmode&RESULT_ALL) == RESULT_ALL) buf[3] = 'X';
      HPrintf(win,x14,y5,"%s",buf);
   }

   // if showRD do full traceback
   if (showRD){
      string s = p;
      char buf[256],*bp;

      for (pp = ppcb.path; pp!=NULL; pp=pp->prev){
         if (pp->owner->node->info.pron != NULL)  // it might be a !NULL tag
            s = string(pp->owner->node->info.pron->word->wordName->name) + string(" ") + s;
      }
      if (s != trbak){
         int w = x1-x0-8,txtw;
         trbak = s;
         HSetGrey(win,PANELGREY1);
         strcpy(buf,s.c_str());  bp = buf;
         txtw = HTextWidth(win,bp);
         while (txtw>w) txtw = HTextWidth(win,++bp);
         HFillRectangle(win,x0+1,y6+2,x0+trbakLastWidth+5,y3-2);
         HSetColour(win,BLACK);
         HPrintf(win,x0+4,y3-4,"%s",bp);
         trbakLastWidth = txtw;
      }
   }

   // runmode is RESULT_ASAP, see if anything more can be disambiguated
   if (runmode&RESULT_ASAP){
      pptb = DoTraceBack(pri);
      if (pptb.n>0){
         if (trace&T_PAN) {
            printf(" traceback at frame %.1f\n",tnow);
            PrintPartialPath(pptb,FALSE);
         }
         for (i=1; i<=pptb.n; i++)
            OutPathElement(i,pptb);
      }
   }
   // runmode is RESULT_IMMED, output any new words regardless
   if ((runmode&RESULT_IMMED) && (ppcb.n>0)){
      if (trace&T_IAN) {
         printf(" current best at frame %.1f\n",tnow);
         PrintPartialPath(ppcb,FALSE);
      }
      for (i=1; i<=ppcb.n; i++)
         OutPathElement(i,ppcb);
   }
}

// output a packet and return its sequence number
void ARec::OutPacket(PhraseType k, string wrd, string tag,
                     int pred, int alt, float ac, float lm, float score,
                     float confidence, float nact, HTime start, HTime end)
{
   OutMarkers(start);
   ++outseqnum;
   APhraseData *pd = (APhraseData *)new APhraseData(k,outseqnum,pred);
   pd->alt = alt; pd->ac = ac; pd->lm = lm; pd->score = score;
   pd->confidence = confidence;
   pd->word = wrd;  pd->tag = tag; pd->nact = nact;
   APacket p(pd);
   p.SetStartTime(start); p.SetEndTime(end);
   out->PutPacket(p);
   if (showRD) DrawOutLine(k,start,wrd,tag);
   if (trace&T_OUT) p.Show();
	
#ifdef __APPLE__
	CFMutableDictionaryRef userInfo = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFNumberRef cfPhraseType = CFNumberCreate(NULL, kCFNumberIntType, &k);
	CFDictionaryAddValue(userInfo, CFSTR("PhraseType"), cfPhraseType);
	CFRelease(cfPhraseType);
	
	CFStringRef cfWord = CFStringCreateWithCString(NULL, wrd.c_str(), kCFStringEncodingUTF8);
	CFDictionaryAddValue(userInfo, CFSTR("Word"), cfWord);
	CFRelease(cfWord);
	
	CFStringRef cfTag = CFStringCreateWithCString(NULL, tag.c_str(), kCFStringEncodingUTF8);
	CFDictionaryAddValue(userInfo, CFSTR("Tag"), cfTag);
	CFRelease(cfTag);
	
	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), CFSTR("ARec::OutPacket"), NULL, userInfo, false);
	
	CFRelease(userInfo);
#endif

}

// output an N-best list derived from a Transcription
void ARec::OutTranscription(Transcription *trans)
{
	//for each transcription
	int i, j,n;
	LabList *ll;
	LLink p;
	char *w, *t;
	char buf[20];
	float acScore,lm,score;     // various likelihoods
	float confidence;      // acoustic confidence
	HTime tst,tet;         // start and end times
	PhraseType k;
	float lmScore;
	int startseqnum, altseqnum, numTrans=1;

	ll = trans->head;
	startseqnum=altseqnum=outseqnum;
	for (i=1; i<=trans->numLists; i++,ll=ll->next) {
		score=0.0;
		//set the alt tag  ( the start seq of the next transcription)
		if(i==trans->numLists) {
			altseqnum=0;
		} else {
			altseqnum=outseqnum+3;
			for (p=ll->head->succ; p->succ!= NULL; p=p->succ)
				altseqnum++;
		}
		//output an open tag
		OutPacket(OpenTag_PT, "", "",startseqnum,altseqnum,0.0,0.0,
			score,-1,(float)tact/frameCount,ll->head->succ->start+stTime,
			ll->head->succ->start+stTime);
		// scan the transcription
		for (p=ll->head->succ,j=1; p->succ!=NULL; p=p->succ,j++) {
			w=t=NULL;
			//get info from label
			w=p->labid->name;
			if (w == NULL || strcmp(w,"!NULL")==0){
				k = Null_PT; w=NULL;
			} else {
				k = Word_PT;
			}
			tst=p->start; tet=p->end;
			acScore=lmScore=0.0;
			for (n=1; n<=ll->maxAuxLab; n++){   //Aux labels
				if(p->auxLab[n]!=NULL) {
					if(strcmp(p->auxLab[n]->name, "acscore")==0)
						acScore=p->auxScore[n];
					if(strcmp(p->auxLab[n]->name, "lmscore")==0)
						lmScore=p->auxScore[n];
					if(n==LAB_TAG)
						t=p->auxLab[n]->name;
				}
			}
			if (t!=NULL){
				if (strstr(t,"!SUBLAT_(") != NULL){
					k=OpenTag_PT;  t = NULL;
				}else if (strstr(t,"!)_SUBLAT-") != NULL){
					k=CloseTag_PT; t += 10;
				}
			}
			wordPen = float((k==Word_PT)?pri->wordPen:0.0);
			lm = lmScore + wordPen;
			char *none="";
			confidence =GetConfidence(pri,int (tst/sampPeriod)+1,int (tet/sampPeriod),
				                      acScore,(w!=NULL)?w:none);
			score+=lm+acScore;
			//output packet - if first word, alter traceback/alt info
			OutPacket(k, string((w!=NULL)?w:""), string((t!=NULL)?t:""),
				outseqnum, 0,
				acScore, lm, score, confidence,
				(float)tact/frameCount, tst+stTime, tet+stTime);

		}
		//output a closetag
		sprintf(buf, "%d / %d",i,trans->numLists);
		OutPacket(CloseTag_PT, "", buf,outseqnum,0,0.0,0.0,
			score,-1,(float)tact/frameCount,ll->tail->pred->end+stTime,
			ll->tail->pred->end+stTime);

	}
}

// Output n'th path element via OutPacket
void ARec::OutPathElement(int n, PartialPath pp)
{
   Word wrd = NULL;
   char *w, *t;
   float ac,lm,score;     // various likelihoods
   float confidence;      // acoustic confidence
   HTime tst,tet;         // start and end times
   PhraseType k;
   NetNode *node;
   Path *p = pp.path;
   int i,frame;
   float prevLike,lmScore;

   // not very efficient but paths never very long
   for (i=pp.n; i>n; i--) {assert(p); p = p->prev;}
   assert(p);

   PathMap::const_iterator it = opMap.find(p);
   if (it != opMap.end()){
      i = opMap[p]; opMap[p] += 1;
      return;
   }
   node = p->owner->node; assert(node);
   t = node->tag; w = NULL;
   if (node->info.pron != NULL) wrd = node->info.pron->word;
   if (wrd != NULL) {w = wrd->wordName->name; k = Word_PT;}
   if (wrd == NULL || (wrd != NULL && strcmp(w,"!NULL")==0)){
      k = Null_PT; w = NULL;
   }
   if (t!=NULL){
      if (strstr(t,"!SUBLAT_(") != NULL){
         k=OpenTag_PT;  t = NULL;
      }else if (strstr(t,"!)_SUBLAT-") != NULL){
         k=CloseTag_PT; t += 10;
      }
   }
   frame = (n==1)?pp.startFrame:p->prev->owner->frame;
   tst = frame * sampPeriod;
   tet = p->owner->frame *sampPeriod;
   prevLike = (n==1)?pp.startLike:float(p->prev->like);
   lmScore = p->lm;  /* SJY 10/12/03  - lm used to be unscaled so this value was scaled
                          here, but now scaling done during recognition */
   // pronScore = (k == Word_PT)?node->info.pron->prob*pp.pronScale:0.0;
   wordPen = float((k==Word_PT)?pp.wordPen:0.0);
   ac = float(p->like - prevLike - lmScore - wordPen);
   lm = lmScore + wordPen;
   score = float(p->like);
   char *none="";
   confidence = GetConfidence(pri,frame+1,p->owner->frame,ac,(w!=NULL)?w:none);

   // printf("CONFB %d. ac=%.1f, prevLike=%.1f, lmScore=%.1f, wordPen=%.1f, lm=%.1f, score=%.1f conf=%.1f\n",
   //   p->owner->frame, ac,prevLike,lmScore,wordPen,lm,score,confidence);

   OutPacket(k, string((w!=NULL)?w:""), string((t!=NULL)?t:""),
	     outseqnum, 0, ac, lm, score, confidence, (float)tact/frameCount,
	     tst+stTime, tet+stTime);
   opMap[p] = 1;
}

// Store unrecognised markers for onward transmission
void ARec::StoreMarker(APacket p)
{
   markers.push_back(p);
}

// Forward stored markers before t, all if t -ve
void ARec::OutMarkers(HTime t)
{
   if (markers.size() == 0 ) return;
   APacket p = markers.front();
   HTime pktTime = p.GetStartTime();
   while (t<0 || pktTime < t){
      markers.pop_front();
      out->PutPacket(p);
      if (markers.size() == 0) return;
      p = markers.front();
      pktTime = p.GetStartTime();
   }
}


// scan the lattice and generate output packets
void ARec::ComputeAnswer()
{
   int i;
   PartialPath pp;
   float score;
   Lattice *lat, *prunedlat;
   Transcription *trans;

   pp = FinalBestPath(pri);
   // PrintLatStuff(pp);
   if (pp.n==0) pp = CurrentBestPath(pri);
   if (trace&T_FAN){
      printf("Final complete answer\n");
      PrintPartialPath(pp,TRUE);
      printf("-----------\n");
   }
   // simple 1 best trace for now - need to make this into a chart
   if (runmode&RESULT_ATEND) {
      if(nBest>0){
         lat=NULL;
         lat=CreateLatticeFromOutput(pri, pp, &ansHeap, sampPeriod);
         //prunedlat=LatPrune(&ansHeap, lat, 300, 30);
         if(lat!=NULL) {
            trans=TransFromLattice(&altHeap, lat, nBest);
            if(trace&T_OUT)
               PrintTranscription(trans,"Output Transcription");
            /*DoConfGen(&ansHeap, lat);*/
            OutTranscription(trans);
            Dispose(&ansHeap,lat);
         }
      } else {
         for (i=1; i<=pp.n; i++)
            OutPathElement(i,pp);
      }
   }
   // Send an END packet
   if (runmode&RESULT_ALL) {
      score = 0.0;
      if (pp.n>0) {
         assert(pp.path);
         score = float(pp.path->like);
      }
      OutPacket(End_PT, "", "",outseqnum,0,0.0,0.0,score,-1,
                       (float)tact/frameCount,enTime,enTime);
   }

   CompleteRecognition(pri);
   OutMarkers(-1);
}

// ARec task
TASKTYPE TASKMOD ARec_Task(void * p)
{
   ARec *avp = (ARec *)p;
   char buf[100],cname[100];
   HEventRec e;

   try{
      strcpy(cname,avp->cname.c_str());
      if (avp->showRD){
         avp->win = MakeHWin(cname,avp->rdx0,avp->rdy0,avp->width,avp->height,1);
         avp->InitDrawRD();
      }
      // Initialise Recogniser
      avp->InitRecogniser();
      avp->in->RequestBufferEvents(INBUFID);
      avp->RequestMessageEvents();
      while (!avp->IsTerminated()){
         e = HGetEvent(0,0);
         if (avp->trace&T_RST)
            printf(" ARec event=%d ev.c=%d state=%d mode=%o\n",
            e.event,e.c,(int)avp->runstate,(int)avp->runmode);
         switch(e.event){
         case HTBUFFER:
            switch(e.c){
            case MSGEVENTID:
               avp->ChkMessage();
               while (avp->IsSuspended()) avp->ChkMessage(TRUE);
               if (avp->runstate==ANS_STATE){
                  avp->ComputeAnswer();
                  avp->runstate = (avp->runmode&CONTINUOUS_MODE)?PRIME_STATE:WAIT_STATE;
               }
               break;
            case INBUFID:
               switch (avp->runstate) {
               case PRIME_STATE:
                  avp->PrimeRecogniser();
                  avp->runstate = FLUSH_STATE;
               case FLUSH_STATE:
                  if (avp->FlushObservation()) {
                     if (avp->terminated) break;   // only possible if file input
                     avp->runstate = RUN_STATE;
                  } else
                     break;
               case RUN_STATE:
                  if (avp->RecObservation())
                     avp->runstate = ANS_STATE;
                  else
                     break;
               case ANS_STATE:
                  avp->ComputeAnswer();
                  avp->runstate = (avp->runmode&CONTINUOUS_MODE)?PRIME_STATE:WAIT_STATE;
                  break;
               }
               // update display
               if (avp->showRD) avp->DrawStatus();
               break;
            }
            break;
            case HWINCLOSE:
               sprintf(buf, "Terminating\n");
               HPostMessage(HThreadSelf(),buf);
               avp->terminated = TRUE;
               break;
         }
      }
      // forward a terminated message to output buffer
      if (avp->trace&T_TOP) printf("%s recogniser exiting\n",avp->cname.c_str());
      avp->SendMarkerPkt("TERMINATED");
      HExitThread(0);
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}

}

// ----------------------End of ARec.cpp ---------------------
