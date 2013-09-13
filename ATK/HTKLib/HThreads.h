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
/*      File: HThreads.h -    Interface for ATK Threading      */
/* ----------------------------------------------------------- */


/* !HVER!HThreads: 1.6.0 [SJY 01/06/07] */

#include "HMem.h"

#ifndef _HTHREADS_H_
#define _HTHREADS_H_

/*  ------------------- Define basic types --------------------
   HThread   - thread
   HLock     - mutex for locking critical sections, supports nested calls
   HSignal   - signal for inter-thread event notification
   HPriority - thread priority level

   These are all pointers to a record which holds the underlying OS
   dependent type (HThreadT, HLockT and HSignalT) plus additional
   information for monitoring
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef UNIX
#include <pthread.h>
#endif

typedef struct _HThreadRec *HThread;
typedef struct _HSignalRec *HSignal;
typedef struct _HLockRec   *HLock;

/* -------------- Primitive OS Dependent Thread Types ------------ */

#ifdef WIN32
typedef HANDLE HThreadT;
typedef HANDLE HLockT;
typedef HANDLE HSignalT;
#define TASKTYPE unsigned
#define TASKMOD  _stdcall
#define WM_AUDOUT   9996
#define WM_HREQCLOSE 9997
#define WM_HTBUFFER 9998
#define WM_HTMONUPD 9999
#endif

#ifdef UNIX
typedef pthread_t HThreadT;
typedef struct {                 /* tracks nested calls */
  pthread_mutex_t mutex;             /* basic mutex */
  pthread_t       owner;             /* owner of the lock */
  int             depth;             /* number of nested "EnterSection" calls */
} HLockT;
typedef pthread_cond_t HSignalT;
#define TASKTYPE void *
#define TASKMOD

typedef struct _QEntryRec *QEntry;
typedef struct _QEntryRec{
  QEntry next;
  void *hev;
} QEntryRec;

typedef struct {
  QEntry head;
  QEntry tail;
  pthread_mutex_t mux;
  pthread_cond_t cond;
} EventQueue;

#endif

/*  The routine defining a thread must have the form

        TASKTYPE TASKMOD taskname(void * arg)
	{
           .....
	   HExitThread(status);
	}
*/

/* ------ Internally recorded info for status monitoring --- */

typedef struct{
  int size;           /* num lines in prBuf */
  int used;           /* num lines used in prBuf */
  int inx;            /* next line to write into */
  int outx;           /* next line to print on monitor Display */
  char **buf;         /* Circular list of printf buffer lines */
  int curlen;         /* length of current line */
} HThreadPrBuffer;

typedef enum {
  THREAD_INITIAL,          /* Thread hasnt yet called any sync primitive */
  THREAD_WAITING,          /* Waiting for a lock or signal */
  THREAD_RUNNING,          /* Running asynchronously */
  THREAD_CRITICAL,         /* Running inside a critical section */
  THREAD_STOPPED,          /* Stopped - probably terminated */
  THREAD_STATUS_SIZE
} HThreadStatus;

typedef struct{
  int inLock;              /* True if in lock */
  HLock  lock;             /* Only valid if inLock true */
  int inSignal;            /* True if in signal */
  HSignal signal;          /* Only valid if inSignal true */
  int returnStatus;        /* Return status set by HExitThread */
  HThreadPrBuffer prBuf;   /* Print buffer for printf and thread msgs */
} HThreadInfo;

/* ------------------- The Exported Types ------------------ */

typedef struct _HThreadRec{
  HThreadT thread;         /* the OS primitive */
  char * name;             /* user defined name */
  unsigned int id;         /* thread id */
  HThreadStatus status;    /* thread status */
  HThreadInfo *info;       /* monitor info */
  HThread next;            /* next HThreadRec in list */
  MemHeap gstack;           /* a gstack for the thread */
#ifdef UNIX
  EventQueue xeq;
#endif
} HThreadRec;

typedef struct _HSignalRec{
  HSignalT signal;         /* the OS primitive */
  char * name;             /* user defined name */
  HSignal next;            /* next HSignalRec in list */
} HSignalRec;

typedef struct _HLockRec{
  HLockT lock;             /* the OS primitive */
  char * name;		   /* user defined name */
  HLock next;		   /* next HLockRec in list */
} HLockRec;

typedef enum { HPRIO_HIGH, HPRIO_NORM, HPRIO_LOW } HPriority;

#define MAINPRBUFSIZE 20
#define PRBUFLINESIZE 255

/* ---------------- Basic operations ------------------ */

typedef enum {HT_NOMONITOR, HT_MSGMON } HMonMode;

void InitThreads(HMonMode monMode);
/*
  Initialise - must be called by main before creating any threads
  In the case of HT_MSGMON, WINMSG is sent to the monitor thread each
  time that the status is updated.
*/

HThread HCreateThread(const char *name, int prBufLines, HPriority pr,
                      TASKTYPE (TASKMOD *task)(void *), void *arg);
/*
  Execute task(arg) as a thread with priority p and return thread
  prBufLines sets the size of the tasks printf buffer
*/

void HCreateMonitor(TASKTYPE (TASKMOD *task)(void *), void *arg);
/*
  Create the monitor thread, this thread, is not recorded in the
  monitor status lists.  It runs at highest priority, hence it
  must be event driven  (ie should only use monMode 2 or 4)
*/

void HKillThread(HThread thread);
/*
  Kill the given thread - hard kill
*/

void HKillMonitor(void);
/*
  Kill the monitor thread
*/

void HJoinThread(HThread thread, int *status);
/*
  Join with given thread and fetch its exit status via *status
*/

void HJoinMonitor(void);
/*
  Join with monitor thread
*/

void HExitThread(int status);
/*
  Exit current thread, returning status
*/

HThread HThreadSelf(void);
/*
  Return identity of calling thread
*/

void HDeschedule(void);
/*
  Calling thread offers to deschedule
*/

void HPauseThread(int n);
/*
  Calling thread sleeps for n msecs
*/

int HPostMessage(HThread thread, const char *m);
/*
  Post a message line to thread's printf buffer.
  If thread is NULL or has no buffer, lines are
  posted to global print buffer.
  return num chars posted.
*/

char * HGetMessage(HThread thread, char *buf);
/*
  Get a message line from thread's printf buffer.  If thread
  is NULL, message is taken from global printf buffer.
  Message is copied into buf, and result is pointer to buf.
  If no message lines, returns NULL
*/

void HGlobalLock();
void HGlobalUnlock();
/*
  Lightweight global lock (not registered or monitored)
  Used primarily by APacket for protecting shared packet ops.
*/

void HMemoryLock();
void HMemoryUnlock();
/*
  Lightweight lock (not registered or monitored). Used by HMem.
*/

HLock HCreateLock(const char *name);
void HEnterSection(HLock lock);
void HLeaveSection(HLock lock);
/*
   Critical section mutual exclusion
*/

HSignal HCreateSignal(const char *name);
void HWaitSignal(HSignal signal, HLock lock);
void HSendSignal(HSignal signal);
/*
  Send wait/send signal.  Signal can only be waited on
  inside a critical section guarded by lock
*/

void HBufferEvent(HThread thread, int bufferId);
/*
  Send a buffer update event to given thread, passing bufferId
  via the wParam.  This is used by the ATK Buffer class to signal
  a thread that it should inspect its buffers.  The bufferId is
  assigned arbitrarily by the thread when it registers its interest
  in receiving this event.
*/

void  HSendAllThreadsCloseEvent();
/* send the close signal to all threads currently active */

/* ---------------- Thread Status Information ----------------- */

/*
  These routines provide access to recorded thread/lock/signal
  information.  MonMode must be > 0 to use these.
*/

Boolean StatusUpdated(void);
/*
  Returns true if status has been updated since last access
*/

void ForceStatusUpdate(void);
/*
  Forces a status update - perhaps to release the monitor thread
*/

void AccessStatus(void);
void ReleaseStatusAccess(void);
/*
  One of AccessStatus must be called before any of the routines below.
  Once status access is completed, ReleaseStatusAccess should be called.
*/

int NumThreadRecords(void);
int NumLockRecords(void);
int NumSignalRecords(void);
/*
  Returns number of objects currently allocated.
*/

HThreadRec GetThreadRecord(int n);
HSignalRec GetSignalRecord(int n);
HLockRec   GetLockRecord(int n);
/*
  Returns copy of n'th record (n=1 is first record)
*/

void PrintThreadStatus(const char *title);
/*
  Print list of current thread status records.  Supplied primarily
  for debugging.
*/

#ifdef __cplusplus
}
#endif

#endif  /* _HTHREADS_H_ */

/* ----------------------------  End HThreads.h ----------------------- */
