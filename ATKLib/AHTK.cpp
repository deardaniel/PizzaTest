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
/*     File: AHTK.cpp -    Interface to the HTK Libraries      */
/* ----------------------------------------------------------- */

static const char * ahtk_version="!HVER!AHTK: 1.6.0 [SJY 01/06/07]";


// Modification history:
//   9/12/02 - added NCInitHTK and ReportErrors

#include "AHTK.h"

static Boolean hasConsole = TRUE;
Boolean HasRealConsole(){return hasConsole; }

extern char * abuffer_version;
extern char * acode_version;
extern char * acomponent_version;
extern char * adict_version;
extern char * agram_version;
extern char * angram_version;
extern char * ahmms_version;
extern char * amonitor_version;
extern char * apacket_version;
extern char * arec_version;
extern char * aresource_version;
extern char * arman_version;
extern char * asource_version;


ReturnStatus CommonInit(Boolean noGraphics) {
   InitMem();   InitLabel();
   InitMath();  InitSigP(); InitUtil();
   InitWave();  InitAudio(); InitModel();
   if(InitParm()<SUCCESS) return FAIL;
   InitGraf(noGraphics);
   InitDict();  InitNet();   InitLM();
   InitRec();  InitLat();  /*InitAdapt();*/
   EnableBTrees();   /* allows unseen triphones to be synthesised */
   Register(abuffer_version);
   Register(acode_version);
   Register(acomponent_version);
   Register(adict_version);
   Register(agram_version);
   Register(angram_version);
   Register(ahmms_version);
   Register(ahtk_version);
   Register(amonitor_version);
   Register(apacket_version);
   Register(arec_version);
   Register(aresource_version);
   Register(arman_version);
   Register(asource_version);
   InfoPrinted();
   return SUCCESS;
}

ReturnStatus InitHTK(int argc, char *argv[],const char * app_version, Boolean noGraphics)
{
   InitThreads(HT_MSGMON);   // enable msg driven monitoring
   if(InitShell(argc,argv,app_version)<SUCCESS) return FAIL;
   return CommonInit(noGraphics);
}

ReturnStatus NCInitHTK(char *configFile, const char * app_version, Boolean noGraphics)
{
   const int argc1 = 3;
   char* argv1[argc1] = {
      "ATK_Application",
         "-C",
         configFile
   };
   InitThreads(HT_MSGMON);   // enable msg driven monitoring
   if(InitShell(argc1,argv1,app_version)<SUCCESS) return FAIL;
   SetNoConsoleMode(); hasConsole = FALSE;
   return CommonInit(noGraphics);
}

void ReportErrors(char *library, int errnum)
{
   int n = HRErrorCount();
   printf("\n%s Error %d\n",library,errnum);
   for (int i=1; i<=n; i++){
      printf("  %d. %s\n",i,HRErrorGetMess(i));
   }
   if (! HasConsole())
      HPauseThread(30000);
   exit(0);
}

char * UCase(const char *s, char *buf)
{
   int len = strlen(s);
   for (int i=0; i<len; i++){
      buf[i] = toupper(s[i]);
   }
   buf[len] = '\0';
   return buf;
}

