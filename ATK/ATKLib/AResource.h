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
/* File: AResource.h -  Interface for abstract Resource Class  */
/* ----------------------------------------------------------- */

/* !HVER!AResource: 1.6.0 [SJY 01/06/07] */

#ifndef _ATK_AResource
#define _ATK_AResource

#include "AHTK.h"

class AResource {
public:
  AResource(){}
  AResource(const string & name);
  string  rname;             // name of resource
  int version;
protected:
  friend class ARMan;
  friend class ResourceGroup;
  int refCount;              // reference counter
  HLock lock;                // shared access control
  AResource *next;           // used by ARMan to link resources
};

#endif

/* -------------------- End of AResource.h ------------------- */

