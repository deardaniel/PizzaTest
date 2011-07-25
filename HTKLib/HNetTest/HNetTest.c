/* ----------------------------------------------------------- */
/*           _ ___   	     ___                               */
/*          |_| | |_/	  |_| | |_/    SPEECH                  */
/*          | | | | \  +  | | | | \    RECOGNITION             */
/*          =========	  =========    SOFTWARE                */
/*                                                             */
/* ================> ATK COMPATIBLE VERSION <================= */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                             */
/*      Machine Intelligence Laboratory (Speech Group)         */
/*      Cambridge University Engineering Department            */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/*      Entropic Cambridge Research Laboratory                 */
/*      (now part of Microsoft)                                */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright: Microsoft Corporation                    */
/*          1995-2000 Redmond, Washington USA                  */
/*                    http://www.microsoft.com                 */
/*                                                             */
/*          2001-2007 Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*    File: HNetTest.c -    Test program for  HNet display     */
/* ----------------------------------------------------------- */

char *hnettest_version = "!HVER!HNetTest: 1.6.0 [SJY 01/06/07]";

#include "HShell.h"
#include "HThreads.h"
#include "HMem.h"
#include "HMath.h"
#include "HSigP.h"
#include "HWave.h"
#include "HAudio.h"
#include "HParm.h"
#include "HLabel.h"
#include "HGraf.h"
#include "HModel.h"
#include "HUtil.h"
#include "HTrain.h"
#include "HAdapt.h"
#include "HFB.h"
#include "HDict.h"
#include "HNet.h"

static char *dictFn;              /* Dictionary */
static char *wdNetFn;             /* Word level lattice */
static char *hmmListFn;           /* HMMs */

static HMMSet hset;               /* the HMM set */
static Vocab vocab;               /* the dictionary */
static Lattice *wdNet;            /* the word level recognition network */
static Network *net;              /* the actual phone level network */
static int width = 600;				 /* display size */
static int depth = 400;

static MemHeap modelHeap;
static MemHeap netHeap;
static MemHeap wdNetHeap;
void ReportUsage(void)
{
   printf("\nUSAGE: HNetTest [options] DictFile NetFile HMMList\n\n");
   PrintStdOpts("H");
   printf("\n\n");
   exit(0);
}

void LoadNetwork()
{
   FILE *nf;
   Boolean isPipe;
   int n=0;

   CreateHeap(&wdNetHeap,"Lattice heap",MSTAK,1,0.0,4000,4000);
   if ( (nf = FOpen(wdNetFn,NetFilter,&isPipe)) == NULL)
      HError(3210,"LoadNetwork: Cannot open Word Net file %s",wdNetFn);
   if((wdNet = ReadLattice(nf,&wdNetHeap,&vocab,TRUE,FALSE))==NULL)
      HError(3210,"LoadNetwork: ReadLattice failed");
   FClose(nf,isPipe);

   printf("Read Word Network with %d nodes / %d arcs\n",wdNet->nn,wdNet->na);

   CreateHeap(&netHeap,"Net heap",MSTAK,1,0,
      wdNet->na*sizeof(NetLink),wdNet->na*sizeof(NetLink));

   net = ExpandWordNet(&netHeap,wdNet,&vocab,&hset);
   printf("Created network with %d nodes / %d links\n",
      net->numNode,net->numLink);

}

void Initialise(int argc, char *argv[])
{
   InitThreads(HT_MSGMON);
	if(InitShell(argc,argv,hnettest_version)<SUCCESS)
		HError(999,"Initialise: couldnt init HShell");
	InitMem();   InitLabel();
	InitMath();  InitSigP();
	InitWave();  InitAudio();
	if(InitParm()<SUCCESS)
		HError(999,"Initialise: couldnt init HParm");
	InitGraf(FALSE);  InitModel();
	InitDict();  InitNet();
	EnableBTrees();   /* allows unseen triphones to be synthesised */
	if (!InfoPrinted() && NumArgs() == 0)
		ReportUsage();
}

int main(int argc, char *argv[])
{
	char *s;

   Initialise(argc,argv);

	CreateHeap(&modelHeap, "Model heap",  MSTAK, 1, 0.0, 100000, 800000 );
   CreateHMMSet(&hset,&modelHeap,TRUE);

   while (NextArg() == SWITCHARG) {
      s = GetSwtArg();
      if (strlen(s)!=1)
         HError(3219,"HNetTest: Bad switch %s; must be single letter",s);
      switch(s[0]){

      case 'H':
         if (NextArg() != STRINGARG)
            HError(3219,"HNetTest: MMF File name expected");
         AddMMF(&hset,GetStrArg());
         break;
		default:
         HError(3219,"HNetTest: Unknown switch %s",s);
      }
   }

	if (NextArg()!=STRINGARG)
      HError(999,"HNetTest: Dictionary file name expected");
   dictFn = GetStrArg();
	if (NextArg()!=STRINGARG)
      HError(999,"HNetTest: Word Net file name expected");
   wdNetFn = GetStrArg();
   if (NextArg()!=STRINGARG)
      HError(999,"HNetTest: HMM list  file name expected");
   hmmListFn = GetStrArg();

	if(MakeHMMSet(&hset,hmmListFn)<SUCCESS)
      HError(999,"HNetTest: MakeHMMSet failed");
   if(LoadHMMSet(&hset,NULL,NULL)<SUCCESS)
      HError(999,"HNetTest: LoadHMMSet failed");

   InitVocab(&vocab);
   if(ReadDict(dictFn,&vocab)<SUCCESS)
      HError(3213, "HNetTest: ReadDict failed");

	LoadNetwork();

}


