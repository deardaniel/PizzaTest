/* ----------------------------------------------------------- */
/*                                                             */
/*                        _ ___                                */
/*                       /_\ | |_/                             */
/*                       | | | | \                             */
/*                       =========                             */
/*                                                             */
/*       Real-time API for HTK-based Speech Recognition        */
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
/*        File: ACode.cpp -    Implementation of Coder         */
/* ----------------------------------------------------------- */

char * acode_version="!HVER!ACode: 1.6.0 [SJY 01/06/07]";

// Modification history:
//   9/12/02 - added PRBUFSIZE for printf display monitor
//   5/08/03 - standardised display config var names
//  11/08/04 - static variables removed
//  21/06/05 - added multiple parameterisation support - MNS

#include "ACode.h"
#define INBUFID 1
#define ACODEPRBUFSIZE 5

#define T_TOP 0001    // top level tracing
#define T_OUT 0002    // trace output packets

#define PANELGREY 40

// ------------- HParm External Source Interface ---------------

static Ptr xOpen(Ptr xInfo, char *fn, BufferInfo *info)
{
   ACode *owner = (ACode *) xInfo;
   owner->inBufUsed = 0;
   return (Ptr)owner->in;
}

static void xClose(Ptr xInfo, Ptr bInfo) {}
static void xStart(Ptr xInfo, Ptr bInfo) {}
static void xStop(Ptr xInfo, Ptr bInfo) {}

static int xNumSamp(Ptr xInfo, Ptr bInfo)
{
   int n = 0;
   int maxn = INBUFSIZE - WAVEPACKETSIZE;

   ACode *owner = (ACode *) xInfo;
   ABuffer *in = (ABuffer *) bInfo;
   n = in->NumPackets() * WAVEPACKETSIZE;  // assume all pkts full
   if ( n > maxn) n = maxn;
   return n;
}

static int xGetData(Ptr xInfo, Ptr bInfo, int n, Ptr data)
{
   ACode *owner = (ACode *) xInfo;
   ABuffer *in = (ABuffer *) bInfo;
   AWaveData *wp;
   AStringData *sd;
   int i,j;
   short *t;

   if (owner->isFlushing){
      for (i=owner->inBufUsed; i<INBUFSIZE; i++)
         owner->inBuffer[i] = FakeSilenceSample();
      owner->inBufUsed = INBUFSIZE;
   }else{
      while (n > owner->inBufUsed){
         APacket pkt = in->GetPacket();
         switch(pkt.GetKind()){
         case WavePacket:
            if (owner->timeNow<0.0)
               owner->timeNow = pkt.GetStartTime();
            wp = (AWaveData *)pkt.GetData();
            for (i=0; i<wp->wused; i++)
               owner->inBuffer[owner->inBufUsed++] = wp->data[i];
            break;
         default:
            owner->HoldPacket(pkt);
         }
      }
   }
   t = (short *) data;
   for (i=0; i<n; i++) *t++ = owner->inBuffer[i];
   for (i=0,j=n; j<owner->inBufUsed; i++,j++)
      owner->inBuffer[i]=owner->inBuffer[j];
   owner->inBufUsed -= n;
   return n;
}

// ------------------- ACode Class --------------------------------

// ACode constructor
ACode::ACode(const string & name, ABuffer *inb, ABuffer *outb, char *confName)
: AComponent(name,(HasRealConsole()?2:ACODEPRBUFSIZE))
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i;
   Boolean b;
   char buf[100];

   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   in = inb; out = outb;
   width = 400; height=220; showFG = FALSE; FGinit=FALSE;
   fgx0 = 420;   fgy0 = 80; maxFrames = 50; isFlushing = FALSE;
   numStreams = 1; timeNow = -1.0; maxFeats = 1000; trace = 0;
   lastDisplayTime = 0;
   if (numParm>0){
      if (GetConfBool(cParm,numParm,"DISPSHOW",&b)) showFG = b;
      if (GetConfInt(cParm,numParm,"DISPXORIGIN",&i)) fgx0 = i;
      if (GetConfInt(cParm,numParm,"DISPYORIGIN",&i)) fgy0 = i;
      if (GetConfInt(cParm,numParm,"DISPWIDTH",&i)) width = i;
      if (GetConfInt(cParm,numParm,"DISPHEIGHT",&i)) height = i;
      if (GetConfInt(cParm,numParm,"MAXFEATS",&i)) maxFeats = i;
      if (GetConfInt(cParm,numParm,"NUMSTREAMS",&i)) numStreams = i;
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   CreateHeap(&mem, buf, MSTAK, 1, 1.0, 10000, 50000);
   ext = CreateSrcExt(this, WAVEFORM, 2, 0.0, xOpen, xClose,
      xStart, xStop, xNumSamp, xGetData);
   /* if a channel name is supplied, use that to initialise a coder channel */
   if(confName==NULL) {
     if((pbuf = OpenBuffer(&mem,"",ext))==NULL){
       HRError(10200,"ACode: OpenBuffer failed - check config");
       throw HTK_Error(10200);
     }
   }
   else {
     chan=SetCoderChannel(confName);
     if((pbuf = OpenChanBuffer(&mem,"",ext,chan))==NULL){
       HRError(10200,"ACode: OpenChanBuffer failed - check config");
       throw HTK_Error(10200);
     }
   }
   GetBufferInfo(pbuf,&info);

}

HTime ACode::GetSampPeriod()
{
   return info.tgtSampRate;
}

// Start the task
TASKTYPE TASKMOD ACode_Task(void * p);
void ACode::Start(HPriority priority)
{
   AComponent::Start(priority,ACode_Task);
}

void ACode::SetCodeHMMSet(AHmms *ahmms)
{

  if(ahmms->GetHMMSet()==NULL) {
    HRError(10999,"Error setting hmmset for coder: hmmset not defined");
    throw HTK_Error(10999);
  }
  SetChanHMMSet(chan, ahmms->GetHMMSet());
}

void ACode::ShowFrame()
{
   int i,j,k;
   AObsData *op = (AObsData *)fgo[lastIn].GetData();
   Vector v;

   for (i = 1, k=0; i<=op->data.swidth[0]; i++) {
      v = op->data.fv[i];
      for (j = 1; j<=op->data.swidth[i]; j++,k++){
         printf("%6.1f",v[j]);
      }
      printf("\n");
   }
}

// Paint the column col with fgo[lastIn], col 0 is leftmost
void ACode::PaintCol(int col, int fgIdx)
{
   int i,j,k,iv;
   int x = x1- cellw*(col+1);
   int y = y0;
   AObsData *op;
   AStringData *sd;

   switch (fgo[fgIdx].GetKind()){
   case ObservationPacket:
      op = (AObsData *)fgo[fgIdx].GetData();
      Vector v;
      for (i = 1, k=0; i<=op->data.swidth[0]; i++) {
         v = op->data.fv[i];
         for (j = 1; j<=op->data.swidth[i] && k<nFeats; j++,k++){
            iv = (int) (v[j]*scale[k] + offset[k]);
            if (iv<0) iv = 0; else if (iv>63)iv = 63;
            HSetGrey(win,iv);
            HFillRectangle(win,x,y,x+cellw,y+cellh);
            y += cellh;
         }
      }
      break;
   case StringPacket:
      sd = (AStringData *)fgo[fgIdx].GetData();
      string marker = sd->GetMarker();
      if (marker=="START")
         HSetColour(win,DARK_GREEN);
      else if ( marker == "STOP")
         HSetColour(win,RED);
      else
         HSetColour(win,WHITE);
      HFillRectangle(win,x,y,x+cellw,y+cellh*nFeats);
      y += cellh*nFeats;
      break;
   }
   // Draw Speech/Silence decision
   Boolean isSpeech = spDet[fgIdx];
   if (isSpeech)HSetColour(win,DARK_GREEN); else HSetColour(win,YELLOW);
   HFillRectangle(win,x,y,x+cellw,y+2*cellh);
   // Update time
   DisplayTime(fgo[fgIdx].GetStartTime());
   // Update Status
   DrawStatus();
}

// Compute scale and offset from the nFrame packets in fgo
void ACode::SetScaling()
{
   int i,j,k,n,pktIdx;
   float *min = new float[nFeats];
   float *max = new float[nFeats];
   Vector v;
   float val;
   AObsData *op;

   for (i=0; i<nFeats; i++){
      min[i]=1e8; max[i]=-1e8;
   }
   // scan thru packets and find min and max
   pktIdx = lastIn;
   for (n=0; n<nFrames; n++){
      if (fgo[pktIdx].GetKind() == ObservationPacket){
         op = (AObsData *)fgo[pktIdx].GetData();
         if (pktIdx==0)pktIdx=maxFrames; --pktIdx;
         k=0;
         for (i = 1; i<=op->data.swidth[0]; i++) {
            v = op->data.fv[i];
            for (j = 1; j<=op->data.swidth[i] && k<nFeats; j++){
               val = v[j];
               if (max[k]<val) max[k]=val;
               if (min[k]>val) min[k]=val;
               k++;
            }
         }
         assert(k==nFeats);
      }
   }
   // now set offset and scaling so that
   //   min*scale+offset = 0 and max*scale+offset=50
   for (k=0; k<nFeats; k++){
      scale[k] = 50 / (max[k]-min[k]);
      offset[k] = -min[k]*scale[k];
   }
   delete[] min;
   delete[] max;
}

// Display current time
void ACode::DisplayTime(HTime t)
{
   int now;
   char buf[30];

   now = (int) (t/1000000.0);
   if (now != lastDisplayTime){
      lastDisplayTime = now;
      sprintf(buf,"%.1fs",(float)now/10.0);
      HSetGrey(win,50);
      HFillRectangle(win,x2,y2-HTextHeight(win,buf)-1,
         x2+HTextWidth(win,buf)+1,y2+1);
      HSetColour(win,BLACK);
      HPrintf(win,x2,y2,"%s",buf);
   }
}

// Redraw all nFrame packets in fgo aligned to left of window
void ACode::RedrawDisplay()
{
   int fgIdx = lastIn;

   for (int col=0; col<nFrames; col++){
      PaintCol(col,fgIdx);
      if (fgIdx == 0)fgIdx = maxFrames; --fgIdx;
   }
}

// Scroll display and then update lastIn column at left edge.
void ACode::UpdateDisplay()
{
   int x = x1-cellw;
   int y = y0;

   HCopyArea(win,x0+cellw,y0,x1-x0-cellw,y1-y0,x0,y0);
   PaintCol(0,lastIn);
}

// Get sil/det parameters and display if changed
void ACode::DisplaySilDetParms()
{
   static float xsnr=0.0,xsil=0.0,xsp=0.0,xthr=0.0;
   float snr,sil,sp,thr;
   char buf[100];

   if (GetSilDetParms(pbuf, &sil, &snr, &sp, &thr)){
      if (sil!=xsil || snr != xsnr || sp != xsp || thr != xthr){
         xsil = sil; xsnr = snr; xsp = sp; xthr = thr;
         sprintf(buf,"Sil=%.1fdB, Sp=%.1fdB, TH=%.1fdB, SNR=%.1fdB",
            sil,sp,thr,snr);
         HDrawPanel(win,x0-3,y3-HTextHeight(win,buf)-1,x1+2,y3+2,PANELGREY);
         HSetColour(win,WHITE);
         HPrintf(win,x0,y3-1,"%s",buf);
      }
   }
}

// Draw current parm buf status
void ACode::DrawStatus()
{
   static PBStatus last = PBStatus(-1);
   PBStatus pbstate = BufferStatus(pbuf);

   if (last != pbstate){
      switch(pbstate){
      case PB_INIT:        HSetColour(win,LIGHT_GREY); break;
      case PB_CALIBRATING: HSetColour(win,YELLOW); break;
      case PB_RUNNING:     HSetColour(win,DARK_GREEN); break;
      case PB_STOPPED:     HSetColour(win,ORANGE); break;
      case PB_CLEARED:     HSetColour(win,RED); break;
      }
      HFillRectangle(win,x4,y4+2,x4+20,y2+2);
      last = pbstate;
   }
}

// Draw Button
void ACode::DrawButton()
{
   RedrawHButton(&calb);
}

// Button Press
void ACode::ButtonPressed()
{
   CalibrateCmd(0.0);
   DrawButton();
}

// Draw/Update FeatureGram - pkt contains new features to add
void ACode::DrawFG(APacket pkt)
{
   const int margin = 6;
   int botMargin;
   char buf1[100],buf2[20];
   AObsData *od = (AObsData *)pkt.GetData();
   Observation *o = &(od->data);
   int but_w = 70,textheight;

   if (!FGinit){
      // calc number of features
      nFeats = 0;
      for (int i = 1; i<=o->swidth[0]; i++) nFeats += o->swidth[i];
      if (nFeats>maxFeats) nFeats = maxFeats;
      scale = new float[nFeats];  offset = new float[nFeats];
      fgo = new APacket[maxFrames];
      spDet = new Boolean[maxFrames];
      nFrames = 0; lastIn = maxFrames-1;
      strcpy(buf1,"Feats: ");
      x0 = margin;  x1 = width - margin;
      cellw = (x1-x0)/maxFrames;
      x0 = (width - cellw * maxFrames)/2;
      x1 = x0 + cellw * maxFrames;
      textheight = HTextHeight(win,buf1);
      botMargin = 2*textheight + 3*margin;
      y0 = margin;  y1 = height - botMargin;
      cellh = (y1-y0)/(nFeats+2);   //+2 for sil/sp mark
      y1 = y0 + cellh * (nFeats+2);
      y2 = y1+margin+textheight;
      y3 = y2+2*margin+textheight;
      // paint background
      HDrawPanel(win,x0-3,y0-3,x1+2,y1+2,PANELGREY);
      // write info
      ParmKind2Str(o->bk,buf2);
      HSetGrey(win,30);
      HPrintf(win,x0,y2,"%s",buf1);
      HSetColour(win,BLACK);
      HPrintf(win,x0+HTextWidth(win,buf1),y2,"%s",buf2);
      strcat(buf1,buf2);
      x2 = x0+HTextWidth(win,buf1)+margin;
      x3 = x2+HTextWidth(win,"9999s")+margin;
      // pbuf status display
      HSetGrey(win,30);
      HPrintf(win,x3,y2,"PBuf:");
      x4 = x3 + HTextWidth(win,"PBuf:")+margin;
      y4 = y2-textheight;
      // create button
      calb.x = x1-but_w; calb.y = y1+margin;
      calb.w = but_w; calb.h = textheight+margin;
      calb.fg = DARK_GREY; calb.bg = LIGHT_GREY; calb.lit = FALSE;
      calb.active = TRUE; calb.toggle = TRUE;  calb.fontSize=0;
      strcpy(calbname,"Calibrate"); calb.str = calbname;
      calb.id = 1; calb.win = win;
      calb.next = 0; calb.action = 0;
      RedrawHButton(&calb);
      // only do all this once
      FGinit = TRUE;
   }
   // Update the current FG - saved packets just wrap round in fgo, with lastIn
   // marking the most recently saved.  Scaling is updated when 5 and 19 frames
   // are available.  Nothing is displayed until 5 frames available. Normally
   // new column corresponding to lastIn is updated and whole display scrolled.
   // When scaling changed, whole display is redrawn.
   if (nFrames<maxFrames) ++nFrames;
   lastIn++;
   if (lastIn == maxFrames)lastIn = 0;
   fgo[lastIn] = pkt;
   spDet[lastIn] = (o->vq[0] == 1)?TRUE:FALSE;
   if (nFrames>0){
      if (nFrames==5 || nFrames==maxFrames-1){
         SetScaling();
         RedrawDisplay();
      }else
         UpdateDisplay();
      DisplaySilDetParms();
   }
}

// Draw/Update FeatureGram - pkt contains new features to add
void ACode::AddMkr2FG(APacket pkt)
{
   if (FGinit){
      if (nFrames<maxFrames) ++nFrames;
      lastIn++;
      if (lastIn == maxFrames)lastIn = 0;
      fgo[lastIn] = pkt;
      spDet[lastIn] = FALSE;
      UpdateDisplay();
   }
}

// Create and fill an obs data packet
APacket ACode::CodePacket()
{
   AObsData *o = new AObsData(&info,numStreams);
   if(!ReadBuffer(pbuf,&(o->data))){
      HRError(10290,"Read buffer failed");
      throw HTK_Error(10290);
   }
   APacket pkt(o);
   pkt.SetStartTime(timeNow);
   timeNow += info.tgtSampRate;
   pkt.SetEndTime(timeNow);
   return pkt;
}

// Return a specimen obsdata container
AObsData * ACode::GetSpecimen()
{
   return new AObsData(&info,numStreams);
}

// Calibrate command
void ACode::CalibrateCmd(HTime t)
{
   CalibrateSilDet(pbuf);
}

// Implement the command interface
void ACode::ExecCommand(const string & cmdname)
{
   char buf[100];
   float ft;
   HTime t;

   if (cmdname == "cmnreset") {
      ResetMeanRec(pbuf);
   }else
      if (cmdname == "calibrate") {
         if (GetFltArg(ft,0.0,-1.0)){
            t = ft;	CalibrateCmd(t);
         }
      } else {
         sprintf(buf,"Unknown command %s\n",cmdname.c_str());
         HPostMessage(HThreadSelf(),buf);
      }
}

// -------------  Marker Hold List Processing --------------

// True if there is a marker packet waiting older than tnow
Boolean ACode::HoldReady(HTime tnow)
{
   if (holdList.size()==0) return FALSE;
   APacket mkr = holdList.front();
   if (mkr.GetStartTime() > tnow) return FALSE;
   return TRUE;
}

// forward the frontmost marker
void ACode::ForwardMkr()
{
   APacket mkr = holdList.front();
   holdList.pop_front();
   if (showFG) AddMkr2FG(mkr);
   out->PutPacket(mkr);
   if (trace&T_OUT)mkr.Show();
}

// Forward all held markers older than tnow.
void ACode::ForwardOldMkrs(HTime tnow)
{
   while (HoldReady(tnow)) ForwardMkr();
}

// Forward all held markers
void ACode::ForwardAllMkrs()
{
   while (holdList.size()>0) ForwardMkr();
}



// Hold the given packet til HParm catches up
void ACode::HoldPacket(APacket p)
{
   string marker;
   holdList.push_back(p);
   if (p.GetKind()==StringPacket){
      AStringData * sd = (AStringData *)p.GetData();
      marker = sd->GetMarker(TRUE);
      if (marker == "SYNTHSTART"){
         InhibitCalibration(pbuf, TRUE);
      }else if (marker == "SYNTHSTOP"){
         InhibitCalibration(pbuf, FALSE);
      }else if (marker=="START") {
         timeNow = p.GetStartTime();
         isFlushing = FALSE;
         StartBuffer(pbuf);
         ResetIsSpeech(pbuf);
      }else if (marker=="STOP") {
         // calculate number of frames needed from buffer
         int endEffect = 0;
         if (HasDelta(info.tgtPK)) ++endEffect;
         if (HasAccs(info.tgtPK)) ++endEffect;
         if (HasThird(info.tgtPK)) ++endEffect;
         int numMissing = (int) ( (float) (p.GetStartTime() - timeNow) / info.tgtSampRate - endEffect);
         isFlushing = TRUE;
         while (numMissing>0) {
            APacket cp = CodePacket();
            out->PutPacket(cp);	--numMissing;
            if (trace&T_OUT)cp.Show();
         }
         ResetBuffer(pbuf); inBufUsed=0;
      }
   }
}

// ACode task
TASKTYPE TASKMOD ACode_Task(void * p)
{
   ACode *acp = (ACode *)p;
   APacket pkt,mkr;
   char buf[100],cname[100];
   HEventRec e;
   HTime tnow;
   PacketKind inpk;

   try{
      strcpy(cname,acp->cname.c_str());
      if (acp->showFG){
         acp->win = MakeHWin(cname,acp->fgx0,acp->fgy0,acp->width,acp->height,1);
         HSetGrey(acp->win,50);
         HFillRectangle(acp->win,0,0,acp->width,acp->height);
      }
      StartBuffer(acp->pbuf);
      acp->in->RequestBufferEvents(INBUFID);
      acp->RequestMessageEvents();
      while (!acp->IsTerminated()){
         e = HGetEvent(0,0);
         switch(e.event){
            case HTBUFFER:
               switch(e.c){
                  case MSGEVENTID:
                     acp->ChkMessage();
                     while (acp->IsSuspended()) acp->ChkMessage(TRUE);
                     break;
                  case INBUFID:
                     // code a packet if HParm already has observations
                     // or next input packet is a wavepacket so we know that HParm
                     // wont block if we go ahead and get an obs anyway
                     inpk=(FramesNeeded(acp->pbuf)<=0)?WavePacket:acp->in->GetFirstKind();
                     while (inpk == WavePacket || inpk == StringPacket){
                        switch (inpk){
                           case WavePacket:
                              pkt = acp->CodePacket();
                              if (acp->showFG) acp->DrawFG(pkt);
                              tnow = pkt.GetStartTime();
                              // forward any pending marker packets
                              acp->ForwardOldMkrs(tnow);
                              acp->out->PutPacket(pkt);
                              if (acp->trace&T_OUT) pkt.Show();
                              break;
                           case StringPacket:
                              pkt = acp->in->GetPacket();
                              acp->HoldPacket(pkt);
                              AStringData * sd = (AStringData *)pkt.GetData();
                              if (sd->GetMarker() == "TERMINATED") {
                                 sprintf(buf, "terminated via input");
                                 HPostMessage(HThreadSelf(),buf);
                                 acp->terminated = TRUE;
                              }
                              break;
                        }
                        inpk=(FramesNeeded(acp->pbuf)<=0)?WavePacket:acp->in->GetFirstKind();
                     }
                     // input audio now processed so forward any remaining held packets
                     acp->ForwardAllMkrs();
               } /* switch(e.c) */
               break;

            case HWINCLOSE:
               sprintf(buf, "terminating");
               HPostMessage(HThreadSelf(),buf);
               acp->terminated = TRUE;
               break;
            case HMOUSEDOWN:
               if (acp->FGinit && TrackButtons(&(acp->calb),e)>0)
                  acp->ButtonPressed();
         }
      }
      // forward a terminated message to output buffer
      AStringData *sd = new AStringData(acp->cname+"::TERMINATED");
      APacket mkrpkt(sd);
      acp->out->PutPacket(mkrpkt);
      if (acp->trace&T_TOP) printf("%s coder exiting\n",acp->cname.c_str());
      HExitThread(0);
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}

// ----------------------End of ACode.cpp ---------------------
