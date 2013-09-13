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
/*    File: ATee.cpp -   Basic plumbing                        */
/* ----------------------------------------------------------- */

char * atee_version="!HVER!ATee: 1.6.0 [SJY 01/06/07]";

//Tee component 1-2 plumbing

#include "ATee.h"
#define INBUFID 1
// ABiFe constructor
ATee::ATee(const string &name, ABuffer *inb, ABuffer *outb1, ABuffer *outb2)
: AComponent(name,2)
{
  in=inb;
  out1=outb1;
  out2=outb2;
}

TASKTYPE TASKMOD CopyFe(void *p)
{
  ATee *acp = (ATee *)p;
  APacket pkt;
  HEventRec e;
  try{
    acp->in->RequestBufferEvents(INBUFID);
    acp->RequestMessageEvents();

    while(!acp->IsTerminated()){
      e = HGetEvent(0,0);
      switch(e.event){
      case HTBUFFER:
      	switch(e.c) {
	case MSGEVENTID:
	  acp->ChkMessage();
	  while (acp->IsSuspended()) acp->ChkMessage(TRUE);
	  break;
	case INBUFID:
	  if (! acp->in->IsEmpty()) {
	    while (! acp->in->IsEmpty()) {
	      pkt=acp->in->GetPacket();
	      acp->out1->PutPacket(pkt);
	      acp->out2->PutPacket(pkt);
	    }
	  }
	  break;
	}
      }
    }
    HExitThread(0);
    return 0;
  }
  catch (ATK_Error e){ReportErrors("ATK",e.i); return 0;}
  catch (HTK_Error e){ReportErrors("HTK",e.i); return 0;}
}

// Start the task
void ATee::Start(HPriority priority)
{
   AComponent::Start(priority,CopyFe);
}

// Implement the command interface
void ATee::ExecCommand(const string & cmdname)
{
  return;
}

