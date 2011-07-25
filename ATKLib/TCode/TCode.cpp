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
/*      File: TACode.cpp -     Test the enCoder component      */
/* ----------------------------------------------------------- */


static const char * version="!HVER!TCode: 1.6.0 [SJY 01/06/07]";


#include "AMonitor.h"
#include "ASource.h"
#include "ACode.h"

void ShowTime(APacket p)
{
   printf("[%7.3f -> %7.3f]\n ",p.GetStartTime()/1e7,p.GetEndTime()/1e7);
}


int main(int argc, char *argv[])
{
   APacket p;
   AStringData *sd;
   AObsData *od;

   try {
      if (InitHTK(argc,argv,version)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      printf("TACode: Coder Test\n");
      printf("TACode: HTK initialised\n");
      // Create Buffers
      ABuffer auChan("auChan");
      ABuffer feChan("feChan");
      printf("TACode: Buffers initialised\n");

      // Create Audio Source and Coder
      ASource ain("AIn",&auChan);
      ACode acode("ACode",&auChan,&feChan);
      printf("TACode: Components initialised\n");

      // Create Monitor and Start it
      AMonitor amon;
      amon.AddComponent(&ain);
      amon.AddComponent(&acode);
      amon.Start();

      // Start components executing
      Boolean rxing;
      int i;
      printf("Starting Coder Test\n");
      ain.Start(); acode.Start();
      Boolean terminated = FALSE;
      while (terminated){
         i = 0;
         p = feChan.GetPacket();
         ShowTime(p);
         if (p.GetKind() != StringPacket){
            HError(0,"String packet expected\n");
         }
         sd = (AStringData *) p.GetData();
         printf("MARKER: %s\n",sd->data.c_str());
         rxing = TRUE;
         while (!ain.IsTerminated() && rxing){
            p = feChan.GetPacket();
            // ShowTime(p);
            switch (p.GetKind()){
            case  StringPacket:
               sd = (AStringData *) p.GetData();
               printf("MARKER: %s\n",sd->data.c_str());
               if (sd->data.find("ENDOFLIST") != string::npos ||
                   sd->data.find("TERMINATED")!= string::npos
                  ){
                  ain.SendMessage("terminate()");
                  acode.SendMessage("terminate()");
                  terminated = TRUE;
               }
               rxing = FALSE;
               break;
            case ObservationPacket:
               od = (AObsData *)p.GetData();
               // PrintObservation(i,&(od->data),13);printf("\n"); ++i;
               break;
            default:
               printf("*** Unexpected packet\n");
            }
         }
         printf("Source stopped\n");
      }
      // Shutdown
      printf("Waiting for sys components\n");fflush(stdout);
      ain.Join();
      acode.Join();
      printf("Waiting for monitor\n");fflush(stdout);
      amon.Terminate();
      HJoinMonitor();
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); }
   catch (HTK_Error e){ ReportErrors("HTK",e.i); }
   return 0;
}


