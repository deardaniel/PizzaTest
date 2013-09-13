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
/*        File: HMathTest.c -    Test program for HMath        */
/* ----------------------------------------------------------- */


char *hmathtest_version = "!HVER!HMathTest: 1.6.0 [SJY 01/06/07]";

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


int main(int argc, char *argv[])
{
   int i,j,k,n;
   float sum,rx,r0 = 1.0;
   Vector x,y,r;
   Matrix rm,rmi;

   InitThreads(HT_NOMONITOR);
   if(InitShell(argc,argv,hmathtest_version)<SUCCESS)
      HError(3200,"HMathTest: InitShell failed");
   InitMem();   InitLabel();
   InitMath();
   InitSigP(); InitWave();  InitAudio(); InitVQ();
   if(InitParm()<SUCCESS)
      HError(3200,"HMathTest: InitParm failed");
   InitGraf(FALSE);

   n = GetIntArg();

   x = CreateVector(&gstack,n);
   y = CreateVector(&gstack,n);
   r = CreateVector(&gstack,2*n-1);

   r[n] = r0;
   for (i=1; i<n; i++) {
      rx = r0/(i+1);
      r[n-i] = r[n+i] = rx;
   }
   for (i=1; i<=n; i++) y[i] = RandomValue();

   ShowVector("Vector r",r,10);
   ShowVector("Vector y",y,10);
   if (n<30) {
      rm = CreateMatrix(&gstack,n,n);
      rmi = CreateMatrix(&gstack,n,n);
      for (i=1; i<=n; i++) {
         for (j=n,k=i; j>=1; j--,k++)
            rm[i][j] = r[k];
      }
      printf("Direct soln by matrix inversion\n");
      ShowMatrix("Matrix R",rm,10,10);
      MatInvert(rm,rmi);

      for (i=1; i<=n; i++) {
         sum = 0.0;
         for (j=1; j<=n; j++) sum += rmi[i][j]*y[j];
         x[i] = sum;
      }
      ShowVector("Direct x",x,10);
   }
   printf("Solution using Toeplitz\n");
   i = Toeplitz(r,x,y,n);
   printf("%d of %d returned\n",i,n);
   ShowVector("Toeplitz x",x,10);

}


