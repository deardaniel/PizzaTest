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
/* File: ARMan.h -   Interface for the Resource Manager Class  */
/* ----------------------------------------------------------- */

/* !HVER!ARMan: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
// ARMAN: AUTOSIL = T   -- auto add sil models around utterance

#include <stdio.h>
#ifndef _ATK_ARMan
#define _ATK_ARMan

#include "AHTK.h"
#include "AHmms.h"
#include "ADict.h"
#include "AGram.h"
#include "ANGram.h"

class ResourceRef {
  ResourceRef();
  ResourceRef(AResource *r);
  AResource *ref;
  int version;
  ResourceRef *next;
  friend class ARMan;
  friend class ResourceGroup;
};

class ResourceGroup {
public:
  string gname;        // name of this group
  ResourceGroup(const string& name);
  ~ResourceGroup();
  // Add resources to group
  void AddHMMs (AHmms  *p);
  void AddDict (ADict  *p);
  void AddGram (AGram  *p);
  void AddNGram(ANGram *p);
  // Get HMMSet from group
  HMMSet *MakeHMMSet();
  // Make a network from group
  Network *MakeNetwork();
  // Get Ngram (if any) from group
  LModel *MakeNGram();
  friend class ARMan;
private:
  // resource update routines, return true if the corresponding
  // resource has been updated.
  Boolean UpdateHMMs();   // update HMMs
  Boolean UpdateDict();   // update Dicts
  Boolean UpdateGram();   // update Grams
  Boolean UpdateNGram();   // update NGrams
  // lock/unlock all of the resources in the group
  void LockAllResources();
  void UnLockAllResources();
  // recursive routines used by MakeNetwork
  void CreateNodes(MemHeap *heap, GramSubN* sub,
		   Lattice* lat, int maxn, Boolean isTagged, Boolean *ok);
  void CreateLinks(GramSubN* sub, Lattice* lat,  int &nn, int maxa);
  void CheckNetwork(Lattice *lat);
  void RemoveNulls(Lattice *lat);
  void CheckFollowers(NodeId ln, NodeId en);
  NodeId NextNode(Lattice* lat, GramNodePtr p, int maxn,
	              const string& subname);
  AHmms *xhmms;        // this group's hmmset ...
  ADict *xdict;        //  ... dict and
  AGram *xgram;        //  ... grammar
  ANGram *xngram;       //  ... ngram lm, if any
  MemHeap hmem;        // HTK memory for the network
  Network *net;        // current network if any
  ResourceRef *hmms;   // constitutent resources
  ResourceRef *dicts;
  ResourceRef *grams;
  ResourceRef *ngrams;
  Boolean autoSil;       // auto add initial/final silence
  ResourceGroup *next;
  HLock lock;          // protected access
};

class ARMan {
public:
  // Construct Empty Resource Manager
  ARMan();
  // Destroy Resource Manager
  ~ARMan();
  // Pool operations
  void StoreHMMs (AHmms *p);
  void StoreDict (ADict *p);
  void StoreGram (AGram *p);
  void StoreNGram(ANGram *p);
  AHmms  * FindHMMs(string name);
  ADict  * FindDict(string name);
  AGram  * FindGram(string name);
  ANGram * FindNGram(string name);
  // Group operations
  ResourceGroup *MainGroup();
  ResourceGroup *NewGroup(string name);
  ResourceGroup *FindGroup(string name);

private:
  Boolean autoSil;       // auto add initial/final silence
  AHmms *poolHMMs;       // resource pools
  ADict *poolDict;
  AGram *poolGram;
  ANGram *poolNGram;
  ResourceGroup *groups; // defined groups
  ResourceGroup *main;   // main group
};

#endif
/*  -------------------- End of ARMan.h --------------------- */

