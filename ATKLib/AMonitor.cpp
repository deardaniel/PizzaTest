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
/*   File: AMonitor.cpp -   Implementation of Monitor Class    */
/* ----------------------------------------------------------- */


char * amonitor_version="!HVER!AMonitor: 1.6.0 [SJY 01/06/07]";

// Modification history:
//   9/12/02 - added display of thread print buffers
//   7/01/03 - removed main print panel when it has a real console
//   5/08/03 - standardised display config var names
//   8/05/05 - termination cleaned up - SJY

#include "AMonitor.h"

// --------------------- CompMonitor ------------------------

// There is one CompMonitor instance for each monitored
// component task

// Component Monitor Constructor
CompMonitor::CompMonitor(AComponentPtr comp, HWin win,
                         int x, int y, int width)
{
   const int margin = 5;  // displaylet margin
   const int xspace = 3;  // horizantal spacing
   const int yspace = 4;  // vertical spacing
   const int ybar = 24;   // height of bar elements
   // set dimensions defined by parent
   theComp = comp; theWin = win;
   x0=x; y0=y; w=width;
   msgSize = theComp->prBufLines;
   strcpy(cname,comp->cname.c_str());
   msgFont = -12; cmdFont = -14;
   // derive internal horizantal anchor points
   HSetFontSize(win,0);
   x1 = x+margin; x6=x+w-margin;
   x2 = x1+xspace+HTextWidth(win,"Cmd: ");
   x3 = x1+xspace+HTextWidth(win,cname);
   if (x3<x2) x3=x2;
   x4 = x3+(x6-x3-xspace)/2;
   x5 = x4+xspace;
   // derive internal vertical anchor points
   HSetFontSize(win,msgFont);
   msgLineHeight = HTextHeight(win,"ABC");
   y1 = y+margin;
   y2 = y1+ybar; y3=y2+yspace; y4=y3+ybar;
   y5 = y4+yspace;
   y6 = y5+msgSize*msgLineHeight + 2;
   d = y6-y0 + margin;
   // initialise text entry
   cmdLength=0;  buf[0] = '\0'; cursorPos=0; cmdActive = FALSE;
}

// LabelIndicator: print label in black in centre of rect (x0,y0,x1,y1)
static void LabelIndicator(HWin w, int x0,int y0,int x1, int y1, char * label)
{
   char *p = label;
   int width = x1-x0;
   HSetColour(w,BLACK);
   HSetFontSize(w,0);
   while (HTextWidth(w,p) > width) ++p;
   int x = CentreX(w,width/2,p);
   int y = CentreY(w,(y1-y0)/2,p);
   HPrintf(w,x0+x,y0+y,"%s",p);
}

// DrawCmdBuffer: print current command s inside rect (x0,y0,x1,y1)
//    the part of s printed is adjusted so that the cursor is in view
static void  DrawCmdBuffer(HWin w, Boolean active, char *s, int curpos,
                           int x0,int y0,int x1,int y1,int font)
{
   int xcur,i,len;
   int width = x1-x0-8;
   char buf[CMDBUFSIZE];

   strcpy(buf,s); i=0;
   while (HTextWidth(w,buf) > width){
      len=strlen(buf);
      if (curpos>1) {
         --curpos; ++i; strcpy(buf,s+i);
      }else {
         buf[len-1] = '\0';
      }
   }
   HSetGrey(w,63);
   HFillRectangle(w,x0+1,y0+1,x1,y1);
   HSetGrey(w,(active)?0:40);
   HSetFontSize(w,font);
   HPrintf(w,x0+4,y1-5,"%s",buf);
   if (active){
      buf[curpos] = '\0';
      xcur = x0+HTextWidth(w,buf)+4;
      HDrawLine(w,xcur,y0+2,xcur,y1-2);
   }
}

// DrawMessages from given component in given area of window
static void DrawMessages(HWin w, HThread t, int x0, int y0, int x1, int y1, int ht, int font)
{
   char buf[PRBUFLINESIZE+1];
   int availWidth,len;
   HSetFontSize(w,font);
   while (HGetMessage(t,buf) != NULL) {
      HCopyArea(w,x0+1,y0+ht+1,x1-x0-1,y1-y0-ht+1,x0+1,y0+1);
      HSetGrey(w,50);
      HFillRectangle(w,x0+1,y1-ht,x1,y1);
      HSetGrey(w,0);
      availWidth = x1 - x0 - 8;
      len = strlen(buf);
      while (len>0 && HTextWidth(w,buf) > availWidth){
         --len;
         buf[len] = '\0';
      }
      HPrintf(w,x0+4,y1-2,"%s",buf);
   }
}

// Redraw an individual component display
void CompMonitor::Redraw(int lev)
{
   if (lev>0){
      // Draw Panel
      HDrawPanel(theWin,x0,y0,x0+w,y0+d,40);
      // Add labels
      HSetGrey(theWin,0);
      HSetFontSize(theWin,0);
      int txtht = HTextHeight(theWin,"ABC");
      HPrintf(theWin,x1,y1+txtht,"%s",theComp->cname.c_str());
      HPrintf(theWin,x1,y3+txtht,"%s","Cmd:");
      HPrintf(theWin,x1,y5+txtht,"%s","Msg:");
      // Add Boxes
      HSetGrey(theWin,20);
      HDrawRectangle(theWin,x3,y1,x4,y2);
      HDrawRectangle(theWin,x5,y1,x6,y2);
      HDrawRectangle(theWin,x2,y3,x6,y4);
      HDrawRectangle(theWin,x2,y5,x6,y6);
      // Draw Cmd Buffer
      DrawCmdBuffer(theWin,cmdActive,buf,cursorPos,x2,y3,x6,y4,cmdFont);
   }
   // Check component is active
   if (theComp->thread == NULL){
      return;
   }
   AccessStatus();
   // Draw lock state
   HColour c;
   switch(theComp->thread->status){
   case THREAD_INITIAL: c=LIGHT_GREY; break;
   case THREAD_WAITING: c=RED; break;
   case THREAD_RUNNING: c=LIGHT_GREEN; break;
   case THREAD_CRITICAL: c=ORANGE; break;
   case THREAD_STOPPED: c=GREY; break;
   }
   HSetColour(theWin,c);
   HFillRectangle(theWin,x3+1,y1+1,x4,y2);
   if (theComp->thread->info->inLock)
      LabelIndicator(theWin,x3,y1,x4,y2,theComp->thread->info->lock->name);
   if (theComp->thread->info->inSignal){
      HSetColour(theWin,YELLOW);
      HFillRectangle(theWin,x5+1,y1+1,x6,y2);
      LabelIndicator(theWin,x5,y1,x6,y2,theComp->thread->info->signal->name);
   }else{
      HSetColour(theWin,LIGHT_GREY);
      HFillRectangle(theWin,x5+1,y1+1,x6,y2);
   }
   ReleaseStatusAccess();

   // Print any messages
   DrawMessages(theWin,theComp->thread,x2,y5,x6,y6, msgLineHeight, msgFont);
}

// Process a keypress event intended for this component display
void CompMonitor::ProcessEvent(HEventRec e)
{
   int i;

   if (e.event== HKEYPRESS){
      if (!cmdActive){
         cmdActive = TRUE;
         cursorPos=0; cmdLength=0;
      }
      switch (e.ktype){
      case NORMALKEY:
         if (cmdLength>=CMDBUFSIZE-1) break;
         for (i=cmdLength; i>cursorPos; i--) buf[i]=buf[i-1];
         buf[cursorPos++] = e.c; cmdLength++;
         break;
      case DELKEY:
         if (cursorPos>0){
            for (i=cursorPos; i<cmdLength; i++) buf[i-1]=buf[i];
            --cmdLength; --cursorPos;
         }
         break;
      case LEFTARROWKEY:
         if (cursorPos>0) --cursorPos;
         break;
      case RIGHTARROWKEY:
         if (cursorPos<cmdLength) ++cursorPos;
         break;
      case ENTERKEY:
         if (cmdLength>0){
            if (theComp->SendMessage(string(buf)))
               cmdActive = FALSE;
         }
         break;
      }
      buf[cmdLength] = '\0';
      DrawCmdBuffer(theWin,cmdActive,buf,cursorPos,x2,y3,x6,y4,cmdFont);
   }
}

// -------------------------- Monitor ------------------------

// Just one monitor instance can be created.  It handles all
// registered components by creating a CompMonitor instance
// for each of them

static int NumMonitorInstances = 0;

// Monitor Constructor - call only once
AMonitor::AMonitor()
{
   if (NumMonitorInstances++ > 0){
      HRError(10400,"AMonitor: only one instance allowed");
      throw ATK_Error(10400);
   }
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm = GetConfig("AMONITOR", TRUE, cParm, MAXGLOBS);
   int i;
   // set basic dimensions of monitor
   margin = 5;  spacing = 5;
   dlwidth = 300;
   winx0 = 30; winy0 = 20;
   msgFont = -12;
   // check for config settings
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"DISPXORIGIN",&i)) winx0 = i;
      if (GetConfInt(cParm,numParm,"DISPYORIGIN",&i)) winy0 = i;
      if (GetConfInt(cParm,numParm,"DISPWIDTH",&i)) dlwidth = i;
   }
   terminated = FALSE;
}

// Add component to monitor
void AMonitor::AddComponent(AComponentPtr comp)
{
   compList.push_back(comp);
}

// Redraw entire monitor display
void AMonitor::Redraw(int lev)
{
   typedef list<CompMonPtr>::const_iterator LI;

   if (lev>1){
      HSetGrey(theWin,50);
      HFillRectangle(theWin,0,0,width,depth);
   }
   for (LI i = dispList.begin(); i != dispList.end(); ++i)
      (*i)->Redraw(lev);
   DrawMessages(theWin,HThreadSelf(),5,msgy0,dlwidth-5,msgy1,msgLineHeight,msgFont);
   DrawMessages(theWin,NULL,5,msgy0,dlwidth-5,msgy1,msgLineHeight,msgFont);
}

// Pass event to individual displaylet
void AMonitor::PassEvent(HEventRec e)
{
   typedef list<CompMonPtr>::const_iterator LI;

   int dy0 = dly0, dy1;

   for (LI i = dispList.begin(); i != dispList.end(); ++i) {
      dy1 = dy0 + (*i)->d;
      if (e.y>=dy0 && e.y<=dy1){
         (*i)->ProcessEvent(e);
         return;
      }
      dy0 += (*i)->d+spacing;
   }
}

// The monitor task itself
TASKTYPE TASKMOD Monitor_Task(void * monp)
{
   AMonitor *mon = (AMonitor *)monp;
   HEventRec nexte,e;
   Boolean hasNextEvent;
   CompMonPtr p;
   typedef list<AComponentPtr>::const_iterator LI;
   try{
      // compute various sizes
      int numComps = mon->compList.size();
      mon->width = mon->dlwidth + 2*mon->margin;
      mon->depth = 2*mon->margin;
      HWin w = MakeHWin("Temp",0,0,1,1,1);
      HSetFontSize(w,mon->msgFont);
      mon->msgLineHeight = HTextHeight(w,"ABC");
      for (LI i = mon->compList.begin(); i != mon->compList.end(); ++i) {
         p = new CompMonitor(*i,w,0,0,mon->dlwidth);
         mon->depth += p->d + mon->spacing;
         delete p;
      }
      int MainPRBufSize = (HasRealConsole())?0:MAINPRBUFSIZE;
      mon->depth += MainPRBufSize * mon->msgLineHeight + mon->spacing;
      CloseHWin(w);

      int y = mon->margin;
      mon->dlx0 = mon->margin; mon->dlx1 = mon->width - mon->margin;
      mon->dly0 = mon->margin; mon->dly1 = mon->depth - mon->margin;

      // create the displaylets
      mon->theWin = MakeHWin("Monitor",mon->winx0,mon->winy0,
         mon->width,mon->depth,4);
      for (LI i = mon->compList.begin(); i != mon->compList.end(); ++i) {
         p = new CompMonitor(*i,mon->theWin,mon->dlx0,y, mon->dlwidth);
         y += p->d + mon->spacing;
         mon->dispList.push_back(p);
      }
      mon->msgy0 = y; mon->msgy1 = mon->msgy0+ MainPRBufSize * mon->msgLineHeight;
      mon->Redraw(2);
      do {
         while (HEventsPending(NULL)==0) {
            HPauseThread(100);
            mon->Redraw(0);
         }
         e = HGetEvent(NULL,NULL);
         // service window messages
         switch(e.event){
            case HTMONUPD:
               mon->Redraw(0);
               break;
            case HREDRAW:
               mon->Redraw(1);
               break;
            case HWINCLOSE:
               printf("Exiting monitor\n");
               HExitThread(0);
               break;
            default:
               if (IsInRect(e.x,e.y,mon->dlx0,mon->dly0,
                  mon->dlx1,mon->dly1))
                  mon->PassEvent(e);
               break;
         }
      } while (!mon->terminated);
      HExitThread(0);
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}

// Start the monitor task running
void AMonitor::Start()
{
   HCreateMonitor(Monitor_Task,(void *)this);
}

// Start the monitor task running
void AMonitor::Terminate()
{
   terminated = TRUE;
}

// Find a component registered with Monitor
AComponentPtr AMonitor::FindComponent(const string& name)
{
   typedef list<AComponentPtr>::const_iterator LI;
   for (LI i = compList.begin(); i != compList.end(); ++i) {
      if ((*i)->cname == name) return *i;
   }
   return 0;
}


// ----------------------End of AMonitor.cpp ---------------------






