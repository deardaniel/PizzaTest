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
/*     File: HRec.c -   Viterbi Recognition Engine Library     */
/* ----------------------------------------------------------- */

char *hrec_version = "!HVER!HRec: 1.6.0 [SJY 01/06/07]";


/* Modification history */

/* 02/11/03      - extended to support NGram LMs. Major areas of change:
    TokSetMerge  - word pair approx replaced by trigram approx
    StepInst2    - applies trigram probs as soon as next word is known
    Alignment    - all code relating to model/state alignment has been removed
    Path handling - rewritten to allow more general N-best recovery
    PRI/VRI distinction removed - SJY
   06/5/04 - Token merge routine rewritten - SJY
   17/08/04 - bugs in nTok path recording fixed - SJY
   20/08/04 - conf scoring updated and some threading issues resolved - SJY
   17/05/05 - added lattice generation and nbest routines - MNS
   11/08/05 - added support for class-based LMs - SJY
*/

#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HWave.h"
#include "HAudio.h"
#include "HParm.h"
#include "HLabel.h"
#include "HModel.h"
#include "HDict.h"
#include "HLM.h"
#include "HNet.h"
#include "HRec.h"
#include "HUtil.h"
#include "HAdapt.h"
#include "HLat.h"
#include "HNBest.h"

/* Trace levels */

#define T_NGEN 1  /* Nbest generation */

#define T_TOP  00001     /* top level flow */
#define T_FRT  00002     /* state at each frame */
#define T_INST 00004     /* trace insts each frame */
#define T_TOKS 00010     /* print tokens in detail */
#define T_CON1 00020     /* top level confidence tracing */
#define T_CON2 00040     /* detailed confidence tracing */
#define T_DPP  00100     /* print paths in detail */
#define T_GBG  00200     /* trace garbage collection */

#define DETAILED_TRACING
#define T_WLM  00400     /* lm handling */
#define T_TSM  01000     /* trace token set merge */

#define NUMTRACEINSTS   8    /* num of instances to trace */

/* Checks */

/* #define SANITY */

static int trace=0;
static int traceDelay = 0;
static Boolean forceOutput=FALSE;

const Token null_token={LZERO,0.0,NULL,FALSE};

/* Define macros for assessing node type */
#define node_hmm(node) ((node)->type & n_hmm)
#define node_word(node) ((node)->type == n_word)
#define node_tr0(node) ((node)->type & n_tr0)
#define node_wd0(node) ((node)->type & n_wd0)


/* Need some null RelTokens */
static const RelToken rmax={0.0,0.0,NULL};    /* First rtok same as tok */
static const RelToken rnull={LZERO,0.0,NULL}; /* Rest can be LZERO */

/* Used for sorting reltokens eg in StepInst2 */
typedef struct {
   RelToken rtok;
   short left;
   short right;
}RTokBinTreeNode;

/* HMMSet information is some precomputed limits plus the precomps */
typedef struct precomp
{
   int id;                  /* Unique identifier for current frame */
   LogFloat outp;           /* State/mixture output likelihood */
}
PreComp;

struct psetinfo
{
   MemHeap heap;            /* Memory for this set of pre-comps */
   HMMSet *hset;            /* HMM Set for recognition */

   int max;                 /* Max states in HMM set */
   Boolean mixShared;
   int nsp;
   PreComp *sPre;           /* Array[1..nsp] State PreComps */
   int nmp;
   PreComp *mPre;           /* Array[1..nmp] Shared mixture PreComps */
   int ntr;
   short ***seIndexes;      /* Array[1..ntr] of seIndexes */
   Token *tBuf;             /* Buffer Array[2..N-1] of tok for StepHMM1 */
   TokenSet *sBuf;          /* Buffer Array[2..N-1] of tokset for StepHMM1_N */

   short stHeapNum;         /* Number of separate state heaps */
   short *stHeapIdx;        /* Array[1..max] of state to heap index */
};

/* Global variables  */
static char  defConfBGHMM[256];
static float defConfScale = 0.15;
static float defConfOffset = 0;
static int defConfMemSize = 2000;
/* static int tsmhits = 0;*/

/* ---------------------- Trace Routines ------------------------- */

#define BARLEN 80

void bar(char *s)
{
   int i,len,n;
   len = strlen(s);  n = (BARLEN - len) / 2;
   printf("\n");
   for (i=0; i<n; i++)printf("="); printf("%s",s);
   for (i=0; i<n; i++)printf("="); printf("\n");
}

void prl(char *s, float f)
{
   if (f==LZERO) printf("%s =%9s",s,"Log0"); else printf("%s =%9.2f",s,f);
}

void prn(NetNode *n,int frame)
{
   assert(n);
   if (!n->info.pron){
      printf("?");
   } else {
      assert(n->info.pron->word);
      assert(n->info.pron->word->wordName);
      assert(n->info.pron->word->wordName->name);
      printf(" %s[%d]",n->info.pron->word->wordName->name,frame);
   }
}


/* prpath: print given path (simple version - in reverse) */
static void prpath(Path *p)
{
   while (p != NULL){
      prn(p->owner->node,p->owner->frame); p = p->prev;
   }
}

/* TraceTokenSet: print summary of given TokenSet */
static void TraceTokenSet(char *title, TokenSet *tset)
{
   int i;
   RelToken *r;

   printf("    %s %d toks",title, tset->n);
   prl(" like",tset->tok.like);
   prl(" lm",tset->tok.lm);
   prpath(tset->tok.path);
   printf("\n");
   for (i=1,r=tset->set; i<=tset->n; i++,r++){
      printf("     rtok %d",i);
      prl(" like",r->like);
      prl(" lm",r->lm);
      prpath(r->path);
      printf("\n");
   }
}

/* TraceInst: print summary info on given instance */
static void TraceInst(NetInst *ni,int i)
{
   int j,n;
   NetNode *node = ni->node;
   char *name,buf[100];
   HLink h; Pron pr; Word w;
   MLink m;
   TokenSet *t;

   printf("%4d.",i);
   if (node==NULL) {printf(" no node\n"); return;}
   printf(" %2x ",node->type);
   if (node_hmm(node)) {
      h = node->info.hmm; assert(h != NULL);
      n=h->numStates-1;
      m = FindMacroStruct(h->owner,'h',h);
      name = m->id->name;
   }else{
      pr = node->info.pron;
      if (pr == NULL) {printf(" no pron\n"); return;}
      w = pr->word; assert(w != NULL);
      name = w->wordName->name;
      n = 1;
   }
   printf("%8s [N=%1d] ",name,n); prl(" max",ni->max);

   /* find next word if unambiguous */
   if (node->wordset == NULL) {
      printf(" ???\n");
   }else{
      printf(" %s\n",node->wordset->name);
   }

   if (trace&T_TOKS){
      for (j=1,t=ni->state; j<n; j++,++t){
         sprintf(buf,"St%2d",j);
         TraceTokenSet(buf,t);
      }
      TraceTokenSet("Exit",ni->exit);
   }
}

/* TraceInsts: find the NUMTRACEINSTS best instances and print them */
static void TraceInsts(PRecInfo *pri)
{
   NetInst *p, *bestInst[NUMTRACEINSTS];
   LogFloat best;
   int i,j, nbest = 0;
   best = LZERO;

   for (p=pri->head.link; p!=NULL; p=p->link) {
      if (p->max > best){
         for (i=0; i<nbest && p->max < bestInst[i]->max; i++);
         if (nbest<NUMTRACEINSTS) ++nbest;
         for (j=nbest-1; j>i; j--) {
            bestInst[j] = bestInst[j-1];
         }
         bestInst[i] = p;
         best = bestInst[nbest-1]->max;
      }
   }
   printf("Best insts:\n");
   for (i=0; i<nbest; i++) TraceInst(bestInst[i],i);
}

/* ---------------------- Module Initialisation ------------------ */

static AdaptXForm *inXForm;
static ConfParam *cParm[MAXGLOBS];      /* config parameters */
static int nParm = 0;


/* EXPORT->InitRec: register module & set configuration parameters */
void InitRec(void)
{
   int i;
   Boolean b;
   double x;
   char buf[256];

   Register(hrec_version);
   nParm = GetConfig("HREC", TRUE, cParm, MAXGLOBS);
   if (nParm>0){
      if (GetConfInt(cParm,nParm,"TRACE",&i)) trace = i;
      if (GetConfInt(cParm,nParm,"TRACEDELAY",&i)) traceDelay = i;
      if (GetConfBool(cParm,nParm,"FORCEOUT",&b)) forceOutput = b;
      defConfBGHMM[0] = '\0';
      if (GetConfFlt(cParm,nParm,"CONFSCALE",&x)) defConfScale = x;
      if (GetConfFlt(cParm,nParm,"CONFOFFSET",&x)) defConfOffset = x;
      if (GetConfInt(cParm,nParm,"CONFMEMSIZE",&i)) defConfMemSize = i;
      if (GetConfStr(cParm,nParm,"CONFBGHMM",buf)) strcpy(defConfBGHMM,buf);
   }
}

/* -------------------- Instance Chain Handling -------------------- */

/* MoveToRecent: move given inst to tail of the inst chain */
static void MoveToRecent(PRecInfo *pri, NetInst *inst)
{
   if (inst->node==NULL) return;

   /* If we are about to move the instance that is used to determine the   */
   /*  next instance to be stepped (to the most recent end of the list) we */
   /*  must use the previous instance to determine the next one to step !! */
   if (inst==pri->nxtInst) pri->nxtInst=inst->knil;

   inst->link->knil=inst->knil; inst->knil->link=inst->link;
   inst->link=&pri->tail;       inst->knil=pri->tail.knil;
   inst->link->knil=inst;       inst->knil->link=inst;

   inst->pxd=FALSE;  inst->ooo=TRUE;

#ifdef SANITY
   if (inst==pri->start_inst)
      HError(8521,"MoveToRecent: Loop resulted in circular move");
   inst->ipos=pri->ipos++;
#endif
}

/* ReOrderList: move the inst of any tr0 node following this node to
      the end of the inst list ie make it most recent.  Then apply
      the same process recursively to each of these tr0 nodes.   */
static void ReOrderList(PRecInfo *pri, NetNode *node)
{
   NetLink *dest;
   int i;

   if (node->inst == NULL || !node->inst->ooo ) return;
   node->inst->ooo=FALSE;
   for (i=0,dest=node->links;i<node->nlinks;i++,dest++) {
      /* tr0 nodes always come 1st, so break as soon as non-tr0 node found */
      if (!node_tr0(dest->node)) break;
      if (dest->node->inst!=NULL)  MoveToRecent(pri,dest->node->inst);
   }
   for (i=0,dest=node->links;i<node->nlinks;i++,dest++) {
      if (!node_tr0(dest->node)) break;
      if (dest->node->inst!=NULL)  ReOrderList(pri,dest->node);
   }
}

/* AttachInst: attach a NetInst record to given node */
static void AttachInst(PRecInfo *pri, NetNode *node)
{
   TokenSet *cur;
   NetInst *inst;
   int i,n;

   inst=(NetInst*) New(&pri->instHeap,0);
   if (node_hmm(node)) n=node->info.hmm->numStates-1; else n=1;

#ifdef SANITY
   if (pri->psi->stHeapIdx[n]<0)
      HError(8592,"AttachInst: State heap not created for %d states",n);
#endif
   inst->node=node;

   /* create unit exit tokenset and initialise to null_token */
   inst->exit=(TokenSet*) New(pri->stHeap+pri->psi->stHeapIdx[1],0);
   inst->exit->n=0; inst->exit->tok=null_token;
   if (pri->nToks>1) {
      inst->exit->set=(RelToken*) New(&pri->rTokHeap,0);
      inst->exit->n=1; inst->exit->set[0]=rmax;
   }

   /* create n-state token set */
   inst->state=(TokenSet*) New(pri->stHeap+pri->psi->stHeapIdx[n],0);
   for (i=1,cur=inst->state;i<=n;i++,cur++) {
      cur->n=0; cur->tok=null_token;
      if (pri->nToks>1) {
         cur->set=(RelToken*) New(&pri->rTokHeap,0);
         cur->n=1; cur->set[0]=rmax;
      }
   }

   /* Initialise max (ie best state score) to 0 */
   inst->max=LZERO;

   /* Link this inst into chain ending at pri->tail */
   inst->link=&pri->tail;    inst->knil=pri->tail.knil;
   inst->link->knil=inst;    inst->knil->link=inst;
   pri->nact++;

   /* Attach the inst to the node owning it */
   node->inst=inst;

   /* Ensure any currently alive following insts are moved */
   /*  to be more recent than it to ensure tokens propagated in */
   /*  correct order. */
   inst->ooo=TRUE;   /* Need keep list in propagation order */
#ifdef SANITY
   inst->ipos=pri->ipos++;  pri->start_inst=inst;
#endif
   ReOrderList(pri,node);
}

/* DetachInst: dispose inst attached to node */
static void DetachInst(PRecInfo *pri, NetNode *node)
{
   TokenSet *cur;
   NetInst *inst;
   int i,n;

   inst=node->inst; pri->nact--;
#ifdef SANITY
   if (inst->node!=node)
      HError(8591,"DetachInst: Node/Inst mismatch");
#endif
   inst->link->knil=inst->knil;
   inst->knil->link=inst->link;

   n=  (node_hmm(node)) ? node->info.hmm->numStates-1 : 1;
   if (pri->nToks>1) {
      for (i=0,cur=inst->state;i<n;i++,cur++)
         Dispose(&pri->rTokHeap,cur->set);
      Dispose(&pri->rTokHeap,inst->exit->set);
   }
   Dispose(pri->stHeap+pri->psi->stHeapIdx[n],inst->state);
   Dispose(pri->stHeap+pri->psi->stHeapIdx[1],inst->exit);
   Dispose(&pri->instHeap,inst);
   node->inst=NULL;
}


/* -------------------- Output Probability Caching -------------------- */

/* cSOutP: caching version of SOutP used when mixPDFs shared */
static LogFloat cSOutP(PRecInfo *pri, HMMSet *hset, int s, Observation *x,
                       StreamElem *se)
{
   PreComp *pre;
   int m;
   LogFloat bx,px,wt, det;
   MixtureElem *me;
   Vector v;

   /* Note hset->kind == SHAREDHS */
   v=x->fv[s];
   me=se->spdf.cpdf+1;
   if (se->nMix==1){     /* Single Mixture Case */
      pre = (me->mpdf->mIdx>0 && me->mpdf->mIdx<=pri->psi->nmp)
         ? pri->psi->mPre+me->mpdf->mIdx : NULL;
      if (pre==NULL) {
	bx= MOutP(ApplyCompFXForm(me->mpdf,v,pri->inXForm,&det,pri->obid),me->mpdf);
	bx += det;
      } else if (pre->id!=pri->obid) {
	bx= MOutP(ApplyCompFXForm(me->mpdf,v,pri->inXForm,&det,pri->obid),me->mpdf);
	bx += det;
	pre->id=pri->obid;
	pre->outp=bx;
      } else
         bx=pre->outp;
   } else {
      bx=LZERO;                   /* Multi Mixture Case */
      for (m=1; m<=se->nMix; m++,me++) {
         wt = MixLogWeight(hset, me->weight);
         if (wt>LMINMIX) {
            pre = (me->mpdf->mIdx>0 && me->mpdf->mIdx<=pri->psi->nmp)
               ? pri->psi->mPre+me->mpdf->mIdx : NULL;
            if (pre==NULL) {
	      px=MOutP(ApplyCompFXForm(me->mpdf,v,pri->inXForm,&det,pri->obid),me->mpdf);MOutP(v, me->mpdf);
	      px += det;
            } else if (pre->id!=pri->obid) {
	      px=MOutP(ApplyCompFXForm(me->mpdf,v,pri->inXForm,&det,pri->obid),me->mpdf);
	      px += det;
	      pre->id=pri->obid;
	      pre->outp=px;
            } else
               px=pre->outp;
            bx=LAdd(bx,wt+px);
         }
      }
   }
   return bx;
}


/* Version of POutP that caches outp values with frame id */
static LogFloat cPOutP(PRecInfo *pri, StateInfo *si)
{
   PSetInfo *psi = pri->psi;
   Observation *obs = pri->obs;
   PreComp *pre;
   LogFloat outp;
   StreamElem *se;
   Vector w;
   int s,S;

   if (si->sIdx>0 && si->sIdx<=pri->psi->nsp)
      pre=pri->psi->sPre+si->sIdx;
   else pre=NULL;

#ifdef SANITY
   if (pre==NULL)
      HError(8520,"cPOutP: State has no PreComp attached");
#endif

   if (pre->id != pri->obid) {
      if (FALSE && (psi->mixShared==FALSE))
      {
         outp=POutP(psi->hset,obs,si);
      }
      else {
         S=obs->swidth[0];
         if (S==1 && si->weights==NULL){
            outp=cSOutP(pri,psi->hset,1,obs,si->pdf+1);
         }
         else {
            outp=0.0;
            se=si->pdf+1;
            w=si->weights;
            for (s=1;s<=S;s++,se++){
               outp+=w[s]*cSOutP(pri,psi->hset,s,obs,se);
            }
         }
      }

      if (outp > pri->confinfo.bestp) {
         pri->confinfo.bestp = outp;
      }
      pri->confinfo.averp = 0.98*pri->confinfo.averp + 0.02*outp;
      pre->outp=outp;
      pre->id=pri->obid;
   }
   return(pre->outp);
}


/* ---------------------- Path Housekeeping ----------------------- */

#ifdef SANITY
static void CheckPaths(PRecInfo *pri, int k)
{
   int j,np,nr,ne;
   Ring *r;
   Path *p;

   if (pri->actvHead == NULL){
      if (pri->actvTail != NULL) {
         printf("Error at %d: active ring list has null head but nonnull tail\n");
         exit(0);
      }
   }else{
      r = pri->actvHead;
      if (r->prev != NULL) {
         printf("Error at %d: 1st elem of active ring list has nonnull prev\n");
         exit(0);
      }
      r = pri->actvTail;
      if (r->next != NULL) {
         printf("Error at %d: last elem of active ring list has nonnull next\n");
         exit(0);
      }
   }

   for (j=0,np=0,nr=0,ne=0,r=pri->actvHead; r != NULL; r=r->next,j++) {
      np += r->nPaths; nr += r->nRefs;
      for (p=r->head; p!=NULL; p=p->next)
         if (p->prev == NULL) ++ne;
   }
   if (j != pri->nusedRings) {
      printf("Error at %d: active ring list has %d elems but pri->nusedRings = %d\n",k,j,pri->nusedRings);
      exit(0);
   }
   if (np != pri->nusedPaths) {
      printf("Error at %d: active ring list has %d paths total but pri->nusedPaths = %d\n",k,np,pri->nusedPaths);
      exit(0);
   }
   if (ne+nr != np){
      printf("Error at %d: nrefs=%d, nullprevs=%d, npaths=%d [pri->nusedPaths=%d]\n",k,ne,nr,np,pri->nusedPaths);
      exit(0);
   }
   for (j=0,r=pri->freeRing; r != NULL; r=r->next,j++);
   if (j != pri->nfreeRings) {
      printf("Error at %d: free ring list has %d elems but pri->nfreeRings = %d\n",k,j,pri->nfreeRings);
      exit(0);
   }

}
#endif

/* InitPathRings: reset rings/paths ready for recognition */
static void InitPathRings(PRecInfo *pri)
{
   ResetHeap(&pri->pathHeap);
   ResetHeap(&pri->ringHeap);
   pri->nfreeRings = pri->nfreePaths = 0;
   pri->nusedRings = pri->nusedPaths = 0;
   pri->nusedLastCollect = 0;
   pri->freePath = NULL;
   pri->freeRing = pri->actvHead = pri->actvTail = NULL;
#ifdef SANITY
   /* CheckPaths(pri,1); */
#endif
}

/* NewRing: return a new empty ring */
static Ring *NewRing(PRecInfo *pri, NetNode *node, int frame)
{
   Ring *r;

   if (pri->freeRing != NULL) {
      r = pri->freeRing; pri->freeRing = r->next;
      --pri->nfreeRings;
   } else
      r = (Ring *)New(&pri->ringHeap,sizeof(Ring));
   r->nRefs = r->nPaths = 0; r->status = cNorm; r->mark=0;
   r->head = r->tail = NULL;
   r->next = NULL;
   r->node = node; r->frame = frame;
   /* Link new ring into active ring list */
   if(pri->actvHead == NULL){
      r->prev = NULL;
      pri->actvHead = pri->actvTail = r;
   }else{
      r->prev = pri->actvTail;
      pri->actvTail->next = r;
      pri->actvTail = r;
   }
   ++pri->nusedRings;
#ifdef SANITY
   /* CheckPaths(pri,2); */
#endif
   return r;
}

/* NewPath: return a new path in given ring */
static Path *NewPath(PRecInfo *pri, Ring *ring, LogFloat like, LogFloat lm)
{
   Path *p;

   if (pri->freePath != NULL){
      p = pri->freePath; pri->freePath = p->next;
      --pri->nfreePaths;
   }else{
      p = (Path *)New(&pri->pathHeap,sizeof(Path));
   }
   p->owner = ring; p->next = p->prev = NULL;
   p->like = like; p->lm = lm;
   p->hist.key = 0;
   p->trbkCount = 0;
   /* link new path into ring */
   if (ring->tail == NULL) ring->head = p; else ring->tail->next = p;
   ring->tail = p;  ++ring->nPaths;
   ++pri->nusedPaths;
#ifdef SANITY
   /* CheckPaths(pri,3); */
#endif
   return p;
}

/* RefPath: record reference to given path */
static void RefPath(Path *p)
{
   ++p->owner->nRefs;
}

/* DeleteRing:  delete a ring and all paths in it */
static void DeleteRing(PRecInfo *pri, Ring *r)
{
   /* Unlink the ring */
   if (r == pri->actvHead){ /* deleting first ring */
      if (r == pri->actvTail) {  /* ... and only ring */
         pri->actvHead = pri->actvTail = NULL;
      }else{
         pri->actvHead = r->next;
         pri->actvHead->prev = NULL;
      }
   } else if (r == pri->actvTail) {
      pri->actvTail = r->prev;
      pri->actvTail->next = NULL;
   } else {
      r->prev->next = r->next;
      r->next->prev = r->prev;
   }
   /* Link the ring's paths into the free path list */
   if (r->nPaths>0){
      r->tail->next = pri->freePath; pri->freePath = r->head;
      pri->nfreePaths += r->nPaths;
      pri->nusedPaths -= r->nPaths;
   }
   /* Link the ring into the free ring list */
   r->next = pri->freeRing; pri->freeRing = r;
   --pri->nusedRings; ++pri->nfreeRings;
}

/* DeRefPath: decrement refs to ring owning this path */
static void DeRefPath(PRecInfo *pri, Path *p)
{
   int i;
   Path *q;
   Ring *r = p->owner;

   if (r->status == cDeleted) return;
   --r->nRefs;
   if (r->nRefs == 0 && r->status != cMarked) {  /* this ring now unused */
      for (i=0,q=r->head; i<r->nPaths; i++,q=q->next){
         if (q->prev != NULL) DeRefPath(pri,q->prev);
      }
      r->status = cDeleted;
   } else  {
      if (r->nRefs<0)
      HError(8595,"ring ref count -ve [%d] (nused=%d, nfree=%d)",
                   r->nRefs,pri->nusedRings,pri->nfreeRings);
   }
}

/* CollectPaths: scan path rings and collect garbage back into free lists */
static void CollectPaths(PRecInfo *pri)
{
   Ring *r,*rnext;
   Path *p,*q;
   NetInst *ni;
   TokenSet *t;
   NetNode *node;
   int i, j, k, n, pcol = 0, rcol = 0;

   if (trace&T_GBG){
      printf("Frame %d: path collection:  last = %d  now-> paths = %d (%d free) %d rings (%d free)\n",
              pri->frame, pri->nusedLastCollect, pri->nusedPaths, pri->nfreePaths,
              pri->nusedRings, pri->nfreeRings);

   }
   /* first scan all active paths and mark referenced rings */
   for (ni=pri->head.link,i=1; ni!=NULL && i<=pri->nact; ni=ni->link,i++) {
      node = ni->node;
      n = (node_hmm(node))?node->info.hmm->numStates-1:1;
      for (j=1,t=ni->state; j<=n; j++,t++) {
         if (pri->nToks <= 0) {
            for (p = t->tok.path; p!=NULL; p = p->prev)
               p->owner->status = cMarked;
         }else{
            for (k=0; k<t->n; k++){
               for (p = t->set[k].path; p!=NULL; p = p->prev)
                  p->owner->status = cMarked;
            }
         }
      }
   }
   /* scan rings and mark inactive and unreferenced rings for deletion */
   for (r=pri->actvHead; r != NULL; r=r->next){
      if (r->status != cMarked  && r->nRefs == 0){
         for (i=0,q=r->head; i<r->nPaths; i++,q=q->next){
            if (q->prev != NULL) DeRefPath(pri,q->prev);
         }
         r->status = cDeleted;
      }
   }
   /* scan rings again and really delete unused rings */
   for (r=pri->actvHead; r != NULL; r=rnext){
      rnext = r->next;
      if (r->status == cDeleted){
         pcol += r->nPaths; DeleteRing(pri,r); ++rcol;
      }else
         r->status = cNorm;
   }
   pri->nusedLastCollect = pri->nusedPaths;
#ifdef SANITY
   /* CheckPaths(pri,4); */
#endif
   if (trace&T_GBG){
      printf("   %d paths (%d rings) collected\n",pcol,rcol);
      printf("   now-> paths = %d (%d free) %d rings (%d free)\n",
              pri->nusedPaths, pri->nfreePaths,
              pri->nusedRings, pri->nfreeRings);
   }
}

/* -------------------- Token Set Handling -------------------- */

/* TokSetMerge:  used to propagate token from state i (src) -> j (res)
   cmp is copy of src->tok with like updated by a_ij.  Tokens below
   nThresh (ie outside nBeam) are ignored */

static void TokSetMerge(PRecInfo *pri, TokenSet *res,Token *cmp,TokenSet *src)
{
   RelToken *cur,*stoks,rtoks[MAX_TOKS];
   int nr,ns,nn,is,ir,k,hash;
   unsigned int key,nullkey;
   LogFloat sdiff,rdiff,baselike,bestlike,slike,rlike,limit;
   Boolean sIsBest;

#ifdef DETAILED_TRACING
   if ((trace&T_TSM) && (pri->frame >= traceDelay)) {
      printf("Token update at frame %d: ",pri->frame);
      prl("lm",cmp->lm); prl("like",cmp->like); prpath(cmp->path);
      printf("\n");
      TraceTokenSet("Initial Target",res);
      TraceTokenSet("Source",src);

   }
#endif
   sdiff = rdiff = 0; sIsBest = FALSE;
   if (cmp->like>=res->tok.like) {
      if (cmp->like>pri->nThresh) {
         if (res->tok.like>pri->nThresh) {
			 /* res is in beam but cmp is better - so make res rel to cmp */
			 rdiff = res->tok.like - cmp->like;
			 res->tok = *cmp;
          sIsBest = TRUE;
		 }else{
            /* res is below beam so copy src to res & no more to do */
            res->tok=*cmp; res->n=src->n;
            for (k=0;k<src->n;k++) res->set[k]=src->set[k];
            return;
         }
	  } else {
		  /* cmp is below beam so do nothing */
		  return;
	  }
   } else {
      if (cmp->like < pri->nThresh) return;
	  /* cmp is in beam but res is better - so make src rel to res */
	  sdiff = cmp->like - res->tok.like;
   }
   /* need to do proper merge so copy res to rtoks */
   nr = res->n;
   for (k=0; k<nr; k++) rtoks[k] = res->set[k];
   ns = src->n; stoks = src->set;

   /* Now merge token sets in rtoks and stoks into res->set */
   baselike = res->tok.like;     /* all reltokens are relative to this */
   limit=pri->nThresh-baselike;  /* require every rel.like+diff > limit */
   nn = 0;                       /* num reltoks stored in res */
   is = ir = 0;                  /* next candidate in each source */
   nullkey = 027777777777;
   if (sIsBest){
      cur = stoks; ++is;
   }else{
      cur = rtoks; ++ir;
   }
   while (cur != NULL) {    /* main merge loop */

      key = (cur->path)?cur->path->hist.key:nullkey;    /* look in hash to see if key is new */
      hash =  key % TSM_HASH;

      while (! (pri->keyhash[hash] == key || pri->keyhash[hash] == 0)) {
         hash = (hash+1) % TSM_HASH;
         /*  ++tsmhits;*/
      }
      if (pri->keyhash[hash]==0) {  /* new key so keep this reltoken */
         pri->keyhash[hash] = key;
         pri->keyused[nn] = hash;
         res->set[nn] = *cur;
         if (sIsBest) {
            res->set[nn].like += sdiff;
         }else{
            res->set[nn].like += rdiff;
         }
         ++nn;
      }
      cur = NULL;                   /* find next candidate */
      if (nn < pri->nToks && (is<ns || ir<nr)){
         bestlike = slike = rlike = LZERO;
         if (is<ns){
            slike = sdiff + stoks[is].like;
            if (slike < limit) is = ns; else {
               bestlike = slike; sIsBest = TRUE;
            }
         }
         if (ir<nr){
            rlike = rdiff + rtoks[ir].like;
            if (rlike < limit) ir = nr; else {
               if (rlike > bestlike){
                  bestlike = rlike; sIsBest = FALSE;
               }
            }
         }
         if (bestlike>limit){
            if (sIsBest){
               cur = stoks+is; ++is;
            }else{
               cur = rtoks+ir; ++ir;
            }
         }
      }
   } /* end of main merge loop */

   /* res tokenset now complete */
   res->n = nn;

   /* finally clean up keyhash for next use */
   for (k=0; k<nn; k++) pri->keyhash[pri->keyused[k]] = 0;

#ifdef DETAILED_TRACING
   if ((trace&T_TSM) && (pri->frame >= traceDelay)) {
      TraceTokenSet("Final Target",res);
      printf("\n");
   }
#endif
}

/* BTreeToTokSet: copy contents of btree back into tset in sort order */
static RelToken * BTreeToTokSet(RTokBinTreeNode *btree, int offset, RelToken *set)
{
   RelToken *p = set;
   RTokBinTreeNode *b = btree+offset;

   if (b->right > 0 )
      p = BTreeToTokSet(btree,b->right,p);
   *p++ = b->rtok;
   if (b->left > 0 )
      p = BTreeToTokSet(btree,b->left,p);

   return p;
}

/* RebaseTokSet: restore relative likes in given Token Set */
static void RebaseTokSet(TokenSet *tset)
{
   int i;
   LogFloat rlike = tset->set[0].like;

   for (i=0; i<tset->n; i++)
      tset->set[i].like -= rlike;
   tset->tok.like += rlike;
   tset->tok.lm = tset->set[0].lm;
   tset->tok.path = tset->set[0].path;
}


/* ------------------------- Utility Routines ---------------------- */

/* qcksrtM: Sort given array of floats */
static void qcksrtM(float *array,int l,int r,int M)
{
   int i,j;
   float x,tmp;

   if (l>=r || l>M || r<M) return;
   x=array[(l+r)/2];i=l-1;j=r+1;
   do {
      do i++; while (array[i]>x);
      do j--; while (array[j]<x);
      if (i<j) {
         tmp=array[i];array[i]=array[j];array[j]=tmp;
      }
   }
   while(i<j);
   if (j<M) qcksrtM(array,j+1,r,M);
   else qcksrtM(array,l,j,M);
}

/* --------------------- Pass One Token Propagation --------------- */

/* StepHMM1: first pass internal token propagation in HMMs */
static void StepHMM1(PRecInfo *pri, NetNode *node)
{
   NetInst *inst = node->inst;
   HMMDef *hmm = node->info.hmm;
   Token max;
   TokenSet *res,cmp,*cur;
   int i,j,k,N,endi;
   LogFloat outp;
   Matrix trP;
   short **seIndex;

   max=null_token; N=hmm->numStates; trP=hmm->transP;
   seIndex=pri->psi->seIndexes[hmm->tIdx];

   /* Scan emitting states first */
   for (j=2,res=pri->psi->sBuf+2;j<N;j++,res++) {
      i=seIndex[j][0];  endi=seIndex[j][1]; cur=inst->state+i-1;
      /* first assume a transition from i to j */
      res->tok=cur->tok; res->n=cur->n;
      for (k=0;k<cur->n;k++) res->set[k]=cur->set[k];
      res->tok.like += trP[i][j];
      /* then compare with all other possible predecessor states */
      for (i++,cur++;i<=endi;i++,cur++) {
         cmp.tok=cur->tok; cmp.tok.like+=trP[i][j];
         if (res->n==0) {
            if (cmp.tok.like > res->tok.like) res->tok=cmp.tok;
         } else {
            TokSetMerge(pri,res,&cmp.tok,cur);
         }
      }
      /* if above pruning threshold add on the output prob */
      if (res->tok.like>pri->genThresh) {
         outp=cPOutP(pri,hmm->svec[j].info);
         res->tok.like += outp;
         /* update max like for this instance */
         if (res->tok.like>max.like) max=res->tok;
         /* record state level alignment if needed */
      } else {
         /* prune this state */
         res->tok=null_token; res->n=((pri->nToks>1)?1:0);
      }
   }

   /* Null entry state ready for external propagation and
      copy tokens from buffer states 2 to N-1 to instance */
   for (i=1,res=pri->psi->sBuf+1,cur=inst->state; i<N;i++,res++,cur++) {
      cur->n=res->n; cur->tok=res->tok;
      for (k=0;k<res->n;k++) cur->set[k]=res->set[k];
   }

   /* Update general beam info */
   if (max.like > pri->genMaxTok.like) {
      pri->genMaxTok=max; pri->genMaxNode=node;
   }
   inst->max = max.like;

   /* Setup for exit state (ignoring tee trP) */
   i=seIndex[N][0];  endi=seIndex[N][1];
   /* first assume a transition from i to N */
   res=inst->exit; cur=inst->state+i-1;
   res->n=cur->n; res->tok=cur->tok;
   for (k=0;k<cur->n;k++) res->set[k]=cur->set[k];
   res->tok.like += trP[i][N];
   /* then compare with all other possible predecessor states */
   for (i++,cur++;i<=endi;i++,cur++) {
      cmp.tok=cur->tok;  cmp.tok.like+=trP[i][N];
      if (res->n==0) {
         if (cmp.tok.like > res->tok.like) res->tok=cmp.tok;
      } else {
         TokSetMerge(pri,res,&cmp.tok,cur);
      }
   }

   /* Update word beam info */
   if (res->tok.like>LSMALL){
      if (res->tok.like > pri->wordMaxTok.like) {
         pri->wordMaxTok=res->tok; pri->wordMaxNode=node;
      }
   } else {
      inst->exit->tok=null_token;
      inst->exit->n=((pri->nToks>1)?1:0);
   }
}

/* StepWord1: just invalidate the tokens */
static void StepWord1(PRecInfo *pri, NetNode *node)
{
   node->inst->state->tok=null_token;
   node->inst->state->n=((pri->nToks>1)?1:0);
   node->inst->exit->tok=null_token;
   node->inst->exit->n=((pri->nToks>1)?1:0);
   node->inst->max=LZERO;
}

/* StepInst1: First pass of token propagation (Internal) */
static void StepInst1(PRecInfo *pri, NetNode *node)
{
   if (node_hmm(node))
      StepHMM1(pri,node);   /* Advance tokens within HMM instance t => t-1 */
   /* Entry tokens valid for t-1, do states 2..N */
   else
      StepWord1(pri,node);
   node->inst->pxd=FALSE;
}

/* --------------------- Pass Two Token Propagation --------------- */

/* StepHMM2: propagate entry to exit in Tee models (may be repeated) */
static void StepHMM2(PRecInfo *pri, NetNode *node)
{
   NetInst *inst = node->inst;
   HMMDef *hmm = node->info.hmm;
   Token cmp;
   TokenSet *res,*cur;
   int N = hmm->numStates;

   cur=inst->state; res=inst->exit;
   cmp=cur->tok;
   cmp.like += hmm->transP[1][N];

   if (res->n==0) {
      if (cmp.like>res->tok.like) res->tok=cmp;
   } else {
      TokSetMerge(pri,res,&cmp,cur);
   }
}

/* StepWord2: external word propagation/path update - may be repeated */
static void StepWord2(PRecInfo *pri, NetNode *node)
{
   NetInst *inst=node->inst;
   Ring *r;
   Path *newpth,*oldpth, *p;
   RelToken *src,*tgt;
   int i,k;
   LabId thisword = NULL;
   LMHistory nullHist;
   Pron pron;
   LogFloat absLike;

   nullHist.key = 0;
   pron = node->info.pron;
   if (pron==NULL && node->tag==NULL) {
      /* If regular NULL node just propagate from entry to exit */
      inst->exit->tok=inst->state->tok;
      inst->exit->n=inst->state->n;
      for (k=0;k<inst->exit->n;k++)
         inst->exit->set[k]=inst->state->set[k];
   } else {
      /* Word or tagged Null Node */
      inst->exit->tok = inst->state->tok;
      absLike = inst->state->tok.like;    /* need this for rel token paths */
      /* If word add on pronunciation prob */
      if (pron!=NULL) {
         inst->exit->tok.like += pron->prob*pri->pronScale;
         /* originally did this only if no ngram and then added wordPen later when
            id of next word known - but now cant see point of delaying */
         inst->exit->tok.like += pri->wordPen;
         thisword = pron->word->wordName;
      }

      /* Extend paths to record this word */
      r = NewRing(pri,node,pri->frame);
      newpth=NewPath(pri,r,inst->exit->tok.like,inst->exit->tok.lm);

      inst->exit->n = 0;       /* assume 1-best by default */
      inst->exit->tok.path=newpth;
      inst->exit->tok.lm=0.0;  /* reset lm ready for next word */
#ifdef SANITY
      if (pri->lm && inst->exit->tok.ngPending){
         HError(8520,"StepWord: word %s reached with NG still pending",
            thisword==NULL?"?":thisword->name);
      }
#endif
      inst->exit->tok.ngPending=TRUE;

      /* Link new path to front of existing path(s) */
      newpth->prev = oldpth = inst->state->tok.path;
      if (oldpth!=NULL) RefPath(oldpth);  /* inc ref count on existing path */

      /* Record history in path */
      for (p=newpth->prev; p != NULL && p->hist.key == 0; p = p->prev);
      if (p==NULL || p->hist.key == 0){
         newpth->hist = SetLMHistory(pri->lm,thisword,nullHist);
      }else{
         newpth->hist = SetLMHistory(pri->lm,thisword,p->hist);
      }

      /* if multiple tokens, then update their individual paths */
      if (pri->nToks>1) {
         /* if ngram then need to propagate all toks, otherwise all
         tokens can be merged here */
         inst->exit->n = (pri->lm)?inst->state->n:1;

         inst->exit->set[0].like = 0.0;
         inst->exit->set[0].lm   = 0.0;
         inst->exit->set[0].path = newpth;

         /* update remaining tokens */
         src = inst->state->set+1; tgt = inst->exit->set+1;
         for (i=1; i<inst->state->n; i++,src++) {
            /* first record word boundaries - do this regardless if ntoks>1 */
            /*newpth=NewPath(pri,r,src->like+absLike,src->lm);*/
            newpth=NewPath(pri,r,src->like+inst->exit->tok.like,src->lm);

            newpth->prev = oldpth = src->path;
            if (oldpth != NULL) RefPath(oldpth);  /* inc ref count on existing path */
            /* Record history in path */
            for (p=newpth->prev; p != NULL && p->hist.key == 0; p = p->prev);
            if (p==NULL || p->hist.key == 0){
               newpth->hist = SetLMHistory(pri->lm,thisword,nullHist);
            }else{
               newpth->hist = SetLMHistory(pri->lm,thisword,p->hist);
            }
            /* if we have an ngram, then need to copy to tgt */
            if (pri->lm){
               tgt->like = src->like;
               tgt->lm   = 0.0;
               tgt->path = newpth;
               tgt++;
            }
         }
      }
   }
}

/* SetEntryState: propagate src tokset into given entry node */
static void SetEntryState(PRecInfo *pri, NetNode *node,TokenSet *src)
{
   NetInst *inst;
   TokenSet *res;
   NetNode *wmax;

   if (node->inst==NULL) AttachInst(pri,node);
   inst=node->inst;  res=inst->state;
#ifdef SANITY
   if ((res->n==0 && src->n!=0) || (res->n!=0 && src->n==0))
      HError(8590,"SetEntryState: TokenSet size mismatch");
#endif
   if (res->n==0) {
      if (src->tok.like > res->tok.like) res->tok=src->tok;
   } else {
      TokSetMerge(pri,res,&src->tok,src);
   }
   /* Update the entry nodes max like */
   if (res->tok.like>inst->max) inst->max = res->tok.like;
   wmax = pri->wordMaxNode;
   /* Update the global word max node */
   if (node->type==n_word &&
        (wmax==NULL || wmax->inst==NULL || res->tok.like > wmax->inst->max))
      pri->wordMaxNode = node;
}

/* StepInst2: Second pass of token propagation (External) */
static void StepInst2(PRecInfo *pri, NetNode *node)
/* Must be able to survive doing this twice !! */
{
   TokenSet xtok, *exit;
   RelToken *rp,*rq,rtoks[MAX_TOKS];
   NetLink *dest;
   LogFloat linkLM,ngLM,rngLM,like;
   int i,j,k;
   LMHistory h,hmain,hlast;
   LabId nextword,w1,w0;
   RTokBinTreeNode *binp,*binq,btree[MAX_TOKS];
   Boolean found;

   /* Do the word or HMM token propagation */
   if (node_word(node))
      StepWord2(pri,node);  /* Merge tokens and update traceback */
   else if (node_tr0(node) /* && node_hmm(node) */)
      StepHMM2(pri,node);   /* Advance tokens within HMM instance t => t-1 */
   exit = node->inst->exit;

   /* Apply word beam pruning */
   if (node_word(node)){
      if (exit->tok.like<pri->wordThresh) {
         node->inst->pxd=TRUE; return;
      }
   }

   /* If still in general beam then propagate it to link nodes */
   if (exit->tok.like>pri->genThresh) {

      /* Create storage for local copy of node's exit token */
       xtok.set=rtoks;

      /* Process all possible continuation nodes for this token */
      for(i=0,dest=node->links;i<node->nlinks;i++,dest++) {
         xtok.n=node->inst->exit->n;

         linkLM = dest->linkLM*pri->lmScale;       /* Pickup LMProb from link */
         xtok.tok.like = exit->tok.like+linkLM;    /* update total likelihood */

         /* skip if linkLM pushes token outside of beam */
         if (xtok.tok.like<pri->genThresh) continue;

         xtok.tok.lm   = exit->tok.lm+linkLM;
         xtok.tok.ngPending = exit->tok.ngPending;
         xtok.tok.path = exit->tok.path;
         for (rp=xtok.set,rq=exit->set,k=0; k<xtok.n; k++,rp++,rq++) {
            rp->like = rq->like; rp->lm = rq->lm + linkLM; rp->path = rq->path;
         }

         /* If using N-gram lm, need to add in ngram prob as soon as it is known */
         /* (this might be some time into the word if phone net is tree structured */

         if (pri->lm != NULL && xtok.tok.ngPending){
            nextword = dest->node->wordset;
            if (nextword != NULL) {  /* then next word now known */
               h.key=0;
               if (xtok.tok.path!=NULL) h = xtok.tok.path->hist;
               ngLM = GetLMProb(pri->lm,h,nextword)*pri->ngScale;
               hmain.key = h.key;
#ifdef DETAILED_TRACING
               if ((trace&T_WLM) && (pri->frame >= traceDelay)){
                  printf("%d. NGRAM Update -> %s\n",pri->frame,nextword->name);
                  /* TraceTokenSet("Initial Tokenset",&xtok); */
                  w0 = GetLMName(pri->lm,h.voc[0]);
                  w1 = GetLMName(pri->lm,h.voc[1]);
                  printf("    linkLM=%f P(%s|%s,%s)=%f\n",
                      linkLM,nextword->name,
                     (w0==NULL)?"-":w0->name, (w1==NULL)?"-":w1->name, ngLM);
               }
#endif
               xtok.tok.like += ngLM;      /* NB used to add wordPen here also */

               /* skip if ngram LM pushes token outside of beam  */
               if (xtok.tok.like<pri->genThresh) continue;

               /* token lm tracks scaled lm scores (linkLM +ngramLM), wordPen can
                  be adjusted for retrospectively in final traceback */
               xtok.tok.lm += ngLM;

               /* if multiple tokens, process reltokens */
               if (pri->nToks>1 > 0){
                  binp = btree;
                  for (k=0,j=0,rp=xtok.set; k<xtok.n; k++,rp++){

                     h.key=0;
                     if (rp->path!=NULL) h = rp->path->hist;

                     if (k==0 || h.key != hlast.key) {
                        if (h.key == hmain.key) {
                           rngLM = ngLM;
                        }else {
                           rngLM = GetLMProb(pri->lm,h,nextword)*pri->ngScale;
                        }
                        hlast.key = h.key;
                     }

                     rp->lm += rngLM;  rp->like += rngLM-ngLM;

                     /* Prune rel tokens which ngram pushes outside the NBeam */
                     if (xtok.tok.like+rp->like < pri->nThresh) continue;

                     binp->rtok = *rp;  binp->left = binp->right = 0;
                     if (j>0) {
                        like = rp->like; found = FALSE; binq=btree;
                        while (!found){
                           if (binq->left == 0  && like <= binq->rtok.like) {
                              binq->left = j; found = TRUE;
                           } else if (binq->right == 0 && like > binq->rtok.like){
                              binq->right = j; found = TRUE;
                           } else  {
                              binq = (like <= binq->rtok.like) ? btree+binq->left : btree+binq->right;
                           }
                        }
                     }
                     binp++; j++;

#ifdef DETAILED_TRACING
                     if ((trace&T_WLM) && (pri->frame >= traceDelay)){
                        w0 = GetLMName(pri->lm,h.voc[0]);
                        w1 = GetLMName(pri->lm,h.voc[1]);
                        printf("      (%d=%f,%f) P(%s|%s,%s)=%f\n",
                           k, rp->like,rp->lm,nextword->name,
                           (w0==NULL)?"-":w0->name, (w1==NULL)?"-":w1->name, rngLM);
                     }
#endif
                  }
                  xtok.n = j;
                  if (j>0){
                     BTreeToTokSet(btree,0,xtok.set);
                     RebaseTokSet(&xtok);
                  }
               }
#ifdef DETAILED_TRACING
               if ((trace&T_WLM) && (pri->frame >= traceDelay)){
                  TraceTokenSet("NGram Update Completed",&xtok);
               }
#endif
               xtok.tok.ngPending = FALSE;
            }

         }

         /* Xfer tokenset to destination node, activating when necessary */
         if (xtok.tok.like>pri->genThresh && (pri->nToks==0 || xtok.n > 0)) {
            SetEntryState(pri,dest->node,&xtok);
         }
      }
   }
   node->inst->pxd=TRUE;
}

/* ------------------------- HMMSet Initialisation --------------------- */

/* CreateSEIndex: compute range of i values for each transition i->j */
static void CreateSEIndex(PSetInfo *psi,HLink hmm)
{
   SMatrix trP;
   short **se; /* Actually (*se)[2] */
   int j,min,max,N;

   trP=hmm->transP; N=hmm->numStates;
   se=psi->seIndexes[hmm->tIdx];
   if (se==NULL) {
      se=(short**) New(&psi->heap,(N-1)*sizeof(short*));
      se-=2;
      for (j=2;j<=N;j++) {
         se[j]=(short*) New(&psi->heap,2*sizeof(short));
         for (min=(j==N)?2:1;min<N;min++) /* Don't want tee transitions */
            if (trP[min][j]>LSMALL) break;
            for (max=N-1;max>1;max--)
               if (trP[max][j]>LSMALL) break;
#ifdef SANITY
               if (min>max) {
                  HError(-8520,"CreateSEIndex: No transitions to state %d",j);
                  min=(j==N)?2:1;
                  max=N-1;
               }
#endif
               se[j][0]=min;
               se[j][1]=max;
      }
      psi->seIndexes[hmm->tIdx]=se;
   }
}

/* InitPSetInfo: prepare HMMSet for recognition.  Allocates seIndex
                  and preComp from hmmset heap.*/
PSetInfo *InitPSetInfo(HMMSet *hset)
{
   PSetInfo *psi;
   RelToken *rtoks;
   int n,h,i;
   HLink hmm;
   MLink q;
   PreComp *pre;
   char name[80];
   static int psid=0;

   psi=(PSetInfo*) New(&gcheap,sizeof(PSetInfo));
   psi->hset=hset;
   sprintf(name,"PRI-%d Heap",psid++);
   CreateHeap(&psi->heap,name,MSTAK,1,1.0,1000,8000);

   psi->max=MaxStatesInSet(hset)-1;

   psi->tBuf=(Token*) New(&psi->heap,(psi->max-1)*sizeof(Token));
   psi->tBuf-=2;

   psi->sBuf=(TokenSet*) New(&psi->heap,psi->max*sizeof(TokenSet));
   rtoks=(RelToken*) New(&psi->heap,psi->max*sizeof(RelToken)*MAX_TOKS);
   psi->sBuf-=1;
   for (i=0; i<psi->max; i++) {
      psi->sBuf[i+1].set=rtoks;rtoks+=MAX_TOKS;
      psi->sBuf[i+1].tok=null_token;
      psi->sBuf[i+1].n=0;
      psi->sBuf[i+1].set[0]=rmax;
   }

   psi->stHeapIdx=(short*) New(&psi->heap,(psi->max+1)*sizeof(short));
   for (i=0; i<=psi->max; i++) psi->stHeapIdx[i]=-1;
   psi->stHeapIdx[1]=0; /* For one state word end models */

   psi->ntr=hset->numTransP;
   psi->seIndexes=(short***) New(&psi->heap, sizeof(short**)*psi->ntr);
   psi->seIndexes--;
   for(i=1;i<=psi->ntr;i++) psi->seIndexes[i]=NULL;
   for (h=0; h<MACHASHSIZE; h++){
      for (q=hset->mtab[h]; q!=NULL; q=q->next) {
         if (q->type=='h') {
            hmm=(HLink)q->structure;
            n=hmm->numStates-1;
            psi->stHeapIdx[n]=0;
            CreateSEIndex(psi,hmm);
         }
      }
   }
   psi->nsp=hset->numStates;
   psi->sPre=(PreComp*) New(&psi->heap, sizeof(PreComp)*psi->nsp);
   psi->sPre--;
   for(i=1,pre=psi->sPre+1;i<=psi->nsp;i++,pre++) pre->id=-1;
   if (hset->numSharedMix>0) {
      psi->mixShared=TRUE;
      psi->nmp=hset->numSharedMix;
      psi->mPre=(PreComp*) New(&psi->heap, sizeof(PreComp)*psi->nmp);
      psi->mPre--;
      for(i=1,pre=psi->mPre+1;i<=psi->nmp;i++,pre++) pre->id=-1;
   } else
      psi->mixShared=FALSE,psi->nmp=0,psi->mPre=NULL;

   for (n=1,i=0;n<=psi->max;n++){
      if (psi->stHeapIdx[n]>=0) psi->stHeapIdx[n]=i++;
   }
   psi->stHeapNum=i;
   if (trace&T_TOP) {
      bar("Build PSetInfo");
      printf("HMMSET:\n");
      PrintHSetProfile(stdout, hset);
      printf("TOKEN BUFFER INFO\n");
      printf(" max=%d, nsp=%d, nmp=%d, ntr=%d\n",
         psi->max,psi->nsp,psi->nmp,psi->ntr);
      printf("  stHeapIdx: ");
      for (n=1;n<=psi->max;n++) printf("%3d",n); printf("\n");
      printf("  %3d heaps  ",psi->stHeapNum);
      for (n=1;n<=psi->max;n++) printf("%3d",psi->stHeapIdx[n]); printf("\n");
   }
   return(psi);
}

/* FreePSetInfo: free PSetInfo rec */
void FreePSetInfo(PSetInfo *psi)
{
   DeleteHeap(&psi->heap);
   Dispose(&gcheap,psi);
}


/* --------------  Confidence Measure Calculation -------------------- */
/* SJY 24/7/03 */

/* InitBGConfRec: set the background HMM and allocate score array */
static void InitBGConfRec(BGConfRec *bcp, HMMSet *hset)
{
   MLink macroName;
   LabId labid;

   bcp->idx  = bcp->frame = 0;
   bcp->bestp = LZERO;
   bcp->averp = 0.0;
   bcp->size = defConfMemSize;
   bcp->hmm = NULL;
   bcp->bgdlike = NULL;
   bcp->maxlike = (LogFloat*) New(&gcheap,defConfMemSize*sizeof(LogFloat));
   bcp->bgdlike = (LogFloat*) New(&gcheap,defConfMemSize*sizeof(LogFloat));
   if(defConfBGHMM[0] != '\0'){
      if((labid = GetLabId( defConfBGHMM, FALSE ))==NULL)
         HError(8593,"InitBGConfRec: Unknown backgound model label %s",defConfBGHMM);
      if((macroName=FindMacroName(hset,'l',labid))==NULL)
         HError(8593,"InitBGConfRec: Unknown background model label %s",labid->name);
      bcp->hmm = (HLink)macroName->structure;
   }
}

/* ResetBGConfRec: reset the like array and indices */
static void ResetBGConfRec(BGConfRec *bcp)
{
   bcp->idx  = bcp->frame = 0;
   bcp->bestp = LZERO;
}

/* UpdateBGConfRec: update the likelihood array with best state/bg model score */
static void UpdateBGConfRec(PRecInfo *pri)
{
   int i;
   LogFloat bkgdp = LZERO,x;
   Observation *obs = pri->obs;
   BGConfRec *bcp = &(pri->confinfo);

   if (++bcp->frame != pri->frame)
      HError(8594,"UpdateBGConfRec: frame sync error last=%d, new=%d",bcp->frame,pri->frame);

   if (bcp->hmm != NULL) {  /* check bg model */
      for(i=2;i<bcp->hmm->numStates;i++) {
         x=OutP(obs, bcp->hmm, i);
         if (x>bkgdp) bkgdp=x;
      }
      bcp->bgdlike[bcp->idx] = bkgdp;
   }else
      bcp->bgdlike[bcp->idx] = bcp->averp;
   bcp->maxlike[bcp->idx] = bcp->bestp;

   /* update the index for next time */
   bcp->idx++;
   if (bcp->idx == bcp->size) bcp->idx = 0;

   /* tracing */
   if ((trace&T_CON2) && (pri->frame >= traceDelay)){
      printf("BGProb at frame %d: bestp=%.1f averp=%.1f",pri->frame,bcp->bestp,bcp->averp);
      if (bkgdp>LZERO) printf(" bkgd=%.1f",bkgdp); else printf(" bkgd=LZERO");
      printf("\n");
   }

   bcp->bestp = LZERO;  /* updated during token processing */
}

static void LatFromPaths(PRecInfo *pri, Ring *ring,int *ln,Lattice *lat)
{
   LNode *ne,*ns;
   LArc *la;
   Word nullwordId;
   Path tmp,*pth;
   MLink ml;
   LabId labid,labpr;
   char buf[80];
   int i,frame;
   double prlk,dur,like,wp;

   /* do we need to check for the mark/if node seen before?*/

   nullwordId = GetWord(lat->voc,GetLabId("!NULL",FALSE),FALSE);

   ne=lat->lnodes-ring->mark;
   ne->time=ring->frame*lat->framedur;
   if (ring->node->info.pron != NULL)
      ne->word=ring->node->info.pron->word;
   else
      ne->word=nullwordId;
   ne->tag=ring->node->tag;
   if (ring->node->info.pron != NULL)
      ne->v=ring->node->info.pron->pnum;
   else
      ne->v=1;

   /* Is this correct or do we have to seperate each path??? */
   ne->score=ring->head->like;

   /* go through each link in the ring */
   for(pth=ring->head, i=0; pth!=NULL && i<ring->nPaths;
	 pth=pth->next, i++){
      la=lat->larcs+(*ln)++;
      if (pth->prev){          /* should be prev->head->like??? */
         ns=lat->lnodes-pth->prev->owner->mark,prlk=pth->prev->like;
      } else {
         ns=lat->lnodes,prlk=0.0;
      }
      la->start=ns;la->end=ne;
      if (ne->word==NULL || ne->word==nullwordId) /* no word or NULL node */
         wp=0.0;                   /* No penalty for current word */
      else wp=pri->wordPen;       /* Inc penalty for current word */
      la->aclike=pth->like-prlk-pth->lm-wp;
      /* get LM vs NG scale right??? */
      if (ring->node->info.pron != NULL) {
         la->aclike-=ring->node->info.pron->prob*pri->pronScale;
         la->prlike=ring->node->info.pron->prob;
      }
      else
         la->prlike=0.0;
      la->lmlike=(lat->lmscale<=0)?0:pth->lm/lat->lmscale;
      la->score=pth->like;
      la->farc=ns->foll;la->parc=ne->pred;
      ns->foll=ne->pred=la;

      if (pth->prev!=NULL && ns->word==NULL)
         LatFromPaths(pri, pth->prev->owner,ln,lat);
   }
}

/* Number/count nodes (in path->usage field) and count links */
static void MarkPaths(Ring *ring,int *nn,int *nl)
{
  Path *path;
  int i,j;

  if(ring->mark>=0) {
    ring->mark=-(*nn)++;   /*number node */
     for (path=ring->head, i=0; path!=NULL && i<ring->nPaths;
	 path=path->next, i++) {  /* iterate through ring */
      (*nl)++;
      if (path->prev) MarkPaths(path->prev->owner,nn,nl); /* backtrace */
    }
  }
}

static Lattice *CreateLattice(PRecInfo *pri, MemHeap *heap,TokenSet *res,HTime frameDur)
{
   Lattice *lat;
   int nn,nl,ln,i;
   NetNode node;

   nn=1;nl=0;ln=0;

   MarkPaths(res->tok.path->owner,&nn,&nl);

   if(trace&T_TOP)
     printf("Num nodes %d, num paths %d\n", nn, nl);
   lat=NewLattice(heap,nn,nl);
   lat->voc=pri->net->vocab;
   lat->acscale=1.0;
   lat->lmscale=pri->ngScale;
   lat->wdpenalty=pri->wordPen;
   lat->prscale=pri->pronScale;
   lat->framedur=frameDur/1.0E+7;

   lat->lnodes[0].time=0.0; lat->lnodes[0].word=NULL;
   lat->lnodes[0].tag=NULL;
   lat->lnodes[0].score=0.0;

   LatFromPaths(pri,res->tok.path->owner,&ln,lat);

#ifdef SANITY
   if (ln!=nl)
      HError(8522,"CreateLattice: Size mismatch (nl (%d) != ln (%d))",nl,ln);
#endif

   return(lat);
}


/* EXPORT->GetConfidence: Return the confidence score of word spanning
frames st & en inclusive with total acoustic log likelihood a.
The confidence is scaled by defConfScale and offset by defConfOffset
such that the former increases the slope and the latter shifts the
threshold.  Note the st and en index base is assumed to be 1.
Value returned is in range 0 to 1.
Return -1 if confidence calculation cannot be completed */
float GetConfidence(PRecInfo *pri, int st, int en, LogFloat a, char * word)
{
   float rawconf,conf,bg,bs,dur;
   float z,zmax,zmin,s;
   int avail,i,j;
   BGConfRec *bcp = &(pri->confinfo);

   /* First check range is valid */
   avail = (bcp->frame>=bcp->size)?bcp->size:bcp->frame;
   if (en-st < 0 ) return -1.0;
   if (st<bcp->frame-avail+1) return -1.0;
   if (en>bcp->frame) return -1.0;
   dur = en-st+1.0;
   /* sum likelihood values over duration of word */
   j = (st-1) % bcp->size;
   for (i=0,bg=0.0,bs=0.0; i<=(en-st); i++){
      bg += bcp->bgdlike[j];
      bs += bcp->maxlike[j++];
      if (j==bcp->size) j=0;
   }
   /* compute average scores */
   a /= dur; bg = bg/dur;  bs = bs/dur;
   /* compute raw confidence in range -1 to 1 */
   rawconf = a - (bs+bg)/2;
   /* compute scaled confidence in range -1 to 1 */
   z = defConfScale*rawconf-defConfOffset;
   conf = exp(z)/ (exp(z) + exp(-z));
   /* rescale to 0 to 1 range */
   if (conf < MIN_CONF) conf = MIN_CONF;
   if (conf > MAX_CONF) conf = MAX_CONF;
   if ((trace&T_CON1) && (pri->frame >= traceDelay)){
      printf("Conf: %10s:[%3d->%3d] %3.2f(%4.2f) bs=%.1f, a=%.1f, bg=%.1f %c\n",
         word,st,en,conf,rawconf,bs,a,bg,(bcp->hmm != NULL)?'*':' ');
   }
   return conf;
}

/* --------------------------- TraceBack Routines ----------------- */

/* SetPPScales: copy scales into pp rec for info */
static void SetPPScales(PRecInfo *pri, PartialPath *pp)
{
   pp->lmScale = pri->lmScale;
   pp->ngScale = pri->ngScale;
   pp->wordPen = pri->wordPen;
   pp->pronScale = pri->pronScale;
}

/* EXPORT->DoTraceBack: of paths from active insts, return disambiguated tail if any */
PartialPath DoTraceBack(PRecInfo *pri)
{
   Path *p;
   PartialPath pp;
   NetInst *ni;
   int i,k,n,nPaths=0;
   TokenSet *t;
   NetNode *node;

   /* first scan all active paths and reset reference count trbkCount */
   for (ni=pri->head.link,i=1; ni!=NULL && i<=pri->nact; ni=ni->link,i++) {
      node = ni->node;
      n = (node_hmm(node))?node->info.hmm->numStates-1:1;
      for (k=1,t=ni->state; k<=n; k++,t++) {
         p = t->tok.path;
         if (p!=NULL) ++nPaths;   /* count total active paths */
         while (p!=NULL && p->trbkCount>=0) {
            p->trbkCount = 0; p = p->prev;
         }
      }
   }

   /* second scan all active paths, update count */
   for (ni=pri->head.link,i=1; ni!=NULL && i<=pri->nact; ni=ni->link,i++) {
      node = ni->node;
      n = (node_hmm(node))?node->info.hmm->numStates-1:1;
      for (k=1,t=ni->state; k<=n; k++,t++) {
         p = t->tok.path;
         while (p!=NULL && p->trbkCount>=0) {
            ++p->trbkCount;  p = p->prev;
         }
      }
   }

   /* now scan most likely path and see if any part is resolved */
   pp.node = NULL; pp.path = NULL; pp.n = 0;
   for (p = pri->genMaxTok.path; p!=NULL && p->trbkCount>=0
      && p->trbkCount<nPaths; p=p->prev);
   if (p!=NULL && p->trbkCount>=0){  /* resolved path found */
      pp.path = p;
      while (p!=NULL && p->trbkCount>=0) {
         p->trbkCount = -1; ++pp.n;
         p = p->prev;
      }
      pp.startFrame = (p==NULL)?0:p->owner->frame;
      pp.startLike  = (p==NULL)?0.0:p->like;
   }
   SetPPScales(pri,&pp);
   return pp;
}

/* EXPORT->CurrentBestPath: current best path from genMaxNode/genMaxTok.path */
PartialPath CurrentBestPath(PRecInfo *pri)
{
   Path *p;
   PartialPath pp;

   pp.n = 0; pp.path = NULL; pp.node = pri->genMaxNode;
   /* make sure that at least one word has been recognised */
   if (pp.node == NULL) return pp;

   pp.startFrame = 0; pp.startLike = 0.0;
   /* now search back thru path history */
   pp.path = pri->genMaxTok.path;
   for (p = pp.path; p!=NULL; p=p->prev) ++pp.n;
   SetPPScales(pri,&pp);
   return pp;
}

/* EXPORT->FinalBestPath:  return the best path on completion of recognition */
PartialPath FinalBestPath(PRecInfo *pri)
{
   PartialPath pp;
   Path *p;

   pp.node = NULL; pp.path = NULL; pp.n = 0;
   pp.startFrame=0; pp.startLike = 0.0;
   if (pri->net->final.inst!=NULL){
      if (pri->net->final.inst->exit->tok.path!=NULL){
         pp.path = pri->net->final.inst->exit->tok.path;
         for (p = pp.path; p!=NULL; p=p->prev) ++pp.n;
      }
   }
   SetPPScales(pri,&pp);
   return pp;
}



/* PrintPartialPath: either just words or in detail */
void PrintPartialPath(PartialPath pp, Boolean inDetail)
{
   int i;
   Path *p;
   Pron pron;
   char s1[1000],s2[1000],s3[100];

   s1[0]='\0';
   /* add final (current) node if any */
   if(pp.node!=NULL) {
     if (pp.node->wordset !=NULL){
       if (inDetail) {
         sprintf(s1,"%2d.      %12s",pp.n+1,pp.node->wordset->name);
       } else {
         sprintf(s1,"%s",pp.node->wordset->name);
       }
     }
   }
   /* trace down the path */
   for (p = pp.path,i=pp.n; p!=NULL && i>0; p=p->prev,i--){
      strcpy(s2,s1); pron = p->owner->node->info.pron;
      if (p->owner->node->tag){
         sprintf(s3,"%s{%s}",pron==NULL?"!NULL":pron->word->wordName->name,p->owner->node->tag);
      }else{
         sprintf(s3,"%s",pron==NULL?"!NULL":pron->word->wordName->name);
      }
      if (inDetail){
         sprintf(s1,"%2d. %4d %12s %10.2f %7.3f\n",i,p->owner->frame,s3,p->like,p->lm);
      }else{
         sprintf(s1,"%s ",s3);
      }
      strcat(s1,s2);
   }
   printf("\n%s\n",s1);
}

/* --------------------- EXPORTED FUNCTIONS --------------------- */

/* EXPORT->InitPRecInfo: initialise ready for recognition */
PRecInfo *InitPRecInfo(PSetInfo *psi,int nToks)
{
   PRecInfo *pri;
   PreComp *pre;
   int i,n;
   char name[80];
   static int prid=0;

   pri=(PRecInfo*) New(&gcheap,sizeof(PRecInfo));
   sprintf(name,"PRI-%d Heap",prid++);
   CreateHeap(&pri->heap,name,MSTAK,1,1.0,1000,8000);
   pri->prid=prid;

#ifdef SANITY
   pri->ipos=0;
   pri->start_inst=NULL;
#endif

   /* Reset readable parameters */
   pri->maxBeam=0;
   pri->genBeam=-LZERO;
   pri->wordBeam=-LZERO;
   pri->nBeam=-LZERO;
   pri->tmBeam=LZERO;
   pri->pCollThresh= 5000;

   /* Set up private parameters */
   pri->qsn=0;pri->qsa=NULL;
   pri->psi=NULL;
   pri->net=NULL;
   pri->lmScale=1.0;
   pri->ngScale=0.0;
   pri->wordPen=0.0;

   if (nToks<=1) pri->nToks=0;
   else if (nToks<=MAX_TOKS) pri->nToks=nToks;
   else pri->nToks=MAX_TOKS;
   for (i=0; i<TSM_HASH; i++) pri->keyhash[i] = 0;

   /* SetUp heaps for recognition */

   /* Model set dependent */
   pri->psi=psi;
   for(i=1,pre=psi->sPre+1;i<=psi->nsp;i++,pre++) pre->id=-1;
   for(i=1,pre=psi->mPre+1;i<=psi->nmp;i++,pre++) pre->id=-1;

   pri->stHeap=(MemHeap *) New(&pri->heap,pri->psi->stHeapNum*sizeof(MemHeap));
   for (n=1;n<=pri->psi->max;n++) {
      if (pri->psi->stHeapIdx[n]>=0) {
         sprintf(name,"State Heap: numStates=%d",n);
         CreateHeap(pri->stHeap+pri->psi->stHeapIdx[n],name,
            MHEAP,sizeof(TokenSet)*n,1.0,100,1600);
      }
   }

   /* nTok dependent */
   if (pri->nToks>1)
      CreateHeap(&pri->rTokHeap,"RelToken Heap",
      MHEAP,sizeof(RelToken)*pri->nToks,1.0,200,1600);
   /* Non dependent */
   CreateHeap(&pri->instHeap,"NetInst Heap",
      MHEAP,sizeof(NetInst),1.0,200,1600);
   CreateHeap(&pri->pathHeap,"Path Heap",
      MHEAP,sizeof(Path),1.0,200,1600);
   CreateHeap(&pri->ringHeap,"Ring Heap",
      MHEAP,sizeof(Ring),1.0,200,1600);

   /* Now set up instances */

   pri->head.node=pri->tail.node=NULL;
   pri->head.state=pri->tail.state=NULL;
   pri->head.exit=pri->tail.exit=NULL;
   pri->head.max=pri->tail.max=LZERO;
   pri->head.knil=pri->tail.link=NULL;
   pri->head.link=&pri->tail;pri->tail.knil=&pri->head;


   /* initialise background model */
   InitBGConfRec(&pri->confinfo,pri->psi->hset);
   return(pri);
}

/* EXPORT->DeleteVRecInfo: Finished with this recogniser */
void DeletePRecInfo(PRecInfo *pri)
{
   int i;

   for (i=0;i<pri->psi->stHeapNum;i++)
      DeleteHeap(pri->stHeap+i);
   if (pri->nToks>1)
      DeleteHeap(&pri->rTokHeap);
   DeleteHeap(&pri->instHeap);
   DeleteHeap(&pri->pathHeap);
   DeleteHeap(&pri->ringHeap);
   DeleteHeap(&pri->heap);
   Dispose(&gcheap,pri);
}

/* EXPORT->StartRecognition: reset rec ready to process observations */
void StartRecognition(PRecInfo *pri,Network *net, float lmScale,
                      LogFloat wordPen, float pScale, float ngScale, LModel *lm)

{
   NetNode *node;
   NetInst *inst,*next;
   PreComp *pre;
   int i;

   if (pri==NULL)
      HError(8570,"StartRecognition: Private rec info is NULL");
   pri->noTokenSurvived=TRUE;
   pri->net=net;
   pri->lmScale=lmScale; pri->ngScale=ngScale;
   pri->wordPen=wordPen; pri->pronScale=pScale;

   /* Reset the Background Confidence Record info */
   ResetBGConfRec(&pri->confinfo);

   /* Store the language model if any */
   pri->lm = lm;

   /* Initialise the network and instances ready for first frame */
   for (node=pri->net->chain;node!=NULL;node=node->chain) node->inst=NULL;
   pri->net->final.inst=pri->net->initial.inst=NULL;

   /* Invalidate all precomputed state and mixture probs */
   for(i=1,pre=pri->psi->sPre+1;i<=pri->psi->nsp;i++,pre++) pre->id=-1;
   for(i=1,pre=pri->psi->mPre+1;i<=pri->psi->nmp;i++,pre++) pre->id=-1;

   /* Reset frame counter, & cumulative and current active model counters */
   pri->frame=0; pri->tact=pri->nact=0;

   /* Attach an inst to initial net node, with like=1.0 and null path */
   AttachInst(pri,&pri->net->initial);
   inst=pri->net->initial.inst;
   inst->state->tok.like=inst->max=0.0;
   inst->state->tok.lm=0.0;
   inst->state->tok.ngPending=FALSE;
   inst->state->tok.path=NULL;
   inst->state->n=((pri->nToks>1)?1:0);

   /* Set General Max Node and Word Max Nodes to NULL */
   pri->genMaxNode=pri->wordMaxNode=NULL;
   pri->genMaxTok=pri->wordMaxTok=null_token;

   /* Set beam thresholds close to floor */
   pri->wordThresh=pri->genThresh=pri->nThresh=LSMALL;

   /* Clear path rings */
   InitPathRings(pri);

   /* Scan all existing instances and detach them */
   for (inst=pri->head.link;inst!=NULL && inst->node!=NULL;inst=next){
      if (inst->max<pri->genThresh) {
         next=inst->link;
         DetachInst(pri,inst->node);
      } else {
         pri->nxtInst=inst;
         StepInst2(pri,inst->node);
         next=pri->nxtInst->link;
      }
   }
}

/* EXPORT->ProcessObservation: move forward recognition one frame */
void ProcessObservation(PRecInfo *pri,Observation *obs, int id,AdaptXForm *xform)
{
   NetInst *inst,*next;
   int j,count;
   float thresh;
   char buf[100];
   PartialPath pp;

   pri->inXForm = xform;
   if (pri==NULL)
      HError(8570,"ProcessObservation: Private recognition info not initialised");
   if (pri->net==NULL)
      HError(8570,"ProcessObservation: Recognition network is null");

   pri->psi->sBuf[1].n=((pri->nToks>1)?1:0); /* Needed every observation */
   pri->frame++;
   pri->obs=obs;
   if (id<0) pri->obid=(pri->prid<<20)+pri->frame;
   else pri->obid=id;

   if (obs->swidth[0]!=pri->psi->hset->swidth[0])
      HError(8571,"ProcessObservation: incompatible number of streams (%d vs %d)",
             obs->swidth[0],pri->psi->hset->swidth[0]);
   if (pri->psi->mixShared){
      for (j=1;j<=obs->swidth[0];j++){
         if (VectorSize(obs->fv[j])!=pri->psi->hset->swidth[j])
            HError(8571,"ProcessObservatio: incompatible stream widths for %d (%d vs %d)",
            j,VectorSize(obs->fv[j]),pri->psi->hset->swidth[j]);
      }
   }

   /* Max model pruning is done initially in a separate pass */
   if (pri->maxBeam>0 && pri->nact>pri->maxBeam) {
	  count = 0;
	  printf("%d. beam=%d nact=%d  ",pri->frame,pri->maxBeam,pri->nact);
      if (pri->nact>pri->qsn) {
         if (pri->qsn>0)
            Dispose(&pri->heap,pri->qsa);
         pri->qsn=(pri->nact*3)/2;
         pri->qsa=(LogFloat*) New(&pri->heap,pri->qsn*sizeof(LogFloat));
      }
      for (inst=pri->head.link,j=0;inst!=NULL;inst=inst->link,j++)
         pri->qsa[j]=inst->max;
      if (j>=pri->maxBeam) {
         qcksrtM(pri->qsa,0,j-1,pri->maxBeam);
         thresh=pri->qsa[pri->maxBeam];
         if (thresh>LSMALL)
            for (inst=pri->head.link;inst->link!=NULL;inst=next) {
               next=inst->link;
			   if (inst->max<thresh) {
                  DetachInst(pri,inst->node); ++count;
			   }
            }
      }
	  printf("   %d detached\n",count);
   }
   if (pri->psi->hset->hsKind==TIEDHS)
      PrecomputeTMix(pri->psi->hset,obs,pri->tmBeam,0);

   /* Pass 1 must calculate top of all beams - inc word end !! */
   pri->genMaxTok = pri->wordMaxTok = null_token;
   pri->genMaxNode = pri->wordMaxNode = NULL;
   for (inst=pri->head.link,j=0; inst!=NULL; inst=inst->link,j++){
      if (inst->node) StepInst1(pri,inst->node);
   }

   /* Not changing beam width for max model pruning */
   pri->wordThresh = pri->wordMaxTok.like - pri->wordBeam;
   if (pri->wordThresh<LSMALL) pri->wordThresh=LSMALL;
   pri->genThresh = pri->genMaxTok.like - pri->genBeam;
   if (pri->genThresh<LSMALL) pri->genThresh=LSMALL;
   if (pri->nToks>1) {
      pri->nThresh=pri->genMaxTok.like-pri->nBeam;
      if (pri->nThresh<LSMALL/2) pri->nThresh=LSMALL/2;
   }

   /* Pass 2 Performs external token propagation and pruning */
   for (inst=pri->head.link,j=0;inst!=NULL && inst->node!=NULL;inst=next,j++) {
      if (inst->max<pri->genThresh) {
         next=inst->link; DetachInst(pri,inst->node);
      } else {
         pri->nxtInst=inst;
         StepInst2(pri,inst->node);
         next=pri->nxtInst->link;
      }
   }

   if ((pri->nusedPaths - pri->nusedLastCollect) > pri->pCollThresh)
      CollectPaths(pri);

   pri->tact+=pri->nact;
   UpdateBGConfRec(pri);

   if ((trace&T_FRT) && (pri->frame >= traceDelay)){
      sprintf(buf,"Frame %d",pri->frame);
      bar(buf); printf("nact=%d",pri->nact);
      prl(" wordMax",pri->wordMaxTok.like);prl(" genMax",pri->genMaxTok.like);
      pp = CurrentBestPath(pri);
      if (pp.n>0) PrintPartialPath(pp,(trace&T_DPP));
      if ((trace&T_INST) && (pri->frame >= traceDelay)) TraceInsts(pri);
   }
}


/* EXPORT->CreateLatticeFromOutput: */
Lattice *CreateLatticeFromOutput(PRecInfo *pri, PartialPath pp, MemHeap *heap, HTime frameDur)
{
  Lattice *lat;
  NetInst *inst;
  TokenSet dummy;
  RelToken rtok[1];
  int i;


  if (pri==NULL)
    HError(8570,"CompleteRecognition: Visible recognition info not initialised");
  if (pri->net==NULL)
    HError(8570,"CompleteRecognition: Recognition not started");
  if (pri->frame==0)
    HError(-8570,"CompleteRecognition: No observations processed");

  /* vri->frameDur=frameDur;(/

  /* Should delay this until we have freed everything that we can */
  lat=NULL;
  if (heap!=NULL) {
     pri->noTokenSurvived=TRUE;
     if (pri->net->final.inst!=NULL)
       if (pri->net->final.inst->exit->tok.path!=NULL)
	 lat=CreateLattice(pri,heap,pri->net->final.inst->exit,frameDur),
	   pri->noTokenSurvived=FALSE;

     if (lat==NULL && forceOutput) {
       dummy.n=((pri->nToks>1)?1:0);
       dummy.tok=pri->genMaxTok;
       dummy.set=rtok;
       dummy.set[0].like=0.0;
       dummy.set[0].path=dummy.tok.path;
       dummy.set[0].lm=dummy.tok.lm;
       lat=CreateLattice(pri,heap,&dummy,frameDur);
     }
  }
  return(lat);
}

/* EXPORT->CompleteRecognition: Free used data */
void CompleteRecognition(PRecInfo *pri)
{
   Lattice *lat;
   NetInst *inst;
   int i;

   if (pri==NULL)
      HError(8570,"CompleteRecognition: Private recognition info not initialised");
   if (pri->net==NULL)
      HError(8570,"CompleteRecognition: Recognition network is null");
   if (pri->frame==0)
     HError(-8570,"CompleteRecognition: No observations processed");

   /* Now dispose of everything apart from the answer */
   for (inst=pri->head.link;inst!=NULL;inst=inst->link){
      if (inst->node) inst->node->inst=NULL;
   }

   /* Remove everything from active lists */
   pri->head.link=&pri->tail;pri->tail.knil=&pri->head;
   pri->nact=pri->frame=0;
   pri->frame=0;
   pri->nact=0;
   pri->genMaxNode=NULL;
   pri->wordMaxNode=NULL;
   pri->genMaxTok=null_token;
   pri->wordMaxTok=null_token;

   if (pri->nToks>1)
      ResetHeap(&pri->rTokHeap);
   ResetHeap(&pri->instHeap);
   for (i=0;i<pri->psi->stHeapNum;i++)
      ResetHeap(pri->stHeap+i);

   /* printf("Total key hash hits = %d\n",tsmhits);*/
}

/* EXPORT->SetPruningLevels: Set pruning levels for following frames */
void SetPruningLevels(PRecInfo *pri,int maxBeam,LogFloat genBeam,
                      LogFloat wordBeam,LogFloat nBeam,LogFloat tmBeam)
{
   pri->maxBeam=maxBeam;
   pri->genBeam=genBeam;
   pri->wordBeam=wordBeam;
   pri->nBeam=nBeam;
   pri->tmBeam=tmBeam;
   if (trace&T_TOP){
      printf("Pruning[%d toks]: maxBeam=%d, genBeam=%.1f, wordBeam=%.1f, nBeam=%.1f\n",
	      pri->nToks,maxBeam,genBeam,wordBeam,nBeam);
   }
}

/* Lattice output routines.  Note lattices need to be sorted before output */

typedef struct nbestentry NBestEntry;

struct nbestentry {
   NBestEntry *link;
   NBestEntry *knil;
   NBestEntry *prev;

   double score;
   double like;
   LNode *lnode;
   LArc *larc;
};

static void MarkBack(LNode *ln,int *nn)
{
   LArc *la;

   ln->n=-2;
   for (la=ln->pred;la!=NULL;la=la->parc)
      if (la->start->n==-1) MarkBack(la->start,nn);
   ln->n=(*nn)++;
}

static Boolean WordMatch(NBestEntry *cmp,NBestEntry *ans)
{
   if (cmp==ans) return(TRUE);
   else if (cmp==NULL || ans==NULL) return(FALSE);
   else if (cmp->larc->end->word!=ans->larc->end->word) return(FALSE);
   else return(WordMatch(cmp->prev,ans->prev));
}

void FormatTranscription(Transcription *trans,HTime frameDur,
                         Boolean states,Boolean models,Boolean triStrip,
                         Boolean normScores,Boolean killScores,
                         Boolean centreTimes,Boolean killTimes,
                         Boolean killWords,Boolean killModels)
{
   LabList *ll;
   LLink lab;
   HTime end;
   char buf[MAXSTRLEN],*p,tail[64];
   int lev,j,frames;

   if (killScores) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ) {
            lab->score=0.0;
            for (j=1;j<=ll->maxAuxLab;j++)
               lab->auxScore[j]=0.0;
         }
      }
   }
   if (triStrip) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ) {
            if (states && !models) {
               strcpy(buf,lab->labid->name);
               if ((p=strrchr(buf,'['))!=NULL) {
                  strcpy(tail,p);
                  *p=0;
               }
               else
                  *tail=0;
               TriStrip(buf); strcat(buf,tail);
               lab->labid=GetLabId(buf,TRUE);
            }
            else {
               strcpy(buf,lab->labid->name);
               TriStrip(buf); lab->labid=GetLabId(buf,TRUE);
            }
            for (j=1;j<=ll->maxAuxLab;j++) {
               if (lab->auxLab[j]==NULL) continue;
               strcpy(buf,lab->auxLab[j]->name);
               TriStrip(buf); lab->auxLab[j]=GetLabId(buf,TRUE);
            }
         }
      }
   }
   if (normScores) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ) {
            frames=(int)floor((lab->end-lab->start)/frameDur + 0.4);
            if (frames==0) lab->score=0.0;
            else lab->score=lab->score/frames;
            if (states && models && ll->maxAuxLab>0 && lab->auxLab[1]!=NULL) {
               end=AuxLabEndTime(lab,1);
               frames=(int)floor((end-lab->start)/frameDur + 0.4);
               if (frames==0) lab->auxScore[1]=0.0;
               else lab->auxScore[1]=lab->auxScore[1]/frames;
            }
         }
      }
   }
   if (killTimes) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ) {
            lab->start=lab->end=-1.0;
         }
      }
   }
   if (centreTimes) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ) {
            lab->start+=frameDur/2;
            lab->end-=frameDur/2;
         }
      }
   }
   if (killWords) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         if (ll->maxAuxLab>0)
            for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ)
               lab->auxLab[ll->maxAuxLab]=NULL;
      }
   }
   if (killModels && models && states) {
      for (lev=1;lev<=trans->numLists;lev++) {
         ll=GetLabelList(trans,lev);
         if (ll->maxAuxLab==2)
            for(lab=ll->head->succ;lab->succ!=NULL;lab=lab->succ) {
               lab->auxLab[1]=lab->auxLab[2];
               lab->auxScore[1]=lab->auxScore[2];
               lab->auxLab[2]=NULL;
            }
      }
   }
}

/* ------------------------------------------------------------- */
/*                 NBest Fibonacci Heap stuff                    */
/* ------------------------------------------------------------- */

static int FibLatMarkStartsEnds(Lattice *lat)
{
   LArc *larc;
   LNode *ln;
   int i,n;

   /* First initialise everything */
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) ln->n=0;
   /* mark start and end */
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
     if(ln->pred==NARC)
       ln->n=LNODE_START;
     else if (ln->foll==NARC)
       ln->n=LNODE_END;
     else
       ln->n=0;
   }

   return(1);
}

/* EXPORT->TransFromLattice: Generate transcription from lattice */
Transcription *TransFromLattice(MemHeap *heap, Lattice *lat,int max)
{
   NBNodeHeap *nodeheap;
   NBNodeEntry *neh;
   LatNodeHeapInfo *fhi;
   LatNodeEntryInfo *info;
   LArc ***lalts;   /* all alternates */
   LArc **larcs;
   LNode *ln;

   Transcription *trans;
   Pron pron;
   LLink lab,where;
   LabList *ll;
   LabId model;
   LabId lablm, labac, labtag;
   float auxScores[2], lmscore, acscore;
   int nAux;
   Word word, nullWord;
   int i,j,cnt,nlabs;

   nullWord=GetWord(lat->voc, GetLabId("!NULL", FALSE), FALSE);

   FibLatMarkStartsEnds(lat);
   nodeheap=PrepareLatNBest(max,EQUIV_WORD,lat,TRUE);
   fhi=(LatNodeHeapInfo*)nodeheap->info;
   ResetHeap(heap);  /*???*/

   trans=CreateTranscription(heap);
   lalts=New(heap,sizeof(LArc)*(max));
   cnt=0;
   while (cnt<max) {
      neh=LatNextBestNBEntry(nodeheap);
      if (neh==NULL) break;

      for (nlabs=0,info=neh->info;info!=NULL && info->prev!=NULL;
	   info=(LatNodeEntryInfo*)info->prev->info)
	 if (info->la!=NULL) nlabs++;

      larcs=New(heap,sizeof(LArc)*(nlabs+1));  /*Ends in NULL */
      if (lalts!=NULL)
	 lalts[cnt]=larcs;
      cnt++;

      for (i=nlabs,larcs[nlabs]=NULL,info=neh->info;info!=NULL && info->prev!=NULL;
	   info=(LatNodeEntryInfo*)info->prev->info)
	 if (info->la!=NULL)
	    larcs[--i]=info->la;
      {
	 LArc *la;
	 double like,dur,aclike,lmlike,prlike, score;

	 ll=CreateLabelList(heap,LAB_MISC);
	 aclike=lmlike=prlike=0.0;
	 for (i=0;i<nlabs;i++) {
	    la=larcs[nlabs-i-1];
	    for (pron=la->end->word->pron;pron!=NULL;pron=pron->next)
	      if (pron->pnum==la->end->v) break;
	    if (pron==NULL || pron->outSym==NULL || pron->outSym->name[0]==0)
	      continue;
	    lab=CreateLabel(heap,ll->maxAuxLab);
	    lab->labid=pron->outSym;
	    lab->score=LArcTotLike(lat,la);
	    lab->start=la->start->time*1.0E7;
	    lab->end=la->end->time*1.0E7;
	    lab->succ=ll->head->succ;lab->pred=ll->head;
	    lab->succ->pred=lab->pred->succ=lab;

	    labac=GetLabId("acscore",TRUE);
	    lablm=GetLabId("lmscore",TRUE);
	    lab->auxLab[LAB_LM]=lablm;
	    lab->auxLab[LAB_AC]=labac;

	    lab->auxScore[LAB_LM]=la->lmlike*lat->lmscale;
	    lab->auxScore[LAB_AC]=la->aclike;
	    if(trace&T_NGEN) {
	       if (la->end->word!=NULL)
		  if (la->end->word->nprons>1)
		     printf("%s%d ",la->end->word->wordName->name,la->end->v);
		  else
		     printf("%s ",la->end->word->wordName->name);
	    }
	    aclike+=la->aclike;
	    lmlike+=LArcTotLMLike(lat,la);
	    prlike+=la->prlike;
	 }
	 AddLabelList(ll,trans);
	 info=(LatNodeEntryInfo*)neh->info;
	 like=neh->score-fhi->like;
	 if(trace&T_NGEN) {
	    printf("%.3f ",like);
	    printf("A=%.2f L=%.2f P=%.2f\n",aclike,lmlike,prlike);
	 }
      }
     }
   /* renumber lat */
   /*
     for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
     ln->n=i+1;
     ln->sublat=NULL;
     } */

   if (cnt<max && lalts!=NULL) lalts[cnt]=NULL;  /* end null */

   DeleteNodeHeap(nodeheap);

   return(trans);
}

/* ------------------------ End of HRec.c ------------------------- */
