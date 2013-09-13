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
/*        File: ANGram.h -  NGram Language Model Class         */
/* ----------------------------------------------------------- */

/* !HVER!ANGram: 1.6.0 [SJY 01/06/07] */

// Modification history:
//   02/08/05 -support for class N-grams added - SJY

// Configuration variables (Defaults as shown)
// ANGRAM: NGRAMFILE    = name of ngram file to load

// Supports standard ARPA N-Gram format and HLM Combined Class/NGram
// format in both text and binary formats.

#ifndef _ATK_ANGram
#define _ATK_ANGram

#include "AHTK.h"
#include "AResource.h"
#include "ADict.h"

class ANGram : public AResource {
public:
   // Default constructor, ngram file name in NGRAMFILE config var
   ANGram(const string& name);
   // Construct gram from specified NGram file
   ANGram(const string& name, const string& fname);
   // destructor
   ~ANGram();
   //init ngram from ARPA Format LM file
   void InitFromFile(const char * ngramFN);
   LModel *lm;    // the HTK/HLM language model
private:
   MemHeap lmHeap;
   Boolean isOpen;
};

#endif
/*  -------------------- End of ANGram.h --------------------- */
