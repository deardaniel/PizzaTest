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
/*   File: ADict.cpp -  Implementation of Dictionary  Class    */
/* ----------------------------------------------------------- */


char * adict_version="!HVER!ADict: 1.6.0 [SJY 01/06/07]";

#include "ADict.h"
#define T_LOAD 001     /* Dict file Loading */

// ----------------------------- Pronunciation -----------------------

// construct empty pronunciation
Pronunciation::Pronunciation()
{
   prob=0.0; outSym=""; pnum=0;
}

// construct a new pronunciation, pronspec consist of a string of phones
// separated by spaces
Pronunciation::Pronunciation(string oSym, string pronspec, float pr)
{
   string phone;

   prob=pr; outSym=oSym; pnum=0;
   int len = pronspec.size();
   pronspec = pronspec+" ";
   int st,en=-1;
   while (en < len){
      st = en+1; ++en;
      while (pronspec[en] != ' ') ++en;
      phone = string(pronspec,st,en-st);
      phones.push_back(phone);
   }
}

// constructor to mirror existing HTK entry
Pronunciation::Pronunciation(Pron p)
{
   prob = p->prob;
   outSym = (p->outSym != NULL) ? string(p->outSym->name) :"";
   pnum = p->pnum;
   for (int i=0; i<p->nphones; i++)
      phones.push_back(string(p->phones[i]->name));
}

// show the pronunciation
void Pronunciation::Show()
{
   PhonList::iterator i;
   printf(" %2d. ",pnum);
   for (i=phones.begin(); i != phones.end(); i++)
      printf(" %s",i->c_str());
   printf("   /%.2f/\n",prob);
}

// ----------------------------- WordEntry ----------------------------

// empty constructor
WordEntry::WordEntry()
{
   word = ""; w = NULL;
}

// 1 pron constructor
WordEntry::WordEntry(string wd, Pronunciation p1)
{
   word = wd;
   p1.pnum = 1;
   pronlist.push_back(p1);
}

// 2 pron constructor
WordEntry::WordEntry(string wd, Pronunciation p1, Pronunciation p2)
{
   word = wd;
   p1.pnum = 1; p2.pnum = 2;
   pronlist.push_back(p1);
   pronlist.push_back(p2);
}

// sync the pronlist with the real data in HTK
void WordEntry::SyncProns()
{
   if (w==NULL){
      HRError(10601,"ADict::SyncProns - no HTK word entry to synch");
      throw ATK_Error(10601);
   }
   Pron p=w->pron;
   for (int n=0; n < w->nprons; n++) {
      pronlist.push_back(Pronunciation(p)); p=p->next;
   }
}

// show the word entry
void WordEntry::Show()
{
   PronList::iterator i;
   int n=0;

   printf("WORD: %s\n",word.c_str());
   for (i=pronlist.begin(); i != pronlist.end(); i++){
      i->Show();
   }
}


// ----------------------------- ADict -------------------------------

// Constructor: builds a Dictionary from name:info in the config file
ADict::ADict(const string& name):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i;
   char buf[100],buf1[100];

   dictFN = ""; trace=0;
   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfStr(cParm,numParm,"DICTFILE",buf1)) dictFN = buf1;
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   // Initialise Vocab structure
   vocab = new Vocab;
   InitVocab(vocab);
   // if external file given, then load it
   if (dictFN != "") {
      strcpy(buf,dictFN.c_str());
      if(ReadDict(buf,vocab)<SUCCESS) {
         HRError(10600, "ADict: failed to load from %s",buf);
         throw HTK_Error(10600);
      }
      if (trace&T_LOAD) printf("ADict: successful load from %s\n",buf);
   }
}

// Constructor: builds a Dictionary from external file
ADict::ADict(const string& name, const string& fname):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i;
   char buf[100];

   dictFN = fname; trace=0;
   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }

   // Read dictionary file and initialise Vocab structure
   vocab = new Vocab;
   InitVocab(vocab);   strcpy(buf,dictFN.c_str());
   if(ReadDict(buf,vocab)<SUCCESS){
      HRError(10600, "ADict: failed to load from %s",buf);
      throw HTK_Error(10600);
   }
   if (trace&T_LOAD) printf("ADict: successful load from %s\n",buf);
}

ADict::~ADict()
{
   if (vocab != NULL) {
      ClearVocab(vocab); vocab = NULL;
   }
}

// open this dict for editing
void ADict::OpenEdit()
{
   HEnterSection(lock);
   if (isOpen){
      HRError(10602,"Attempting to open an already open dict %s",rname.c_str());
      throw ATK_Error(10602);
   }
   isOpen = TRUE;
}

// close dict to allow use by recogniser
void ADict::CloseEdit()
{
   if (!isOpen){
      HRError(10602,"Attempting to close an already closed dict %s",rname.c_str());
      throw ATK_Error(10602);
   }
   ++version;  // ensure that ARMan rebuilds the network
   isOpen = FALSE;
   HLeaveSection(lock);
}

// Get word from dict, we.w == NULL if it is not there
WordEntry ADict::GetWord0(char *s)
{
   WordEntry we;
   LabId id = GetLabId(s,FALSE);
   if (id != NULL) we.w = GetWord(vocab,id,FALSE);
   return we;
}

// Find word in dict, use GetWord0 for each attempt
WordEntry ADict::FindWord0(const string& word, Boolean chkonly)
{
   char buf[256];
   WordEntry we;

   // Look for word in dictionary
   we = GetWord0(strcpy(buf,word.c_str()));
   if (chkonly || we.w == NULL) return we;
   // word is in dictionary, retrieve it
   we.word = word;  // return dict form
   we.SyncProns();
   return we;
}

// True if word is in dict
Boolean ADict::HasWord(const string& word)
{
   WordEntry we = FindWord0(word,TRUE);
   if (we.w == NULL) return FALSE;
   return TRUE;
}

// Find word in dict
WordEntry ADict::FindWord(const string& word)
{
   WordEntry we = FindWord0(word,FALSE);
   return we;
}

// Remove word from dict
void ADict::RemoveWord(WordEntry& word)
{
   if (word.w==NULL){
      HRError(10601,"ADict::RemoveWord - no HTK word entry to synch");
      throw ATK_Error(10601);
   }
   DelWord(vocab,word.w);
}

// Update word in dict
void ADict::UpdateWord(WordEntry& we)
{
   LabId wid,oid;
   LabId plist[500];  // assume a max pron length
   int n,k;
   PronList::iterator i;
   PhonList::iterator j;

   if (HasWord(we.word)){
      WordEntry tmp = FindWord(we.word);
      RemoveWord(tmp);
   }
   wid = GetLabId(we.word.c_str(),TRUE);
   we.w = GetWord(vocab, wid, TRUE);
   for (i=we.pronlist.begin(); i != we.pronlist.end(); i++){
      oid = GetLabId(i->outSym.c_str(),TRUE);
      n = i->phones.size(); k = 0;
      if(n>=500){
         HRError(10603,"ADict::UpdateWord - pron too long");
         throw ATK_Error(10603);
      }
      for (j=i->phones.begin(); j != i->phones.end(); j++) {
         plist[k++] = GetLabId(j->c_str(),TRUE);
      }
      NewPron(vocab,we.w,n,plist,oid,i->prob);
   }
}

// Show entire dictionary
void ADict::Show()
{
   ShowDict(vocab);
}

// ------------------------ End ADict.cpp ---------------------


