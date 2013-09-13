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
/*     File: AGram.cpp -  Implementation of Grammar Class      */
/* ----------------------------------------------------------- */

char * agram_version="!HVER!AGram: 1.6.0 [SJY 01/06/07]";

// Modification history:
// 19/10/03   SetEnds modified to ensure entry node is first in list
// 19/10/03   Error reporting improved in InitFromFile
// 19/03/03   Modified to allow external SLFs to have no main subnet

#include "AGram.h"
#define T_LOAD 001     /* Grammar file Loading */
#define T_SHOW 002     /* Show a grammar after loading */
#define T_DEL  040     /* Trace destructors */

static int trace = 0;

// ------------------- Grammar Arcs -------------------

// construct a link to given target
GramLink::GramLink(GramNode *target, float p)
{
   node = target; prob = p;
}

// print the name of the node at the end of the link
void GramLink::PrintTargetName()
{
   printf(" %s",node->name.c_str());
   if (prob>=0.0) printf("(%.2f)",prob);
}

// ------------------- Grammar Nodes -------------------

// construct a grammar node of kind k
GramNode::GramNode(NodeKind k,
                   const string& nname, const string& tag)
{
   name = nname; kind = k; stag = tag; id = -1;
}

// mark this node and all successors as seen
void GramNode::MarkSuccs()
{
   if (!seen){
      seen = TRUE;
      for (LinkIterator l=succ.begin(); l!=succ.end(); ++l)
         l->node->MarkSuccs();
   }
}

// show the contents of the node
void GramNode::Show()
{
   switch(kind){
   case NullNode: printf("Null:"); break;
   case WordNode: printf("Word:"); break;
   case CallNode: printf("Call:"); break;
   }
   printf(" %8s [%6s]",name.c_str(),stag.c_str());
   if (seen) printf("*"); printf("\n");
   if (pred.size()>0){
      printf(" pred:");
      for (LinkIterator i = pred.begin(); i != pred.end(); ++i)
         i->PrintTargetName();
      printf("\n");
   }
   if (succ.size()>0){
      printf(" succ:");
      for (LinkIterator i = succ.begin(); i != succ.end(); ++i)
         i->PrintTargetName();
      printf("\n");
   }
}

// ---------------------- SubNets ----------------------

// construct an empty subnet
GramSubN::GramSubN(const string& nname)
{
   name = nname;  entry = NULL; exit = NULL; refcount = 0;
}

// read HTK subnet definition and create corresponding subnet
GramSubN::GramSubN(Source *src)
{
   char nbuf[132],vbuf[132],buf[32],*ptr,ntype,del;
   int i,n,st,en,nl=0, nn=0;
   float prob;
   Boolean isOk = TRUE;

   try{
      name = "!!MAIN!!";  // if sublat it will be overwritten with HTK name
      entry = NULL; exit = NULL; refcount = 0;

      // read the header info
      while((ptr=GetNextFieldName(nbuf,&del,src))) {
         if (nbuf[0]=='\n') {
            if (nl != 0 && nn != 0) break;
         } else if (strlen(ptr)==1) {
            ntype=*ptr;
            switch(ntype) {
            case 'N': nn=GetIntField('N',del,vbuf,src); break;
            case 'L': nl=GetIntField('L',del,vbuf,src); break;
            default:   GetFieldValue(0,src);            break;
            }
         } else {
            if (strcmp(ptr,"SUBLAT") == 0)
               GetFieldValue(vbuf,src),name=vbuf;
            else
               GetFieldValue(NULL,src);
         }
      }
      if (feof(src->f)) return;  // No main network
      // read the node and link defs
      vector<GramNode *> nodemap(nn);
      string nodename,tag; NodeKind kind;

      for (i=0; i<nn; i++) nodemap[i] = NULL;
      do {
         if ((ptr=GetNextFieldName(nbuf,&del,src)) == NULL) break;
         /* Recognised line types have only one character names */
         if (strlen(ptr)==1) ntype=*ptr; else ntype=0;
         if (ntype == '.') {
            ptr = NULL; break;
         }
         switch(ntype) {
         case '\n':
            break;
         case 'I':
            n=GetIntField('I',del,vbuf,src);
            if (n < 0 || n >= nn){
               HRError(10723,"AGram: node %d is missing",n); throw ATK_Error(10723);
            }
            if (nodemap[n] != NULL){
               HRError(10724,"AGram: multiple defs for node %d",n); throw ATK_Error(10724);
            }
            nodename=""; tag=""; kind = UnknNode;
            while((ptr=GetNextFieldName(nbuf,&del,src)) != NULL) {
               if (nbuf[0]=='\n') break;
               else {
                  if (strlen(ptr)>=1) ntype=*ptr; else ntype=0;
                  switch(ntype) {
                  case 'W':
                     GetFieldValue(vbuf,src);
                     if (strcmp(vbuf,"!NULL")==0) kind = NullNode; else {
                        nodename = vbuf; kind = WordNode;
                     }
                     break;
                  case 's':
                     GetFieldValue(vbuf,src); tag = vbuf;
                     break;
                  case 'L':
                     GetFieldValue(vbuf,src); nodename = vbuf; kind = CallNode;
                     break;
                  default:
                     GetFieldValue(0,src);
                     break;
                  }
               }
            }
            if (kind == UnknNode){
               HRError(10725,"AGram: node %d not fully defined",n); throw ATK_Error(10725);
            }
            if (kind==NullNode) {sprintf(buf,"%d",n); nodename=buf;}
            nodemap[n] = new GramNode(kind,nodename,tag);
            nodes.push_back(nodemap[n]);
            break;
         case 'J':
            n=GetIntField('I',del,vbuf,src);
            if (n<0 || n>=nl){
               HRError(10726,"AGram: link %d is not possible",n); throw ATK_Error(10726);
            }
            st = en = -1;  prob=0.0;
            while ((ptr=GetNextFieldName(nbuf,&del,src))) {
               if (nbuf[0]=='\n') break;
               else {
                  if (strlen(ptr)>=1) ntype=*ptr; else ntype=0;
                  switch(ntype) {
                  case 'S':
                     st=GetIntField('S',del,vbuf,src);
                     if (st<0 || st>=nn){
                        HRError(10727,"AGram: start node %d out of range",st); throw ATK_Error(10727);
                     }
                     break;
                  case 'E':
                     en=GetIntField('E',del,vbuf,src);
                     if (en<0 || en>=nn){
                        HRError(10727,"AGram: end node %d out of range",en); throw ATK_Error(10727);
                     }
                     break;
                  case 'l':
                     prob=float(GetFltField('l',del,vbuf,src));
                     break;
                  default:
                     GetFieldValue(0,src);
                     break;
                  }
               }
            }
            if (st<0 || en <0){
               HRError(10728,"AGram: incomplete link spec (%d to %d)",st,en); throw ATK_Error(10728);
            }
            if (nodemap[st]==NULL){
               HRError(10729,"AGram: start node %d undefined",st); throw ATK_Error(10729);
            }
            if (nodemap[en]==NULL){
               HRError(10729,"AGram: end node %d undefined",en); throw ATK_Error(10729);
            }
            AddLink(nodemap[st],nodemap[en],prob);
            break;
         default:
            GetFieldValue(0,src);
            while ((ptr=GetNextFieldName(nbuf,&del,src))) {
               if (nbuf[0]=='\n') break;
               else GetFieldValue(0,src);
            }
            break;
         }
      }while(ptr != NULL);
      // set end points
      SetEnds();
      // finally check that all is well
      n = IsBroken(TRUE);
      if (n != 0){
         switch(n){
         case 1: HRError(10729,"AGram: entry/exit node missing");     break;
         case 2: HRError(10729,"AGram: entry node has predecessors"); break;
         case 3: HRError(10729,"AGram: exit node has successors");    break;
         case 4: HRError(10729,"AGram: no path from entry to exit");  break;
         case 5: HRError(10729,"AGram: unreachable nodes");           break;
         }
         throw ATK_Error(10729);
      }
   }
   catch (ATK_Error e){
      HRError(e.i,"AGram: error detected in subnet %s",name.c_str());
      throw e;
   }
}

// destructor - delete all constituent nodes
GramSubN::~GramSubN()
{
   if (trace&T_DEL) printf(" deleting subnet %s\n",name.c_str());
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n)
      delete *n;
}

// copy constructor
GramSubN::GramSubN(const GramSubN& sub)
{
   GramNode *n;
   GramNodeList::const_iterator ni;
   vector<GramNode*> map(sub.nodes.size());
   int i;

   name = sub.name;
   // create new nodes
   for (ni=sub.nodes.begin(),i=0; ni != sub.nodes.end(); ++ni,++i){
      n = new GramNode(*(*ni));
      (*ni)->id = i;               // mark position in list
      map[i]=n;                    // remember new pointer
      nodes.push_back(n);
   }
   // fix up the links
   for (ni=nodes.begin(); ni != nodes.end(); ++ni){
      for (LinkIterator l=(*ni)->succ.begin(); l!=(*ni)->succ.end(); ++l)
         l->node = map[l->node->id];
      for (LinkIterator l=(*ni)->pred.begin(); l!=(*ni)->pred.end(); ++l)
         l->node = map[l->node->id];
   }
   refcount = 0;
   SetEnds();
}

// assignment by full clone
GramSubN& GramSubN::operator=(const GramSubN& sub)
{
   GramNode *n;
   GramNodeList::const_iterator ni;
   vector<GramNode*> map(sub.nodes.size());
   int i;

   name = sub.name;
   // create new nodes
   for (ni=sub.nodes.begin(),i=0; ni != sub.nodes.end(); ++ni,++i){
      n = new GramNode(*(*ni));
      (*ni)->id = i;               // mark position in list
      map[i]=n;                    // remember new pointer
      nodes.push_back(n);
   }
   // fix up the links
   for (ni=nodes.begin(); ni != nodes.end(); ++ni){
      for (LinkIterator l=(*ni)->succ.begin(); l!=(*ni)->succ.end(); ++l)
         l->node = map[l->node->id];
      for (LinkIterator l=(*ni)->pred.begin(); l!=(*ni)->pred.end(); ++l)
         l->node = map[l->node->id];
   }
   refcount = 0;
   SetEnds();
   return *this;
}

// find a node by the word it represents
GramNode *GramSubN::FindbyWord(const string& word)
{
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n){
      GramNode* p = *n;
      if (p->kind == WordNode && p->name == word) return p;
   }
   return NULL;
}

// find a node by the tag attached to it
GramNode *GramSubN::FindbyTag(const string& tag)
{
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n){
      GramNode* p = *n;
      if (p->stag == tag) return p;
   }
   return NULL;
}

// find a node by the SubN it calls
GramNode *GramSubN::FindbyCall(const string& subnetname)
{
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n){
      GramNode* p = *n;
      if (p->kind == CallNode && p->name == subnetname) return p;
   }
   return NULL;
}

// find a null node by its tag
GramNode *GramSubN::FindbyNullId(int nullid)
{
   string id; char buf[32];

   sprintf(buf,"%d",nullid); id=buf;
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n){
      GramNode* p = *n;
      if (p->kind == NullNode && p->name == id) return p;
   }
   return NULL;
}

// add new word node to SubN (tag is optional)
GramNode *GramSubN::NewWordNode(const string& word, const string& tag)
{
   GramNode *p = new GramNode(WordNode,word,tag);
   nodes.push_back(p);
   return p;
}

// add new NULL node to SubN (tag is optional)
GramNode *GramSubN::NewNullNode(int nullid, const string& tag)
{
   string id; char buf[32];

   sprintf(buf,"%d",nullid); id=buf;
   GramNode *p = new GramNode(NullNode,id,tag);
   nodes.push_back(p);
   return p;
}

// add new SubN call node to SubN (tag is optional)
GramNode *GramSubN::NewCallNode(const string& subnet, const string& tag)
{
   GramNode *p = new GramNode(CallNode,subnet,tag);
   nodes.push_back(p);
   return p;
}

// remove a node from SubN, this will also remove all attached arcs
void GramSubN::DeleteNode(GramNode* node)
{
   NodeIterator ni;
   LinkIterator l,k;
   GramNode *n;
   Boolean ok;

   // find node in subnet
   for (ok=FALSE,ni=nodes.begin(); ni!=nodes.end(); ++ni){
      if (*ni == node){ ok = TRUE; break; }
   }
   if (!ok) throw ATK_Error(10791);
   // delete links from pred nodes to this node
   for (l=node->pred.begin(); l!=node->pred.end(); ++l){
      n=l->node;
      for (ok=FALSE,k=n->succ.begin(); k!=n->succ.end(); ++k)
         if (k->node == node) {n->succ.erase(k); ok=TRUE; break;}
         if (!ok) throw ATK_Error(10791);
   }
   // delete links from succ nodes to this node
   for (l=node->succ.begin(); l!=node->succ.end(); ++l){
      n=l->node;
      for (ok=FALSE,k=n->pred.begin(); k!=n->pred.end(); ++k)
         if (k->node == node) {n->pred.erase(k); ok=TRUE; break;}
         if (!ok) throw ATK_Error(10791);
   }
   // finally delete the node itself
   delete node;
   nodes.erase(ni);
}

// add a link between given nodes
void GramSubN::AddLink(GramNode* from, GramNode* to, float prob)
{
   from->succ.push_back(GramLink(to,prob));
   to->pred.push_back(GramLink(from,-1.0));
}

// remove a link between given pair of nodes

void GramSubN::DeleteLink(GramNode* from, GramNode* to)
{
   LinkIterator li;
   Boolean ok;

   for (ok=FALSE,li=from->succ.begin(); li!=from->succ.end(); ++li){
      if (li->node == to) {from->succ.erase(li); ok=TRUE; break;}
   }
   if (!ok) throw ATK_Error(10790);
   for (ok=FALSE,li=to->pred.begin(); li!=to->pred.end(); ++li){
      if (li->node == from) {to->pred.erase(li); ok=TRUE; break;}
   }
   if (!ok) throw ATK_Error(10790);
}

// set the entry and exit points
// (a) automatically
void GramSubN::SetEnds()
{
   NodeIterator entryi;

   entry = exit = NULL;
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n){
      if ((*n)->pred.size()==0){
         if (entry != NULL){
            HRError(10751,"GramSubN:SetEnds, more than 1 start node in subnet %s",name.c_str());
            throw ATK_Error(10751);
         }
         entryi = n;
         entry = *n;
      }
      if ((*n)->succ.size()==0){
         if (exit != NULL){
            HRError(10752,"GramSubN:SetEnds, more than 1 end node in subnet %s",name.c_str());
            throw ATK_Error(10752);
         }
         exit = *n;
      }
   }
   if (entry == NULL || exit == NULL){
      HRError(10753,"GramSubN:SetEnds, cant find entry/exit nodes in subnet %s",name.c_str());
      throw ATK_Error(10753);
   }
   // make sure entry is first node in list
   if (nodes.front() != entry) {
      nodes.erase(entryi);
      nodes.push_front(entry);
   }
}

// (b) explicitly
void GramSubN::SetEnds(GramNode* st, GramNode* en)
{
   entry = st; exit = en;
}

// show the contents of the subnet
void GramSubN::Show()
{
   printf("SubNet: %s [entry=%s, exit=%s] refcount=%d\n",name.c_str(),
      entry==NULL?"<NONE>":entry->name.c_str(),
      exit==NULL?"<NONE>":exit->name.c_str(),refcount);
   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n)
      (*n)->Show();
   printf("------------\n");
}

// return >0 if
//  (a) there is a route from entry to exit node
//  (b) entry has no predecessors and exit has nosuccessors.
//  (c) If noStrays, then all nodes must have a route back to entry node
int GramSubN::IsBroken(Boolean noStrays)
{
   NodeIterator ni;

   if (entry==NULL || exit==NULL) return 1;
   if (entry->pred.size()>0) return 2;
   if (exit->succ.size()>0) return 3;
   for (ni=nodes.begin(); ni!=nodes.end(); ++ni) (*ni)->seen = FALSE;
   entry->MarkSuccs();
   if (! exit->seen) return 4;
   if (noStrays){
      for (ni=nodes.begin(); ni!=nodes.end(); ++ni)
         if(!(*ni)->seen) return 5;
   }
   return 0;
}

// check that entry node is first in nodelist
void GramSubN::CheckNodeOrder()
{
   if (nodes.front() != entry) {
      for (NodeIterator ni=nodes.begin(); ni!=nodes.end(); ++ni)
         if ((*ni)==entry){
            nodes.erase(ni);
            nodes.push_front(entry);
            break;
         }
   }
}

// delete any nodes which have no route to start node
void GramSubN::DeleteUnlinked()
{
   if (int n = IsBroken(FALSE)){
      HRError(10721,"AGram::DeleteUnlinked - subnet not valid(%d)",n);
      ATK_Error(10721);
   }
   NodeIterator ni=nodes.begin();
   while( ni!=nodes.end()){
      if((*ni)->seen) ++ni; else {
         delete *ni;
         ni=nodes.erase(ni);
      }
   }
}

// normalise probabilities so that they sum to one
void GramSubN::NormaliseProbs()
{
   float sum;
   LinkIterator l;

   for (NodeIterator n=nodes.begin(); n != nodes.end(); ++n){
      sum = 0.0;
      for (l=(*n)->succ.begin(); l!=(*n)->succ.end(); ++l) sum += l->prob;
      for (l=(*n)->succ.begin(); l!=(*n)->succ.end(); ++l) l->prob /= sum;
   }
}

// ------------------------ Grammars -------------------------

// Initialise grammar from given file
void AGram::InitFromFile(const char * gramFN)
{
   FILE *nf;
   char gFN[512];
   Source src;
   GramSubN *sub;
   Boolean ok;

   if (trace&T_LOAD) printf("Loading HTK SLF %s\n",gramFN);
   // Open the grammar file and attach to a source
   strcpy(gFN,gramFN);
   ok = ( (nf = fopen(gFN,"r")) != NULL)?TRUE:FALSE;
   if (!ok){
      if (gFN[0]=='/') {
         gFN[0] = gFN[1]; gFN[1]=':';
         ok = ((nf = fopen(gFN,"r")) != NULL)?TRUE:FALSE;
      }
   }
   if (!ok){
      HRError(10710,"AGram: Cannot open Word Net file %s",gramFN);
      throw ATK_Error(10710);
   }
   AttachSource(nf,&src);

   // read the network defs
   sub = NewSubN(&src);
   if (sub==NULL){
      HRError(10710,"AGram: Nothing in file %s",gramFN);
      throw ATK_Error(10710);
   }
   while (sub !=NULL && sub->name != "!!MAIN!!")  {
      sub = NewSubN(&src);
   }
   main = sub;
   if (sub != NULL) main->name = rname+":main";

   // close source file
   fclose(nf);
   if (trace&T_SHOW) Show();
}


// Constructor: builds a Grammar from name:info in the config file
AGram::AGram(const string& name):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm,i;
   char buf[100],buf1[100],gramFN[100];

   main = NULL; isOpen = TRUE; gramFN[0] = '\0';
   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
      if (GetConfStr(cParm,numParm,"GRAMFILE",buf1)) strcpy(gramFN,buf1);
   }
   if (gramFN[0] != '\0') InitFromFile(gramFN);
   isOpen = FALSE;
}

// Constructor: builds a Grammar from given file, or empty gram if no file
AGram::AGram(const string& name, const string& fname):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm,i;
   char buf[100];

   main = NULL; isOpen = TRUE;
   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   if (fname!="") InitFromFile(fname.c_str());
   isOpen = FALSE;
}

// copy constructor
AGram::AGram(const AGram& gram):AResource(gram.rname)
{
   GramSubN *s;
   GramSubNList::const_iterator si;

   bakup = NULL; isOpen = FALSE;
   for (si=gram.subs.begin(); si != gram.subs.end(); ++si){
      s = new GramSubN(*(*si)); ++s->refcount;
      if (gram.main == (*si)) main = s;
      subs.push_back(s);
   }
}

// destructor - delete all the subnets
AGram::~AGram()
{
   if (trace&T_DEL) printf(" deleting gram %s\n",rname.c_str());
   for (SubNIterator si=subs.begin(); si != subs.end(); ++si)
      if (--(*si)->refcount == 0) delete (*si);
}

// assignment by full clone
AGram& AGram::operator=(const AGram& gram)
{
   GramSubN *s;
   GramSubNList::const_iterator si;

   rname = gram.rname; bakup = NULL; isOpen = FALSE;
   lock = HCreateLock(string("res:"+rname).c_str());
   version = 0;  next = NULL;
   for (si=gram.subs.begin(); si != gram.subs.end(); ++si){
      s = new GramSubN(*(*si)); ++s->refcount;
      if (gram.main == (*si)) main = s;
      subs.push_back(s);
   }
   return *this;
}

// open this gram for editing
void AGram::OpenEdit()
{
   HEnterSection(lock);
   if (isOpen){
      HRError(10761,"Attempting to open an already open grammar %s",rname.c_str());
      throw ATK_Error(10761);
   }
   isOpen = TRUE;
}

// close gram to allow use by recogniser
void AGram::CloseEdit()
{
   if (!isOpen){
      HRError(10762,"Attempting to close an already closed grammar %s",rname.c_str());
      throw ATK_Error(10762);
   }
   for (SubNIterator si=subs.begin(); si!=subs.end(); ++si){
      // ensure that grammar is valid
      if (int n = (*si)->IsBroken(TRUE)){
         HRError(10731,"Attempting to close invalid grammar %s(%d)",rname.c_str(),n);
         throw ATK_Error(10731);
      }
      // ensure that entry node is the first in every nodelist
      (*si)->CheckNodeOrder();
   }
   ++version;  // ensure that ARMan rebuilds the network
   isOpen = FALSE;
   HLeaveSection(lock);
}

// create a backup copy of gram
void AGram::Save()
{
   bakup = new AGram(*this);
}

// restore grammar from backup copy
void AGram::Restore()
{
   if (bakup == NULL){
      HRError(10796,"AGram::Restore - nothing saved!");
      throw ATK_Error(10796);
   }
   *this = *bakup;
   delete bakup; bakup = NULL;
}

// create a new empty Subnet called name
GramSubN *AGram::NewSubN(const string& name)
{
   if (FindSubN(name) != NULL) {
      HRError(10722,"AGram::NewSubN %s already used",name.c_str());
      ATK_Error(10722);
   }
   GramSubN *s = new GramSubN(name);
   ++s->refcount;  subs.push_back(s);
   return s;
}

// create a new Subnet defined in src
GramSubN *AGram::NewSubN(Source *src)
{
   if (!isOpen){
      HRError(10763,"Attempting to edit a closed grammar %s",rname.c_str());
      throw ATK_Error(10763);
   }
   GramSubN *s = new GramSubN(src);
   if (s->entry == NULL) {   // sub not found (missing main network)
      delete s;
      return NULL;
   }else{
      ++s->refcount;  subs.push_back(s);
      return s;
   }
}

// find SubN with given name
GramSubN *AGram::FindSubN(const string& name)
{
   for (SubNIterator si=subs.begin(); si != subs.end(); ++si){
      if ((*si)->name == name) return *si;
   }
   return NULL;
}

// delete given subnet from this grammar
void AGram::DeleteSubN(GramSubN *s)
{
   for (SubNIterator si=subs.begin(); si != subs.end(); ++si){
      if (*si = s){
         if(--s->refcount == 0) delete s;
         return;
      }
   }
   HRError(10741,"AGram::DeleteSubN cannot find subnet");
   throw ATK_Error(10741);
}

// show the contents of the grammar
void AGram::Show()
{
   printf("-----------------------------------------------\n");
   printf("  GRAMMAR: %s [main = %s]\n",rname.c_str(),
      main==NULL?"<NONE>":main->name.c_str());
   printf("-----------------------------------------------\n");

   for (SubNIterator si=subs.begin(); si!=subs.end(); ++si)
      (*si)->Show();
   printf("===============================================\n");
}

// save grammar to given file
void AGram::SaveToFile(const string& fname)
{
   FILE *nf;
   char buf[100];

   strcpy(buf,fname.c_str());
   if ( (nf = fopen(buf,"w")) == NULL){
      HRError(10711,"AGram: Cannot create save file %s",buf);
      throw ATK_Error(10711);
   }
   fclose(nf);
}

// count number of nodes and links needed in fully expanded subnet
void AGram::ExpandCount(int& nn, int& na, GramSubN *sub)
{
   for (NodeIterator n=sub->nodes.begin(); n != sub->nodes.end(); ++n){
      ++nn;
      na += (*n)->succ.size();
      (*n)->id = -1;    // reset the id map read for build
      if ((*n)->kind == CallNode){
         GramSubN *s = FindSubN((*n)->name);
         if (s==NULL){
            HRError(10745,"AGram::ExpandCount, no subnet called %s",(*n)->name.c_str());
            throw ATK_Error(10745);
         }
         ExpandCount(nn,na,s);
      }
   }
   ++na;  // 1 extra for return link
}


// ------------------------ End AGram.cpp ---------------------


