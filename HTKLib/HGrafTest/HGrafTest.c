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
/*        File: HGrafTest.c -    Test program for HGraf        */
/* ----------------------------------------------------------- */


char *hgraftest_version = "!HVER!HGrafTest: 1.6.0 [SJY 01/06/07]";

#include "HShell.h"
#include "HThreads.h"
#include "HMem.h"
#include "HMath.h"
#include "HSigP.h"
#include "HAudio.h"
#include "HWave.h"
#include "HLabel.h"
#include "HVQ.h"
#include "HParm.h"
#include "HGraf.h"

HWin MakeAWindow(char *name,int x, int y, int w, int h, int n)
{
  HWin win;
  int xx,yy;
  char buf[100];

  win = MakeHWin(name,x,y,w,h,1);

  strcpy(buf,"ATK-2: "); strcat(buf,name);
  xx = CentreX(win,w/2,buf);
  yy = CentreY(win,h/2,buf);
  HSetColour(win,RED+n);
  HDrawLine(win,0,0,w,10);
  HDrawLine(win,0,h-10,w,h);
  HPrintf(win,xx,yy,"%s",buf);
  return win;
}

int main(int argc, char *argv[])
{
  HWin w1,w2,w3;
  HEventRec e;

  InitThreads(HT_NOMONITOR);
  if(InitShell(argc,argv,hgraftest_version)<SUCCESS)
    HError(3200,"HGrafTest: InitShell failed");
  /* InitThreads(0); */
  InitMem();   InitLabel();
  InitMath();
  InitSigP(); InitWave();  InitAudio(); InitVQ();
  if(InitParm()<SUCCESS)
    HError(3200,"HGrafTest: InitParm failed");
  InitGraf(FALSE);

  w1 = MakeAWindow("w1",40,40,200,50,0);
  w2 = MakeAWindow("w2",140,140,200,50,1);
  w3 = MakeAWindow("w3",240,240,200,50,2);
  do {
    e = HGetEvent(NULL,NULL);
  } while (e.event != HMOUSEDOWN);
}


