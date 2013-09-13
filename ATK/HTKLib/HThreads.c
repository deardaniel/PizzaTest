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
/*      File: HThreads.c -    Interface for ATK Threading      */
/* ----------------------------------------------------------- */

/* Linux support by MNS */

char *hthreads_version = "!HVER!HThreads: 1.6.0 [SJY 01/06/07]";

#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HThreads.h"
#include "HGraf.h"
#ifdef UNIX
#include <pthread.h>
#ifdef XGRAFIX
#include <X11/Xlib.h>
#endif
#define eMask ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionHintMask| PointerMotionMask
#endif

static int mode = HT_NOMONITOR;

static HThread threadList = NULL;  /* list of allocated threads */
static HThread mainThread = NULL;  /* the main thread (and end of list) */
static HSignal signalList = NULL;  /* list of allocated signals */
static HLock   lockList = NULL;    /* list of allocated locks */
static int numThreadRecords = 0;   /* count objects allocated */
static int numSignalRecords = 0;
static int numLockRecords   = 0;

static HLockT glock;               /* global lightweight lock */
static HLockT mlock;               /* hmem lock */
static HLockT tlock;               /* lock protecting this module */
static Boolean updated=FALSE;      /* true when updated */
static HThread monThread=NULL;     /* the monitor thread ... */

static char * tsmap[THREAD_STATUS_SIZE] = {
  "Initial", "Waiting", "Running", "Critcal", "Stopped"
};

/* ------------- Internal Management Routines -------------------- */

/* CopyName: allocate a new string and copy s into it */
static char * CopyName(const char *s)
{
  char *t = (char *) malloc(strlen(s)+1);
  strcpy(t,s);
  return t;
}

/* HTError: report an error and die */
static void HTError(const char *msg, int errn)
{
  fprintf(stderr,"*** Thread Error ***\n%s [%d] %s\n\n",msg,errn, strerror(errn));
  exit(1);
}

/* HGLockCreate: create the global lightweight lock */
static void HGLockCreate()
{
  int n;
#ifdef WIN32
  glock = CreateMutex(NULL,FALSE,NULL);
  if (glock == NULL)
    HTError("HGLockCreate: cannot create global lock",GetLastError());
#endif
#ifdef UNIX
  n = pthread_mutex_init(&glock.mutex,NULL);
  if(n!=0)
    HTError("HGLockCreate: cannot create internal lock",n);
#endif
}

/* HGlobalLock: obtain the lightweight global lock */
void HGlobalLock()
{
 int n,rc;
#ifdef WIN32
  rc = WaitForSingleObject(glock,INFINITE);
  if (rc != WAIT_OBJECT_0)
    switch (rc){
    case WAIT_FAILED:
      HTError("HGlobalLock: wait failed reacquiring mutex",
	      GetLastError());
    case WAIT_ABANDONED:
      HTError("HGlobalLock: reaquired mutex was abandoned",
	      GetLastError());
    default:
      HTError("HGlobalLock: unknown error reacquiring mutex",
	      GetLastError());
    }
#endif
#ifdef UNIX
  n = pthread_mutex_lock(&glock.mutex);
  if (n!=0)
    HTError("HGlobalLock: cannot set global lock",n);
#endif
}

/* HGLobalUnlock: release the lightweight global lock */
void HGlobalUnlock()
{
  int n;
#ifdef WIN32
  ReleaseMutex(glock);
#endif
#ifdef UNIX
  n = pthread_mutex_unlock(&glock.mutex);
  if (n!=0)
    HTError("HGlobalUnlock: cannot release global lock",n);
#endif
}

/* HMLockCreate: create the global lightweight lock */
static void HMLockCreate()
{
  int n;
#ifdef WIN32
  mlock = CreateMutex(NULL,FALSE,NULL);
  if (mlock == NULL)
    HTError("HMLockCreate: cannot create memory lock",GetLastError());
#endif
#ifdef UNIX
  n = pthread_mutex_init(&mlock.mutex,NULL);
  if(n!=0)
    HTError("HMLockCreate: cannot create memory lock",n);
#endif
}

/* HMemoryLock: obtain the lightweight global lock */
void HMemoryLock()
{
 int n,rc;
#ifdef WIN32
  rc = WaitForSingleObject(mlock,INFINITE);
  if (rc != WAIT_OBJECT_0)
    switch (rc){
    case WAIT_FAILED:
      HTError("HMemoryLock: wait failed reacquiring mutex",
	      GetLastError());
    case WAIT_ABANDONED:
      HTError("HMemoryLock: reaquired mutex was abandoned",
	      GetLastError());
    default:
      HTError("HMemoryLock: unknown error reacquiring mutex",
	      GetLastError());
    }
#endif
#ifdef UNIX
  n = pthread_mutex_lock(&mlock.mutex);
  if (n!=0)
    HTError("HMemoryLock: cannot set global lock",n);
#endif
}

/* HGLobalUnlock: release the lightweight global lock */
void HMemoryUnlock()
{
  int n;
#ifdef WIN32
  ReleaseMutex(mlock);
#endif
#ifdef UNIX
  n = pthread_mutex_unlock(&mlock.mutex);
  if (n!=0)
    HTError("HMemoryUnlock: cannot release global lock",n);
#endif
}

/* HTCreate: create the internal lock and signal needed for monitoring */
static void HTCreate()
{
  int n;
#ifdef WIN32
  tlock = CreateMutex(NULL,FALSE,NULL);
  if (tlock == NULL)
    HTError("HTCreate: cannot create internal lock",GetLastError());
#endif
#ifdef UNIX
  n = pthread_mutex_init(&tlock.mutex,NULL);
  if(n!=0)
    HTError("HTCreate: cannot create internal lock",n);
#endif
}

/* HTLock: obtain the internal lock */
static void HTLock(void)
{
  int n,rc;
#ifdef WIN32
  rc = WaitForSingleObject(tlock,INFINITE);
  if (rc != WAIT_OBJECT_0)
    switch (rc){
    case WAIT_FAILED:
      HTError("HTLock: wait failed reacquiring mutex",
	      GetLastError());
    case WAIT_ABANDONED:
      HTError("HTLock: reaquired mutex was abandoned",
	      GetLastError());
    default:
      HTError("HTLock: unknown error reacquiring mutex",
	      GetLastError());
    }
#endif
#ifdef UNIX
  n = pthread_mutex_lock(&tlock.mutex);
  if (n!=0)
    HTError("HTLock: cannot set lock",n);
#endif
}

/* HTUnlock: release the internal lock */
static void HTUnlock(void)
{
  int n;
#ifdef WIN32
  ReleaseMutex(tlock);
#endif
#ifdef UNIX
  n = pthread_mutex_unlock(&tlock.mutex);
  if (n!=0)
    HTError("HTUnlock: cannot release lock",n);
#endif
}

/* Send a buffer update event to given thread */
void HBufferEvent(HThread thread, int bufferId)
{
#ifdef WIN32
   if (thread == NULL)
      HTError("HBufferEvent: null thread (bufferID=%d",bufferId);
   if (thread->status < THREAD_STOPPED) {
      while (!PostThreadMessage(thread->id,WM_HTBUFFER,bufferId,0))
         Sleep(10);
   }
#endif
#ifdef UNIX
   /* in Linux the equivalent to the ThreadMessage queue is emulated
   using the XDisplay queues, with a seperate connection to the
   display server being made by each thread and a window for thread
   messages */
   HEventRec r;
   int rc;

   if (thread == NULL)
      HTError("HBufferEvent: null thread (bufferID=%d",bufferId);
   if (thread->status < THREAD_STOPPED) {
      r.event=HTBUFFER;
      r.c =bufferId;
      postEventToQueue(thread, r);
   }
#endif
}

/* HTUpdate: send internal signal */
static void HTUpdate(void)
{
  int n;

  if (mode==HT_MSGMON){
#ifdef WIN32
    if (monThread != NULL){
      while (!PostThreadMessage(monThread->id,WM_HTMONUPD,0,0))
	   Sleep(10);
    }
#endif
#ifdef UNIX
    if (monThread != NULL){
      int rc;
      static int foo=0;
      HEventRec r;
      r.event=HTMONUPD;
      postEventToQueue(monThread, r);
    }
#endif
  }
}

/* GetSelf: return self (internal version) */
static HThread GetSelf(void){
  HThread t;
#ifdef WIN32
  unsigned int id = GetCurrentThreadId();
  for (t=threadList; t!=NULL; t=t->next)
    if (t->id == id) return t;
#endif
#ifdef UNIX
  HThreadT tt = pthread_self();
  for (t=threadList; t!=NULL; t=t->next)
    if (t->thread == tt) return t;
#endif
  HTError("GetSelf: cannot find self",0);
  return NULL;
}

void HSendAllThreadsCloseEvent(void)
{
  HThread t,self;
#ifdef WIN32
  for (t=threadList; t!=NULL; t=t->next) {
     if (t!=mainThread && t!=monThread  && t->status < THREAD_STOPPED) {
        while (!PostThreadMessage(t->id,WM_HREQCLOSE,0,0))
           Sleep(10);
     }
  }
#endif
#ifdef UNIX
  HEventRec r;
  r.event=HWINCLOSE;
  for (t=threadList; t!=NULL; t=t->next) {
     if (t!=mainThread && t!=monThread  && t->status < THREAD_STOPPED)
       postEventToQueue(t, r);
  }
#endif
}

/* InitThreadPrBuf: initialise a printf buffer to given size */
static void InitThreadPrBuf(HThreadPrBuffer *prb, int n)
{
	int i;

	prb->size = n; prb->curlen = 0;
	prb->inx = prb->outx = prb->used = 0;
	prb->buf = (char **)malloc(sizeof(char *) * n);
	for (i=0; i<prb->size; i++){
		prb->buf[i] = (char *)malloc(PRBUFLINESIZE+1);
		prb->buf[i][0] = '\0';
	}
}

/* ---------------- Exported Routines ------------------*/

/* InitThreads: initialise this module */
void InitThreads(HMonMode monMode)
{
  HThread t;
  int rc;
  mode = monMode;
  t = (HThread)malloc(sizeof(HThreadRec));
  t->name = CopyName("main"); t->info = NULL;
  HTCreate();
  HGLockCreate();
  HMLockCreate();
  t->status = THREAD_INITIAL;
  if (mode>HT_NOMONITOR) {
    t->info = (HThreadInfo *)malloc(sizeof(HThreadInfo));
    t->info->inLock = 0;  t->info->inSignal = 0;
    t->info->returnStatus = 0;
	 InitThreadPrBuf(&(t->info->prBuf),MAINPRBUFSIZE);
  }
  t->next = NULL;
  threadList = mainThread = t;
  numThreadRecords = 1;
  CreateHeap(&(t->gstack), "ThreadStack",  MSTAK, 1, 0.0, 100000, ULONG_MAX );
#ifdef WIN32
  t->id = GetCurrentThreadId();
  t->thread = GetCurrentThread();
#endif
#ifdef UNIX
#ifdef XGRAFIX
  XInitThreads();
#endif
  t->id = 0;
  t->thread = pthread_self();
  t->xeq.head=NULL;
  t->xeq.tail=NULL;
  rc = pthread_cond_init(&(t->xeq.cond),NULL);
  if (rc!=0)
    HTError("InitThreads: cant create signal",rc);
  rc= pthread_mutex_init(&(t->xeq.mux),NULL);
  if (rc!=0)
    HTError("InitThreads: cant create mux",rc);
#endif
}

/* HCreateThread: Exec task(arg) as thread with prio p, store thread in *tp */
HThread HCreateThread(const char *name, int prBufLines, HPriority pr,
		      TASKTYPE (TASKMOD *task)(void *), void *arg){
  static unsigned int threadIDCounter = 1;
  HThreadT thread;
  HThread t; int i;
  unsigned int threadID;
#ifdef WIN32
  int winprio;
#endif
#ifdef UNIX
  int rc;
  int scope;
  pthread_mutexattr_t mattr;
  pthread_attr_t attr;
  struct sched_param param;
#endif

  HTLock();
  t = (HThread)malloc(sizeof(HThreadRec));
  t->name = CopyName(name);
  t->status = THREAD_INITIAL;
  if (mode>HT_NOMONITOR){
    t->info = (HThreadInfo *)malloc(sizeof(HThreadInfo));
    t->info->inLock = 0;  t->info->inSignal = 0;
    t->info->returnStatus = 0;
    InitThreadPrBuf(&(t->info->prBuf),prBufLines);
  }
  t->next = threadList; threadList = t; ++numThreadRecords;
#ifdef WIN32
  thread = (HANDLE)_beginthreadex(NULL,0,task,arg,0,&threadID);
  t->thread = thread;
  t->id = threadID;
  if (thread==NULL)
    HTError("HCreateThread: Cannot create thread",GetLastError());
  switch(pr){
  case HPRIO_HIGH:
    winprio = THREAD_PRIORITY_HIGHEST;
    break;
  case HPRIO_LOW:
    winprio = THREAD_PRIORITY_LOWEST;
    break;
  case HPRIO_NORM:
    winprio = THREAD_PRIORITY_NORMAL;
    break;
  default:
    HTError("HCreateThread: Bad priority",pr);
  }
  if (SetThreadPriority(thread,winprio)==0)
    HTError("HCreateThread: cannot set priority",GetLastError());
#endif

#ifdef UNIX
  pthread_attr_init(&attr);
  rc =  pthread_attr_setschedpolicy(&attr,SCHED_OTHER);
  if (rc != 0)
    HTError("HCreateThread: Cannot set policy",rc);
  /*  rc=pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
      if (rc != 0)
      HTError("HCreateThread: Cannot set schedule",rc); */

  switch(pr){
  case HPRIO_HIGH:
    param.sched_priority = sched_get_priority_max(SCHED_OTHER);
    pthread_attr_setschedparam(&attr,&param);
    break;
  case HPRIO_LOW:
    param.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&attr,&param);
    break;
  case HPRIO_NORM:
    break;
  default:
    HTError("HCreateThread: Bad priority" ,pr);
  }

  rc = pthread_create(&thread, &attr, task, arg);

  t->thread = thread;
  if (rc != 0)
    HTError("HCreateThread: Cannot create thread",rc);
  threadID = ++threadIDCounter;
  t->id = threadID;
  t->xeq.head=NULL;
  t->xeq.tail=NULL;
  rc = pthread_cond_init(&(t->xeq.cond),NULL);
  if (rc!=0)
    HTError("HCreateThread: cant create signal",rc);
  rc=pthread_mutex_init(&(t->xeq.mux),NULL);
  if (rc!=0)
    HTError("HCreateThread: cant create mux",rc);
#endif
  updated = TRUE;

  HTUnlock();
  CreateHeap(&(t->gstack), "ThreadStack",  MSTAK, 1, 0.0, 100000, ULONG_MAX );

  if (mode==HT_MSGMON) HTUpdate();
  return t;
}

/* HCreateMonitor: create the monitor thread with max priority */
void HCreateMonitor(TASKTYPE (TASKMOD *task)(void *), void *arg)
{
  monThread = HCreateThread("HT_Monitor",1,HPRIO_HIGH,task,arg);
}

/* HKillThread: kill the given thread */
void HKillThread(HThread thread)
{
#ifdef WIN32
if (!TerminateThread(thread->thread,-1))
  HTError("HKillThread: TerminateThread: failed",GetLastError());
#endif
#ifdef UNIX
  pthread_cancel(thread->thread);
#endif
  HTLock();
  thread->status = THREAD_STOPPED;
  if (mode>HT_NOMONITOR){
    updated = TRUE;
  }
  HTUnlock();
  if (mode==HT_MSGMON) HTUpdate();
}

/* HKillMonitor: kill the monitor */
void HKillMonitor(void)
{
  HKillThread(monThread);
}

/* HExitThread: Exit current thread, returning status */
void HExitThread(int status){
  HThread t;
  int *statusp;

  HTLock();
  t = GetSelf();
  t->status = THREAD_STOPPED;
  if (mode>HT_NOMONITOR){
    t->info->returnStatus = status;
    updated = TRUE;
  }
  HTUnlock();
  if (mode==HT_MSGMON && t != monThread) HTUpdate();

#ifdef WIN32
  ExitThread(status);
#endif

#ifdef UNIX
  statusp = malloc(sizeof(int));
  *statusp = status;
  pthread_exit((void *)statusp);
#endif
}

/* HJoinThread: join with given thread & get its status via *status */
void HJoinThread(HThread thread, int *status){

#ifdef WIN32
  int rc;

  rc = WaitForSingleObject(thread->thread,INFINITE);
  if (rc != WAIT_OBJECT_0)
    switch (rc){
    case WAIT_FAILED:
      HTError("HThreadJoin: wait failed",GetLastError());
    case WAIT_ABANDONED:
      HTError("HThreadJoin: mutex was abandoned",GetLastError());
    default:
      HTError("HThreadJoin: unknown error",GetLastError());
    }
  rc = GetExitCodeThread(thread->thread,status);
  if (!rc)
    HTError("HThreadJoin: cannot retrieve exit code",GetLastError());
#endif
#ifdef UNIX
  int rc;
  int *statusp;

  rc=pthread_join(thread->thread,(void **)&statusp);
  if (rc != 0)
    HTError("HJoinThread: Cannot join thread",rc);
  if(statusp == PTHREAD_CANCELED)
    printf("thread cancelled\n");
  else
    *status = * statusp;
#endif
  if (thread != monThread){
     thread->status = THREAD_STOPPED;
     if (mode>HT_NOMONITOR){
        HTLock();
        updated = TRUE;
        HTUnlock();
        if (mode==HT_MSGMON) HTUpdate();
     }
  }
}

/*  HJoinMonitor: Join with monitor thread */
void HJoinMonitor(void)
{
  int status;
  HJoinThread(monThread,&status);
}

/* HThreadSelf: Return identity of calling thread */
HThread HThreadSelf(void){
  HThread t;

#ifdef WIN32
  unsigned int id = GetCurrentThreadId();

  HTLock();
  for (t=threadList; t!=NULL; t=t->next)
    if (t->id == id) {HTUnlock(); return t;}
  HTError("HThreadSelf: cannot find self",id);
#endif
#ifdef UNIX

  HThreadT tt = pthread_self();

  HTLock();
  for (t=threadList; t!=NULL; t=t->next)
    if (t->thread == tt) {HTUnlock(); return t;}
  HTError("HThreadSelf: cannot find self",0);

#endif
  return NULL;
}

/* HDeschedule: Calling thread offers to deschedule */
void HDeschedule(void){
#ifdef WIN32
  Sleep(0);
#endif
#ifdef UNIX
  sched_yield();
#endif
}

/* HPauseThread: Calling thread sleeps for n msecs */
void HPauseThread(int n){
#ifdef WIN32
  Sleep(n);
#endif
#ifdef UNIX
  struct timespec t;
  struct timespec rem;
  int rc;

  if (n>=1000) {
    t.tv_sec = n/1000; n -= t.tv_sec*1000;
  }
  else
    t.tv_sec = 0;

  t.tv_nsec = n*1000000;

  rc = nanosleep(&t,&rem);
  /*  rc=usleep(n*1000);*/
  if (rc < 0 )
    if (errno == EINTR )
      HTError("HPauseThread: process recieved interrupt",rc);
    else
      HTError("HPauseThread: error in specified time interval",rc);
#endif
}

/* HPostMessage: post m to threads printf buffer */
int HPostMessage(HThread thread, const char *m)
{
   HThreadPrBuffer *prb;
   int i,n;
   char c,*pp;

   if (mode==HT_NOMONITOR) return 0;

   /* get the appropriate print buffer into prb */
   HTLock();
   if (thread==NULL) thread = mainThread;
   assert(thread->info != NULL);
   prb = &(thread->info->prBuf);
   /* copy m char by char starting newline after each \n in m */
   i=0; n = strlen(m);
   pp = prb->buf[prb->inx];
   while (prb->curlen<PRBUFLINESIZE &&  i<n) {
      c = m[i++];
      if (c != '\n'  && prb->curlen<PRBUFLINESIZE) {
         pp[prb->curlen++] = c;
      } else {
         pp[prb->curlen] = '\0';  prb->curlen = 0;
         if (++prb->inx==prb->size) prb->inx = 0;
         ++prb->used;
         pp = prb->buf[prb->inx];
      }
   }
   pp[prb->curlen] = '\0';
   /* check and remove existing lines overwritten */
   while (prb->used>prb->size){
      --prb->used; ++prb->outx;
      if (prb->outx==prb->size)
         prb->outx = 0;
   }
   updated = TRUE;
   HTUnlock();
   if (mode==HT_MSGMON) HTUpdate();
   return n;
}

/* HGetMessage: get a message from printf buffer */
char * HGetMessage(HThread thread, char *buf)
{
   HThreadPrBuffer *prb;
   if (mode==HT_NOMONITOR) return NULL;
   if (thread==NULL) thread = mainThread;
   assert(thread->info != NULL);
   prb = &(thread->info->prBuf);
   if (prb->used==0) return NULL;
   HTLock();
   strcpy(buf,prb->buf[prb->outx++]);
   if (prb->outx==prb->size) prb->outx = 0;
   --prb->used;
   HTUnlock();
   return buf;
}

/* HCreateLock: create and return a mutex lock */
HLock HCreateLock(const char *name){
  HLockT lock;
  HLock l;

#ifdef WIN32
  lock = CreateMutex(NULL,FALSE,NULL);
  if (lock == NULL)
    HTError("HCreateLock: cannot create mutex",GetLastError());
#endif
#ifdef UNIX
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&(lock.mutex),&attr);
  lock.owner = 0;
  lock.depth = 0;
#endif
  l = (HLock)malloc(sizeof(HLockRec));
  l->lock = lock;
  l->name = CopyName(name);
  HTLock();
  l->next = lockList; lockList = l; ++numLockRecords;
  HTUnlock();
  return l;
}

/* HEnterSection: enter a critical sections */
void HEnterSection(HLock lock){
   HThread t;
   int rc;

   HTLock();
   t = GetSelf();
   t->status = THREAD_WAITING;
   if (mode>HT_NOMONITOR){
      t->info->inLock = 1; t->info->lock = lock;
      updated = TRUE;
   }
   HTUnlock();
   /* if (mode==HT_MSGMON) HTUpdate();*/
#ifdef WIN32
   rc = WaitForSingleObject(lock->lock,INFINITE);
   if (rc != WAIT_OBJECT_0)
      switch (rc){
         case WAIT_FAILED:
            HTError("HEnterSection: wait failed",GetLastError());
         case WAIT_ABANDONED:
            HTError("HEnterSection: mutex was abandoned",GetLastError());
         default:
            HTError("HEnterSection: unknown error",GetLastError());
      }
#endif

#ifdef UNIX
  rc = pthread_mutex_lock(&lock->lock.mutex);
  if (rc!=0)
    HTError("HEnterSection: wait failed",rc);
  lock->lock.owner=pthread_self();
#endif
  HTLock();
  t->status = THREAD_CRITICAL;
  if (mode>HT_NOMONITOR){
     updated = TRUE;
  }
  HTUnlock();
  /* if (mode==HT_MSGMON) HTUpdate();*/
}

/* HLeaveSection: leave a critical sections */
void HLeaveSection(HLock lock){
  HThread t;

#ifdef WIN32
  ReleaseMutex(lock->lock);
#endif

#ifdef UNIX
  /*  if (pthread_self()!=lock->lock.owner)
    HTError("HLeaveSection: invalid thread owner",0);
   if (--lock->lock.depth == 0) */
  pthread_mutex_unlock(&lock->lock.mutex);
#endif

  HTLock();
  t = GetSelf();
  t->status = THREAD_RUNNING;
  if (mode>HT_NOMONITOR){
     t->info->inLock = 0;
     updated = TRUE;
  }
  HTUnlock();
  /* if (mode==HT_MSGMON) HTUpdate();*/
}

/* HCreateSignal: create and return a signal */
HSignal HCreateSignal(const char *name)
{
  HSignalT signal;
  HSignal s;
  int rc;

#ifdef WIN32
  signal = CreateEvent(NULL,FALSE,FALSE,NULL);
  if (signal==NULL)
    HTError("HCreateSignal: cannot create event",GetLastError());
#endif

#ifdef UNIX
  rc = pthread_cond_init(&signal,NULL);
  if (rc!=0)
    HTError("HCreateSignal: cant create",rc);
#endif
  s = (HSignal)malloc(sizeof(HSignalRec));
  s->signal = signal;
  s->name = CopyName(name);
  HTLock();
  s->next = signalList; signalList = s; ++numSignalRecords;
  HTUnlock();
  return s;
}

/* HWaitSignal: wait for signal inside lock-ed section */
void HWaitSignal(HSignal signal, HLock lock){
  HThread t;
  int rc;


  HTLock();
  t = GetSelf();
  t->status = THREAD_WAITING;
  if (mode>HT_NOMONITOR){
     t->info->inSignal = 1; t->info->signal = signal;
     if (!t->info->inLock || lock != t->info->lock)
        HTError("HWaitSignal: lock conflict",0);
     updated = TRUE;
  }
  HTUnlock();
  if (mode==HT_MSGMON) HTUpdate();

#ifdef WIN32
  ReleaseMutex(lock->lock);
  rc = WaitForSingleObject(signal->signal,INFINITE);
  if (rc != WAIT_OBJECT_0)
    switch (rc){
    case WAIT_FAILED:
      HTError("HWaitSignal: wait on signal failed",GetLastError());
    case WAIT_ABANDONED:
      HTError("HWaitSignal: signal was abandoned",GetLastError());
    default:
      HTError("HWaitSignal: unknown error waiting for signal",GetLastError());
    }
  rc = WaitForSingleObject(lock->lock,INFINITE);
  if (rc != WAIT_OBJECT_0)
    switch (rc){
    case WAIT_FAILED:
      HTError("HWaitSignal: wait failed reacquiring mutex",GetLastError());
    case WAIT_ABANDONED:
      HTError("HWaitSignal: reaquired mutex was abandoned",GetLastError());
    default:
      HTError("HWaitSignal: unknown error reacquiring mutex",GetLastError());
    }
#endif

#ifdef UNIX
  /* cond_wait unlocks the mutex,
     then reaquires it once the condition is passed */
  rc = pthread_cond_wait(&signal->signal,&lock->lock.mutex);
  if (rc !=0)
    HTError("HWaitSignal: cant execute wait on signal",rc);
#endif


  HTLock();
  t->status = THREAD_CRITICAL;
  if (mode>HT_NOMONITOR){

     t->info->inSignal = 0;
     updated = TRUE;
  }
  HTUnlock();
  /* if (mode==HT_MSGMON) HTUpdate();*/
}

/* HSendSignal: send signal */
void HSendSignal(HSignal signal){
  int rc;
#ifdef WIN32
  if (SetEvent(signal->signal)==FALSE)
    HTError("HSendSignal: cannot set WIN32 event",GetLastError());
#endif

#ifdef UNIX
  rc = pthread_cond_signal(&(signal->signal));
  if (rc!=0)
    HTError("HSendSignal: cant send signal",rc);
#endif
}

/* ------------------------- Thread Status Recorder ----------------------- */

/* CheckMode: raise error if monitor level incorrect */
static void CheckMode(char *op, Boolean ok)
{
  char buf[256];

  if (!ok) {
    strcpy(buf,"Monitor mode does not support: ");
    strcat(buf,op);
    HTError(buf,mode);
  }
}

/* StatusUpdated: true if status has been updated */
Boolean StatusUpdated(void)
{
  Boolean upd;
  CheckMode("StatusUpdated", mode>HT_NOMONITOR);
  HTLock(); upd = updated; HTUnlock();
  return upd;
}

/* ForceStatusUpdate: usually to release the monitor thread */
void ForceStatusUpdate(void)
{
  CheckMode("ForceStatusUpdate", mode>HT_NOMONITOR);
  HTLock(); updated = TRUE; HTUnlock();
  if (mode==HT_MSGMON) HTUpdate();
}

/* AccessStatus: get tlock immediately */
void AccessStatus(void)
{
  CheckMode("AccessStatusImmediate", mode>HT_NOMONITOR);
  HTLock();
}

/* ReleaseStatusAccess: release the tlock */
void ReleaseStatusAccess(void)
{
  CheckMode("ReleaseStatusAccess", mode>HT_NOMONITOR);
  updated = FALSE;
  HTUnlock();
}

/* NumXXXRecords: return number of object records */
int NumThreadRecords(void){return numThreadRecords; }
int NumSignalRecords(void){return numSignalRecords; }
int NumLockRecords(void){return numLockRecords; }

/* GetThreadRecord: return copy of n'th record */
HThreadRec GetThreadRecord(int n)
{
  HThread p;
  int i = n;

  for (p=threadList; p!=NULL && i>1; p = p->next)--i;
  if (p==NULL) HTError("GetThreadRecord: Cannot access %d'th record",n);
  return *p;
}

/* GetSignalRecord: return copy of n'th record */
HSignalRec GetSignalRecord(int n)
{
  HSignal p;
  int i = n;

  for (p=signalList; p!=NULL && i>1; p = p->next)--i;
  if (p==NULL)
    HTError("GetSignalRecord: Cannot access record",n);
  return *p;
}

/* GetLockRecord: return copy of n'th record */
HLockRec GetLockRecord(int n)
{
  HLock p;
  int i = n;

  for (p=lockList; p!=NULL && i>1; p = p->next)--i;
  if (p==NULL)
    HTError("GetLockRecord: Cannot access record",n);
  return *p;
}

/* PrintThreadStatus: print list of current thread records */
void PrintThreadStatus(const char *title)
{
  HThread p;
  HSignal s;
  HLock l;

  CheckMode("PrintThreadStatus", mode>HT_NOMONITOR);
  printf("\nThread Status: %s\n",title);
  printf("%12s %6s %8s  %10s %10s\n","Thread","ID","Status","Lock","Signal");
  for (p=threadList; p!=NULL; p = p->next){
    printf("%12s %6d %8s  %10s %10s\n",
	   p->name,p->id,tsmap[p->status],
	   p->info->inLock?p->info->lock->name:"-",
	   p->info->inSignal?p->info->signal->name:"-"
	   );

  }
  printf("%2d Signals:",numSignalRecords);
  for (s=signalList; s!=NULL; s = s->next)printf(" %s",s->name);
  printf("\n");
  printf("%2d Locks:  ",numLockRecords);
  for (l=lockList; l!=NULL; l = l->next)printf(" %s",l->name);
  printf("\n\n"); fflush(stdout);
}


/* ----------------------------  End HThread.c ----------------------- */
