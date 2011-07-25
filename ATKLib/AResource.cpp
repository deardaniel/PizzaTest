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
/*  File: AResource.cpp -   Implementation of Resource Class   */
/* ----------------------------------------------------------- */


char * aresource_version="!HVER!AResource: 1.6.0 [SJY 01/06/07]";

#include "AResource.h"

// Constructor: sets name and initialises lock
AResource::AResource(const string& name)
{
  rname = name;
  string s = "res:"+name;
  lock = HCreateLock(s.c_str());
  version = 0;
  refCount = 0;
  next = NULL;
}

// ------------------------ End AResource.cpp ---------------------


