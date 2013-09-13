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
/*           File: HLM.c -   language model handling           */
/* ----------------------------------------------------------- */

char *hlm_version = "!HVER!HLM: 1.6.0 [SJY 01/06/07]";

#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HWave.h"
#include "HLabel.h"
#include "HDict.h"
#include "HLM.h"

/* Modifications
   11/08/05 - support for class-based LMs added (SJY)
*/

/* --------------------------- Trace Flags ------------------------- */

#define T_TOP 001     /* top level tracing */
#define T_LDM 002     /* trace model loading */
#define T_PRB 004     /* trace prob lookup */

static int trace=0;

/* --------------------------- Initialisation ---------------------- */

#define LN10 2.30258509299404568 /* Defined to save recalculating it */

static Boolean rawMITFormat = FALSE;    /* Don't use HTK quoting and escapes */
static Boolean upperCaseLM = FALSE;     /* map all words to upper case */


static ConfParam *cParm[MAXGLOBS];      /* config parameters */
static int nParm = 0;

/* EXPORT->InitLM: initialise configuration parameters */
void InitLM(void)
{
   Boolean b;
   int i;

   Register(hlm_version);
   nParm = GetConfig("HLM", TRUE, cParm, MAXGLOBS);
   if (nParm>0){
      if (GetConfInt(cParm,nParm,"TRACE",&i)) trace = i;
      if (GetConfBool(cParm,nParm,"RAWMITFORMAT",&b)) rawMITFormat = b;
      if (GetConfBool(cParm,nParm,"UPPERCASELM",&b)) upperCaseLM = b;
   }
}

/*------------------------- Input Scanner ---------------------------*/

/* GetInLine: read a complete line from source */
static char *GetInLine(char *buf, Source *src)
{
   int  i, c;

   c = GetCh(src);
   if (c==EOF) return NULL;
   i = 0;
   while (c!='\n' && i<MAXSTRLEN) {
      buf[i++] = c;  c = GetCh(src);
   }
   buf[i] = '\0';
   return buf;
}

/* SyncStr: read input until str found in line*/
static void SyncStr(char *buf, char *str, Source *src)
{
   Boolean found = FALSE;
   if (strstr(buf,str)!=NULL) found = TRUE;
   while (!found) {
      if (GetInLine(buf,src)==NULL)
         HError(8150,"SyncStr: EOF searching for %s", str);
      if (strstr(buf,str)!=NULL) found = TRUE;
   }
}

/* GetInt: read int from input stream */
static int GetInt(Source *src)
{
   int x;
   char buf[100];

   if (!ReadInt(src,&x,1,FALSE))
      HError(8150,"GetInt: Int Expected at %s",SrcPosition(*src,buf));
   return x;
}

/* GetFLoat: read float from input stream */
static float GetFloat(Boolean bin, Source *src)
{
   float x;
   char buf[100];

   if (!ReadFloat(src,&x,1,bin))
      HError(8150,"GetFloat: Float Expected at %s",SrcPosition(*src,buf));
   return x;
}

/* ReadLMWord: read a string from input stream */
static char *ReadLMWord(char *buf, Source *src)
{
   int i, c;
   char *s;

   if (rawMITFormat) {
      while (isspace(c=GetCh(src)));
      i=0;
      while (!isspace(c) && c!=EOF && i<MAXSTRLEN){
         buf[i++] = c; c=GetCh(src);
      }
      buf[i] = '\0';
      UnGetCh(c,src);
      if (i>0) {
         if (upperCaseLM) {
            for (s=buf; *s!='\0'; s++) *s = toupper(*s);
         }
         return buf;
      } else
         return NULL;
   } else {
      if (ReadString(src,buf)){
         if (upperCaseLM){
            for (s=buf; *s!='\0'; s++) *s = toupper(*s);
         }
         return buf;
      } else
         return NULL;
   }
}

/*------------------------- NEntry handling ---------------------------*/

static int hvs[]= { 165902236, 220889002, 32510287, 117809592,
                    165902236, 220889002, 32510287, 117809592 };

/* GetNEntry: Access specific NGram entry indexed by ndx */
NEntry *GetNEntry(LModel *lm, lmId ndx[NSIZE],Boolean create)
{
   NEntry *ne;
   unsigned int hash;
   int i;
   /* #define LM_HASH_CHECK */
   hash=0;
   for (i=0;i<NSIZE-1;i++)
      hash=hash+(ndx[i]*hvs[i]);
   hash=(hash>>7)&(lm->hashsize-1);
   for (ne=lm->hashtab[hash]; ne!=NULL; ne=ne->link) {
      if (ne->word[0]==ndx[0] && ne->word[1]==ndx[1]) break;
   }
   if (ne==NULL && create) {
      ne=(NEntry *) New(lm->heap,sizeof(NEntry));
      lm->counts[0]++;

      for (i=0;i<NSIZE-1;i++)
         ne->word[i]=ndx[i];
      ne->user=0;
      ne->nse=0;
      ne->se=NULL;;
      ne->bowt=0.0;
      ne->link=lm->hashtab[hash];
      lm->hashtab[hash]=ne;
   }
   return(ne);
}

/*--------------------- ARPA-style NGrams ------------------------*/

static int se_cmp(const void *v1,const void *v2)
{
   SEntry *s1,*s2;

   s1=(SEntry*)v1;s2=(SEntry*)v2;
   return((int)(s1->voc - s2->voc));
}

static int nep_cmp(const void *v1,const void *v2)
{
   NEntry *n1,*n2;
   int res,i;

   res=0; n1=*((NEntry**)v1); n2=*((NEntry**)v2);
   for(i=NSIZE-2;i>=0;i--)
      if (n1->word[i]!=n2->word[i]) {
         res=(n1->word[i]-n2->word[i]);
         break;
      }
   return(res);
}

#define NGHSIZE1 8192
#define NGHSIZE2 32768
#define NGHSIZE3 131072

/* InitBoNGram: Initialise basic NGram structures
    Backoff NGram language models with size defined by counts.
    vocSize=number of words/classes in Ngram
    counts[1]=number of unigrams
    counts[2]=approximate number of bigrams
    counts[3]=approximate number of trigrams
               (approximate sizes are used to determine hash table size)
*/
static void InitBoNGram(LModel *lm, int vocSize, int counts[NSIZE])
{
   lmId ndx[NSIZE];
   int i,k;

   for (i=0;i<=NSIZE;i++) lm->counts[i]=0;
   for (i=1;i<=NSIZE;i++)
      if (counts[i]==0) break;
      else lm->counts[i]=counts[i];
   lm->nsize=i-1;

   /* Don't count final layer */
   for (k=0,i=1;i<lm->nsize;i++)
      k+=lm->counts[i];
   /* Then use total to guess NEntry hash size */
   if (k<25000)
      lm->hashsize=NGHSIZE1;
   else if (k<250000)
      lm->hashsize=NGHSIZE2;
   else
      lm->hashsize=NGHSIZE3;

   lm->hashtab=(NEntry **) New(lm->heap,sizeof(NEntry*)*lm->hashsize);
   for (i=0; i<lm->hashsize; i++)
      lm->hashtab[i]=NULL;

   lm->vocSize = vocSize;
   lm->unigrams = CreateVector(lm->heap,lm->vocSize);

   for (i=0;i<NSIZE;i++) ndx[i]=0;
   GetNEntry(lm,ndx,TRUE);
}

#define BIN_ARPA_HAS_BOWT 1
#define BIN_ARPA_INT_LMID 2

/* ReadNGrams: read n grams list from source */
static int ReadNGrams(LModel *lm,int n,int count, Boolean bin, Source *src)
{
   float prob;
   LabId wdid;
   SEntry *cse;
   char wd[255];
   lmId ndx[NSIZE+1];
   NEntry *ne,*le=NULL;
   int i, g, idx, total;
   unsigned char size, flags;

   cse = (SEntry *) New(lm->heap,count*sizeof(SEntry));
   for (i=1;i<=NSIZE;i++) ndx[i]=0;

   if (trace&T_LDM)
      printf(" reading %1d-grams\n",n);

   total=0;
   for (g=1; g<=count; g++){
      if (trace&T_LDM) {
         if ((g%25000)==0)
            printf(". "),fflush(stdout);
         if ((g%800000)==0)
            printf("\n   "),fflush(stdout);
      }
      if (bin) {
         size = GetCh (src);
         flags = GetCh (src);
      }

      prob = GetFloat(bin,src)*LN10;

      if (n==1) { /* unigram treated as special */
         ReadLMWord(wd,src);
         wdid = GetLabId(wd, TRUE);
         if (wdid->lmid != 0)
            HError(8150,"ReadNGrams: Duplicate word/class (%s) in 1-gram list", wdid->name);
         wdid->lmid = g;
         lm->wdlist[g] = wdid;
         lm->unigrams[g] = prob;
         ndx[0]=g;
      } else {    /* bigram, trigram, etc. */
         for (i=0;i<n;i++) {
            if (bin) {
               if (flags & BIN_ARPA_INT_LMID) {
                  /* unsigned */ int ui;
                  if (!ReadInt (src, &ui, 1, bin))
                     HError (9999, "ReadNGrams: failed reading int lm word id");
                  idx = ui;
               }
               else {
                  /* unsigned */ short us;
                  if (!ReadShort (src, &us, 1, bin))
                     HError (9999, "ReadNGrams: failed reading short lm word id at");
                  idx = us;
               }
            }
            else {
               ReadLMWord(wd,src);
               wdid = GetLabId(wd, FALSE);
               idx = (wdid==NULL?0:wdid->lmid);
            }
            if (idx<1 || idx>lm->vocSize){
               HError(8150,"ReadNGrams: Unseen word/class (%s) in %dGram",wd,n);
            }
            ndx[n-1-i]=idx;
         }
      }
      total++;
      ne = GetNEntry(lm,ndx+1,FALSE);
      if (ne == NULL)
         HError(8150,"ReadNGrams: Backoff weight not seen for %dth %dGram",g,n);
      if (ne!=le) {
         if (le != NULL && ne->se != NULL)
            HError(8150,"ReadNGrams: %dth %dGrams out of order",g,n);
         if (le != NULL) {
            if (le->nse==0) {
               le->se=NULL;
            } else {
               qsort(le->se,le->nse,sizeof(SEntry),se_cmp);
            }
         }
         ne->se = cse;
         ne->nse = 0;
         le = ne;
      }
      cse->prob = prob;
      cse->voc = ndx[0];
      ne->nse++; cse++;

      /* read back-off weight */
      if (bin) {
         if (flags & BIN_ARPA_HAS_BOWT) {
            ne = GetNEntry(lm,ndx,TRUE);
            ne->bowt = GetFloat (TRUE,src)*LN10;
         }
      }
      else {
         SkipWhiteSpace(src);
         if (!src->wasNewline) {
            ne=GetNEntry(lm,ndx,TRUE);
            ne->bowt = GetFloat(FALSE,src)*LN10;
         }
      }
   }

   /* deal with the last accumulated set */
   if (le != NULL) {
      if (le->nse==0) {
         le->se=NULL;
      } else {
         qsort(le->se,le->nse,sizeof(SEntry),se_cmp);
      }
   }
   return(total);
}

/* GetNGramProb: return probability of voc given hist */
static float GetNGramProb(LModel *lm, LMHistory hist, lmId voc)
{
   NEntry *ne;
   SEntry *se;
   LogFloat bowt;
   int i,j,s,l,u;
   TGCache *t;

   if (voc==0 || voc>lm->vocSize)
      HError(999,"voc %d is not in NGram LM",voc);

   /* if ngram hashed then return it */
   t = lm->triCache + (hist.key*voc) % TGHASHSIZE;
   if (t->voc == voc && t->hkey == hist.key) {
      return t->prob;
   }
   t->voc = voc; t->hkey = hist.key;

   /* not hashed so look it up */
   s = -1;
   if (hist.voc[0] != 0) {
      s=0;
      if (hist.voc[1] != 0)  s=1;
   }

   /* If no history return unigram */
   if (s<0) {
      if (voc!=0)
         t->prob = lm->unigrams[voc];
      else
         t->prob = log(1.0/lm->vocSize);
      return t->prob;
   }

   ne = GetNEntry(lm,hist.voc,FALSE);

   /* binary search for required ngram */
   if (ne != NULL) {
      l = 0; u = ne->nse-1; se = NULL;
      while(l<=u) {
         i = (l+u)/2;
         if (ne->se[i].voc==voc) {
            se = ne->se+i; break;
         }
         if (ne->se[i].voc>voc) u = i-1; else l = i+1;
      }
      if (se != NULL) {
         t->prob = se->prob; return t->prob;
      }
      bowt=ne->bowt;
   } else {
      bowt=0.0;
   }

   /* ngram not found, so recurse */
   if (s==0)
      t->prob = lm->unigrams[voc]+bowt; /* Backoff to unigram */
   else {
      hist.voc[1] = 0;
      t->prob = bowt+GetNGramProb(lm,hist,voc);       /* else recurse */
   }
   return t->prob;
}

/*------------------------- User Interface --------------------*/

/* EXPORT->GetLMProb: return probability of word wd_id given hist */
float GetLMProb(LModel *lm, LMHistory hist, LabId wdid)
{
   LogFloat classprob;
   lmId voc;
   if (lm->numClasses==0){
      voc = wdid->lmid;
      classprob = 0.0;
   }else{
      voc = lm->cmap[wdid->lmid].classId;
      classprob = lm->cmap[wdid->lmid].prob;
   }
   return GetNGramProb(lm,hist,voc)+classprob;
}

/* EXPORT->ReadLModel: Determine LM type and then read-in */
LModel *ReadLModel(MemHeap *heap, char *fn)
{
   Source source;           /* input file */
   LModel *lm;
   int i,j,k,counts[NSIZE+1];
   Boolean ngBin[NSIZE+1];
   char buf[MAXSTRLEN+1], line[MAXSTRLEN+1],syc[64];
   char ngFmtCh, *s,*wp,*cp;
   LabId wdid,clid;
   LogFloat f;

   lm=(LModel*)New(heap,sizeof(LModel));
   lm->heap=heap;
   lm->name=CopyString(heap,fn); lm->cmap = NULL;

   if(InitSource(fn,&source,LangModFilter)<SUCCESS)
      HError(8110,"ReadLModel: Can't open language model source file %s", fn);

   lm->numWords = lm->numClasses = 0;
   GetInLine(buf,&source);
   if (strstr(buf,"CLASS MODEL")!=NULL){
      /* its a class model so get size info */
      SyncStr(buf,"Number of classes",&source);
      lm->numClasses = atoi(buf+19);
      SyncStr(buf,"Number of words",&source);
      lm->numWords = atoi(buf+16);
   }
   /* load header from ARPA N-gram section */
   SyncStr(buf,"\\data\\",&source);
   for (i=1;i<=NSIZE;i++) counts[i]=0;
   for (i=1;i<=NSIZE;i++) {
      GetInLine(buf,&source);
      if (sscanf(buf, "ngram %d%c%d", &j, &ngFmtCh, &k)!=3 && i>1)
         break;
      if (i!=j || k==0)
         HError(8150,"ReadLModel: %dGram count missing (%s)",i,buf);
      switch (ngFmtCh) {
      case '=': ngBin[j] = FALSE; break;
      case '~': ngBin[j] = TRUE; break;
      default:
         HError (9999, "ReadLModel: unknown ngram format type '%c'", ngFmtCh);
      }
      counts[j]=k;
   }
   if (ngBin[1])
      HError (8113, "ReadLModel: unigram must be stored as text");
   if (lm->numClasses > 0 && lm->numClasses != counts[1])
      HError (8113, "ReadLModel: class count differs between class and ngram headers");
   if (lm->numClasses==0) lm->numWords = counts[1];

   if (trace&T_TOP){
      printf("Loading lm from %s: %d words, %d classes\n",
               fn,lm->numWords,lm->numClasses);
   }
   /* create wdlist indexed 1..numWords+numClasses */
   k = lm->numClasses+lm->numWords;
   lm->wdlist = (LabId *) New(lm->heap,k*sizeof(LabId));
   lm->wdlist--;
   for (i=1;i<=k;i++) lm->wdlist[i]=NULL;
   /* read the N-gram data */
   InitBoNGram(lm,counts[1],counts);
   for (i=1;i<=lm->nsize;i++) {
      sprintf(syc,"\\%d-grams:",i);
      SyncStr(buf,syc,&source);
      ReadNGrams(lm,i,lm->counts[i], ngBin[i],&source);
   }
   SyncStr(buf,"\\end\\",&source);
   if (trace&T_LDM) {
      printf(" NEntry==%d ",lm->counts[0]);
      for(i=1;i<=lm->nsize;i++)
         printf(" %d-Grams==%d",i,lm->counts[i]);
      printf("\n");
   }
   /* if class-gram create and then read the class map */
   if (lm->numClasses>0){
      lm->cmap = (ClassEntry *) New(lm->heap,lm->numWords*sizeof(ClassEntry));
      lm->cmap -= (lm->numClasses+1);
      for (i=1; i<=lm->numWords; i++){
         if (GetInLine(buf,&source)==NULL)
            HError(8114,"ReadLModel: EOF while reading classmap");
         strcpy(line,buf);
         s=wp=buf; j=strlen(buf);
         while (j>0 && !isspace(*s)) {++s; --j;}
         if (j<=0 || s==wp)
            HError(8114,"ReadLModel: word end not found in line %s", line);
         *s = '\0'; s++; --j;
         while (j>0 && isspace(*s)) {++s; --j;}
         if (j<=0)
            HError(8114,"ReadLModel: class start not found in line %s", line);
         cp=s;
         while (j>0 && !isspace(*s)) {++s; --j;}
         if (j<=0 || s==cp)
            HError(8114,"ReadLModel: class end not found in line %s", line);
         *s = '\0'; s++; --j;
         while (j>0 && isspace(*s)) {++s; --j;}
         if (j<=0)
            HError(8114,"ReadLModel: prob start not found in line %s", line);

         wdid = GetLabId(wp,TRUE);
         if (wdid->lmid != 0)
            HError(8114,"ReadLModel: word %s multiply defined in class map", wp);
         wdid->lmid = lm->numClasses+i;
         clid = GetLabId(cp,FALSE);
         if (clid==NULL || clid->lmid ==0)
            HError(8114,"ReadLModel: unknown class %s", cp);
         lm->cmap[wdid->lmid].classId = clid->lmid;
         f = (LogFloat)atof(s);
         if (f>0.0)
            HError(8114,"ReadLModel: illegal probability %s", s);
         lm->cmap[wdid->lmid].prob = f;
      }
      if (trace&T_LDM)
         printf(" %d word-class-prob defs read\n",lm->numWords);
   }
   CloseSource(&source);

   /* finally initialise the hashtable */
   for (i=0; i<TGHASHSIZE; i++) {
      lm->triCache[i].voc = 0;
   }
   if (trace&T_TOP)
      printf("LM %s loaded\n",lm->name);
   return(lm);
}

/* EXPORT->DeleteLModel: Delete given language model */
void DeleteLModel(LModel *lm)
{
   DeleteHeap(lm->heap);
}

/* EXPORT->SetLMHistory: return updated history w preceded by prev.word[0] */
LMHistory SetLMHistory(LModel *lm, LabId w, LMHistory prev)
{
   LMHistory h;
   if (w==NULL) return prev;
   if (lm==NULL || lm->numClasses==0) {
      h.voc[0] = w->lmid;
   }else{
      h.voc[0] = lm->cmap[w->lmid].classId;
   }
   h.voc[1] = prev.voc[0];
   return h;
}

/* EXPORT->Return labid corresponding to LM id w */
LabId GetLMName(LModel *lm, lmId w)
{
   if (w!=0) return lm->wdlist[w];
   return NULL;
}

/* ----------------Option fns???--------------------------- */

/* FindSEntry: find SEntry for wordId in array using binary search */
static SEntry *FindSEntry (SEntry *se, lmId pronId, int l, int h)
{
   /*#### here l,h,c must be signed */
   int c;

   while (l <= h) {
      c = (l + h) / 2;
      if (se[c].voc == pronId)
         return &se[c];
      else if (se[c].voc < pronId)
         l = c + 1;
      else
         h = c - 1;
   }
   return NULL;
}

/* LMTrans: return logprob of transition from src labelled word. Also
            return dest state.  */
LogFloat LMTrans (LModel *lm, LMState src, LabId wdid, LMState *dest)
{
   LModel *nglm;
   LogFloat lmprob;
   lmId hist[NSIZE] = {0};      /* initialise whole array to zero! */
   int i, l;
   NEntry *ne;
   SEntry *se;
   lmId word;

   /* assert (lm->type == boNGram);  Check LModel type???*/
   nglm = lm;
   word = (int) wdid->aux;
   if (word==0 || word>lm->vocSize) {
      HError (-9999, "word %d not in LM wordlist", word);
      *dest = NULL;
      return (LZERO);
   }
   ne = src;
   if (!src) {          /* unigram case */
      lmprob = nglm->unigrams[word];
   }
   else {
      /* lookup prob p(word | src) */
      /* try to find pronid in SEntry array */
      se = FindSEntry (ne->se, word, 0, ne->nse - 1);
      assert (!se || (se->voc == word));
      if (se)        /* found */
         lmprob = se->prob;
      else {             /* not found */
         lmprob = 0.0;  l = 0;
         hist[NSIZE-1] = 0;
         for (i = 0; i < NSIZE-1; ++i) {
            hist[i] = ne->word[i];
            if (hist[i] != 0) l = i;
         } /* l is now the index of the last (oldest) non zero element */
         for ( ; l > 0; --l) {
            if (ne)
               lmprob += ne->bowt;
            hist[l] = 0;   /* back-off: discard oldest word */
            ne = GetNEntry (nglm, hist, FALSE);
            if (ne) {   /* skip over non existing hists. fix for weird LMs */
               /* try to find pronid in SEntry array */
               se = FindSEntry (ne->se, word, 0, ne->nse - 1);
               assert (!se || (se->voc == word));
               if (se) { /* found it */
                  lmprob += se->prob; l = -1;
                  break;
               }
            }
         }
         if (l == 0) {          /* backed-off all the way to unigram */
            assert (!se);
            lmprob += ne->bowt;  lmprob += nglm->unigrams[word];
         }
      }
   }
   /* now determine dest state */
   if (src) {
      ne = (NEntry *) src;

      l = 0;
      hist[NSIZE-1] = 0;
      for (i = 1; i < NSIZE-1; ++i) {
         hist[i] = ne->word[i-1];
         if (hist[i] != 0) l = i;
      } /* l is now the index of the last (oldest) non zero element */
   }else {
      for (i = 1; i < NSIZE-1; ++i) hist[i] = 0;
      l = 1;
   }
   hist[0] = word;
   ne = (LMState) GetNEntry (nglm, hist, FALSE);
   for ( ; !ne && (l > 0); --l) {
      hist[l] = 0;              /* back off */
      ne = (LMState) GetNEntry (nglm, hist, FALSE);
   }
   /* if we left the loop because l=0, then ne is still NULL, which is what we want */
   *dest = ne;
   /* printf ("lmprob = %f  dest %p\n", lmprob, *dest);*/
   return (lmprob);
}

/* ------------------------- End of HLM.c ------------------------- */
