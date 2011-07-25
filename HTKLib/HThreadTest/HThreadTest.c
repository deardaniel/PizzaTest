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
/*     File: HThreadTest.c -    Test program for  HThreads     */
/* ----------------------------------------------------------- */


char *hthreadtest_version = "!HVER!HThreadTest: 1.6.0 [SJY 01/06/07]";

#include <stdio.h>
#include <math.h>
#include "HShell.h"
#include "HThreads.h"
#include "HMem.h"
#include "HMath.h"
#include "HSigP.h"
#include "HAudio.h"
#include "HWave.h"
#include "HVQ.h"
#include "HParm.h"
#include "HLabel.h"
#include "HGraf.h"

static int sMon = HT_NOMONITOR; /* HT_MSGMON */

void ReportUsage(void)
{
   printf("\nUSAGE: HThreadTest n .... \n");
   printf("   1.  ParallelForkAndJoin(numThreads,delay)\n");
   printf("   2.  SimpleMutex(numThreads,lockon)\n");
   printf("   3.  SimpleSignal()\n");
   printf("   4.  BufferTest(nChars,bSize,pDelay,cDelay,pcPrioStr)\n");
   exit(1);
}

HWin MakeAWindow(char *name,int x, int y, int w, int h, int n)
{
	HWin win;
	int xx,yy;
	char buf[100];

	win = MakeHWin(name,x,y,w,h,1);

	strcpy(buf,"Window: "); strcat(buf,name);
	xx = CentreX(win,w/2,buf);
	yy = CentreY(win,h/2,buf);
	HSetColour(win,RED+n);
	HDrawLine(win,0,0,w,h);
	HPrintf(win,xx,yy,"%s",buf);
	return win;
}

TASKTYPE TASKMOD tmon4(void * nv)
{
	HEventRec e;
	HWin w;

	printf("WinMsg Monitor task running\n"); fflush(stdout);
	w = MakeAWindow("monitor",40,40,200,50,1);
	do {
		e = HGetEvent(NULL,NULL);
		printf("event %d\n",e.event);
	} while (e.event != HMOUSEDOWN);
	return 0;
}

/* ------------- Test 1 -  Fork and Join ------------- */

static int test1_delay;

TASKTYPE TASKMOD test1(void * nv)
{
	int i,n;
	n = (int)nv;
	for (i=0; i<n; i++){
		if (test1_delay) HPauseThread(test1_delay);
		printf("Test1 Thread %d\n",n); fflush(stdout);
	}
	HExitThread(n);
	return 0;
}

void ParallelForkAndJoin(int numThreads,int delay)
{
	int i,j;
	HThread t[1000];
	int status[1000];
	char name[100];

	if (numThreads <1) numThreads=1;
	if (numThreads>1000) {
		printf("too many threads\n"); exit(1);
	}
	if (delay<0) delay = 0; test1_delay = delay;
	printf(" creating %d threads\n",numThreads); fflush(stdout);
	for (i=0; i<numThreads; i++){
		j = i+1;
		sprintf(name,"thread%d",j);
		t[i] = HCreateThread(name,10,HPRIO_NORM,test1,(void *)j);
		printf("   %d created\n",j);
	}
	printf(" waiting for termination\n");
	for (i=0; i<numThreads; i++){
		HJoinThread(t[i],&status[i]);
		printf("   %d terminated with status %d\n",i+1,status[i]);
		fflush(stdout);
	}
}

/* ------------- Test 2 -  Simple Mutex ------------- */

static HLock t2_lock;
static float t2_sum = 0;
static float t2_inc;
static int t2_lockon;

TASKTYPE TASKMOD test2(void * hp)
{
	int i; float x;
	HPriority p;
	FILE *f;

	p = (HPriority)hp;
	if (t2_lockon) HEnterSection(t2_lock);
	x = t2_sum;                        /* Load shared variable */
	f = fopen("Readme.txt","r");       /* waste a bit of time on i/o */
	if (f==NULL)
		printf("Cant open ../HThreadTest.c\n");
	else {
		while ((i=fgetc(f)) != '\n') putchar(i);
		printf("\n"); fflush(stdout);
		fclose(f);
	}
	x = x + t2_inc;
	t2_sum = x;                   /* update shared variable */
	if (t2_lockon)   HLeaveSection(t2_lock);
	HExitThread(0);
	return 0;
}

void SimpleMutex(int numThreads, int lockon)
{
	int i,j;
	HThread t[1000];
	int status[1000];
	char name[100];
	HPriority prio;

	if (numThreads <1) numThreads=1;
	if (numThreads>1000) {
		printf("too many threads\n"); exit(1);
	}
	t2_lockon = lockon;
	printf(" creating %d threads\n",numThreads); fflush(stdout);
	t2_lock = HCreateLock("t2_lock");
	t2_inc = (float)1.0 / numThreads;
	for (i=0; i<numThreads; i++){
		j = i+1;
		sprintf(name,"thread%d",j);
		prio = i%3;
		t[i] = HCreateThread(name,10,prio,test2,(void *)prio);
		printf("   %d created\n",j);
	}
	printf(" waiting for termination\n");
	for (i=0; i<numThreads; i++)
		HJoinThread(t[i],&status[i]);
	printf("Final sum = %f (should be 1)\n",t2_sum);
}

/* ------------- Test 3 -  Simple Signal ------------- */

static HLock t3_lock;
static HSignal t3_signal;

TASKTYPE TASKMOD test3(void * hp)
{
	HEnterSection(t3_lock);
	printf("Test 3 - waiting for Signal\n"); fflush(stdout);
	HWaitSignal(t3_signal,t3_lock);
	printf("Test 3 - Signal Received\n"); fflush(stdout);
	HLeaveSection(t3_lock);
	HExitThread(0);
	return 0;
}

void SimpleSignal(void)
{
	HThread t;
	int status;

	t3_lock = HCreateLock("t3_lock");
	t3_signal = HCreateSignal("t3_signal");
	t = HCreateThread("t3wait",10,HPRIO_NORM,test3,(void *)0);
	printf(" press key to send signal\n"); getchar();
	HSendSignal(t3_signal);
	HJoinThread(t,&status);
}

/* ------------- Test 4 -  Buffer Test  ------------- */

HThread prod_th, cons_th, mon_th;
char buf[10000];
int inx,outx,used,bsize;
HLock lock;
HSignal notfull, notempty;
int pdel,cdel;

HPriority getpr(char c)
{
	switch(c){
	case 'l': return HPRIO_LOW;
	case 'n': return HPRIO_NORM;
	case 'h': return HPRIO_HIGH;
	default: printf("bad priority char\n"); exit(1);
	}
}
void bput(char c)
{
	HPauseThread(pdel);
	printf("<%c",c); fflush(stdout);
	HEnterSection(lock);
	while (used>=bsize) {
		printf("!");
		HWaitSignal(notfull,lock);
	}
	buf[inx] = c; inx++; used++;
	if (inx == bsize) inx = 0;
	HSendSignal(notempty);
	HLeaveSection(lock);
	printf(">"); fflush(stdout);
}

char bget()
{
	int c;
	HPauseThread(cdel); printf("{");   fflush(stdout);
	HEnterSection(lock);
	while (used==0) {
		printf("?");
		HWaitSignal(notempty,lock);
	}
	c = buf[outx]; outx++; --used;
	if (outx == bsize) outx = 0;
	HSendSignal(notfull);
	HLeaveSection(lock);
	printf("%c}",c); fflush(stdout);
	return c;
}
TASKTYPE TASKMOD producer(void * n)
{
   int i,nn; char c;
   nn = (int)n;
   printf(" [[Producer started (n=%d)] ",nn);
   i=0;
   for (i=0,c='A'; c<='Z' && i<nn; c++,i++){
      bput(c);
   }
   bput(' '); ++i;
   HExitThread(i);
   return 0;
}

TASKTYPE TASKMOD consumer(void * n){
	int i,j; char c;

	printf(" [[Consumer started]] ");
	i=0; j=0; c = 'A';
	while (c != ' '  && j<500) {
		++i; ++j;
		c = bget();
		if (i==50){
			printf("\n"); i=1;
		}
	}
	HExitThread(j);
	return 0;
}

void BufferTest(int nChars,int bSize,int pDelay,int cDelay,char *pcPrioStr)
{
	HPriority pp,cp;
	int n = nChars;
	int pstat,cstat;

	printf("Starting Buffer Test\n\n");
	lock = HCreateLock("Buffer");
	notfull =  HCreateSignal("NotFull");
	notempty = HCreateSignal("NotEmpty");
	inx = 0; outx=0; used=0; bsize = bSize;
	pp = getpr(pcPrioStr[0]);
	cp = getpr(pcPrioStr[1]);
	pdel = pDelay; cdel = cDelay;
	prod_th = HCreateThread("Producer",10, pp, producer, (void *)n);
	cons_th = HCreateThread("Consumer",10, cp, consumer, (void *)n);
	HJoinThread(prod_th,&pstat);
	HJoinThread(cons_th,&cstat);
	printf("\n\nProducer terminated[%d chars sent]\n",pstat);
	printf("Consumer terminated[%d chars received]\n",cstat);
}


/* ---------------------- End of Tests --------------------------- */

int main(int argc, char *argv[])
{
	int n,a1,a2,a3,a4;

	InitThreads(sMon);
	if(InitShell(argc,argv,hthreadtest_version)<SUCCESS)
		HError(1100,"HThreadTest: InitShell failed");
	if (NumArgs() < 1) ReportUsage();
	InitMem();   InitLabel();
	InitMath();  InitSigP();
	InitWave();  InitAudio();
	InitVQ();
	if(InitParm()<SUCCESS)
		HError(3200,"HThreadTest: InitParm failed");
	InitGraf(FALSE);
   if (sMon==HT_MSGMON) HCreateMonitor(tmon4,(void *)0);
	if (NextArg() == INTARG){
		n = GetIntArg();
		switch(n){
		case 1:
			a1 = GetIntArg(); a2 = GetIntArg();
			ParallelForkAndJoin(a1,a2); break;
		case 2:
			a1 = GetIntArg(); a2 = GetIntArg();
			SimpleMutex(a1,a2); break;
		case 3:
			SimpleSignal(); break;
		case 4:
			a1 = GetIntArg(); a2 = GetIntArg();
			a3 = GetIntArg(); a4 = GetIntArg();
			BufferTest(a1,a2,a3,a4,GetStrArg()); break;
		default:
			printf("Bad test number %d\n",n); ReportUsage();
		}
	}
	if (sMon>0){
		AccessStatus();
		PrintThreadStatus("Final");
		ReleaseStatusAccess();
		if (sMon==HT_MSGMON)HJoinMonitor();
	}
}
