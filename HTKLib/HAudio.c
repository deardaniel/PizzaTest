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
/*           File: HAudio.c -    Audio Input/Output            */
/* ----------------------------------------------------------- */

char *haudio_version = "!HVER!HAudio: 1.6.0 [SJY 01/06/07]";

#include "HShell.h"        /* HTK Libraries */
#include "HMem.h"
#include "HMath.h"
#include "HWave.h"
#include "HAudio.h"
#include "HThreads.h"
#include "HGraf.h"

/* ----------------------------- Trace Flags ------------------------- */

static int trace = 0;
#define T_TOP  0001     /* Top Level tracing */
#define T_AUD  0002     /* Trace audio input */
#define T_OUT  0004     /* Trace audio output */
#define T_ECH  0010     /* Trace echo cancellation */
#define T_DMP  0020     /* dump both channels of echo buffer to file */
#define T_SNR  0040     /* output energy in each input block */

/* -------------------- Configuration Parameters --------------------- */

static ConfParam *cParm[MAXGLOBS];       /* config parameters */
static int numParm = 0;
static FILE *dumpf;
static Boolean echoCancelling = FALSE;   /* set true to enable echo cancelling */
static int echoFilterSize = 512;         /* num filter taps 0 .. N-1 */
static int echoMaxDelay   = 1000;        /* max delay in samples */
static int echoFilterAWin =  8;          /* num blocks in filter analysis window */
static float blockEnergy;                /* used by T_SNR option */
static long blockSamples;

/* ---------------------------------------------------------- */

#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#endif

#ifdef UNIX
#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#else
#include <alsa/asoundlib.h>
#endif
#endif

typedef enum { ADS_INIT, ADS_OPEN, ADS_SAMPLING,
               ADS_STOPPED, ADS_CLOSED } AudioDevStatus;

#ifdef WIN32
#define MMAPI_BUFFER_COUNT 5

typedef struct mmapibuf{
   int n;                 /* Number of valid samples in buffer */
   Boolean isLast;        /* used on output to signal end of utterance */
   LPWAVEHDR waveHdr;     /* Pointer to WAVEHDR */
   LPSTR waveData;        /* Data in buffer */
   struct mmapibuf *next;
} mmApiBuf;
#endif
#ifdef UNIX
#ifdef __APPLE__
#define AUDIO_QUEUE_BUFFER_COUNT 5
typedef struct coreaudiobuf {
	int n;
	Boolean isLast;
	AudioQueueBufferRef audioQueueBuffer;
	struct coreaudiobuf *next;
} coreAudioBuf;
#else

#ifdef LAPTOP_AUDIO
#define PLAYBACKPACKETS 16
#else
#define PLAYBACKPACKETS 8
#endif

#define CALLBACKSPERPACKET 4

typedef struct alsabuf{
   int n;                 /* Number of valid samples in buffer */
  int id;
   Boolean isLast;        /* used on output to signal end of utterance */
   short waveData[WAVEPACKETSIZE];        /* Data in buffer */
   struct alsabuf *next;
} alsaBuf;
#endif
#endif

#ifdef WIN32
#define HAudioLock(); EnterCriticalSection(&(a->c));
#define HAudioUnlock(); LeaveCriticalSection(&(a->c));
#endif
#ifdef UNIX
#define HAudioLock(); pthread_mutex_lock(&(a->mux));
#define HAudioUnlock();  pthread_mutex_unlock(&(a->mux));
#endif

#define ECHOBUFFERSIZE  100   /* num wave pkts in echo ring buffer */
#define ECHOBUFFERHEAD    3   /* num unfilled advance blocks in ring */
#define ECHOTHRESHOLD 2.0E8   /* min energy in 1st blok sufficient to cause echo */

typedef struct {
   short spin[WAVEPACKETSIZE];
   short sout[WAVEPACKETSIZE];
   int tin;     /* approx time of last sample in spin in msecs */
   int oseq;    /* seq number for sout 1,2,3, ... */
}WavBlok;

typedef struct {
   WavBlok buf[ECHOBUFFERSIZE];
   int inx,outx,used;   /* bounded buffer indices and count */
   int lidx;            /* index of last output block */
   int lseq;            /* seq number of last output block */
   Vector fir;          /* the FIR filter 1..echoFilterSize*/
   Boolean firSet;      /* set when FIR filter is set */
   int delay;           /* estimated delay between output and echo */
}EchoBuffer;

typedef struct _AudioIn {
   /* -- Machine Independent Part -- */
   HTime sampPeriod;         /* sampling period in 100ns units */
   AudioDevStatus devStat;   /* indicates when device active */
   int curVol;               /* Current pk-pk volume of input speech */
   EchoBuffer echo;          /* Input ring buf for echo cancellation */
   int updateFilterIndex;    /* set to index of analysis window when update needed */
   /* -- Machine Dependent Part -- */
#ifdef WIN32
   MMRESULT mmError;
   HWAVEIN waveIn;
   LPPCMWAVEFORMAT waveFmt; /* Pointer to PCMWAVEFORMAT */
   LPMMTIME wavePos;        /* Pointer to MMTIME */
   mmApiBuf * current;      /* Index of current buffer */
   CRITICAL_SECTION c;
   HANDLE callBackEvent;
#endif
#ifdef UNIX
#ifdef __APPLE__
	AudioQueueRef audioQueue;
	AudioStreamBasicDescription audioFormatDescription;
   coreAudioBuf * current;
#else
  snd_pcm_t *pcm_handle;
  char *pcm_name;
#endif
  pthread_mutex_t mux;
  pthread_cond_t cond;
#endif
}AudioInRec;

typedef struct _AudioOut {
   /* -- Machine Independent Part -- */
   float vol;                /* current volume */
   Boolean isActive;         /* true when output device active */
   AudioIn ain;             /* associated input buffer if any */
   int total;               /* Total number of samples queued */
   int nBlocks;             /* Total number of blocks queued */
   /* -- Machine Dependent Part -- */
#ifdef WIN32
   MMRESULT mmError;
   HWAVEOUT waveOut;        /* handle to wave output device */
   LPPCMWAVEFORMAT waveFmt; /* Pointer to PCMWAVEFORMAT */
   LPMMTIME wavePos;        /* Pointer to MMTIME */
   mmApiBuf *current;       /* Pointer to current buffer */
   mmApiBuf *pHead;         /* Head of buffer list */
   mmApiBuf *pTail;         /* Tail of buffer list */
   DWORD callThreadId;      /* id of last thread to use this out channel */
   CRITICAL_SECTION c;      /* semaphore */
   HANDLE callBackEvent;    /* calls after every output buffer done */
#endif
#ifdef UNIX
  pthread_mutex_t mux;
  pthread_cond_t cond;
#ifdef __APPLE__
	AudioQueueRef audioQueue;
	AudioStreamBasicDescription audioFormatDescription;
	coreAudioBuf *current;
	coreAudioBuf *pHead;
	coreAudioBuf *pTail;
#else
  MemHeap *mem;
  snd_pcm_t *pcm_handle;
  char *pcm_name;
  alsaBuf *playingHead, *playingTail, *toPlayHead, *toPlayTail;
  HThread callThreadId;
  long int played; /* current buffer pos */
  long int stored;
  long vmin, vmax;
  snd_mixer_elem_t *mixer_elem;
#endif
#endif
}AudioOutRec;

/* --------------------- Echo Cancellation ---------------------- */

/* InitEchoBuf: initialise the echo buffer */
void InitEchoBuf(AudioIn a)
{
   int i,j;
   WavBlok *wb;
   a->echo.inx = a->echo.outx = a->echo.used = 0;
   a->echo.lidx = a->echo.lseq = 0;
#ifdef WIN32
   assert(a->waveFmt->wf.nSamplesPerSec%1000 == 0);  /* check rate is x1000 */
#endif
   for (i=0,wb=a->echo.buf; i<ECHOBUFFERSIZE; i++,wb++) {
      wb->oseq = 0;
      wb->tin = 0;
      for (j=0; j<WAVEPACKETSIZE; j++) wb->sout[j]=0;
   }
   a->echo.fir = CreateVector(&gcheap,echoFilterSize);
   ZeroVector(a->echo.fir);
   a->echo.delay = 0;   a->echo.firSet = FALSE;
   if (trace&T_TOP) {
      printf("Echo Buffer initialised: %d blocks ", ECHOBUFFERSIZE);
      if (echoCancelling)
         printf("EC ON - %d taps, maxdelay=%d, awin = %d blks\n",
                       echoFilterSize,echoMaxDelay,echoFilterAWin);
      else
         printf("EC OFF\n");
   }
}

/* ShowEchoBuffer: display echo buffer on stdout */
void ShowEchoBuf(AudioIn a, char *s)
{
   int i;

   fprintf(stdout,"%s\n",s);
   fprintf(stdout,"inx=%d, outx=%d, used=%d, lseq=%d, lidx=%d\n",
      a->echo.inx,a->echo.outx,a->echo.used,a->echo.lseq,a->echo.lidx);
   for (i=0; i<ECHOBUFFERSIZE; i++)
      fprintf(stdout,"%d. oseq=%d\n",i,a->echo.buf[i].oseq);
}

/* routines to save fir data for use in Matlab
static int firidx=0;
static void Savefir(Vector fir, int n)
{
   FILE *f;
   int i;
   char fn[100];

   sprintf(fn,"fir%d",++firidx);
   f = fopen(fn,"w");
   for (i=1; i<=n; i++) fprintf(f,"%e\n",fir[i]);
   fclose(f);
}

static int rxyidx=0;
static void Saverxy(Vector rxy, int n)
{
   FILE *f;
   int i;
   char fn[100];

   sprintf(fn,"rxy%d",++rxyidx);
   f = fopen(fn,"w");
   for (i=1; i<=n; i++) fprintf(f,"%e\n",rxy[i]);
   fclose(f);
}

static int ryyidx=0;
static void Saveryy(Vector ryy, int n)
{
   FILE *f;
   int i;
   char fn[100];

   sprintf(fn,"ryy%d",++ryyidx);
   f = fopen(fn,"w");
   for (i=1; i<=n; i++) fprintf(f,"%e\n",ryy[i]);
   fclose(f);
}

*/

/* UpdateEchoFilter: using bloks with oseq = 1..enIdx */
void UpdateEchoFilter(AudioIn a, int enIdx)
{
   /* x is noisy input, y is ref output */
   WavBlok *wbi,*wbi0,*wbj;   /* j is i delayed by k */
   int size;   /* size of filter and delay */
   int n,k,nsamp;    /* index across window, delay and num samples */
   int i,j,stIdx;    /* block indices - stIdx is start block */
   int ix,jx;        /* indices within a block */
   double sumyy,sumxy,scale;
   int ipeak;        /* index of peak in Rxy */
   float peak;
   Boolean toepOK;
   Vector Rxy,Ryy;   /* storage for computing correlations */

   /* calc sizes and create storage - note that last block in window is */
   /* only used to accommodate the overlap */
   size = echoFilterSize+echoMaxDelay+1;
   nsamp = (echoFilterAWin-1)*WAVEPACKETSIZE;
   assert(size<nsamp);
   Rxy = CreateVector(&gstack,echoFilterSize+echoMaxDelay+1);
   Ryy = CreateVector(&gstack,2*echoFilterSize-1);

   /* Find start of the window */
   stIdx = enIdx-echoFilterAWin+1;    /* find first blok */
   if (stIdx<0) stIdx += ECHOBUFFERSIZE;
   wbi0 = a->echo.buf+stIdx;
   if(wbi0->oseq != 1) { /* probably closing down ASource */
      HError(-1,"Filter stIdx oseq = %d",wbi0->oseq); return;
   }
   /* Calculate cross correlation coefs over window */
   scale = nsamp * 16000.0 * 16000.0;
   for (k=0; k<size; k++){
      sumyy = 0.0; sumxy = 0.0;
      wbi = wbi0;
      i = j = stIdx;
      ix = 0; jx = k;
      while (jx >= WAVEPACKETSIZE) {
         jx -= WAVEPACKETSIZE;
         if (++j >= ECHOBUFFERSIZE) j = 0;
      }
      wbj = a->echo.buf+j;
      for (n=0; n<nsamp; n++) {
         if (k<echoFilterSize) /* only need Ryy for filter */
            sumyy += wbi->sout[ix] * wbj->sout[jx];
         sumxy += wbi->sout[ix] * wbj->spin[jx];
         if (++ix >= WAVEPACKETSIZE) {
            ix = 0;
            if (++i >= ECHOBUFFERSIZE) i = 0;
            wbi = a->echo.buf+i;
         }
         if (++jx >= WAVEPACKETSIZE) {
            jx = 0;
            if (++j >= ECHOBUFFERSIZE) j = 0;
            wbj = a->echo.buf+j;
         }
      }
      sumxy /= scale;
      Rxy[k+1] = sumxy;
      if (k<echoFilterSize){
         sumyy /=scale;
         Ryy[echoFilterSize-k] = sumyy;
         Ryy[echoFilterSize+k] = sumyy;
      }
   }
   /* Scan Rxy and find peak */
   ipeak = 0; peak = 0.0;
   for (k=1; k<=echoMaxDelay; k++){
      if (fabs(Rxy[k]) > peak) {
         peak = fabs(Rxy[k]);
         ipeak = k;
      }
   }
   /* Enable these to save vectors for external analysis in Matlab */
   /* Saverxy(Rxy,size); */
   /* Saveryy(Ryy,2*echoFilterSize-1); */
   peak = 0.95 * ipeak;  /* only interested in position */
   a->echo.delay = peak;
   /* Shift Rxy samples */
   for (k=1; k<=echoFilterSize; k++) Rxy[k] = Rxy[k+a->echo.delay];
   /* Update filter */
   toepOK = Toeplitz(Ryy,a->echo.fir,Rxy,echoFilterSize);
   if (!toepOK) HError(9999,"UpdateEchoFilter: Cant invert toeplitz matrix\n");
   /* Savefir(a->echo.fir,echoFilterSize); */
   if (trace&T_TOP) {
      printf("Echo Filter updated: delay = %d (peak at %d)\n",a->echo.delay,ipeak);
   }
   if (trace&T_ECH) {
      ShowVector("Ryy",Ryy+echoFilterSize-1,32);
      ShowVector("Rxy",Rxy,32);
      ShowVector("FIR",a->echo.fir,32);
   }
   FreeVector(&gstack,Rxy);
}

/* StoreInputBlok: store wave packet in ring buffer.   This is called directly
   by the callback routine within the critical section */
void StoreInputBlok(AudioIn a, short *wave)
{
   int i, x, minSamp, maxSamp, discard;
   struct timeb tnow;
   long ts;

   /* locate next input blok and set current sample time */
   WavBlok *wb = a->echo.buf + a->echo.inx;
   ftime(&tnow);
   ts = (tnow.time%100)*1000+tnow.millitm;
   wb->tin = ts;    /* set time for this blok  */
   /* fprintf(stdout,">%d [t=%d]\n",a->echo.inx,ts);*/
   /* copy wave into current blok, and calc pk-pk */
   minSamp = maxSamp = wave[0];
   for (i=0; i<WAVEPACKETSIZE; i++){
      x = wave[i]; wb->spin[i] = x;
      if (x>maxSamp) maxSamp = x; else if (x<minSamp) minSamp = x;
   }
   a->curVol = maxSamp-minSamp;
   /* update indices and used count */
   if (++a->echo.inx == ECHOBUFFERSIZE) a->echo.inx = 0;
   ++a->echo.used; discard = 0;
   while(a->echo.used+ECHOBUFFERHEAD > ECHOBUFFERSIZE) {
      --a->echo.used; ++discard;
      if (++a->echo.outx == ECHOBUFFERSIZE) a->echo.outx = 0;
   }
   if (discard>0)
      fprintf(stdout,"HAudio overrun warning: %d blocks discarded\n",discard);
}

/* GetInputBlok: get input blok from echo buffer */
void GetInputBlok(AudioIn a, short *wave)
{
   int i,j,k,minb,outxm1,outxm2,isum;
   float sum,y;
   WavBlok *wb,*wbm1,*wbm2;   /* current blok[t], blok[t-1], blok[t-2] */

   HAudioLock();
   /* if not sampling and no data, return silence */
   if (a->devStat!=ADS_SAMPLING  && a->echo.used==0) {
      for (i=0; i<WAVEPACKETSIZE; i++)wave[i] = 0;
      HAudioUnlock();
      return;
   }
   /* if sampling wait till there is an input blok available */
   if (a->devStat==ADS_SAMPLING){
      while (a->echo.used <= echoFilterAWin+1) {
#ifdef WIN32
         HAudioUnlock();
         WaitForSingleObject(a->callBackEvent, INFINITE);
         ResetEvent(a->callBackEvent);
         HAudioLock();
#endif
#ifdef UNIX
      pthread_cond_wait(&(a->cond), &(a->mux));
#endif
      }
   }
   /* See if echo filter update needed and its predecessor */
   if (echoCancelling && a->updateFilterIndex>0) {
      HAudioUnlock();
      UpdateEchoFilter(a,a->updateFilterIndex);
      HAudioLock();
      a->updateFilterIndex = 0;
   }
   /* locate output blok */
   wb = a->echo.buf + a->echo.outx;
   outxm1 = a->echo.outx-1; if (outxm1<0) outxm1 += ECHOBUFFERSIZE;
   outxm2 = a->echo.outx-2; if (outxm2<0) outxm2 += ECHOBUFFERSIZE;
   wbm1 = a->echo.buf + outxm1;
   wbm2 = a->echo.buf + outxm2;
   if (echoCancelling) {
      /* if output active, then filter input */
      if ((wb->oseq>0 || wbm1->oseq>0) && a->echo.delay >= 0) {
         for (i=0; i<WAVEPACKETSIZE; i++) {
            sum = 0.0;
            for (k=0; k<echoFilterSize; k++) {
               j = i-(k+a->echo.delay);
               if (j>=0) y = wb->sout[j]; else {
                  j += WAVEPACKETSIZE;
                  if (j>=0) y = wbm1->sout[j]; else {
                     j += WAVEPACKETSIZE;
                     assert(j>=0);
                     y = wbm2->sout[j];
                  }
               }
               sum += y * a->echo.fir[k+1];
            }
            isum = sum;
            if (abs(wb->spin[i] - isum) < abs(wb->spin[i]))
               wb->spin[i] -= isum;
         }
      }
      /* if required output energy in block */
      if (trace&T_SNR) {
         for (i=0; i<WAVEPACKETSIZE; i++)
            blockEnergy += wb->spin[i]*wb->spin[i];
         blockSamples += WAVEPACKETSIZE;
      }
      /* reset seq num and zero output wave in preceding blok if any */
      if (wbm1->oseq > 0) {
         wbm1->oseq = 0;
         for (i=0; i<WAVEPACKETSIZE; i++)wbm1->sout[i] = 0;
         wbm1->tin = 0;
      }
   }
   /* copy wave into current blok */
   for (i=0; i<WAVEPACKETSIZE; i++)wave[i] = wb->spin[i];
   /* if required, dump both input and output to file */
   if (trace&T_DMP){
      for (i=0; i<WAVEPACKETSIZE; i++){
         fwrite(wb->spin+i,2,1,dumpf);
         fwrite(wb->sout+i,2,1,dumpf);
      }
   }
   /* update indices and used count */
   if (++a->echo.outx == ECHOBUFFERSIZE) a->echo.outx = 0;
   --a->echo.used;
   HAudioUnlock();
}

/* StoreOutputBlok: store this output block alongside the input wave */
void StoreOutputBlok(AudioIn a, short *wave, Boolean isLast)
{
   int i,wbi;
   WavBlok *wb;
   float sum;
   struct timeb tnow;
   long ts;

   /* decide where to put output block */
   if (a->echo.lseq==0) {
      /* this is the first block - so check its got some energy */
      sum = 0.0;
      for (i=0; i<WAVEPACKETSIZE; i++) sum += wave[i] * wave[i];
      if (sum < ECHOTHRESHOLD) return;
      /* ok, block is not just silence, so store alongside input */
      wbi = a->echo.inx-1;
      if (wbi<0) wbi = ECHOBUFFERSIZE-1;
      /* check for drift */
      /*ftime(&tnow);
      ts = (tnow.time%100)*1000+tnow.millitm;
      fprintf(stdout,"  tin=%d  tnow=%d\n",a->echo.buf[wbi].tin,ts);*/
   }else{
      /* otherwise use lidx */
      wbi = a->echo.lidx+1;
      if (wbi==ECHOBUFFERSIZE) wbi=0;
   }
   a->echo.lidx = wbi;  /* remember index for next time */
   wb = a->echo.buf+wbi;  wb->oseq = ++a->echo.lseq;
   /* copy wave into current blok */
   for (i=0; i<WAVEPACKETSIZE; i++) wb->sout[i] = wave[i];
   /* if AWIN'th blok of output, estimate filter coef */
   if (wb->oseq==echoFilterAWin) a->updateFilterIndex = wbi;
   /* if that was the last, reset the output index */
   if (isLast) a->echo.lseq = 0;
   if (trace&T_OUT)
      fprintf(stdout,"Echo> %d. seq=%d [inx=%d]\n",wbi,wb->oseq,a->echo.inx);
}

/* InSamples: return number of packets in echo buffer */
static int InSamples(AudioIn a)
{
   int inSamps;
   if (a->devStat!=ADS_SAMPLING && a->devStat != ADS_STOPPED)
      inSamps = 0;
   else {
      HAudioLock();
      inSamps = a->echo.used;
      HAudioUnlock();
   }
   if (trace&T_AUD)
      printf("Insample packets = %d\n",inSamps);
   return inSamps;
}

/* --------------------- Input Device Handling ---------------------- */

#ifdef WIN32
void * mmeAllocMem(size_t size)
{
   void * ptr;
   ptr=GlobalAlloc(GMEM_FIXED,size);
   if (ptr==NULL)
      HError(6006,"mmeAllocMem: Cannot allocate memory for mme structure");
   return(ptr);
}

void mmeFreeMem(void *ptr)
{
   ptr=GlobalFree(ptr);
   if (ptr!=NULL)
      HError(6006,"mmeFreeMem: Cannot free memory for mme structure");
}

/* CreateMMApiBuf: create an MMApiBuf for input */
mmApiBuf * CreateMMApiBuf(AudioIn a)
{
   mmApiBuf * p;
   p=(mmApiBuf *)malloc(sizeof(mmApiBuf));
   p->waveHdr = mmeAllocMem(sizeof(WAVEHDR));
   p->n=0; p->next=NULL;
   p->waveData = mmeAllocMem(WAVEPACKETSIZE*sizeof(short));
   /* Set up header */
   p->waveHdr->lpData = p->waveData;
   p->waveHdr->dwBufferLength = WAVEPACKETSIZE*sizeof(short);
   p->waveHdr->dwBytesRecorded = 0; /* Unused */
   p->waveHdr->dwUser = (DWORD)p;
   p->waveHdr->dwLoops = 0; p->waveHdr->dwFlags = 0;
   return p;
}

/* CallBackIn: called by the Windows mmAudio input thread */
void CALLBACK callBackIn(HWAVE hwaveIn, UINT msg, DWORD aptr,
                         LPARAM param1, LPARAM param2)
{
   AudioIn a;
   LPWAVEHDR curHdr;
   mmApiBuf *cur,*p,*r;
   int nSamples;
   a = (AudioIn)aptr;
   if (a->devStat==ADS_SAMPLING && msg==MM_WIM_DATA){
      HAudioLock();
      curHdr=(LPWAVEHDR)param1;
      assert(a->current->waveHdr==curHdr);
      assert(a->current->waveHdr->dwUser == (DWORD)a->current);
      StoreInputBlok(a,(short *)curHdr->lpData);
      /* add current mmapibuf back into driver */
      if ((a->mmError=waveInAddBuffer(a->waveIn, curHdr,sizeof(WAVEHDR)))!=MMSYSERR_NOERROR)
         HError(6006,"callBackIn: waveInAddBuffer failed %d",a->mmError);
      /* update current buf pointer */
      a->current=a->current->next;
      HAudioUnlock();
      /* signal to input read routine in case it is blocked */
      SetEvent(a->callBackEvent);
   }
}
#endif
#ifdef UNIX
#ifdef __APPLE__

coreAudioBuf * CreateCoreAudioBuf(AudioIn a)
{
   coreAudioBuf * p;
   p=(coreAudioBuf *)malloc(sizeof(coreAudioBuf));
   p->n=0; p->next=NULL;
   return p;
}

void CoreAudioQueueInputCallback (void                            *inUserData,
									   AudioQueueRef                       inAQ,
									   AudioQueueBufferRef                 inBuffer,
									   const AudioTimeStamp                *inStartTime,
									   UInt32                              inNumberPacketDescriptions,
									   const AudioStreamPacketDescription  *inPacketDescs
									   )
{
   OSStatus st;
   AudioIn a = (AudioIn)inUserData;
/*	NSLog(CFSTR("Recorded %d bytes"), inBuffer->mAudioDataByteSize); */
   if (a->devStat==ADS_SAMPLING) {
      HAudioLock();
      StoreInputBlok(a, (short *)inBuffer->mAudioData);
   	st = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
   	if (st) HError(6006,"CoreAudioQueueInputCallback: Error in AudioQueueEnqueueBuffer\n", st);
   	    pthread_cond_signal(&(a->cond));
      HAudioUnlock();
   }
}

#else
void alsa_capture(snd_async_handler_t *pcm_callback)
{
  snd_pcm_t *pcm_handle_cb;
  snd_pcm_sframes_t avail;
  AudioInRec *a;
  int i, n;
  short waveData[WAVEPACKETSIZE];

  pcm_handle_cb=snd_async_handler_get_pcm(pcm_callback);
  a =(AudioInRec*) snd_async_handler_get_callback_private(pcm_callback);
  avail=snd_pcm_avail_update(pcm_handle_cb);
  if(avail<WAVEPACKETSIZE) {
    if(trace&&T_AUD)
      printf("Capture: Not enough samples! %d\n", avail);
    return;
  }

  while(snd_pcm_avail_update(pcm_handle_cb)>WAVEPACKETSIZE){
    n=WAVEPACKETSIZE;
    HAudioLock();
    snd_pcm_prepare(pcm_handle_cb);
    i=snd_pcm_readi(pcm_handle_cb, waveData, n);
    if(i<0)
      HError(6006,"PlayAudo: Error in capture callback routine\n");
    if(i!=n)
      HError(6006,"PlayAudo: capture callback routine - too few read\n");
    if (a->devStat==ADS_SAMPLING)
      StoreInputBlok(a,waveData);

    pthread_cond_signal(&(a->cond));
    HAudioUnlock();
  }

}
#endif
#endif

/* InitAudi: initialise the given audio input device */
static void InitAudi(AudioIn a, HTime *sampPeriod)
{
   if (a->devStat!=ADS_INIT) return;
   if (trace&T_AUD) {
      printf("Initialising Audio Input @%.0f\n",*sampPeriod);
      fflush(stdout);
   }
#ifdef WIN32
   {
      int i=0;
      mmApiBuf *tail,*p;
      /* Allocate special structures */
      a->waveFmt = mmeAllocMem(sizeof(PCMWAVEFORMAT));
      a->wavePos = mmeAllocMem(sizeof(MMTIME));
      /* Set up required format */
      a->waveFmt->wf.wFormatTag = WAVE_FORMAT_PCM;
      a->waveFmt->wf.nChannels = 1;
      if(*sampPeriod == 0.0){
         *sampPeriod = 1.0E+07 / (float)16000;
         a->waveFmt->wf.nSamplesPerSec = 16000;
      }else
         a->waveFmt->wf.nSamplesPerSec = 1.0E+07F / *sampPeriod;
      a->waveFmt->wf.nBlockAlign = sizeof(short);
      a->waveFmt->wf.nAvgBytesPerSec =
         a->waveFmt->wf.nBlockAlign*a->waveFmt->wf.nSamplesPerSec;
      a->waveFmt->wBitsPerSample = 16;
      /* Set up position query */
      a->wavePos->wType = TIME_SAMPLES;
      /* Init thread stuff */
      InitializeCriticalSection( &(a->c) );
      a->callBackEvent = CreateEvent( NULL, TRUE, FALSE, "inCallBack" );
      /* Create ring of mmapibufs */
      a->current = CreateMMApiBuf(a); tail = a->current;
      for (i=1; i<MMAPI_BUFFER_COUNT; i++) {
         p = CreateMMApiBuf(a);
         p->next = a->current; a->current = p;
      }
      tail->next = a->current;  /* make it a ring */
      if (trace & T_AUD)
         printf(" Initialised MMAPI audio input at %.2fkHz\n",
            a->waveFmt->wf.nSamplesPerSec*1E-3);
   }
#endif
#ifdef UNIX
#ifdef __APPLE__
   {
      /* int bufferSize = (SAMPLE_TIME / 1000.0f) * (audioFormatDescription.mSampleRate * audioFormatDescription.mBytesPerFrame);*/
      OSStatus st;
      int bufferSize = WAVEPACKETSIZE*sizeof(short);
      int i;
      int err;
      coreAudioBuf *p, *tail;
      
		a->audioFormatDescription.mBitsPerChannel = 16;
		a->audioFormatDescription.mChannelsPerFrame = 1;
		a->audioFormatDescription.mBytesPerFrame = 2;
		a->audioFormatDescription.mFramesPerPacket = 1;
		a->audioFormatDescription.mBytesPerPacket = 2;
		a->audioFormatDescription.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
		a->audioFormatDescription.mFormatID = kAudioFormatLinearPCM;
		a->audioFormatDescription.mSampleRate = 16000;

      st = AudioQueueNewInput(&a->audioFormatDescription, CoreAudioQueueInputCallback, a, NULL, NULL, 0, &a->audioQueue);
      if (st) HError(6006,"InitAudi: AudioQueueNewInput failed",st);
      
      a->current = CreateCoreAudioBuf(a); tail = a->current;      
      for (i=1; i<AUDIO_QUEUE_BUFFER_COUNT; i++) {
         p = CreateCoreAudioBuf(a);
         p->next = a->current; a->current = p;
      }
      tail->next = a->current;  /* make it a ring */
      
      err=pthread_mutex_init(&(a->mux),NULL);
      if (err!=0)
        HError(9999,"InitAudi: cant create mux",err);
      err=pthread_cond_init(&(a->cond),NULL);
      if (err!=0)
        HError(9999,"InitAudi: cannot create internal update signal",err);
   }
#else
   {
     int err, dir, rate;
     snd_pcm_hw_params_t *hwparams;
     snd_pcm_sw_params_t *swparams;
     snd_async_handler_t *pcm_callback;
     unsigned int exact_rate, periods;
     snd_pcm_uframes_t buffersize; /* Periodsize (bytes) */
     snd_pcm_uframes_t periodsize=WAVEPACKETSIZE; /* Periodsize (bytes) */
     snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;

     if(*sampPeriod == 0.0){
       *sampPeriod = 1.0E+07 / (float)16000;
       rate = 16000;
     }else
       rate = 1.0E+07F / *sampPeriod;

     /* create mux and conds */
     err=pthread_mutex_init(&(a->mux),NULL);
     if (err!=0)
       HError(9999,"InitAudi: cant create mux",err);
     err=pthread_cond_init(&(a->cond),NULL);
     if (err!=0)
       HError(9999,"InitAudi: cannot create internal update signal",err);

     a->pcm_name = strdup("plughw:0,0");
     snd_pcm_hw_params_malloc(&hwparams);
     snd_pcm_sw_params_malloc(&swparams);
     /* ??? replace with NONBLOCK? */
     if (snd_pcm_open(&a->pcm_handle, a->pcm_name, stream, SND_PCM_STREAM_CAPTURE) < 0) {
       HError(6006,"InitAudi: Error opening PCM device %s\n", a->pcm_name);
     }
     /* Init hwparams with full configuration space */
     if (snd_pcm_hw_params_any(a->pcm_handle, hwparams) < 0) {
       HError(6006, "InitAudi: Can not configure this PCM device.\n");
     }
     if (snd_pcm_hw_params_set_access(a->pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
     HError(6006, "InitAudi: Error setting access.\n");
     }
     /* Set sample format*/
     if (snd_pcm_hw_params_set_format(a->pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
       HError(6006, "InitAudi: Error setting format.\n");
     }
     /* Set sample rate. If the exact rate is not supported */
     /* by the hardware, use nearest possible rate.         */
     exact_rate = rate;
     if (snd_pcm_hw_params_set_rate_near(a->pcm_handle, hwparams, &exact_rate, 0) < 0) {
       HError(6006, "InitAudi: Error setting rate.\n");
     }
     if (rate != exact_rate) {
       HError(-6006, "InitAudi: The rate %d Hz is not supported by your hardware.\n==> Using %d Hz instead.\n", rate, exact_rate);
     }

     /* Set mono */
     if (snd_pcm_hw_params_set_channels(a->pcm_handle, hwparams, 1) < 0) {
       HError(6006, "InitAudi: Error setting channels.\n");
     }
     dir=0;
     /* Set number of periods. Periods used to be called fragments. */
     /*periods =4;
       if (snd_pcm_hw_params_set_periods_near(a->pcm_handle, hwparams, &periods, &dir) < 0) {
       HError(6006, "InitAudi: Error setting periods.\n");
       }*/
     /* Set buffer size (in frames). The resulting latencyis given by */
     dir=0;
     /* latency = periodsize * periods / (rate * bytes_per_frame)     */
     if (snd_pcm_hw_params_set_period_size_near(a->pcm_handle, hwparams, &periodsize,&dir) < 0) {
       HError(6006, "InitAudi: Error setting periodsize.\n");
     }
     /* Apply HW parameter settings to */
     /* PCM device and prepare device  */
     if (snd_pcm_hw_params(a->pcm_handle, hwparams) < 0) {
       HError(6006, "InitAudi: Error setting HW params.\n");
     }
     snd_pcm_hw_params_get_period_size( hwparams,&periodsize,0);
     snd_pcm_hw_params_get_buffer_size( hwparams,&buffersize);
     snd_pcm_hw_params_get_periods(hwparams,&periods,0);
     if (trace&T_AUD) {
       printf("Capture Period size %d\n", periodsize);
       printf("Capture size %d\n", buffersize);
       printf("Capture Periods %d\n", periods);
     }
     if (snd_pcm_sw_params_current (a->pcm_handle, swparams)<0)
       HError(6006, "InitAudi: Error getting current swe params\n");

     if (snd_pcm_sw_params_set_start_threshold(a->pcm_handle, swparams, WAVEPACKETSIZE)<0)
       HError(6006, "InitAudi: Error setting SW start thresh.\n");

     if(snd_pcm_sw_params_set_avail_min(a->pcm_handle, swparams, WAVEPACKETSIZE)<0)
       HError(6006, "InitAudi: Error setting SW set avail.\n");

     if ((err=snd_pcm_sw_params(a->pcm_handle, swparams)) < 0) {
       HError(6006, "InitAudi: Error setting SW params. err=%d\n",err);
     }
     snd_async_add_pcm_handler(&pcm_callback, a->pcm_handle, alsa_capture, a);
     snd_pcm_hw_params_free(hwparams);
     snd_pcm_sw_params_free(swparams);
   }
#endif
#endif
   InitEchoBuf(a);
   a->devStat = ADS_OPEN;
   a->updateFilterIndex = 0;
}

/* CloseAudi: close the given audio input device */
static void CloseAudi(AudioIn a)
{
   if (a->devStat<ADS_OPEN || a->devStat==ADS_CLOSED) return;
   if (trace&T_AUD) {
      printf("Closing Audio Input from %d\n",a->devStat);
      fflush(stdout);
   }
#ifdef WIN32
   {
      int i;
      mmApiBuf *p,*next;
      for (i=0,p=a->current;i<MMAPI_BUFFER_COUNT;i++,p=next) {
         next = p->next;
         if ((a->mmError=waveInUnprepareHeader(a->waveIn, p->waveHdr,
            sizeof(WAVEHDR)))!=MMSYSERR_NOERROR)
            HError(6006,"CloseAudi: Header Unpreparation failed");
         mmeFreeMem(p->waveHdr);  mmeFreeMem(p->waveData);
         free(p);
      }
      mmeFreeMem(a->waveFmt);  mmeFreeMem(a->wavePos);
      if (a->devStat>ADS_SAMPLING)
         if ((a->mmError = waveInClose(a->waveIn))!=MMSYSERR_NOERROR)
            HError(6006,"CloseAudi: Could not close MMAPI audio [ERR=%d]",a->mmError);
      if (trace & T_AUD)
         printf(" Closing MMAPI audio input\n");
   }
#endif
#ifdef UNIX
#ifdef __APPLE__
   {
      OSStatus st;
   	/* TODO free memory */
      printf(" Closing CoreAudio audio input\n");
      st = AudioQueueDispose(a->audioQueue, true);
      if (st) HError(6006,"CloseAudi: AudioQueueDispose failed",st);
   }
#else
   /* TODO: check this is okay? Need to clean up cond/mux?*/
   if (trace & T_AUD)
         printf(" Closing ALSA audio input\n");
   snd_pcm_drop(a->pcm_handle);
   snd_pcm_close (a->pcm_handle);
#endif
#endif
   a->devStat = ADS_CLOSED;
   a->curVol = 0;
   a->updateFilterIndex = 0;
}


/* StartAudi: start the audio input device sampling */
static void StartAudi(AudioIn a)
{
   a->curVol = 0;
   if (a->devStat!=ADS_OPEN) return;
   if (trace&T_AUD) {
      printf("Starting Audio Input from %d\n",a->devStat);
      fflush(stdout);
   }
#ifdef WIN32
   {
      int i;
      mmApiBuf *p;

      if ((a->mmError=waveInOpen(NULL, WAVE_MAPPER, (LPWAVEFORMATEX)a->waveFmt,
                                 0,0,WAVE_FORMAT_QUERY))!=MMSYSERR_NOERROR)
         HError(6006,"StartAudi: Requested data format is not supported [ERR=%d]",a->mmError);
      if ((a->mmError=waveInOpen(&a->waveIn, WAVE_MAPPER,(LPWAVEFORMATEX)a->waveFmt,
                                 (DWORD)callBackIn,(DWORD)a,CALLBACK_FUNCTION))!=MMSYSERR_NOERROR)
                                 HError(6006,"StartAudi: Cannot open MMAPI audio input [ERR=%d]",a->mmError);
      /* Add all ring buffers into device queue */
      for (i=0,p=a->current; i<MMAPI_BUFFER_COUNT; i++,p=p->next) {
         if ((a->mmError=waveInPrepareHeader(a->waveIn, p->waveHdr,
            sizeof(WAVEHDR)))!=MMSYSERR_NOERROR)
            HError(6006,"StartAudi: Header preparation failed");

         if ((a->mmError=waveInAddBuffer(a->waveIn, p->waveHdr,
            sizeof(WAVEHDR)))!=MMSYSERR_NOERROR)
            HError(6006,"StartAudi: waveInAddBuffer [ERR=%d]",a->mmError);
      }
      /* And start recording */
      if ((a->mmError=waveInStart( a->waveIn ))!=MMSYSERR_NOERROR)
         HError(6006,"StartAudi: waveInStart [ERR=%d]",a->mmError);
      ResetEvent(a->callBackEvent);
   }
#endif
#ifdef UNIX
#ifdef __APPLE__
  {
     int i;
     AudioQueueBufferRef b;
     OSStatus st;
     coreAudioBuf *p;
     
     for (i=0,p=a->current; i<AUDIO_QUEUE_BUFFER_COUNT; i++,p=p->next) {
        st = AudioQueueAllocateBuffer(a->audioQueue, WAVEPACKETSIZE*sizeof(short), &b);
        if (st) HError(6006,"StartAudi: AudioQueueAllocateBuffer failed",st);
        p->audioQueueBuffer = b;
        st = AudioQueueEnqueueBuffer(a->audioQueue, b, 0, NULL);
        if (st) HError(6006,"StartAudi: AudioQueueEnqueueBuffer failed",st);
     }
     st = AudioQueueStart(a->audioQueue, NULL);
     if (st) HError(6006,"StartAudi: AudioQueueStart failed",st);
  }
#else
  {
    int err;
    if(snd_pcm_start(a->pcm_handle)!=0)
       HError(6006, "StartAudi: Unable to start ALSA audio port Err=%d",err);
   }
#endif
#endif
   a->devStat = ADS_SAMPLING;
   if (trace&T_SNR) { blockEnergy = 0.0; blockSamples = 0; }
}

/* StopAudi: stop the audio input device sampling */
static void StopAudi(AudioIn a)
{
   if (a->devStat!=ADS_SAMPLING) return;
   if (trace&T_AUD)
      printf("Stopping Audio Input from %d\n",a->devStat);
#ifdef WIN32
   if ((a->mmError=waveInStop(a->waveIn))!=MMSYSERR_NOERROR)
      HError(6006,"StopAudi: Cannot stop MMAPI input [ERR=%d]",a->mmError);
   a->devStat = ADS_STOPPED;
   if ((a->mmError=waveInReset(a->waveIn))!=MMSYSERR_NOERROR)
      HError(6006,"StopAudi: Cannot reset MMAPI input [ERR=%d]",a->mmError);
#endif
#ifdef UNIX
#ifdef __APPLE__
   {
      OSStatus st;
      a->devStat = ADS_STOPPED;
      st = AudioQueueStop(a->audioQueue, true);
      if (st) HError(6006,"StopAudi: AudioQueueStop failed",st);
   }
#else
   /* StopAudi: stop the audio input device sampling */
   a->devStat = ADS_STOPPED;
   snd_pcm_drop(a->pcm_handle);
   /* TODO fix and clear? */
#endif
#endif
   a->curVol = 0;
   if (trace&T_SNR)
      printf("Input energy per sample = %.4f dB\n",10*(log(blockEnergy)-log(blockSamples)));
}

/* -------------------- Device Dependent Output Routines ------------- */

#ifdef WIN32
static mmApiBuf * pool = NULL;
static int poolSize = 0;
static int allocated = 0;

/* GetMMBuf: get an output buffer from pool */
static mmApiBuf *GetMMBuf(AudioOut a)
{
   mmApiBuf * p;
   if (pool!=NULL) {
      p = pool; pool = p->next;
      --poolSize;
   } else {
      p=(mmApiBuf *)malloc(sizeof(mmApiBuf));
      p->waveHdr = mmeAllocMem(sizeof(WAVEHDR));
   }
   ++allocated;
   return p;
}

/* RemoveUsedBuffers: scan a and remove all played buffers */
static void RemoveUsedBuffers(AudioOut a)
{
   mmApiBuf *p,*acur;
   /* get a copy of a->current, it might change whilst this
      routine is operating, but this is harmless since it will only result
      in some used buffers being left on the queue.
   */
   HAudioLock();
   acur = a->current;
   HAudioUnlock();
   p = a->pHead;
   if (p==NULL || p==acur) return;
   while (p!=NULL && p!=acur) {
      assert(a->nBlocks>0);
      --a->nBlocks;
      if((a->mmError=waveOutUnprepareHeader(a->waveOut, p->waveHdr,
         sizeof(WAVEHDR))) != MMSYSERR_NOERROR)
         HError(6006,"RemoveUsedBuffers: MMAPI Header unprep failed [ERR=%d]",a->mmError);
      mmeFreeMem(p->waveData);
      /* detach p from queue */
      if (p->next==NULL) {
         a->pHead = a->pTail = NULL;
      }else{
         a->pHead = p->next;
      }
      /* put p back into pool */
      p->next = pool;  pool = p;
      --allocated; ++poolSize;
      /* and repeat */
      p = a->pHead;
   }
}

/* callBackOut: called by the mmAudio output thread */
void CALLBACK callBackOut(HWAVE hwaveIn, UINT msg,
                          DWORD aptr, DWORD waveHdr, DWORD notUsed)
{
   AudioOut a;
   mmApiBuf *p;

   if (msg!=MM_WOM_DONE) return;  /* not a WOM callback */
   a = (AudioOut)aptr;
   assert(a!=NULL);
   HAudioLock();
   if (a->current != NULL) {
      assert(a->current->waveHdr == (LPWAVEHDR)waveHdr);
      if (echoCancelling) {
         if (a->isActive && a->ain != NULL && a->ain->devStat == ADS_SAMPLING)
            StoreOutputBlok(a->ain,(short *)a->current->waveData,a->current->isLast);
      }
      SetEvent(a->callBackEvent);
      if (a->current->isLast && a->callThreadId != 0) {
         PostThreadMessage(a->callThreadId,WM_AUDOUT,0,0);
      }
      a->current = a->current->next;
   }
   HAudioUnlock();
}
#endif

#ifdef UNIX
#ifdef __APPLE__
  /* TODO */
#else
/* try to store nBlocks from the now playing queue*/
int alsa_store_from_playing(AudioOutRec *a, int tostore)
{
  alsaBuf *aBuf;
  int i,stored=0;
  while(tostore>WAVEPACKETSIZE) {
    if(!a->playingHead) {
      if(trace&T_OUT)printf("Now Playing queue null/empty\n");
      a->playingTail=NULL;
      return 0;
    }
    aBuf=a->playingHead;
    if (echoCancelling) {
      if (a->ain != NULL && a->ain->devStat == ADS_SAMPLING)
	StoreOutputBlok(a->ain,aBuf->waveData,aBuf->isLast);
    }
    pthread_cond_signal(&(a->cond));
    if(trace&T_AUD)
      printf("stored %d\n", aBuf->id);
    if (aBuf->isLast && a->callThreadId != 0) {
      HEventRec r;
      r.event=HAUDOUT;
      postEventToQueue(a->callThreadId,r);
      if(trace&T_AUD)
	printf("HAUDIO: sent HAUDOUT signal for last block\n");
    }
    a->playingHead=aBuf->next;
    stored+=aBuf->n;
    tostore-=aBuf->n;
    Dispose(a->mem, aBuf);
  }
  return stored;
}



/* put as much from the queue onto the audio device and save as OutputBlok */
/* returns number of blocks played */
int alsa_play_from_queue(AudioOutRec *a)
{
  /* and play */
  snd_pcm_sframes_t avail;
  alsaBuf *aBuf;
  int i,played=0;

  while((avail=snd_pcm_avail_update(a->pcm_handle))>WAVEPACKETSIZE) {
    /*if(snd_pcm_avail_update(a->pcm_handle)>WAVEPACKETSIZE) {*/
    if(!a->toPlayHead) {
      if(trace & T_OUT) printf("To play queue null/empty\n");
      a->toPlayTail=NULL; return played;
    }
    aBuf=a->toPlayHead;
    i=snd_pcm_writei(a->pcm_handle, aBuf->waveData, aBuf->n);
    if(i<0) {
      if(i==-EPIPE) {
	HError(-6006,"PlayAudo: Buffer Underrun! %d\n", i);
	snd_pcm_prepare(a->pcm_handle);
	/* TODO Should query avail for degree of underrun, push all playing onto*/
	/* played and pad with zeros for the rest */
	continue;
      }
      else
	HError(6006,"PlayAudo: Error in pcm_write routine %d\n", i);
    }
    if(i!=aBuf->n)
      HError(6006,"PlayAudo: Not enough written %d\n", i);
    played+=i;
    if(trace&T_AUD)
      printf("sent block %d to audio dev\n", aBuf->id);
    a->toPlayHead=aBuf->next;
    /*push onto now playing queue */
    if(a->playingHead==NULL)
      a->playingHead=a->playingTail=aBuf;
    else {
      a->playingTail->next=aBuf;
      a->playingTail=aBuf;
    }
    a->playingTail->next=NULL;
  }
  return played;
}

void alsa_playback(snd_async_handler_t *pcm_callback)
{
  snd_pcm_t *pcm_handle_cb = snd_async_handler_get_pcm(pcm_callback);
  AudioOutRec *a;
  int avail, played, tostore;

  a =(AudioOutRec*) snd_async_handler_get_callback_private(pcm_callback);
  HAudioLock();
  played=alsa_play_from_queue(a);
  avail=snd_pcm_avail_update(a->pcm_handle);
  a->played+=played;
  tostore=a->played+avail-a->stored;
  if(trace&T_OUT)
    printf("Hit playback callback, avail = %d played= %d tostore %d\n", avail, played, tostore);
  a->stored+=alsa_store_from_playing(a,tostore);

  HAudioUnlock();
}
#endif
#endif

/* InitAudo: initialise the given audio output device */
static void InitAudo(AudioOut a, HTime *sampPeriod)
{
#ifdef WIN32
   {
      /* Initialise */
      a->total=0; a->nBlocks = 0;
      a->current=NULL; a->pHead=a->pTail=NULL;
      a->callThreadId = 0;
      /* Allocate special structures */
      a->waveFmt = mmeAllocMem(sizeof(PCMWAVEFORMAT));
      a->wavePos = mmeAllocMem(sizeof(MMTIME));
      /* Set up required format */
      a->waveFmt->wf.wFormatTag = WAVE_FORMAT_PCM;
      a->waveFmt->wf.nChannels = 1;
      if(*sampPeriod == 0.0){
         *sampPeriod = 1.0E+07 / (float)16000;
         a->waveFmt->wf.nSamplesPerSec = 16000;
      }else
         a->waveFmt->wf.nSamplesPerSec = 1.0E+07 / *sampPeriod;
      a->waveFmt->wf.nBlockAlign = sizeof(short);
      a->waveFmt->wf.nAvgBytesPerSec =
         a->waveFmt->wf.nBlockAlign*a->waveFmt->wf.nSamplesPerSec;
      a->waveFmt->wBitsPerSample = 16;
      /* Set up position query */
      a->wavePos->wType = TIME_SAMPLES;
      /* Open wave device */
      if ((a->mmError=waveOutOpen(&a->waveOut, WAVE_MAPPER,
              (LPWAVEFORMATEX)a->waveFmt, (DWORD)callBackOut, (DWORD)a,
               CALLBACK_FUNCTION))!=MMSYSERR_NOERROR)
         HError(6006,"InitAudo: Cannot open MMAPI audio output [ERR=%d]",a->mmError);
      InitializeCriticalSection( &(a->c) );
      a->callBackEvent = CreateEvent( NULL, TRUE, FALSE, "outCallBack" );
      if (trace & T_AUD) {
         printf(" Initialised MMAPI audio output at %.2fkHz\n",
                a->waveFmt->wf.nSamplesPerSec*1E-3);
         fflush(stdout);
      }
   }
#endif

#ifdef UNIX
#ifdef __APPLE__
   {
		/* TODO set up output audioqueue, mux and cond */
   }
#else
   {
     int err, dir, rate;
     snd_pcm_hw_params_t *hwparams;
     snd_pcm_sw_params_t *swparams;
     snd_async_handler_t *pcm_callback;
     unsigned int exact_rate, periods;
     snd_pcm_uframes_t buffersize; /* Periodsize (bytes) */
     snd_pcm_uframes_t periodsize;
     /*(set as a fraction to recover from weird missing periods?*/
     snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
     snd_mixer_t *mixer_handle;
     snd_mixer_selem_id_t *sid;

     a->callThreadId = NULL;
     a->total=0;a->nBlocks=0; a->played=0; a->stored=0;
     a->toPlayHead=a->toPlayTail=NULL;
     a->playingHead=a->playingTail=NULL;
     if(*sampPeriod == 0.0){
       *sampPeriod = 1.0E+07 / (float)16000;
       rate = 16000;
     } else
       rate = 1.0E+07F / *sampPeriod;

     a->mem=&gcheap;

     /* create mux and conds */
     err=pthread_mutex_init(&(a->mux),NULL);
     if (err!=0)
       HError(9999,"InitAudo: cant create mux",err);
     err=pthread_cond_init(&(a->cond),NULL);
     if (err!=0)
       HError(9999,"InitAudo: cannot create internal update signal",err);

     a->pcm_name = strdup("plughw:0,0");
     snd_pcm_hw_params_malloc(&hwparams);
     snd_pcm_sw_params_malloc(&swparams);
     /* ??? NONBLOCK! */
     if (snd_pcm_open(&a->pcm_handle, a->pcm_name, stream,SND_PCM_STREAM_PLAYBACK) < 0) {
       HError(6006,"InitAudo: Error opening PCM device %s\n", a->pcm_name);
     }
     /* Init hwparams with full configuration space */
     if (snd_pcm_hw_params_any(a->pcm_handle, hwparams) < 0) {
       HError(6006, "InitAudo: Can not configure this PCM device.\n");
     }
     if (snd_pcm_hw_params_set_access(a->pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
     HError(6006, "InitAudo: Error setting access.\n");
     }
     /* Set sample format*/
     if (snd_pcm_hw_params_set_format(a->pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
       HError(6006, "InitAudo: Error setting format.\n");
     }
     /* Set sample rate. If the exact rate is not supported */
     /* by the hardware, use nearest possible rate.         */
     exact_rate = rate;
     if (snd_pcm_hw_params_set_rate_near(a->pcm_handle, hwparams, &exact_rate, 0) < 0) {
       HError(6006, "InitAudo: Error setting rate.\n");
     }
     if (rate != exact_rate) {
       HError(-6006, "InitAudo: The rate %d Hz is not supported by your hardware.\n==> Using %d Hz instead.\n", rate, exact_rate);
     }
     /* Set mono */
     if (snd_pcm_hw_params_set_channels(a->pcm_handle, hwparams, 1) < 0) {
       HError(6006, "InitAudo: Error setting channels.\n");
     }
     dir=0;
     /* Set number of periods. Periods used to be called fragments. */
     /* The idea here is that we get a callback every period */
     /* As we don't want to miss anything, we ask for several callbacks*/
     /* per WAVEPACKET - sometimes a callback will be missed */
     periods=PLAYBACKPACKETS*CALLBACKSPERPACKET+CALLBACKSPERPACKET-1;
     periodsize=WAVEPACKETSIZE/CALLBACKSPERPACKET; /* Periodsize (bytes) */

     if (snd_pcm_hw_params_set_periods_near(a->pcm_handle, hwparams, &periods, &dir) < 0) {
       HError(6006, "InitAudo: Error setting periods.\n");
     }
     dir=0;
     /* set period size */
     /* latency = periodsize * periods / (rate * bytes_per_frame)  */
     if (snd_pcm_hw_params_set_period_size_near(a->pcm_handle, hwparams, &periodsize,&dir) < 0) {
       HError(6006, "InitAudo: Error setting periodsize.\n");
     }
     buffersize=(periods)*periodsize;
     if (snd_pcm_hw_params_set_buffer_size_near(a->pcm_handle, hwparams, &buffersize) < 0) {
       HError(6006, "InitAudo: Error setting periodsize.\n");
     }
     /* Apply HW parameter settings to */
     /* PCM device and prepare device  */
     if (snd_pcm_hw_params(a->pcm_handle, hwparams) < 0) {
       HError(6006, "InitAudo: Error setting HW params.\n");
     }
     dir=0;
     snd_pcm_hw_params_get_period_size( hwparams,&periodsize,&dir);
     snd_pcm_hw_params_get_buffer_size( hwparams,&buffersize);
     dir=0;
     snd_pcm_hw_params_get_periods(hwparams,&periods,&dir);
     if (trace&T_AUD) {
       printf("Playback Period size %d\n", periodsize);
       printf("Playback Buffer size %d\n", buffersize);
       printf("Playback Periods %d\n", periods);
     }
     if (snd_pcm_sw_params_current (a->pcm_handle, swparams)<0)
       HError(6006, "InitAudo: Error getting current swe params\n");

     if (snd_pcm_sw_params_set_start_threshold(a->pcm_handle, swparams, WAVEPACKETSIZE)<0)
       HError(6006, "InitAudo: Error setting SW start thresh.\n");

     if(snd_pcm_sw_params_set_avail_min(a->pcm_handle, swparams, WAVEPACKETSIZE)<0)
       HError(6006, "InitAudo: Error setting SW set avail.\n");

     if ((err=snd_pcm_sw_params(a->pcm_handle, swparams)) < 0) {
       HError(6006, "InitAudo: Error setting SW params. err=%d\n",err);
     }
     snd_async_add_pcm_handler(&pcm_callback, a->pcm_handle, alsa_playback, a);

     /* finally set up mixer stuff: */
     /* open the mixer */

     if ((err=snd_mixer_open (&mixer_handle, 0)) < 0)
       HError(6006, "InitAudo: Error getting mixer, err=%d\n",err);
     /* attach the handle to the default card */
     if((err=snd_mixer_attach(mixer_handle, "default"))<0)
       HError(6006, "InitAudo: Error attaching mixer, err=%d\n",err);
     if((err=snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0)
       HError(6006, "InitAudo: Error registering mixer, err=%d\n",err);
     if((err=snd_mixer_load(mixer_handle)) < 0)
       HError(6006, "InitAudo: Error registering mixer, err=%d\n",err);
     snd_mixer_selem_id_malloc(&sid);
     snd_mixer_selem_id_set_name (sid, "Master");
     a->mixer_elem = snd_mixer_find_selem(mixer_handle, sid);
     if (!a->mixer_elem)
       HError(6006, "InitAudo: Error finding mixer element err=%d\n",a->mixer_elem);
     if (!snd_mixer_selem_has_playback_volume(a->mixer_elem))
       HError(6006, "InitAudo: Error can't set volume err=%d\n",err);

     snd_mixer_selem_get_playback_volume_range (a->mixer_elem,
						&(a->vmin),
						&(a->vmax));
     if (trace&T_AUD)
       printf("Min vol: %d, max vol %d\n", a->vmin, a->vmax);
     snd_mixer_selem_id_free(sid);
     snd_pcm_hw_params_free(hwparams);
     snd_pcm_sw_params_free(swparams);
   }
#endif
#endif
   a->isActive = FALSE;
}

/* CloseAudo: close the given audio output device */
static void CloseAudo(AudioOut a)
{
  a->isActive = FALSE;
#ifdef WIN32
   {
      mmApiBuf *p;

      /* Block until finished playing */
      while(a->current != NULL) {
         WaitForSingleObject(a->callBackEvent, INFINITE);
      }
      ResetEvent(a->callBackEvent);
      if((a->mmError=waveOutReset(a->waveOut))!=MMSYSERR_NOERROR)
         HError(6006,"CloseAudo: Cannot reset MMAPI output audio device [ERR=%d]",a->mmError);
      RemoveUsedBuffers(a);
      if((a->mmError=waveOutClose( a->waveOut )) != MMSYSERR_NOERROR)
         HError(6006,"CloseAudo: Cannot close MMAPI output audio device [ERR=%d]",a->mmError);
      mmeFreeMem(a->waveFmt);  mmeFreeMem(a->wavePos);
      if (trace & T_TOP) {
         printf(" Closing MMAPI audio output\n");
         fflush(stdout);
      }
   }
#endif

#ifdef UNIX
#ifdef __APPLE__
	/* TOOD stop audioqueue */
#else
   /* Stop PCM device and drop pending frames */
   if (trace & T_TOP) {
     printf(" Closing ALSA audio output\n");
     fflush(stdout);
   }
   snd_pcm_drop(a->pcm_handle);
   snd_pcm_close (a->pcm_handle);
#endif
#endif
}

/* FlushAudo: flush any queued audio output but leave device active */
void FlushAudo(AudioOut a)
{
   a->isActive = FALSE;
#ifdef WIN32
   a->mmError=waveOutReset(a->waveOut);
   if (a->mmError!=MMSYSERR_NOERROR)
      HError(6006,"FlushAudo: Cannot reset MMAPI output audio [ERR=%d]",a->mmError);
   a->total = 0;
   if (trace & T_TOP) {
      printf(" Reseting MMAPI audio output\n");
      fflush(stdout);
   }
#endif
#ifdef UNIX
#ifdef __APPLE__
	/*TODO you know what you have TODO */
#else
   alsaBuf *aBuf;

   HAudioLock();
   aBuf=a->toPlayHead;
   while(aBuf) {
     a->toPlayHead=a->toPlayHead->next;
     if (aBuf->isLast && a->callThreadId != 0) {
       HEventRec r;
       r.event=HAUDOUT;
       postEventToQueue(a->callThreadId,r);
       if(trace&T_AUD)
	 printf("HAudio: flushing, sent last block!\n");
     }
     Dispose(a->mem,aBuf);
     aBuf=a->toPlayHead;
   }
   a->toPlayHead=a->toPlayTail=NULL;
   a->total=0; a->nBlocks=0;
   a->stored=a->played=0;
   HAudioUnlock();
#endif
#endif
}

/* OutSamples: return num samples left to play in output device */
static int OutSamples(AudioOut a)
{
   int total = 0;
   if (!a->isActive) return 0;
#ifdef WIN32
   if (a->current==NULL) return(0);
   a->mmError=waveOutGetPosition( a->waveOut, a->wavePos,sizeof(MMTIME));
   total = a->total - a->wavePos->u.sample;
   if(a->mmError != MMSYSERR_NOERROR)
      HError(6006,"OutSamples: Cannot get current play back position");
#endif
#ifdef UNIX
#ifdef __APPLE__
	/* TODO */
#else
   /* TODO: Check correct get frames*/
   {
     int avail, total;
     snd_pcm_sframes_t delay;
     /* It's positive and less than buffer size in normal situation
	and negative on playback underrun  */
     avail=snd_pcm_avail_update(a->pcm_handle);
     if(avail<0)
       HError(6006, "OutSamples: underflow reported");

     total=a->total-avail;
   }
#endif
#endif
	return total;
}

/* PlayAudio: put nSamples from buf into output queue */
static void PlayAudio(AudioOut a, short *buf, int nSamples, Boolean isLast)
{
#ifdef WIN32
   mmApiBuf *p;
   int i,size;
   short *bufp;

   if (trace & T_OUT) {
      printf(" Putting %d sample block into MMAPI audio output queue",nSamples);
      if (isLast) printf(" - the last"); printf("\n");
   }
   assert(nSamples>0);
   assert(nSamples==WAVEPACKETSIZE || isLast);
   /* first claim back any deleted buffers */
   RemoveUsedBuffers(a);
   /* create new buffer */
   p = GetMMBuf(a);
   p->n=WAVEPACKETSIZE; p->next=NULL;
   /* last output packet might not be full, so pad it with zeros */
   size = WAVEPACKETSIZE*sizeof(short);
   p->waveData = mmeAllocMem(size);
   memcpy(p->waveData,buf,nSamples*sizeof(short));
   if (nSamples<WAVEPACKETSIZE){
      bufp = (short *)p->waveData;
      for (i=nSamples; i<WAVEPACKETSIZE; i++) bufp[i]=0;
   }
   p->isLast = isLast;
   /* Set up header */
   p->waveHdr->lpData = p->waveData;
   p->waveHdr->dwBufferLength = size;
   p->waveHdr->dwBytesRecorded = 0; /* Unused */
   p->waveHdr->dwFlags = 0;  p->waveHdr->dwLoops = 0;
   /* Add new buffer to end of list */
   HAudioLock();
   ++a->nBlocks; a->total+=WAVEPACKETSIZE;
   /* p->waveHdr->dwUser = p->index; */
   a->mmError=waveOutPrepareHeader(a->waveOut,p->waveHdr,sizeof(WAVEHDR));
   if (a->mmError !=MMSYSERR_NOERROR)
      HError(6006,"PlayAudio: MM Prep Header err = %d",a->mmError);
   if (a->pHead==NULL) {
      a->pHead=a->pTail=p;
   } else {
      a->pTail->next=p; a->pTail=p;
   }
   /* And play */
   if (a->current==NULL) a->current=p;
   a->callThreadId=GetCurrentThreadId();
   HAudioUnlock();
   a->mmError=waveOutWrite(a->waveOut, p->waveHdr, sizeof(WAVEHDR));
   if (a->mmError!=MMSYSERR_NOERROR)
      HError(6006,"PlayAudio: MM Write Out err = %d",a->mmError);
#endif

#ifdef UNIX
#ifdef __APPLE__
	/* TODO */
#else
   /* alloc an alsabuf */
   alsaBuf *aBuf;
   int i;
   static int id=0;

   if (trace & T_OUT) {
      printf(" Putting %d sample block on ALSA audio output queue",nSamples);
      if (isLast) printf(" - the last");
      printf("\n");
   }
   assert(nSamples>0);
   assert(nSamples==WAVEPACKETSIZE || isLast);
   aBuf=(alsaBuf*)New(a->mem, sizeof(alsaBuf));
   aBuf->n=WAVEPACKETSIZE;
   aBuf->isLast=isLast;      /* used on output to signal end of utterance */
   for(i=0;i<nSamples;i++)
     aBuf->waveData[i]=buf[i];
   /* last output packet might not be full, so pad it with zeros */
   if (nSamples<WAVEPACKETSIZE){
     for (i=nSamples; i<WAVEPACKETSIZE; i++)
       aBuf->waveData[i]=0;
   }
   aBuf->next=NULL; aBuf->id=id++;
   HAudioLock();
   a->callThreadId=HThreadSelf();
   /* check queue status - if head empty*/
   if(a->toPlayHead==NULL) {
     /* set as head/tail */
     snd_pcm_prepare(a->pcm_handle);
     a->toPlayHead=a->toPlayTail=aBuf;
     /* clear any remaining "now playing" packets? */
     /*a->playingHead=a->playingTail=NULL;*/
     a->nBlocks=a->total=aBuf->n;
     a->stored=a->played=0;
     }
   else {
     a->toPlayTail->next=aBuf;
     a->toPlayTail=aBuf;
     a->nBlocks++; a->total+=aBuf->n;
   }
   alsa_play_from_queue(a);
   HAudioUnlock();
#endif
#endif
   a->isActive = TRUE;
}

/* SetVol: set output play level (0-1) of output device */
static void SetVol(AudioOut a, float volume)
{
   if (a==NULL) return;
   if (volume>1.0) volume = 1.0;
   if (trace&T_OUT) printf("HAudio: setting vol to %f\n",volume);
#ifdef WIN32
   {
      DWORD vol;
      vol=65535*volume;vol=((vol<<16)&0xffff0000)|(vol&0x0000ffff);
      /* Windows lets you set the instance volume */
      if ((a->mmError=waveOutSetVolume(a->waveOut,
                                       (DWORD)vol))!=MMSYSERR_NOERROR)
         HError(6006,"SetVol: Failed to set MMAPI volume [ERR=%d]",a->mmError);
   }
#endif
#ifdef UNIX
#ifdef __APPLE__
	/* TODO */
#else
   {
     int tmp;

     tmp = volume * a->vmax; /*(a->vmax - a->vmin);*/
     if (trace&T_OUT) printf("HAudio: setting vol to %d\n",tmp);
     snd_mixer_selem_set_playback_volume(a->mixer_elem,
					 SND_MIXER_SCHN_FRONT_LEFT, tmp);
     snd_mixer_selem_set_playback_volume(a->mixer_elem,
					 SND_MIXER_SCHN_FRONT_RIGHT, tmp);

   }
#endif
#endif
}

/* ------------------------------------------------------------------- */
/* -------------------- End of Device Dependent Code ----------------- */
/* ------------------------------------------------------------------- */

/* InitAudio: initialise this module */
void InitAudio(void)
{
   int i;
   Boolean b;

   Register(haudio_version);
   numParm = GetConfig("HAUDIO", TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm, "TRACE",&i)) trace = i;
      if (GetConfBool(cParm,numParm, "ECHOCANCEL",&b)) echoCancelling = b;
      if (GetConfInt(cParm,numParm, "ECHOFILTERSIZE",&i)) echoFilterSize = i;
      if (GetConfInt(cParm,numParm, "ECHOFILTERAWIN",&i)) echoFilterAWin = i;
      if (GetConfInt(cParm,numParm, "ECHOMAXDELAY",&i)) echoMaxDelay = i;
   }
   if (trace&T_DMP) {
      dumpf = fopen("dumpfile.wav","wb");
      if (dumpf==NULL)
         HError(999,"Cannot open dumpfile to write");
   }
}

/* ---------------------- Audio Input Routines -------------------- */

/* EXPORT->OpenAudioInput:  Initialise and return an audio stream */
AudioIn OpenAudioInput(HTime *sampPeriod){
   AudioIn a;
   HTime t;

   if (trace&T_TOP)
      printf("HAudio: opening audio input - sampP=%.0f\n",*sampPeriod);
   a = (AudioIn)malloc(sizeof(AudioInRec));
   a->devStat=ADS_INIT;
   if (*sampPeriod <= 0.0  && GetConfFlt(cParm,numParm,"SOURCERATE",&t))
      *sampPeriod = t;
   InitAudi(a,sampPeriod);
   a->sampPeriod = *sampPeriod;
   return a;
}

/* EXPORT->StartAudioInput: start sampling on given stream */
void StartAudioInput(AudioIn a)
{
   if (trace&T_TOP)
      printf("HAudio: starting audio input\n");
   if (a==NULL) HError(6015,"StartAudioInput: null audio device");
   StartAudi(a);
}

/* EXPORT->StopAudioInput: stop sampling on given stream */
void StopAudioInput(AudioIn a)
{
   if (trace&T_TOP)
      printf("HAudio: stopping audio input\n");
   if (a==NULL) HError(6015,"StopAudioInput: null audio device");
   StopAudi(a);
}

/* EXPORT-> CloseAudioInput: terminate and free memory */
void CloseAudioInput(AudioIn a)
{
   if (trace&T_TOP)
      printf("HAudio: closing audio input\n");
   if (a==NULL) HError(6015,"CloseAudioInput: null audio device");
   CloseAudi(a); free(a);
}

/* EXPORT->PacketsInAudio: return num wave packets available from a */
int PacketsInAudio(AudioIn a)
{
  int n;
  n = InSamples(a);
  return n;
}

/* EXPORT->GetCurrentVol: obtain volume of input source range 0->100*/
float GetCurrentVol(AudioIn a)
{
   float v,knee;
   if (a==NULL) return 0.0;
   knee = 10000.0;
   if (a->curVol<knee)
      v = (a->curVol*50)/knee;
   else
      v = 50.0 * (1 + (a->curVol-knee)/(65536.0 - knee));
   if (v<0.0) v = 0.0; else if (v>100.0) v = 100.0;
   return v;
}

/* EXPORT->GetAudio: Get single wave packet from a and store in buf */
void GetAudio(AudioIn a, short *buf)
{
   if (a==NULL) HError(6015,"GetRawAudio: null audio device");
   GetInputBlok(a,buf);
}

/* ---------------------- Audio Output Routines -------------------- */

/* EXPORT->OpenAudioOutput: return an audio output stream for given rate */
AudioOut OpenAudioOutput(HTime *sampPeriod)
{
   AudioOut a;

   if (trace&T_TOP)
      printf("HAudio: opening audio output - sampP=%.0f\n",*sampPeriod);
   a = (AudioOut)malloc(sizeof(AudioOutRec));
   a->vol = -1;
   InitAudo(a,sampPeriod);
   return a;
}

/* EXPORT->PlayAudioOutput: put block of nSamples stored in buf to output queue*/
void PlayAudioOutput(AudioOut a, long nSamples, short *buf,
                     Boolean isLast, AudioIn ain)
{
   if (trace&T_TOP && !a->isActive)
      printf("HAudio: playing new utterance\n");
   if(trace&T_TOP && isLast)
      printf("HAudio: playing last block of utterance\n");
   if (a==NULL) HError(6015,"PlayAudioOutput: null audio device");
   if (a->vol >= 0) SetVol(a,a->vol);
   a->ain = ain;
   PlayAudio(a,buf,nSamples,isLast);
}

/* EXPORT->FlushAudioOutput: kill current audio output */
void FlushAudioOutput(AudioOut a)
{
   if (trace&T_TOP)
      printf("HAudio: killing audio output\n");
   FlushAudo(a);
}

/* EXPORT->CloseAudioOutput: Terminate audio stream a */
void CloseAudioOutput(AudioOut a)
{
   if (trace&T_TOP)
      printf("HAudio: closing audio output\n");
   if (a==NULL) HError(6015,"CloseAudioOutput: null audio device");
   CloseAudo(a);  free(a);
}

/* EXPORT->SetVolume: set volume on audio stream a */
void SetOutVolume(AudioOut a, int volume)
{
   if (a==NULL) HError(6015,"SetVolume: null audio device");
   if (volume<1) volume = 0;
   if (volume>100) volume = 100;
   a->vol = (float) volume / 100.0;
   /* Most machines already log scaled
      a->vol = (volume < 10) ? (float)volume/1000.0 :
      exp (log(10.0)*((float)volume/50.0 - 2.0));
   */
   SetVol(a,a->vol);
}

/* EXPORT->SamplesToPlay: return num samples left to play */
int SamplesToPlay(AudioOut a)
{
   if (a==NULL) HError(6015,"SamplesToPlay: null audio device");
   return OutSamples(a);
}

/* EXPORT->FakeSilenceSample: for padding audio inputs with trailing silence */
int FakeSilenceSample()
{
   float x = RandomValue();
   if (x>0.8) return 1;
   if (x<0.2) return -1;
   return 0;
}

/* ------------------------ End of HAudio.c ------------------------- */
