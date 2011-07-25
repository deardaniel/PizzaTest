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
/*     File: TRec.cpp -     Test the Recogniser component      */
/* ----------------------------------------------------------- */

//  This program demonstrate very simple use of the ATK recogniser.
//  All resources are read from a configuration script.  The supplied
//  config script defines a simple digit dialler recogniser.  Once
//  loaded you can say "DIAL ONE TWO THREE" etc.   This demo runs
//  with a simple grammar network.  You can also run the demo
//  with an ngram language model and a simple word loop network.
//  To do this, use the config var NGRAMFILE to define the name
//  of the ngram language model.   Note that the ANGram resource
//  is only created if NGRAMFILE is defined.

static const char * version="!HVER!TRec: 1.6.0 [SJY 01/06/07]";


#include "AMonitor.h"
#include "ASource.h"
#include "ACode.h"
#include "ARec.h"

#define WITHMON

void ShowTime(APacket p)
{
   printf("[%7.3f -> %7.3f]\n ",p.GetStartTime()/1e7,p.GetEndTime()/1e7);
}


int main(int argc, char *argv[])
{
   APacket p;

   try {

      // if (NCInitHTK("TRec.cfg",version)<SUCCESS){
      if (InitHTK(argc,argv,version)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      printf("TRec: Basic Recogniser Test\n");
      ConfParam *cParm[MAXGLOBS];       /* config parameters */
      int numParm,i;
      char ngramFN[100],buf[100];
      ngramFN[0] = '\0';
      // Read configuration parms for ANGRAM to see if NGram used
      numParm = GetConfig("ANGRAM", TRUE, cParm, MAXGLOBS);
      if (numParm>0){
         if (GetConfStr(cParm,numParm,"NGRAMFILE",buf)) strcpy(ngramFN,buf);
      }
      printf("TRec: HTK initialised: %s\n",ngramFN);
      // Create Buffers
      ABuffer auChan("auChan");
      ABuffer feChan("feChan");
      ABuffer ansChan("ansChan");
      printf("TRec: Buffers initialised\n");
      // create a resource manager
      ARMan rman;

      // Create Audio Source and Coder
      ASource ain("AIn",&auChan);
      ACode acode("ACode",&auChan,&feChan);
      ARec  arec("ARec",&feChan,&ansChan,&rman,0);
      printf("TRec: Components initialised\n");

      // create global resources
      AHmms hset("HmmSet"); // load info in config
      ADict dict("ADict");
      AGram gram("AGram");
      rman.StoreHMMs(&hset);
      rman.StoreDict(&dict);
      rman.StoreGram(&gram);

      ResourceGroup *main = rman.NewGroup("main");
      main->AddHMMs(&hset);
      main->AddDict(&dict);
      main->AddGram(&gram);

      if (strlen(ngramFN)>0){
         ANGram * ngram  = new ANGram("ANGram");
         rman.StoreNGram(ngram);
         main->AddNGram(ngram);
      }

#ifdef WITHMON
      // Create Monitor and Start it
      AMonitor amon;
      amon.AddComponent(&ain);
      amon.AddComponent(&acode);
      amon.AddComponent(&arec);
      amon.Start();
#endif

      // Start components executing
      ain.Start();
      acode.Start();
      arec.Start();
      arec.SendMessage("usegrp(main)");
      arec.SendMessage("start()");

      Boolean terminated = FALSE;
      while (!terminated) {
         APacket p = ansChan.GetPacket();
         if (p.GetKind() == StringPacket){
            AStringData * sd = (AStringData *)p.GetData();
            if (sd->data.find("TERMINATED") != string::npos) {
               terminated = TRUE;
            }
         }
         p.Show();
      }
      // Shutdown
      printf("Waiting for ain\n");
      ain.Join();
      printf("Waiting for acode\n");
      acode.Join();
      printf("Waiting for arec\n");
      arec.Join();
#ifdef WITHMON
      printf("Waiting for monitor\n");
      amon.Terminate();
      HJoinMonitor();
#endif
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}
