/* ----------------------------------------------------------- */
/*                                                             */
/*                        _ ___                                */
/*                       /_\ | |_/                             */
/*                       | | | | \                             */
/*                       =========                             */
/*                                                             */
/*        Real-time API for HTK-base Speech Recognition        */
/*                                                             */
/*       Machine Intelligence Laboratory (Speech Group)        */
/*        Cambridge University Engineering Department          */
/*                  http://mi.eng.cam.ac.uk/                   */
/*                                                             */
/*               Copyright CUED 2000-2007                      */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/* File: ARMan.cpp -  Implementation of Resource Manager Class */
/* ----------------------------------------------------------- */

char * arman_version="!HVER!ARMan: 1.6.0 [SJY 01/06/07]";

// Modification history:
// 30/10/03   Replaced HTK::GetWord with ADict::FindWord in CreateNodes
// 30/10/03   Delayed error abort in CreateNodes until all words have been checked
// 31/10/03   NGram support added
// 08/05/05   Termination cleaned up
// 29/07/05   Bug in MakeNetwork fixed, and NULL nodes minimised

#include "ARMan.h"
#define T_TOP 001     /* Top level tracing */
#define T_LAT 002     /* Lattice building */
#define T_WLT 004     /* Print generated HTK lattice */
#define T_GUP 010     /* trace grammar update and combination */
#define T_DEL 040     /* Trace destructors */

static int trace = 0;

// #define sanity

// -------------------- Resource Ref ------------------------

ResourceRef::ResourceRef()
{
   ref=NULL; version=-1; next=NULL;
}

ResourceRef::ResourceRef(AResource *r)
{
   assert(r != NULL);
   ref = r;  next=NULL;
   version = -1;   // ensure that dependent network is updated
}

// --------------------- Resource Group ---------------------

// Construct empty group with given name
ResourceGroup::ResourceGroup(const string& name)
{
   char buf[100];
   gname = name;
   hmms = dicts = grams = ngrams = NULL;
   xhmms = NULL;  xdict = NULL; xgram = NULL;  xngram = NULL;
   next = NULL; net=NULL;
   strcpy(buf,name.c_str()); strcat(buf,":net");
   CreateHeap(&hmem, buf,  MSTAK, 1, 0.2F, 5000, 20000 );
   strcpy(buf,name.c_str()); strcat(buf,":grp");
   lock = HCreateLock(buf);
}

// delete the resource lists, any local copies and network
ResourceGroup::~ResourceGroup()
{
   if (trace&T_DEL) printf("  deleting resgroup %s\n",gname.c_str());
   DeleteHeap(&hmem);
   ResourceRef *p,*pnxt;
   for (p=hmms; p!=NULL; p=pnxt){pnxt=p->next; delete p;}
   for (p=dicts; p!=NULL; p=pnxt){pnxt=p->next; delete p;}
   for (p=grams; p!=NULL; p=pnxt){pnxt=p->next; delete p;}
   for (p=ngrams; p!=NULL; p=pnxt){pnxt=p->next; delete p;}
   if (trace&T_DEL) printf("  resgroup deleted\n");
}

// add resource to given group
void ResourceGroup::AddHMMs(AHmms *p)
{
   HEnterSection(lock);
   ResourceRef *r = new ResourceRef(p);
   r->next = hmms; hmms = r;
   HLeaveSection(lock);
}
void ResourceGroup::AddDict(ADict *p)
{
   HEnterSection(lock);
   ResourceRef *r = new ResourceRef(p);
   r->next = dicts; dicts = r;
   HLeaveSection(lock);
}
void ResourceGroup::AddGram(AGram *p)
{
   HEnterSection(lock);
   ResourceRef *r = new ResourceRef(p);
   r->next = grams; grams = r;
   HLeaveSection(lock);
}
void ResourceGroup::AddNGram(ANGram *p)
{
   HEnterSection(lock);
   ResourceRef *r = new ResourceRef(p);
   r->next = ngrams; ngrams = r;
   HLeaveSection(lock);
}

// lock/unlock the entire resource group
void  ResourceGroup::LockAllResources()
{
   HEnterSection(lock);
   ResourceRef *p;
   for (p=hmms;  p!=NULL; p=p->next){HEnterSection(p->ref->lock);}
   for (p=dicts; p!=NULL; p=p->next){HEnterSection(p->ref->lock);}
   for (p=grams; p!=NULL; p=p->next){HEnterSection(p->ref->lock);}
   for (p=ngrams; p!=NULL; p=p->next){HEnterSection(p->ref->lock);}
}
void  ResourceGroup::UnLockAllResources()
{
   ResourceRef *p;
   for (p=hmms;  p!=NULL; p=p->next){HLeaveSection(p->ref->lock);}
   for (p=dicts; p!=NULL; p=p->next){HLeaveSection(p->ref->lock);}
   for (p=grams; p!=NULL; p=p->next){HLeaveSection(p->ref->lock);}
   for (p=ngrams; p!=NULL; p=p->next){HLeaveSection(p->ref->lock);}
   HLeaveSection(lock);
}

// resource builder routines
Boolean ResourceGroup::UpdateHMMs()
{
   Boolean update = FALSE;
   // only allow one HMM set
   assert(hmms!=NULL && hmms->next==NULL);

   AHmms *p = (AHmms *)hmms->ref;
   if (hmms->version != p->version){
      update = TRUE;
      hmms->version = p->version;
      xhmms = p;
   }
   return update;
}

Boolean ResourceGroup::UpdateDict()
{
   Boolean update = FALSE;
   // only allow one dict for now, but eventually
   // allow multiple dictionaries to be concatenated
   // and individual dictionaries to be edited
   assert(dicts!=NULL && dicts->next==NULL);

   ADict *p = (ADict *)dicts->ref;
   if (dicts->version != p->version){
      update = TRUE;
      dicts->version = p->version;
      xdict = p;
   }
   return update;
}

// Scan the grams in this group for any changes, if so
// return a new AGram representing the parallel combination
// of all the grams in the group
Boolean ResourceGroup::UpdateGram()
{
   Boolean update;
   ResourceRef *r;
   AGram *p,*oldxgram;
   SubNIterator psi;

   update = (xgram==NULL)?TRUE:FALSE;  // must update first time
   // scan the resource list
   for (r=grams; r!=NULL; r=r->next){
      p = (AGram *)r->ref;
      if (r->version != p->version){
         update = TRUE;
         r->version = p->version;
      }
   }
   // if update needed
   if (update) {
      if (trace&T_GUP) printf("Updating Grams for group %s\n",gname.c_str());
      oldxgram = xgram;
      xgram = new AGram(gname+":xgram","");
      // create a main with entry and exit nullnodes
      GramNodePtr entry, exit;
      xgram->main = xgram->NewSubN(gname+":xgram"+":main");
      if (autoSil){
         entry = xgram->main->NewWordNode("SIL");
         exit = xgram->main->NewWordNode("SIL");
      }else{ // use null nodes
         entry = xgram->main->NewNullNode(0);
         exit = xgram->main->NewNullNode(1);
      }
      xgram->main->SetEnds(entry,exit);
      // scan each gram in the group
      for (r=grams; r!=NULL; r=r->next){
         p = (AGram *)r->ref; assert(p);
         // scan sublist of each gram and collect subs
         // combine mains in parallel
         for (psi=p->subs.begin(); psi!=p->subs.end(); ++psi){
            xgram->subs.push_back(*psi); (*psi)->refcount++;
            if (*psi == p->main) {
               GramNodePtr n = xgram->main->NewCallNode(p->main->name);
               xgram->main->AddLink(entry,n); xgram->main->AddLink(n,exit);
            }
         }
      }
   }
   //if (oldxgram!=NULL) delete oldxgram;   // now safe to delete since any needed
   return update;                         // subnets will have been refed
}

Boolean ResourceGroup::UpdateNGram()
{
   Boolean update = FALSE;

   if (ngrams != NULL){
      // only allow one NGram LM
      assert(ngrams->next==NULL);

      ANGram *p = (ANGram *)ngrams->ref;
      if (ngrams->version != p->version){
         update = TRUE;
         ngrams->version = p->version;
         xngram = p;
      }
   }
   return update;
}

// make a HMMset from group resources
HMMSet *ResourceGroup::MakeHMMSet()
{
   UpdateHMMs();
   return xhmms->hset;
}

// make an NGram, if possible, from group resources
LModel *ResourceGroup::MakeNGram()
{
   UpdateNGram();
   if (xngram==NULL) return NULL;
   return xngram->lm;
}

// NextNode: allocates the next node from the lnodes list in lat
NodeId ResourceGroup::NextNode(Lattice* lat, GramNodePtr p, int maxn,
							   const string& subname)
{
	NodeId ln;

	assert(lat->nn<maxn);
	ln = lat->lnodes+lat->nn;
	ln->n = lat->nn;
	if (p->id < 0) p->id = lat->nn;
	if (trace&T_LAT)
		printf(" Creating node %d[%d]=%s[%s] in sub %s\n",lat->nn,p->id,
		p->name.c_str(), p->stag.c_str(),subname.c_str());
	++lat->nn;
	ln->tag = NULL;
	return ln;
}

// Recursive Routine to create HTK Lattice Nodes
void ResourceGroup::CreateNodes(MemHeap *heap, GramSubN* sub, Lattice* lat,
                                int maxn, Boolean isTagged, Boolean *ok)
{
   LNode *ln;
   GramSubN *s;
   char buf[250];
   WordEntry we;

   if (trace&T_LAT) {
      printf("Entering Create Nodes\n");
      printf("--------------------------------\n");
      sub->Show();
   }
   if (sub->entry != sub->nodes.front()){
      HRError(10821,"ARMan::CreateNodes entry not first in sub %s\n",
         sub->name.c_str());
      throw ATK_Error(10821);
   }
   for (NodeIterator n=sub->nodes.begin(); n != sub->nodes.end(); ++n){
      ln = NextNode(lat,(*n),maxn,sub->name);
      if (n==sub->nodes.begin()){   // add sublat tag if needed
         if (isTagged) ln->tag = CopyString(heap,"!SUBLAT_(");
      }else {
         if ((*n)->stag != "")
            ln->tag = CopyString(heap,(*n)->stag.c_str());
      }
      switch((*n)->kind) {
      case WordNode:
         we = xdict->FindWord((*n)->name);
         ln->word = we.w;
         if (ln->word == NULL){
            HRError(10822,"ARMan::CreateNodes: word %s not in dict",
               (*n)->name.c_str());
            *ok = FALSE;
         }
         break;
      case NullNode:
         ln->word = GetWord(lat->voc,GetLabId("!NULL", TRUE),TRUE);
         break;
      case CallNode:
         ln->word = GetWord(lat->voc,GetLabId("!NULL", TRUE),TRUE);
         if (ln->tag != NULL){
            strcpy(buf,"!)_SUBLAT-"); strcat(buf,ln->tag);
            ln->tag = CopyString(heap,buf);
         }
         s = xgram->FindSubN((*n)->name);
         if (s==NULL){
            HRError(10823,"ARMan::CreateNodes: no subnet called %s",
               (*n)->name.c_str());
            throw ATK_Error(10823);
         }
         CreateNodes(heap,s,lat,maxn,ln->tag==NULL?FALSE:TRUE,ok);
         break;
      default:
         HRError(10824,"ARMan::CreateNodes: bad node kind");
         throw ATK_Error(10824);
      }
   }
}

// Recursive Routine to create HTK Lattice Links
void ResourceGroup::CreateLinks(GramSubN* sub,
                    Lattice* lat, int& nn, int maxa)
{
   LArc *la;
   GramSubN *s;
   int offset,st,en,xit;
   NodeIterator n=sub->nodes.begin();
   float prob;
   NodeKind stkind,enkind;
   int callerId = nn-1;
   short *stp, *enp;  // use hook to count num left/right links

   // calculate id offset between this instance (nn) and the canonical
   // ids stored in the node instances (*n)->id.
   offset=nn-(*n)->id;
   if (trace&T_LAT) {
      printf("Entering Create Links for %s[offset = %d]\n",sub->name.c_str(),offset);
      printf("--------------------------------\n");
   }
   // loop through nodes creating follow links
   for (n=sub->nodes.begin(); n != sub->nodes.end(); ++n){
      assert(lat->na<maxa);
      ++nn;
      st = (*n)->id + offset; stkind = (*n)->kind;
      if (stkind==CallNode) {
         s = xgram->FindSubN((*n)->name);
         CreateLinks(s,lat,nn,maxa);
      }
      if ((*n)->succ.size() > 0){
         if (trace&T_LAT)
            printf(" Linking node %d=%s in sub %s\n",st,(*n)->name.c_str(),
            sub->name.c_str());
         for (LinkIterator l=(*n)->succ.begin(); l!=(*n)->succ.end(); l++){
            assert(lat->na<maxa);
            la = NumbLArc(lat,lat->na); ++lat->na;
            prob = (*l).prob;   enkind = (*l).node->kind;
            en = (*l).node->id+offset;
            if (enkind==CallNode) ++en;
            if (trace&T_LAT) printf("   -> %d=%s(%.1f)\n",en,(*l).node->name.c_str(),prob);
            la->start = lat->lnodes+st; la->end = lat->lnodes+en; la->lmlike = prob;
            la->farc = la->start->foll; la->start->foll = la;
            la->parc = la->end->pred; la->end->pred = la;
            // update link counters used by RemoveNulls to identify redundant null nodes
            stp = (short *)(&la->start->hook); ++(*stp);
            enp = (short *)(&la->end->hook); ++enp; ++(*enp);
         }
      }else
         xit = st;
   }
   if (callerId >= 0) {   // add return link
      en = callerId;
      if (trace&T_LAT)
         printf(" Linking exit node %d in sub %s to %d\n",xit,sub->name.c_str(),en);
      la = NumbLArc(lat,lat->na); ++lat->na;
      la->start = lat->lnodes+xit; la->end = lat->lnodes+en; la->lmlike = 0.0;
      la->farc = la->start->foll; la->start->foll = la;
      la->parc = la->end->pred; la->end->pred = la;
      // update link counters used by RemoveNulls to identify redundant null nodes
      stp = (short *)(&la->start->hook); ++(*stp);
      enp = (short *)(&la->end->hook); ++enp; ++(*enp);
   }
}

void ResourceGroup::CheckFollowers(NodeId ln, NodeId en)
{
	LArc *la;
	if (ln->foll == NULL  && ln != en){
		HRError(10828,"ARMan::CheckNetwork - no path from node %d to end\n",ln->n);
		throw ATK_Error(10828);
	}
	ln->v = 1;
	for (la = ln->foll; la != NULL; la=la->farc)
		if (la->end->v == 0) CheckFollowers(la->end,en);
}

// CheckNetwork:  scans a compiled lattice and checks that it is valid
// this is for debugging only
void ResourceGroup::CheckNetwork(Lattice *lat)
{
	int i;
	NodeId ln;
	NodeId st = FindLatStart(lat);
	if (st==NULL) {
      HRError(10825,"ARMan::CheckNetwork - no start node\n");
      throw ATK_Error(10825);
	}
	NodeId en = FindLatEnd(lat);
	if (en==NULL) {
      HRError(10826,"ARMan::CheckNetwork - no end node\n");
      throw ATK_Error(10826);
	}
	// recursively mark all nodes following st, error
	// is flagged if any node has no followers and is
	// not the end node
	CheckFollowers(st,en);
	// check all nodes have been observed
	for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
		if (ln->v==0){
			HRError(10827,"ARMan::CheckNetwork - node %d is unconnected\n",i);
			throw ATK_Error(10827);
		}
		ln->v=0;
	}
}

// RemoveNulls:  scans a compiled lattice and removes redundant null nodes
// note that create links uses the 'hook' of each lnode to store a count
// of the number of left and right connecting arcs.
void ResourceGroup::RemoveNulls(Lattice *lat)
{
	int i,rcount=0;
	NodeId ln,ln2;
   short *stp, *enp;
   LabId nullId = GetLabId("!NULL", FALSE);
	for(i=0,ln=lat->lnodes;i<lat->nn;i++,ln++) {
		stp = (short *)(&ln->hook); enp=stp+1;
      if (ln->word->wordName==nullId && *stp==1 && *enp==1 && ln->tag == NULL) {
         ln2 = ln->foll->end;
         ln->pred->end = ln2;
         ln2->pred = ln->pred;
         // mark node as unused and marked as seen
         ln->n = -1; ln->v = 1;
         ++rcount;
      }
      ln->hook = NULL;
   }
}

// make a network from group resources.  This will reconstruct
// an existing network if any constituent has changed.
Network *ResourceGroup::MakeNetwork()
{
   Lattice *lat;
   MemHeap heap;
   int i,nn,na;
   LNode *ln;
   LArc *la;
   char netName[512];
   Boolean ok;

   LockAllResources();
   Boolean b0 = (net==NULL)?TRUE:FALSE;
   Boolean b1 = UpdateHMMs();
   Boolean b2 = UpdateDict();
   Boolean b3 = UpdateGram();
   Boolean b4 = UpdateNGram();

   if (b0||b1||b2||b3|b4){
      strcpy(netName,gname.c_str());
      if (trace&T_TOP) printf("Making new net for group %s\n",netName);
      // delete existing net
      if (net != NULL) { ResetHeap(&hmem); net=NULL;}
      // create HTK lattice corresponding to xgram
      CreateHeap(&heap,"Lattice heap",MSTAK,1,0.2F,4000,10000);
      lat = (Lattice *) New(&heap,sizeof(Lattice));
      lat->heap=&heap; lat->subLatId=NULL; lat->chain=NULL;
      lat->voc=xdict->vocab; lat->refList=NULL; lat->subList=NULL;
      lat->vocab=NULL; lat->hmms=NULL;
      lat->lmscale=1.0; lat->wdpenalty=0.0;
      lat->utterance = NULL; lat->net = netName;
      lat->format=HLAT_SHARC|HLAT_ALABS;
      // calculate number of nodes nn and arcs na required
      nn = 0;    // init counters
      na = -1;   // top level doesnt have return link
      xgram->ExpandCount(nn,na,xgram->main);
      if (trace&T_TOP) printf("  -- %d nodes, %d links\n",nn,na);
      // allocate node and link space
      lat->lnodes=(LNode *) New(&heap, sizeof(LNode)*nn);
      lat->larcs=(LArc *) New(&heap, sizeof(LArc_S)*na);
      for(i=0, ln=lat->lnodes; i<nn; i++, ln++) {
         ln->hook=NULL; ln->pred=NULL; ln->foll=NULL;
         ln->n = i; ln->v = 0;
      }
      for(i=0, la=lat->larcs; i<na; i++, la=NextLArc(lat,la)) {
         la->lmlike=0.0;
         la->start=la->end=NNODE;
         la->farc=la->parc=NARC;
      }
      // scan grammar and create lat nodes
      lat->nn=0;
      ok = TRUE;    // set false if soft error occurs
      CreateNodes(&heap,xgram->main,lat,nn,FALSE,&ok);
      if (!ok) throw ATK_Error(10822);   // usually one or more missing dict entries
      assert(nn == lat->nn);
      // scan again and add links
      lat->na=0; nn=0;
      CreateLinks(xgram->main,lat,nn,na);
      assert(na == lat->na);
      RemoveNulls(lat);
#ifdef sanity
	  CheckNetwork(lat);
#endif
      if (trace&T_WLT) WriteLattice(lat, stdout, HLAT_DEFAULT);
      if (trace&T_TOP) printf("Expanding lattice\n");
      net=ExpandWordNet(&hmem,lat,xdict->vocab,xhmms->hset);
      DeleteHeap(&heap);
   }
   UnLockAllResources();
   return net;
}

// --------------------- Resource Manager -------------------

// Construct empty resource manager object
ARMan::ARMan()
{
   // Get config parameters, if any
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm, i;
   Boolean b;

   autoSil = FALSE;
   numParm = GetConfig("ARMAN", TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfBool(cParm,numParm,"AUTOSIL",&b)) autoSil = b;
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   // Initialise the structure
   poolHMMs = NULL;
   poolDict = NULL;
   poolGram = NULL;
   poolNGram = NULL;
   groups = NULL; main = NULL;
}

// Delete the resource manager and all enclosed resource objects
ARMan::~ARMan()
{
   // delete resource groups
   if (trace&T_DEL) printf(" deleting ARMan\n");
   ResourceGroup *g,*gnxt;
   for (g=groups; g!=NULL; g=gnxt){gnxt = g->next; delete g;}
   // note that the actual resources are passed into ARMan by the
   // application and therefore the application should take responsibility
   // for deleting them
   if (trace&T_DEL) printf(" ARMan deleted\n");
}

// Store resources in pool
void ARMan::StoreHMMs(AHmms *p){p->next = poolHMMs; poolHMMs = p;}
void ARMan::StoreDict(ADict *p){p->next = poolDict; poolDict = p;}
void ARMan::StoreGram(AGram *p){p->next = poolGram; poolGram = p;}
void ARMan::StoreNGram(ANGram *p){p->next = poolNGram; poolNGram = p;}

// Find a resource
AHmms * ARMan::FindHMMs(string name)
{
   for (AHmms *p=poolHMMs; p!=NULL; p=(AHmms *)p->next){
      if (p->rname==name) return p;
   }
   return NULL;
}
ADict * ARMan::FindDict(string name)
{
   for (ADict *p=poolDict; p!=NULL; p=(ADict *)p->next){
      if (p->rname==name) return p;
   }
   return NULL;
}
AGram * ARMan::FindGram(string name)
{
   for (AGram *p=poolGram; p!=NULL; p=(AGram *)p->next){
      if (p->rname==name) return p;
   }
   return NULL;
}
ANGram * ARMan::FindNGram(string name)
{
   for (ANGram *p=poolNGram; p!=NULL; p=(ANGram *)p->next){
      if (p->rname==name) return p;
   }
   return NULL;
}

// Create a new empty group
ResourceGroup *ARMan::NewGroup(string name)
{
   ResourceGroup *g = new ResourceGroup(name);
   g->autoSil = autoSil;
   g->next = groups;  groups = g;
   if (main==NULL) main = g;
   return g;
}

// Find a group by name
ResourceGroup *ARMan::FindGroup(string name)
{
   for (ResourceGroup *g=groups; g!=NULL; g=g->next){
      if (g->gname==name) return g;
   }
   return NULL;
}

// Find Main group
ResourceGroup *ARMan::MainGroup()
{
   if (main==NULL) {
      HError(10800,"ARMan: no main group defined");
      throw ATK_Error(10800);
   }
   return main;
}

// ------------------------ End ARMan.cpp ---------------------


