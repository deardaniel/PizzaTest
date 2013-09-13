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
/*     File: HRec.h -   Viterbi Recognition Engine Library     */
/* ----------------------------------------------------------- */

/* !HVER!HREC: 1.6.0 [SJY 01/06/07] */

#ifndef _HREC_H_
#define _HREC_H_

#ifdef __cplusplus
extern "C" {
#endif

/* max number of tokens that can be used in HRec */
#define MAX_TOKS 50
/* size hash table used in tok set merging */
#define TSM_HASH 513

/* Each HMMSet that is used for recognition needs to be    */
/* initialised prior to use.  Initialisation routine adds  */
/* necessary structues to models and returns PSetInfo used */
/* by Viterbi recogniser to access model set.              */
typedef struct psetinfo PSetInfo; /* Private HMMSet information (HRec.c) */

/* Each recognition requires a RecInfo to maintain status   */
/* information thoughtout the utterance.  */
typedef struct precinfo PRecInfo; /*  private recognition information */

/* Traceback information takes the form of Path structures */
typedef struct ring Ring;          /* Ring of Paths */
typedef struct path Path;          /* Traceback route */

/* Tokens define the head of each partial hypothesis */
typedef struct token
{
   LogDouble like;	/* Likelihood of token */
   LogFloat lm;      /* LM likelihood of token */
   Path *path;		   /* Route (word level) through network */
   Boolean ngPending;/* true when ngram prob update is pending */
}Token;
extern const Token null_token; /* Null token is part of HRec.c */

/* NBest token handling is kept private */
typedef struct reltoken
{
   LogFloat like;       /* Relative Likelihood of token */
   LogFloat lm;         /* LM likelihood of token */
   Path *path;          /* Route (word level) through network */
} RelToken;

/* A tokenset is effectively a state instance */
typedef struct tokenset
{
   short n;                  /* Number of rtok valid (0==1-best, 1>==N-best) */
   RelToken *set;            /* Likelihood sorted array[0..nToks] of rtoks */
   Token tok;                /* Most likely Token in state */
} TokenSet;

struct ring  /* A ring is a common word end point of path alternatives */
{
   int nRefs;           /* number of references to this ring */
   int nPaths;          /* number of paths in this ring */
   Path *head;          /* actual ring of paths nb it is circular! */
   Path *tail;          /* but head of ring is most likely */
   Ring *next;          /* next ring in list */
   Ring *prev;          /* prev ring in list */
   NetNode *node;       /* Word level traceback info */
   int frame;           /* Time (frame) of boundary (end of word) */
   enum {cNorm,cMarked,cDeleted} status;
   int mark;
};

struct path  /* A path defines a word history emanating from a token */
{
   Ring *owner;         /* Ring owning this path */
   Path *prev;		      /* Previous word record */
   LogDouble like;      /* Likelihood at boundary */
   LogFloat lm;         /* LM likelihood of current word */
   LMHistory hist;      /* Language model history */
   Path *next;          /* Next path in ring */
   int trbkCount;       /* -1 if already output, >=0 usage count */
};

typedef struct {        /* ATK PARTIAL PATH for answer processing */
  int startFrame;       /* start frame of path, 0 if beginning */
  float startLike;      /* initial log likelihood, 0 if beginning */
  NetNode *node;        /* current head of the path, may be null */
  Path *path;           /* rest of the path */
  int  n;               /* num valid records in path */
  float lmScale;        /* lm scale factor */
  float ngScale;        /* ng scale factor */
  float wordPen;        /* word insertion penalty */
  float pronScale;      /* pronunciation scale factor */
} PartialPath;

typedef struct {
   HLink hmm;			 /* Background model if any */
   LogFloat *maxlike; /* array[0..size-1] of max state likelihood */
   LogFloat *bgdlike; /* array[0..size-1] of background model likelihood */
   LogFloat bestp;    /* best state prob */
   LogFloat averp;    /* average state prob */
   int idx;				 /* index of next slot to store into */
   int size;			 /* size of max/bgd likelihood memory array */
   int frame;         /* frame number of next expected - sanity check */
} BGConfRec;

/* The instances actually store tokens and links etc */
/* Instances are stored in creation/token propagation order to allow */
/* null/word/tee instances to be connected together and still do propagation */
/* in one pass.  Only HMMs need extra tokens, others are 1 state */
struct _NetInst
{
   struct _NetInst *link; /* Doubly linked list of instances, forward */
   struct _NetInst *knil; /* Doubly linked list of instances, backward */

   NetNode *node;       /* Position of instance within network */

   /* int flags;        /* Flags, active ... */
   TokenSet *state;     /* TokenSet[0..N-2] in state [1..N-1] for hmm */
   TokenSet *exit;      /* TokenSet in exit state */
   LogFloat max;        /* Likelihood for pruning of instance */

   Boolean pxd;         /* External propagation done this frame */
   Boolean ooo;         /* Instance potentially out of order */

   int ipos;   /* only used by sanity checking */
};

struct precinfo        /* Private recognition information */
{
   MemHeap heap;            /* General storage CHEAP  */
   HTime frameDur;          /* Sample rate (to convert frame to time) */
   Boolean noTokenSurvived; /* Set when no valid final token produced */
   AdaptXForm *inXForm;     /* Adaptation transform */

   /* Pruning thresholds - setable every frame */
   int maxBeam;             /* Maximum model instance beam */
   LogFloat genBeam;        /* Global beam width */
   LogFloat wordBeam;       /* Separte word end beam width */
   int nToks;               /* Maximum tokens to propagate (0==1) */
   LogFloat nBeam;          /* Beam width for non-best tokens */
   LogFloat tmBeam;         /* Beam width for tied mixtures */
   int pCollThresh;         /* Max path records created before collection */

   /* Configuration variables */
   float lmScale;           /* LM (Net probs) scale factor */
   float ngScale;           /* NGram scale factor */
   LogFloat wordPen;        /* Word insertion penalty */
   float pronScale;         /* Pronunciation probs scale factor */

   /* Status information - readable every frame */
   int frame;               /* Current frame number */
   int nact;                /* Number of active models */
   int tact;                /* Cummulative number of active instances */
   NetNode *genMaxNode;     /* Most likely node in network */
   NetNode *wordMaxNode;    /* Most likely word end node in network */
   Token genMaxTok;         /* Most likely token */
   Token wordMaxTok;        /* Most likely word end token */

   /* Private storage for recognition */
   Observation *obs;         /* Current Observation */
   PSetInfo *psi;           /* HMMSet information */
   Network *net;            /* Recognition network */
   int obid;                /* Unique observation identifier */
   int prid;                /* Unique pri identifier */

   LogFloat genThresh;      /* Cutoff from global beam */
   LogFloat wordThresh;     /* Cutoff for word end propagation */
   LogFloat nThresh;        /* Cutoff for non-best tokens */

   LogFloat *qsa;           /* Array for performing qsort */
   int qsn;                 /* Sizeof qsa */

   MemHeap instHeap;        /* Inst heap */
   MemHeap *stHeap;         /* Array[0..stHeapNum-1] of heaps for states */
   MemHeap rTokHeap;        /* RelToken heap */
   MemHeap pathHeap;        /* Path heap */
   MemHeap ringHeap;        /* Path Ring heap */

   int nusedRings;          /* Current number of path rings allocated */
   int nusedPaths;          /* Total number of path records allocated */
   int nusedLastCollect;    /* num paths used after last collect */
   int nfreeRings;          /* Num of free rings in freeRing */
   int nfreePaths;          /* Num of free paths in freePath */
   Path *freePath;          /* Freelist of path records */
   Ring *freeRing;          /* Freelist of ring records */
   Ring *actvHead;          /* Head of active ring list */
   Ring *actvTail;          /* Tail of active ring list */

   NetInst head;            /* Head (oldest) of Inst linked list */
   NetInst tail;            /* Tail (newest) of Inst linked list */
   NetInst *nxtInst;        /* Inst used to select next in step sequence */
   LModel *lm;              /* ngram language model if any */
   BGConfRec confinfo;      /* background likes for confidence calc */

   unsigned int keyhash[TSM_HASH];  /* hash for key checking in tokset merge */
   int keyused[MAX_TOKS];			/* record which keys need clearing */

   /* private use by sanity checking */
   NetInst *start_inst;     /* Inst that started a move */
   int ipos;                /* Current inst position */
};

/* define confidence score limits */
#define MIN_CONF 0.01
#define MAX_CONF 0.99

/* types of info put into transcription */
enum{LAB_LM=1, LAB_AC, LAB_TAG, LAB_MISC};

void InitRec(void);
/*
   Initialise module
*/

/*
   Functions specific to HMMSet

   Providing calls to ProcessObservation are not made simultaneously
   and that each recogniser's VRecInfo is separately initialised
   multiple recognisers can share HMMSets.  Use of unique observation
   ids allows correct caching of observation output probabilities.
*/

PSetInfo *InitPSetInfo(HMMSet *hset);
/*
   Attach precomps to HMMSet and return PSetInfo
   describing HMMSet
*/

void FreePSetInfo(PSetInfo *psi);
/*
   Free storage allocated by InitPSetInfo
*/


/*
   Functions specific to each recogniser started
   Each recogniser that needs to be run in parallel must be separately
   initialised
*/

PRecInfo *InitPRecInfo(PSetInfo *psi,int nToks);
/*
   Initialise a recognition engine attached to a particular HMMSet
   that will use nToks tokens per state
*/

void DeletePRecInfo(PRecInfo *pri);
/*
   Detach recogniser and free storage
*/

/*
   Each utterance is processed by calling StartRecognition, then
   ProcessObservation for each frame and finally traceback is
   performed by calling CompleteRecognition.
*/

/* EXPORT->StartRecognition: initialise network ready for recognition */

void StartRecognition(PRecInfo *pri,Network *net, float lmScale,
      LogFloat wordPen, float pScale, float ngScale, LModel *lm);
/*
   Commence recognition using previously initialised recogniser using
   supplied network, language model scales and word insertion penalty.
   If lm not null, then add ngScale*ngProg to all scaled link probs in network.
*/

void ProcessObservation(PRecInfo *pri,Observation *obs,int id, AdaptXForm *xform);
/*
   Process a single observation updating traceback and status
   information in vri.  Each call to ProcessObservation should
   provide an id value unique to a particular observation.
*/

void CompleteRecognition(PRecInfo *pri);
/*
   Recover all allocated data structures (except for paths)
*/

float GetConfidence(PRecInfo *pri, int st, int en, LogFloat a, char * word);
/*
   Get the confidence of the acoustic hyp between frames st and en
	(inclusive).  Return -1.0 if confidence cannot be computed.
	word is only needed for debug purposes, pass "" otherwise.
*/

void SetPruningLevels(PRecInfo *pri,int maxBeam,LogFloat genBeam,
		      LogFloat wordBeam,LogFloat nBeam,LogFloat tmBeam);
/*
   At any time after initialisation pruning levels can be set
   using SetPruningLevels or by directly altering the vri values
*/

Lattice *CreateLatticeFromOutput(PRecInfo *pri, PartialPath pp, MemHeap *heap, HTime frameDur);
/*
   Make a lattice from traceback information
*/

PartialPath DoTraceBack(PRecInfo *pri);
/*
   Traceback paths from all active Insts and return disambiguated tail if any.
   Once a partial path has been returned, it will never be returned again.
*/

PartialPath CurrentBestPath(PRecInfo *pri);
/*
  direct traceback of current best path from genMaxNode/genMaxTok.path
*/

PartialPath FinalBestPath(PRecInfo *pri);
/*
  traceback of path from pri->net->final.inst->exit
*/

void PrintPartialPath(PartialPath pp, Boolean inDetail);
/*
  print the given path, either just the words or in detail
*/

Transcription *TransFromLattice(MemHeap *heap, Lattice *lat, int max);
/*
   Generate NBest labels from lattice
*/

void FormatTranscription(Transcription *trans,HTime frameDur,
                         Boolean states,Boolean models,Boolean triStrip,
                         Boolean normScores,Boolean killScores,
                         Boolean centreTimes,Boolean killTimes,
                         Boolean killWords,Boolean killModels);
/*
   Format a label transcription prior to calling LSave
*/

void TracePath(FILE *file,Path *path);
/*
   Output to file the sequence of words in path
*/

#ifdef __cplusplus
}
#endif

#endif  /* _HREC_H_ */
