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
/*       File: HNet.c -   Network and Lattice Functions        */
/* ----------------------------------------------------------- */

char *hnet_version = "!HVER!HNet: 1.6.0 [SJY 01/06/07]";

#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HWave.h"
#include "HAudio.h"
#include "HParm.h"
#include "HLabel.h"
#include "HGraf.h"
#include "HModel.h"
#include "HUtil.h"
#include "HDict.h"
#include "HLM.h"
#include "HNet.h"

/* Cleaned up threading problems 20/08/04 - SJY */

#ifdef nil
#undef nil
#endif

/* ----------------------------- Trace Flags ------------------------- */

#define T_INF 0001         /* Print network stats */
#define T_CXT 0002         /* Trace context definitions */
#define T_CST 0004         /* Trace network construction */
#define T_MOD 0010         /* Show models making up each word */
#define T_ALL 0020         /* Show whole network */
#define T_FND 0040         /* Trace find model */


static int trace=0;
static ConfParam *cParm[MAXGLOBS];      /* config parameters */
static int nParm = 0;
static LabId nullNodeId;


/* --------------------------- Global Flags -------------------------- */

Boolean forceCxtExp=FALSE;
/*
force triphone context exp to get model names
ie. don't use model names direct from dict
without expansion (is overridden by allowCxtExp)
*/
Boolean forceLeftBiphones=FALSE;
Boolean forceRightBiphones=FALSE;
/*
force biphone context exp to get model names
ie. don't try triphone names
*/
Boolean allowCxtExp=TRUE;
/*
allow context exp to get model names
*/
Boolean allowXWrdExp=FALSE;
/*
allow context exp across words
*/
Boolean cfWordBoundary=TRUE;
/*
In word internal systems treat context free phones as word boundaries.
*/
Boolean factorLM=FALSE;
/*
factor lm likelihoods throughout words
*/
Boolean phnTreeStruct=FALSE;
/*
tree structure network at the phone level
*/
char *frcSil=NULL,frcSilBuf[MAXSTRLEN];
/*
Automagically add these sil models to the end of words.
*/
Boolean remDupPron=TRUE;
/*
Remove duplicate pronunciations
*/

Boolean sublatmarkers=FALSE;
/*
Add sublatstart and sublatend markers to the lattice
*/
char *subLatStart="!SUBLAT_(",subLatStartBuf[MAXSTRLEN];
char *subLatEnd="!)_SUBLAT",subLatEndBuf[MAXSTRLEN];
/*
Set these strings as the start and end sublattice markers
*/

/* --------------------------- Initialisation ---------------------- */

/* EXPORT->InitNet: register module & set configuration parameters */
void InitNet(void)
{
   Boolean b;
   int i;

   Register(hnet_version);
   nParm = GetConfig("HNET", TRUE, cParm, MAXGLOBS);
   if (nParm>0){
      if (GetConfBool(cParm,nParm,"FORCECXTEXP",&b)) forceCxtExp = b;
      if (GetConfBool(cParm,nParm,"FORCELEFTBI",&b)) forceLeftBiphones = b;
      if (GetConfBool(cParm,nParm,"FORCERIGHTBI",&b)) forceRightBiphones = b;
      if (GetConfBool(cParm,nParm,"ALLOWCXTEXP",&b)) allowCxtExp = b;
      if (GetConfBool(cParm,nParm,"ALLOWXWRDEXP",&b)) allowXWrdExp = b;
      if (GetConfBool(cParm,nParm,"CFWORDBOUNDARY",&b)) cfWordBoundary = b;
      if (GetConfBool(cParm,nParm,"FACTORLM",&b)) factorLM = b;
      if (GetConfStr(cParm,nParm,"ADDSILPHONES",frcSilBuf)) frcSil=frcSilBuf;
      if (GetConfStr(cParm,nParm,"STARTSUBLAT",subLatStartBuf))
         subLatStart=subLatStartBuf;
      if (GetConfStr(cParm,nParm,"ENDSUBLAT",subLatEndBuf))
         subLatEnd=subLatEndBuf;
      if (GetConfBool(cParm,nParm,"REMDUPPRON",&b)) remDupPron = b;
      if (GetConfBool(cParm,nParm,"MARKSUBLAT",&b)) sublatmarkers = b;
      if (GetConfBool(cParm,nParm,"PHNTREESTRUCT",&b)) phnTreeStruct = b;
      if (GetConfInt(cParm,nParm,"TRACE",&i)) trace = i;
   }
   nullNodeId = GetLabId("!NULL",TRUE);
}

/* ====================================================================*/
/*                     PART ONE - LATTICES                             */
/* ====================================================================*/


/* ------------------------ Lattice Creation ------------------------- */

#define SafeCopyString(heap,str) ((str)==NULL?NULL:CopyString((heap),(str)))

/* EXPORT->NewLattice: Allocate and initialise a new lattice structure */
Lattice *NewLattice(MemHeap *heap,int nn,int na)
{
   Lattice *lat;
   LNode *ln;
   LArc *la;
   int i;

   lat=(Lattice *) New(heap,sizeof(Lattice));
   lat->heap=heap;
   lat->nn=nn;
   lat->na=na;

   lat->format=0;
   lat->utterance=lat->vocab=NULL;
   lat->net=lat->hmms=NULL;
   lat->lmscale=1.0; lat->wdpenalty=0.0;
   lat->prscale=0.0; lat->framedur=0.0;
   lat->subList = NULL;
   lat->refList = NULL;
   lat->subLatId = NULL;
   lat->chain = NULL;

   if (nn>0) lat->lnodes=(LNode *) New(heap, sizeof(LNode)*nn);
   else lat->lnodes=NULL;

   if (na>0) lat->larcs=(LArc *) New(heap, sizeof(LArc)*na);
   else lat->larcs=NULL;

   for(i=0,ln=lat->lnodes;i<nn;i++,ln++) {
      ln->time=0.0;ln->word=NULL;ln->tag=NULL;
      ln->foll=ln->pred=NARC;
      ln->hook=NULL;
      ln->sublat=NULL;
   }
   for(i=0,la=lat->larcs;i<na;i++,la++) {
      la->aclike=la->lmlike=la->prlike=0.0;
      la->start=la->end=NNODE;
      la->farc=la->parc=NARC;
      la->nAlign=0;la->lAlign=NULL;
   }
   return(lat);
}

/* EXPORT->NewILattice: Allocate and initialise a new lattice structure */
Lattice *NewILattice(MemHeap *heap,int nn,int na,Lattice *info)
{
   Lattice *lat;
   LNode *ln,*in;
   LArc *la,*ia;
   int i,j;

   lat=(Lattice *) New(heap,sizeof(Lattice));
   lat->heap=heap;
   lat->nn=nn;
   lat->na=na;

   lat->format = info->format;

   lat->voc = info->voc;
   lat->subLatId = info->subLatId;
   lat->subList=NULL; lat->refList=NULL; lat->chain=NULL;
   lat->utterance = SafeCopyString(heap,info->utterance);
   lat->vocab = SafeCopyString(heap,info->vocab);
   lat->hmms = SafeCopyString(heap,info->hmms);
   lat->net = SafeCopyString(heap,info->net);
   lat->lmscale = info->lmscale;
   lat->wdpenalty = info->wdpenalty;
   lat->prscale = info->prscale;
   lat->framedur = info->framedur;

   if (nn==-1) {
      lat->lnodes=(LNode *) New(heap, sizeof(LNode)*info->nn);
      lat->nn=info->nn;
   }
   else if (nn>0) {
      lat->lnodes=(LNode *) New(heap, sizeof(LNode)*nn);
      lat->nn=nn;
   }
   else {
      lat->lnodes=NULL;
      lat->nn=0;
   }

   if (info->format&HLAT_SHARC) i=sizeof(LArc_S);
   else i=sizeof(LArc);

   if (na==-1) {
      lat->larcs=(LArc *) New(heap, i*info->na);
      lat->na=info->na;
   }
   else if (na>0) {
      lat->larcs=(LArc *) New(heap, i*na);
      lat->na=na;
   }
   else {
      lat->larcs=NULL;
      lat->na=0;
   }

   if (nn==-1)
      for(i=0,ln=lat->lnodes,in=info->lnodes;i<lat->nn;i++,ln++,in++) {
         *ln=*in;
         if (in->word==lat->voc->subLatWord){
            ln->sublat=AdjSubList(lat,in->sublat->lat->subLatId,
					in->sublat->lat,+1);
            if(ln->sublat==NULL){
               HError(8253, "NewILattice: AdjSubList failed");
            }
         }
         if (in->foll!=NULL) ln->foll=NumbLArc(lat,LArcNumb(in->foll,info));
         if (in->pred!=NULL) ln->pred=NumbLArc(lat,LArcNumb(in->pred,info));
      }
		else
			for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
				ln->time=0.0;ln->word=NULL;ln->tag=NULL;
				ln->foll=ln->pred=NARC;
				ln->hook=NULL;
				ln->sublat=NULL;
			}
			if (na==-1)
				for(i=0;i<lat->na;i++) {
					la=NumbLArc(lat,i);
					ia=NumbLArc(info,i);
					if (info->format&HLAT_SHARC) *(LArc_S*)la=*(LArc_S*)ia;
					else *la=*ia;
					la->start=ia->start-info->lnodes+lat->lnodes;
					la->end=ia->end-info->lnodes+lat->lnodes;
					if (ia->farc!=NULL) la->farc=NumbLArc(lat,LArcNumb(ia->farc,info));
					if (ia->parc!=NULL) la->parc=NumbLArc(lat,LArcNumb(ia->parc,info));
					if (!(info->format&HLAT_SHARC) &&
						ia->nAlign>0 && ia->lAlign!=NULL) {
						la->lAlign=(LAlign *) New(heap,ia->nAlign*sizeof(LAlign));
						for (j=0;j<ia->nAlign;j++) la->lAlign[j]=ia->lAlign[j];
					}
				}
				else
					for(i=0;i<lat->na;i++) {
						la=NumbLArc(lat,i);
						la->lmlike=0.0;
						la->start=la->end=NNODE;
						la->farc=la->parc=NARC;
						if (!(info->format&HLAT_SHARC))
							la->aclike=0.0,la->nAlign=0,la->lAlign=NULL;
					}
					return(lat);
}

/* EXPORT->FreeLattice: free memory used by a lattice structure */
void FreeLattice(Lattice *lat)
{
   Dispose(lat->heap,lat);
}

/* ------------------------ Lattice Output ------------------------- */

#define SUBLATHASHSIZE 101

/* Search hash table for lattice matching subLatId.  If subLat!=NULL */
/* add (if not found) or check (if found) the new subLat definition. */
static Lattice *GetSubLat(LabId subLatId,Lattice *subLat)
{
   int h;
   Lattice *cur,*nxt;
   static Lattice **subLatHashTab = NULL;

   if (subLatHashTab==NULL) {
      /* Need to allocate and initialise table */
      subLatHashTab=(Lattice **) New(&gcheap,SUBLATHASHSIZE*sizeof(Lattice *));
      for (h=0;h<SUBLATHASHSIZE;h++) subLatHashTab[h]=NULL;
   }
   if (subLatId==NULL) {
      for (h=0;h<SUBLATHASHSIZE;h++)
         for (cur=subLatHashTab[h];cur!=NULL;cur=nxt) {
            nxt=cur->chain;
            cur->chain=NULL;
         }
			Dispose(&gcheap,subLatHashTab);
			subLatHashTab=NULL;
			return(NULL);
   }
   h=(((unsigned) subLatId)%SUBLATHASHSIZE);
   for (cur=subLatHashTab[h];cur!=NULL;cur=cur->chain)
      if (cur->subLatId==subLatId) break;
		if (subLat!=NULL) {
			if (cur==NULL) {
				/* Add this to table */
				cur=subLat;
				cur->chain=subLatHashTab[h];
				subLatHashTab[h]=cur;
			}
			if (cur!=subLat)
				HError(8253,"GetSubLat: All sublats must have unique names");
		}

		return(cur);
}

/* Search list of sublats for current lattice for a match */
/*  When add>0 either add new sublat or increase usage of match */
/*  when add<0 either decrease usage of match and possibly remove */
/*  If adding new sublat get actual lattice definition from hash */
/*  table containing all loaded so far unless supplied */
/* When add>0 if can't find sublat returns NULL - error condition */
SubLatDef *AdjSubList(Lattice *lat,LabId subLatId,Lattice *subLat,int adj)
{
   SubLatDef *p,*q,*r,*s;

   /* Inefficient linear search may be good enough */
   for (p=lat->subList,q=NULL;p!=NULL;q=p,p=p->next)
      if (p->lat->subLatId==subLatId) break;
		if (adj<0) {
			if (p==NULL)
				HError(8253,"AdjSubList: Decreasing non-existent sublat",
				subLatId->name);
			p->usage+=adj;
			if (p->usage<=0) {
				/* First remove from refList */
				for (r=p->lat->refList,s=NULL;r!=NULL;s=r,r=r->chain)
					if (r==p) break;
					if (r!=p || r==NULL)
						HError(8253,"AdjSubList: Could not find SubLatDef in refList");
					if (s==NULL) p->lat->refList=p->chain;
					else s->chain=r->chain;
					/* Then remove from subList */
					if (q==NULL) lat->subList=p->next;
					else q->next=p->next;
					p=NULL;
			}
		}
		else if (adj>0) {
			if (p==NULL) {
				p=(SubLatDef *) New(lat->heap,sizeof(SubLatDef));
				/* p->subLatId=subLatId; */
				if (subLat!=NULL) p->lat=subLat;
				else if ((p->lat=GetSubLat(subLatId,NULL))==NULL){
					HRError(8253,"AdjSubList: SUBLAT %s not found",subLatId->name);
					return NULL;
				}
				p->next=lat->subList;
				lat->subList=p;
				if (p->lat==lat){
					HRError(8253,"AdjSubList: Circular subLat reference to %s",
						subLatId->name);
					return NULL;
				}
				p->chain=p->lat->refList;
				p->lat->refList=p;
				p->usage=0;
			}
			p->usage+=adj;
		}
		return(p);
}

#define MAXLATDEPTH 32

Lattice *SubLatList(Lattice *lat, Lattice *tail, int depth)
{
   SubLatDef *sub;

   if (depth>MAXLATDEPTH)
      HError(8253,"SubLatList: Nesting too deep (%d == recursive ?)",depth);

   for (sub=lat->subList;sub!=NULL;sub=sub->next) {
      if (tail==NULL) sub->lat->chain=NULL;
      if (sub->lat->chain==NULL)
         tail=SubLatList(sub->lat,tail,depth+1);
   }
   if (tail!=NULL)
      for (sub=lat->subList;sub!=NULL;sub=sub->next) {
         if (sub->lat->chain!=NULL) continue;  /* Done it already */
#if 0 /* Algorithm sanity check only needed once */
         {
            Lattice *cur;
				/* Quick sanity check */
				cur=tail->chain; /* Actual lattice == last in list */
				for (cur=cur->chain;cur!=tail->chain;cur=cur->chain)
					if (sub->lat==cur || sub->lat->subLatId==cur->subLatId ||
						sub->lat->subLatId==NULL) break;
					if (cur!=tail->chain) /* Match */
						HError(8253,"Match");
         }
#endif
         sub->lat->chain=tail->chain;
         tail->chain=sub->lat;
         tail=sub->lat;
      }
		return(tail);
}

/* Lattices often need to be sorted before output */

static Lattice *slat;   /* Used by qsort cmp routines */

/* QSCmpNodes: order on time, then score if times equal */
static int QSCmpNodes(const void *v1,const void *v2)
{
   int s1,s2;
   double tdiff,sdiff;

   s1=*((int*)v1);s2=*((int*)v2);
   tdiff=slat->lnodes[s1].time-slat->lnodes[s2].time;
   sdiff=slat->lnodes[s1].score-slat->lnodes[s2].score;
   if (tdiff==0.0) {
      if (sdiff==0.0) return(s1-s2);
      else if (sdiff>0.0) return(1);
      else return(-1);
   }
   else if (tdiff>0.0) return(1);
   else return(-1);
}

/* QSCmpArcs: order on end node order, then start node order */
static int QSCmpArcs(const void *v1,const void *v2)
{
   int s1,s2,j,k;

   s1=*((int*)v1);s2=*((int*)v2);
   j=slat->larcs[s1].end->n-slat->larcs[s2].end->n;
   k=slat->larcs[s1].start->n-slat->larcs[s2].start->n;
   if (k==0 && j==0) return(s1-s2);
   else if (j==0) return(k);
   else return(j);
}

/* OutputIntField: output integer as text or binary */
static void OutputIntField(char field,int val,Boolean bin,
                           char *form,FILE *file)
{
   fprintf(file,"%c%c",field,bin?'~':'=');
   if (bin)
      WriteInt(file,&val,1,TRUE);
   else
      fprintf(file,form,val);
   fprintf(file," ");
}

/* OutputFloatField: output float as text or binary */
static void OutputFloatField(char field,float val,Boolean bin,
                             char *form,FILE *file)
{
   fprintf(file,"%c%c",field,bin?'~':'=');
   if (bin)
      WriteFloat(file,&val,1,TRUE);
   else
      fprintf(file,form,val);
   fprintf(file," ");
}

/* OutputAlign: output models aligned with this arc */
static void OutputAlign(LArc *la,int format,FILE *file)
{
   int i;
   LAlign *lal;

   fprintf(file,"d=:");
   for(i=0,lal=la->lAlign;i<la->nAlign;i++,lal++) {
      fprintf(file,"%s",lal->label->name);
      if (format&HLAT_ALDUR)
         fprintf(file,",%.2f",lal->dur);
      if (format&HLAT_ALLIKE)
         fprintf(file,",%.2f",lal->like);
      fprintf(file,":");
   }
}

/* WriteOneLattice: Write a single lattice to file */
ReturnStatus WriteOneLattice(Lattice *lat,FILE *file,LatFormat format)
{
   int i, *order, *rorder, st, en;
   LNode *ln;
   LArc *la;

   /* Rather than return an error assume labels on nodes !! */
   order=(int *) New(&gstack, sizeof(int)*(lat->nn<lat->na ? lat->na+1 : lat->nn+1));
   rorder=(int *) New(&gstack, sizeof(int)*lat->nn);

   if (lat->subLatId) fprintf(file,"SUBLAT=%s\n",lat->subLatId->name);

   fprintf(file,"N=%-4d L=%-5d\n",lat->nn,lat->na);

   for (i=0;i<lat->nn;i++)
      order[i]=i;
   if (!(lat->format&HLAT_SHARC) && !(format&HLAT_NOSORT)) {
      slat=lat;
      qsort(order,lat->nn,sizeof(int),QSCmpNodes);
   }

   if ((format&HLAT_TIMES) || !(format&HLAT_ALABS)) {
      for (i=0;i<lat->nn;i++) {
         ln=lat->lnodes+order[i];
         rorder[order[i]]=i;
         ln->n = i;
         OutputIntField('I',i,format&HLAT_LBIN,"%-4d",file);
         if (format&HLAT_TIMES)
            OutputFloatField('t',ln->time,format&HLAT_LBIN,"%-5.2f",file);
         if (!(format&HLAT_ALABS)) {
            if (ln->word==lat->voc->subLatWord && ln->sublat!=NULL)
               fprintf(file,"L=%-19s ",
					ReWriteString(ln->sublat->lat->subLatId->name,
					NULL,ESCAPE_CHAR));
            else if (ln->word!=NULL) {
               fprintf(file,"W=%-19s ",
						ReWriteString(ln->word->wordName->name,
						NULL,ESCAPE_CHAR));
               if ((format&HLAT_PRON) && ln->v>=0)
                  OutputIntField('v',ln->v,format&HLAT_LBIN,"%-2d",file);
               if ((format&HLAT_TAGS) && ln->tag!=NULL)
                  fprintf(file,"s=%-19s ",
						ReWriteString(ln->tag,NULL,ESCAPE_CHAR));
            }
            else
               fprintf(file,"W=%-19s ","!NULL");
         }
         fprintf(file,"\n");
      }
   }

   for (i=0;i<lat->na;i++)
      order[i]=i;
   if (!(lat->format&HLAT_SHARC) && !(format&HLAT_NOSORT)) {
      slat=lat;
      qsort(order,lat->na,sizeof(int),QSCmpArcs);
   }
   for (i=0;i<lat->na;i++) {
      la=NumbLArc(lat,order[i]);
      OutputIntField('J',i,format&HLAT_LBIN,"%-5d",file);
      st=rorder[la->start-lat->lnodes];
      en=rorder[la->end-lat->lnodes];
      OutputIntField('S',st,format&HLAT_LBIN,"%-4d",file);
      OutputIntField('E',en,format&HLAT_LBIN,"%-4d",file);
      if (format&HLAT_ALABS) {
         if (la->end->word!=NULL)
            fprintf(file,"W=%-19s ",
				ReWriteString(la->end->word->wordName->name,
				NULL,ESCAPE_CHAR));
         else
            fprintf(file,"W=%-19s ","!NULL");
         if ((format&HLAT_PRON) && ln->v>=0)
            OutputIntField('v',la->end->v,format&HLAT_LBIN,"%-2d",file);
      }
      if (!(lat->format&HLAT_SHARC) && (format&HLAT_ACLIKE))
         OutputFloatField('a',la->aclike,format&HLAT_LBIN,"%-9.2f",file);
      if (format&HLAT_LMLIKE) {
         if (lat->net==NULL)
            OutputFloatField('l',la->lmlike*lat->lmscale+lat->wdpenalty,
				format&HLAT_LBIN,"%-8.2f",file);
         else
            OutputFloatField('l',la->lmlike,format&HLAT_LBIN,"%-7.3f",file);
      }
      if (!(lat->format&HLAT_SHARC) && (format&HLAT_PRLIKE))
         OutputFloatField('r',la->prlike,format&HLAT_LBIN,"%-6.2f",file);
      if (!(lat->format&HLAT_SHARC) && (format&HLAT_ALIGN) && la->nAlign>0)
         OutputAlign(la,format,file);
      fprintf(file,"\n");
   }

   if (lat->subLatId) fprintf(file,".\n\n");

   Dispose(&gstack,order);
   slat=NULL;
   return(SUCCESS);
}

/* EXPORT->WriteLattice: Write lattice to file */
ReturnStatus WriteLattice(Lattice *lat,FILE *file,LatFormat format)
{
   LabId id;
   Lattice *list;

   fprintf(file,"VERSION=%s\n",L_VERSION);
   if (lat->utterance!=NULL)
      fprintf(file,"UTTERANCE=%s\n",lat->utterance);
   if (lat->net!=NULL) {
      fprintf(file,"lmname=%s\nlmscale=%-6.2f wdpenalty=%-6.2f\n",
			lat->net,lat->lmscale,lat->wdpenalty);
   }
   if (format&HLAT_PRLIKE)
      fprintf(file,"prscale=%-6.2f\n",lat->prscale);
   if (lat->vocab!=NULL) fprintf(file,"vocab=%s\n",lat->vocab);
   if (lat->hmms!=NULL) fprintf(file,"hmms=%s\n",lat->hmms);

   /* First write all subsidiary sublattices */
   if (lat->subList!=NULL && !(format&HLAT_NOSUBS)) {
      /* Set all chain fields to NULL */
      lat->chain=NULL; SubLatList(lat,NULL,1);
      /* Set chain fields to make linked list */
      lat->chain=lat; list=SubLatList(lat,lat,1);
      /* Disconnect loop */
      list->chain=NULL;
      for (list=lat->chain;list!=NULL;list=list->chain) {
         if (list->subLatId==NULL){
            HRError(8253,"WriteLattice: Sublats must be labelled");
            return(FAIL);
         }
         if(WriteOneLattice(list,file,format)<SUCCESS){
            return(FAIL);
         }
      }
   }
   id=lat->subLatId;
   lat->subLatId=NULL;
   if(WriteOneLattice(lat,file,format)<SUCCESS){
      return(FAIL);
   }
   lat->subLatId=id;
   return(SUCCESS);
}


/* ------------------------ Lattice Input ------------------------- */

/* CheckStEndNodes: lattice must only have one start and one end node */
static ReturnStatus CheckStEndNodes(Lattice *lat)
{
   int i,st,en;
   NodeId ln;

   st=en=0;
   for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++){
      if (ln->pred==NARC) ++st;
      if (ln->foll==NARC) ++en;
   }
   if (st != 1){
      HRError(-8252,"CheckStEndNodes: lattice has %d start nodes",st);
      return(FAIL);
   }
   if (en != 1){
      HRError(-8252,"CheckStEndNodes: lattice has %d end nodes",en);
      return(FAIL);
   }
   return(SUCCESS);
}

/* GetNextFieldName: put field name into buf and delimiter into del */
char *GetNextFieldName(char *buf, char *del, Source *src)
{
   int ch,i;
   char *ptr;

   buf[0]=0;
   ch=GetCh(src);
   while (isspace(ch) && ch!='\n')
      ch=GetCh(src);
   if (ch==EOF) {
      ptr=NULL;
   }
   else if (ch=='\n') {
      buf[0]='\n';buf[1]=0;ptr=buf;
   }
   else if (!isalnum(ch) && ch != '.') {
      if (ch!='#')
         HError(8250,"GetNextFieldName: Field name expected");
      while (ch!='\n' && ch!=EOF) ch=GetCh(src);
      buf[0]='\n';buf[1]=0;ptr=buf;
   }
   else if (ch != '.') {
      i=0;
      while(isalnum(ch)) {
         buf[i++]=ch;
         ch=GetCh(src);
         if (ch==EOF)
            HError(8250,"GetNextFieldName: EOF whilst reading field name");
      }
      buf[i]=0;
      if (ch!='=' && ch!='~')
         HError(8250,"GetNextFieldName: Field delimiter expected %s|%c|",buf,ch);
      *del=ch;
      ptr=buf;
   }
   else {
      buf[0]='.';buf[1]=0;ptr=buf;
   }
   return(ptr);
}

/* GetFieldValue: into buf and return type */
LatFieldType GetFieldValue(char *buf, Source *src)
{
   static char tmp[MAXSTRLEN];
   int ch;

   ch=GetCh(src);
   if (isspace(ch) || ch==EOF)
      HError(8250,"GetFieldValue: Field value expected");
   UnGetCh(ch,src);
   if (buf==NULL)
      ReadString(src,tmp);
   else
      ReadString(src,buf);
   if (src->wasQuoted)
      return(STR_FIELD);
   else
      return(UNK_FIELD);
}

/* ParseNumber: if buf is number convert it and return in rval */
LatFieldType ParseNumber(double *rval,char *buf)
{
   char *ptr;
   double val;
   LatFieldType type;

   type=STR_FIELD;
   if (isdigit(buf[0]) ||
		((buf[0]=='-' || buf[0]=='+') &&
		isdigit(buf[1]))) {
      val=strtod(buf,&ptr);
      if (ptr != buf) {
         type=INT_FIELD;
         if (strchr(buf,'.') != NULL) {
            type=FLT_FIELD;
            *rval=val;
         }
         else
            *rval=val;
      }
   }
   return(type);
}

/* GetIntField: return integer field */
int GetIntField(char ntype,char del,char *vbuf,Source *src)
{
   int vtype,iv;
   double rv;
   int fldtype;

   if (del=='=') {
      if ((vtype=GetFieldValue(vbuf,src))==STR_FIELD)
         HError(8250,"GetIntField: %c field expects numeric value (%s)",
			ntype,vbuf);
      if ((fldtype = ParseNumber(&rv,vbuf))!=INT_FIELD)
         HError(8250,"GetIntField: %c field expects integer value (%s==%d)",
			ntype,vbuf,fldtype);
   }
   else {
      if (!ReadInt(src,&iv,1,TRUE))
         HError(8250,"GetIntField: Could not read integer for %c field",ntype);
      rv=iv;
   }
   return((int)rv);
}

/* GetFltField: return float field */
double GetFltField(char ntype,char del,char *vbuf,Source *src)
{
   int vtype;
   double rv;
   float fv;

   if (del=='=') {
      if ((vtype=GetFieldValue(vbuf,src))==STR_FIELD)
         HError(8250,"GetFltField: %c field expects numeric value (%s)",ntype,vbuf);
      if (ParseNumber(&rv,vbuf)==STR_FIELD)
         HError(8250,"GetFltField: %c field expects numeric value (%s)",ntype,vbuf);
   }
   else {
      if (!(ReadFloat(src,&fv,1,TRUE)))
         HError(8250,"GetFltField: Could not read float for %c field",ntype);
      rv=fv;
   }
   return(rv);
}

static int ReadAlign(Lattice *lat,LArc *la,char *buf)
{
   LAlign *lal;
   char *p,*str;
   int i,n,c;

   for(n=-1,p=buf;*p;p++) if (*p==':') n++;
   if (n<1) return(0);

   la->lAlign=(LAlign *) New(lat->heap,sizeof(LAlign)*n);

   for(i=0,lal=la->lAlign,p=buf;i<n;i++,lal++) {
      if (*p!=':')
         HError(8250,"ReadAlign: ':' Expected at start of field %d in %s",
			i,buf);
      for (str=++p;*p!=':' && *p!=',';p++)
         if (*p==0) HError(8250,"ReadAlign: Unexpected end of field %s",buf);
			c=*p;*p=0;lal->label=GetLabId(str,TRUE);
			if ((str=strchr(lal->label->name,'['))!=NULL)
				lal->state=atoi(str+1);
			else lal->state=-1;
			*p=c;
			if (*p==',') {
				str=p+1;
				lal->dur=strtod(str,&p);
				if (p==str || (*p!=':' && *p!=','))
					HError(8250,"ReadAlign: Cannot read duration %d from field %s",
					i,buf);
			}
			if (*p==',') {
				str=p+1;
				lal->like=strtod(str,&p);
				if (p==str || *p!=':')
					HError(8250,"ReadAlign: Cannot read like %d from field %s",i,buf);
			}
   }
   return(n);
}

/* ReadOneLattice: Read (one level) of lattice from file */
static Lattice *ReadOneLattice(Source *src, MemHeap *heap, Vocab *voc,
                               Boolean shortArc, Boolean add2Dict)
{
   int i,s,e,n,v,nn,na;
   Lattice *lat;
   LNode *ln;
   LArc *la;
   Word wordId;
   double time,aclike,lmlike;
   double prlike;
   char nbuf[132],vbuf[132],*ptr,ntype,del;
   char dbuf[4096];
   double lmscl=1.0,lmpen=0.0;

   char *uttstr,*lmnstr,*vocstr,*hmmstr,*sublatstr,*tag;
   SubLatDef *subLatId;

   lat = (Lattice *) New(heap,sizeof(Lattice));
   lat->heap=heap; lat->subLatId=NULL; lat->chain=NULL;
   lat->voc=voc; lat->refList=NULL; lat->subList=NULL;

   /* Initialise default header values */
   nn=0;na=0; uttstr=lmnstr=vocstr=hmmstr=sublatstr=NULL;
   /* Process lattice header */
   while((ptr=GetNextFieldName(nbuf,&del,src))) {
      if (nbuf[0]=='\n') {
         if (na != 0 && nn != 0) break;
      }
      else if (strlen(ptr)==1) {
         ntype=*ptr;
         switch(ntype) {
         case 'N':
            nn=GetIntField('N',del,vbuf,src);
            break;
         case 'L':
            na=GetIntField('L',del,vbuf,src);
            break;
         default:
            GetFieldValue(0,src);
            break;
         }
      }
      else {
         if (!strcmp(ptr,"UTTERANCE"))
            GetFieldValue(vbuf,src),uttstr=CopyString(heap,vbuf);
         else if (!strcmp(ptr,"SUBLAT"))
            GetFieldValue(vbuf,src),sublatstr=CopyString(heap,vbuf);
         else if (!strcmp(ptr,"vocab"))
            GetFieldValue(vbuf,src),vocstr=CopyString(heap,vbuf);
         else if (!strcmp(ptr,"hmms"))
            GetFieldValue(vbuf,src),hmmstr=CopyString(heap,vbuf);
         else if (!strcmp(ptr,"lmname"))
            GetFieldValue(vbuf,src),lmnstr=CopyString(heap,vbuf);
         else if (!strcmp(ptr,"wdpenalty"))
            lmpen=GetFltField('p',del,vbuf,src);
         else if (!strcmp(ptr,"lmscale"))
            lmscl=GetFltField('s',del,vbuf,src);
         else
            GetFieldValue(NULL,src);
      }
   }

   if(ptr == NULL){
      /* generic memory clearing routine */
      Dispose(heap, lat);
      HRError(8250,"ReadLattice: Premature end of lattice file before header");
      return(NULL);
   }

   /* Initialise lattice based on header information */
   lat->nn=nn;
   lat->na=na;
   lat->utterance=uttstr;lat->vocab=vocstr;lat->hmms=hmmstr;
   lat->net=lmnstr;lat->lmscale=lmscl;lat->wdpenalty=lmpen;
   /* Set format to indicate type and default word label position */
   lat->format=(shortArc?HLAT_SHARC|HLAT_ALABS:HLAT_ALABS);

   /* Presence of SUBLAT=id string indicates more to come */
   lat->subList=NULL; lat->chain=NULL;
   if (sublatstr!=NULL) lat->subLatId = GetLabId(sublatstr,TRUE);
   else lat->subLatId = NULL;

   /* Allocate and initiailise nodes/arcs */
   lat->lnodes=(LNode *) New(heap, sizeof(LNode)*nn);
   if (shortArc)
      lat->larcs=(LArc *) New(heap, sizeof(LArc_S)*na);
   else
      lat->larcs=(LArc *) New(heap, sizeof(LArc)*na);

   for(i=0, ln=lat->lnodes; i<nn; i++, ln++) {
      ln->hook=NULL;
      ln->pred=NULL;
      ln->foll=NULL;
   }
   for(i=0, la=lat->larcs; i<na; i++, la=NextLArc(lat,la)) {
      la->lmlike=0.0;
      la->start=la->end=NNODE;
      la->farc=la->parc=NARC;
   }
   if (!shortArc)
      for(i=0, la=lat->larcs; i<na; i++, la=NextLArc(lat,la)) {
         la->aclike=la->prlike=la->score=0.0;
         la->nAlign=0;
         la->lAlign=NULL;
      }

		do {
			if ((ptr=GetNextFieldName(nbuf,&del,src)) == NULL)
				break;
			/* Recognised line types have only one character names */
			if (strlen(ptr)==1)
				ntype=*ptr;
			else
				ntype=0;
			if (ntype == '.') {
				ptr = NULL;
				break;
			}
			switch(ntype) {
			case '\n': break;
			case 'I':
				n=GetIntField('I',del,vbuf,src);
				if (n < 0 || n >= lat->nn){
					Dispose(heap, lat);
					HRError(8251,"ReadLattice: Lattice does not contain node %d",n);
					return(NULL);
				}
				ln=lat->lnodes+n;
				if (ln->hook!=NULL){
					Dispose(heap, lat);
					HRError(8251,"ReadLattice: Duplicate info info for node %d",n);
					return(NULL);
				}
				time=0.0;wordId=voc->nullWord;tag=NULL;v=-1;
				while((ptr=GetNextFieldName(nbuf,&del,src)) != NULL) {
					if (nbuf[0]=='\n') break;
					else {
						if (strlen(ptr)>=1)
							ntype=*ptr;
						else
							ntype=0;
						switch(ntype) {
						case 't':
							time=GetFltField('t',del,vbuf,src);
							lat->format |= HLAT_TIMES;
							break;
						case 'W':
							GetFieldValue(vbuf,src);
							wordId=GetWord(voc,GetLabId(vbuf,add2Dict),add2Dict);
							if (wordId==NULL){
								Dispose(heap, lat);
								HRError(8251,"ReadLattice: Word %s not in dict",vbuf);
								return(NULL);
							}
							break;
						case 's':
							GetFieldValue(vbuf,src);
							tag=CopyString(heap,vbuf);
							lat->format |= HLAT_TAGS;
							break;
						case 'L':
							GetFieldValue(vbuf,src);
							wordId=voc->subLatWord;
							if((subLatId=AdjSubList(lat,GetLabId(vbuf,TRUE),NULL,+1))==NULL) {
								HRError(8251,"ReadLattice: AdjSubLat failed");
								return(NULL);
							}

							break;
						case 'v':
							lat->format |= HLAT_PRON;
							v=GetIntField('v',del,vbuf,src);
							break;
						default:
							GetFieldValue(0,src);
							break;
						}
					}
				}
				if (wordId != voc->nullWord)
					lat->format &= ~HLAT_ALABS;
				ln->time=time;
				ln->word=wordId;
				ln->tag=tag;
				ln->v=v;
				if (wordId == voc->subLatWord)
					ln->sublat = subLatId;
				else
					ln->sublat = NULL;
				ln->hook=ln;
				nn--;
				break;
			case 'J':
				n=GetIntField('I',del,vbuf,src);
				if (n<0 || n>=lat->na){
					Dispose(heap, lat);
					HRError(8251,"ReadLattice: Lattice does not contain arc %d",n);
					return(NULL);
				}
				la=NumbLArc(lat,n);
				if (la->start!=NULL){
					Dispose(heap, lat);
					HRError(8251,"ReadLattice: Duplicate info for arc %d",n);
					return(NULL);
				}
				s=e=v=-1; wordId=NULL; aclike=lmlike=0.0;
				prlike=0.0;
				while ((ptr=GetNextFieldName(nbuf,&del,src))) {
					if (nbuf[0]=='\n') break;
					else {
						if (strlen(ptr)>=1) ntype=*ptr;
						else ntype=0;
						switch(ntype)
                  {
                  case 'S':
                     s=GetIntField('S',del,vbuf,src);
                     if (s<0 || s>=lat->nn){
                        Dispose(heap, lat);
                        HRError(8251,"ReadLattice: Lattice does not contain start node %d",s);
                        return(NULL);
                     }
                     break;
                  case 'E':
                     e=GetIntField('E',del,vbuf,src);
                     if (e<0 || e>=lat->nn){
                        Dispose(heap, lat);
                        HRError(8251,"ReadLattice: Lattice does not contain end node %d",e);
                        return(NULL);
                     }
                     break;
                  case 'W':
                     GetFieldValue(vbuf,src);
                     wordId=GetWord(voc,GetLabId(vbuf,add2Dict),add2Dict);
                     if (wordId==NULL || wordId==voc->subLatWord){
                        Dispose(heap, lat);
                        HRError(8251,"ReadLattice: Word %s not in dict",
									vbuf);
                        return(NULL);
                     }
                     break;
                  case 'v':
                     lat->format |= HLAT_PRON;
                     v=GetIntField('v',del,vbuf,src);
                     break;
                  case 't':
                     v=GetIntField('t',del,vbuf,src);
                     break;
                  case 'a':
                     lat->format |= HLAT_ACLIKE;
                     aclike=GetFltField('a',del,vbuf,src);
                     break;
                  case 'l':
                     lat->format |= HLAT_LMLIKE;
                     lmlike=GetFltField('l',del,vbuf,src);
                     break;
                  case 'r':
                     lat->format |= HLAT_PRLIKE;
                     prlike=GetFltField('r',del,vbuf,src);
                     break;
                  case 'd':
                     lat->format |= HLAT_ALIGN;
                     GetFieldValue(dbuf,src);
                     if (!shortArc)
                        la->nAlign=ReadAlign(lat,la,dbuf);
                     break;
                  default:
                     GetFieldValue(0,src);
                     break;
                  }
					}
				}
				if (s<0 || e<0 ||(wordId==NULL && (lat->format&HLAT_ALABS))){
					Dispose(heap, lat);
					HRError(8250,"ReadLattice: Need to know S,E [and W] for arc %d",n);
					return(NULL);
				}
				la->start=lat->lnodes+s;
				la->end=lat->lnodes+e;
				la->lmlike=lmlike;

				if ((lat->format&HLAT_ALABS) && la->end->word == voc->nullWord)
					la->end->word=wordId;
				if (wordId != NULL && la->end->word != wordId){
					Dispose(heap, lat);
					HRError(8251,"ReadLattice: Lattice arc (%d) W field (%s) different from node (%s)",  n,wordId->wordName->name,la->end->word->wordName->name);
					return(NULL);
				}

				la->farc=la->start->foll;
				la->parc=la->end->pred;
				la->start->foll=la;
				la->end->pred=la;
				if (!shortArc) {
					la->aclike=aclike;
					la->prlike=prlike;
				}
				na--;
				break;
      default:
         GetFieldValue(0,src);
         while ((ptr=GetNextFieldName(nbuf,&del,src))) {
            if (nbuf[0]=='\n') break;
            else GetFieldValue(0,src);
         }
         break;
      }
   }
   while(ptr != NULL);
   if (na!=0 || (nn!=0 && nn!=lat->nn)){
      Dispose(heap, lat);
      HRError(8250,"ReadLattice: %d Arcs unseen and %d Nodes unseen",na,nn);
      return(NULL);
   }

   if(CheckStEndNodes(lat)<SUCCESS){
      Dispose(heap, lat);
      HRError(8250,"ReadLattice: Start/End nodes incorrect",na,nn);
      return(NULL);
   }

   for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      ln->hook=NULL;
   if (shortArc) lat->format&=~(HLAT_ACLIKE|HLAT_PRLIKE|HLAT_ALIGN);
   return(lat);
}


/* EXPORT->ReadLattice: Read lattice from file - calls ReadOneLattice */
/*                      for each level of a multi-level lattice file  */
Lattice *ReadLattice(FILE *file, MemHeap *heap, Vocab *voc,
                     Boolean shortArc, Boolean add2Dict)
{
   Lattice *lat,*list,*fLat;
   Source source;

   AttachSource(file,&source);

   if((lat=ReadOneLattice(&source,heap,voc,shortArc,add2Dict))==NULL)
      return NULL;

   if (lat->subLatId!=NULL) {
      /* Need to preserve first lattice to return */
      fLat=lat; lat = (Lattice *) New(heap,sizeof(Lattice)); *lat=*fLat;
      do {
         /* Add SUBLAT to hash table for later lookup */
         GetSubLat(lat->subLatId,lat);
         if((lat=ReadOneLattice(&source,heap,voc,shortArc,add2Dict))==NULL){
            Dispose(heap, fLat); /*fLat points to 1st thing on heap*/
            return NULL;
         }

      }
      while(lat->subLatId!=NULL);

      /* Clear hash table */
      GetSubLat(NULL,NULL);
      /* Set all chain fields to NULL */
      lat->chain=NULL;
      SubLatList(lat,NULL,1);
      /* Set chain fields to make linked list */
      lat->chain=lat;
      list=SubLatList(lat,lat,1);
      /* Disconnect loop */
      list->chain=NULL;
      /* Copy last to first Lattices to ensure lat is first thing on stack */
      *fLat=*lat; lat=fLat;
   }
   return(lat);
}

/* ----------------------- ExpandLattice -------------------------- */

static DictEntry specialNull;

/* ExpandedLatticeSize: Calculate the size of the new lattice */
static void ExpandedLatticeSize(Lattice *lat, int *nNodes,int *nArcs)
{
   int i;
   NodeId thisNode;

   for (i=0; i<lat->nn; i++) {
      thisNode = lat->lnodes+i;
      if (thisNode->word == lat->voc->subLatWord && thisNode->sublat != NULL) {
         if (thisNode->tag != NULL) {
            *nNodes += 1;
            *nArcs  += 1;
            if (sublatmarkers) {
               *nNodes += 1;
               *nArcs  += 1;
            }
         }
         ExpandedLatticeSize(thisNode->sublat->lat,nNodes,nArcs);
      }
   }
   *nNodes += lat->nn;
   *nArcs  += lat->na;
}

/* CopyLattice: copy lattice from lat to newlat starting at offsets         */
/*              *newNodes and *newArcs - ignore NULL id words if ignoreNull */
void CopyLattice(Lattice *lat, Lattice *newlat,
                 int *newNodes, int *newArcs, Boolean ignoreNull)
{
   int i,j;
   LNode *oldNode,*newNode;
   LArc *oldArc,*newArc;

   for (i=0,j=0; i< lat->nn; i++) {
      oldNode = lat->lnodes+i;
      if ((oldNode->word != &specialNull) || !ignoreNull) {
         newNode = newlat->lnodes+j+*newNodes;
         newNode->word = oldNode->word;
         newNode->tag = NULL;
         if (oldNode->tag != NULL)
            newNode->tag = oldNode->tag;
         newNode->sublat = oldNode->sublat;
         if (oldNode->foll != NULL) {
            newNode->foll=NumbLArc(newlat,
					*newArcs+LArcNumb(oldNode->foll,lat));
            for (oldArc=oldNode->foll; oldArc != NARC; oldArc = oldArc->farc) {
               newArc = NumbLArc(newlat,*newArcs+LArcNumb(oldArc,lat));
               newArc->start = newNode;
               newArc->lmlike = oldArc->lmlike;
            }
         }
         else
            newNode->foll = NULL;
         if (oldNode->pred != NULL) {
            newNode->pred = NumbLArc(newlat,
					*newArcs+LArcNumb(oldNode->pred,lat));
            for (oldArc = oldNode->pred; oldArc != NARC; oldArc = oldArc->parc) {
               newArc = NumbLArc(newlat,*newArcs+LArcNumb(oldArc,lat));
               newArc->end = newNode;
               newArc->lmlike = oldArc->lmlike;
            }
         }
         else
            newNode->pred = NULL;
         j++;
      }
   }
   for (i=0; i< lat->na; i++) {
      oldArc = NumbLArc(lat,i);
      newArc = NumbLArc(newlat,i+*newArcs);
      if (oldArc->farc != NULL)
         newArc->farc = NumbLArc(newlat,*newArcs+LArcNumb(oldArc->farc,lat));
      else
         newArc->farc = NULL;
      if (oldArc->parc != NULL)
         newArc->parc = NumbLArc(newlat,*newArcs+LArcNumb(oldArc->parc,lat));
      else
         newArc->parc = NULL;
   }
   *newNodes += j;
   *newArcs += lat->na;
}

/*  SubLattice: sub latStart/latEnd in place of thisNode in newlat */
void SubLattice(Lattice *newlat,NodeId thisNode, NodeId latStart,NodeId latEnd)
{
   ArcId thisArc;

   for (thisArc = thisNode->foll; thisArc != NULL; thisArc=thisArc->farc)
      thisArc->start = latEnd;
   for (thisArc = thisNode->pred; thisArc != NULL; thisArc=thisArc->parc)
      thisArc->end = latStart;
   latStart->pred = thisNode->pred;
   latEnd->foll = thisNode->foll;
   thisNode->pred = thisNode->foll = NARC;
   thisNode->word = &specialNull;
}


/* ExpandLattice: Expand all the subLats in newlat (recursively) */
static void ExpandLattice(Lattice *newlat, int nNodes, int nArcs)
{
   int i;
   NodeId thisNode;
   NodeId latStart,latEnd,node;
   ArcId arc;
   int newNodes, newArcs;
   int len;
   Lattice *subLat;

   for (i=0; i< nNodes; i++) {
      thisNode = newlat->lnodes+i;
      if (thisNode->word == newlat->voc->subLatWord) {
         newNodes = nNodes; newArcs = nArcs;
         subLat = thisNode->sublat->lat;
         CopyLattice(subLat,newlat,&newNodes,&newArcs,FALSE);
         if (thisNode->tag != NULL) {
            latStart = newlat->lnodes + nNodes +
               (FindLatStart(subLat)-subLat->lnodes);
            latEnd = newlat->lnodes + nNodes +
               +(FindLatEnd(subLat)-subLat->lnodes);

            if (sublatmarkers) {
               /* add sublat start marker */
               node = newlat->lnodes + newNodes;
               arc = NumbLArc(newlat,newArcs);
               node->word=GetWord(newlat->voc,nullNodeId,TRUE);
               node->tag=SafeCopyString(newlat->heap,subLatStart);
               /* node->word=GetWord(newlat->voc,
					GetLabId(subLatStart, TRUE),TRUE);
					if (node->word->pron==NULL)
					NewPron(newlat->voc,node->word,0,NULL,
					node->word->wordName,1.0); */

               arc->start = node;
               arc->end = latStart;
               node->foll = arc;
               latStart->pred = node->foll;
               latStart = node;

               newNodes++; newArcs++;

               /* add sublat end marker */
               node = newlat->lnodes + newNodes;
               arc = NumbLArc(newlat,newArcs);
               node->word=GetWord(newlat->voc,nullNodeId,TRUE);
               len = strlen(subLatEnd) + strlen(thisNode->tag) + 4;
               node->tag=(char *) New(newlat->heap,sizeof(char)*len);
               strcpy(node->tag, subLatEnd);
               strcat(node->tag, "-");
               strcat(node->tag, thisNode->tag);
               /* node->word=GetWord(newlat->voc,
					GetLabId(subLatEnd, TRUE),TRUE);
					if (node->word->pron==NULL)
					NewPron(newlat->voc,node->word,0,NULL,
					node->word->wordName,1.0);*/

               arc->start = latEnd;
               arc->end = node;
               latEnd->foll = arc;
               node->pred = latEnd->foll;
               node->foll = NARC;
               latEnd = node;

               newNodes++; newArcs++;
            }
            else {
               /* add a tagged !NULL node holding the sublat name tag */
               node = newlat->lnodes + newNodes;
               arc = NumbLArc(newlat,newArcs);
               arc->start = latEnd;
               arc->end = node;
               latEnd->foll = arc;
               node->foll=NARC;
               node->pred=arc;
               node->word=GetWord(newlat->voc,nullNodeId,TRUE);
               node->tag=SafeCopyString(newlat->heap,thisNode->tag);
               latEnd = node;

               newNodes++; newArcs++;
            }
         }
         else {
            latStart = newlat->lnodes + nNodes +
               (FindLatStart(subLat)-subLat->lnodes);
            latEnd = newlat->lnodes + nNodes +
               +(FindLatEnd(subLat)-subLat->lnodes);
         }
         SubLattice(newlat,thisNode,latStart,latEnd);
         nNodes = newNodes; nArcs = newArcs;
      }
   }
}

/* CountNonNullNodes: count the nodes with a non-NULL word id */
static int CountNonNullNodes(Lattice *lat)
{
   int i,count=0;
   NodeId thisNode;

   for (i=0; i< lat->nn; i++) {
      thisNode = lat->lnodes+i;
      if (thisNode->word != &specialNull)
         count++;
   }
   return count;
}

/* EXPORT->ExpandMultiLevelLattice: Expand multi-level lattice lat  */
/*                                  into a single-level lattice.    */
Lattice *ExpandMultiLevelLattice(MemHeap *heap, Lattice *lat, Vocab *voc)
{
   Lattice *newlat;
   Lattice *final;
   int  nArcs, nNodes;
   int newArcs, newNodes;

   nNodes = nArcs = 0;
   ExpandedLatticeSize(lat,&nNodes,&nArcs);
   newlat = NewLattice(&gstack,nNodes,nArcs);
   newlat->voc = lat->voc;
   newArcs = newNodes = 0;
   CopyLattice(lat,newlat,&newNodes,&newArcs,FALSE);  /* copy the top level  */
   ExpandLattice(newlat,newNodes,newArcs);        /* expand all sub-lats */
   nNodes = CountNonNullNodes(newlat);
   final = NewILattice(heap,nNodes,nArcs,lat);
   newArcs = newNodes = 0;
   CopyLattice(newlat,final,&newNodes,&newArcs,TRUE); /* remove NULL id nodes */
   Dispose(&gstack,newlat);

   final->subList=NULL;  /* Actually unnecessary */

   return final;
}


/* ------------------------ Misc Lattice Ops ------------------------- */

/* EXPORT->LatticeFromLabels: Create lattice from label list for alignment */
Lattice *LatticeFromLabels(LabList *ll,LabId bnd,Vocab *voc,MemHeap *heap)
{
   Lattice *lat;
   LNode *ln;
   LArc *la;
   LLink l;
   LabId labid;
   int i,n,N;

   N=CountLabs(ll);  /* total number of symbols */
   n=1;              /* index of first word in list */
   if (bnd!=NULL) N+=2,n--;
   lat=NewLattice(heap,N,N-1);
   lat->voc=voc;
   for (i=1,ln=lat->lnodes;i<=N;i++,ln++,n++) {
      if (bnd!=NULL && (i==1 || i==N)) {
         labid=bnd;
      }
      else {
         l=GetLabN(ll,n);
         labid=l->labid;
      }
      /* Node */
      ln->n=i-1;
      if ((ln->word=GetWord(voc,labid,FALSE))==NULL)
         HError(8220,"LatticeFromLabels: Word %s not defined in dictionary",
			labid->name);
      ln->v=-1;
      ln->time=0.0;
      ln->foll=ln->pred=NULL;
      ln->score=0.0;
      ln->hook=NULL;
      if (i!=1) {
         /* Arc */
         la=NumbLArc(lat,i-2);
         la->start=ln-1;
         la->end=ln;
         la->lmlike=0.0;
         la->farc=la->parc=NULL;
         la->end->pred=la->start->foll=la;
      }
   }
   return(lat);
}


/* EXPORT->NumNodeFoll: return num outgoing arcs from node n */
int NumNodeFoll(NodeId n)
{
   int c = 0;
   ArcId a;

   for (a = n->foll; a != NARC; a=a->farc) ++c;
   return c;
}

/* EXPORT->NumNodePred: return num outgoing arcs from node n */
int NumNodePred(NodeId n)
{
   int c = 0;
   ArcId a;

   for (a = n->pred; a != NARC; a=a->parc) ++c;
   return c;
}


/* EXPORT->FindLatStart: find and return the lattice start node */
NodeId FindLatStart(Lattice *lat)
{
   int i;
   NodeId ln;

   for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      if (ln->pred==NARC)
         return ln;
		HError(8252,"FindLatStart: lattice has no start node");
		return(NARC);
}

/* EXPORT->FindLatEnd: find and return the lattice end node */
NodeId FindLatEnd(Lattice *lat)
{
   int i;
   NodeId ln;

   for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++)
      if (ln->foll==NARC)
         return ln;
		HError(8252,"FindLatEnd: lattice has no end node");
		return(NARC);
}


/* ====================================================================*/
/*                     PART TWO - NETWORKS                             */
/* ====================================================================*/

/*
  Networks are created directly from lattices (which may be of type
  shortArc to minimise lattice storage requirements).

  Cross word context dependent networks are created in an
  automagic manner from the hmmlist and monophone dictionary.

  Contexts
    -1 == context free - skip this phone when determining context
     0 == context independent - matches any context - or undefined
     n == unique context identifier

  Many labids may map to a single context (generalised contexts)
  or there may be single context per phone (triphone contexts)
  There is currently no way to determine automatically which
  context a model belongs to for generalised contexts.
*/

typedef struct pronholder
{
   LNode *ln;       /* Node that created this instance */
   Pron pron;       /* Actual pronunciation */
   short nphones;   /* Number of phones for this instance */
   LabId *phones;   /* Phone sequence for the instance */
   LogFloat fct;    /* LM likelihood to be factored into each phone */
   int ic;          /* Initial context - cache saves finding for all links */
   int fc;          /* Final context - cache saves finding for all links */
   Boolean fci;     /* Final phone context independent */
   Boolean tee;     /* TRUE if word consists solely of tee models */
   int clen;        /* Number of non-cf phones in pronunciation */
   NetNode **lc;    /* Left contexts - linked to word initial models */
   NetNode **rc;    /* Right contexts - linked to word end nodes */
   int nstart;      /* Number of models in starts chain */
   int nend;        /* Number of models in ends chain */
   NetNode *starts; /* Chain of initial models */
   NetNode *ends;   /* Chain of final models */
   NetNode *chain;  /* Chain of other nodes in word */
   struct pronholder *next;
} PronHolder;

typedef struct pinstinfo {
   Pron pron;
   int silId;
   int n;
   int t;
   LabId *phones;   /* derived list of phones for this instance */
} PInstInfo;

#define HCI_CXT_BLOCKSIZE 256

typedef struct buildinfo {   /* "static purging" by SJY 20/8/04 */
   /* stuff used during build process */
   NetNode *wnHashTab[WNHASHSIZE];    /* access to word end nodes */
   int nwi;          /* Word internal nodes */
   int nin;          /* Word initial nodes */
   int nfi;          /* word final nodes */
   int ncn;          /* chain Nodes  */
   int nll;          /* null nodes */
   int ncf;          /* context-free nodes */
   int nwe;          /* word end nodes  */
   int nil;          /* word-internal links */
   int nxl;          /* cross-word links  */
   int nnl;          /* null-word links  */
   MemHeap tmpStak;  /* temporary private stack */
}BuildInfo;

/* ------------------------- Debug print routines ------------------ */

/* PrintNode: print contents of given node in context of given hset */
static void PrintNode(NetNode *node,HMMSet *hset)
{
   printf("Node[%05d] ",(((unsigned) node)/sizeof(NetNode))%100000);
   if (node->type & n_hmm)
      printf("{%s}\n",HMMPhysName(hset,node->info.hmm));
   else if (node->type == n_word && node->info.pron==NULL) {
      printf("NULL");
      if (node->tag != NULL)
         printf(" ... tag=%s\n",node->tag);
      else
         printf("\n");
   }
   else if (node->type == n_word) {
      printf("%s",node->info.pron->word->wordName->name);
      if (node->tag != NULL)
         printf(" ... tag=%s\n",node->tag);
      else
         printf("\n");
   }
   else
      printf("{%d}\n",node->type);
   if (node->wordset) printf("Wordset = %p\n",node->wordset);
   fflush(stdout);

}

/* PrintLinks: print a list of node links */
static void PrintLinks(NetLink *links,int nlinks)
{
   int i;

   for (i=0; i<nlinks; i++) {
      printf("    %-2d: -> [%05d (%d)] == %7.3f\n",i,
			(((unsigned) links[i].node)/sizeof(NetNode)%100000),
			(links[i].node->type) & 017,links[i].linkLM);
      fflush(stdout);
   }
}

/* PrintChain: print all nodes in network in context of hset */
static void PrintChain(Network *wnet,HMMSet *hset)
{
   NetNode *thisNode;

	printf("Initial Node\n");
	PrintNode(&wnet->initial,hset);
	PrintLinks(wnet->initial.links,wnet->initial.nlinks);
	printf("\nFinal Node\n");
	PrintNode(&wnet->final,hset);
	PrintLinks(wnet->final.links,wnet->final.nlinks);
	printf("\nChain\n");
   thisNode = wnet->chain;
   while (thisNode != NULL) {
      PrintNode(thisNode,hset);
      PrintLinks(thisNode->links,thisNode->nlinks);
      thisNode = thisNode->chain;
   }
}

/* ----------------------- Utility Functions ------------------- */

/* InitBuildInfo: reset the word end node hash table and counters */
static void InitBuildInfo(BuildInfo *bi)
{
	int i;

   CreateHeap(&bi->tmpStak,"Temp Build Stack",MSTAK,1,0,8000,80000);
   for (i=0; i<WNHASHSIZE; i++) bi->wnHashTab[i]=NULL;
    bi->nwi = 0;
    bi->nin = 0;
    bi->nfi = 0;
    bi->ncn = 0;
    bi->nll = 0;
    bi->ncf = 0;
    bi->nwe = 0;
    bi->nil = 0;
    bi->nxl = 0;
    bi->nnl = 0;
}


/* IsWd0Link: true if link transits to a word node in zero time */
static Boolean IsWd0Link(NetLink *link)
{
   int i;
   NetNode *nextNode;

   nextNode = link->node;
   if ((nextNode->type&n_nocontext) == n_word)
      return TRUE;
   if (nextNode->type & n_tr0) {
      for (i = 0; i < nextNode->nlinks; i++)
         if (IsWd0Link(&nextNode->links[i]))
            return TRUE;
			return FALSE;
   } else
      return FALSE;
}

/* AddChain: add a chain of nodes to the front of the network */
static void AddChain(Network*net, NetNode *hd)
{
   NetNode *tl;

   if (hd == NULL) return;
   tl = hd;
   while (tl->chain != NULL) tl = tl->chain;
   tl->chain = net->chain;
   net->chain = hd;
}

/* BSearch: use binary search to find index of labid in n element array
           indexed 1..n-1. If not found, zero is returned */
static int BSearch(LabId labid, int n,LabId *array)
{
   int l,u,c;

   l=1;u=n;
   while(l<=u) {
      c=(l+u)/2;
      if (array[c]==labid) return(c);
      else if (array[c]<labid) l=c+1;
      else u=c-1;
   }
   return 0;
}

/* BAddSearch: use binary search to find labid in *np element array *ap
           if not found, the labid is added (which may increase size of *ap */
static int BAddSearch(HMMSetCxtInfo *hci,LabId labid, int *np,LabId **ap)
{
   LabId *array,*newId;
   int l,u,c;

   array=*ap;
	/* first see if labid already in ap */
	if (c = BSearch(labid,*np,*ap) > 0) return c;
	/* otherwise add it */
   if (((*np+1)%HCI_CXT_BLOCKSIZE)==0) {
		/* need to extend the array */
      newId=(LabId *) New(&gcheap,sizeof(LabId)*(*np+1+HCI_CXT_BLOCKSIZE));
      for (c=1;c<=*np;c++) newId[c]=array[c];
      Dispose(&gcheap,array);
      *ap=array=newId;
   }
   for (c=1;c<=*np;c++){
      if (labid<array[c]) break;
	}
	for (u=(*np)++;u>=c;u--)
		array[u+1]=array[u];
	array[c]=labid;
	return(c);
}

/* ---------------------- Context Handling routines ------------------ */

/* ClosedDict: return true if every model in dictionary appears explicitly
               in the model set (ie automagic expansion not necessary */
static Boolean ClosedDict(Vocab *voc,HMMSet *hset)
{
   Word word;
   Pron pron;
   int i,h;

   for (h=0; h<VHASHSIZE; h++)
      for (word=voc->wtab[h]; word!=NULL; word=word->next)
         for (pron=word->pron; pron!=NULL; pron=pron->next)
            for (i=0;i<pron->nphones;i++)
               if (FindMacroName(hset,'l',pron->phones[i])==NULL)
                  return(FALSE);
	return(TRUE);
}

/* NewHMMSetCxtInfo: create a HMMSetCxtInfo record */
static HMMSetCxtInfo *NewHMMSetCxtInfo(HMMSet *hset, Boolean frcCxtInd)
{
   HMMSetCxtInfo *hci;

   hci=(HMMSetCxtInfo *) New(&gcheap,sizeof(HMMSetCxtInfo));
   hci->hset=hset;
   hci->nc=hci->xc=hci->nci=hci->ncf=0;
   hci->sLeft=hci->sRight=FALSE;
   if (frcCxtInd) {
      hci->cxs=hci->cis=hci->cfs=NULL;
   } else {
      hci->cxs=(LabId *) New(&gcheap,sizeof(LabId)*HCI_CXT_BLOCKSIZE);
      hci->cis=(LabId *) New(&gcheap,sizeof(LabId)*HCI_CXT_BLOCKSIZE);
      hci->cfs=(LabId *) New(&gcheap,sizeof(LabId)*HCI_CXT_BLOCKSIZE);
      hci->cxs[0]=hci->cis[0]=hci->cfs[0]=GetLabId("<undef>",TRUE);
   }

   return(hci);
}

/* ShowHMMSetCxtInfo: print contents of a HMMSetCxtInfo record */
static void ShowHMMSetCxtInfo(HMMSetCxtInfo *hci)
{
	int i;

	printf(" CTX: %d contexts, %d xword contexts, %d ci models, %d cf models\n",
		hci->nc,hci->xc,hci->nci,hci->ncf);
	printf("      Left context %sseen, right context %sseen\n",
		hci->sLeft?"":"un", hci->sRight?"":"un");
	printf("      Contexts:");
	for (i=1; i<=hci->nc; i++) {
		printf(" %s",hci->cxs[i]->name);
		if (i%16 == 0) printf("\n               ");
	}
	printf("\n      Cxt Ind :");
	for (i=1; i<=hci->nci; i++){
		printf(" %s",hci->cis[i]->name);
		if (i%16 == 0) printf("\n               ");
	}
	printf("\n      Cxt Free:");
	for (i=1; i<=hci->ncf; i++) printf(" %s",hci->cfs[i]->name);
	printf("\n");
}

/* GetHCIContext: return context defined by given labid (after triphone
   context stripping): -1=cfree, 0=cind, n=actual context */
int GetHCIContext(HMMSetCxtInfo *hci,LabId labid)
{
   LabId cxt;
   char buf[80];
   int c;

   if (hci->nc==0) return(0);
   strcpy(buf,labid->name); TriStrip(buf);
   cxt=GetLabId(buf,FALSE);

   c=BSearch(cxt,hci->nc,hci->cxs);
   if (c>0) return(c);
   c=BSearch(cxt,hci->ncf,hci->cfs);
   if (c>0) {
      if (hci->xc>0 || !cfWordBoundary) return(-1);
      else return(0); /* Context free are word boundaries */
   }
   return(0);
}

/* IsHCIContextInd: return true if labid is context independent */
Boolean IsHCIContextInd(HMMSetCxtInfo *hci,LabId labid)
{
   int c;

   if (hci->nc==0) return(TRUE);
   c = BSearch(labid,hci->nci,hci->cis);
	return c>0;
}

/* DefineContexts: define contexts (cxs), identify context ind models (cis)
		and which ci models are also context free (cfs) from given hmmset */
static int DefineContexts(HMMSetCxtInfo *hci,BuildInfo *bi)
{
   MLink ml,il;
   LabId labid;
   char buf[80],*ptr;
   int h,c,*cdCount;

   hci->nc=0; hci->sLeft=hci->sRight=FALSE;
   /* Scan for all contexts that appear */
   for (h=0; h<MACHASHSIZE; h++){
      for (ml=hci->hset->mtab[h]; ml!=NULL; ml=ml->next){
         if (ml->type=='l') {
            strcpy(buf,ml->id->name); TriStrip(buf);
            labid=GetLabId(buf,FALSE);

            /* Start by adding all models to cis then check later if CI */
            BAddSearch(hci,labid,&hci->nci,&hci->cis);
            /* Check for left contexts */
            if (strchr(ml->id->name,'-')!=NULL) {
               strcpy(buf,ml->id->name); strchr(buf,'-')[0]=0;
               labid=GetLabId(buf,TRUE); hci->sLeft=TRUE;
               BAddSearch(hci,labid,&hci->nc,&hci->cxs);
            }
            /* Check for right contexts */
            if (strchr(ml->id->name,'+')!=NULL) {
               strcpy(buf,ml->id->name); ptr=strchr(buf,'+');
               labid=GetLabId(ptr+1,TRUE); hci->sRight=TRUE;
               BAddSearch(hci,labid,&hci->nc,&hci->cxs);
            }
         }
		}
	}
   /* remove all models from cis which have cd instances in hmm set */
	cdCount=(int *) New(&bi->tmpStak,sizeof(int)*hci->nci);cdCount--;
	for (c=1;c<=hci->nci;c++) cdCount[c]=0;
	for (h=0; h<MACHASHSIZE; h++){
		for (ml=hci->hset->mtab[h]; ml!=NULL; ml=ml->next){
			if (ml->type=='l') {
				strcpy(buf,ml->id->name);	TriStrip(buf);
				labid=GetLabId(buf,FALSE);
				il=FindMacroName(hci->hset,'l',labid);
				c = BSearch(labid,hci->nci,hci->cis);
				assert(c>0);
				if (il!=ml)cdCount[c]++;
			}
		}
	}
	for (c=1;c<=hci->nci;c++)
		if (cdCount[c]!=0) hci->cis[c]=NULL;
	Dispose(&bi->tmpStak,cdCount+1);
	for (c=1,h=1;c<=hci->nci;c++,h++) {
		for (;h<=hci->nci;h++) if (hci->cis[h]!=NULL) break;
		if (h>hci->nci) break;
		hci->cis[c]=hci->cis[h];
	}
	hci->nci=c-1;
	/* Any model now in cis but not in cxs is context free, so copy it to cfs */
	for (h=0; h<MACHASHSIZE; h++)
		for (ml=hci->hset->mtab[h]; ml!=NULL; ml=ml->next)
			if (ml->type=='l') {
				c=GetHCIContext(hci,ml->id);
				if (c==0) {
					if (!IsHCIContextInd(hci,ml->id))
						HError(8230,"DefineContexts: Context free models must be context independent (%s)",ml->id->name);
					BAddSearch(hci,ml->id,&hci->ncf,&hci->cfs);
				}
			}
	return(hci->nc);
}

/* GetHMMSetCxtInfo: get context for a HMMSet, if already stored in the hset as a
   macro then just retrieve it, otherwise construct a new HMMSetCxtInfo struct and
	add it to hset */
HMMSetCxtInfo *GetHMMSetCxtInfo(HMMSet *hset, Boolean frcCxtInd, BuildInfo *bi)
{
   HMMSetCxtInfo *hci;
   LabId labid;
   MLink ml;

   if (frcCxtInd)
      labid=GetLabId("@HCI-CI@",TRUE);
   else
      labid=GetLabId("@HCI-CD@",TRUE);
   ml=FindMacroName(hset,'@',labid);
   if (ml==NULL) {
      hci=NewHMMSetCxtInfo(hset,frcCxtInd);
      if (!frcCxtInd) DefineContexts(hci,bi);
      NewMacro(hset,0,'@',labid,hci);
   } else
      hci=(HMMSetCxtInfo *) ml->structure;
   return(hci);
}

/* FindLContext: search through pron for left context for phone in position pos
   skipping over any context free phones.  If start of word is reached return lc */
static int FindLContext(HMMSetCxtInfo *hci,PronHolder *p, int pos, int lc)
{
   int i,c;

   for (i=pos-1,c=-1;i>=0;i--){
      if ((c=GetHCIContext(hci,p->phones[i]))>=0) break;
	}
	if (c<0) c=lc;
	return(c);
}

/* FindRContext: search through pron for right context for phone in position pos
   skipping over any context free phones.  If end of word is reached return rc */
static int FindRContext(HMMSetCxtInfo *hci, PronHolder *p, int pos, int rc)
{
   int i,c;

   for (i=pos+1,c=-1;i<p->nphones;i++){
      if ((c=GetHCIContext(hci,p->phones[i]))>=0) break;
	}
	if (c<0) c=rc;
	return(c);
}

/* ContextName: return labid for given context c, error if c not a real context */
static LabId ContextName(HMMSetCxtInfo *hci, int c)
{
   if (c<0 || c>hci->nc)
      HError(8290,"ContextName: Context %d not defined (-1..%d)",c,hci->nc);

   return(hci->cxs[c]);
}

/* FindModel: find and return model [lc]-name+[rc] with given context */
static HLink FindModel(HMMSetCxtInfo *hci,int lc,LabId name,int rc, LabId *logname)
{
   LabId labid;
   MLink ml;
   char buf[80];

   if (trace&T_FND) {
      printf("FindModel: %d-%s+%d  \n",lc,name->name,rc);
      ShowHMMSetCxtInfo(hci);
   }
   /* First try constructing the cd name */
   if (!allowCxtExp || (lc<=0 && rc<=0) || IsHCIContextInd(hci,name)) {
      strcpy(buf,name->name); labid=name;
   } else if ((lc==0 || forceRightBiphones || !hci->sLeft) &&
		         rc>0 && !forceLeftBiphones) {
      sprintf(buf,"%s+%s",name->name,ContextName(hci,rc)->name);
      labid=GetLabId(buf,TRUE);
   } else if ((rc==0 || forceLeftBiphones || !hci->sRight) &&
		         lc>0 && !forceRightBiphones) {
      sprintf(buf,"%s-%s",ContextName(hci,lc)->name,name->name);
      labid=GetLabId(buf,TRUE);
   } else if (!forceLeftBiphones && !forceRightBiphones) {
      sprintf(buf,"%s-%s+%s",ContextName(hci,lc)->name,
			name->name,ContextName(hci,rc)->name);
      labid=GetLabId(buf,TRUE);
   } else{
      strcpy(buf, name->name); labid=name;
   }
   ml=GetLogHMMMacro(hci->hset,labid);
   if(logname != NULL) *logname=labid;
   if (trace&T_FND) {
      printf(" Constructed name %s %s found\n",labid->name,(ml==NULL)?"not":"");
   }
   /* Otherwise, try the name itself */
   if (ml==NULL && (((lc==0 && rc==0) || !forceCxtExp) ||
	   (lc==0 || !forceLeftBiphones) || (rc==0 || !forceRightBiphones))) {
		   ml=FindMacroName(hci->hset,'l',name);
		   if(logname != NULL) *logname = name;
		   if (trace&T_FND) {
			   printf(" Actual name %s %s found\n",name,(ml==NULL)?"not":"");
		   }
	   }
   if (ml==NULL) return(NULL);
   return((HLink) ml->structure);
}

/* ------------------------ Node Find and Create Routines ---------------------- */

/* NewNullNode: create new Null NetNode */
static NetNode *NewNullNode(MemHeap *heap)
{
   NetNode *node;

   node=(NetNode *)New(heap,sizeof(NetNode));
   node->nlinks=0;  node->links=NULL;
   node->inst=NULL; node->type=n_word;
   node->info.pron=NULL; node->tag=NULL;
	node->wordset = NULL;
   node->onePred = TRUE;
   node->newNetNode = NULL;
   return(node);
}

/* NewHMMNode: create new HMM NetNode (and optionally NetLinks as well) */
static NetNode *NewHMMNode(MemHeap *heap, HLink hmm,
                           int nlinks, LabId logname,PronHolder *pInst)
{
   NetNode *node;

   node=(NetNode *)New(heap,sizeof(NetNode));
   node->type=(hmm->transP[1][hmm->numStates]>LSMALL?
		(n_hmm|n_tr0) : n_hmm );
   node->info.hmm=hmm;
   node->inst=NULL; node->chain=NULL;
   node->nlinks=nlinks;
   node->tag=(logname)?logname->name:NULL;
   if (pInst != NULL && pInst->pron != NULL && pInst->pron->word !=NULL)
      node->wordset = pInst->pron->word->wordName;
   node->onePred = TRUE;
   node->newNetNode = NULL;
   if (nlinks==0)
      node->links=NULL;
   else
      node->links=(NetLink*) New(heap,sizeof(NetLink)*node->nlinks);
   return(node);
}

/* FindWordNode: use hash table to lookup word end node corresponding to
   given given Pron, ptr to PronHolder and type.  If not found create it.
   Note that type is always n_word but may have context encoded in it  */
static NetNode *FindWordNode(BuildInfo *bi, MemHeap *heap, Pron pron,
                             PronHolder *pInst,NetNodeType type)
{
   union {
      Ptr ptrs[3];
      unsigned char chars[12];
   } un;
   unsigned int hash,i;
   NetNode *node;

   hash=0;
   un.ptrs[0]=pron; un.ptrs[1]=pInst; un.ptrs[2]=(Ptr)type;
   for (i=0;i<12;i++)
      hash=((hash<<8)+un.chars[i])%WNHASHSIZE;

   for (node=bi->wnHashTab[hash];node!=NULL;node=node->chain) {
      if (node->info.pron==pron && node->inst==(NetInst*)pInst &&
			node->type==type) break;
	}
	if (node==NULL) {  /* create a new word end node */
		bi->nwe++;
		if (heap==NULL)
			HError(8291,"FindWordNode: Node %s[%d] %d not created",
			pron->word->wordName->name,pron->pnum,type);
		node=(NetNode *) New(heap,sizeof(NetNode));
		node->info.pron=pron;  node->type=type;
		node->inst=(NetInst*)pInst;
		node->nlinks=0; node->links=NULL;
		node->tag=NULL;
      node->onePred = pInst->ln->onePred;
      node->newNetNode = NULL;
      node->wordset = pron->word->wordName;
      if (node->wordset == nullNodeId) node->wordset = NULL;
		node->chain=bi->wnHashTab[hash];
		bi->wnHashTab[hash]=node;
	}
	return(node);
}

/* CopyOnlyNode: copy only the node but not the forward links */
static NetNode *CopyOnlyNode(MemHeap *heap, NetNode *orig)
{
   NetNode *node;

   node = (NetNode *)New(heap,sizeof(NetNode));
   node->type = orig->type;
   node->info.hmm = orig->info.hmm;
   node->inst = orig->inst;
   node->chain = NULL;
   node->nlinks = 0;
   node->wordset = orig->wordset;
   node->tag = orig->tag;
   node->onePred = orig->onePred;
   node->newNetNode = NULL;
   orig->newNetNode = node;
   node->links = NULL;
   return(node);
}

/* ---------------------------------------------------------------------- */

/* IsRContextInd: determine if phone in position pos is independent of right context */
static Boolean IsRContextInd(HMMSetCxtInfo *hci, PronHolder *p, int pos)
{
   LabId labid=NULL;
   HLink hmm,cmp;
   int i,j,lc;

   for (i=pos-1;i>=0;i--){
      if (GetHCIContext(hci,p->phones[i])>=0) {
         labid=p->phones[i]; break;
      }
	}
	if (labid!=NULL) {
		lc=FindLContext(hci,p,i,-1);
		if (lc==-1) {
			return(IsHCIContextInd(hci,labid));
		}
		else {
			hmm=NULL;
			for (j=1;j<hci->nc;j++) {
				cmp=FindModel(hci,lc,labid,j,NULL);
				if (hmm==NULL) hmm=cmp;
				else if (cmp!=hmm)
					return(FALSE);
			}
			return(TRUE);
		}
	}
	else {
		for (i=pos-1;i>=0;i--)
			if (!IsHCIContextInd(hci,p->phones[i])) break;
			if (i<0) return(TRUE);
	}

	HError(8290,"IsRContextInd: Context check not possible for %s",
		p->pron->outSym==NULL?p->pron->word->wordName->name:
	p->pron->outSym->name);
	return(FALSE);
}

/* Determine if dictionary voc be constructed solely from word internal */
/* models.  Notionally checks for cross word/word internal differences */
static Boolean InternalDict(Vocab *voc,HMMSetCxtInfo *hci)
{
   Word word;
   Pron pron;
   int i,j,h,lc,rc;

   for (h=0; h<VHASHSIZE; h++)
     for (word=voc->wtab[h]; word!=NULL; word=word->next)
       for (pron=word->pron; pron!=NULL; pron=pron->next) {
	 for (i=0;i<pron->nphones;i++) {
	   for (j=i-1,lc=-1;j>=0;j--)
	     if ((lc=GetHCIContext(hci,pron->phones[j]))>=0)
	       break;
	   for (j=i+1,rc=-1;j<pron->nphones;j++)
	     if ((rc=GetHCIContext(hci,pron->phones[j]))>=0)
	       break;
	   if (lc<0) lc=0; if (rc<0) rc=0;
	   if (FindModel(hci,lc,pron->phones[i],rc,NULL)==NULL)
	     return(FALSE);
	 }
       }
   return(TRUE);
}

/* ------------------------- PronHolder Routines ------------------------ */

/* SetNullWord: check if !NULL is user-defined, otherwise create it */
static void SetNullWord(Network *net,Vocab *voc)
{
	Pron thisPron;

   net->nullWord = GetWord(voc,nullNodeId,TRUE);
   for (thisPron=net->nullWord->pron;thisPron!=NULL;thisPron=thisPron->next){
      if (thisPron->nphones!=0) {
         net->nullWord=NULL;  break;
      }
	}
	if (net->nullWord!=NULL) {
		if (net->nullWord->pron==NULL)
			NewPron(voc,net->nullWord,0,NULL,net->nullWord->wordName,1.0);
	}
}

/* NewPronHolder: allocate and initialise a PronHolder */
static PronHolder *NewPronHolder(MemHeap *heap, HMMSetCxtInfo *hci,
                                 Pron thisPron, int np, LabId *phones)
{
   PronHolder *pInst;
   int n;

   pInst = (PronHolder *) New(heap,sizeof(PronHolder));
   pInst->pron = thisPron;
   pInst->nphones=np; pInst->phones=phones;
   pInst->clen=0;
   pInst->nstart=pInst->nend=0;
   pInst->starts=pInst->ends=NULL;
   pInst->chain=NULL;
   pInst->tee=FALSE;  /* Empty words are !NULL */
	pInst->lc=pInst->rc=NULL;
   if (hci->xc>0) {
      pInst->lc = (NetNode**) New(heap,sizeof(NetNode*)*hci->xc);
      pInst->rc = (NetNode**) New(heap,sizeof(NetNode*)*hci->xc);
      for (n=0;n<hci->xc;n++) pInst->lc[n]=pInst->rc[n]=NULL;
   }
   if (np==0) {
      pInst->ic = pInst->fc = -1;
      pInst->fci=FALSE;
   } else if (hci->xc>0) {
      pInst->fc=FindLContext(hci,pInst,pInst->nphones,-1);
      pInst->ic=FindRContext(hci,pInst,-1,-1);
      for (n=0;n<pInst->nphones;n++)
         if (GetHCIContext(hci,pInst->phones[n])>=0)  pInst->clen++;
		if (pInst->clen==0 || pInst->fc==-1 || pInst->ic==-1)
			HError(8230,"NewPronHolder: Every word must define some context [%s=%d/%d/%d]",
				          thisPron->outSym->name,pInst->ic,pInst->clen,pInst->fc);
		pInst->fci=IsRContextInd(hci,pInst,pInst->nphones);
   }
   return(pInst);
}

/* InitPronHolders: create pronholders and chain them to the sublat field of
   the corresponding lattice word node. The ADDSILPHONES config var can be used
   to append/delete trailing silence phones */
static int InitPronHolders(Network *net,Lattice *lat,HMMSetCxtInfo *hci,
                     BuildInfo *bi, Vocab *voc,MemHeap *heap,char *frcSil)
{
   PronHolder *pInst;
   NetNode *wordNode;
   Pron thisPron;
   Word thisWord;
   LNode *thisLNode;
   PInstInfo *pii;
   LabId silPhones[MAXPHONES],addPhones[MAXPHONES],labid;
   int i,j,k,l,n,lc,type,nNull,npii,nSil,nAdd;
	int numDupsRemoved;
   char *ptr,*p,*nxt,name[MAXSTRLEN],st;

	/*  ResetWNHashTab();  now done in InitBuildInfo SJY 20/8/04*/
	SetNullWord(net,voc);

	/* If config defined sil phones, extract names from config variable */
	nSil=nAdd=0;
 	if (frcSil!=NULL && strlen(frcSil)>0) {
		for(nSil=nAdd=0,ptr=frcSil;ptr!=NULL;ptr=nxt) {
			if ((nxt=ParseString(ptr,name))==NULL) break;
			st=0; p=name;
			if (name[0]=='+' || name[0]=='-') {
				st=name[0]; p=name+1;
			}
			if (strlen(p)==0) labid=NULL; else labid=GetLabId(p,TRUE);
			if (st=='+' || st==0) addPhones[++nAdd]=labid;
			if (st=='-' || st==0) silPhones[++nSil]=labid;
		}
	}

	/* Factor the LM scores if required */
	for (i=0; i<lat->nn; i++) {
		float fct;
		LArc *la;

		thisLNode = lat->lnodes+i;
		fct = 0.0;
		if (factorLM && thisLNode->pred!=NULL)
			for (la=thisLNode->pred,fct=LZERO;la!=NULL;la=la->parc)
				if (la->lmlike>fct) fct=la->lmlike;
		thisLNode->score = fct;   /* ie largest predecessor link prob */
	}
	if (factorLM)
		for (i=0; i<lat->na; i++) {
			LArc *la;
			la=NumbLArc(lat,i);
			la->lmlike -= la->end->score;
		}

	/* Create instance for each pronunciation in lattice */
	numDupsRemoved=0; nNull=0;
	for (i=0; i < lat->nn; i++) {
		thisLNode = lat->lnodes+i;
		thisWord = thisLNode->word;
		if (thisWord==NULL) thisWord=voc->nullWord;
		if (thisWord==voc->subLatWord)
			HError(8220,"InitPronHolders: Expand lattice before making network");
		thisLNode->sublat=NULL;
		if (thisWord->nprons<=0)
			HError(8220,"InitPronHolders: Word %s not defined in dictionary",
			             thisWord->wordName->name);

		pii=(PInstInfo *) New(&bi->tmpStak,(thisWord->nprons+1)*(nAdd+1)*sizeof(PInstInfo));
		pii--;
		/* Scan current pronunciations and make modified ones */
		for (j=1,thisPron=thisWord->pron,npii=0; thisPron!=NULL;
		     j++,thisPron=thisPron->next) {

			n=thisPron->nphones;
			/* remove any trailing phones listed in silPhones */
			if (n>0) {
				for (k=1; k<=nSil; k++)
					if (thisPron->phones[thisPron->nphones-1]==silPhones[k]) {
						n--; break;  /* Strip it */
					}
			}
			if (thisPron->nphones==0 || nAdd==0 || n==0) {
				/* Just need one pronunciation */
				if (thisPron->nphones==0) {
					if (thisWord!=net->nullWord && (trace&T_CXT))
						printf("InitPronHolders: Word %s has !NULL pronunciation\n",
						thisWord->wordName->name);
					nNull++;
				}
				if (n==0) n=thisPron->nphones;
				pii[++npii].pron=thisPron; pii[npii].silId=-1;
				pii[npii].n=n;  pii[npii].t=n;
				pii[npii].phones=thisPron->phones;
			}
			else {
				/* Make one instance per silence label */
				for (k=1;k<=nAdd;k++) {
					pii[++npii].pron=thisPron;
					pii[npii].silId=k;
					pii[npii].n = pii[npii].t = n;
					if (addPhones[k]!=NULL)  pii[npii].t++;
					pii[npii].phones=(LabId *) New(heap,sizeof(LabId)*pii[npii].t);
					for(l=0;l<pii[npii].n;l++)
						pii[npii].phones[l]=pii[npii].pron->phones[l];
					if (addPhones[k]!=NULL)
						pii[npii].phones[pii[npii].n]=addPhones[k];
				}
			}
		}
		/* Scan new pronunciations and mark duplicates for removal */
		if (remDupPron)
			for (j=2; j<=npii; j++) {
				n=pii[j].t;
				if (pii[j].pron==NULL) continue;
				for (k=1; k<j; k++) {
					if (pii[j].pron==NULL || pii[k].pron==NULL ||
						pii[k].t!=n || pii[j].pron->prob!=pii[k].pron->prob)
						continue;
					for(l=0;l<n;l++)
						if (pii[j].phones[l]!=pii[k].phones[l]) break;
					if (l==n) { pii[j].pron=NULL; numDupsRemoved++; }
				}
			}
		/* Now make the PronHolders */
		for (j=1; j<=npii; j++) {
			if (pii[j].pron==NULL) continue;  /* Don't add duplicates */
			/* Build inst for each pron */
			pInst=NewPronHolder(heap,hci,pii[j].pron,pii[j].t,pii[j].phones);
			pInst->ln = thisLNode;
			pInst->next = (PronHolder*)thisLNode->sublat;
			thisLNode->sublat = (SubLatDef*) pInst;
			if (pInst->nphones<=0) pInst->fct = 0.0;
			else pInst->fct = thisLNode->score/pInst->nphones;
			/* Fake connections from SENT_[START/END] */
			if (hci->xc>0) {
				if (thisLNode->pred==NULL)
					pInst->lc[0]=(NetNode*)lat;
				if (thisLNode->foll==NULL) {
					if (pInst->nphones==0) lc=0;
					else lc = pInst->fc;
					type = n_word + lc*n_lcontext; /* rc==0 */
					wordNode=FindWordNode(bi,net->heap,pInst->pron,pInst,type);
					wordNode->tag=SafeCopyString(net->heap,thisLNode->tag);
					wordNode->nlinks = 0;
					pInst->rc[0]=wordNode;
				}
			}
			else if (thisLNode->foll==NULL) {
				wordNode = FindWordNode(bi,net->heap,pInst->pron,pInst,n_word);
				wordNode->tag=SafeCopyString(net->heap,thisLNode->tag);
				wordNode->nlinks = 0;
			}
		}
		Dispose(&bi->tmpStak,++pii);
	}
	if (numDupsRemoved!=0)
		HError(-8221,"InitPronHolders: Total of %d duplicate pronunciations removed",numDupsRemoved);
	return(nNull);
}

/* Find model matching context & set logname to actual logical model name */
HLink GetHCIModel(HMMSetCxtInfo *hci,int lc,LabId name,int rc, LabId *logname)
{
   HLink hmm;

   hmm=FindModel(hci,lc,name,rc,logname);
   if (hmm==NULL)
      HError(8231,"GetHCIModel: Cannot find hmm [%s-]%s[+%s]",
		(lc>0?ContextName(hci,lc)->name:"???"),name->name,
		(rc>0?ContextName(hci,rc)->name:"???"));
   return(hmm);
}

/* AddInitialFinal: Add links to/from initial/final net nodes */
static void AddInitialFinal(BuildInfo *bi, Lattice *wnet, Network *net,int xc)
{
   PronHolder *pInst;
   NetNode *node;
   LNode *thisLNode;
   int ninitial = 0;
   int i,type;

   /* Simple case */
   for (i=0; i < wnet->nn; i++) {
      if (wnet->lnodes[i].pred == NULL) {
         for (pInst=(PronHolder*)wnet->lnodes[i].sublat;
              pInst!=NULL; pInst=pInst->next) ninitial++;
      }
   }
   if (ninitial==0)
      HError(8232,"AddInitialFinal: No initial links found");
   net->initial.onePred = TRUE;
   net->initial.newNetNode = NULL;
   net->initial.type = n_word;
   net->initial.tag = NULL;
   net->initial.info.pron = NULL;
   net->initial.nlinks = 0;
   net->initial.wordset = NULL;
   net->initial.links = (NetLink *) New(net->heap,ninitial*sizeof(NetLink));
   for (i=0,thisLNode=wnet->lnodes; i<wnet->nn; i++,thisLNode++) {
      if (thisLNode->pred != NULL) continue;
      for (pInst=(PronHolder*)thisLNode->sublat;
           pInst!=NULL;pInst=pInst->next) {
         if (xc==0)
            node=pInst->starts;
         else if (pInst->nphones!=0)
            node=pInst->lc[0];
         else
            node=FindWordNode(bi,NULL,pInst->pron,pInst,n_word);
         net->initial.links[net->initial.nlinks].node = node;
         net->initial.links[net->initial.nlinks++].linkLM = 0.0;
      }
   }
   net->final.onePred = FALSE;
   net->final.newNetNode = NULL;
   net->final.type = n_word;
   net->final.info.pron = NULL;
   net->final.tag = NULL;
   net->final.nlinks = 0; net->final.wordset = NULL;
   net->final.links = NULL;
   for (i=0; i < wnet->nn; i++) {
      thisLNode = wnet->lnodes+i;
      if (thisLNode->foll != NULL) continue;
      for (pInst=(PronHolder*)thisLNode->sublat;
           pInst!=NULL;pInst=pInst->next) {
         if (xc==0 || pInst->nphones==0)
            node=FindWordNode(bi,NULL,pInst->pron,pInst,n_word);
         else {
            type = n_word + pInst->fc*n_lcontext; /* rc==0 */
            node=FindWordNode(bi,NULL,pInst->pron,pInst,type);
         }

         if (node->nlinks>0)
            HError(8232,"AddInitialFinal: End node already connected");
         node->nlinks = 1;
         node->links = (NetLink *)New(net->heap,sizeof(NetLink));
         node->links[0].node = &net->final;
         node->links[0].linkLM = 0;
      }
   }
}


#define MAX_DEPTH 10 /* Max number of !NULL !NULL links before assuming loop */

void SetNullLRecurse(PronHolder *pInst,Lattice *lat, int xc, int *depth)
{
   PronHolder *lInst;
   LNode *thisLNode;
   LArc *la;
   int lc;


   if (++(*depth)>MAX_DEPTH)
      HError(8232,"SetNullLRecurse: Net probably has loop containing just !NULL");
   thisLNode=pInst->ln;
   for(la=thisLNode->pred;la!=NULL;la=la->parc)
      for (lInst=(PronHolder*)la->start->sublat;lInst!=NULL;lInst=lInst->next)
         if (lInst->nphones==0)
            SetNullLRecurse(lInst,lat,xc,depth);
			for(la=thisLNode->pred;la!=NULL;la=la->parc)
				for (lInst=(PronHolder*)la->start->sublat;
				lInst!=NULL;lInst=lInst->next) {
					if (lInst->nphones!=0) continue;
					for(lc=0;lc<xc;lc++) {
						if (lInst->lc[lc]!=NULL && pInst->lc[lc]==NULL)
							pInst->lc[lc]=(NetNode*)lat;
					}
				}
				(*depth)--;
}

void SetNullRRecurse(PronHolder *pInst,Lattice *lat,int xc,int *depth)
{
   PronHolder *rInst;
   LNode *thisLNode;
   LArc *la;
   int rc;

   if (++(*depth)>MAX_DEPTH)
      HError(8232,"SetNullRRecurse: Net probably has loop containing just !NULL");
   thisLNode=pInst->ln;
   for(la=thisLNode->foll;la!=NULL;la=la->farc)
      for (rInst=(PronHolder*)la->end->sublat;rInst!=NULL;rInst=rInst->next)
         if (rInst->nphones==0)
            SetNullRRecurse(rInst,lat,xc,depth);
			for(la=thisLNode->foll;la!=NULL;la=la->farc)
				for (rInst=(PronHolder*)la->end->sublat;rInst!=NULL;rInst=rInst->next) {
					if (rInst->nphones!=0) continue;
					for(rc=0;rc<xc;rc++) {
						if (rInst->rc[rc]!=NULL && pInst->rc[rc]==NULL)
							pInst->rc[rc]=(NetNode*)lat;
					}
				}
				(*depth)--;
}

void SetNullContexts(Lattice *lat,int xc)
{
   PronHolder *lInst,*rInst,*pInst;
   LNode *thisLNode;
   LArc *thisLArc;
   Boolean doPairs;
   int i,lc,rc;
	int ldepth =0,rdepth=0;

   doPairs=FALSE;
   for (i=0; i<lat->na; i++) {
      thisLArc = NumbLArc(lat, i);
      for (lInst=(PronHolder*)thisLArc->start->sublat;
		lInst!=NULL;lInst=lInst->next)
         for (rInst=(PronHolder*)thisLArc->end->sublat;
			rInst!=NULL;rInst=rInst->next) {
            if (rInst->nphones==0 && lInst->nphones==0) {
               doPairs=TRUE;
               rInst->fci=lInst->fci=TRUE;
            }
            else if (rInst->nphones==0) {
               lc = lInst->fc;
               rInst->lc[lc]=(NetNode*)lat; /* never returned by New */
            }
            else if (lInst->nphones==0) {
               rc = rInst->ic;
               lInst->rc[rc]=(NetNode*)lat; /* never returned by New */
            }
         }
   }
   if (doPairs) {
      for (i=0; i < lat->nn; i++) {
         thisLNode = lat->lnodes+i;
         for(pInst=(PronHolder*)thisLNode->sublat;
			pInst!=NULL;pInst=pInst->next) {
            if (pInst->nphones!=0 || !pInst->fci) continue;
            SetNullLRecurse(pInst,lat,xc,&ldepth);
            SetNullRRecurse(pInst,lat,xc,&rdepth);
         }
      }
   }
}

/* Process the cross word links, the first time heap!=NULL and */
/*  the links are counted and wordnodes created, the second    */
/*  time heap==NULL and link likelihoods/destinations are set. */
void ProcessCrossWordLinks(BuildInfo *bi, MemHeap *heap,Lattice *lat,int xc)
{
   PronHolder *lInst,*rInst;
   NetNode *wordNode;
   LArc *thisLArc;
   int i,lc,rc,type;

   /*  Currently a new word end is created for all logical contexts */
   /*  This is only needed for single phone words for which several */
   /*  models (in different contexts) connect to a single word end. */
   /*  For multi-phone words no models are shared and so a single   */
   /*  word end per distinct physical model would be fine.          */
   for (i=0; i<lat->na; i++) {
      thisLArc = NumbLArc(lat, i);
      for (lInst=(PronHolder*)thisLArc->start->sublat;
		lInst!=NULL;lInst=lInst->next)
         for (rInst=(PronHolder*)thisLArc->end->sublat;
			rInst!=NULL;rInst=rInst->next) {
            if (xc==0) {
               wordNode = FindWordNode(bi,heap,lInst->pron,lInst,n_word);
               if (heap!=NULL)
                  wordNode->tag=SafeCopyString(heap,thisLArc->start->tag);
               if (heap==NULL) {
                  wordNode->links[wordNode->nlinks].node=rInst->starts;
                  wordNode->links[wordNode->nlinks].linkLM=thisLArc->lmlike;
               }
               wordNode->nlinks++;
            }
            else if (rInst->nphones==0 && lInst->nphones==0) {
               for (lc=0;lc<xc;lc++) {
                  if (lInst->lc[lc]==NULL || rInst->lc[lc]==NULL)
                     continue;
                  for (rc=0;rc<xc;rc++) {
                     if (lInst->rc[rc]==NULL || rInst->rc[rc]==NULL)
                        continue;
                     type = n_word + lc*n_lcontext + rc*n_rcontext;
                     wordNode=FindWordNode(bi,heap,lInst->pron,lInst,type);
                     if (heap!=NULL)
                        wordNode->tag=SafeCopyString(heap,
								thisLArc->start->tag);
                     if (heap==NULL) {
                        if (rInst->ln->foll==NULL)
                           type = n_word;
                        wordNode->links[wordNode->nlinks].node=
                           FindWordNode(bi,heap,rInst->pron,rInst,type);
                        wordNode->links[wordNode->nlinks].linkLM=
                           thisLArc->lmlike;
                     }
                     wordNode->nlinks++;
                  }
               }
            }
            else if (rInst->nphones==0) {
               if (thisLArc->end->foll==NULL) lc=0;
               else lc = lInst->fc;
               if (lInst->fci) {
                  type = n_word + lc*n_lcontext;
                  wordNode=FindWordNode(bi,heap,lInst->pron,lInst,type);
                  if (heap!=NULL)
                     wordNode->tag=SafeCopyString(heap,thisLArc->start->tag);
               }
               else
                  wordNode=NULL; /* Keeps the compiler happy */
               for (rc=0;rc<xc;rc++) {
                  if (rInst->rc[rc]==NULL) continue;
                  if (!lInst->fci) {
                     type = n_word + lc*n_lcontext + rc*n_rcontext;
                     wordNode=FindWordNode(bi,heap,lInst->pron,lInst,type);
                     if (heap!=NULL)
                        wordNode->tag=SafeCopyString(heap,
								thisLArc->start->tag);
                  }
                  if (heap==NULL) {
                     type = n_word + lc*n_lcontext + rc*n_rcontext;
                     wordNode->links[wordNode->nlinks].node=
                        FindWordNode(bi,heap,rInst->pron,rInst,type);
                     wordNode->links[wordNode->nlinks].linkLM=thisLArc->lmlike;
                  }
                  else
                     lInst->rc[rc]=wordNode;
                  wordNode->nlinks++;
               }
            }
            else if (lInst->nphones==0) {
               if (thisLArc->start->pred==NULL) rc=0;
               else rc = rInst->ic;
               for (lc=0;lc<xc;lc++) {
                  if (lInst->lc[lc]==NULL) continue;
                  type = n_word + lc*n_lcontext + rc*n_rcontext;
                  wordNode=FindWordNode(bi,heap,lInst->pron,lInst,type);
                  if (heap!=NULL)
                     wordNode->tag=SafeCopyString(heap,
							thisLArc->start->tag);
                  if (heap==NULL) {
                     wordNode->links[wordNode->nlinks].node=rInst->lc[lc];
                     wordNode->links[wordNode->nlinks].linkLM=thisLArc->lmlike;
                  }
                  else
                     rInst->lc[lc]=(NetNode*)lat;
                  wordNode->nlinks++;
               }
            }
            else {
               lc = lInst->fc;
               rc = rInst->ic;
               if (lInst->fci) type = n_word + lc*n_lcontext;
               else type = n_word + lc*n_lcontext + rc*n_rcontext;
               wordNode = FindWordNode(bi,heap,lInst->pron,lInst,type);
               if (heap!=NULL)
                  wordNode->tag=SafeCopyString(heap,
						thisLArc->start->tag);
               if (heap==NULL) {
                  wordNode->links[wordNode->nlinks].node=rInst->lc[lc];
                  wordNode->links[wordNode->nlinks].linkLM=thisLArc->lmlike;
               }
               else {
                  lInst->rc[rc]=wordNode;
                  rInst->lc[lc]=(NetNode*)lat;
               }
               wordNode->nlinks++;
            }
         }
   }
}


void CreateWIModels(BuildInfo *bi, PronHolder *pInst,int p,int q,
                    Network *net,HMMSetCxtInfo *hci)
{
   NetNode *node;
   HLink hmm;
   int j;
	LabId logname;

   for(j=q-1;j>p;j--) {
      hmm=GetHCIModel(hci,FindLContext(hci,pInst,j,0),
			pInst->phones[j],
			FindRContext(hci,pInst,j,0),&logname);
      if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;

      bi->nwi++;
      node=NewHMMNode(net->heap,hmm,(pInst->chain==NULL?0:1),logname,pInst);
      if (pInst->chain!=NULL) {
         bi->nil++;
         node->links[0].node=pInst->chain;
         node->links[0].linkLM=pInst->fct;
      }
      node->chain=pInst->chain;
      pInst->chain=node;
   }
}

void CreateIEModels(BuildInfo *bi,Word thisWord,PronHolder *pInst,int p,int q,
                    Network *net,HMMSetCxtInfo *hci)
{
   NetNode *node,*wordNode;
   HLink hmm;
   LabId logname;

   if (q==p) {
      /* One phone word */
      hmm=GetHCIModel(hci,0,pInst->phones[0],0,&logname);
      if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;

      wordNode = FindWordNode(bi,NULL,pInst->pron,pInst,n_word);

      bi->nin++; bi->nil++;
      node = NewHMMNode(net->heap,hmm,1,logname,pInst);
      node->links[0].node=wordNode;
      node->links[0].linkLM=pInst->fct;
      node->onePred = pInst->ln->onePred;
      node->newNetNode = NULL;
      pInst->starts=node;
      pInst->nstart=1;
   }
   else {
      /* End */
      hmm=GetHCIModel(hci,FindLContext(hci,pInst,q,0),
			pInst->phones[q],0,&logname);
      if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;

      wordNode = FindWordNode(bi,NULL,pInst->pron,pInst,n_word);

      bi->nfi++; bi->nil++;
      node=NewHMMNode(net->heap,hmm,1,logname,pInst);
      node->links[0].node=wordNode;
      node->links[0].linkLM=pInst->fct;
      node->onePred = TRUE;
      node->newNetNode = NULL;
      wordNode->onePred = TRUE;

      pInst->ends=node;
      pInst->nend=1;

      /* Start */
      hmm=GetHCIModel(hci,0,pInst->phones[p],
			FindRContext(hci,pInst,p,0),&logname);
      if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;

      bi->nin++; bi->nil++;
      node=NewHMMNode(net->heap,hmm,1,logname,pInst);
      node->links[0].node=(pInst->chain?pInst->chain:pInst->ends);
      node->links[0].linkLM=pInst->fct;
      node->onePred = pInst->ln->onePred;
      node->newNetNode = NULL;
      pInst->starts=node;
      pInst->nstart=1;

      /* Chain */
      if (pInst->chain!=NULL) {
         for (node=pInst->chain;node->chain!=NULL;
			node=node->chain);
         node->nlinks=1;
         bi->nil++;
         node->links=(NetLink*) New(net->heap,sizeof(NetLink));
         node->links[0].node=pInst->ends;
         node->links[0].linkLM=pInst->fct;
      }
   }
}

static void CreateX1Model(BuildInfo *bi,PronHolder *pInst,int p, int q,
                          Network *net,HMMSetCxtInfo *hci,MemHeap *heap)
{
   NetNode *node,*dest,*wordNode,*linkNode;
   NetLink *links;
   HLink hmm;
   Ptr tptr;
   Boolean tee,initTee,anyTee;
   int j,k,n;
   LabId logname;

   /* Single phone word means that we need to */
   /*  build a complete cross-bar of contexts */

   tee=FALSE; /* Assume okay */

   /* Special case because one phone words so expensive */
   if (IsHCIContextInd(hci,pInst->phones[p])) {
      hmm=GetHCIModel(hci,0,pInst->phones[p],0,&logname);
      if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;
      bi->nin++; bi->nil++;
      node=NewHMMNode(net->heap,hmm,0,logname,pInst);

      pInst->starts=node;

      /* As well as copies of final context free ones */
      for (n=q+1;n<pInst->nphones;n++) {
         hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
         if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;
         bi->ncf++;
         dest=NewHMMNode(net->heap,hmm,0,logname,pInst);
         dest->chain=pInst->chain;pInst->chain=dest;

         bi->nil++;
         node->links=(NetLink*) New(net->heap,sizeof(NetLink));
         node->nlinks=1;
         node->links[0].node=dest;
         node->links[0].linkLM=pInst->fct;

         node=dest;
      }
      links=(NetLink*) New(heap,sizeof(NetLink)*hci->xc);
      for(j=0,wordNode=NULL;j<hci->xc;j++)
         if (pInst->rc[j]!=NULL) {
            wordNode=pInst->rc[j];
            for (k=0;k<node->nlinks;k++)
               if (links[k].node==wordNode) {
                  wordNode=NULL;
                  break;
               }
					if (wordNode!=NULL) {
						links[node->nlinks].node=wordNode;
						links[node->nlinks++].linkLM=pInst->fct;
					}
         }

			node->links=(NetLink*) New(net->heap,sizeof(NetLink)*node->nlinks);
			for (k=0;k<node->nlinks;k++)
				node->links[k]=links[k];
			Dispose(heap,links);

			/* Create any previous context free nodes */
			node = pInst->starts;
			pInst->starts=NULL;

			for (n=p-1;n>=0;n--) {
				dest=node;
				dest->chain=pInst->chain;
				pInst->chain=dest;
				bi->ncf++;
				hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
				if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;
				node=NewHMMNode(net->heap,hmm,1,logname,pInst);
				bi->nil++;
				node->links[0].node=dest;
				node->links[0].linkLM=pInst->fct;
			}
			pInst->nstart=1;
			pInst->starts=node;

			for (j=0;j<hci->xc;j++)
				if (pInst->lc[j]!=NULL)
					pInst->lc[j]=node;

   }
   else if (!hci->sLeft) {
      if (p==0) {
         /* Create NULL node */
         /*  Used as single collating point for all r contexts */
         node = NewNullNode(net->heap);
         node->chain=pInst->starts;
         pInst->starts=node;
         pInst->nstart++;
         bi->nll++;
      }
      else {
         bi->ncf++;
         hmm=GetHCIModel(hci,-1,pInst->phones[0],-1,&logname);
         if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;
         node=NewHMMNode(net->heap,hmm,0,logname,pInst);

         /* Chain these after NULL node */
         node->chain=pInst->starts;
         pInst->starts=node;
         pInst->nstart++;

         /* Create any previous context free nodes */
         for (n=1;n<p;n++) {
            bi->ncf++;
            hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
            if (hmm->transP[1][hmm->numStates]<LSMALL) pInst->tee=FALSE;
            dest=NewHMMNode(net->heap,hmm,0,logname,pInst);
            node->nlinks=1;
            bi->nil++;
            node->links=(NetLink*) New(net->heap,sizeof(NetLink));
            node->links[0].node=dest;
            node->links[0].linkLM=0.0;

            /* Chain these after NULL node */
            dest->chain=pInst->chain;
            pInst->chain=dest;

            node=dest;
         }
      }
      linkNode=node;

      for(j=0;j<hci->xc;j++)
         if (pInst->lc[j]!=NULL)
            pInst->lc[j]=pInst->starts;

			/* Now create actual cd phone */
			tptr=New(heap,1); /* Need place holder to free */
			anyTee=FALSE; /* Haven't seen any final tee chains */
			for(k=0;k<hci->xc;k++) {
				if (pInst->rc[k]==NULL) continue;

				hmm=GetHCIModel(hci,0,pInst->phones[q],k,&logname);
				for(node=pInst->ends;node!=NULL;node=node->chain)
					if (node->info.hmm==hmm) break;

					if (node==NULL) {
						if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Okay now */
						else tee=TRUE; /* Still could be save by final CF models */
						/* Create new model */
						bi->nin++;
						node=NewHMMNode(net->heap,hmm,0,logname,pInst);
						node->chain=pInst->ends;pInst->ends=node;
						pInst->nend++;
						linkNode->nlinks++;

						/* As well as copies of final context free ones */
						for (n=q+1;n<pInst->nphones;n++) {
							hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
							if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Saved */
							bi->ncf++;
							dest=NewHMMNode(net->heap,hmm,0,logname,pInst);
							dest->chain=pInst->chain;pInst->chain=dest;

							bi->nil++;
							node->links=(NetLink*) New(net->heap,sizeof(NetLink));
							node->nlinks=1;
							node->links[0].node=dest;
							node->links[0].linkLM=0.0;

							node=dest;
						}
						if (tee) anyTee=TRUE; /* A single tee chain is too many */

						/* Use inst pointer to get to final model in links */
						pInst->ends->inst=(NetInst*)node;

						node->links=(NetLink*) New(heap,sizeof(NetLink)*hci->xc);
					}
					else
						/* Find final model in links */
						node = (NetNode*)node->inst;

					node->links[node->nlinks].node=pInst->rc[k];
					node->links[node->nlinks].linkLM=0.0;
					node->nlinks++;
			}
			if (!anyTee) pInst->tee=FALSE; /* Didn't see any tee chains */

			/* Now allocate and copy links */
			for (node=pInst->ends;node!=NULL;node=node->chain) {
				dest=(NetNode*)node->inst; /* Find end of cf models */
				bi->nil+=dest->nlinks;
				links=(NetLink*) New(net->heap,sizeof(NetLink)*dest->nlinks);
				for (n=0;n<dest->nlinks;n++)
					links[n]=dest->links[n];
				dest->links=links;
			}
			Dispose(heap,tptr);

			/* And finally link null node to models */
			bi->nil+=linkNode->nlinks;
			linkNode->links=(NetLink*)New(net->heap,sizeof(NetLink)*linkNode->nlinks);
			for (dest=pInst->ends,n=0,node=NULL;dest!=NULL;dest=dest->chain) {
				node=dest;
				linkNode->links[n].node=dest;
				linkNode->links[n].linkLM=0.0;
				n++;
			}
			/* Move these models over to chain */
			node->chain=pInst->chain;
			pInst->chain=pInst->ends;
			pInst->ends=NULL;
			pInst->nend=0;
   }
   else {
      /* Otherwise we do it properly */
      anyTee=FALSE; /* Haven't seen any tee chains */

      for(j=0;j<hci->xc;j++) {
         if (pInst->lc[j]==NULL) continue;

         initTee=TRUE; /* Start off assuming the worse */

         if (p==0) {
            /* Create NULL node */
            /*  Used as single collating point for all r contexts */
            node = NewNullNode(net->heap);

            node->chain=pInst->starts;
            pInst->starts=node;
            pInst->nstart++;
            bi->nll++;

            pInst->lc[j]=node;
         }
         else {
            bi->ncf++;
            hmm=GetHCIModel(hci,-1,pInst->phones[0],-1,&logname);
            node=NewHMMNode(net->heap,hmm,0,logname,pInst);
            pInst->lc[j]=node;

            /* Chain these after NULL node */
            node->chain=pInst->starts;
            pInst->starts=node;
            pInst->nstart++;

            /* Create any previous context free nodes */
            for (n=1;n<p;n++) {
               bi->ncf++;
               hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
               if (hmm->transP[1][hmm->numStates]<LSMALL) initTee=FALSE; /* Okay now */
               dest=NewHMMNode(net->heap,hmm,0,logname,pInst);
               node->nlinks=1;
               bi->nil++;
               node->links=(NetLink*) New(net->heap,sizeof(NetLink));
               node->links[0].node=dest;
               node->links[0].linkLM=pInst->fct;

               /* Chain these after NULL node */
               dest->chain=pInst->chain;
               pInst->chain=dest;

               node=dest;
            }
         }
         linkNode=node;

         /* Now create actual cd phone */
         tptr=New(heap,1); /* Need place holder to free */

         for(k=0;k<hci->xc;k++) {
            if (pInst->rc[k]==NULL) continue;

            hmm=GetHCIModel(hci,j,pInst->phones[q],k,&logname);
            for(node=pInst->ends;node!=NULL;node=node->chain)
               if (node->info.hmm==hmm) break;

					if (node==NULL) {
						if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Okay */
						else tee=initTee; /* Start at end of initial chain */

						/* Create new model */
						bi->nin++;
						node=NewHMMNode(net->heap,hmm,0,logname,pInst);
						node->chain=pInst->ends;pInst->ends=node;
						pInst->nend++;
						linkNode->nlinks++;

						/* As well as copies of final context free ones */
						for (n=q+1;n<pInst->nphones;n++) {
							hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
							if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Saved */
							bi->ncf++;
							dest=NewHMMNode(net->heap,hmm,0,logname,pInst);
							dest->chain=pInst->chain;pInst->chain=dest;

							bi->nil++;
							node->links=(NetLink*) New(net->heap,sizeof(NetLink));
							node->nlinks=1;
							node->links[0].node=dest;
							node->links[0].linkLM=pInst->fct;

							node=dest;
						}
						if (tee) anyTee=TRUE; /* A single tee chain is too many */

						/* Use inst pointer to get to final model in links */
						pInst->ends->inst=(NetInst*)node;

						node->links=(NetLink*) New(heap,sizeof(NetLink)*hci->xc);
					}
					else
						/* Find final model in links */
						node = (NetNode*)node->inst;

					node->links[node->nlinks].node=pInst->rc[k];
					node->links[node->nlinks].linkLM=pInst->fct;
					node->nlinks++;
         }
         /* Now allocate and copy links */
         for (node=pInst->ends;node!=NULL;node=node->chain) {
            dest=(NetNode*)node->inst; /* Find end of cf models */
            bi->nil+=dest->nlinks;
            links=(NetLink*) New(net->heap,sizeof(NetLink)*dest->nlinks);
            for (n=0;n<dest->nlinks;n++)
               links[n]=dest->links[n];
            dest->links=links;
         }
         Dispose(heap,tptr);

         /* And finally link null node to models */
         bi->nil+=linkNode->nlinks;
         linkNode->links=(NetLink*)New(net->heap,sizeof(NetLink)*linkNode->nlinks);
         for (dest=pInst->ends,n=0,node=NULL;dest!=NULL;dest=dest->chain) {
            node=dest;
            linkNode->links[n].node=dest;
            linkNode->links[n].linkLM=(p==0?0.0:pInst->fct);
            n++;
         }
         /* Move these models over to chain */
         node->chain=pInst->chain;
         pInst->chain=pInst->ends;
         pInst->ends=NULL;
         pInst->nend=0;
      }
      if (!anyTee) pInst->tee=FALSE; /* Didn't see any completely tee chains */
   }
}

static void CreateXEModels(BuildInfo *bi,PronHolder *pInst,int p, int q,
                           Network *net,HMMSetCxtInfo *hci,MemHeap *heap)
{
   NetNode *node,*dest,*chainNode,*searchNode;
   NetLink *links;
   HLink hmm;
   Ptr tptr;
   Boolean tee,anyTee;
   int j,n;
   LabId logname;

   /* Cross word context and more than one phone */

   /* Last cd phone */
   chainNode=NULL;searchNode=NULL;
   tptr=New(heap,1);
   anyTee=FALSE; /* Haven't seen any final tee chains */
   for(j=0;j<hci->xc;j++) {
      if (pInst->rc[j]==NULL) continue;
      hmm=GetHCIModel(hci,FindLContext(hci,pInst,q,-1),
			pInst->phones[q],j,&logname);
      for(node=pInst->ends;node!=NULL;node=node->chain)
         if (node->info.hmm==hmm) break;
			if (node==NULL) {
				if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Okay now */
				else tee=TRUE; /* Still could be save by final CF models */
				bi->nfi++;
				node=NewHMMNode(net->heap,hmm,0,logname,pInst);
				node->chain=pInst->ends;
				pInst->ends=node;
				pInst->nend++;

				for (n=q+1;n<pInst->nphones;n++) {
					hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
					if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Saved */
					bi->ncf++;
					dest=NewHMMNode(net->heap,hmm,0,logname,pInst);
					dest->chain=chainNode;chainNode=dest;

					bi->nil++;
					node->links=(NetLink*) New(net->heap,sizeof(NetLink));
					node->nlinks=1;
					node->links[0].node=dest;
					node->links[0].linkLM=pInst->fct;

					node=dest;
				}

				/* Use inst pointer to get to final model in links */
				pInst->ends->inst=(NetInst*)node;

				node->links=(NetLink*) New(heap,sizeof(NetLink)*hci->xc);
				if (tee) anyTee=TRUE; /* A single tee chain is too many */
			}
			else
				/* Find end of cf models */
				node = (NetNode*)node->inst;

			node->links[node->nlinks].node=pInst->rc[j];
			node->links[node->nlinks].linkLM=pInst->fct;
			node->nlinks++;
			if (pInst->fci) break; /* Only need to do this once */
   }
   if (!anyTee) pInst->tee=FALSE; /* Didn't see any tee chains */

   /* Now allocate and copy links */
   for (node=pInst->ends;node!=NULL;node=node->chain) {
      dest=(NetNode*)node->inst; /* Find end of cf models */
      bi->nil+=dest->nlinks;
      links=(NetLink*) New(net->heap,sizeof(NetLink)*dest->nlinks);
      for (j=0;j<dest->nlinks;j++)
         links[j]=dest->links[j];
      dest->links=links;
   }
   Dispose(heap,tptr);

   /* And finally link to ci part of word */
   if (pInst->chain!=NULL) {
      for (node=pInst->chain;node->chain!=NULL;node=node->chain);
      node->nlinks=pInst->nend;
      bi->nil+=node->nlinks;
      node->links=(NetLink*) New(net->heap,sizeof(NetLink)*node->nlinks);
      for (dest=pInst->ends,n=0;dest!=NULL;dest=dest->chain) {
         node->links[n].node=dest;
         node->links[n].linkLM=pInst->fct;
         n++;
      }
   }
   anyTee=FALSE; /* Haven't seen any initial tee chains */
   /* Create first cd phone */
   for(j=0;j<hci->xc;j++) {
      if (pInst->lc[j]==NULL) continue;
      hmm=GetHCIModel(hci,j,pInst->phones[p],
			FindRContext(hci,pInst,p,-1),&logname);
      for(node=pInst->starts;node!=NULL;node=node->chain)
         if (node->info.hmm==hmm) break;
			if (node==NULL) {
				if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Okay now */
				else tee=TRUE; /* Still could be save by initial CF models */
				bi->nin++;
				node=NewHMMNode(net->heap,hmm,
					(pInst->chain==NULL?pInst->nend:1),logname,pInst);
				bi->nil+=node->nlinks;

				node->chain=pInst->starts;pInst->starts=node;
				pInst->nstart++;

				if (pInst->chain!=NULL) {
					node->links[0].node=pInst->chain;
					node->links[0].linkLM=pInst->fct;
				}
				else {
					/* Two phone words need crossbar linking */
					for (dest=pInst->ends,n=0;dest!=NULL;
					dest=dest->chain,n++) {
						node->links[n].node=dest;
						node->links[n].linkLM=pInst->fct;
					}
				}

				for (n=p-1;n>=0;n--) {
					dest=node;
					bi->ncf++;
					hmm=GetHCIModel(hci,-1,pInst->phones[n],-1,&logname);
					if (hmm->transP[1][hmm->numStates]<LSMALL) tee=FALSE; /* Saved */
					node=NewHMMNode(net->heap,hmm,1,logname,pInst);
					if (n!=0) {
						node->chain=chainNode;
						chainNode=node;
					}
					bi->nil++;
					node->links[0].node=dest;
					node->links[0].linkLM=pInst->fct;
				}
				if (tee) anyTee=TRUE; /* A single tee chain is too many */

				/* Link to start of cf models */
				pInst->starts->inst=(NetInst*)node;
			}
			else
				node=(NetNode*)node->inst; /* Find start of cf models */

			/* Point to start of cf models */
			pInst->lc[j]=node;
   }
   if (!anyTee) pInst->tee=FALSE; /* Didn't see any tee chains */

   if (p!=0) {
      /* Need to fix starts list */
      for (node=pInst->starts,pInst->starts=NULL;node!=NULL;node=dest) {
         dest=(NetNode*)node->inst;
         dest->chain=pInst->starts;
         pInst->starts=dest;
         dest=node->chain;
         node->chain=chainNode;
         chainNode=node;
      }
   }
   if (pInst->chain==NULL)
      pInst->chain=chainNode;
   else {
      for (node=pInst->chain;node->chain!=NULL;
		node=node->chain);
      node->chain=chainNode;
   }
}

void ShowWords(Lattice *lat,Vocab *voc,HMMSetCxtInfo *hci)
{
   NetNode *node,*dest;
   Word thisWord;
   PronHolder *pInst;
   LNode *thisLNode;
   MLink ml;
   int i,j,k;

   for (i=0; i < lat->nn; i++) {
      thisLNode = lat->lnodes+i;
      thisWord = thisLNode->word;
      if (thisWord==NULL) thisWord=voc->nullWord;
      for(pInst=(PronHolder*)thisLNode->sublat;
		pInst!=NULL;pInst=pInst->next) {
         if (pInst->pron->outSym==NULL)
            printf("[%-18s]\n",pInst->pron->word->wordName->name);
         else printf("%-20s\n",pInst->pron->outSym->name);
         if (hci->xc==0) {
            for (node=pInst->starts;(node->type&n_nocontext)!=n_word;
				node=node->links[0].node) {
               ml=FindMacroStruct(hci->hset,'h',node->info.hmm);
               if (ml==NULL)
                  printf(" null");
               else if (node->type&n_tr0)
                  printf(" (%s)",ml->id->name);
               else
                  printf(" %s",ml->id->name);
            }
            printf(" ==> %d\n",node->nlinks);
         }
         else if (pInst->clen==0) {
            printf("!NULL word contexts [L, R]\n");
            for (j=0;j<hci->xc;j++)
               if (pInst->lc[j]!=NULL)
                  printf(" %s",ContextName(hci,j)->name);
					printf(", ");
					for (j=0;j<hci->xc;j++)
						if (pInst->rc[j]!=NULL)
							printf(" %s",ContextName(hci,j)->name);
						printf("\n");
         }
         else if (pInst->clen<2) {
            printf("One phone word\n");
            for (j=0;j<hci->xc;j++) {
               if (pInst->lc[j]==NULL) continue;
               if ((pInst->lc[j]->type&n_nocontext)==n_word) {
                  if (hci->sLeft)
                     printf("  %s -> \n",ContextName(hci,j)->name);
                  for (k=0;k<pInst->lc[j]->nlinks;k++) {
                     dest=pInst->lc[j]->links[k].node;
                     ml=FindMacroStruct(hci->hset,'h',dest->info.hmm);
                     printf("      %s",ml->id->name);
                     if ((dest->links[0].node->type&n_nocontext)!=n_word)
                        printf(" ... => %d\n",dest->links[0].node->nlinks);
                     else
                        printf(" => %d\n",dest->nlinks);
                  }
               }
               else {
                  node=pInst->lc[j];
                  ml=FindMacroStruct(hci->hset,'h',node->info.hmm);
                  if (node->type&n_tr0)
                     printf(" (%s)\n",ml->id->name);
                  else
                     printf(" %s\n",ml->id->name);
                  break;
               }
               if (!hci->sLeft) break;
            }
         }
         else {
            printf(" Word initial models \n");
            for (j=0;j<hci->xc;j++) {
               if (pInst->lc[j]==NULL) continue;
               ml=FindMacroStruct(hci->hset,'h',pInst->lc[j]->info.hmm);
               printf("  %s -> %s [%d]\n",
						ContextName(hci,j)->name,ml->id->name,
						pInst->lc[j]->nlinks);
            }
            if (pInst->chain!=NULL)
               printf(" Word internal models \n ");
            for (node=pInst->chain;node!=NULL;node=node->chain) {
               ml=FindMacroStruct(hci->hset,'h',node->info.hmm);
               if (ml==NULL)
                  printf(" null");
               else if (node->type&n_tr0)
                  printf(" (%s)",ml->id->name);
               else
                  printf(" %s",ml->id->name);
            }
            printf("\nWord final models\n");
            for (node=pInst->ends;node!=NULL;node=node->chain) {
               ml=FindMacroStruct(hci->hset,'h',node->info.hmm);
               printf("  %s",ml->id->name);
               if ((node->links[0].node->type&n_nocontext)!=n_word)
                  printf(" ... => %d\n",node->links[0].node->nlinks);
               else
                  printf(" => %d\n",node->nlinks);
            }
         }
      }
      fflush(stdout);
   }
}

/* MarkLatticeNodes: set the one predecessor flags */
static void MarkLatticeNodes( Lattice *lat )
{
   LNode *thisLNode;
   LArc *fArc;
   int i,j;
   PronHolder *tInst;

   for (i=0; i < lat->nn; i++) {
      thisLNode = lat->lnodes+i;
      if (thisLNode->pred != NARC ){
         tInst = (PronHolder*)thisLNode->pred->start->sublat;
         if (tInst!=NULL){
            if ((thisLNode->pred->parc == NARC) && ((PronHolder*)tInst->next == NULL)){
               thisLNode->onePred = TRUE;
            }else{
               thisLNode->onePred = FALSE;
            }
         }
      }else{
         thisLNode->onePred = TRUE;
      }
   }
}

/* TreeStructPhnNet: recursively make a copy of and the network and simultaneously
                     tree structure it at the phone level. */
static void TreeStructPhnNet(Network *treeNet, NetNode *focal, HMMSet *hset, BuildInfo *bi)
{

   NetLink *mask;
   NetLink *newLinks;
   NetLink *oldLinks;
   HLink refHmm;
   NetNode *oldNode,*newNode;
   char buf[80];
   int i,j,k,l,m,numLinks, numOldLinks, nMerged;

   /* focal is a NetNode already created in the tree network and */
   /* linked to the follower nodes in the linear network         */

   mask = focal->links;

   /* newLinks is used to calculate additional 'links' for node */
   /* following 'focal'                                         */

   for (i=0; i<focal->nlinks; i++){
      oldNode=mask[i].node;
      /* If NOT merged and NOT already processed create tree net node */
      if (oldNode && (oldNode->newNetNode==NULL)){

         /* Follower node cannot have > focal->nlinks additional links */
         newLinks=(NetLink*) New(&bi->tmpStak,sizeof(NetLink)*focal->nlinks);
         for(j=0; j<focal->nlinks; j++)
            newLinks[j].node = NULL;

         /* Network end node already exists */
         if (oldNode->nlinks == 0){
            if (oldNode->newNetNode == NULL){
               treeNet->final.type = oldNode->type;
               treeNet->final.info.hmm = oldNode->info.hmm;
               treeNet->final.info.pron = oldNode->info.pron;
               treeNet->final.tag = oldNode->tag;
               treeNet->final.wordset = oldNode->wordset;
               treeNet->final.nlinks = oldNode->nlinks;
               treeNet->final.onePred = FALSE;
               treeNet->final.links = oldNode->links;
               treeNet->final.chain = oldNode->chain;
               oldNode->newNetNode = &(treeNet->final);
            }
            mask[i].node = &(treeNet->final);
         }else{
            /* Create copy of linear network node */
            newNode = CopyOnlyNode(treeNet->heap, oldNode);

            /* Preserve existing follower links */
            numOldLinks = oldNode->nlinks;
            oldLinks=(NetLink*) New(&bi->tmpStak,sizeof(NetLink)*numOldLinks);
            for(j=0; j<numOldLinks; j++){
               oldLinks[j].node = oldNode->links[j].node;
               oldLinks[j].linkLM = oldNode->links[j].linkLM;
            }
            mask[i].node = newNode;
            AddChain(treeNet, newNode);

            /* Search for nodes with Phys HMM identical to reference node */
            /* remove link from 'mask' and add to tree node's 'links'     */
            refHmm = mask[i].node->info.hmm;
            if (oldNode->onePred) { /* If can merge check for matches */
               for (j=i+1; j<focal->nlinks; j++){
                  if (mask[j].node!=NULL && mask[i].linkLM == mask[j].linkLM &&
                     (mask[j].node->type & n_hmm) && (mask[j].node->onePred) &&
                     refHmm == mask[j].node->info.hmm) {
                     newLinks[newNode->nlinks].node = mask[j].node->links[0].node;
                     newLinks[newNode->nlinks].linkLM = mask[j].node->links[0].linkLM;
                     newNode->nlinks++;
                     /* unless shared followers lead to same word, reset wordset to null */
                     if (newNode->wordset != mask[j].node->wordset)
                        newNode->wordset = NULL;
                     mask[j].node = NULL;
                  }
               }
            }
            /* Update follower links for new node */
            numLinks = numOldLinks + newNode->nlinks;
            newNode->links=(NetLink*) New(treeNet->heap,sizeof(NetLink)*numLinks);
            for (j=0; j<numOldLinks; j++){
               newNode->links[j].node = oldLinks[j].node;
               newNode->links[j].linkLM = oldLinks[j].linkLM;
            }
            for (l=numOldLinks,j=0; j<newNode->nlinks; j++,l++){
               newNode->links[l].node = newLinks[j].node;
               newNode->links[l].linkLM = newLinks[j].linkLM;
            }
            newNode->nlinks = numLinks;

         }
         Dispose(&bi->tmpStak, newLinks);
         TreeStructPhnNet(treeNet, mask[i].node, hset, bi);

         /*if NOT merged BUT is already processed link to existing tree net node*/
      }else if(mask[i].node && (mask[i].node->newNetNode!=NULL)){
         mask[i].node = mask[i].node->newNetNode;
      }
   }

   /* Update the following links for 'focal' post merging */
   nMerged = 0;
   for (m=0; m<focal->nlinks; m++) {
      if (!focal->links[m].node) nMerged++;
   }
   for (m=0; m<focal->nlinks; m++){
      if (!focal->links[m].node){
         k=m+1;
         while(!focal->links[k].node && k<focal->nlinks){
            k++;
         }
         if (focal->links[k].node && k<focal->nlinks){
            focal->links[m].node = focal->links[k].node;
            focal->links[m].linkLM = focal->links[k].linkLM;
            focal->links[k].node=NULL;
         }
      }
   }
   focal->nlinks -= nMerged;
}

/* ExpandWordNet: expand given word net to phone net */
Network *ExpandWordNet(MemHeap *heap,Lattice *lat,Vocab *voc,HMMSet *hset)
{
   HMMSetCxtInfo *hci;
   Network *net,*treeNet;
   MemHeap tempHeap;
   NetNode *node,*wordNode,*chainNode;
   NetLink netlink;
   Word thisWord;
   PronHolder *pInst;
   LNode *thisLNode;
   MemHeap holderHeap;
   int i,j,p,q,nc,nNull;
   BuildInfo bi;     /* "static purging" by SJY 20/8/04 */

   /* Initialise */
   InitBuildInfo(&bi);
   CreateHeap(&holderHeap,"Holder Heap",MSTAK,1,0.0,80000,80000);

   if (phnTreeStruct){
      CreateHeap(&tempHeap,"Temp Net heap",MSTAK,1,0,8000,80000);

      net=(Network*) New(&tempHeap,sizeof(Network));
      net->heap=&tempHeap;
      net->vocab=voc;
      net->numNode=net->numLink=0;
      net->chain=NULL;

      treeNet=(Network*) New(heap,sizeof(Network));
      treeNet->heap=heap;
      treeNet->vocab=voc;
      treeNet->numNode=treeNet->numLink=0;
      treeNet->chain=NULL;
   }else{
      net=(Network*) New(heap,sizeof(Network));
      net->heap=heap;
      net->vocab=voc;
      net->numNode=net->numLink=0;
      net->chain=NULL;
   }

   if (!(allowXWrdExp || allowCxtExp) ||
		(forceCxtExp==FALSE && forceRightBiphones==FALSE &&
		forceLeftBiphones==FALSE && ClosedDict(voc,hset))) {
      hci=GetHMMSetCxtInfo(hset,TRUE,&bi);
      if (forceCxtExp && hci->nc==0)
         HError(8230,"ExpandWordNet: No contexts defined and FORCECXTEXT is true");

      nc=0;
   }
   else {
      hci=GetHMMSetCxtInfo(hset,FALSE,&bi);
      nc=hci->nc;
   }
   if (allowXWrdExp && nc>0 && (forceRightBiphones || forceLeftBiphones ||
		forceCxtExp || !InternalDict(voc,hci)))
      hci->xc=nc+1;
   else
      hci->xc=0;

   if (trace&T_CXT) {
      if (hci->xc==0)
         printf("Performing network expansion%s\n",
			(nc>0?" with word internal contexts":""));
      else {
         printf("Performing network expansion with cross word contexts\n");
      }
      if (nc>0) {
         printf(" Defined %d contexts\n",nc);
         for (i=1;i<=nc;i++)
            printf("  %s",ContextName(hci,i)->name);
         printf("\n");
      }
   }


   /* First create context arrays and pronunciation instances */
   nNull=InitPronHolders(net,lat,hci,&bi,voc,&holderHeap,frcSil);
   if (phnTreeStruct){
      if (hci->xc!=0)
         HError(9999,"ExpandWordNet: Cannot perform tree structuring with cross word contexts");
      /* Mark lattice nodes as single or multi predecessor */
      MarkLatticeNodes( lat );
   }

   /* Need to find out the phonetic contexts for all NULL words */
   if (hci->xc>0 && nNull>0)
      SetNullContexts(lat,hci->xc);

   /* Count xwrd links and create word ends */
   ProcessCrossWordLinks(&bi,net->heap,lat,hci->xc);

   /* Build models on basis of contexts seen */
   net->teeWords=FALSE;
   for (i=0; i < lat->nn; i++) {
      thisLNode = lat->lnodes+i;
      thisWord = thisLNode->word;
      if (thisWord==NULL) thisWord=voc->nullWord;

      if (trace&T_CST) {
         printf("Building word %s\n",thisWord->wordName->name);
      }

      for(pInst=(PronHolder*)thisLNode->sublat;
		pInst!=NULL;pInst=pInst->next) {
         /* !NULL consists only of word ends */
         if (pInst->nphones==0) {
            /* Flawed */
            if (hci->xc==0) {
               /* But we need a pointer for xc==0 cases */
               wordNode = FindWordNode(&bi,NULL,pInst->pron,pInst,n_word);
               pInst->starts = wordNode;
               pInst->nstart = 0; /* Stops us adding node into chain twice */
            }
            continue;
         }

         /* Determine which bits of word are l and r cd */
         if (hci->xc>0) {
            for (p=0;p<pInst->nphones;p++)
               if (GetHCIContext(hci,pInst->phones[p])>=0) break;
					for (q=pInst->nphones-1;q>=0;q--)
						if (GetHCIContext(hci,pInst->phones[q])>=0) break;
         }
         else {
            p=0;
            q=pInst->nphones-1;
         }

         pInst->tee=TRUE;
         /* Make wrd-int cd phones (possibly none!) */
         CreateWIModels(&bi,pInst,p,q,net,hci);
         if (hci->xc==0) {
            /* Word internal context only */
            CreateIEModels(&bi,thisWord,pInst,p,q,net,hci);
         }
         /* Cross word context */
         else if (pInst->clen==1) {
            /* Single phone word means that we need to */
            /*  build a complete cross-bar of contexts */
            CreateX1Model(&bi,pInst,p,q,net,hci,&holderHeap);
         }
         else {
            /* Cross word context and more than one phone */
            CreateXEModels(&bi,pInst,p,q,net,hci,&holderHeap);
         }
         if (pInst->tee) {
            HError(-8232,"ExpandWordNet: Pronunciation %d of %s is 'tee' word",
					pInst->pron->pnum,pInst->pron->word->wordName->name);
            net->teeWords=TRUE;
         }
      }
   }


   /* Allocate NetLinks from hash table stats. Zero counters */
   for (i=0; i<WNHASHSIZE; i++) {
      /* Build links for each word end model */
      for (node=bi.wnHashTab[i];node!=NULL;node=node->chain) {
         if (node->nlinks>0){
            node->links=(NetLink*) New(net->heap,sizeof(NetLink)*node->nlinks);
         }else
            node->links=NULL;
         bi.nxl+=node->nlinks;
         node->nlinks=0;
      }
   }

   /* Finally put in the cross word links */
   ProcessCrossWordLinks(&bi,NULL,lat,hci->xc);

   if (trace & T_MOD)
      ShowWords(lat,voc,hci);

   /* First disassemble wnHashTab and link to end nodes as necessary */
   AddInitialFinal(&bi,lat,net,hci->xc);

   for (i=0; i<WNHASHSIZE; i++) {
      AddChain(net,bi.wnHashTab[i]);
   }

   /* Finally chain all nodes together */
   for (i=0; i < lat->nn; i++) {
      for (pInst=(PronHolder*)lat->lnodes[i].sublat;
      pInst!=NULL;pInst=pInst->next) {
         if (pInst->nstart>0)
            AddChain(net,pInst->starts);
         AddChain(net,pInst->chain);
         AddChain(net,pInst->ends);
      }
   }
   /* And then clear up after ourselves */
   for (i=0; i < lat->nn; i++)
      lat->lnodes[i].sublat = NULL;
   DeleteHeap(&holderHeap);


   if (phnTreeStruct){
      treeNet->nullWord = net->nullWord;
      treeNet->teeWords = net->teeWords;
      treeNet->vocab = net->vocab;
      treeNet->initial.nlinks = net->initial.nlinks;
      treeNet->initial.info.hmm = net->initial.info.hmm;
      treeNet->initial.info.pron = net->initial.info.pron;
      treeNet->initial.tag = net->initial.tag;
      treeNet->initial.wordset = net->initial.wordset;
      treeNet->initial.type = net->initial.type;
      net->initial.newNetNode = &treeNet->initial;
      treeNet->numLink=net->initial.nlinks;
      treeNet->numNode=1;
      treeNet->initial.links=(NetLink*) New(treeNet->heap,sizeof(NetLink)*treeNet->initial.nlinks);
      for (i=0; i<treeNet->initial.nlinks; i++){
         treeNet->initial.links[i].node = net->initial.links[i].node;
         treeNet->initial.links[i].linkLM = net->initial.links[i].linkLM;
      }

      TreeStructPhnNet(treeNet, &(treeNet->initial), hset, &bi);

      /* Destroy linear net and set 'net' to tree structured network */
      ResetHeap(&tempHeap);
      net = treeNet;
   }

   /* Count the initial/final nodes/links */
   net->numLink=net->initial.nlinks;
   net->numNode=2;
   /* now reorder links and identify wd0 nodes */
   for (chainNode = net->chain, bi.ncn=0; chainNode != NULL;
   chainNode = chainNode->chain,net->numNode++,bi.ncn++) {
      chainNode->inst=NULL;
      chainNode->type=chainNode->type&n_nocontext;
      net->numLink+=chainNode->nlinks;
      /* Make !NULL words really NULL */
      if (chainNode->type==n_word && chainNode->info.pron!=NULL &&
         net->nullWord!=NULL && chainNode->info.pron->word==net->nullWord)
         chainNode->info.pron=NULL;
      /* First make n_wd0 nodes */
      if (chainNode->type & n_hmm){
         for (i = 0; i < chainNode->nlinks; i++){
            if ( IsWd0Link(&chainNode->links[i]) ) {
               chainNode->type |= n_wd0;
               break;
            }
         }
      }
      /* Then put all n_tr0 nodes first */
      for (i = 0; i < chainNode->nlinks; i++) {
         /* Don't need to move any initial n_tr0 links */
         if (chainNode->links[i].node->type & n_tr0) continue;
         /* Find if there are any n_tr0 ones to swap with */
         for (j = i+1; j < chainNode->nlinks; j++)
            if (chainNode->links[j].node->type & n_tr0) break;
            /* No, finished */
            if (j >= chainNode->nlinks) break;
            /* Yes, swap then carry on */
            netlink = chainNode->links[i];
            chainNode->links[i] = chainNode->links[j];
            chainNode->links[j] = netlink;
      }
   }
   if (trace&T_INF) {
      printf("%d nodes = %d word (%d null), init %d, int %d, fin %d, cf %d\n",
         net->numNode,bi.nwe,bi.nll,bi.nin,bi.nwi,bi.nfi,bi.ncf);
      printf("%d links = int %d, ext %d (%d for null words)\n",
         net->numLink,bi.nil,bi.nxl,bi.nnl);
      fflush(stdout);
   }
   if (trace&T_ALL) {
      /* PrintChain(net,hset); */
      ShowNetwork("Main recognition network",net,hset);
   }
   return(net);
}


/* ====================================================================*/
/*              PART THREE - GRAPHIC NETWORK DISPLAY                   */
/* ====================================================================*/


/* Real network nodes are grouped into nested series and parallel lists of show records.  Each
show record contains a series or parallel set of nested records.  Once the entire network is
grouped into a single show record, display space can be allocated and the network drawn
*/

/* define display characteristics */
const int maxWidth  = 25;     /* max cells wide */
const int maxHeight = 30;     /* max cells high */
const int hdrDepth  = 20;     /* depth of header in pixels */
const int ftrDepth  = 24;     /* footer depth in pixels */
const int dispXMargin = 20;   /* margin around display */
const int dispYMargin = 20;   /* margin around display */
const int cellWidth = 64;		/* width of display cell in pixels */
const int cellDepth = 44;		/* depth of display cell in pixels */
const int cellBoxHW = 20;     /* halfwidth of node box within cell in pixels */
const int cellBoxHH = 12;     /* halfheight of node box within cell in pixels */
const int textFont = -12;     /* font size for main network */

typedef enum {isTerminal,isSeries,isParallel} ShowRecKind;

typedef struct _ShowRec{
	ShowRecKind kind;
	short numStartNodes;			/* number of start nodes */
	NetNode **startNodes;	   /* sorted array[0..numStartNodes-1] of NetNode *  */
	short numEndNodes;			/* number of end nodes */
	NetNode **endNodes;			/* sorted array[0..numEndNodes-1] of NetNode *  */
	short numSuccNodes;			/* number of succ nodes */
	NetNode **succNodes;			/* sorted array[0..numSuccNodes-1] of NetNode  * */
	short numPredNodes;			/* number of pred nodes */
	NetNode **predNodes;			/* sorted array[0..numPredNodes-1] of NetNode  * */
	short predNodesMax;        /* actual num slots in pred node list */
	union{
		ShowRecPtr child;       /* pointer to nested show node */
		NetNode *netNode;       /* or real net node */
	} contents;
	ShowRecPtr parent;			/* parent of this show node */
	ShowRecPtr fchain;         /* chain of all non-nested show nodes */
	ShowRecPtr bchain;			/* reverse chain */
	int w,h;					      /* width and height in cell units */
	int cx,cy;						/* coordinates of terminals in cell units */
}ShowRec;

/* temporary globals used by ShowNetwork for convenience */
static Network *theNet;			/* copied from ShowNetwork args for convenience */
static HMMSet *theHSet;
static ShowRecPtr chainHead;  /* chain of all non-nested show nodes */
static ShowRecPtr chainTail;
static HWin showWin;          /* the display window */
static int textHeight;        /* pixel height of textFont size font */
static unsigned short **hChan;      /* matrix of horizantal channel allocations */
static unsigned short **vChan;      /* matrix of vertical channel allocations */
static signed char *chanOffset;     /* horizantal pixel offsets indexed by chan num */

/* GetNodeInfo1: store first line of node label into buf and return ptr to it */
static char * GetNodeInfo1(NetNode *node, char *buf)
{
	buf[0]='\0';
   if (node->type & n_hmm) {
      sprintf(buf,"%s",HMMPhysName(theHSet,node->info.hmm));
		return buf;
	}
   if (node->type == n_word && node->info.pron==NULL) {
      sprintf(buf,"NULL");
		return buf;
	}
	if (node->type == n_word) {
      sprintf(buf,"%s",node->info.pron->word->wordName->name);
		return buf;
   }
   sprintf(buf,"{%d}",node->type);
	return buf;
}

/* GetNodeInfo2: store second line of node label into buf and return ptr to it */
static char * GetNodeInfo2(NetNode *node, char *buf)
{
	buf[0]='\0';
	if (node->tag != NULL)
		strcpy(buf,node->tag);
	return buf;
}

/* PrintShowRecs: Print a list of show records */
static void Prindent(int d)
{
	int i;
	for (i=0; i<d; i++) printf(" ");
}
static void PrintNodeRecs(int n, NetNode **list, char * lab)
{
	int i;
	printf("%s:",lab);
	for (i=0;i<n;i++) printf(" %p",list[i]);
	printf("  ");
}
static void PrintShowRecs(ShowRecPtr head, int indent)
{
	char buf1[256],buf2[256];
	ShowRecPtr p;

	for (p=head; p!=NULL; p=p->fchain) {
		Prindent(indent);
		if (p->kind == isTerminal){
			printf("Terminal[%p] %s(%s) [w=%d, h=%d]",p,
				GetNodeInfo1(p->contents.netNode,buf1),
				GetNodeInfo2(p->contents.netNode,buf2),p->w,p->h);
			printf(" %d st, %d en, %d succ\n",p->numStartNodes,p->numEndNodes,p->numSuccNodes);
			Prindent(indent);
			PrintNodeRecs(p->numStartNodes,p->startNodes," Start");
			PrintNodeRecs(p->numEndNodes,p->endNodes,"End");
			PrintNodeRecs(p->numSuccNodes,p->succNodes,"Succ");
			PrintNodeRecs(p->numPredNodes,p->predNodes,"Pred");
			printf("\n");
		}else{
			printf("Non-Term[%p]) %s  [w=%d, h=%d]\n",p,p->kind==isSeries?"series":"parallel",p->w,p->h);
			Prindent(indent);
			PrintNodeRecs(p->numStartNodes,p->startNodes," Start");
			PrintNodeRecs(p->numEndNodes,p->endNodes,"End");
			PrintNodeRecs(p->numSuccNodes,p->succNodes,"Succ");
			PrintNodeRecs(p->numPredNodes,p->predNodes,"Pred");
			printf("\n");
			PrintShowRecs(p->contents.child, indent+3);
		}
	}
}

/* GetGridPoint: return coordinates of grid point */
static HPoint GetGridPoint(int cx, int cy)
{
	HPoint pt;
	pt.x = cx*cellWidth + dispXMargin;
	pt.y = cy*cellDepth + hdrDepth+dispYMargin;
	return pt;
}

/* GetNodeCentre: return coordinates of grid point */
static HPoint GetNodeCentre(int cx, int cy)
{
	HPoint pt;
	pt.x = (cx+0.5)*cellWidth + dispXMargin;
	pt.y = (cy+0.5)*cellDepth + hdrDepth+dispYMargin;
	return pt;
}

/* DrawShowRec: draw the given show node at cell(cx,cy) */
static void DrawShowRec(ShowRecPtr p, int cx, int cy)
{
	char buf1[256],buf2[256];
	ShowRecPtr q;
	HPoint n;
	int x0,x1,x2,x3;
	int y0,y1,y2,y3;
	NetNode *nn;
	Boolean onelab;

	p->cx = cx; p->cy = cy;
	n = GetNodeCentre(cx,cy);
	/* HSetColour(showWin,RED);
	 HDrawRectangle(showWin,n.x-cellWidth/2,n.y-cellDepth/2,
	 n.x+(2*p->w-1)*(cellWidth/2),n.y+(2*p->h-1)*(cellDepth/2));*/
	switch (p->kind){
	case isTerminal:
		HSetColour(showWin,BLACK);
		x0 = n.x - cellBoxHW;
		x1 = n.x - HTextWidth(showWin,GetNodeInfo1(p->contents.netNode,buf1))/2;
		x2 = n.x - HTextWidth(showWin,GetNodeInfo2(p->contents.netNode,buf2))/2;
		x3 = n.x + cellBoxHW;
		onelab = strlen(buf2)==0 || strcmp(buf1,buf2)==0;
		y0 = n.y-cellBoxHH;
		y1 = n.y-2;
		if (onelab) y1+=textHeight/2;
		y2 = y1+textHeight;
		y3 = n.y+cellBoxHH;
		HDrawRectangle(showWin,x0,y0,x3,y3);
		nn = p->contents.netNode;
      if (nn->type&1) HFillRectangle(showWin,x3-2,y3-2,x3,y3);
      if (nn->type&8) HFillRectangle(showWin,x0,y0,x0+2,y0+2);
		if (nn->type == n_word){
			if (nn->info.pron!=NULL) HSetColour(showWin,DARK_BLUE);
			else HSetColour(showWin,ORANGE);
		}
		else if (nn->type & n_hmm) HSetColour(showWin,DARK_GREEN);
		HPrintf(showWin,x1,y1,"%s",buf1);
		if (!onelab) HPrintf(showWin,x2,y2,"%s",buf2);
      if (nn->wordset != NULL){
           HSetColour(showWin,MAUVE);
           HPrintf(showWin,x0,y3+textHeight,"%s",nn->wordset->name);
      }
      HSetColour(showWin,BLACK);
		break;
	case	isSeries:
		for (q=p->contents.child; q!=NULL; q = q->fchain) {
			DrawShowRec(q,cx,cy); cx += q->w;
		}
		break;
	case  isParallel:
		for (q=p->contents.child; q!=NULL; q = q->fchain) {
			DrawShowRec(q,cx,cy); cy += q->h;
		}
		break;
	}
}

/* InitConnectChannels: initial the connection channels */
static void InitConnectChannels(int nCols, int nRows)
{
	int i,j;
	const int chanWidth = 16;

	hChan = New(&gstack, sizeof(unsigned short*)*(nCols+1));
	vChan = New(&gstack, sizeof(unsigned short*)*(nCols+1));
	for (i=0; i<=nCols; i++) {
		hChan[i] = New(&gstack, sizeof(unsigned short)*(nRows+1));
		vChan[i] = New(&gstack, sizeof(unsigned short)*(nRows+1));
		for (j=0; j<nRows; j++) hChan[i][j] = vChan[i][j] = 0;
	}
	chanOffset = New(&gstack, chanWidth);
	for (i=0; i<chanWidth; i++){
		chanOffset[i] = (i+1)*2-chanWidth;
	}
}

/* GetConnectChan: return a channel offset from plus (above or right)
or minus (below or left) of centerline */
static int GetConnectChan(int cx, int cy, unsigned short ** chan, Boolean plus)
{
	const int chanCentre = 8;
	const int chanWidth  = 16;
	unsigned short bitselect;
	int k,i;

	for (k=1; k<=2; k++){
		/* scan preferred side then alternate side */
		i=7; bitselect = 128;
		if (plus) {i=8; bitselect = 256;}
		while (i>=0 && i<chanWidth && (chan[cx][cy]&bitselect)){
			if (plus) {i++; bitselect *=2; } else {i--; bitselect /=2;}
		}
		if (i>=0 && i<chanWidth) {
			chan[cx][cy] |= bitselect;
			return chanOffset[i];
		}else{ plus = !plus; }
	}
	return 0;
}

/* DrawVConnection: connect node at (cx,cy0) to (cx,cy1) */
static HPoint DrawVConnection(HPoint from, int cx, int cy0, int cy1, Boolean inReverse)
{
	HPoint gc,stpt,enpt,savept;
	int cy,vc,vcx;
	Boolean isPlus;

	assert(cy0 < cy1);
	/* first see which side of vertical 'from' point is */
	gc	= GetGridPoint(cx,cy0);
	isPlus = from.x>gc.x;
	/* draw vertical line, segment by segment */
	vc = GetConnectChan(cx,cy0,vChan,isPlus);
	stpt = gc; stpt.x += vc;
	enpt = stpt; enpt.y += cellDepth;
	savept = stpt;
	HDrawLine(showWin,stpt.x,stpt.y,enpt.x,enpt.y); stpt = enpt;
	for (cy = cy0+1; cy<cy1; cy++){
		vcx = GetConnectChan(cx,cy,vChan,isPlus);
		if (vcx != vc){ /* channel hop */
			HDrawLine(showWin,stpt.x,stpt.y,stpt.x+vcx-vc,stpt.y);
			stpt.x +=vcx-vc; vc = vcx;
		}
		enpt = stpt; enpt.y += cellDepth;
		HDrawLine(showWin,stpt.x,stpt.y,enpt.x,enpt.y); stpt = enpt;
	}
	stpt = savept;
	/* join up to from to start and return end */
	if (!inReverse) {   /*  stpt -> enpt */
		HDrawLine(showWin,from.x,from.y,stpt.x,stpt.y);
		return enpt;
	}else{              /*  enpt -> stpt */
		HDrawLine(showWin,from.x,from.y,enpt.x,enpt.y);
		return stpt;
	}
}

/* DrawHConnection: connect node at (cx0,cy) to (cx1,cy) */
static HPoint DrawHConnection(HPoint from, int cx0, int cx1, int cy, Boolean inReverse)
{
	HPoint gc,stpt,enpt,savept;
	int cx,vc,vcy;
	Boolean isPlus;

	assert(cx0 < cx1);
	/* first see which side of horizantal 'from' point is */
	gc	= GetGridPoint(cx0,cy);
	isPlus = from.y>gc.y;
	/* draw horizantal line, segment by segment */
	vc = GetConnectChan(cx0,cy,hChan,isPlus);
	stpt = gc; stpt.y += vc;
	enpt = stpt; enpt.x += cellWidth;
	savept = stpt;
	HDrawLine(showWin,stpt.x,stpt.y,enpt.x,enpt.y); stpt = enpt;
	for (cx = cx0+1; cx<cx1; cx++){
		vcy = GetConnectChan(cx,cy,hChan,isPlus);
		if (vcy != vc){ /* channel hop */
			HDrawLine(showWin,stpt.x,stpt.y,stpt.x,stpt.y+vcy-vc);
			stpt.y +=vcy-vc; vc = vcy;
		}
		enpt = stpt; enpt.x += cellWidth;
		HDrawLine(showWin,stpt.x,stpt.y,enpt.x,enpt.y); stpt = enpt;
	}
	stpt = savept;
	/* join up to from to start and return end */
	if (!inReverse) {   /*  stpt -> enpt */
		HDrawLine(showWin,from.x,from.y,stpt.x+1,stpt.y);
		return enpt;
	}else{              /*  enpt -> stpt */
		HDrawLine(showWin,from.x,from.y,enpt.x-1,enpt.y);
		return stpt;
	}
}

/* DrawLinkProb: indicate the link prob */
static void DrawLinkProb(int x, int y, float linkLM)
{
   float p;
   p = linkLM==LZERO?0.0:exp(linkLM);
   HSetColour(showWin,LIGHT_GREEN);
   HPrintf(showWin,x,y,"%.1f",p);
   HSetColour(showWin,BLACK);
}

/* DrawConnection: connect node at (cx0,cy0) to (cx1,cy1) */
static void DrawConnection(int cx0,int cy0,int cx1,int cy1,float linkLM)
{
	HPoint p,q;

	p = GetNodeCentre(cx0,cy0);
	p.x += cellBoxHW;
	if (cx0==cx1-1 && cy0 == cy1) {					/* adjacent */
		q = p; q.x += cellWidth-cellBoxHW*2;
		HDrawLine(showWin,p.x,p.y,q.x,q.y);
      DrawLinkProb(p.x+1,p.y-1,linkLM);
	}else if (cx0==cx1-1 && cy0 == cy1+1) {		/* diag -ve */
		q = GetGridPoint(cx1,cy0);
		HDrawLine(showWin,p.x,p.y,q.x,q.y); p=q;
		q = GetNodeCentre(cx1,cy1); q.x -= cellBoxHW;
		HDrawLine(showWin,p.x,p.y,q.x,q.y);
	}else if (cx0==cx1-1 && cy0+1 == cy1) {		/* diag +ve */
		q = GetGridPoint(cx1,cy1);
		HDrawLine(showWin,p.x,p.y,q.x,q.y); p=q;
		q = GetNodeCentre(cx1,cy1); q.x -= cellBoxHW;
		HDrawLine(showWin,p.x,p.y,q.x,q.y);
	}else {
		if((cy0==cy1 || cy1+1==cy0) && cx1>cx0) {					/* y 0,-1, x +ve */
			p = DrawHConnection(p,cx0+1,cx1,cy0,FALSE);
		}else if(cy0+1==cy1 && cx1>cx0) {							/* y 1, x +ve */
			p = DrawHConnection(p,cx0+1,cx1,cy0+1,FALSE);
		}else if((cy1==cy0 || cy0==cy1+1) && cx1<cx0) {			/* y -1,0, x -ve */
			HSetColour(showWin,RED);
			p = DrawHConnection(p,cx1,cx0+1,cy0,TRUE);
		}else if(cy1==cy0+1 && cx1<=cx0) {							/* y 1, x -ve */
			HSetColour(showWin,RED);
			p = DrawHConnection(p,cx1,cx0+1,cy0+1,TRUE);
		}else if(cx0+1 == cx1 && cy1>cy0) {							/* x 0, y +ve */
			p = DrawVConnection(p,cx1,cy0+1,cy1,FALSE);
		}else if(cx0+1 == cx1 && cy1<cy0) {							/* x 0, y -ve */
			p = DrawVConnection(p,cx1,cy1+1,cy0,TRUE);
		}else if(cx0<cx1 && cy0<cy1) {								/* x +ve, y +ve */
			p = DrawVConnection(p,cx0+1,cy0+1,cy1,FALSE);
			p = DrawHConnection(p,cx0+1,cx1,cy1,FALSE);
		}else if(cx0<cx1 && cy0>cy1) {								/* x +ve, y -ve */
			p = DrawVConnection(p,cx0+1,cy1+1,cy0,TRUE);
			p = DrawHConnection(p,cx0+1,cx1,cy1+1,FALSE);
		}else if(cx0>=cx1 && cy0<cy1) {								/* x -ve, y +ve */
			HSetColour(showWin,RED);
			p = DrawVConnection(p,cx0+1,cy0+1,cy1,FALSE);
			p = DrawHConnection(p,cx1,cx0+1,cy1,TRUE);
		}else {																/* x -ve, y -ve */
			HSetColour(showWin,RED);
			p = DrawVConnection(p,cx0+1,cy1+1,cy0,TRUE);
			p = DrawHConnection(p,cx1,cx0+1,cy1+1,TRUE);
		}
		q = GetNodeCentre(cx1,cy1); q.x -= cellBoxHW;
		HDrawLine(showWin,p.x,p.y,q.x,q.y);
		HSetColour(showWin,BLACK);
	}
}

/* ConnectNode: connect given node to its successors */
static void ConnectNode(NetNode * node)
{
	int i,srcx,srcy,tgtx,tgty;
	NetNode * succ;

	srcx = node->sptr->cx;
	srcy = node->sptr->cy;
	for (i=0; i<node->nlinks; i++){
		succ = node->links[i].node;
		tgtx = succ->sptr->cx;
		tgty = succ->sptr->cy;
		DrawConnection(srcx,srcy,tgtx,tgty,node->links[i].linkLM);
	}

}

/* DrawConnections: draw all the connections between nodes */
static void DrawConnections(int nCols, int nRows)
{
   NetNode *node;

	InitConnectChannels(nCols,nRows);
	ConnectNode(&(theNet->initial));
	for (node=theNet->chain; node != NULL; node = node->chain) {
		ConnectNode(node);
   }
}

/* SortShowNodes:  sort a list of node pointers into ascending order */
static int QSShowNodes(const void *v1, const void *v2)
{
	unsigned int i1,i2;
	i1 = *((unsigned int*)v1); i2 = *((unsigned int *)v2);
	if (i1<i2) return -1;
	if (i1>i2) return 1;
	return 0;
}

static void SortShowNodes(int n, NetNode **list)
{
	qsort(list,n,sizeof(NetNode *),QSShowNodes);
}

/* SameShowLists: return true if lists identical */
static Boolean SameShowLists(int na, NetNode **lista, int nb, NetNode **listb)
{
	int i;

	if (na != nb) return FALSE;
	for (i=0; i<na; i++)
		if (lista[i] != listb[i]) return FALSE;
		return TRUE;
}

/* HasCommonShowListMember: returns true if any member of lista is in list b */
static Boolean HasCommonShowListMember(int na, NetNode **lista, int nb, NetNode **listb)
{
	int i,j;
	NetNode *x;

	for (i=0; i<na; i++) {
		x = lista[i];
		for (j=0; j<nb; j++) {
			if (x == listb[j]) return TRUE;
		}
	}
   return FALSE;
}

/* IsShowListMember: return true if given node x is a member of list */
static Boolean IsShowListMember(NetNode *x, int n, NetNode **list)
{
	int i;

	for (i=0; i<n; i++)
		if (x == list[i]) return TRUE;
		return FALSE;
}

/* OuterParent: return outermost parent of given NetNode* */
static ShowRecPtr OuterParent(NetNode *node)
{
	ShowRecPtr p;

	p = node->sptr;
	while (p->parent != NULL) p = p->parent;
	return p;
}

/* CreateShowRec: create a show record and return a ptr to it.
This routine either encapsulates an actual network node, or
a list of nested show records */
static ShowRecPtr CreateShowRec(ShowRecKind kind, NetNode *netnode, ShowRecPtr childList)
{
	int i,h1,w1;
	ShowRecPtr p = New(&gstack, sizeof(ShowRec));
	ShowRecPtr q;

   p->kind = kind;
	p->numStartNodes = p->numEndNodes = p->numSuccNodes = 0;
	p->parent = NULL;
	p->fchain = p->bchain = NULL;
	if (kind != isTerminal){   /* regular show record */
		assert(childList != NULL);
		p->contents.child = childList;
		if (kind==isSeries){  /* join in series */
			q = childList;
			p->numStartNodes = q->numStartNodes; p->startNodes = q->startNodes;
			p->numPredNodes = q->numPredNodes; p->predNodes = q->predNodes;
			h1 = q->h; w1 = q->w;
			q->parent = p;
			while (q->fchain != NULL) {
				q = q->fchain;
				if (q->h > h1) h1 = q->h;
				w1 += q->w;
				q->parent = p;
			}
			p->numEndNodes = q->numEndNodes; p->endNodes = q->endNodes;
			p->numSuccNodes = q->numSuccNodes; p->succNodes = q->succNodes;
			p->h = h1; p->w = w1;
		} else {              /* join in parallel */
			p->numStartNodes = p->numEndNodes = p->numSuccNodes = p->numPredNodes = 0;
			for (q = childList; q!=NULL; q=q->fchain) {
				p->numStartNodes += q->numStartNodes;
				p->numEndNodes   += q->numEndNodes;
				p->numSuccNodes  += q->numSuccNodes;
				p->numPredNodes  += q->numPredNodes;
			}
			p->startNodes = New(&gstack,p->numStartNodes*sizeof(NetNode *));
			p->endNodes   = New(&gstack,p->numEndNodes*sizeof(NetNode *));
			p->succNodes  = New(&gstack,p->numSuccNodes*sizeof(NetNode *));
			p->predNodes  = New(&gstack,p->numPredNodes*sizeof(NetNode *));
			p->numStartNodes = p->numEndNodes = p->numSuccNodes = p->numPredNodes = 0;
			h1 = 0; w1=0;
			for (q = childList; q!=NULL; q=q->fchain) {
				for (i=0; i<q->numStartNodes; i++)
					p->startNodes[p->numStartNodes++] = q->startNodes[i];
				for (i=0; i<q->numEndNodes; i++)
					p->endNodes[p->numEndNodes++] = q->endNodes[i];
				for (i=0; i<q->numSuccNodes; i++)
					p->succNodes[p->numSuccNodes++] = q->succNodes[i];
				for (i=0; i<q->numPredNodes; i++)
					p->predNodes[p->numPredNodes++] = q->predNodes[i];
				if (q->w > w1) w1 = q->w;
				h1 += q->h;
				q->parent = p;
			}
			SortShowNodes(p->numStartNodes,p->startNodes);
			SortShowNodes(p->numEndNodes,p->endNodes);
			SortShowNodes(p->numSuccNodes,p->succNodes);
			SortShowNodes(p->numPredNodes,p->predNodes);
			p->h = h1; p->w = w1;
		}
	}else{   /* net node */
		assert(netnode != NULL);
		p->contents.netNode = netnode;
		p->numStartNodes = p->numEndNodes = 1;
		p->startNodes = p->endNodes = New(&gstack,sizeof(NetNode *));
		p->startNodes[0] = netnode;
		p->numSuccNodes = netnode->nlinks;
		p->succNodes  = New(&gstack,p->numSuccNodes*sizeof(NetNode *));
		p->predNodesMax = 10; p->numPredNodes = 0;
		p->predNodes  = New(&gstack,p->predNodesMax*sizeof(NetNode *));
		for (i=0; i<p->numSuccNodes; i++)
			p->succNodes[i] = netnode->links[i].node;
		SortShowNodes(p->numSuccNodes,p->succNodes);
		p->w = 1; p->h = 1;   /* all terminals unit size */
		netnode->sptr = p;   /* link to netnode to cross-ref it */
	}
	return p;
}

/* LinkShowRec: link given show record into main chain */
static void LinkShowRec(ShowRecPtr p)
{
	if (chainHead == NULL){
		chainHead = p;
	}else{
		chainTail->fchain = p;
		p->bchain = chainTail; p->fchain = NULL;
	}
	chainTail = p;
}

/* UnlinkShowRec: unlink given show record from main chain */
static void UnlinkShowRec(ShowRecPtr p)
{
	if (p==chainHead && p==chainTail){
		chainHead = chainTail = NULL;
	} else if (p==chainHead) {
		chainHead = chainHead->fchain;
		chainHead->bchain = NULL;
	}else if (p==chainTail){
		chainTail = chainTail->bchain;
		chainTail->fchain = NULL;
	}else{
		p->bchain->fchain = p->fchain;
		p->fchain->bchain = p->bchain;
	}
	p->bchain = p->fchain = NULL;
}

/* InitShowRecChain: scan net nodes and make initial set of show nodes */
static void InitShowRecChain(void)
{
   NetNode *thisNode;
	NetNode **newlist;
   ShowRecPtr p,q;
	int i,j;

	chainTail = chainHead = CreateShowRec(isTerminal,&(theNet->initial), NULL);
	thisNode = theNet->chain;
   while (thisNode != NULL) {
      p = CreateShowRec(isTerminal,thisNode, NULL);
		chainTail->fchain = p;
		p->bchain = chainTail; chainTail = p;
      thisNode = thisNode->chain;
   }
	p = CreateShowRec(isTerminal,&(theNet->final), NULL);
	chainTail->fchain = p;
	p->bchain = chainTail; chainTail = p;
	/* fix-up pred nodes */
	for (p=chainHead; p!=NULL; p=p->fchain){
		thisNode = p->contents.netNode;
		for (i=0; i<thisNode->nlinks; i++){
			q = thisNode->links[i].node->sptr;
			if (q->numPredNodes==q->predNodesMax){
				q->predNodesMax *= 10;
				newlist = New(&gstack,q->predNodesMax * sizeof(NetNode *));
				for (j=0; j<q->numPredNodes; j++)
					newlist[j] = q->predNodes[j];
				q->predNodes = newlist;
			}
			q->predNodes[q->numPredNodes++] = thisNode;
		}
	}
	for (p=chainHead; p!=NULL; p=p->fchain){
		SortShowNodes(p->numPredNodes,p->predNodes);
	}

}

/* MergeParallel: find recs with same preds and succs */
static Boolean MergeParallel(void)
{
	int i;
	NetNode *n;
	Boolean same;
	ShowRecPtr p,q,newchain;

	/* first find a parallel set of show recs */
	for (n=theNet->chain; n!=NULL; n=n->chain){
		same = FALSE;
		if (n->nlinks>1){
			p = OuterParent(n->links[0].node);
			same = TRUE;
			for (i=1; same && i<n->nlinks; i++){
				q = OuterParent(n->links[i].node);
				if (p==q) {same=FALSE; break;}
				same = SameShowLists(p->numSuccNodes,p->succNodes,
					q->numSuccNodes,q->succNodes) &&
					SameShowLists(p->numPredNodes,p->predNodes,
					q->numPredNodes,q->predNodes);
			}
			if (same) break;
		}
	}
	if (!same) return FALSE;
	/* found a parallel set so merge them */
	newchain = OuterParent(n->links[0].node);
	UnlinkShowRec(newchain);
	for (i=1; i<n->nlinks; i++){
		q = OuterParent(n->links[i].node);
		UnlinkShowRec(q);
		newchain->bchain = q; q->fchain = newchain;
		newchain = q;
	}
	/* create new parallel show rec */
	p = CreateShowRec(isParallel,NULL,newchain);
	LinkShowRec(p);
	return TRUE;
}

/* MergeSerial: find recs with succs == starts */
static ShowRecPtr SoleFollower(ShowRecPtr p)
{
	NetNode *n;
	ShowRecPtr q;

	if (p->numSuccNodes==0) return NULL;
	n = p->succNodes[0];
	q = OuterParent(n);
	if (SameShowLists(p->numSuccNodes,p->succNodes,q->numStartNodes,q->startNodes) &&
		SameShowLists(p->numEndNodes,p->endNodes,q->numPredNodes,q->predNodes))
		return q;
	return NULL;
}
static Boolean MergeSerial(void)
{
	ShowRecPtr p,newhead,newtail;
	int size =0;

	/* first find a pair to start a sequence */
	newhead = newtail = NULL;
	for (p=chainHead; p!=NULL; p=p->fchain){
		if ((newtail=SoleFollower(p)) != NULL) break;
	}
	if (newtail==NULL) return FALSE;
	size = 2;
	UnlinkShowRec(p); UnlinkShowRec(newtail);
	newhead = p; p->fchain = newtail; newtail->bchain=p;
	/* extend the chain if possible */
	while ((p=SoleFollower(newtail)) != NULL) {
		UnlinkShowRec(p);
		newtail->fchain = p; p->bchain = newtail; newtail = p;
		++size;
	}
	/* create new series show rec */
	p = CreateShowRec(isSeries,NULL,newhead);
	LinkShowRec(p);
	return TRUE;
}

/* MergeRemains: merge remaining recs in chain regardless */
static void MergeRemains(void)
{
	ShowRecPtr p,nextp,q,newchain;
	int maxw,maxh,newchainsize;

	/* first find the end of the network */
	for (p=chainTail,q=NULL; p!=NULL; p=p->bchain){
		if (p->numSuccNodes == 0) {
			q = p; break;
		}
	}
	if (q==NULL)
		HError(999,"MergeRemains: cannot find end of network");
	/* repeatedly find all nodes with at least one direct link to q */
	while (q!=NULL && chainHead != chainTail){
		newchain = NULL; newchainsize = 0;
		for (p=chainHead; p!=NULL; p=nextp){
			nextp = p->fchain;
			if (p!=q) {
				if (HasCommonShowListMember(p->numSuccNodes,p->succNodes,
					q->numStartNodes,q->startNodes)){
					UnlinkShowRec(p);
					if (newchain == NULL)
						newchain = p;
					else {
						p->fchain = newchain; newchain->bchain = p; newchain = p;
					}
					++newchainsize;
				}
			}
		}
		if (newchain != NULL){
			/*,merge new list in // then join to q in series */
			UnlinkShowRec(q);
			if (newchainsize>1){
				p = CreateShowRec(isParallel,NULL,newchain);
			}else{
				p = newchain;
			}
			p->bchain = NULL; p->fchain = q;
			q->bchain = p; q->fchain = NULL;
			q = CreateShowRec(isSeries,NULL,p);
			LinkShowRec(q);
		}else
			q = NULL;
	}
	/* bundle any remaining recs in series or parallel depending on size */
	if (chainHead != chainTail){
		maxw = maxh = 0;
		for (p=chainHead; p!=NULL; p=p->fchain){
			if (p->w > maxw) maxw = p->w;
			if (p->h > maxh) maxh = p->h;
		}
		if (maxh>maxw)
			chainHead = CreateShowRec(isSeries,NULL,chainHead);
		else
			chainHead = CreateShowRec(isParallel,NULL,chainHead);
		chainTail = chainHead;
	}
}

/* EXPORT->ShowNetwork: display given network on HGraf window */
void ShowNetwork(char * title, Network *net, HMMSet *hset)
{
	int width,depth;
	HEventRec e;
	HButton *b;
	int bx0,bx1,by0,by1,id;
	Boolean ch1,ch2,changed;

	theNet = net; theHSet = hset;
	/* Make every node in net a show node */
	InitShowRecChain();

	/* merge cells into nested serial and parallel chains */
	changed = TRUE;
	while (changed && chainHead != chainTail) {
		while (ch1 = MergeSerial());
		while (ch2 = MergeParallel());
		changed = ch1 || ch2;
	}
	if (chainHead != chainTail) MergeRemains();

	if (chainHead->w > maxWidth || chainHead->h > maxHeight)
		HError(9999,"ShowNetwork: network is too big to display (w=%d,h=%d)",
		chainHead->w,chainHead->h);

	/* Create display window */
	width = chainHead->w * cellWidth+2*dispXMargin;
	depth = chainHead->h * cellDepth+hdrDepth+ftrDepth+2*dispYMargin;
	printf("creating window %d by %d\n",width,depth);

	showWin = MakeHWin(title,10,10,width,depth,1);
	if (showWin==NULL)
		HError(999,"ShowNetwork: cannot create display window");
	HSetFontSize(showWin,textFont);
	textHeight = HTextHeight(showWin,"ABC");
	HSetFontSize(showWin,16);
	HPrintf(showWin,5,20,"Network: %s  %d nodes; %d links ",title,net->numNode,net->numLink);
	HSetFontSize(showWin,textFont);

	/* Draw the Network */
	DrawShowRec(chainHead,0,0);
	DrawConnections(chainHead->w,chainHead->h);

	/* Create dismiss button and wait */
	bx0 = width/2 - 12; bx1 = bx0+ 25;
	by0 = depth-30; by1 = by0+20;
	b = CreateHButton(showWin,NULL,1,bx0,by0,25,20,"OK",BLACK,LIGHT_GREY,12,NULL);
	RedrawHButton(b);
	do {
		id = 0;
		e = HGetEvent(NULL,NULL);
		switch(e.event){
		case HMOUSEDOWN:
			if (IsInRect(e.x,e.y,bx0,by0,bx1,by1)) {
				id = TrackButtons(b, e);
			}
		}
	} while (id != 1);

	CloseHWin(showWin);
}


/* ------------------------ End of HNet.c ------------------------- */
