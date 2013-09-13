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
/*   File: ADict.h -     Interface for the Dictionary Class    */
/* ----------------------------------------------------------- */

/* !HVER!ADict: 1.6.0 [SJY 01/06/07] */

// Configuration variables (Defaults as shown)
// ADICT: DICTFILE    = name of dictionary file to load

#ifndef _ATK_ADict
#define _ATK_ADict

#include "AHTK.h"
#include "AResource.h"

// Configuration variables (Defaults as shown)
//
// ADICT: TRACE        = 0     -- set trace level
// ADICT: DICTFILE     = ""    -- external dictionary file

typedef list<string> PhonList;
class Pronunciation {
public:
  Pronunciation();          // empty constructor
  Pronunciation(Pron p);    // copy a htk pron
  Pronunciation(string oSym, string pronspec, float pr);    // create a new pron
  void Show();
  PhonList phones;
  int pnum;
  float prob;
  string outSym;
};

typedef list<Pronunciation> PronList;
class WordEntry {
public:
  WordEntry();         // empty constructor
  WordEntry(string wd, Pronunciation p1);                     // 1 pron word
  WordEntry(string wd, Pronunciation p1, Pronunciation p2);   // 2 pron word
  void SyncProns();    // copy HTK prons to pronlist
  void Show();         // show this word entry
  string word;         // the word
  PronList pronlist;   // list of pronuncations
  Word w;    //Actual HTK dict entry, NULL if undefined
};

class ADict : public AResource {
public:
  // Construct Dictionary from name:info in config file
  ADict(const string& name);            // filename given in config
  ADict(const string& name, const string& fname);  //bypass config
  // Destroy Dictionary including disposal of MemHeap
  ~ADict();
  // Dictionary editing
  void OpenEdit();       // open this dict for editing
  void CloseEdit();      // close dict to allow use by recogniser
  void Save();           // create a backup copy of dict
  void Restore();        // restore dict from backup copy
  Boolean HasWord(const string& word);    // True if word is in dict
  WordEntry FindWord(const string& word); // Find word in dict
  void RemoveWord(WordEntry& word);       // Remove word from dict
  void UpdateWord(WordEntry& we);         // Update word in dict
  void Show();        // show all dictionary entries
  friend class AGram;
  friend class ResourceGroup;
private:
  WordEntry FindWord0(const string& word, Boolean chkonly);
  WordEntry GetWord0(char *s);
  Boolean isOpen;
  int trace;             // trace flags
  string dictFN;         // name of dictionary file
  Vocab *vocab;          // HTK representation
};

#endif
/*  -------------------- End of ADict.h --------------------- */

