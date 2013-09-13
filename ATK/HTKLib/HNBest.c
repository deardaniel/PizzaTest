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
/*   File: HNBest.c   Core NBest functions (inc Fib Heap)      */
/* ----------------------------------------------------------- */

static char *hnbest_version = "!HVER!HNBest: 1.6.0 [SJY 01/06/07]";

/* Generic includes */

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
#include "HUtil.h"
#include "HDict.h"
#include "HLM.h"
#include "HNet.h"
#include "HRec.h"
#include "HNBest.h"

/* ----------------------------- Trace Flags ------------------------- */

#define T_GEN 0001         /* General tracing */

static int trace=0;
static ConfParam *cParm[MAXGLOBS];      /* config parameters */
static int nParm = 0;

/* --------------------------- Global Flags -------------------------- */

/* --------------------------- Initialisation ---------------------- */

/* EXPORT->InitNet: register module & set configuration parameters */
void InitNBest(void)
{
   Register(hnbest_version);
   nParm = GetConfig("HNBEST", TRUE, cParm, MAXGLOBS);
   if (nParm>0){
   }
}

static void Consolidate(NBNodeHeap *heap)
{
   int i,deg,d;
   NBNodeEntry *x,*w,*y,*t,*head;
   static NBNodeEntry *nhcA[MAX_HEAP_DEGREE];

   if (heap->head==NULL) return;

   for (i=heap->count,deg=1;i>0;deg++,i=i>>1);
   for (i=0;i<=deg;i++) nhcA[i]=NULL;

   head=heap->head; w=head->succ;
   while(w!=NULL) {
      x=w;d=x->degree;
      if (w==head) w=NULL; else w=w->succ;
      while((y=nhcA[d])!=NULL) {
         if (x->score<y->score) t=y,y=x,x=t;
         y->mark=0; x->degree++; y->parent=x;
         y->succ->pred=y->pred; y->pred->succ=y->succ;
         if (x->child!=NULL) {
            y->succ=x->child->succ; y->pred=x->child;
            y->pred->succ=y->succ->pred=y;
         } else {
            y->succ=y->pred=y;
            x->child=y;
         }
         nhcA[d++]=NULL;
      }
      nhcA[d]=x;
   }
   head=NULL; heap->length=0;
   for (i=0;i<deg;i++){
      x=nhcA[i];
      if (x != NULL) {
         if (head==NULL) {
            heap->length++; x->succ=x->pred=x;
            head=x;
         } else {
            heap->length++;
            x->succ=head->succ; x->pred=head;
            x->pred->succ=x->succ->pred=x;
            if (x->score>head->score) head=x;
         }
      }
   }
   heap->head=head;
}

static void HeapCut(NBNodeEntry *x,NBNodeHeap *heap)
{
   NBNodeEntry *y;

   y=x->parent; heap->length++;

   /* Remove from chain */
   x->succ->pred=x->pred;
   x->pred->succ=x->succ;
   if (y->child==x) {
      if (x->succ==x) y->child=NULL;
      else y->child=x->succ;
   }

   /* Add info NBNodeHeap list */
   x->succ=heap->head->succ; x->pred=heap->head;
   x->pred->succ=x->succ->pred=x;
   y->degree--; x->mark=0; x->parent=NULL;
   if (y->parent!=NULL) {
      if (!y->mark) y->mark=1;
      else HeapCut(y,heap);
   }
}

NBNodeEntry *GetNodeEntry(NBNodeHeap *nodeheap)
{
   NBNodeEntry *child,*best;

   best=nodeheap->head;
   if (best==NULL) return NULL;
   child = best->child;
   if (child != NULL) {
      do {
         child->parent=0; child=child->succ;
         nodeheap->length++;
      }
      while(child!=best->child);
      child->pred->succ=nodeheap->head->succ;
      nodeheap->head->succ->pred=child->pred;
      nodeheap->head->succ=child;
      child->pred=nodeheap->head;
   }
   best->succ->pred=best->pred;
   best->pred->succ=best->succ;
   nodeheap->count--; nodeheap->length--;
   if (best->succ==best){
      nodeheap->head=NULL;
   }else {
      nodeheap->head=best->succ;
      Consolidate(nodeheap);
   }
   best->child=NULL; best->parent=NULL;
   best->succ=NULL;  best->pred=NULL;
   best->status=neh_expanded;
   nodeheap->exps++;
   return best;
}

NBNodeEntry *NewNodeEntry(double score,NBNodeEntry *neh,NBNodeHeap *nodeheap)
{
  NBNodeEntry *newNode;

  /* Allocate */
  if (neh==NULL) {
     newNode=New(&nodeheap->nehHeap,0);
     if (nodeheap->isize>0) newNode->info = (Ptr)(newNode+1);
     else newNode->info=NULL;
  } else
     newNode=neh;
  /* Initialise */
  newNode->id=nodeheap->id++;
  newNode->degree=0; newNode->mark=0; newNode->status=neh_used;
  newNode->parent=NULL; newNode->child=NULL;
  newNode->score=score;
  if (score>-HUGE_VAL) {
     /* Add to head of list */
     if (nodeheap->head==NULL) {
        newNode->succ=newNode->pred=newNode;
        nodeheap->head=newNode;
     }
     else {
        newNode->succ=nodeheap->head->succ;
        newNode->pred=nodeheap->head;
        nodeheap->head->succ=newNode;
        newNode->succ->pred=newNode;
        if (newNode->score>nodeheap->head->score)
           nodeheap->head=newNode;
     }
     nodeheap->count++; nodeheap->length++;
  }
  return newNode;
}

void DeleteNodeEntry(NBNodeEntry *neh,NBNodeHeap *nodeheap)
{
   if (neh->parent!=NULL || neh->child!=NULL ||
       neh->succ!=NULL || neh->pred!=NULL)
      HError(9999,"DeleteNodeEntry: Deleting node before removal from heap");
   Dispose(&nodeheap->nehHeap,neh);
}

void ExpandNodeEntry(NBNodeEntry *ent,double score,NBNodeHeap *nodeheap)
{
   if (ent->status==neh_expanded)
      HError(9999,"ExpandNodeEntry: Node has already been expanded");
   if (ent->score>score) return;
   ent->score=score;
   if (ent->parent!=NULL && ent->score>ent->parent->score)
      HeapCut(ent,nodeheap);
   if (ent->score>nodeheap->head->score)
      nodeheap->head=ent;
}

NBNodeHeap *CreateNodeHeap(size_t nehSize,size_t fhiSize)
{
   NBNodeHeap *nodeheap;

   nodeheap=New(&gcheap,MRound(sizeof(NBNodeHeap))+MRound(fhiSize));
   CreateHeap(&nodeheap->nehHeap,"NodeHeap Entries", MHEAP,
              sizeof(NBNodeEntry)+nehSize, 0.0, 200, 4000);
   nodeheap->info=(Ptr)(((char*)nodeheap)+MRound(sizeof(NBNodeHeap)));
   nodeheap->isize=nehSize;
   nodeheap->count=nodeheap->exps=nodeheap->length=0;
   nodeheap->head=NULL;
   nodeheap->id=1;
   return nodeheap;
}

void InitNodeHeap(NBNodeHeap *nodeheap)
{
   ResetHeap(&nodeheap->nehHeap);
   nodeheap->count=nodeheap->exps=nodeheap->length=0;
   nodeheap->head=NULL;
   nodeheap->id=1;
}

void DeleteNodeHeap(NBNodeHeap *nodeheap)
{
   DeleteHeap(&nodeheap->nehHeap);
   Dispose(&gcheap,nodeheap);
}

/* -------- Lattice stuff ------------- */

static void MarkBack(LNode *ln,int *nn)
{
   LArc *la;

   ln->n=-2; /* GREY */
   for (la=ln->pred;la!=NULL;la=la->parc)
      if (la->start->n==-1) /* WHITE */
         MarkBack(la->start,nn);
   ln->n=(*nn)++; /* BLACK */
}

static void MarkNBack(int n,Lattice *lat,int *res,int *nn)
{
   LArc *la;
   int s;

   res[n]=-2; /* GREY */
   for (la=lat->lnodes[n].pred;la!=NULL;la=la->parc) {
      s=la->start-lat->lnodes;
      if (res[s]==-1) /* WHITE */
         MarkNBack(s,lat,res,nn);
   }
   res[n]=(*nn)++; /* BLACK */
}

static int ncmp(const void *v1,const void *v2)
{
   LNode *ln1,*ln2;
   ln1=(LNode*)v1;ln2=(LNode*)v2;
   return(ln1->n-ln2->n);
}

static int tcmp(const void *v1,const void *v2)
{
   LNode *ln1,*ln2;
   ln1=(LNode*)v1;ln2=(LNode*)v2;
   if (ln1->time>ln2->time) return(1);
   else if (ln1->time<ln2->time) return(-1);
   return(ln1->n-ln2->n);
}

static int npcmp(const void *v1,const void *v2)
{
   LNode *ln1,*ln2;
   ln1=*(LNode**)v1;ln2=*(LNode**)v2;
   return(ln1->n-ln2->n);
}

static int tpcmp(const void *v1,const void *v2)
{
   LNode *ln1,*ln2;
   ln1=*(LNode**)v1;ln2=*(LNode**)v2;
   if (ln1->time>ln2->time) return(1);
   else if (ln1->time<ln2->time) return(-1);
   return(ln1->n-ln2->n);
}

void SortLNodes(Lattice *lat,Boolean onTime,Boolean nOnly)
{
   LNode *ln,*st,*en,**lnArray;
   LArc *la;
   int i,n,ns,ne,*nNumber,*nStore;

   nNumber=New(&gstack,sizeof(int)*lat->nn);
   nStore=New(&gstack,sizeof(int)*lat->nn);

   /* Get topological order */
   for (i=0;i<lat->nn;i++) nNumber[i]=-1; /* WHITE */
   for (i=0,n=0;i<lat->nn;i++)
      if (nNumber[i]==-1) /* WHITE */
         MarkNBack(i,lat,nNumber,&n);

   /* Setup ln->n and remember previous value */
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
      nStore[nNumber[i]]=ln->n;
      ln->n=nNumber[i];
   }
   /* Check we can do this in time order */
   if (onTime){
      for (i=0,la=lat->larcs;i<lat->na;i++,la++){
         if (la->start->time>la->end->time) {
            onTime=FALSE;
            HError(9999,"SortLNodes: Topological not time sort [%d]",i);
            break;
         }
      }
      if (nOnly) {
         lnArray=New(&gstack,sizeof(LNode*)*lat->nn);
         for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) lnArray[i]=ln;
         if (onTime)
            /* Major sort on time - minor sort on topology */
            qsort(lnArray,lat->nn,sizeof(LNode*),tpcmp);
         else
            /* Only sort on topology */
            qsort(lnArray,lat->nn,sizeof(LNode*),npcmp);
         for (i=0;i<lat->nn;i++) lnArray[i]->n=i;
      } else {
         if (onTime)
            /* Major sort on time - minor sort on topology */
            qsort(lat->lnodes,lat->nn,sizeof(LNode),tcmp);
         else
            /* Only sort on topology */
            qsort(lat->lnodes,lat->nn,sizeof(LNode),ncmp);
         /* Fix references to nodes */
         for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
            ln->n=nStore[ln->n];
            for (la=ln->foll;la!=NULL;la=la->farc) la->start=ln;
            for (la=ln->pred;la!=NULL;la=la->parc) la->end=ln;
         }
      }
   }
   Dispose(&gstack,nNumber);
}

LogDouble LatLAhead(Lattice *lat,int dir)
{
   LNode *ln;
   LArc *la;
   double *nfwd,*nrev,like,score;
   Boolean inOrder,useNodeNumb;
   int i,j,n,ns,ne,st,en,*nOrder,*nNumber;

   /* la->score = total succ likelihood */
   for (i=0,inOrder=useNodeNumb=TRUE,la=lat->larcs;i<lat->na;i++,la++) {
      if (la->end<la->start) inOrder=FALSE;
      if (la->end->n<la->start->n) useNodeNumb=FALSE;
      la->score=LArcTotLike(lat,la);
   }
   /* If nodes are not already in order */
   if (!inOrder) {
      nOrder=New(&gstack,sizeof(int)*lat->nn);
      if (useNodeNumb) {
         /*  We can sometimes just use their ln->n fields to define order */
         for (i=0;i<lat->nn;i++) nOrder[i]=-1;
         for (i=0,ln=lat->lnodes;useNodeNumb && i<lat->nn;i++,ln++)
            if (ln->n>=0 && ln->n<lat->nn) nOrder[ln->n]=i;
            else useNodeNumb=FALSE;
            for (i=0;useNodeNumb && i<lat->nn;i++)
               if (nOrder[i]<0) useNodeNumb=FALSE;
      }
      if (!useNodeNumb) {
         /* Otherwise we have to do calculate order from scratch */
         nNumber=New(&gstack,sizeof(int)*lat->nn);
         for (i=0;i<lat->nn;i++) nNumber[i]=-1;
         for (i=0,n=0;i<lat->nn;i++)
            if (nNumber[i]==-1) MarkNBack(i,lat,nNumber,&n);
         for (i=0;i<lat->nn;i++) nOrder[nNumber[i]]=i;
      }
   }
   /* If dir==0 then we are generating relative scores so need a single */
   /* start and end plus some extra storage for both fwd and rev scores */
   if (dir==0) {
      for (i=0,ns=ne=0;i<lat->nn;i++,ln++) {
         j=(inOrder?i:nOrder[i]);
         ln=lat->lnodes+j;
         if (ln->foll==NULL) en=j,ne++;
         else if (ln->pred==NULL) st=j,ns++;
         ln->score=LZERO;
      }
      if (ns!=1 || ne!=1)
         HError(9999,"LatLAhead: Cannot score with multiple start[%d]/end[%d]",
         ns,ne);
      nfwd=New(&gstack,sizeof(double)*lat->nn);
      nrev=New(&gstack,sizeof(double)*lat->nn);
   } else {
      /* Otherwise initialise node scores to starting position */
      for (i=0;i<lat->nn;i++) {
         j=(inOrder?i:nOrder[i]);
         ln=lat->lnodes+j;
         if (dir<0 ? ln->foll==NULL : ln->pred==NULL) ln->score=0.0;
         else ln->score=LZERO;
      }
   }
   if (dir>=0) {
      if (dir==0) lat->lnodes[st].score=0.0;
      /* Now score the nodes forwards */
      for (i=0;i<lat->nn;i++) {
         j=(inOrder?i:nOrder[i]);
         ln=lat->lnodes+j;
         for (la=ln->foll;la!=NULL;la=la->farc) {
            score=ln->score+la->score;
            if (score>la->end->score) la->end->score=score;
         }
         if (dir==0) {
            nfwd[j]=ln->score;
            ln->score=LZERO; /* Initialise for forward scores */
         }
      }
      if (dir>0) like=lat->lnodes[lat->nn-1].score;
   }
   if (dir<=0) {
      /* Initialise end node score, set like = best path likelihood */
      if (dir==0) lat->lnodes[en].score=0.0,like=nfwd[en];
      /* Now score the nodes backwards */
      for (i=lat->nn-1;i>=0;i--) {
         j=(inOrder?i:nOrder[i]);
         ln=lat->lnodes+j;
         for (la=ln->pred;la!=NULL;la=la->parc) {
            score=ln->score+la->score;
            if (score>la->start->score) la->start->score=score;
         }
         if (dir==0) {
            nrev[j]=ln->score;
            ln->score=nfwd[j]+nrev[j]-like; /* Relative likelihood */
         }
      }
      if (dir<0) like=lat->lnodes[0].score;
   }
   if (dir==0) {
      /* Finally calculate relative arc likelihood */
      for (i=0,la=lat->larcs;i<lat->na;i++,la++) {
         st=la->start-lat->lnodes;
         en=la->end-lat->lnodes;
         la->score=nfwd[st]+nrev[en]+la->score-like;
      }
      Dispose(&gstack,nfwd);
   }
   if (!inOrder) Dispose(&gstack,nOrder);
   return like;
}

static void MarkLatPUsed(Lattice *lat,LNode *ln,float thresh,char *sArc)
{
   LArc *la;
   int n;

   if (!(ln->n&1)) {
      ln->n|=1;
      for (la=ln->pred;la!=NULL;la=la->parc)
         if (la->score>thresh || thresh==0.0) {
            n=LArcNumb(la,lat); sArc[n]|=1;
            MarkLatPUsed(lat,la->start,thresh,sArc);
         }
   }
}

static void MarkLatFUsed(Lattice *lat,LNode *ln,float thresh,char *sArc)
{
   LArc *la;
   int n;

   if (!(ln->n&2)) {
      ln->n|=2;
      for (la=ln->foll;la!=NULL;la=la->farc)
         if (la->score>thresh || thresh==0.0) {
            n=LArcNumb(la,lat); sArc[n]|=2;
            MarkLatFUsed(lat,la->end,thresh,sArc);
         }
   }
}

Lattice *PruneLattice(MemHeap *heap,Lattice *lat,float thresh)
{
   Lattice *newNode;
   LNode *ln,*nn;
   LArc *la,*na;
   int i,j,n,l,*nNode;
   char *sArc;

   nNode=(int*)New(&gstack,sizeof(int)*lat->nn);
   sArc=(char*)New(&gstack,sizeof(char)*lat->na);
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      nNode[i]=ln->n,ln->n=0;
   for (i=0;i<lat->na;i++) sArc[i]=0;

   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      if (ln->foll==NULL) MarkLatPUsed(lat,ln,thresh,sArc);
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      if (ln->pred==NULL) MarkLatFUsed(lat,ln,thresh,sArc);

   n=l=0;
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) if (ln->n==3) n++;
   for (i=0,l=0;i<lat->na;i++) if (sArc[i]==3) l++;

   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      if (((ln->pred==NULL || ln->foll==NULL) && ln->n!=3) ||
         ln->word==lat->voc->subLatWord) {
            i=-1;
	 break;
      }
   if (i<0)
      newNode=NULL;
   else {
      newNode=NewILattice(heap,n,l,lat);
      for (i=0,n=0,ln=lat->lnodes;i<lat->nn;i++,ln++){
         if (ln->n==3) {
            ln->n=n; nn=newNode->lnodes+n++; *nn=*ln;
            nn->n=i; nn->foll=nn->pred=NULL;
         }
         else ln->n=-1;
      }
      for (i=0,l=0,la=lat->larcs;i<lat->na;i++,la++){
         if (sArc[i]==3) {
            na=newNode->larcs+l++; *na=*la;
            if (la->nAlign>0 && la->lAlign!=NULL) {
               na->lAlign=New(heap,la->nAlign*sizeof(LAlign));
               for (j=0;j<la->nAlign;j++) na->lAlign[j]=la->lAlign[j];
            }
            na->start=newNode->lnodes+la->start->n;
            na->end=newNode->lnodes+la->end->n;
            na->farc=na->start->foll;na->parc=na->end->pred;
            na->start->foll=na->end->pred=na;
         }
      }
   }
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) ln->n=nNode[i];
   Dispose(&gstack,nNode);
   return newNode;
}


/* Reorder the farc chain for each node to ensure that the first */
/* one represents the most likely path */
void SortLArcs(Lattice *lat,int dir)
{
   LArc ta,*la,*na,*pa,*list;
   LNode *ln;
   int i;

   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
      if (dir>=0) {
         /* Sort foll list according to relative likelihood of arc */
         list=ln->foll;
         ta.farc=NULL;
         for (la=list;la!=NULL;la=na) {
            na=la->farc;
            for (pa=&ta;pa!=NULL;pa=pa->farc) {
               if (pa->farc==NULL || la->score>pa->farc->score) {
                  la->farc=pa->farc; pa->farc=la;
                  break;
               }
            }
         }
         ln->foll=ta.farc;
      }
      if (dir<=0) {
         /* Sort pred list according to relative likelihood of arc */
         list=ln->pred;
         ta.parc=NULL;
         for (la=list;la!=NULL;la=na) {
            na=la->parc;
            for (pa=&ta;pa!=NULL;pa=pa->parc) {
               if (pa->parc==NULL || la->score>pa->parc->score) {
                  la->parc=pa->parc; pa->parc=la;
                  break;
               }
            }
         }
         ln->pred=ta.parc;
      }
   }
}

/* --------- Lattice NBest stuff --------- */

static unsigned int RecursiveHash(unsigned int hv,Ptr ptr,int size)
{
   unsigned int val=(unsigned int)ptr;
   hv=((hv<<8)+(val&0xff))%size;
   hv=((hv<<8)+((val>>8)&0xff))%size;
   hv=((hv<<8)+((val>>16)&0xff))%size;
   hv=((hv<<8)+((val>>24)&0xff))%size;
   return(hv);
}

Ptr LNodeMatchPtr(LNode *ln,AnsMatchType type)
{
   Pron pron;

   /* Null words should really be skipped */
   if (ln->word==NULL) return(NULL);
   switch(type) {
    case EQUIV_WORD:
       /* Same word */
       return(ln->word);
    case EQUIV_PRON:
       /* Same pronunciation */
       for (pron=ln->word->pron;pron!=NULL;pron=pron->next)
          if (pron->pnum==ln->v) break;
       if (pron==NULL) return(ln->word);
       else return(pron);
    case EQUIV_OUTSYM:
       /* Same outsym */
       for (pron=ln->word->pron;pron!=NULL;pron=pron->next)
          if (pron->pnum==ln->v) break;
       if (pron==NULL || pron->outSym==NULL) return(ln->word);
       else return(pron->outSym);
   }
   /* Don't match anything */
   return(NULL);
}

static Boolean AnsRecMatch(LatNodeEntryInfo *a,LatNodeEntryInfo *b,
			   AnsMatchType type)
{
   Ptr aa,bb;

   /* Match is good */
   if (a==b) return(TRUE);
   /* If both are at beginning stop */
   if (lnType(a->ln)==LNODE_START && lnType(b->ln)==LNODE_START) return(TRUE);
   /* One or other at end so fail */
   if (a==NULL || b==NULL) return(FALSE);
   /* Get symbols to match */
   /* Rather than calls below use precomputer answers */
   /* aa=LNodeMatchPtr(a->ln,type);bb=LNodeMatchPtr(b->ln,type); */
   aa=a->ln->sublat; bb=b->ln->sublat;
   /* Force failure if NULL */
   if (aa==NULL || bb==NULL) return(FALSE);
   /* Otherwise need to check then recurse */
   if (aa==bb && a->hv==b->hv && a->prev!=NULL && b->prev!=NULL)
      return(AnsRecMatch((LatNodeEntryInfo*)a->prev->info,
			 (LatNodeEntryInfo*)b->prev->info,
			 type));
   return(FALSE);
}

static NBNodeEntry *FindAnsMatch(LatNodeEntryInfo *info,NBNodeHeap *nodeheap)
{
   LatNodeHeapInfo *nhi;
   NBNodeEntry *neh;

   nhi=(LatNodeHeapInfo*)nodeheap->info;
   if (nhi->type==EQUIV_NONE) return(NULL);
   for (neh=nhi->ansHashTab[info->hv];neh!=NULL;
	neh=((LatNodeEntryInfo*)neh->info)->hlink)
      if (((LatNodeEntryInfo*)neh->info)->ln==info->ln ||
	  (lnType(info->ln)==LNODE_VIABLE_EN &&
	   lnType(((LatNodeEntryInfo*)neh->info)->ln)==LNODE_VIABLE_EN))
	 if (AnsRecMatch(info,neh->info,nhi->type))
	    return(neh);
   return(NULL);
}

static void MarkStartReachable(LNode *ln)
{
   LArc *la;

   ln->n|=LNODE_FROM_ST;
   if (lnType(ln)==LNODE_END) return;
   for (la=ln->foll;la!=NULL;la=la->farc) {
      if (!(la->end->n&LNODE_FROM_ST))
	 MarkStartReachable(la->end);
   }
}

static void MarkEndReachable(LNode *ln)
{
   LArc *la;

   ln->n|=LNODE_FROM_EN;
   if (lnType(ln)==LNODE_START) return;
   for (la=ln->pred;la!=NULL;la=la->parc)
      if (!(la->start->n&LNODE_FROM_EN))
	 MarkEndReachable(la->start);
}

/* Add a continuation to neh based on LArc sa */
/* Will need to check if matches current possibility and if so */
/* check if new entry should be disgarded (and the next arc entered) */
/* or previous entry should be relaxed (after adding its next arc) */
static void LatMatch(NBNodeHeap *nodeheap,NBNodeEntry *neh,LArc *sa)
{
   LatNodeHeapInfo *nhi;
   LatNodeEntryInfo *info,*repl,*cont,fake;
   NBNodeEntry *mtch,*newNode;
   LArc *la;
   LogDouble like,score;

   nhi=(LatNodeHeapInfo*)nodeheap->info;
   info=(LatNodeEntryInfo*)neh->info;

   for (la=sa;la!=NULL;la=la->farc) {
      like=info->like+la->score;  /* la->score == total likelihood */
      score=like+la->end->score;  /* ln->score == backward lahead */

      if (score>LSMALL && (lnType(la->end)&LNODE_VIABLE)) {
         fake.prev=neh; fake.ln=la->end; fake.la=la;
         fake.hv=RecursiveHash(info->hv,la->end->sublat,nhi->ansHashSize);
         if (mtch=FindAnsMatch(&fake,nodeheap)) {
            if (score>mtch->score) {
               /* About to forget about matching entry but cannot */
               /*  forget about the next one in the foll list */

               repl=(LatNodeEntryInfo*)mtch->info;
               if (repl->narc!=NULL)
                  LatMatch(nodeheap,repl->prev,repl->narc);

               ExpandNodeEntry(mtch,score,nodeheap);
               repl->prev=neh;
               repl->like=like;
               repl->ln=la->end;
               repl->la=la;
               repl->narc=la->farc;
#ifdef SANITY
               if (repl->hv!=RecursiveHash(info->hv,la->end->sublat,
                  nhi->ansHashSize))
                  HError(9999,"No food on sundays");
               repl->hv=RecursiveHash(info->hv,la->end->sublat,
                  nhi->ansHashSize);
#endif
               break;
            }
            /* Can just forget about this possibility if */
            /* we add the next la in following list */
         }
         else {
            /* We are only going to add a single follower */
            /*  The rest will be added when this gets expanded. */
            /*  Note: this will only work correctly if we sort the farc */
            /*        linked list into most likely to least likely order */

            newNode=NewNodeEntry(score,NULL,nodeheap);
            cont=(LatNodeEntryInfo*)newNode->info;
            cont->prev=neh;
            cont->like=like;
            cont->ln=la->end;
            cont->la=la;
            cont->narc=la->farc;
            cont->hv=RecursiveHash(info->hv,la->end->sublat,
               nhi->ansHashSize);
            cont->hlink=nhi->ansHashTab[cont->hv];
            nhi->ansHashTab[cont->hv]=newNode;
            break;
         }
      }
   }
}

NBNodeEntry *LatNextBestNBEntry(NBNodeHeap *nodeheap)
{
   LatNodeHeapInfo *nhi;
   LatNodeEntryInfo *info;
   NBNodeEntry *best;

   nhi=(LatNodeHeapInfo*)nodeheap->info;
   /* If we don't get anything the nodeheap is empty */
   while ((best=GetNodeEntry(nodeheap))!=NULL) {
      info=(LatNodeEntryInfo*)best->info;

      /* May need to add next possibility from previous node expansion */
      if (info->narc!=NULL)
         LatMatch(nodeheap,info->prev,info->narc);

      /* If ln->score==0.0 we have reached the end of our path */
      /*  Needs better check for case where we don't end at the end */
      /*  of the lattice and ln->score<0.0 at the end of nbest search */
      if (lnType(info->ln) == LNODE_VIABLE_EN) break;

      LatMatch(nodeheap,best,info->ln->foll);
   }
   return best;
}

NBNodeHeap *PrepareLatNBest(int n,AnsMatchType type,Lattice *lat,Boolean noSort)
{
   NBNodeHeap *nodeheap;
   LatNodeHeapInfo *nhi;
   NBNodeEntry *neh;
   LNode *ln;
   LatNodeEntryInfo *info;
   double score,like;
   int i,ansHashSize;

   /* Make the NBNodeHeap and AnsMatch hash table */
   ansHashSize=NB_HASH_SIZE;
   nodeheap=CreateNodeHeap(sizeof(LatNodeEntryInfo),MRound(sizeof(LatNodeHeapInfo))+
		     sizeof(NBNodeEntry*)*ansHashSize);
   nhi=(LatNodeHeapInfo*)nodeheap->info;
   nhi->lat=lat;
   nhi->n=n;
   nhi->type=type;
   nhi->ansHashSize=ansHashSize;
   nhi->ansHashTab=(NBNodeEntry**)(((char*)nhi)+MRound(sizeof(LatNodeHeapInfo)));
   for (i=0;i<nhi->ansHashSize;i++) nhi->ansHashTab[i]=NULL;

   /* Sort lattice prior to doing NBest */
   if (!noSort)
      SortLNodes(lat,TRUE,FALSE);
   nhi->like=LatLAhead(lat,0);
   SortLArcs(lat,-1);
   SortLArcs(lat,1);
   /* Mark lattice nodes with likelihood and reachable status */
   like=LatLAhead(lat,1);
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
      if (lnType(ln)==LNODE_START) MarkStartReachable(ln);
      if (lnType(ln)==LNODE_END) MarkEndReachable(ln);
   }
   /* Check which nodes are viable and for each viable start */
   /*  node put a dummy start entry into the nodeheap */
   for (i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
      if (ln->sublat!=NULL)
         HError(9999,"SetUpForNBest: Cannot do NBest through sublats");
      if ((ln->n&LNODE_FROM_ST) && (ln->n&LNODE_FROM_EN))
         ln->n=lnType(ln)|LNODE_VIABLE;
      if (lnType(ln)==LNODE_VIABLE_ST && ln->score>LSMALL) {
         /* Put in dummy start entry for viable start node */
         neh=NewNodeEntry(+HUGE_VAL,NULL,nodeheap);
         info=(LatNodeEntryInfo*)neh->info;
         info->prev=NULL; info->like=ln->score; info->ln=ln;
         info->la=NULL; info->narc=NULL; info->hv=0;
         info->hlink=nhi->ansHashTab[info->hv];
         nhi->ansHashTab[info->hv]=neh;
      }
      ln->sublat=LNodeMatchPtr(ln,type);
   }
   /* Calculate lookahead likelihoods (from end of utterance) so */
   /*  that we can remove the dummy entries and put them back again */
   /*  with the correct score */
   like=LatLAhead(lat,-1);
   while(nodeheap->head!=NULL && nodeheap->head->score==+HUGE_VAL) {
      neh=GetNodeEntry(nodeheap);
      info=neh->info;
      score=info->like+info->ln->score;
      NewNodeEntry(score,neh,nodeheap);
   }
   return nodeheap;
}

/* ------------------------ End of HNBest.c ----------------------- */
