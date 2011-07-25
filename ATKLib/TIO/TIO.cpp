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
/*   File: TIO.cpp  Test the AIO High Level Interface          */
/* ----------------------------------------------------------- */

static const char * version="!HVER!TIO: 1.6.0 [SJY 01/06/07]";

#include "AMonitor.h"
#include "AIO.h"

static AIO *aio;               // the AIO subsystem
static ABuffer *inChan;        // input channel from AIO

#define inBufID 10

enum AppEvent {
   TimeOut, Number, NumEnd, GoodBye, SynDone, Terminated, Unknown
};

static char *ev[] = {"TimeOut", "Number", "NumEnd", "GoodBye",
                     "SynDone", "Terminated", "Unknown" };

AppEvent GetAppEvent(APacket p)
{
   if (p.GetKind() == CommandPacket) {
      ACommandData * cd = (ACommandData *) p.GetData();
      if (cd->GetCommand() == "asrTimeOut")
         return TimeOut;
      if (cd->GetCommand() == "terminated")
         return Terminated;
      else if (cd->GetCommand() == "synFinished")
         return SynDone;
   }
   if (p.GetKind() == PhrasePacket) {
      APhraseData * ap = (APhraseData *) p.GetData();
      if (ap->ptype == End_PT)
         return NumEnd;
      else if (ap->ptype == Word_PT){
         if (ap->tag == "bye")
            return GoodBye;
         else if (ap->tag != "")
            return Number;
         else
            return Unknown;
      }else
         return Unknown;
   }else
      return Unknown;
}

int GetAppNumber(APacket p)
{
   APhraseData * ap = (APhraseData *) p.GetData();
   return atoi(ap->tag.c_str());
}

string NumString(int *num, int i)
{
   char buf[1000],n[100];
   if (i==0){
      sprintf(buf,"zero");
   }else {
      buf[0] = '\0';
      for (int j=0; j<i; j++){
         sprintf(n,"%d ",num[j]);
         strcat(buf,n);
      }
   }
   printf(" Recognised ==> %s\n",buf);
   return string(buf);
}

int main(int argc, char *argv[])
{
   HEventRec e;
   try {
      if (InitHTK(argc,argv,version)<SUCCESS){
         // if (NCInitHTK("TIO.cfg",version)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      printf("\n      TIO: AIO Interface Test\n");
      printf("\nWARNING - this test uses an open microphone.\n");
      printf("Use headphones to ensure that speech output\n");
      printf("is not picked up by the microphone.\n\n");
      printf("     Press Return to Continue\n\n");
      getchar();

      // Create Buffers
      inChan = new ABuffer("inChan");
      // inChan->RequestBufferEvents(inBufID);

      // Create IO interface
      aio = new AIO("aio",inChan);

      // Define filler words
      aio->DefineFiller("ER");
      aio->DefineFiller("SIL");
      aio->DefineFiller("OH");

      // Create Monitor and Start it
      AMonitor amon;
      aio->AttachMonitor(&amon);
      amon.Start();

      // Start the AIO subsystem
      aio->Start();

      printf("started ...\n");
      Boolean active = TRUE;
      Boolean collecting = FALSE;
      APacket p; AppEvent e;
      int num[100];
      int i = 0;
      do {
         aio->SendMessage("ask(\"What number do you wish to dial\")");
         if (!aio->IsTerminated()) do {
            p = inChan->GetPacket();
            p.Show();
            e = GetAppEvent(p); collecting = TRUE;
            // printf("Event = %s\n",ev[e]);
            switch (e){
            case TimeOut:
               aio->SendMessage("tell(\"Sorry I cant hear you!\")");
               i = 0; collecting = FALSE;
               break;
            case Terminated:
               active = FALSE;
               break;
            case GoodBye:
               aio->SendMessage("tell(\"Thank you for using the service.\")");
               active = FALSE; collecting = FALSE;
               break;
            case Number:
               num[i++] = GetAppNumber(p);
               break;
            case NumEnd:
               string s = NumString(num,i);
               if (i<6){
                  aio->SendMessage("tell(\"Number "+s+" is too short!\")");
               }else{
                  aio->SendMessage("tell(\"Dialling "+s+".\")");
               }
               i = 0; collecting = FALSE;
               break;
            }
         } while (!aio->IsTerminated() && collecting);
         if (active)
            do {
               p = inChan->GetPacket();
            }while ( GetAppEvent(p) != SynDone);
      }while(active);
      // Shutdown
      printf("Shutting down TIO\n");
      aio->SendMessage("closedown()");
      aio->SendMessage("terminate()");
      aio->Join();
      printf("Waiting for aio to terminate\n");
      amon.Terminate();
      printf("Waiting for monitor to terminate\n");
      HJoinMonitor();
      printf("Exiting\n");
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); }
   catch (HTK_Error e){ ReportErrors("HTK",e.i); }
   return 0;
}
