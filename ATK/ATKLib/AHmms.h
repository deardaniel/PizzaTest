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
/*     File: AHmms.h -     Interface for the HMMSet Class      */
/* ----------------------------------------------------------- */

/* !HVER!AHmms: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
// AHMMS: HMMLIST    = name of hmm list to use
// AHMMS: HMMDIR     = name of directory to look for HMMs
// AHMMS: MMF[0-9]   = specify upto 10 MMF files to load
// AHMMS: XFORMNAME  = name of the model transform

#include <stdio.h>
#ifndef _ATK_AHmms
#define _ATK_AHmms

#include "AHTK.h"
#include "AResource.h"

class AHmms : public AResource {
public:
  // Construct HMMSet from name:info in config file
  AHmms(const string& name);
  // Construct HMMSet from explicitly supplied hmmlist and MMF (only one MMF)
  AHmms(const string& name, const string& hmmlist, const string& mmf0,
	     const string& mmf1, int trace=0);
  // Destroy HMMSet including disposal of MemHeap
  ~AHmms();
  // Get Properties of HMMSet
  HSetKind GetKind();
  ParmKind GetParmKind();
  int GetNumLogHMM();
  int GetNumPhyHMM();
  int GetNumStates();
  int GetNumSharedStates();
  int GetNumMix();
  int GetNumSharedMix();
  int GetNumTransP();
  HMMSet *GetHMMSet();
  // Check for compatibility with given observation
  Boolean CheckCompatible(Observation *o);
  // Load XForms
  Boolean XFormAdded;
  Boolean AddXForm(char *name);
  friend class ResourceGroup;
private:
  int trace;             // trace flags
  HMMSet *hset;          // the HTK data structure
  XFInfo xfinfo;         // model transform
  MemHeap hmem;          // heap for use by HTK
  string hmmList;        // name of list file
  string hmmDir;         // name of hmm dir
  string hmmExt;         // name of hmm ext
  string mmfn[10];       // mmf file names mmf0, mmf1, mmf2, ... , mmf9
};

#endif
/*  -------------------- End of AHmms.h --------------------- */

