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
/*      File: HParm.h -    Speech Parameter Input/Output       */
/* ----------------------------------------------------------- */



/* !HVER!HParm: 1.6.0 [SJY 01/06/07] */

#ifndef _HPARM_H_
#define _HPARM_H_

#ifdef __cplusplus
extern "C" {
#endif

enum _BaseParmKind{
      WAVEFORM,            /* Raw speech waveform (handled by HWave) */
      LPC,LPREFC,LPCEPSTRA,LPDELCEP,   /* LP-based Coefficients */
      IREFC,                           /* Ref Coef in 16 bit form */
      MFCC,                            /* Mel-Freq Cepstra */
      FBANK,                           /* Log Filter Bank */
      MELSPEC,                         /* Mel-Freq Spectrum (Linear) */
      USER,                            /* Arbitrary user specified data */
      DISCRETE,                        /* Discrete VQ symbols (shorts) */
      PLP,                             /* Standard PLP coefficients */
      ANON};

typedef short ParmKind;          /* BaseParmKind + Qualifiers */

#define HASENERGY  0100       /* _E log energy included */
#define HASNULLE   0200       /* _N absolute energy suppressed */
#define HASDELTA   0400       /* _D delta coef appended */
#define HASACCS   01000       /* _A acceleration coefs appended */
#define HASCOMPX  02000       /* _C is compressed */
#define HASZEROM  04000       /* _Z zero meaned */
#define HASCRCC  010000       /* _K has CRC check */
#define HASZEROC 020000       /* _0 0'th Cepstra included */
#define HASVQ    040000       /* _V has VQ index attached */
#define HASTHIRD 0100000       /* _T has Delta-Delta-Delta index attached */

#define BASEMASK  077         /* Mask to remove qualifiers */

/*
   An observation contains one or more stream values each of which
   is either a vector of continuous values and/or a single
   discrete symbol.  The discrete vq symbol is included if the
   target kind is DISCRETE or the continuous parameter has the
   HASVQ qualifier. Observations are input via buffers or tables.  A
   buffer is a FIFO structure of potentially infinite length and it is
   always sourced via HAudio.  A table is a random access array of
   observations and it is sourced from a file possibly via HWave.
   Buffers are input only, a table can be input and output.
   Too allow discrete systems to be used directly from continuous
   data the observation also holds a separate parm kind for the
   parm buffer and routines which supply observations use this to
   determine stream widths when the observation kind is DISCRETE.
*/

typedef enum {
   FALSE_dup=FALSE, /*  0 */
   TRUE_dup=TRUE,   /*  1 */
   TRI_UNDEF=-1     /* -1 */
}
TriState;

typedef struct {
   Boolean eSep;         /* Energy is in separate stream */
   short swidth[SMAX];   /* [0]=num streams,[i]=width of stream i */
   ParmKind bk;          /* parm kind of the parm buffer */
   ParmKind pk;          /* parm kind of this obs (bk or DISCRETE) */
   short vq[SMAX];       /* array[1..swidth[0]] of VQ index */
   Vector fv[SMAX];      /* array[1..swidth[0]] of Vector */
} Observation;

/*
   A ParmBuf holds either a static table of parameter frames
   loaded from a file or a potentially infinite sequence
   of frames from an audio source. The key information relating
   to the speech data in a buffer or table can be obtained via
   a BufferInfo Record.  A static table behaves like a stopped
   buffer.
*/

typedef enum {
   PB_INIT,     /* Buffer is initialised and empty */
   PB_CALIBRATING,  /* Buffer is filling in order to calibrate */
   PB_RUNNING,  /* Normal running state */
   PB_STOPPED,  /* Buffer has stopped but not yet empty */
   PB_CLEARED   /* Buffer has been emptied */
} PBStatus;

typedef struct _ParmBuf  *ParmBuf;

typedef struct {
   ParmKind srcPK;            /* Source ParmKind */
   FileFormat srcFF;          /* Source File format */
   HTime srcSampRate;         /* Source Sample Rate */
   int frSize;                /* Number of source samples in each frame */
   int frRate;                /* Number of source samples forward each frame */
   ParmKind tgtPK;            /* Target ParmKind */
   FileFormat tgtFF;          /* Target File format */
   HTime tgtSampRate;         /* Target Sample Rate */
   int tgtVecSize;            /* Size of target vector */
   float spDetSil;            /* Silence level for channel */
   float chPeak;              /* Peak-to-peak input level for channel */
   float spDetSp;             /* Speech level for channel */
   float spDetSNR;            /* Speech/noise ratio for channel */
   float spDetThresh;         /* Silence/speech level threshold */
   float curVol;              /* Volume level of last frame (0.0-100.0dB) */
   char *matTranFN;           /* Matrix transformation name */
}BufferInfo;

/*  External source definition structure */

typedef struct hparmsrcdef *HParmSrcDef;

/* -------------------- Initialisation ------------------ */

ReturnStatus InitParm(void);
/*
   Initialise the module
*/

typedef struct channelinfo *ChannelInfoLink;

/* -------------------- Define External Source --------------*/
HParmSrcDef CreateSrcExt(Ptr xInfo, ParmKind pk, int size, HTime sampPeriod,
                         Ptr (*fOpen)(Ptr xInfo,char *fn,BufferInfo *info),
                         void (*fClose)(Ptr xInfo,Ptr bInfo),
                         void (*fStart)(Ptr xInfo,Ptr bInfo),
                         void (*fStop)(Ptr xInfo,Ptr bInfo),
                         int (*fNumSamp)(Ptr xInfo,Ptr bInfo),
                         int (*fGetData)(Ptr xInfo,Ptr bInfo,int n,Ptr data));
/*
   Create an external source   xInfo = application related data
*/

ChannelInfoLink SetCoderChannel(char *confName);
  /* set up a new coder channel using the config variables preceded by confName: */

void SetChanHMMSet(ChannelInfoLink newChan, Ptr parmhset);

/* ---------------- Buffer Input Routines ------------------ */

ParmBuf OpenBuffer(MemHeap *x, char *fn, HParmSrcDef ext);
ParmBuf OpenChanBuffer(MemHeap *x, char *fn, HParmSrcDef ext, ChannelInfoLink coderChan);
/*
   Open and return a ParmBuf object connected to the current channel.
*/

PBStatus BufferStatus(ParmBuf pbuf);
/*
   Return current status of buffer.
*/

void ResetBuffer(ParmBuf pbuf);
/*
   Reset buffer back to init state
*/

void StartBuffer(ParmBuf pbuf);
/*
   Start and filling the buffer.  If signals have been enabled
   then effect is delayed until first signal is sent.  If
   silence/speech detection is enabled then frames will
   accumulate when speech starts and buffer will stop filling
   when silence is detected.  If silence/speech detection is
   not enabled but signals are, then a second signal will stop
   the filling.  This operation will fail if pbuf status is not
   PB_INIT.
   This operation should now be non-blocking.
*/

void StopBuffer(ParmBuf pbuf);
/*
   Filling the buffer is stopped regardless of whether signals
   and/or silence/speech detection is enabled.  After making
   this call, the pbuf status will change to PB_STOPPED.
   Only when the buffer has been emptied will the status change
   to PB_CLEARED.
*/

void CloseBuffer(ParmBuf pbuf);
/*
   Close the given buffer, close the associated audio stream if
   any and release any associated memory.
*/

Boolean ReadBuffer(ParmBuf pbuf,Observation *o);
/*
   Version used by ATK.  Whilst there is data on the
   input, it will keep pbuf full.  If speech/silence detector enabled
   it will label observations as speech by setting o->vq[0] to 1.
*/

void ResetMeanRec(ParmBuf pbuf);
/*
   Reset the mean record
*/

int FramesNeeded(ParmBuf pbuf);
/*
   return number of frames needed to complete next ReadBuffer call
   if <=0, then next call to ReadBuffer is guaranteed not to block
*/

void CalibrateSilDet(ParmBuf pbuf);
/*
   Force recalibration of speech/silence detector, and return result
*/

void InhibitCalibration(ParmBuf pbuf, Boolean inhibit);
/*
   When inhibit TRUE, prevent calibration
*/

Boolean GetSilDetParms(ParmBuf pbuf, float *sil, float *snr,
		       float *sp, float *thresh);
/*
   Get main sp/sil dectector parameters.  Returns false if not
   set
*/

void ResetIsSpeech(ParmBuf pbuf);
/*
   Force the sp/sil decision back to silence.
*/

void GetBufferInfo(ParmBuf pbuf, BufferInfo *info);
/*
   Get info associated with pbuf.
   Does not block.
*/

/* ----------------- Observation Handling Routines -------------- */

Observation MakeObservation(MemHeap *x, short *swidth,
                            ParmKind pkind, Boolean forceDisc, Boolean eSep);
/*
   Create observation using info in swidth, eSep and pkind
   If forceDisc is true the observation will be DISCRETE but can
   read from a continuous parameter parmbuffer.
*/

void ExplainObservation(Observation *o, int itemsPerLine);
/*
   Explain the structure of given observation by printing
   a template showing component structure
*/

void PrintObservation(int i, Observation *o, int itemsPerLine);
/*
   Print the given observation. If i>0 then print with an index.
*/

void ZeroStreamWidths(int numS, short *swidth);
/*
   Stores numS in swidth[0] and sets remaining components of
   swidth to zero
*/

void  SetStreamWidths(ParmKind pk, int size, short *swidth, Boolean *eSep);
/*
   If swidth has been 'zeroed' by ZeroStreamWidths, then this function
   sets up stream widths in swidth[1] to swidth[S] for number of streams
   S specified in swidth[0]. If eSep then energy is extracted as a
   separate stream.  If swidth[n] is non-zero, then it only eSep is
   set.
*/
/* EXPORT->SyncBuffers: if matrix transformations are used this syncs the two buffers */
Boolean SyncBuffers(ParmBuf pbuf,ParmBuf pbuf2);

void SetParmHMMSet(Ptr hset);
/*
   The prototype  for this should really be
   void SetParmHMMSet(HMMSet *hset);
   However the .h files have an issue with this. A
   cast is performed in the first line of the function.
*/

/* ------------------- Parameter Kind Conversions --------------- */

char *ParmKind2Str(ParmKind kind, char *buf);
ParmKind Str2ParmKind(char *str);
/*
   Convert between ParmKind type & string form.
*/

ParmKind BaseParmKind(ParmKind kind);
Boolean HasEnergy(ParmKind kind);
Boolean HasDelta(ParmKind kind);
Boolean HasNulle(ParmKind kind);
Boolean HasAccs(ParmKind kind);
Boolean HasThird(ParmKind kind);
Boolean HasCompx(ParmKind kind);
Boolean HasCrcc(ParmKind kind);
Boolean HasZerom(ParmKind kind);
Boolean HasZeroc(ParmKind kind);
Boolean HasVQ(ParmKind kind);
/*
   Functions to separate base param kind from qualifiers
*/

Boolean ValidConversion(ParmKind src, ParmKind tgt);
/*
   Checks that src -> tgt conversion is possible
*/

#ifdef __cplusplus
}
#endif

#endif  /* _HPARM_H_ */
/* ------------------------ End of HParm.h ------------------------- */
