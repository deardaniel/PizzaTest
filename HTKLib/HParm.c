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
/*          2000-2007 Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*    File: HParm.c -   Speech Parameter File Input/Output     */
/* ----------------------------------------------------------- */

/* Added the HTK_V3.3 functionality
   - the arbitrary input transform
   - added multiple coder support  MNS 01/03/05
*/


char * hparm_version="!HVER!HParm: 1.6.0 [SJY 01/06/07]";

#include <assert.h>
#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HSigP.h"
#include "HAudio.h"
#include "HWave.h"
#include "HVQ.h"
#include "HParm.h"
#include "HLabel.h"
#include "HModel.h"
#ifdef UNIX
#include <sys/ioctl.h>
#endif

/* ----------------------------- Trace Flags ------------------------- */

static int trace = 0;
#define T_TOP  0001     /* Top Level tracing */
#define T_BUF  0002     /* Buffer operations */
#define T_CPX  0004     /* Compression/Decompression */
#define T_PBS  0010     /* Buffer status */
#define T_QUA  0020     /* Qualifier operations */
#define T_OBS  0040     /* Observation extraction */
#define T_DET  0100     /* Silence detector operation */
#define T_BFX  0200     /* Very detailed buffer tracing */
#define T_CAL  0400     /* silence detector calibration */
#define T_MAT  0200     /* Matrix operations */


/* --------------------- Global Variables ------------------- */

static Boolean natWriteOrder = FALSE; /* Preserve natural write byte order*/
extern Boolean vaxOrder;              /* true if byteswapping needed to
                                               preserve SUNSO */

static Boolean highDiff = FALSE;   /* compute higher oder differentials, only up to fourth */
static ParmKind ForcePKind = ANON; /* force to output a customized parm kind to make older versions
                                    happy for all the parm kind types supported here */
static HMMSet *hset = NULL;        /* hmmset to be used for frontend */


/* ------------------------------------------------------------------- */
/*
   Parameter layout in tables/buffers is

      Static [C0] [E]  Deltas Accs

   _N option is ignored everywhere except when copying from buffer
   or table into an observation (ie in ExtractObservation) and in
   GetBufferInfo() which returns the observation vector size in
   tgtvecSize taking into account _N.
   When _0 is used alone it behaves exactly like _E.  When _0_E,
   C0 is placed immediately before energy and in this case deltas
   are not allowed.
*/

/* ----------------- Configuration Information ----------------- */

/*
   An IOConfig record specifies the mapping from the source
   to the target parameterisation.  Its built in defaults
   can be overridden using configuration parameters.
*/

typedef enum { FFTbased, LPCbased, VQbased} CodeStyle;

typedef struct {
   /* ------- Overrideable parameters ------- */
   ParmKind srcPK;            /* Source ParmKind */
   FileFormat srcFF;          /* Source File format */
   HTime srcSampRate;         /* Source Sample Rate */
   Boolean zMeanSrc;          /* Zero Mean the Source */
   ParmKind tgtPK;            /* Target ParmKind */
   FileFormat tgtFF;          /* Target File format */
   HTime tgtSampRate;         /* Target Sample Rate */
   Boolean saveCompressed;    /* If LPREFC save as IREFC else _C */
   Boolean saveWithCRC;       /* Append check sum on save */
   HTime winDur;              /* Source window duration */
   Boolean useHam;            /* Use Hamming Window */
   float preEmph;             /* PreEmphasis Coef */
   Boolean usePower;          /* Use power instead of Magnitude */
   int numChans;              /* Number of filter bank channels */
   float loFBankFreq;         /* Fbank lo frequency cut-off */
   float hiFBankFreq;         /* Fbank hi frequency cut-off */
   float warpFreq;            /* Warp freq axis for vocal tract normalisation */
   float warpLowerCutOff;     /* lower and upper threshold frequencies */
   float warpUpperCutOff;     /*   for linear frequency warping */
   int lpcOrder;              /* Order of lpc analysis */
   float compressFact;        /* Compression factor for PLP */
   int cepLifter;             /* Cepstral liftering coef */
   int numCepCoef;            /* Number of cepstral coef */
   float cepScale;            /* Scaling factor to avoid arithmetic problems */
   Boolean rawEnergy;         /* Use raw energy before preEmp and ham */
   Boolean eNormalise;        /* Normalise log energy */
   float eScale;              /* Energy scale factor */
   float silFloor;            /* Silence floor in dBs */
   int delWin;                /* Delta window halfsize */
   int accWin;                /* Accel window halfsize */
   Boolean simpleDiffs;       /* Use simple differences for delta calcs */
   /* Silence detector parameters */
   float silDiscard;          /* Calibrate discard level */
   float spThresh;            /* Speech Threshold (in dB above sil level) */
   int spcSeqCount;           /* Number of frames for speech window */
   int spcGlchCount;          /*   of spc in sil acceptable as glitches */
   int silGlchCount;          /*   of sil in spc acceptable as glitches */
   int silSeqCount;           /*   of silence before stopping */
   int maxSpcFrames;          /* max number of speech frames before forcing sil */
   int calWindow;             /* Num frames needed for calibrating spDet */
   int calPeriod;             /* recalibration period, 0 = no recalibration */
   float silUpdateRate;       /* k, where new sil level = curSil*k + oldSil*(1-k); */
   float initialSil;          /* initial silence when silUpdateRate < 1.0 */
   Boolean enableInhibit;     /* enable calibration inhibit when asource is talking */
   /* Misc */
   char *vqTabFN;             /* Name of VQ Table Defn File */
   float addDither;           /* Additional dither added to file */
   Boolean doubleFFT;         /* use twice the required FFT size */
   float cmnTConst;           /* cep mean normalisation Time Constant */
   Boolean cmnResetOnStop;    /* Reset cmn when parmbuffer is reset */
   int cmnMinFrames;          /* min frames before running mean is used */
   char *cmnDefault;          /* Name of Cepstral Mean Vector File */
   char *MatTranFN;           /* points to the file name string */
   Matrix MatTran;            /* Stores transformation matrix */
   int thirdWin;              /* Accel window halfsize */
   int fourthWin;             /* Fourth order differential halfsize */
   Boolean v1Compat;          /* V1 compatibility mode */
   VQTable vqTab;             /* VQ table */

   /* ------- Internally derived parameters ------- */
   /*  These values are allocated in the IOConfigRec but are really */
   /*  specific to each pbuf and do not rely on any kind of initialisation */
   /* Following 3 variables always reflect the actual state of */
   /* the associated data which may be intermediate between src and tgt */
   ParmKind curPK;    /* Used to track conversion from srcPK to tgtPK */
   ParmKind unqPK;    /* Used to track conversion from srcPK to tgtPK */
   int nUsed;         /* num columns used in each row of the parm block */
   /* The next two are static buffer sizes */
   int nCols;         /* num columns in each row of the parameter block */
   int nCvrt;         /* num columns produced from coding */
   /* sizes of source and target */
   long nSamples;     /* num samples in original (WAVEFORM only) */
   int tgtUsed;       /* num columns which will be used once converted */
   /* Working storage needed for conversions, etc */
   CodeStyle style;   /* style encoding */
   int frSize;        /* Total number of waveform samples in frame */
   int frRate;        /* Number of waveform samples advanced each frame */
   Vector s;          /* speech vector */
   ShortVec r;        /* raw speech vector */
   char *rawBuffer;   /* buffer for external data */
   float curVol;      /* current volume dB (0.0-100.0) */
   Vector a,k;        /* lpc and refc vectors */
   Vector fbank;      /* filterbank vector */
   Vector c;          /* cepstral vector */
   Vector as, ac, lp; /* Auditory, autocorrelation an lp vectors for PLP */
   Vector eql;        /* Equal loundness curve */
   DMatrix cm;        /* Cosine matrix for IDFT */
   FBankInfo fbInfo;  /* FBank info used for filterbank analysis */
   MeanRec mean;      /* Cepstral Mean information */

   /* Running stuff */
   Source src;        /* Source to read HParm file from */
   Boolean bSwap;     /* TRUE if source needs byte swapping */
   unsigned short crcc; /* Running CRCC */
   Vector A;          /* Parameters for decompressing */
   Vector B;          /*  HTK parameterised files */
   ParmKind matPK;
   int preFrames;
   int postFrames;
   Boolean preQual;
   InputXForm *xform;
   HMMSet *cfhset;   /* tagged in hset info for LoadMat only!*/
}IOConfigRec;

typedef IOConfigRec *IOConfig;

typedef enum {
   /* Source characteristics */
   SOURCEKIND,    /* ParmKind */
   SOURCEFORMAT,  /* FileFormat */
   SOURCERATE,    /* Source sample rate in 100ns */
   ZMEANSOURCE,   /* Zero Mean (Wave only) */
   /* Target characteristics */
   TARGETKIND,    /* ParmKind */
   TARGETFORMAT,  /* FileFormat */
   TARGETRATE,    /* Target sample rate in 100ns */
   SAVECOMPRESSED,/* Save output files in compressed form */
   SAVEWITHCRC,   /* Add crc check to output files */
   /* Waveform Analysis */
   WINDOWSIZE,    /* Window size in 100ns */
   USEHAMMING,    /* Apply Hamming Window */
   PREEMCOEF,     /* Preemphasis Coefficient */
   /* Filterbank Analysis */
   USEPOWER,      /* Use power instead of magnitude */
   NUMCHANS,      /* Num filterbank channels */
   LOFREQ,        /* Lo Fbank frequency */
   HIFREQ,        /* Hi Fbank frequency */
   WARPFREQ,      /* Vocal tract length compensation by frequency warping */
   WARPLCUTOFF,   /* VTL warping cutoff frequencies for smoothing */
   WARPUCUTOFF,
   /* LPC Analysis and Conversion */
   LPCORDER,      /* LPC order */
   COMPRESSFACT,  /* Compression Factor fo PLP */
   /* Cepstral Conversion */
   CEPLIFTER,     /* Cepstral liftering coefficient */
   NUMCEPS,       /* Num cepstral coefficients */
   CEPSCALE,      /* Scale factor to prevent arithmetic errors */
   /* Energy Computation */
   RAWENERGY,     /* Use raw energy */
   ENORMALISE,    /* Normalise log energy */
   ESCALE,        /* Log energy scale factor */
   SILFLOOR,      /* Silence floor in dBs */
   /* Regression Coefficients */
   DELTAWINDOW,   /* Window size for 1st diffs */
   ACCWINDOW,     /* Window size for 2nd diffs */
   SIMPLEDIFFS,   /* Use simple differences */

   /* Silence Detector */
   SILDISCARD,    /* Energy below which frames discarded when calibrating */
   SPEECHTHRESH,  /* Speech detector threshold */
   SPCSEQCOUNT,   /* Speech sequence count */
   SPCGLCHCOUNT,  /* Speech glitch count */
   SILGLCHCOUNT,  /* Silence glitch count */
   SILSEQCOUNT,   /* Silence sequence count */
   MAXSPCFRAMES,  /* Max number of speech frames before forcing sil */
   CALWINDOW,     /* Window for calibrating silence detector */
   CALPERIOD,     /* Recalibration period */
   SILUPDATERATE, /* silence update factor (0 to 1.0) */
   INITIALSIL,    /* initial silence estimate in dB */
   ENABLEINHIBIT, /* enable calibration inhibit when asource is talking */
   /* Vector Quantisation */
   VQTABLE,       /* Name of file holding VQ table */
   ADDDITHER,     /* Amount of additional dither added to file */
   DOUBLEFFT,     /* Use twice the required FFT size */
   /* Dynamic cepstral mean normalisation */
   CMNTCONST,     /* time constant */
   CMNRESETONSTOP, /* enable reset when parmbuffer reset */
   CMNMINFRAMES,  /* min frames before updating default */
   CMNDEFAULT,    /* file name of default mean vec */
  /* MatTran file */
   MATTRANFN,     /* File name for MatTran file */
   MATTRAN,
  /* Extended Deltas */
   THIRDWINDOW,
   FOURTHWINDOW,
   V1COMPAT,      /* Set Version 1 compatibility mode */
   CFGSIZE
}IOConfParm;

static char * ioConfName[CFGSIZE] = {
   "SOURCEKIND", "SOURCEFORMAT", "SOURCERATE", "ZMEANSOURCE",
   "TARGETKIND", "TARGETFORMAT", "TARGETRATE",
   "SAVECOMPRESSED", "SAVEWITHCRC",
   "WINDOWSIZE", "USEHAMMING", "PREEMCOEF",
   "USEPOWER", "NUMCHANS", "LOFREQ", "HIFREQ",
   "WARPFREQ", "WARPLCUTOFF", "WARPUCUTOFF",
   "LPCORDER",   "COMPRESSFACT",
   "CEPLIFTER", "NUMCEPS",  "CEPSCALE",
   "RAWENERGY","ENORMALISE", "ESCALE", "SILFLOOR",
   "DELTAWINDOW", "ACCWINDOW", "SIMPLEDIFFS",
   "SILDISCARD", "SPEECHTHRESH",
   "SPCSEQCOUNT", "SPCGLCHCOUNT", "SILGLCHCOUNT", "SILSEQCOUNT",
   "MAXSPCFRAMES","CALWINDOW",
   "CALPERIOD", "SILUPDATERATE", "INITIALSIL", "ENABLEINHIBIT",
   "VQTABLE", "ADDDITHER", "DOUBLEFFT",
   "CMNTCONST", "CMNRESETONSTOP", "CMNMINFRAMES", "CMNDEFAULT",
   "MATTRANFN", "MATTRAN", "THIRDWINDOW", "FOURTHWINDOW",
   "V1COMPAT",
};

/* -------------------  Default Configuration Values ---------------------- */

static const IOConfigRec defConf = {
   ANON, HTK, 0.0, FALSE, /* SOURCEKIND SOURCEFORMAT SOURCERATE ZMEANSOURCE */
   ANON, HTK, 0.0,        /* TARGETKIND TARGETFORMAT TARGETRATE */
   FALSE, TRUE,           /* SAVECOMPRESSED SAVEWITHCRC */
   256000.0, TRUE, 0.97,  /* WINDOWSIZE USEHAMMING PREEMCOEF */
   FALSE, 20, -1.0, -1.0, /* USEPOWER NUMCHANS LOFREQ HIFREQ */
   1.0, 0.0, 0.0,         /* WARPFREQ WARPLCUTOFF WARPUCUTOFF */
   12, 0.33,              /* LPCORDER COMPRESSFACT */
   22, 12, 1.0,           /* CEPLIFTER NUMCEPS CEPSCALE */
   TRUE, TRUE, 0.1, 50.0, /* RAWENERGY ENORMALISE ESCALE SILFLOOR */
   2, 2, FALSE,           /* DELTAWINDOW ACCWINDOW SIMPLEDIFFS */
   0.0, 9.0,              /* SILDISCARD SPEECHTHRESH */
   10,0,2,100,            /* SPCSEQCOUNT SPCGLCHCOUNT SILGLCHCOUNT SILSEQCOUNT */
   0,40,                  /* MAXSPCFRAMES, CALWINDOW */
   0,1.0,35.0,FALSE,      /* CALPERIOD, SILUPDATERATE, INITIALSIL, ENABLEINHIBIT */
   NULL, 0.0, FALSE,      /* VQTABLE ADDDITHER DOUBLEFFT */
   0.995,TRUE,12,NULL,    /* CMNTCONST CMNRESETONSTOP CMNMINFRAMES CMNDEFAULT */
   NULL, NULL, 2, 2,      /* MATTRANFN, MATTRAN THIRDWIN FOURTHWIN */
   FALSE,                 /* V1COMPAT */
};

/* ------------------------- Buffer Definition  ------------------------*/

/* HParm can deal with multiple channels (eg Audio*N/Files/RFE) */
/*  Each channel can have its own setup and preserved information */
typedef struct channelinfo {
   char *confName;        /* Configuration name associated with mean */
   int fCnt;              /* Number of files processed for this channel */
   int sCnt;              /* Number of files processed in current session */
   int oCnt;              /* Number of observations processed in session */
   Boolean spDetParmsSet; /* Speech detector parameters set */
   Boolean calInhibit;    /* inhibit calibration */
   float frMin;           /* Measured minimum frame energy for channel (dB) */
   float spDetSil;        /* Measured/set silence level for channel (dB) */
   float spDetThresh;     /* Measured/set speech/silence threshold (dB) */
   float spDetSp;         /* Measured/set speech level for channel (dB) */
   float frMax;           /* Measured maximum frame energy for channel (dB) */
   float spDetSNR;        /* Measured/set silence/speech ratio (dB) */
   IOConfigRec cf;        /* Channel configuration */
   struct channelinfo *next;  /* Next channel record */
}
ChannelInfo;

typedef struct hparmsrcdef {
   Ptr xInfo;         /* Application data */
   ParmKind pk;       /* Type of source - split into parmKind and */
   int size;          /* Sample size fields */
   HTime sampPeriod;  /* Either 0.0 or the fixed sample rate of source */

   Ptr (*fOpen)(Ptr xInfo,char *fn,BufferInfo *info);  /* Open new buffer */
   /*
      Return: Pointer to buffer specific data

      Connect to source and allocate necessary structures.
      Each buffer is associated with a specific pointer that is assigned
      to the return value of this function.  All other buffer operations
      are passed this pointer.  Typically it will be used to access a
      source specific data structure containing the necessary information
      for operating the source.
   */

   void (*fClose)(Ptr xInfo,Ptr bInfo);  /* Close buffer and free resources */
   /*
      Ptr bInfo: Pointer returned by fOpen for this buffer

      Free all the resources associated with the buffer (including if
      necessary the info block itself).
   */

   void (*fStart)(Ptr xInfo,Ptr bInfo);  /* Start data capture for real-time sources */
   /*
      Ptr bInfo: Pointer returned by fOpen for this buffer

      Start data capture.  Offline sources can ignore this call.
   */

   void (*fStop)(Ptr xInfo,Ptr bInfo);  /* Stop data capture for real-time sources */
   /*
      Ptr bInfo: Pointer returned by fOpen for this buffer

      Stop data capture.  Offline sources can ignore this call.
   */

   int (*fNumSamp)(Ptr xInfo,Ptr bInfo);  /* Query samples */
   /*
      Ptr bInfo: Pointer returned by fOpen for this buffer
      Return:   Samples readable
      If there are n pending samples, return n.
      If there are no pending samples, but audio src still active return 1
      If audio source stopped, and no pending samples return 0
   */

   int (*fGetData)(Ptr xInfo,Ptr bInfo,int n,Ptr data);  /* Read samples */
   /*
      Ptr bInfo: Pointer returned by fOpen for this buffer
      int n:    Number of samples required
      Ptr data: Buffer for returned samples
      Return:   Samples read correctly
   */
} HParmSrcDefRec;

#define MAX_INT 536870911 /* Don't use INT_MAX cos get numeric overflow */

typedef struct _ParmBuf {
   MemHeap *mem;       /* Memory heap for this parm buf */
   PBStatus status;    /* status of this buffer */
   ChannelInfo *chan;  /* input channel for this buffer */
   IOConfig cf;        /* configuration for this channel */

   /* New parameters for channel type buffer */
   HParmSrcDef ext;     /* external source functions */
   Ptr xdata;           /* data for external source */
   unsigned short crcc; /* Put crcc here when we read it !! */

   /*  Channel buffer consists of a single block of floats */
   /*  arranged as maxRows rows, cf->nUsed cols */
   /*  (this is highly simplifed compared to HTK HParm */
   float *data;        /* data buffer - index base 0 */
   int maxRows;        /* max rows in buffer */
   int inRow;          /* index of next row to input */
   int outRow;         /* index of next row to output */
   int qst;            /* next row in main block to qualify (qst>qwin) */
   int qen;            /* final row in main block qualified (last valid row) */
   int qwin;           /* Width of qualify window (needed on each side) */
   /* Silence detector parameters and results */
   float *spVal;       /* Array [spValLast..maxRows-1] of sp levels */
   int spValLast;      /* index of oldest slot in spVal array */
   int spDetCnt;       /* Number of speech frames in spVal window */
   int silDetCnt;      /* Number of silence frames in spVal window */
   Boolean isSpeech;   /* Last frame output was speech not silence */
   int spDetFrms;      /* number of frames of speech detected */
}ParmBufRec;

/* ----------------------------- Local Memory  --------------------------*/

static ConfParam *cParm[MAXGLOBS];      /* config parameters */
static int nParm = 0;

static MemHeap parmHeap;                /* HParm no longer uses gstack */

static Boolean hparmBin=TRUE; /* HTK format files are binary */

static ChannelInfo *defChan=NULL;
static ChannelInfo *curChan=NULL;

/* ----------------------- IO Configuration Handling ------------------ */

/*
   After the global feature transform is loaded as a macro via HModel, if the
   channel feature transform config is empty then this function is invoked in
   LoadMat to pass on all the channel config setup from the loaded input
   linear transform data structure.
*/
static void SetInputXFormConfig(IOConfig cf, InputXForm *xf)
{
   LinXForm *xform;

   xform = xf->xform;
   cf->xform = xf;
   if (IntVecSize(xform->blockSize) != 1)
      HError(999,"Only full linear transforms currently supported");
   cf->matPK = xf->pkind;
   cf->preQual = xf->preQual;
   /* Currently hard-wired and ignored */
   cf->preFrames = 0;
   cf->postFrames = 0;
   cf->MatTran = xform->xform[1];
   if (cf->MatTranFN == NULL) /* set to non-NULL */
      cf->MatTranFN = xf->xformName;
   if (((cf->preFrames>0) || (cf->postFrames>0)) && (HasZerom(cf->tgtPK)))
      HError(-1,"Mismatch possible for ZeroMean due to end truncations.\n All static parameters floored (including Energy) as using matrix transformation.\n For post transformation dynamic parameters also floored.");
}

/* EXPORT->SetParmHMMSet: specifies the HMMSet to be used with the frontend */
void SetParmHMMSet(Ptr aset)
{
   char buf[MAXSTRLEN];
   InputXForm *cfg_xf, *hmm_xf;
   LabId id;

   hset = (HMMSet *)aset;
   hmm_xf = hset->xf;
   if (defChan != NULL) { /* xforms may already be set using config files */
      cfg_xf = defChan->cf.xform;
      defChan->cf.cfhset=hset;
      if (cfg_xf != NULL) { /* is there a transform currently set */
         if (hmm_xf != NULL) {
            /*
               need to check that the transforms are the same. This may be achieved
               by ensuring that the transforms have the same macroname.
            */
            if (strcmp(hmm_xf->xformName,cfg_xf->xformName))
               HError(6396,"Incompatible XForm macros in MMF and config file %s and %s",
                      hmm_xf->xformName,cfg_xf->xformName);
            else if (cfg_xf != hmm_xf)
               HRError(6396,"Assumed compatible XForm macro %s in files %s and %s",
                       hmm_xf->xformName,hmm_xf->fname,cfg_xf->fname);
         } else {
            /*
               there is a config transform specified, so check that it is compatble
               with the model set MMFId
            */
            if ((hset->hmmSetId != NULL) && (!MaskMatch(cfg_xf->mmfIdMask,buf,hset->hmmSetId)))
               HError(6396,"HMM Set %s is not compatible with InputXForm",hset->hmmSetId);
            /*
              also need to ensure that the transform from the cofig option is converted
              into a macro.
            */
            id = GetLabId(cfg_xf->xformName,TRUE);
            NewMacro(hset,-2,'j',id,(Ptr)cfg_xf);
            cfg_xf->nUse++;
            hset->xf = cfg_xf;
         }
      } else {
         /*
            transform needs to be set-up from the model set.
            Also need to reload the variance floor.
         */
         if (hmm_xf != NULL) {
            SetInputXFormConfig(&(defChan->cf),hmm_xf);
            if ((hset->hmmSetId != NULL) && (!MaskMatch(hmm_xf->mmfIdMask,buf,hset->hmmSetId)))
               HError(6396,"HMM Set %s is not compatible with InputXForm",hset->hmmSetId);
         }
      }
   } else { /* somehow this is happening prior to InitParm ... */
      HError(6396,"Calling SetParmHMMSet prior to InitParm");
   }
}

/*
   Load a global feature transform given in the channel config setup via a function
   defined in HModel since the transform is also treated as a macro. Afterwards check
   the consistency between the transform's model id. If the channel global feature
   transform setup is empty the pass on all the information from the loaded transform
*/
static void LoadMat (MemHeap *x, IOConfig cf)  /*static??*/
{
   InputXForm *xf;
   char macroname[MAXSTRLEN];
   xf = LoadInputXForm(cf->cfhset,NameOf(cf->MatTranFN,macroname),cf->MatTranFN);
   if (xf == NULL)
     HError(999,"Cannot correctly load input transform from file %s",cf->MatTranFN);
   if (cf->xform == NULL) { /* No transform from model set */
      SetInputXFormConfig(cf,xf);
   } else { /* check that transform is the same as the model one */
      if (strcmp(xf->xformName,cf->xform->xformName)) {
         HRError(999,"Possibly incompatible XForm macros in MMF and config file %s and %s",
                 xf->xformName,cf->xform->xformName);
         SetInputXFormConfig(cf,xf);
      }
   }
}

static void ApplyXForm2Vector(LinXForm *linXForm, Vector mean)
{
   Vector vec, bias;
   int size,b,bsize;
   Matrix A;
   float tmp;
   int i,j;
   int cnt,cnti,cntj;

   /* Check dimensions */
   size = linXForm->vecSize;
   if (size != VectorSize(mean))
      HError(999,"Transform dimension (%d) does not match mean dimension (%d)",
             size,VectorSize(mean));
   vec = CreateVector(&gstack,size);
   CopyVector(mean,vec); ZeroVector(mean);
   /* Transform mean */
   for (b=1,cnti=1,cnt=1;b<=IntVecSize(linXForm->blockSize);b++) {
      bsize = linXForm->blockSize[b];
      A = linXForm->xform[b];
      for (i=1;i<=bsize;i++,cnti++) {
         tmp = 0;
         for (j=1,cntj=cnt;j<=bsize;j++,cntj++)
            tmp += A[i][j] * vec[cntj];
         mean[cnti] = tmp;
      }
      cnt += bsize;
   }
   /* Apply bias if required */
   bias = linXForm->bias;
   if (bias != NULL) {
      for (i=1;i<=size;i++)
         mean[i] += bias[i];
   }
   FreeVector(&gstack,vec);
}

/*
   Rather than put this all in InitParm and in InitChannel
   we abstract it into a separate function.
*/

char *GS(char *s){static char b[100]; GetConfStr(cParm,nParm,s,b); return b;}
int     GI(char *s){int i;     GetConfInt(cParm,nParm,s,&i); return i;}
double  GF(char *s){double d;  GetConfFlt(cParm,nParm,s,&d); return d;}
Boolean GB(char *s){Boolean b; GetConfBool(cParm,nParm,s,&b); return b;}

/* ReadIOConfig: Create an IOConfig object.  Initial values are copied
   from defCon and then updated from configuration parameters. */
static IOConfig ReadIOConfig(IOConfig p)
{
   IOConfParm i;
   char *s;

   for (i=SOURCEKIND; i<CFGSIZE; i=(IOConfParm) (i+1)){
      s = ioConfName[i];
      if (HasConfParm(cParm,nParm,s))
         switch (i) {
         case SOURCEKIND:     p->srcPK = Str2ParmKind(GS(s)); break;
         case SOURCEFORMAT:   p->srcFF = Str2Format(GS(s)); break;
         case SOURCERATE:     p->srcSampRate = GF(s); break;
         case ZMEANSOURCE:    p->zMeanSrc = GB(s); break;
         case TARGETKIND:     p->tgtPK = Str2ParmKind(GS(s)); break;
         case TARGETFORMAT:   p->tgtFF = Str2Format(GS(s)); break;
         case TARGETRATE:     p->tgtSampRate = GF(s); break;
         case SAVECOMPRESSED: p->saveCompressed = GB(s); break;
         case SAVEWITHCRC:    p->saveWithCRC = GB(s); break;
         case WINDOWSIZE:     p->winDur = GF(s); break;
         case USEHAMMING:     p->useHam = GB(s); break;
         case PREEMCOEF:      p->preEmph = GF(s); break;
         case USEPOWER:       p->usePower = GB(s); break;
         case NUMCHANS:       p->numChans = GI(s); break;
         case LOFREQ:         p->loFBankFreq = GF(s); break;
         case HIFREQ:         p->hiFBankFreq = GF(s); break;
         case WARPFREQ:       p->warpFreq = GF(s); break;
         case WARPLCUTOFF:    p->warpLowerCutOff = GF(s); break;
         case WARPUCUTOFF:    p->warpUpperCutOff = GF(s); break;
         case LPCORDER:       p->lpcOrder = GI(s); break;
         case COMPRESSFACT:   p->compressFact = GF(s); break;
         case CEPLIFTER:      p->cepLifter= GI(s); break;
         case NUMCEPS:        p->numCepCoef = GI(s); break;
         case CEPSCALE:       p->cepScale = GF(s); break;
         case RAWENERGY:      p->rawEnergy = GB(s); break;
         case ENORMALISE:     p->eNormalise = GB(s); break;
         case ESCALE:         p->eScale = GF(s); break;
         case SILFLOOR:       p->silFloor = GF(s); break;
         case DELTAWINDOW:    p->delWin = GI(s); break;
         case ACCWINDOW:      p->accWin = GI(s); break;
         case SIMPLEDIFFS:    p->simpleDiffs = GB(s); break;
         case SILDISCARD:     p->silDiscard = GF(s); break;
         case SPEECHTHRESH:   p->spThresh = GF(s); break;
         case SPCSEQCOUNT:    p->spcSeqCount = GI(s); break;
         case SPCGLCHCOUNT:   p->spcGlchCount = GI(s); break;
         case SILGLCHCOUNT:   p->silGlchCount = GI(s); break;
         case SILSEQCOUNT:    p->silSeqCount = GI(s); break;
	      case MAXSPCFRAMES:   p->maxSpcFrames=GI(s); break;
         case CALWINDOW:      p->calWindow = GI(s); break;
         case CALPERIOD:      p->calPeriod = GI(s); break;
         case SILUPDATERATE:  p->silUpdateRate = GF(s); break;
         case INITIALSIL:     p->initialSil = GF(s); break;
         case ENABLEINHIBIT:  p->enableInhibit = GB(s); break;
         case VQTABLE:        p->vqTabFN = CopyString(&gcheap,GS(s)); break;
         case ADDDITHER:      p->addDither = GF(s); break;
         case DOUBLEFFT:      p->doubleFFT = GB(s); break;
         case CMNTCONST:      p->cmnTConst = GF(s); break;
         case CMNRESETONSTOP: p->cmnResetOnStop = GB(s); break;
	      case CMNMINFRAMES:   p->cmnMinFrames = GI(s); break;
	      case CMNDEFAULT:     p->cmnDefault =  CopyString(&gcheap, GS(s)); break;
	      case MATTRANFN:      p->MatTranFN= CopyString(&gcheap, GS(s)); break;
         case THIRDWINDOW:    p->thirdWin = GI(s); break;
         case FOURTHWINDOW:   p->fourthWin = GI(s); break;
	      case V1COMPAT:       p->v1Compat = GB(s); break;
    	   default:   HError(6999,"ReadIOConfig:  unknown parameter %d",i);
         }
   }
   if (p->MatTranFN != NULL){
      LoadMat (&gcheap,p);
   }
   return p;
}

/* EXPORT->InitParm: initialise memory and configuration parameters */
ReturnStatus InitParm(void)
{
   Boolean b;
   int i;

   CreateHeap(&parmHeap, "HPARM C Heap",  MSTAK, 1, 1.0, 20000, 80000 );

   Register(hparm_version);
   nParm = GetConfig("HPARM", TRUE, cParm, MAXGLOBS);
   if (nParm>0){
      if (GetConfInt(cParm,nParm,"TRACE",&i)) trace = i;
      if (GetConfBool(cParm,nParm,"NATURALWRITEORDER",&b)) natWriteOrder = b;
      if (GetConfBool(cParm,nParm,"HIGHDIFF",&b)) highDiff = b;
  }

   defChan=curChan= (ChannelInfo *) New(&gcheap,sizeof(ChannelInfo));
   defChan->confName=CopyString(&gcheap,"HPARM");
   defChan->fCnt=defChan->sCnt=defChan->oCnt=0;
   defChan->cf.cfhset=NULL;
   defChan->spDetThresh=defChan->spDetSNR=-1.0;
   defChan->spDetSp=0.0;
   defChan->spDetParmsSet=FALSE;
   defChan->calInhibit=FALSE;
   defChan->next=NULL;
   /* Set up configuration parameters - once only now */
   defChan->cf=defConf;
   ReadIOConfig(&defChan->cf);
   defChan->spDetSil = defChan->cf.initialSil;
   return(SUCCESS);
}

/* This is a slightly modified version of SetParmHMMSet that doesn't use globals:
   see SetParmHMMSet for comments */
void SetChanHMMSet(ChannelInfoLink nChan, Ptr aset)
{
   char buf[MAXSTRLEN];
   HMMSet *parmhset;
   ChannelInfo *newChan;
   InputXForm *cfg_xf, *hmm_xf;
   LabId id;

   newChan=(ChannelInfo *) nChan;
   parmhset = (HMMSet *)aset;
   newChan->cf.cfhset=parmhset;
   hmm_xf = parmhset->xf;
   if (newChan != NULL) { /* xforms may already be set using config files */
      cfg_xf = newChan->cf.xform;
      if (cfg_xf != NULL) { /* is there a transform currently set */
         if (hmm_xf != NULL) {
             if (strcmp(hmm_xf->xformName,cfg_xf->xformName))
               HError(6396,"Incompatible XForm macros in MMF and config file %s and %s",
                      hmm_xf->xformName,cfg_xf->xformName);
            else if (cfg_xf != hmm_xf)
               HRError(6396,"Assumed compatible XForm macro %s in files %s and %s",
                       hmm_xf->xformName,hmm_xf->fname,cfg_xf->fname);
         } else {
             if ((parmhset->hmmSetId != NULL) && (!MaskMatch(cfg_xf->mmfIdMask,buf,parmhset->hmmSetId)))
               HError(6396,"HMM Set %s is not compatible with InputXForm",parmhset->hmmSetId);
             id = GetLabId(cfg_xf->xformName,TRUE);
	     NewMacro(parmhset,-2,'j',id,(Ptr)cfg_xf);
	     cfg_xf->nUse++;
	     parmhset->xf = cfg_xf;
         }
      } else {
         if (hmm_xf != NULL) {
            SetInputXFormConfig(&(newChan->cf),hmm_xf);
            if ((parmhset->hmmSetId != NULL) && (!MaskMatch(hmm_xf->mmfIdMask,buf,parmhset->hmmSetId)))
               HError(6396,"HMM Set %s is not compatible with InputXForm",parmhset->hmmSetId);
         }
      }
   } else { /* somehow this is happening prior to InitParm ... */
       HError(6396,"Calling SetParmHMMSet prior to InitParm");
   }
}

ChannelInfoLink SetCoderChannel(char *confName)
{
   char buf[MAXSTRLEN],*in,*out;
   ChannelInfo *newChan;

   if (confName==NULL)
     newChan=defChan;
   else {
     for (in=confName,out=buf;*in!=0;in++,out++) *out=toupper(*in);
     *out=0;
   }

   newChan=(ChannelInfo *)New(&gcheap,sizeof(ChannelInfo));
   newChan->confName=CopyString(&gcheap,buf);
   newChan->fCnt=newChan->sCnt=newChan->oCnt=0;
   newChan->spDetThresh=newChan->spDetSNR=-1.0;
   newChan->spDetSp=0.0;
   newChan->spDetParmsSet=FALSE;
   newChan->calInhibit=FALSE;
   newChan->next=NULL;
   /* Set up configuration parameters - copy from default conf? */
   newChan->cf=defConf;
   newChan->cf.cfhset=NULL;
   nParm = GetConfig(newChan->confName, TRUE, cParm, MAXGLOBS);
   ReadIOConfig(&newChan->cf);
   newChan->spDetSil=newChan->cf.initialSil;
   return(newChan);
}

/* ------------------- Buffer Status Operations --------------- */

static char * pbStatMap[] = {
   "PB_INIT","PB_CALIBRATING","PB_RUNNING","PB_STOPPED","PB_CLEARED"
};

/* ChangeState: change state of buffer and trace if enabled */
static void ChangeState(ParmBuf pbuf, PBStatus newState)
{
   if (trace&T_PBS)
      printf("HParm:  %s -> %s\n",pbStatMap[pbuf->status],pbStatMap[newState]);
   pbuf->status = newState;
}

static void SI(int i){
   if (i==MAX_INT) printf("    MXI");
   else printf("%7d",i);
}

/* ShowBuffer: display current state of the buffer */
static void ShowBuffer(ParmBuf pbuf, char * title)
{
   IOConfig cf = pbuf->cf;
   float *fp = pbuf->data;
   int i;

   printf(" PBuf [%s]: Status=%s\n",title,pbStatMap[pbuf->status]);
   printf("   inRow outRow maxRows    qst   qen   qwin\n");
   printf("%7d%7d%7d%7d%7d%7d\n",pbuf->inRow, pbuf->outRow, pbuf->maxRows,
	  pbuf->qst, pbuf->qen, pbuf->qwin);
   printf("IOConfig: nUsed  nCols  nCvrt tgtUsed\n");
   printf("%14d%7d%7d%7d\n", cf->nUsed, cf->nCols, cf->nCvrt, cf->tgtUsed);
   printf(" Main data[0..4]\n");
   printf("%14.2f%7.2f%7.2f%7.2f\n",fp[0],fp[1],fp[2],fp[3]);
   printf("-------------------------------------------------------\n");
}

/* ------------------- Parameter Kind Conversions --------------- */

static char *pmkmap[] = {"WAVEFORM", "LPC", "LPREFC", "LPCEPSTRA",
                         "LPDELCEP", "IREFC",
                         "MFCC", "FBANK", "MELSPEC",
                         "USER", "DISCRETE", "PLP",
                         "ANON"};

/* EXPORT-> ParmKind2Str: convert given parm kind to string */
char *ParmKind2Str(ParmKind kind, char *buf)
{
   strcpy(buf,pmkmap[BaseParmKind(kind)]);
   if (HasEnergy(kind))    strcat(buf,"_E");
   if (HasDelta(kind))     strcat(buf,"_D");
   if (HasNulle(kind))     strcat(buf,"_N");
   if (HasAccs(kind))      strcat(buf,"_A");
   if (HasThird(kind))     strcat(buf,"_T");
   if (HasCompx(kind))     strcat(buf,"_C");
   if (HasCrcc(kind))      strcat(buf,"_K");
   if (HasZerom(kind))     strcat(buf,"_Z");
   if (HasZeroc(kind))     strcat(buf,"_0");
   if (HasVQ(kind))        strcat(buf,"_V");
   return buf;
}

/* EXPORT->Str2ParmKind: Convert string representation to ParmKind */
ParmKind Str2ParmKind(char *str)
{
   ParmKind i = -1;
   char *s,buf[255];
   Boolean hasE,hasD,hasN,hasA,hasT,hasC,hasK,hasZ,has0,hasV,found;
   int len;

   hasV=hasE=hasD=hasN=hasA=hasT=hasC=hasK=hasZ=has0=FALSE;
   strcpy(buf,str); len=strlen(buf);
   s=buf+len-2;
   while (len>2 && *s=='_') {
      switch(*(s+1)){
      case 'E': hasE = TRUE; break;
      case 'D': hasD = TRUE; break;
      case 'N': hasN = TRUE; break;
      case 'A': hasA = TRUE; break;
      case 'T': hasT = TRUE; break;
      case 'C': hasC = TRUE; break;
      case 'K': hasK = TRUE; break;
      case 'Z': hasZ = TRUE; break;
      case '0': has0 = TRUE; break;
      case 'V': hasV = TRUE; break;
      default: HError(6370,"Str2ParmKind: unknown ParmKind qualifier %s",str);
      }
      *s = '\0'; len -= 2; s -= 2;
   }
   found = FALSE;
   do {
      s=pmkmap[++i];
      if (strcmp(buf,s) == 0) {
         found = TRUE;
         break;
      }
   } while (strcmp("ANON",s)!=0);
   if (!found)
      return ANON;
   if (i == LPDELCEP)         /* for backward compatibility with V1.2 */
      i = LPCEPSTRA | HASDELTA;
   if (hasE) i |= HASENERGY;
   if (hasD) i |= HASDELTA;
   if (hasN) i |= HASNULLE;
   if (hasA) i |= HASACCS;
   if (hasT) i |= HASTHIRD;
   if (hasK) i |= HASCRCC;
   if (hasC) i |= HASCOMPX;
   if (hasZ) i |= HASZEROM;
   if (has0) i |= HASZEROC;
   if (hasV) i |= HASVQ;
   return i;
}

/* EXPORT->BaseParmKind: return the basic sample kind without qualifiers */
ParmKind BaseParmKind(ParmKind kind) { return kind & BASEMASK; }

/* EXPORT->HasXXXX: returns true if XXXX included in ParmKind */
Boolean HasEnergy(ParmKind kind){return (kind & HASENERGY) != 0;}
Boolean HasDelta(ParmKind kind) {return (kind & HASDELTA) != 0;}
Boolean HasAccs(ParmKind kind)  {return (kind & HASACCS) != 0;}
Boolean HasThird(ParmKind kind) {return (kind & HASTHIRD) != 0;}
Boolean HasNulle(ParmKind kind) {return (kind & HASNULLE) != 0;}
Boolean HasCompx(ParmKind kind) {return (kind & HASCOMPX) != 0;}
Boolean HasCrcc(ParmKind kind)  {return (kind & HASCRCC) != 0;}
Boolean HasZerom(ParmKind kind) {return (kind & HASZEROM) != 0;}
Boolean HasZeroc(ParmKind kind) {return (kind & HASZEROC) != 0;}
Boolean HasVQ(ParmKind kind)    {return (kind & HASVQ) != 0;}

/* Apply the global feature transform */
static void ApplyStaticMat(IOConfig cf, float *data, Matrix trans, int vSize, int n, int step, int offset)
{

   float *fp,*fp1;
   int i,j,k,l,mrows,mcols,nframes,fsize,m;
   Vector *odata,tmp;
   mrows = NumRows(trans); mcols = NumCols(trans);
   nframes = 1 + cf->preFrames + cf->postFrames;
   fsize = cf->nUsed;
   odata = New(&gstack,nframes*sizeof(Vector));
   odata--;
   fp = data-1;
   for (i=1;i<=nframes;i++)
      odata[i] = CreateVector(&gstack,fsize);
   for (i=2;i<=nframes;i++) {
     for (j=1;j<=fsize;j++)
         odata[i][j] = fp[j];
      fp += vSize;
   }
   fp1 = data-1;

   if ((fsize*nframes) != mcols)
      HError(-1,"Incorrect number of elements (%d %d)",cf->nUsed ,mcols);
   for (i=1;i<=n-nframes+1;i++){
      tmp = odata[1];
      for (j=1;j<=nframes-1;j++)
         odata[j] = odata[j+1];
      odata[nframes] = tmp;
      for (j=1;j<=fsize;j++){
         tmp[j]=fp[j];
      }
      for (j=1;j<=mrows;j++) {
         fp++; fp1++; *fp1=0; m=0;
         for(l=1;l<=nframes;l++) {
            for (k=1;k<=fsize;k++) {
               m++;
               *fp1 += trans[j][m]*odata[l][k];
            }
         }
      }
      fp += vSize-mrows;
      fp1 += vSize-mrows;
   }

   Dispose(&gstack,odata+1);
   cf->nUsed = mrows;
}

/* ---------------- Data Sizing and Memory allocation --------------- */

/* MakeIOConfig: Create an IOConfig object.  Initial values are copied
   from defCon and then updated from configuration parameters */
static IOConfig MakeIOConfig(MemHeap *x,ChannelInfo *chan)
{
   IOConfig p;

   p = (IOConfig)New(x,sizeof(IOConfigRec));
   *p = chan->cf;
   return p;
}

/* SetCodeStyle: set the coding style in given cf */
static void SetCodeStyle(IOConfig cf)
{
   ParmKind tgt = cf->tgtPK&BASEMASK;
   char buf[MAXSTRLEN];

   switch (tgt) {
   case LPC: case LPREFC: case LPCEPSTRA:
      cf->style = LPCbased;
      break;
   case MELSPEC: case FBANK: case MFCC: case PLP:
      cf->style = FFTbased;
      break;
   case DISCRETE:
      cf->style = VQbased;
      break;
   default:
      HError(6321,"SetCodeStyle: Unknown style %s",ParmKind2Str(tgt,buf));
   }
}

/* ValidCodeParms: check to ensure reasonable wave->parm code params */
static void ValidCodeParms(IOConfig cf)
{
   int order;
   ParmKind btgt = cf->tgtPK&BASEMASK;

   if (cf->srcSampRate<=0.0 || cf->srcSampRate>10000000.0)
      HError(6371,"ValidCodeParms: src frame rate %f unlikely",cf->srcSampRate);
   if (cf->tgtSampRate<=cf->srcSampRate || cf->tgtSampRate>10000000.0)
      HError(6371,"ValidCodeParms: parm frame rate %f unlikely",cf->tgtSampRate);
   if (cf->winDur<cf->tgtSampRate || cf->winDur>cf->tgtSampRate*100.0)
      HError(6371,"ValidCodeParms: window duration %f unlikely",cf->winDur);
   if (cf->preEmph<0.0 || cf->preEmph>1.0)
      HError(6371,"ValidCodeParms: preEmph %f illegal",cf->preEmph);
   SetCodeStyle(cf);  /* in case not set yet */
   switch (cf->style){
   case LPCbased:
      order = cf->lpcOrder;
      if (order<2 || order>1000)
         HError(6371,"ValidCodeParms: unlikely lpc order %d",cf->lpcOrder);
      if (cf->tgtPK&HASZEROC)
         HError(6321,"ValidCodeParms: cannot have C0 with lpc");
      break;
   case FFTbased:
      order = cf->numChans;
      if (order<2 || order>1000)
         HError(6371,"ValidCodeParms: unlikely num channels %d",cf->numChans);
      if (cf->loFBankFreq > cf->hiFBankFreq ||
          cf->hiFBankFreq > 0.5E7/cf->srcSampRate)
         HError(6371,"ValidCodeParms: bad band-pass filter freqs %.1f .. %.1f",
                cf->loFBankFreq,cf->hiFBankFreq);
      if (btgt == PLP) {
         order = cf->lpcOrder;
         if (order < 2 || order > 1000)
            HError(6371,"ValidCodeParms: unlikely lpc order %d",cf->lpcOrder);
	 if (!cf->usePower)
	    HError(-6371,"ValidCodeParms: Using linear spectrum with PLP");
	 if (cf->compressFact >= 1.0 || cf->compressFact <= 0.0)
	    HError(6371,"ValidCodeParms: Compression factor (%f) should have a value between 0 and 1\n",cf->compressFact);
      }
      break;
   default: break;
   }
   if (btgt == LPCEPSTRA || btgt == MFCC || btgt == PLP){
      if (cf->numCepCoef < 2 || cf->numCepCoef > order)
         HError(6371,"ValidCodeParms: unlikely num cep coef %d",cf->numCepCoef);
      if (cf->cepLifter < 0 || cf->cepLifter > 1000)
         HError(6371,"ValidCodeParms: unlikely cep lifter %d",cf->cepLifter);
   }
   if (cf->warpFreq < 0.5 || cf->warpFreq > 2.0)
      HError (6371, "ValidCodeParms: unlikely warping factor %s\n", cf->warpFreq);
   if (cf->warpFreq != 1.0) {
      if (cf->warpLowerCutOff == 0.0 || cf->warpUpperCutOff == 0.0 ||
          cf->warpLowerCutOff > cf->warpUpperCutOff)
         HError (6371, "ValidCodeParms: invalid warping cut-off frequencies %f %f \n",
                 cf->warpLowerCutOff , cf->warpUpperCutOff);
      if (cf->warpUpperCutOff == 0.0 && cf->warpLowerCutOff != 0.0) {
         cf->warpUpperCutOff = cf->warpLowerCutOff;
         HError (-6371, "ValidCodeParms: setting warp cut-off frequencies to %f %f\n",
                 cf->warpLowerCutOff, cf->warpUpperCutOff);
      }
      if (cf->warpLowerCutOff == 0.0 && cf->warpUpperCutOff != 0.0) {
         cf->warpUpperCutOff = cf->warpLowerCutOff ;
         HError (-6371, "ValidCodeParms: setting warp cut-off frequencies to %f %f\n",
                 cf->warpLowerCutOff, cf->warpUpperCutOff);
      }
   }
   if (HasZerom(cf->tgtPK) && cf->mean.defMeanVec == NULL)
     HError(6371,"ValidCodeParms: cep mean required for _Z",cf->cepLifter);
}

/* EXPORT->ValidConversion: checks that src -> tgt conversion is possible */
Boolean ValidConversion (ParmKind src, ParmKind tgt)
{
   static short xmap[13][13] = {
      { 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0},    /* src = WAVEFORM */
      { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* src = LPC */
      { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* src = LPREFC */
      { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* src = LPCEPSTRA */
      { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* src = LPDELCEP */
      { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* src = IREFC */
      { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},    /* src = MFCC */
      { 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0},    /* src = FBANK */
      { 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0},    /* src = MELSPEC */
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},    /* src = USER */
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},    /* src = DISCRETE */
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},    /* src = PLP */
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* src = ANON */
   };
   if (src == tgt) return TRUE;
   if (xmap[src&BASEMASK][tgt&BASEMASK] == 0 ) return FALSE;
   if ((tgt&BASEMASK) == DISCRETE){
      if ((tgt&~(BASEMASK|HASCRCC)) != 0) return FALSE;
   } else {
      if ((tgt&HASENERGY) && !(src&HASENERGY) )
         return FALSE;
      if ((tgt&HASZEROC)  && !(src&HASZEROC) )
         return FALSE;
      if ((tgt&HASENERGY) && (tgt&HASZEROC) && (tgt&HASDELTA))
         return FALSE;
      if ((tgt&HASENERGY) && (tgt&HASZEROC) && (tgt&HASNULLE))
         return FALSE;
      if (!(tgt&HASDELTA) && (tgt&HASACCS)) return FALSE;
      if ((tgt&HASNULLE) && !((tgt&HASENERGY) || (tgt&HASZEROC)) )
         return FALSE;
      if ((tgt&HASNULLE) && !(tgt&HASDELTA) ) return FALSE;
   }
   return TRUE;
}



/* FindSpans: Finds the positions of the subcomponents of a parameter
   vector with size components as follows (span values are -ve if
   subcomponent does not exist):
   span: [0] .. [1]  [2]  [3]  [4] .. [5]  [6] .. [7][ 8] .. [9]
          statics   cep0 energy  deltas       accs      third       */
static void FindSpans(short span[12], ParmKind k, int size)
{
   int i,stat,en=0,del=0,acc=0,c0=0,third=0,fourth=0;

   for (i=0; i<10; i++) span[i] = -1;
   /*
      if having higher order differentials and the precondition
      is there is third oder differentials
   */
   if (k&HASTHIRD && highDiff == TRUE)
      fourth = third = acc = del = size/5;
   else if (k&HASTHIRD)
      third = acc = del = size/4;
   else if (k&HASACCS)
      acc = del = size/3;
   else if (k&HASDELTA)
      del = size/2;
   if (k&HASENERGY) ++en;  if (k&HASZEROC) ++c0;
   stat = size - c0 - en - del - acc - third - fourth;
   if (stat>0) { span[0] = 0;              span[1] = stat-1;}
   if (c0>0)     span[2] = stat; if (en>0) span[3] = stat+c0;
   if (del>0)  { span[4] = stat+c0+en;     span[5] = stat+c0+en+del-1;}
   if (acc>0)  { span[6] = stat+c0+en+del; span[7] = stat+c0+en+del+acc-1;}
   if (third>0){ span[8] = stat+c0+en+del+acc; span[9] = stat+c0+en+del+acc+third-1;}
   if (fourth>0){ span[10] = stat+c0+en+del+acc+third; span[11] = stat+c0+en+del+acc+third+fourth-1;}
}

/* TotalComps: return the total number of components in a parameter vector
   with nStatic components and ParmKind pk */
static int TotalComps(int nStatic, ParmKind pk)
{
   int n,x;

   n = nStatic;
   if (pk&HASENERGY) ++n;
   if (pk&HASZEROC)  ++n;
   x = n;
   if (pk&HASDELTA){
      n += x;
      if (pk&HASACCS) {
        n += x;
        if (pk&HASTHIRD) {
           n += x;
           if (highDiff == TRUE){
              n += x;
           }
        }
      }
   }
   return n;
}

/* NumStatic: return the number of static components in a parameter vector
   with nTotal components and ParmKind pk */
static int NumStatic(int nTotal, ParmKind pk)
{
   short span[12];

   FindSpans(span,pk,nTotal);
   return span[1]-span[0]+1;
}

/* NumEnergy: return the number of energy components in a parameter vector
   with nTotal components and ParmKind pk */
static int NumEnergy(ParmKind pk)
{
   int e;

   if (!(pk&(HASENERGY|HASZEROC))) return 0;
   e = 1;
   if (pk&HASDELTA){
      ++e;
      if (pk&HASACCS) {
        ++e;
        if (pk&HASTHIRD){
           ++e;
           if (highDiff == TRUE){
              ++e;
           }
        }
      }
   }
   return e;
}

/* EqualKind: return true if kinds are internally compatible */
static Boolean EqualKind(ParmKind a, ParmKind b)
{
   /* Energy suppression only occurs at observation level */
   a = a&(~HASNULLE); b = b&(~HASNULLE);
   return a==b;
}

/* --------------- Parameter Conversion Routines ----------------- */

/* All of the routines in this section operate on a table of floats.
   Typically the same operation is applied to each row.  Source slice
   is indexed by si relative to start of that row, target slice is
   indexed by ti relative to start of that row


   data
   v
   ................................... ^
   si              ti             |
   .....xxxxxxxxxxx.....xxxxxxxxxxx... nRows
   <--- d --->     <--- d --->    |
   ................................... v
   <--------------nCols-------------->
*/

/* AddDiffs: target slice of each row are regression coeffs corresponding
   to source slice.  Regression is calculated over rows
   -winSize to +winSize. This assumes that hdMargin valid rows are stored
   before the parameter block, and tlMargin valid rows are stored after
   the parameter block.  When applied to tables these will be both zero.
   For buffers, they may be positive. */

/* Note that unlike before this function really will process nRows of data */
/* And will not automagically do extra (which was a necessary and probably */
/* unintended side effect of the way the function worked previously) */

static void AddDiffs(float *data, int nRows, int nCols, int si, int ti, int d,
                     int winSize, int hdMargin, int tlMargin, Boolean v1Compat,
		     Boolean simpDiffs)
{
   float *p;
   int n,offset = ti-si;
   int head,tail;

   if (hdMargin<0) hdMargin=0;
   if (hdMargin>winSize) head = 0;
   else head = winSize - hdMargin;
   if (tlMargin<0) tlMargin=0;
   if (tlMargin>winSize) tail = 0;
   else tail = winSize - (tlMargin>0?tlMargin:0);

   p = data+si; n=nRows-(head+tail);
   if (n<=0) {
      if (head>0 && tail>0) {
         /* Special case to cope with ultra short buffers */
         AddRegression(p,d,nRows,nCols,offset,winSize,
                       hdMargin,tlMargin,simpDiffs);
         head=tail=n=0; return;
      }
      else if (tail==0) head=nRows,n=0; /* Just do head rows */
      else if (head==0) tail=nRows,n=0; /* Just do tail rows */
   }

   /* Make sure have winSize before and after columns to qualify */
   if (head>0) {
      if (v1Compat)
         AddHeadRegress(p,d,head,nCols,offset,0,simpDiffs);
      else
         AddRegression(p,d,head,nCols,offset,winSize,
                       hdMargin,winSize,simpDiffs);
      p += head*nCols;
   }
   if (n>0) {
      AddRegression(p,d,n,nCols,offset,winSize,winSize,winSize,simpDiffs);
      p += n*nCols;
   }
   if (tail>0) {
      if (v1Compat)
         AddTailRegress(p,d,tail,nCols,offset,0,simpDiffs);
      else
         AddRegression(p,d,tail,nCols,offset,winSize,
                       winSize,tlMargin,simpDiffs);
   }
}

/* DeleteColumns: delete source slice (NB: nCols is not changed ) */
static void DeleteColumn(float *data, int nUsed, int si, int d)
{
   int rest;
   char *p1,*p2;

   p1 = (char *)data + si*sizeof(float);
   p2 = p1 + d*sizeof(float);
   rest = (nUsed - (si+d))*sizeof(float);
   memmove(p1,p2,rest);
}


/* AddQualifiers: add quals needed to get from cf->curPK to cf->tgtPK. */
/*  Ensures that nRows of data are valid and fully qualified. */
/*  This means that delta coefs may be calculated beyond this range */
/*  Values of hd/tlValid indicate that margin of static data exists at */
/*  start/end of nRows */
static void AddQualifiers(ParmBuf pbuf,float *data, int nRows, IOConfig cf,
                          int hdValid, int tlValid)
{
   char buf[100],buff1[256],buff2[256];
   int si,ti,d,ds,de, size;
   short span[12];
   ParmKind tgtBase;
   Vector tmp;
   LinXForm *xf;

   if (highDiff && ((cf->curPK&HASDELTA) || (cf->curPK&HASACCS) || (cf->curPK&HASTHIRD)))
       HError (6371, "AddQualifiers: HIGHDIFF=T not supported with source features that contain derivatives already");

   if ((cf->curPK == cf->tgtPK && (cf->MatTranFN == NULL))) return;
   if (trace&T_QUA)
      printf("HParm:  adding Qualifiers to %s ...",ParmKind2Str(cf->curPK,buf));
   if (cf->MatTranFN != NULL) { /* Do the generic checks that the matrix is appropriate */
     if (((cf->matPK&BASEMASK)&(cf->curPK&BASEMASK)) != (cf->matPK&BASEMASK))
       HError(6371, "AddQualifiers: Incorrect source parameter type (%s %s)",ParmKind2Str(cf->curPK,buff1), ParmKind2Str(cf->matPK,buff2));
     if ((HasEnergy(cf->matPK) && !(HasEnergy(cf->curPK))) || (!(HasEnergy(cf->matPK)) && (HasEnergy(cf->curPK))) ||
	 (HasZeroc(cf->matPK) && !(HasZeroc(cf->curPK))) || (HasZeroc(cf->curPK) && !(HasZeroc(cf->matPK))))
       HError(6371,"AddQualifiers: Incorrect qualifiers in parameter type (%s %s)",ParmKind2Str(cf->curPK,buff1),ParmKind2Str(cf->matPK,buff2));
   }

   if ((cf->MatTranFN != NULL) && (cf->preQual)) {
     if ((HasZerom(cf->matPK) && !(HasZerom(cf->curPK))) || (HasZerom(cf->curPK) && !(HasZerom(cf->matPK))))
       HError(6371, "AddQualifiers: Incorrect qualifiers in parameter type (%s %s)",ParmKind2Str(cf->curPK,buff1),ParmKind2Str(cf->matPK,buff2));
     ApplyStaticMat(cf,data,cf->MatTran,cf->nCols,nRows,0,0);
   }
   if ((cf->MatTranFN != NULL) && (!cf->preQual)) {
     tgtBase = cf->tgtPK&BASEMASK;
     size = TotalComps(cf->numCepCoef,cf->tgtPK);
     FindSpans(span,cf->tgtPK,size);
   } else
     FindSpans(span,cf->tgtPK,cf->tgtUsed);

   /* Add any required difference coefficients */
   if ((cf->tgtPK&HASDELTA) && !(cf->curPK&HASDELTA)){
      d = span[5]-span[4]+1; si = span[0]; ti = span[4];
      if (trace&T_QUA)
         printf("\nHParm:  adding %d deltas to %d rows",d,nRows);

      /* Deltas need to preceed everything a little when they can */
      if (hdValid>0) ds=pbuf->qwin-cf->delWin; else ds=0;
      if (tlValid>0) de=nRows+pbuf->qwin-cf->delWin-1; else de=nRows-1;
      AddDiffs(data+cf->nCols*ds,de-ds+1,cf->nCols,si,ti,d,cf->delWin,
               hdValid+ds,tlValid-ds,cf->v1Compat,cf->simpleDiffs);
      cf->curPK |= HASDELTA; cf->nUsed += d;
   }
   if ((cf->tgtPK&HASACCS) && !(cf->curPK&HASACCS)) {
      d = span[7]-span[6]+1; si = span[4]; ti = span[6];
      if (trace&T_QUA)
         printf("\nHParm:  adding %d accs to %d rows",d,nRows);
      /* the Accs now have to precede the thirds by accWin  */
      /* uses qwin to calc so doesn't matter if you don't have thirds! */
      if (hdValid>0) ds=pbuf->qwin-cf->delWin-cf->accWin; else ds=0;
      if (tlValid>0) de=nRows+pbuf->qwin-cf->delWin-cf->accWin-1; else de=nRows-1;

      AddDiffs(data+cf->nCols*ds,de-ds+1,cf->nCols,si,ti,d,cf->delWin,
	hdValid+ds,tlValid-ds,cf->v1Compat,cf->simpleDiffs);
      cf->curPK |= HASACCS;  cf->nUsed += d;
      if ((cf->tgtPK&HASTHIRD) && !(cf->curPK&HASTHIRD)) {
	d = span[9]-span[8]+1; si = span[6]; ti = span[8];
	if (trace&T_QUA)
	  printf("\nHParm:  adding %d thirds to %d rows",d,nRows);
	/* finally the thirds precede everything if a matrix is used */
	/* as the matrix writes over the values needed */
	if (hdValid>0) ds=pbuf->qwin-cf->delWin-cf->accWin-cf->thirdWin; else ds=0;
	if (tlValid>0) de=nRows+pbuf->qwin-cf->delWin-cf->accWin-cf->thirdWin-1; else de=nRows-1;

	AddDiffs(data+cf->nCols*ds,de-ds+1,cf->nCols,si,ti,d,cf->delWin,
		 hdValid+ds,tlValid-ds,cf->v1Compat,cf->simpleDiffs);
	cf->curPK |= HASTHIRD;  cf->nUsed += d;
	/* Adding fourth order differentials */
	if (highDiff == TRUE) {
	  d = span[11]-span[10]+1; si = span[8]; ti = span[10];
	  if (trace&T_QUA)
	    printf("\nHParm:  adding %d fourths to %d rows\n",d,nRows);
	  AddDiffs(data,nRows,cf->nCols,si,ti,d,cf->fourthWin,
		   hdValid,tlValid,cf->v1Compat,cf->simpleDiffs);
	  cf->nUsed += d;
	}
      }
   }
   /* Zero Mean the static coefficients if required */
   if ((cf->tgtPK&HASZEROM) && !(cf->curPK&HASZEROM)) {
     if (cf->MatTranFN == NULL || (!cf->preQual)) {
       d = span[1]-span[0]+1;
       if (cf->tgtPK&HASZEROC && !(cf->curPK&HASNULLE))  /* zero mean c0 too */
         ++d;
     } else { /* No idea where the statics are so do everything .... */
       d = span[1]-span[0]+1;
       if (cf->tgtPK&HASZEROC && !(cf->curPK&HASNULLE)) d++;
       if (cf->tgtPK&HASENERGY && !(cf->curPK&HASNULLE)) d++;
     }
     if (trace&T_QUA)
       printf("\nHParm:  zero-meaning first %d cols from %d rows [%s]",
       d,nRows,pbuf->isSpeech?"speech":"sil");
     if (pbuf->cf->calWindow>0)
        FZeroMeanCMN(data,d,nRows,cf->nCols,&cf->mean,pbuf->isSpeech);
     else
        FZeroMeanCMN(data,d,nRows,cf->nCols,&cf->mean,TRUE);

     cf->curPK |= HASZEROM;
   }
   if (trace&T_QUA) printf("\n");
   if ((cf->MatTranFN != NULL) && (!cf->preQual)) {
     if (cf->matPK != cf->curPK) {
       /* Need to check that the correct qualifiers are used */
       HError(999,"Incorrect qualifiers in parameter type (%s %s)",
	      ParmKind2Str(cf->curPK,buff1),ParmKind2Str(cf->matPK,buff2));
     }
     ApplyStaticMat(cf,data,cf->MatTran,cf->nCols,nRows,0,0);
   }
   if (trace&T_QUA)
     printf("\nHParm:  quals added to give %s\n",ParmKind2Str (cf->curPK, buf));
}

/* DelQualifiers: delete quals in cf->curPK but not in cf->tgtPK.
   Conversion is applied to the single row pointed to by data. */
static void DelQualifiers(float *data, IOConfig cf)
{
   char buf[100];
   int si,d,used=cf->nUsed;
   short span[12];
   Boolean baseX,statX,eX,zX;

   statX = (cf->curPK&BASEMASK) != (cf->tgtPK&BASEMASK);
   eX = (cf->curPK&HASENERGY) && !(cf->tgtPK&HASENERGY);
   zX = (cf->curPK&HASZEROC) && !(cf->tgtPK&HASZEROC);
   baseX = statX || eX || zX;
   if (trace&T_TOP)
      printf("HParm:  deleting Qualifiers in %s ...",ParmKind2Str(cf->curPK,buf));
   FindSpans(span,cf->curPK,cf->nUsed);
   /* Remove acc coefs if not required in target or statics will change */
   if ((cf->curPK&HASACCS) && (baseX || !(cf->tgtPK&HASACCS)) ) {
      si = span[6]; d = span[7]-span[6]+1;
      if (si<0) HError(6390,"DelQualifiers: no accs to remove");
      if (trace&T_QUA)
         printf("\nHParm:  removing %d accs at col %d",d,si);
      DeleteColumn(data,cf->nUsed,si,d);
      cf->curPK &= ~HASACCS; cf->nUsed -= d;
   }
   /* Remove del coefs if not required in target or statics will change */
   if ((cf->curPK&HASDELTA) && (baseX || !(cf->tgtPK&HASDELTA)) ) {
      si = span[4]; d = span[5]-span[4]+1;
      if (si<0) HError(6390,"DelQualifiers: no deltas to remove");
      if (trace&T_QUA)
         printf("\nHParm:  removing %d deltas at col %d",d,si);
      DeleteColumn(data,cf->nUsed,si,d);
      cf->curPK &= ~HASDELTA; cf->nUsed -= d;
   }
   /* Remove energy if not required in target  */
   if (eX) {
      si = span[3];
      if (si<0) HError(6390,"DelQualifiers: no energy to remove");
      if (trace&T_QUA)
         printf("\nHParm:  removing energy at col %d",si);
      DeleteColumn(data,cf->nUsed,si,1);
      cf->curPK &= ~HASENERGY; --cf->nUsed;
   }
   /* Remove c0 if not required in target  */
   if (zX) {
      si = span[2];
      if (si<0) HError(6390,"DelQualifiers: no c0 to remove");
      if (trace&T_QUA)
         printf("\nHParm:  removing c0 at col %d",si);
      DeleteColumn(data,cf->nUsed,si,1);
      cf->curPK &= ~HASZEROC; --cf->nUsed;
   }
   if (cf->nUsed!=used && (trace&T_QUA)) printf("\n");
   if (trace&T_TOP)
      printf("HParm:  quals deleted to give %s\n",ParmKind2Str(cf->curPK,buf));
}

/* XformLPC2LPREFC: Convert Static Coefficients LPC -> LPREFC */
static void XformLPC2LPREFC(float *data,int d)
{
   Vector a,k;
   int j;
   float *p;

   a = CreateVector(&parmHeap,d);  k = CreateVector(&parmHeap,d);
   p = data-1;
   for (j=1; j<=d; j++) a[j] = p[j];
   LPC2RefC(a,k);
   for (j=1; j<=d; j++) p[j] = k[j];
   FreeVector(&parmHeap,k); FreeVector(&parmHeap,a);
}

/* XformLPREFC2LPC: Convert Static Coefficients LPREFC -> LPC */
static void XformLPREFC2LPC(float *data,int d)
{
   Vector a,k;
   int j;
   float *p;

   a = CreateVector(&parmHeap,d);  k = CreateVector(&parmHeap,d);
   p = data-1;
   for (j=1; j<=d; j++) k[j] = p[j];
   RefC2LPC(k,a);
   for (j=1; j<=d; j++) p[j] = a[j];
   FreeVector(&parmHeap,k); FreeVector(&parmHeap,a);
}

/* XformLPC2LPCEPSTRA: Convert Static Coefficients LPC -> LPCEPSTRA */
static void XformLPC2LPCEPSTRA(float *data,int d,int dnew,int lifter)
{
   Vector a,c;
   int j;
   float *p;

   if (dnew>d)
      HError(6322,"XformLPC2LPCEPSTRA: lp cep size cannot exceed lpc vec");
   a = CreateVector(&parmHeap,d);  c = CreateVector(&parmHeap,dnew);
   p = data-1;
   for (j=1; j<=d; j++) a[j] = p[j];
   LPC2Cepstrum(a,c);
   if (lifter>0)
      WeightCepstrum(c,1,dnew,lifter);
   for (j=1; j<=d; j++) p[j] = c[j];
   FreeVector(&parmHeap,c); FreeVector(&parmHeap,a);
}

/* XformLPCEPSTRA2LPC: Convert Static Coefficients LPCEPSTRA -> LPC */
static void XformLPCEPSTRA2LPC(float *data,int d,int lifter)
{
   Vector a,c;
   int j;
   float *p;

   a = CreateVector(&parmHeap,d);  c = CreateVector(&parmHeap,d);
   p = data-1;
   for (j=1; j<=d; j++) c[j] = p[j];
   if (lifter>0)
      UnWeightCepstrum(c,1,d,lifter);
   Cepstrum2LPC(c,a);
   for (j=1; j<=d; j++) p[j] = a[j];
   FreeVector(&parmHeap,c); FreeVector(&parmHeap,a);
}

/* XformMELSPEC2FBANK: Convert Static Coefficients MELSPEC -> FBANK */
static void XformMELSPEC2FBANK(float *data,int d)
{
   Vector v;
   int j;
   float *p;

   v = CreateVector(&parmHeap,d);
   p = data-1;
   for (j=1; j<=d; j++) v[j] = p[j];
   MelSpec2FBank(v);
   for (j=1; j<=d; j++) p[j] = v[j];
   FreeVector(&parmHeap,v);
}

/* XformFBANK2MELSPEC: Convert Static Coefficients FBANK -> MELSPEC */
static void XformFBANK2MELSPEC(float *data,int d)
{
   Vector v;
   int j;
   float *p;

   v = CreateVector(&parmHeap,d);
   p = data-1;
   for (j=1; j<=d; j++) v[j] = p[j];
   FBank2MelSpec(v);
   for (j=1; j<=d; j++) p[j] = v[j];
   FreeVector(&parmHeap,v);
}

/* XformFBANK2MFCC: Convert Static Coefficients FBANK -> MFCC */
static void XformFBANK2MFCC(float *data,int d,int dnew,int lifter)
{
   Vector fbank,c;
   int j;
   float *p;

   if (dnew>d)
      HError(6322,"XformFBANK2MFCC: mfcc size cannot exceed fbank size");
   fbank = CreateVector(&parmHeap,d);  c = CreateVector(&parmHeap,dnew);
   p = data-1;
   for (j=1; j<=d; j++) fbank[j] = p[j];
   FBank2MFCC(fbank,c,dnew);
   if (lifter>0)
      WeightCepstrum(c,1,dnew,lifter);
   for (j=1; j<=d; j++) p[j] = c[j];
   FreeVector(&parmHeap,c); FreeVector(&parmHeap,fbank);
}

/* XformBase: convert statics to change basekind of cf->curPK to cf->tgtPK.
      Conversion is applied to a single row pointed to by data.  */
static void XformBase(float *data, IOConfig cf)
{
   char b1[50],b2[50];
   ParmKind curBase,tgtBase,quals;
   int d, dnew, lifter;
   short span[12];

   curBase = cf->curPK&BASEMASK;
   tgtBase = cf->tgtPK&BASEMASK;
   if (curBase == tgtBase) return;
   quals = cf->curPK&~BASEMASK;
   if (trace&T_TOP)
      printf("HParm: Attempting to xform static parms from %s to %s\n",
             ParmKind2Str(curBase,b1),ParmKind2Str(tgtBase,b2));
   FindSpans(span, cf->curPK, cf->nUsed);
   d = span[1]-span[0]+1; dnew = cf->numCepCoef; lifter = cf->cepLifter;
   switch (curBase) {
   case LPC:
      switch(tgtBase){
      case LPREFC:
         XformLPC2LPREFC(data,d);
         break;
      case LPCEPSTRA:
         XformLPC2LPCEPSTRA(data,d,dnew,lifter);
         if (dnew<d) {
            DeleteColumn(data,cf->nUsed,dnew,d-dnew);
            cf->nUsed -= d-dnew;
         }
         break;
      default:
         HError(6322,"XformBase: Bad target %s",ParmKind2Str(tgtBase,b1));
      }
      break;
   case LPREFC:
      switch(tgtBase){
      case LPC:
         XformLPREFC2LPC(data,d);
         break;
      case LPCEPSTRA:
         XformLPREFC2LPC(data,d);
         XformLPC2LPCEPSTRA(data,d,dnew,lifter);
         if (dnew<d) {
            DeleteColumn(data,cf->nUsed,dnew,d-dnew);
            cf->nUsed -= d-dnew;
         }
         break;
      default:
         HError(6322,"XformBase: Bad target %s",ParmKind2Str(tgtBase,b1));
      }
      break;
   case LPCEPSTRA:
      switch(tgtBase){
      case LPREFC:
         XformLPCEPSTRA2LPC(data,d,lifter);
         XformLPC2LPREFC(data,d);
         break;
      case LPC:
         XformLPCEPSTRA2LPC(data,d,lifter);
         break;
      default:
         HError(6322,"XformBase: Bad target %s",ParmKind2Str(tgtBase,b1));
      }
      break;
   case MELSPEC:
      switch(tgtBase){
      case FBANK:
         XformMELSPEC2FBANK(data,d);
         break;
      case MFCC:
         XformMELSPEC2FBANK(data,d);
         XformFBANK2MFCC(data,d,dnew,lifter);
         if (dnew<d) {
            DeleteColumn(data,cf->nUsed,dnew,d-dnew);
            cf->nUsed -= d-dnew;
         }
         break;
      default:
         HError(6322,"XformBase: Bad target %s",ParmKind2Str(tgtBase,b1));
      }
      break;
   case FBANK:
      switch(tgtBase){
      case MELSPEC:
         XformFBANK2MELSPEC(data,d);
         break;
      case MFCC:
         XformFBANK2MFCC(data,d,dnew,lifter);
         if (dnew<d) {
            DeleteColumn(data,cf->nUsed,dnew,d-dnew);
            cf->nUsed -= d-dnew;
         }
         break;
      default:
         HError(6322,"XformBase: Bad target %s",ParmKind2Str(tgtBase,b1));
      }
      break;
   default:
      HError(6322,"XformBase: Bad source %s",ParmKind2Str(curBase,b1));
   }
   cf->curPK = tgtBase|quals;
   if (trace&T_TOP)
      printf("HParm: Xform complete,  current is %s\n",ParmKind2Str(cf->curPK,b1));
}

/* ----------------- Parameter Coding Routines -------------------- */

/* ZeroMeanFrame: remove dc offset from given vector */
void ZeroMeanFrame(Vector v)
{
   int size,i;
   float sum=0.0,off;

   size = VectorSize(v);
   for (i=1; i<=size; i++) sum += v[i];
   off = sum / size;
   for (i=1; i<=size; i++) v[i] -= off;
}

/* SetupMeanRec: Read default cepstral mean vector if any */
static void SetupMeanRec(MeanRec *m, IOConfig cf)
{
   Source src;
   char buf[MAXSTRLEN], *ptr;
   int i;

   m->frames = 0;
   m->defMeanVec = NULL;
   m->curMeanVec = NULL;
   m->minFrames = cf->cmnMinFrames;
   m->tConst = cf->cmnTConst;
   if (cf->cmnDefault != NULL){
       if (InitSource(cf->cmnDefault,&src,NoFilter)==SUCCESS){
		   if (ReadString(&src, buf) && (strcmp(buf,"<MEAN>") == 0)){
			   if (ReadInt(&src,&i,1,FALSE)) {
				   m->defMeanVec=CreateVector(&gcheap,i);
				   if (ReadFloat(&src,m->defMeanVec+1,i,FALSE)) {
					   m->curMeanVec=CreateVector(&gcheap,i);
					   CopyVector(m->defMeanVec, m->curMeanVec);
				   }else{
					   FreeVector(&gcheap,m->defMeanVec);
					   m->defMeanVec=NULL;
				   }
			   }
		   }
       } else {
	 HError(6371,"Cannot find cepmean %s\n",cf->cmnDefault);
       }
   }
}

/* EXPORT->ResetMeanRec: Reset the mean record */
void ResetMeanRec(ParmBuf pbuf)
{
   MeanRec *m = &pbuf->cf->mean;
   m->frames = 0;
   if (m->defMeanVec != NULL)
     CopyVector(m->defMeanVec, m->curMeanVec);
   if (trace&T_TOP) { printf("Cep Mean reset\n"); fflush(stdout); }
}

/* SetUpForCoding: set style, sizes and  working storage */
static void SetUpForCoding(MemHeap *x, IOConfig cf, int frSize)
{
   char buf[50];
   ParmKind btgt;

   cf->s = CreateVector(x,frSize);
   cf->r = CreateShortVec(x,frSize);
   cf->curPK = btgt = cf->tgtPK&BASEMASK;
   cf->a = cf->k = cf->c = cf->fbank = NULL;
   SetCodeStyle(cf);
   switch(cf->style){
   case LPCbased:
      cf->nUsed = (btgt==LPCEPSTRA)?cf->numCepCoef:cf->lpcOrder;
      if (btgt==LPREFC)
         cf->k = CreateVector(x,cf->lpcOrder);
      else
         cf->a = CreateVector(x,cf->lpcOrder);
      if (btgt == LPCEPSTRA)
         cf->c = CreateVector(x,cf->numCepCoef);
      break;
   case FFTbased:
      cf->nUsed = (btgt==MFCC || btgt == PLP)?cf->numCepCoef:cf->numChans;
      cf->fbank = CreateVector(x,cf->numChans);
      cf->fbInfo = InitFBank (x, frSize, (long) cf->srcSampRate, cf->numChans,
                              cf->loFBankFreq, cf->hiFBankFreq, cf->usePower,
                              (btgt == PLP) ? FALSE : btgt != MELSPEC,
                              cf->doubleFFT,
                              cf->warpFreq, cf->warpLowerCutOff, cf->warpUpperCutOff);

      if (btgt != PLP) {
         if (btgt == MFCC)
            cf->c = CreateVector(x,cf->numCepCoef);
      }
      else {            /* initialisation for PLP */
         cf->c = CreateVector (x, cf->numCepCoef+1);
         cf->as = CreateVector (x, cf->numChans+2);
         cf->eql = CreateVector (x, cf->numChans);
         cf->ac = CreateVector (x, cf->lpcOrder+1);
         cf->lp = CreateVector (x, cf->lpcOrder+1);
         cf->cm = CreateDMatrix (x, cf->lpcOrder+1, cf->numChans+2);
         InitPLP (cf->fbInfo, cf->lpcOrder, cf->eql, cf->cm);
      }
      break;
   default:
      HError(6321,"SetUpForCoding: target %s is not a parameterised form",
             ParmKind2Str(cf->tgtPK,buf));
   }
   if (cf->tgtPK&HASENERGY) {
      cf->curPK |= HASENERGY; ++cf->nUsed;
   }
   if (cf->tgtPK&HASZEROC) {
      cf->curPK |= HASZEROC; ++cf->nUsed;
   }
   if (!ValidConversion(cf->curPK,cf->tgtPK))
      HError(6322,"SetUpForCoding: cannot convert to %s",ParmKind2Str(cf->tgtPK,buf));
   if (cf->MatTranFN == NULL)
      cf->tgtUsed = TotalComps(NumStatic(cf->nUsed,cf->curPK),cf->tgtPK);
   else {
      if (cf->preQual)
         cf->tgtUsed = NumRows(cf->MatTran)*(1+HasDelta(cf->tgtPK)+HasAccs(cf->tgtPK)+HasThird(cf->tgtPK));
      else
	cf->tgtUsed = NumRows(cf->MatTran);
   }
   cf->nCols=TotalComps(NumStatic(cf->nUsed,cf->curPK),cf->tgtPK);
   cf->nCols = (cf->nCols>cf->tgtUsed)?cf->nCols:cf->tgtUsed;
   cf->nCvrt = cf->nUsed;
}

/* ConvertFrame: convert frame in cf->s and store in pbuf, return total
   parameters stored in pbuf */
static int ConvertFrame(IOConfig cf, float *pbuf)
{
   ParmKind btgt = cf->tgtPK&BASEMASK;
   float re,rawte,te,*p, cepScale = 1.0;
   int i,bsize;
   Vector v;
   char buf[50];
   Boolean rawE;

   p = pbuf;
   rawE = cf->rawEnergy;
   if (btgt<MFCC && cf->v1Compat)
      rawE = FALSE;

   if (cf->addDither!=0.0)
      for (i=1; i<=VectorSize(cf->s); i++)
         cf->s[i] += (RandomValue()*2.0 - 1.0)*cf->addDither;

   if (cf->zMeanSrc && !cf->v1Compat)
      ZeroMeanFrame(cf->s);
   if ((cf->tgtPK&HASENERGY) && rawE){
      rawte = 0.0;
      for (i=1; i<=VectorSize(cf->s); i++)
         rawte += cf->s[i] * cf->s[i];
   }
   if (cf->preEmph>0.0)
      PreEmphasise(cf->s,cf->preEmph);
   if (cf->useHam) Ham(cf->s);
   switch(btgt){
   case LPC:
      Wave2LPC(cf->s,cf->a,cf->k,&re,&te);
      v = cf->a; bsize = cf->lpcOrder;
      break;
   case LPREFC:
      Wave2LPC(cf->s,cf->a,cf->k,&re,&te);
      v = cf->k; bsize = cf->lpcOrder;
      break;
   case LPCEPSTRA:
      Wave2LPC(cf->s,cf->a,cf->k,&re,&te);
      LPC2Cepstrum(cf->a,cf->c);
      if (cf->cepLifter > 0)
         WeightCepstrum(cf->c, 1, cf->numCepCoef, cf->cepLifter);
      v = cf->c; bsize = cf->numCepCoef;
      break;
   case MELSPEC:
   case FBANK:
      Wave2FBank(cf->s, cf->fbank, rawE?NULL:&te, cf->fbInfo);
      v = cf->fbank; bsize = cf->numChans;
      break;
   case MFCC:
      Wave2FBank(cf->s, cf->fbank, rawE?NULL:&te, cf->fbInfo);
      FBank2MFCC(cf->fbank, cf->c, cf->numCepCoef);
      if (cf->cepLifter > 0)
         WeightCepstrum(cf->c, 1, cf->numCepCoef, cf->cepLifter);
      v = cf->c; bsize = cf->numCepCoef;
      break;
   case PLP:
      Wave2FBank(cf->s, cf->fbank, rawE ? NULL : &te, cf->fbInfo);
      FBank2ASpec(cf->fbank, cf->as, cf->eql, cf->compressFact, cf->fbInfo);
      ASpec2LPCep(cf->as, cf->ac, cf->lp, cf->c, cf->cm);
      if (cf->cepLifter > 0)
         WeightCepstrum(cf->c, 1, cf->numCepCoef, cf->cepLifter);
      v = cf->c;
      bsize = cf->numCepCoef;
      break;
   default:
      HError(6321,"ConvertFrame: target %s is not a parameterised form",
             ParmKind2Str(cf->tgtPK,buf));
   }
   if (btgt == PLP || btgt == MFCC)
      cepScale = (cf->v1Compat) ? 1.0 : cf->cepScale;
   for (i=1; i<=bsize; i++)
      *p++ = v[i] * cepScale;

   if (cf->tgtPK&HASZEROC){
      if (btgt == MFCC) {
         *p = FBank2C0(cf->fbank) * cepScale;
         if (cf->v1Compat) *p *= cf->eScale;
         ++p;
      }
      else      /* For PLP include gain as C0 */
         *p++ = v[bsize+1] * cepScale;
      cf->curPK|=HASZEROC ;
   }
   if (cf->tgtPK&HASENERGY) {
      if (rawE) te = rawte;
      *p++ = (te<MINLARG) ? LZERO : log(te);
       cf->curPK|=HASENERGY;
   }
   return p - pbuf;
}

/* Get data from external source and convert to 16 bit linear */
static int fGetWaveData(int n, void *data, short *res,
                        HParmSrcDef ext, void *bInfo)
{
   int r,i;

   r=ext->fGetData(ext->xInfo,bInfo,n,data);
   /* Transfer whole frame to processing buffer */
   if (ext->size==1) {
      /* 8 bit mulaw */
      unsigned char *d;
      static short u2l[]={
         -32124,-31100,-30076,-29052,-28028,-27004,-25980,-24956,
         -23932,-22908,-21884,-20860,-19836,-18812,-17788,-16764,
         -15996,-15484,-14972,-14460,-13948,-13436,-12924,-12412,
         -11900,-11388,-10876,-10364,-9852, -9340, -8828, -8316,
         -7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140,
         -5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
         -3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004,
         -2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
         -1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436,
         -1372, -1308, -1244, -1180, -1116, -1052, -988,  -924,
         -876,  -844,  -812,  -780,  -748,  -716,  -684,  -652,
         -620,  -588,  -556,  -524,  -492,  -460,  -428,  -396,
         -372,  -356,  -340,  -324,  -308,  -292,  -276,  -260,
         -244,  -228,  -212,  -196,  -180,  -164,  -148,  -132,
         -120,  -112,  -104,  -96,   -88,   -80,   -72,   -64,
         -56,   -48,   -40,   -32,   -24,   -16,   -8,    0,
         32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956,
         23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
         15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412,
         11900, 11388, 10876, 10364, 9852,  9340,  8828,  8316,
         7932,  7676,  7420,  7164,  6908,  6652,  6396,  6140,
         5884,  5628,  5372,  5116,  4860,  4604,  4348,  4092,
         3900,  3772,  3644,  3516,  3388,  3260,  3132,  3004,
         2876,  2748,  2620,  2492,  2364,  2236,  2108,  1980,
         1884,  1820,  1756,  1692,  1628,  1564,  1500,  1436,
         1372,  1308,  1244,  1180,  1116,  1052,  988,   924,
         876,   844,   812,   780,   748,   716,   684,   652,
         620,   588,   556,   524,   492,   460,   428,   396,
         372,   356,   340,   324,   308,   292,   276,   260,
         244,   228,   212,   196,   180,   164,   148,   132,
         120,   112,   104,   96,    88,    80,    72,    64,
         56,    48,    40,    32,    24,    16,    8,     0    };

      for (i=0,d=(unsigned char*)data;i<n;i++) res[i]=u2l[*d++];
   }
   if (ext->size==0x0101) {
      /* 8 bit alaw */
      unsigned char *d;
      static short a2l[]={
         -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
         -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
         -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
         -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
         -22016,-20992,-24064,-23040,-17920,-16896,-19968,-18944,
         -30208,-29184,-32256,-31232,-26112,-25088,-28160,-27136,
         -11008,-10496,-12032,-11520, -8960, -8448, -9984, -9472,
         -15104,-14592,-16128,-15616,-13056,-12544,-14080,-13568,
         -344,  -328,  -376,  -360,  -280,  -264,  -312,  -296,
         -472,  -456,  -504,  -488,  -408,  -392,  -440,  -424,
         -88,   -72,   -120,  -104,  -24,   -8,    -56,   -40,
         -216,  -200,  -248,  -232,  -152,  -136,  -184,  -168,
         -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
         -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
         -688,  -656,  -752,  -720,  -560,  -528,  -624,  -592,
         -944,  -912,  -1008, -976,  -816,  -784,  -880,  -848,
         5504,  5248,  6016,  5760,  4480,  4224,  4992,  4736,
         7552,  7296,  8064,  7808,  6528,  6272,  7040,  6784,
         2752,  2624,  3008,  2880,  2240,  2112,  2496,  2368,
         3776,  3648,  4032,  3904,  3264,  3136,  3520,  3392,
         22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
         30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
         11008, 10496, 12032, 11520, 8960,  8448,  9984,  9472,
         15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
         344,   328,   376,   360,   280,   264,   312,   296,
         472,   456,   504,   488,   408,   392,   440,   424,
         88,    72,    120,   104,   24,    8,     56,    40,
         216,   200,   248,   232,   152,   136,   184,   168,
         1376,  1312,  1504,  1440,  1120,  1056,  1248,  1184,
         1888,  1824,  2016,  1952,  1632,  1568,  1760,  1696,
         688,   656,   752,   720,   560,   528,   624,   592,
         944,   912,  1008,   976,   816,   784,   880,   848 };

      for (i=0,d=(unsigned char*)data;i<n;i++) res[i]=a2l[*d++];
   }
   if (ext->size==2) {
      /* 16 bit linear */
      short *d;
      for (i=0,d=(short*)data;i<n;i++) res[i]=*d++;
   }
   return(r);
}

/* ---------------- Silence detector code ------------------- */

#define DEF_SNR 30

enum {
   ML_SIL_ST=2,
   ML_SIL_EN=3,
   ML_SP_ST=8,
   ML_SP_EN=9,
   ML_PARTS=10
};

static int fcmp(const void *v1,const void *v2)
{
   float f1,f2;
   f1=*(float*)v1;f2=*(float*)v2;
   if (f1<f2) return(-1);
   else if (f1>f2) return(1);
   else return(0);
}

/* SetSpDetParms: set detector parameters on current spVal buffer */
static Boolean SetSpDetParms(ParmBuf pbuf, Boolean calInhibited)
{
   Vector eFr;
   ChannelInfo *chan = pbuf->chan;
   IOConfig cf = pbuf->cf;
   float sil,newSil,low,sp,snr,range,thresh,nBl,aBl[ML_PARTS+1];
   int i,j,s,b,c,e,nFr,ndFr;
   Boolean changed = FALSE;

   nFr=pbuf->inRow - pbuf->spValLast;
   if (nFr>cf->calWindow) nFr = cf->calWindow;
   assert(nFr>2);
   eFr=CreateVector(&parmHeap,nFr);
   for (i=1,j=pbuf->inRow-1; i<=nFr; i++,j--) eFr[i]=pbuf->spVal[j];

   if (trace&T_CAL){
      printf("SetSpDetParms: %s NFr=%d spDetThresh=%.1f silDiscard=%.1f spThresh=%.2f\n",
         chan->spDetParmsSet?"Set":"Unset",
         nFr,chan->spDetThresh,cf->silDiscard, cf->spThresh);
      /*printf("Unsorted Frame levels\n");
      for (j=1;j<=nFr;j++) {
         printf(" %5.2f",eFr[j]); if ((j%10)==0) printf("\n");
      }*/
   }

   /* Do partition and decide on levels */
   qsort((void*)(eFr+1),nFr,sizeof(float),fcmp);

   for(i=1,s=0,sil=0.0;i<=nFr;i++) {
     if (eFr[i]<cf->silDiscard) { s++ ; sil+=eFr[i];}
   }
   aBl[0]=(s>0?sil/s:0.0); ndFr=nFr-s;
   for(b=1;b<=ML_PARTS;b++) aBl[b]=0.0;
   if (ndFr>=ML_PARTS) {
      /* Find number of frames in each bin */
      nBl=((float)ndFr)/ML_PARTS;
      /* Find average energy in each bin */
      for(b=1,i=s+1,thresh=s+nBl;b<=ML_PARTS;b++,thresh+=nBl) {
         for (c=0,e=(int)(thresh+0.5);i<=e;i++,c++) aBl[b]+=eFr[i];
         aBl[b]=(c>0?aBl[b]/c:0.0);
      }
      for (sil=0,i=ML_SIL_ST;i<=ML_SIL_EN;i++) sil+=aBl[i];
      for (sp=0,i=ML_SP_ST;i<=ML_SP_EN;i++) sp+=aBl[i];

      if (calInhibited){ /* ie there is a prompt playing */
         /* silence and speech are the same */
         sp = (sil+sp)/(ML_SIL_EN - ML_SIL_ST +1  + ML_SP_EN-ML_SP_ST+1);
         sil = sp;
      }else{
         sil=sil/(ML_SIL_EN-ML_SIL_ST+1);
         sp=sp/(ML_SP_EN-ML_SP_ST+1);
         /* If person speaks continuously this sil estimate may be too high */
         /*  so check lowest level is similar to silence level */
         low=aBl[1];
         if (sil-low>cf->spThresh) {
            /* Silence level is dodgy replace with lowest estimate */
            sil=low;
         }
      }
      snr = sp - sil; range = eFr[nFr]-eFr[1];
      if (calInhibited || (range>cf->spThresh && snr>cf->spThresh)) {
         /* Either we have a reasonable SNR - so accept this estimate of sil */
         /* or if calInhibited, then accept sil on temporary basis */
         chan->spDetParmsSet = TRUE;
      }
      else {
         /* SNR ratio is abysmal - assume signal was basically silence */
         chan->spDetParmsSet = FALSE;
      }

      /* update the silence estimate */
      newSil = (1.0-cf->silUpdateRate)*chan->spDetSil + cf->silUpdateRate * sil;
      thresh = newSil + cf->spThresh;

      /* assign computed results to channel variables */
      if ( chan->spDetParmsSet || chan->spDetThresh<0.0){
         chan->frMin=eFr[1];      chan->frMax=eFr[nFr];
         chan->spDetSil=newSil;      chan->spDetSp=sp;
         chan->spDetSNR=snr;      chan->spDetThresh=thresh;
         changed = TRUE;
      }
      if (trace&T_CAL) {
         /*printf("Sorted Frame levels\n");
         for (j=1;j<=nFr;j++) {
            printf(" %5.2f",eFr[j]); if ((j%10)==0) printf("\n");
         }*/
         printf("Blockgram: ");
         for (j=0;j<=ML_PARTS;j++) printf(" %.2f",aBl[j]);
         printf("\n");
      }
      if ((trace&T_TOP)||(trace&T_CAL)) {
         printf("Levels: Min %.1fdB, Max %.1fdB\n", chan->frMin,chan->frMax);
         printf("        Silence %.1fdB[%.1fdB], Thresh %.1fdB, Speech %.1fdB (SNR %.1f, RANGE %.1f)\n",
            newSil,sil,thresh,sp,snr,range);
         if (changed) printf("SPDETPARMS UPDATED"); else printf("SPDETPARMS NOT CHANGED");
         if (calInhibited) printf(" (inhibited)"); printf("\n");
      }
   }else{
      if ((trace&T_TOP)||(trace&T_CAL)) printf("SPDETPARMS ABORTED\n");
   }
   FreeVector(&parmHeap, eFr);
   return chan->spDetParmsSet;
}

/* Decide whether current frame is speech or not */
static Boolean DetectSpeech(ParmBuf pbuf)
{
   IOConfig cf = pbuf->cf;
   float thresh = pbuf->chan->spDetThresh;
   Boolean changed = FALSE;
   int oldest,winsize,i,n;

   if ((trace&T_DET)){
     printf("DetectSpeech: %s[lev=%.1fdB, thresh=%.1fdB] outrow=%d -> ",
	    pbuf->isSpeech?"speech":"silence",pbuf->spVal[pbuf->outRow],thresh,pbuf->outRow);
   }

   /* compute counts */
   winsize= (pbuf->isSpeech)?cf->silSeqCount:cf->spcSeqCount;
   oldest = pbuf->inRow - winsize;
   if (oldest>pbuf->outRow) oldest = pbuf->outRow;
   pbuf->spDetCnt=pbuf->silDetCnt=0;
   for (n=1,i=oldest; n<=winsize; i++,n++) {
     if (pbuf->spVal[i]>thresh) ++pbuf->spDetCnt;
     else ++pbuf->silDetCnt;
   }

   /* Classify speech based on the running counts */
   if (pbuf->isSpeech) {  /* in speech so looking for silence */
      if(pbuf->spDetCnt  <= cf->spcGlchCount) {
         pbuf->isSpeech = FALSE; pbuf->spDetFrms=0;
      }
      if(cf->maxSpcFrames!=0 && ++pbuf->spDetFrms>cf->maxSpcFrames){
         pbuf->isSpeech=FALSE; pbuf->spDetFrms=0;
      }
   }else{ /* in silence so looking for speech */
      if (pbuf->silDetCnt <= cf->silGlchCount) pbuf->isSpeech = TRUE;
   }

   if ((trace&T_DET)){
     printf("%s silCnt=%d spCnt=%d  ",
	    pbuf->isSpeech?"speech":"silence",pbuf->silDetCnt, pbuf->spDetCnt);
     printf("inRow=%d oldest=%d winsize=%d\n",pbuf->inRow,oldest,winsize);
   }
   return pbuf->isSpeech;
}

/* EXPORT->CalibrateSilDet: force recalibration */
void CalibrateSilDet(ParmBuf pbuf)
{
  if (pbuf->cf->calWindow != 0)
    ChangeState(pbuf,PB_CALIBRATING);
  pbuf->chan->spDetParmsSet = FALSE;
}

/*  EXPORT->InhibitCalibration: When inhibit, prevent calibration */
void InhibitCalibration(ParmBuf pbuf, Boolean inhibit)
{
   if (pbuf->cf->enableInhibit)
      pbuf->chan->calInhibit = inhibit;
}

/* EXPORT->GetSilDetParms: get the current sildet parms */
Boolean GetSilDetParms(ParmBuf pbuf, float *sil, float *snr,
		       float *sp, float *thresh)
{
  /* if (!pbuf->chan->spDetParmsSet) return FALSE; */
  *snr = pbuf->chan->spDetSNR;
  *sil = pbuf->chan->spDetSil;
  *sp = pbuf->chan->spDetSp;
  *thresh = pbuf->chan->spDetThresh;
  return TRUE;
}

/* EXPORT->ResetIsSpeech: reset isSpeech decision to silence */
void ResetIsSpeech(ParmBuf pbuf)
{
   if (trace&T_DET)
      printf("Sildet reset\n");
   pbuf->isSpeech = FALSE;
}

/* ----------------- Observation Handling Routines -------------- */

/* ExtractObservation: copy vector of floats starting at fp into
   observation, splitting into streams as necessary */
static void ExtractObservation(float *fp, Observation *o)
{
   int i,j,k,n,w1,w2,w,nStatic;
   int numS = o->swidth[0];
   Vector v,ev;
   Boolean wantE,skipE;

   if (trace&T_OBS) {
      for (i=1,j=0; i<=numS; i++) j += o->swidth[i];
      printf("HParm: Extracting %d observation components\n",j);
      for (i=0; i<j; i++) printf("%8.4f ",fp[i]); printf("\n");
   }
   if (o->eSep){
      wantE = !(o->pk&HASNULLE);
      if (numS == 2){
         w1 = o->swidth[1]; w2 = NumEnergy(o->pk);
         w = w1+w2; n = w/w2;
         v = o->fv[1]; ev = o->fv[numS];
         for (i=j=k=1; i<=w; i++){
            if (i%n == 0 )  {
               if (wantE || i>n) ev[k++] = *fp;
               fp++;
            } else
               v[j++] = *fp++;
         }
      } else {
         ev = o->fv[numS];
         for (i=1,k=1; i<numS; i++){
            v = o->fv[i];
            for (j=1; j<=o->swidth[i]; j++)
               v[j] = *fp++;
            if (wantE || i>1) ev[k++] = *fp;
            fp++;
         }
      }
      if (k-1 != o->swidth[numS])
         HError(6391,"ExtractObservation: %d of %d E vals copied",
                k-1,o->swidth[numS]);
   } else {
      skipE = (o->pk&(HASENERGY|HASZEROC)) && (o->pk&HASNULLE);
      if (skipE) {
         nStatic = o->swidth[1];
         if (numS==1) nStatic = (nStatic+1)/NumEnergy(o->pk) - 1;
      }
      for (i=1,k=0; i<=numS; i++){
         v = o->fv[i];
         for (j=1; j<=o->swidth[i]; j++){
            v[j] = *fp++; k++;
            if (skipE && k==nStatic) ++fp;
         }
      }
   }
}

/* EXPORT->MakeObservation: Create obs using info in swidth and pkind */
Observation MakeObservation(MemHeap *x, short *swidth,
                            ParmKind pkind, Boolean forceDisc, Boolean eSep)
{
   Observation ob;
   int i,numS;

   ob.pk = pkind; ob.bk = pkind&(~HASNULLE); ob.eSep = eSep;
   for (i=0; i<SMAX; i++) ob.fv[i]=NULL,ob.vq[i]=-1;
   if (forceDisc) {
      if ((pkind&BASEMASK) != DISCRETE && !(pkind&HASVQ))
         HError(6373,"MakeObservation: No way to force discrete observation");
      ob.pk = DISCRETE+(pkind&HASNULLE);
   }
   numS = swidth[0];
   if (numS>=SMAX)
      HError(6372,"MakeObservation: num streams(%d) > MAX(%d)",numS,SMAX-1);
   for (i=0; i<=numS; i++){
      ob.swidth[i] =swidth[i];
      if (i>0 && (pkind&BASEMASK) == DISCRETE && swidth[i] != 1)
         HError(6372,"MakeObservation: discrete stream widths must be 1");
   }
   /* Note that the vectors are created even if ob.pk==DISCRETE as */
   /* these are used in ReadAs????? but should not be accessed elsewhere */
   if ((pkind&BASEMASK) != DISCRETE)
      for (i=1; i<=numS; i++)
         ob.fv[i] = CreateVector(x,swidth[i]);
   return ob;
}

#define OBMARGIN 6         /* margin width for displaying observations */
#define OBFLTFORM "%8.3f"  /* format for displaying component values */
#define OBEXPFORM "%8s"    /* format for displaying component names */

/* EXPORT->ExplainObservation: explain the structure of given observation */
void ExplainObservation(Observation *o, int itemsPerLine)
{
   char idx[10],str[5],buf[20],blank[10];
   int s,j,k,len,n,nTotal,nStatic,numS,nDel;
   Boolean idd = FALSE,isE;

   for (j=0; j<OBMARGIN; j++) blank[j] = ' ';
   blank[OBMARGIN] = '\0';
   numS = o->swidth[0];
   for (s=1,nTotal=0; s<=numS; s++)
      nTotal += o->swidth[s];
   if (o->pk&HASNULLE) ++nTotal;
   n=1;
   if (o->pk&HASDELTA) {
      ++n; if (o->pk&HASACCS) {
	++n;
        if (o->pk&HASTHIRD){
           ++n;
           if (highDiff == TRUE){
              ++n;
           }
        }
      }
   }
   nStatic = nTotal/n;
   if (o->eSep) --nStatic;
   nDel = nStatic*2;
   if (o->pk&HASNULLE && !o->eSep) { --nDel; --nStatic; --nTotal;}
   if (trace&T_OBS)
      printf("HParm: ExplainObs: nTotal=%d, nStatic=%d, nDel=%d\n",nTotal,nStatic,nDel);
   if ((o->pk&BASEMASK) == DISCRETE || (o->pk&HASVQ)){
      strcpy(buf,"x:");
      len = strlen(buf);
      while (len++<OBMARGIN) strcat(buf," ");
      printf("%s",buf);
      for (s=1; s<=numS; s++) printf(" VQ[%d]=VQindex ",s);
      printf("\n");
      idd = TRUE;
   }
   if ((o->pk&BASEMASK) == DISCRETE) return;
   for (s=1; s<=o->swidth[0]; s++){
      buf[0] = str[0] = idx[0] = '\0';
      if (!idd || numS>1) {
         strcpy(idx,"x");
         if (numS>1) sprintf(str,".%d",s);
         strcpy(buf,idx); strcat(buf,str); strcat(buf,":");
      }
      len = strlen(buf);
      while (len++<OBMARGIN) strcat(buf," ");
      printf("%s",buf);
      for (j=1,k=0; j<=o->swidth[s]; j++){
         n = j;
         if (o->eSep && s==numS){
            strcpy(str,(o->pk&HASENERGY)?"E":"C0"); isE = TRUE;
            switch((o->pk&HASNULLE)?j+1:j){
            case 1:
               strcpy(buf,str); break;
            case 2:
               strcpy(buf,"Del"); strcat(buf,str);
               break;
            case 3:
               strcpy(buf,"Acc"); strcat(buf,str);
               break;
            }
         } else {
            strcpy(str,""); strcpy(buf,""); isE = FALSE;
            if ((o->pk&(HASENERGY|HASZEROC)) && !o->eSep ) {
               if(s==1)
                  isE = (!(o->pk&HASNULLE) && (j==nStatic)) || (j==nDel) || (j==nTotal);
               else
                  isE = j==o->swidth[s];
            }
            if (isE) strcpy(str,(o->pk&HASENERGY)?"E":"C0");
            switch(s){
            case 1:
               if (j<=nStatic) {
                  if(!isE) {
                     ParmKind2Str(BaseParmKind(o->pk),buf);
                     buf[5] = '\0';   /* truncate to 5 chars max */
                  }
               } else if (j<=nDel) {
                  strcpy(buf,"Del"); n -= nStatic;
               } else {
                  strcpy(buf,"Acc"); n -= nDel;
               }
               break;
            case 2:
               strcpy(buf,"Del"); break;
            case 3:
               strcpy(buf,"Acc"); break;
            }
            strcat(buf,str);
            if (!isE) {
               sprintf(idx,"-%d",n); strcat(buf,idx);
            }
         }
         printf(OBEXPFORM,buf);
         if (++k == itemsPerLine) {
            printf("\n%s",(j<o->swidth[s])?blank:"");
            k = 0;
         }
      }
      if (k>0)printf("\n");
   }
}

/* EXPORT->PrintObservation: Print o with index i (if i>0) */
void PrintObservation(int i, Observation *o, int itemsPerLine)
{
   char idx[10],str[5],buf[20],blank[10];
   int s,j,k,len;
   Boolean idd = FALSE;

   for (j=0; j<OBMARGIN; j++) blank[j] = ' ';
   blank[OBMARGIN] = '\0';

   if ((o->pk&BASEMASK) == DISCRETE || (o->pk&HASVQ)){
      if (i>=0) sprintf(buf,"%d:",i);
      len = strlen(buf);
      while (len++<OBMARGIN) strcat(buf," ");
      printf("%s",buf);
      for (s=1; s<=o->swidth[0]; s++) printf(" VQ[%d]=%d ",s,o->vq[s]);
      printf("\n");
      idd = TRUE;
   }
   if ((o->pk&BASEMASK) == DISCRETE) return;
   for (s=1; s<=o->swidth[0]; s++){
      buf[0] = str[0] = idx[0] = '\0';
      if ((i>=0 && !idd) || (i>=0 && o->swidth[0]>1)) {
         sprintf(idx,"%d",i);
         if (o->swidth[0]>1) sprintf(str,".%d",s);
         strcpy(buf,idx); strcat(buf,str); strcat(buf,":");
      }
      len = strlen(buf);
      while (len++<OBMARGIN) strcat(buf," ");
      printf("%s",buf);
      for (j=1,k=0; j<=o->swidth[s]; j++){
         printf(OBFLTFORM,o->fv[s][j]);
         if (++k == itemsPerLine) {
            printf("\n%s",(j<o->swidth[s])?blank:"");
            k = 0;
         }
      }
      if (k>0)printf("\n");
   }
   fflush(stdout);
}

/* ZeroStreamWidths: store numS in swidth[0] and set rest to 0 */
void ZeroStreamWidths(int numS, short *swidth)
{
   int s;

   if (numS >=SMAX)
      HError(6372,"ZeroStreamWidths: num streams(%d) > MAX(%d)\n",
             numS,SMAX-1);
   swidth[0] = numS;
   for (s=1; s<=numS; s++) swidth[s] = 0;
}

/* EXPORT->SetStreamWidths: if not already set, put stream widths in swidth
           for desired stream split and set eSep.  Otherwise just set eSep */
void  SetStreamWidths(ParmKind pk, int size, short *swidth, Boolean *eSep)
{
   Boolean isSet,ok;
   short span[12], sw[SMAX];
   char buf[50];
   int s,neTab,neObs;

   /* adjust the target vector size when _N used */
   if ((pk&(HASENERGY|HASZEROC)) && (pk&HASNULLE))
      size++;

   *eSep = FALSE;  ok = TRUE;
   if ((pk&BASEMASK)==DISCRETE){
      swidth[0] = size;
      for (s=1; s<=swidth[0]; s++) swidth[s]=1;
      return;
   }
   isSet = swidth[1] != 0;
   ZeroStreamWidths(swidth[0],sw);
   FindSpans(span,pk,size);
   neObs = neTab = NumEnergy(pk);
   if (pk&HASNULLE) --neObs;
   switch (sw[0]) {
   case 1:
      sw[1] = size;
      if (pk&HASNULLE) --sw[1];
      break;
   case 2:
      if (pk&(HASENERGY|HASZEROC)){
         sw[2] = neObs;
         sw[1] = size - neTab;
         *eSep = TRUE;
      } else if (!(pk&HASACCS) && (pk&HASDELTA)){
         sw[2] = span[5]-span[4]+1;
         sw[1] = size - sw[2];
      } else
         ok = FALSE;
      break;
   case 3:
      if (pk&HASACCS){
         sw[2] = span[5]-span[4]+1;
         sw[3] = span[7]-span[6]+1;
         sw[1] = size - sw[2] - sw[3];
         if (pk&HASNULLE) --sw[1];
      } else if ((pk&HASDELTA) && (pk&(HASENERGY|HASZEROC))){
         sw[1] = sw[2] = span[1]-span[0]+1;
         sw[3] = neObs;
         *eSep = TRUE;
      } else
         ok = FALSE;
      break;
   case 4:
      if ((pk&HASACCS) && (pk&(HASENERGY|HASZEROC))){
         sw[1] = sw[2] = sw[3] = span[1]-span[0]+1;
         sw[4] = neObs;
         *eSep = TRUE;
      } else
         ok = FALSE;
      break;
   }
   if (ok && isSet){   /* see if standard split, if not clear eSep */
      for (s=1; s<=sw[0]; s++)
         if (sw[s] != swidth[s]){
            *eSep = FALSE; break;
         }
   } else if (ok && !isSet) {   /* return standard split */
      for (s=1; s<=sw[0]; s++)
         swidth[s] = sw[s];
   } else if (!ok && isSet){
      *eSep = FALSE;            /* must be non standard split */
   } else
      HError(6372,"SetStreamWidths: cant split %s into %d streams",
             ParmKind2Str(pk,buf),swidth[0]);
}

/* ------------------- Channel Operations ------------------- */

/* Get a single frame from particular channel */
/*  Return value indicates number of frames read okay */
static Boolean GetFrameFromChannel(ParmBuf pbuf,float *fp)
{
   IOConfig cf = pbuf->cf;
   int r,i,j,x,n;
   double m,e;

   /* First Copy overlap */
   if (pbuf->inRow>0) {
     n=cf->frRate;              /* num new samples needed */
     x=cf->frSize-cf->frRate;   /* size of overlap */
     for (i=1;i<=x;i++)         /* copy overlap */
       cf->r[i]=cf->r[i+cf->frRate];
   } else {                      /* first frame so no */
     n=cf->frSize; x=0;         /* overlap */
   }
   /* Get new data - nsamples starting at r[x+1] */
   if (fGetWaveData(n,cf->rawBuffer,cf->r+x+1,pbuf->ext,pbuf->xdata)!=n)
     return FALSE;

   /* Copy to float buffer */
   for (j=1;j<=cf->frSize;j++) cf->s[j]=cf->r[j];

   /* Calc frame energy 0.0-100dB and store it in cf->curVol */
   for (j=1,m=e=0.0;j<=cf->frSize;j++) {
     x=(int)cf->s[j];
     m+=x; e+=x*x;
   }
   m=m/cf->frSize;e=e/cf->frSize-m*m;
   if (e>0.0) e=10.0*log10(e/0.32768);
   else e=0.0;
   cf->curVol = e;

   /* If using silence detector then store e at end of spVal array */
   if (pbuf->spVal!=NULL){
     pbuf->spVal[pbuf->inRow] = e;
   }

   /* Reset current nUsed/PK to indicate results of conversion */
   cf->nUsed = cf->nCvrt; cf->curPK = cf->tgtPK&BASEMASK;
   /* Then convert it to a frame */
   if (ConvertFrame(cf, fp) != cf->nCvrt)
     HError(6391,"GetFrameFromChannel: convert count != %d",cf->nCvrt);
   /* Update kinds */
   cf->nCvrt = cf->nUsed; cf->unqPK = cf->curPK;

   return TRUE;
}

/* ------------ Read and Convert Data from Channel Input ------------ */
#define CRCC_NONE 65535

/* ResetBuffer: reset the pbuf */
void ResetBuffer(ParmBuf pbuf)
{
   IOConfig cf = pbuf->cf;
   pbuf->outRow = 0;
   pbuf->inRow = 0;
   pbuf->spDetFrms = 0;
   pbuf->qst=0; pbuf->qen=-1;
   pbuf->status = PB_INIT;
   if (cf->cmnResetOnStop)
     ResetMeanRec(pbuf);
}


/* OpenAsChannel: open and create an audio input buffer */
static ReturnStatus OpenAsChannel(ParmBuf pbuf, char *fname)
{
   BufferInfo info;
   long dBytes;
   char b1[50];
   IOConfig cf = pbuf->cf;
   int i,minRows,spSize;
   const int maxObs = 200;

   /* Source must be a waveform */
   cf->srcFF = WAV;
   assert(pbuf->ext!=NULL);

   /* Channel parameters */
   pbuf->crcc=CRCC_NONE;  /* Only HParm files have CRCC */

   pbuf->outRow = 0;
   pbuf->inRow = 0;
   pbuf->spDetFrms=0;
   pbuf->qst=0; pbuf->qen=-1;
   pbuf->qwin=0;          /* set qwin wide enough to allow 1 computable frame */
   if (cf->tgtPK&HASDELTA) {
      if (cf->tgtPK&HASACCS) {
	pbuf->qwin += cf->accWin;
	if(cf->tgtPK&HASTHIRD) pbuf->qwin += cf->thirdWin;
      }
      pbuf->qwin += cf->delWin;
   }
   /* double qwin if an input tx is used to prevent data */
   /* needed for accs/tris being overwritten */
   if(cf->MatTranFN != NULL)  pbuf->qwin*=2;

   /* Determine sample rate */
   if (pbuf->ext->sampPeriod!=0.0 && cf->srcSampRate!=0.0 &&
       pbuf->ext->sampPeriod!=cf->srcSampRate!=0.0)
     HRError(-6371,"OpenAsChannel: External sample rate does not match config");
   if (cf->srcSampRate==0.0)
     cf->srcSampRate=pbuf->ext->sampPeriod;

   /* Setup remaining size information in IOConfig record */
   SetupMeanRec(&cf->mean,cf);
   ValidCodeParms(cf);
   cf->frSize = (int) (cf->winDur/cf->srcSampRate);
   cf->frRate = (int) (cf->tgtSampRate/cf->srcSampRate);
   SetUpForCoding(pbuf->mem,cf,cf->frSize);
   cf->rawBuffer=(char *) New(pbuf->mem,cf->frSize*(pbuf->ext->size&0xff));

   /* Call user defined Open routine */
   GetBufferInfo(pbuf,&info);
   pbuf->xdata = pbuf->ext->fOpen(pbuf->ext->xInfo,fname,&info);

   /* Initialise speech/sil detector */
   if (cf->calWindow!=0) {
      pbuf->spDetCnt=pbuf->silDetCnt=0;
   }

   /* Allocate main data buffer */
   pbuf->maxRows = (cf->calWindow > maxObs)
                       ?cf->calWindow:maxObs;
   minRows=1+2*pbuf->qwin;
   if (pbuf->maxRows<=minRows)
      pbuf->maxRows=minRows;
   dBytes = cf->nCols * pbuf->maxRows * sizeof(float);
   pbuf->data = New(pbuf->mem,dBytes);

   /* and auxiliary spVal buffer if needed, this extends back in time to allow
      silSeqCount to be greater than spcSeqCount */
   if (cf->calWindow!=0) {
      spSize = (cf->silSeqCount>cf->spcSeqCount)?cf->silSeqCount:cf->spcSeqCount;
      pbuf->spValLast = -spSize; spSize += pbuf->maxRows;
      pbuf->spVal = (float *) New(pbuf->mem,sizeof(float)*spSize);
      pbuf->spVal -= pbuf->spValLast;
      for (i=pbuf->spValLast; i<pbuf->maxRows; i++) pbuf->spVal[i]=0.0;

      pbuf->isSpeech = FALSE;

   }else
      pbuf->spVal = NULL;

   pbuf->status = PB_INIT;
   if (trace&T_BUF){
      printf("OpenAsChannel: curPK=%s; maxRows=%d; nUsed=%d; calWindow=%d;\n",
             ParmKind2Str(cf->curPK,b1),pbuf->maxRows,cf->nUsed,cf->calWindow);
      if (trace&T_BFX) ShowBuffer(pbuf,"channel opened");
      fflush(stdout);
   }
   return(SUCCESS);
}

/* EXPORT->OpenBuffer: open and return an input buffer */
ParmBuf OpenBuffer(MemHeap *x, char *name, HParmSrcDef ext)
{
   ParmBuf pbuf;

   if (x->type != MSTAK) {
      HRError(6316,"OpenBuffer: memory must be an MSTAK");
      return(NULL);
   }
   pbuf = (ParmBuf)New(x,sizeof(ParmBufRec));
   pbuf->mem = x;
   pbuf->chan = curChan; pbuf->ext=ext;
   pbuf->cf = MakeIOConfig(pbuf->mem, pbuf->chan);
   if (pbuf->cf->addDither>0.0) RandInit(12345);
   if(OpenAsChannel(pbuf,name)<SUCCESS){
      Dispose(x, pbuf);
      HRError(6316,"OpenBuffer: OpenAsChannel failed");
      return(NULL);
   }
   return pbuf;
}

/* EXPORT->OpenChanBuffer: open and return an input buffer as a channel*/
ParmBuf OpenChanBuffer(MemHeap *x, char *name, HParmSrcDef ext, ChannelInfoLink coderChan)
{
   ParmBuf pbuf;
   /*   ChannelInfo *theChan;

   theChan=(ChannelInfo *)coderChan;*/
   if (x->type != MSTAK) {
      HRError(6316,"OpenBuffer: memory must be an MSTAK");
      return(NULL);
   }
   pbuf = (ParmBuf)New(x,sizeof(ParmBufRec));
   pbuf->mem = x;
   pbuf->chan = coderChan; pbuf->ext=ext;
   pbuf->cf = MakeIOConfig(pbuf->mem, pbuf->chan);
   if (pbuf->cf->addDither>0.0) RandInit(12345);
   if(OpenAsChannel(pbuf,name)<SUCCESS){
      Dispose(x, pbuf);
      HRError(6316,"OpenBuffer: OpenAsChannel failed");
      return(NULL);
   }
   return pbuf;
}

/* EXPORT->CreateSrcExt: open and return input buffer using extended source */
HParmSrcDef CreateSrcExt(Ptr xInfo, ParmKind pk, int size, HTime sampPeriod,
                         Ptr (*fOpen)(Ptr xInfo,char *fn,BufferInfo *info),
                         void (*fClose)(Ptr xInfo,Ptr bInfo),
                         void (*fStart)(Ptr xInfo,Ptr bInfo),
                         void (*fStop)(Ptr xInfo,Ptr bInfo),
                         int (*fNumSamp)(Ptr xInfo,Ptr bInfo),
                         int (*fGetData)(Ptr xInfo,Ptr bInfo,int n,Ptr data))
{
   HParmSrcDef ext;

   ext=(HParmSrcDef) New(&gcheap,sizeof(HParmSrcDefRec));
   ext->xInfo=xInfo; ext->pk=pk; ext->size=size; ext->sampPeriod=sampPeriod;
   ext->fOpen=fOpen;ext->fClose=fClose;
   ext->fStart=fStart;ext->fStop=fStop;
   ext->fNumSamp=fNumSamp;ext->fGetData=fGetData;
   return(ext);
}

/* EXPORT->BufferStatus: Return current status of buffer */
PBStatus BufferStatus(ParmBuf pbuf)
{
   return pbuf->status;
}

/* ReadObs: convert the outRow of pbuf to an observation */
static Boolean ReadObs(ParmBuf pbuf, Observation *o)
{
   float *fp;
   char b1[50],b2[50];

   if (!EqualKind(o->bk,pbuf->cf->tgtPK))
      HError(6373,"ReadObs: Obs kind=%s but buffer kind=%s",
             ParmKind2Str(o->bk,b1),ParmKind2Str(pbuf->cf->curPK,b2));

   if (pbuf->outRow > pbuf->qen) return FALSE;
   fp = pbuf->data + pbuf->outRow*pbuf->cf->nCols;
   ExtractObservation(fp,o);
   if (pbuf->cf->calWindow != 0)
     o->vq[0] = DetectSpeech(pbuf);
   else
     o->vq[0] = 0;
   ++pbuf->outRow;
   return TRUE;
}

int FramesNeeded(ParmBuf pbuf)
{
   IOConfig cf = pbuf->cf;
   int minRequired;
   int newrows,avail,avail1,avail2;

   /* find # new rows which need to be added */
   /* avail1 = fully qualified rows available */
   /* avail2 = excess rows available beyond speech detector buffer */

   minRequired = (pbuf->status == PB_CALIBRATING)?cf->calWindow:1;
   avail = (pbuf->qen<0)?0:pbuf->qen - pbuf->outRow + 1;
   if (pbuf->status == PB_CALIBRATING || pbuf->status == PB_RUNNING){
     avail1 = avail;
     avail2 = (pbuf->qen<0)?0:pbuf->inRow - pbuf->outRow;
     avail2 -= cf->spcSeqCount;   /* lookahead needed for speech detect */
     avail = (avail1<avail2)?avail1:avail2;
   }
   newrows = minRequired - avail;
   if (trace&T_BUF) {
     printf("FramesNeeded: %d required, %d avail[a1=%d,a2=%d], %d new rows needed\n",minRequired,
	    avail,avail1,avail2,newrows);
      printf("Buffer: in=%d, out=%d, qst=%d, qen=%d\n",
              pbuf->inRow,pbuf->outRow,pbuf->qst,pbuf->qen);
   }
   return newrows;
}

/* FillBuffer: Fill pbuf, blocking if necessary */
static void FillBuffer(ParmBuf pbuf)
{
   int i,head,tail,newRows;
   int nQual,space,nShift;
   float *fp1,*fp2;
   char cbuf[100];
   IOConfig cf = pbuf->cf;

   if (trace&T_BUF) {
      printf("FillBuffer: in=%d, out=%d, qst=%d, qen=%d\n",
              pbuf->inRow,pbuf->outRow,pbuf->qst,pbuf->qen);
      if (trace&T_BFX) ShowBuffer(pbuf,"filling parm buf");
   }

   newRows = FramesNeeded(pbuf);
   if (newRows<=0) return;

   /* Check and make space if needed */
   space = pbuf->maxRows - pbuf->inRow;
   if (newRows>space){
      nShift = pbuf->outRow;  /* try to shift as much as possible */
      if (nShift> (pbuf->qst - pbuf->qwin))nShift = pbuf->qst - pbuf->qwin;
      if (nShift>0){
	fp1 = pbuf->data; fp2 = fp1 + nShift*cf->nCols;
	memmove(fp1,fp2,(pbuf->inRow-nShift)*cf->nCols*sizeof(float));
	if (pbuf->spVal!=NULL) {
	  fp1 = pbuf->spVal+pbuf->spValLast; fp2 = fp1 + nShift;
	  memmove(fp1,fp2,(pbuf->inRow-pbuf->spValLast-nShift)*sizeof(float));
	}
	/* Rebase everything so that still points to correct point */
	pbuf->inRow-=nShift; pbuf->outRow-=nShift;
	pbuf->qst-=nShift; space += nShift;
	if (trace&T_BFX) {
	  sprintf(cbuf,"after %d rows shifted",nShift);
	  ShowBuffer(pbuf,cbuf);
	}
      }
   }
   if (newRows > space)
     HError(9999,"FillBuffer:  cant meet request for %d new rows",newRows);

   /* Try to input newRows frames into pbuf */
   fp1 = pbuf->data + pbuf->inRow*cf->nCols;
   for (i=0; i<newRows; i++) {
     if (!GetFrameFromChannel(pbuf,fp1))break;
     fp1 += cf->nCols;
     pbuf->inRow++;
   }
   if (trace&T_BFX) {
     sprintf(cbuf,"after %d of desired %d new rows loaded",i,newRows);
     ShowBuffer(pbuf,cbuf);
   }

   /* Qualify as much as possible */
   pbuf->qen = pbuf->inRow - pbuf->qwin - 1;
   head = pbuf->qst;
   tail = pbuf->qwin;
   if(pbuf->qen>=pbuf->qst){
     fp1=pbuf->data + pbuf->qst*cf->nCols;
     nQual = pbuf->qen-pbuf->qst+1;
     AddQualifiers(pbuf,fp1,nQual,cf,head,tail);
     pbuf->qst=pbuf->qen+1;
     if (trace&T_BFX) {
       sprintf(cbuf,"after %d rows qualed[head=%d, tail=%d]",nQual,head,tail);
       ShowBuffer(pbuf,cbuf);
     }
   }
}

/* EXPORT->StartBuffer: start audio and fill the buffer */
void StartBuffer(ParmBuf pbuf)
{
   IOConfig cf = pbuf->cf;

   /* Call user defined Start routine */
   if (pbuf->ext->fStart!=NULL)
     pbuf->ext->fStart(pbuf->ext->xInfo,pbuf->xdata);
   /*printf("StartBuffer: Starting\n");*/
   if (cf->calWindow != 0)
     ChangeState(pbuf,PB_CALIBRATING);
   else
     ChangeState(pbuf,PB_RUNNING);
}

/* EXPORT->StopBuffer: stop audio and let the buffer empty */
void StopBuffer(ParmBuf pbuf)
{
  /* Call user defined Stop routine */
  if (pbuf->ext->fStop!=NULL)
    pbuf->ext->fStop(pbuf->ext->xInfo,pbuf->xdata);
   ChangeState(pbuf,PB_STOPPED);
}

/* EXPORT->CloseBuffer: close given ParmBuf object */
void CloseBuffer(ParmBuf pbuf)
{
   pbuf->ext->fClose(pbuf->ext->xInfo,pbuf->xdata);
   Dispose(pbuf->mem,pbuf);
}

/* EXPORT->ReadBuffer:  direct blocking version for ATK */
Boolean ReadBuffer(ParmBuf pbuf, Observation *o)
{
   IOConfig cf = pbuf->cf;
   static int calCount = 0;

   switch (pbuf->status){
   case PB_INIT:
      HError(999,"ReadBuffer: buffer not started");
      break;
   case PB_CALIBRATING:
      if (cf->calWindow == 0)
         HError(999,"ReadBuffer: calWindow not set");
      FillBuffer(pbuf);
      if (SetSpDetParms(pbuf,pbuf->chan->calInhibit))
         ChangeState(pbuf,PB_RUNNING);
      break;
   case PB_RUNNING:
      FillBuffer(pbuf);
      if (cf->calPeriod>0 && ++calCount>=cf->calPeriod) {
         calCount = 0; ChangeState(pbuf,PB_CALIBRATING);
      }
      break;
   case PB_STOPPED:
      if (pbuf->outRow > pbuf->qen){
         ChangeState(pbuf,PB_CLEARED);
         return FALSE;
      }
      break;
   case PB_CLEARED:
      return FALSE;
      break;
   }
   return ReadObs(pbuf,o);
}

/* ------------------ Other buffer operations ------------------- */

/* EXPORT->GetBufferInfo: Get info associated with pbuf */
void GetBufferInfo(ParmBuf pbuf, BufferInfo *info)
{
   ChannelInfo *chan;
   IOConfig cf;

   if (pbuf!=NULL) {
      chan=pbuf->chan,cf=pbuf->cf;
   }
   else chan=curChan, cf=&curChan->cf;

   info->srcPK       = cf->srcPK;
   info->srcFF       = cf->srcFF;
   info->srcSampRate = cf->srcSampRate;
   info->frSize      = cf->frSize;
   info->frRate      = cf->frRate;
   info->tgtPK       = cf->tgtPK;
   info->tgtFF       = cf->tgtFF;
   info->tgtSampRate = cf->tgtSampRate;
   info->tgtVecSize  = cf->tgtUsed;
   /* adjust the target vector size when _N used */
   if ((cf->tgtPK&HASNULLE) && (cf->tgtPK&(HASENERGY|HASZEROC)))
      info->tgtVecSize -= 1;
   info->spDetSil=chan->spDetSil;
   info->spDetSp=chan->spDetSp;
   info->spDetThresh=chan->spDetThresh;
   info->curVol=cf->curVol;
   info->matTranFN = cf->MatTranFN;
}

/* --------------------------  HParm.c ------------------------- */
