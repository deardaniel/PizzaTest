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
/*           File: HLM.h -  language model handling            */
/* ----------------------------------------------------------- */


/* !HVER!HLM: 1.6.0 [SJY 01/06/07] */

/* This version of HLM is specific to ATK.  It supports both */
/* word and class trigrams.  The word "voc" is used to stand */
/* for an element of the core ngram which will either be a word */
/* or a class */

#ifndef _HLM_H_
#define _HLM_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LMID 65534          /* Max number of words */
typedef unsigned short lmId;    /* Type used by lm to id words  1..MAX_LMID */
typedef unsigned short lmCnt;   /* Type used by lm to count wds 0..MAX_LMID */

#define NSIZE 3                 /* 3==trigram - do not change in ATK */
#define TGHASHSIZE 9973         /* size of cache hash table */

typedef struct sentry {         /* HLM NGram probability */
   lmId voc;                    /* word or class id */
   float prob;                  /* probability */
} SEntry;

typedef struct nentry {         /* HLM NGram history */
   lmId word[NSIZE-1];          /* Word history representing this entry */
   lmCnt nse;                   /* Number of ngrams for this entry */
   float bowt;                  /* Back-off weight */
   SEntry *se;                  /* Array[0..nse-1] of ngram probabilities */
   struct nentry *link;         /* Next entry in hash table */
   void *user;                  /* Accumulator or cache storage */
} NEntry;

typedef struct classentry {
   lmId classId;                /* id of class of this word */
   LogFloat prob;                  /* P(word|class) */
} ClassEntry;

typedef struct {                /* trigram cache entry */
   lmId voc;
   unsigned int hkey;
   LogFloat prob;
}TGCache;

typedef struct lmodel {
   int numWords;                /* word ids go from 1..numWords */
   int numClasses;              /* classes from numWords+1 ... */
   char *name;                  /* Name used for identifying lm */
   int nsize;                   /* Unigram==1, Bigram==2, Trigram==3 */
   unsigned int hashsize;       /* Size of hashtab (adjusted by lm counts) */
   NEntry **hashtab;            /* Hash table for finding NEntries */
   int counts[NSIZE+1];         /* Number of [n]grams */
   int vocSize;                 /* Core LM size */
   Vector unigrams;             /* Unigram probabilities */
   LabId *wdlist;               /* Lookup table for words/classes from lmId */
   LogFloat pen;                /* Word insertion penalty */
   float scale;                 /* Language model scale */
   ClassEntry *cmap;            /* array[1..numWords] of ClassEntry */
   MemHeap *heap;               /* Heap for allocating lm structs */
   TGCache triCache[TGHASHSIZE];    /* Hash table trigram cache */
} LModel;

typedef union {                 /* History type used in paths etc */
    unsigned int key;
    lmId voc[NSIZE-1];
} LMHistory;


void InitLM(void);
/*
   Initialise the module
*/

float GetLMProb(LModel *lm, LMHistory hist, LabId wdid);
/*
   Return log P(wdid|hist)
*/

NEntry *GetNEntry(LModel *lm, lmId ndx[NSIZE],Boolean create);
/*
   Return a complete NEntry for given history
*/

LModel *ReadLModel(MemHeap *heap,char *fn);
/*
   Create and read ngram language model from specified file.
*/

void DeleteLModel(LModel *lm);
/*
   Delete given language model
*/

LMHistory SetLMHistory(LModel *lm, LabId w, LMHistory prev);
/*
   Given current word w and history w1,w2 , return history w,w1.
   Except if w==NULL, return prev.
   If class model, w is mapped into its class.
*/


LabId GetLMName(LModel *lm, lmId w);
/*
   Return labid corresponding to LM id w
*/

typedef Ptr LMState;
LogFloat LMTrans (LModel *lm, LMState src, LabId wdid, LMState *dest);


#ifdef __cplusplus
}
#endif

#endif  /* _HLM_H_ */

/* ---------------------- End of HLM.h ----------------------- */
