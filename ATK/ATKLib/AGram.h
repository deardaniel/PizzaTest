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
/*     File: AGram.h -     Interface for the Grammar Class     */
/* ----------------------------------------------------------- */

/* !HVER!AGram: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
// AGRAM: GRAMFILE    = name of grammar file to load

// Grammar files must be in standard HTK SLF format

#ifndef _ATK_AGram
#define _ATK_AGram

#include "AHTK.h"
#include "AResource.h"
#include "ADict.h"

// ATK Grammar Representation

class GramNode;
class GramSubN;
typedef GramNode * GramNodePtr;

// ------------ Define links ---------------

class GramLink {
public:
  float prob;                  // prob if any, 1.0 by default
  GramNodePtr node;              // node linked to
  GramLink(GramNodePtr target, float p);
  void PrintTargetName();      // print name of target node
};
typedef list<GramLink>  GramLinkList;
typedef GramLinkList::iterator LinkIterator;


// ------------ Define nodes ---------------

enum NodeKind{
  NullNode,   // glue node
  WordNode,   // represents dictionary word
  CallNode,   // represents subnet call
  UnknNode
};

class GramNode {
public:
  string name;                 // word, subnet or nullid name
  string stag;                 // semantic tag if any
  NodeKind kind;               // kind of node
  GramLinkList succ;           // successor and
  GramLinkList pred;           // predecessor links
  // constructor
  GramNode(NodeKind k, const string& nname = "",
	   const string& tag = "");
  void Show();                 // print out contents
private:
  int id;                      // used for mapping to HTK lattices
  Boolean seen;                // used for checking
  void MarkSuccs();            // mark this & successors as seen
  friend class GramSubN;
  friend class AGram;
  friend class ResourceGroup;
  friend class ARMan;
};

typedef list<GramNodePtr>  GramNodeList;
typedef GramNodeList::iterator NodeIterator;


// ------------ Define subnets ---------------

class GramSubN {
public:
  string name;                 // name of this subnet
  GramNodePtr entry;             // entry node
  GramNodePtr exit;              // exit  node
  GramNodeList nodes;          // nodes in this subnet
  // constructors
  GramSubN(const string& nname);     // empty subnetwork
  GramSubN(Source *src);             // from HTK SLF file
  GramSubN(const GramSubN& sub);     // copy by full clone
  // destructor : delete all constituent nodes
  ~GramSubN();
  // assignment clones all constituent nodes
  GramSubN& operator=(const GramSubN& sub);
  // routines to find a node
  GramNodePtr FindbyWord(const string& word);
  GramNodePtr FindbyCall(const string& subnetname);
  GramNodePtr FindbyTag (const string& tag);
  GramNodePtr FindbyNullId (int n);
  // routines to create/delete new nodes
  GramNodePtr NewWordNode(const string& word, const string& tag = "");
  GramNodePtr NewNullNode(int nullid, const string& tag = "");
  GramNodePtr NewCallNode(const string& subnet, const string& tag = "");
  void DeleteNode(GramNode* node);
  // Link/unlink nodes
  void AddLink(GramNode* from, GramNode* to, float prob=0.0);
  void DeleteLink(GramNode* from, GramNode* to);
  void SetEnds();   // find start/end nodes and set entry/exit
  void SetEnds(GramNode* st, GramNode* en);  // explicit version
  // Utility
  void Show();
  int IsBroken(Boolean noStrays);   // returns 0 if not broken
  void CheckNodeOrder();  // ensure entry node is first
  void DeleteUnlinked();
  void NormaliseProbs();
private:
  int refcount;                // num refs to this subnet
  friend class AGram;
  friend class ResourceGroup;
  friend class ARMan;
};
typedef list<GramSubN *>  GramSubNList;
typedef GramSubNList::iterator SubNIterator;


// ------------ Define grammars ---------------

class AGram : public AResource {
public:
  GramSubN *main;              // top level subnet
  GramSubNList subs;           // list of subnets
  // Construct gram from HTK lattice file fn named in config
  //       name: GRAMFILE=fn
  AGram(const string& name);
  // Construct gram from specified HTK lattice file, or
  // if fname is "", empty grammar is created
  AGram(const string& name, const string& fname);
  // Copy constuctor - by full clone
  AGram(const AGram& gram);
  // destructor
  ~AGram();
  // assignment clones all constituent subnets
  AGram& operator=(const AGram& gram);
  // Grammar editing
  void OpenEdit();             // open this gram for editing
  void CloseEdit();            // close gram to allow use by recogniser
  void Save();                 // create a backup copy of gram
  void Restore();              // restore grammar from backup copy
  // Subnet operation
  GramSubN *NewSubN(const string& name);  // create a new subnet called name
  GramSubN *NewSubN(Source *src);         // create a new subnet from HTK File
  GramSubN *FindSubN(const string& name); // find subnet with given name
  void DeleteSubN(GramSubN *s);            // delete given subnet
  // Utility
  void Show();
  void SaveToFile(const string& fname);   // save gram to HTK lattice file
  void InitFromFile(const char * gramFN); //init gram from HTK file
  void ExpandCount(int& nn, int& na, GramSubN *sub);
  friend class ResourceGroup;
  friend class ARMan;
private:
  Boolean isOpen;
  AGram *bakup;
};

#endif
/*  -------------------- End of AGram.h --------------------- */

