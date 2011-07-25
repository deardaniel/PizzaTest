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
/*     File: TBase.cpp -     Test the basic ATK framework      */
/* ----------------------------------------------------------- */


static const char * version="!HVER!TBase: 1.6.0 [SJY 01/06/07]";

#include "AComponent.h"
#include "AMonitor.h"

//=============================================================
//  Example of a simple ATK Component: receives "balls" in
//  its input buffer and transmits "balls" to its out buffer
//=============================================================

// PART A: Derive a class from AComponent
// """"""""""""""""""""""""""""""""""""""

#define INBUFID 1

class ABalls: public AComponent {
public:
   ABalls(){}
   ABalls(const string & name, int initBalls, ABuffer *inb, ABuffer *outb);

private:
   friend TASKTYPE TASKMOD ABalls_Task(void *p);
   void DrawBallBox();
   void NewBallCmd();
   void PassBallCmd();


   void ExecCommand(const string & cmdname);
   int numBalls;        // num balls in container
   HWin win;            // display window
   int width,height;    // width and height of win
   int size;            // diameter of balls in display
   ABuffer *in;
   ABuffer *out;
};

// ABalls constructor
ABalls::ABalls(const string & name, int initBalls, ABuffer *inb, ABuffer *outb)
: AComponent(name,5)
{
   numBalls = initBalls;
   in = inb; out = outb;
   width = 100; height=100; size = 10;
}

// PART B: Define component commands & execution routine
//
void ABalls::DrawBallBox()
{
   int i,x,y;

   HSetColour(win,YELLOW);
   HFillRectangle(win,0,0,width,height);
   HSetColour(win,RED);
   x = 0; y = height - size;
   for (i=1; i<=numBalls; i++) {
      HFillArc(win,x,y,x+size,y+size,0,360);
      x = x+size;
      if (x >= width - 1){
         x=0; y -=size;
      }
   }
}

void ABalls::NewBallCmd()
{
   int i;

   if (GetIntArg(i,1,100)) {
      numBalls += i;
      printf("New %d balls: %d total\n",i,numBalls);
      DrawBallBox();
   }
}
void ABalls::PassBallCmd()
{
   int i;
   APacket p;

   if (numBalls>0 && GetIntArg(i,1,numBalls)) {
      for (int j=1; j<=i; j++) out->PutPacket(p);
      numBalls -= i;
      printf("Pass %d balls: %d left\n",i,numBalls);
      DrawBallBox();
   }
}

// Execution routine for above
void ABalls::ExecCommand(const string & cmdname)
{
   if (cmdname == "new")
      NewBallCmd();
   else if (cmdname == "pass")
      PassBallCmd();
   else
      printf("ExecCommand: unknown command %s\n",cmdname.c_str());
}

// PART C: Define the actual task
// """"""""""""""""""""""""""""""""""""""""""""""""""""
TASKTYPE TASKMOD ABalls_Task(void * p)
{
   ABalls *thisComp = (ABalls *)p;
   APacket pkt;
   int i;
   char cname[100];
   int offset = 100+((int)thisComp->cname[0] - (int)'a')*140;
   HEventRec e;

   strcpy(cname,thisComp->cname.c_str());
   thisComp->win = MakeHWin(cname,360,offset,100,100,1);
   thisComp->DrawBallBox();
   thisComp->in->RequestBufferEvents(INBUFID);
   thisComp->RequestMessageEvents();

   while (!thisComp->IsTerminated()){
      e = HGetEvent(0,0);
      switch(e.event){
      case HTBUFFER:
	switch(e.c){
	case MSGEVENTID:
	  thisComp->ChkMessage();
	  while (thisComp->IsSuspended()) thisComp->ChkMessage(TRUE);
	  break;
	case INBUFID:
	  if (! thisComp->in->IsEmpty()) {
	    i = 0;
	    while (! thisComp->in->IsEmpty()) {
	      pkt = thisComp->in->GetPacket(); ++i;
	      ++thisComp->numBalls;
	    }
	    printf("Received %d balls: %d total\n",i,thisComp->numBalls);
	    thisComp->DrawBallBox();
	    break;
	  }
	default:
	  printf("missed buffer event %d\n", e.c);
	}
	break;
      case HMOUSEDOWN:
	thisComp->SendMessage("pass(1)");
	break;
      case HWINCLOSE:
	printf("terminating\n");
	thisComp->Terminate();
	break;
      }
      thisComp->DrawBallBox();
   }
   HExitThread(0);
   return 0;
}

//=============================================================
//                      End of example Component
//=============================================================


//---------- Main: instantiate components and run ------------

int main(int argc, char *argv[])
{
   int bsize  = 4;
   int iballs = 4;

   if (InitHTK(argc,argv,version)<SUCCESS){
   //if (NCInitHTK("TBase.cfg",version)<SUCCESS){
      ReportErrors("Main",0); exit(-1);
   }
   printf("TBase: Basic Framework Test\n");

   // Create Buffers
   ABuffer ab("ab",bsize);
   ABuffer bc("bc",bsize);
   ABuffer ca("ca",bsize);

   // Create Components
   ABalls a("a",iballs,&ca,&ab);
   ABalls b("b",iballs,&ab,&bc);
   ABalls c("c",iballs,&bc,&ca);

   // Create Monitor and Start it
   AMonitor amon;
   amon.AddComponent(&a);
   amon.AddComponent(&b);
   amon.AddComponent(&c);
   amon.Start();

   // Start components executing

   printf("Starting tasks\n"); fflush(stdout);
   a.Start(HPRIO_NORM,ABalls_Task);
   b.Start(HPRIO_NORM,ABalls_Task);
   c.Start(HPRIO_NORM,ABalls_Task);

   // Shutdown
   printf("Waiting for balls to terminate\n");
   a.Join(); printf("join a\n"); fflush(stdout);b.Join(); printf("join b\n");fflush(stdout);c.Join();printf("join c\n");fflush(stdout);
   printf("Waiting for monitor\n");fflush(stdout);
   HJoinMonitor();
   printf("Exiting\n");
   return 0;
}

