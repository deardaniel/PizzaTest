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
/*      File: AHTK.h -    Interface to the HTK Libraries       */
/* ----------------------------------------------------------- */

/* !HVER!AHTK: 1.6.0 [SJY 01/06/07] */

// Modification history:
//   9/12/02 - added NCInitHTK and ReportErrors
//  29/07/04 - noGraphics switch added for linux version


#ifndef _ATK_HTK
#define _ATK_HTK

#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HSigP.h"
#include "HWave.h"
#include "HAudio.h"
#include "HParm.h"
#include "HLabel.h"
#include "HGraf.h"
#include "HModel.h"
#include "HUtil.h"
#include "HTrain.h"
#include "HAdapt.h"
#include "HFB.h"
#include "HDict.h"
#include "HLM.h"
#include "HNet.h"
#include "HRec.h"
#include "HThreads.h"
#include "HLat.h"
#include "HNBest.h"
#include <vector>
#include <string>
#include <list>
#include <map>
using namespace std;

// initialise the HTK libraries
ReturnStatus InitHTK(int argc, char *argv[], const char * app_version, Boolean noGraphics=FALSE);

// "no console" version of above
ReturnStatus NCInitHTK(char *configFile, const char * app_version, Boolean noGraphics=FALSE);

// find if system has a console or not
Boolean HasRealConsole();

// Define exceptions
struct HTK_Error {
  int i;
  HTK_Error(int ii) { i = ii; }
};

struct ATK_Error {
  int i;
  ATK_Error(int ii) { i = ii; }
};

// Report pending errors and die,
void ReportErrors(char *library, int errnum);

// Make an upper case copy of s in buf
char * UCase(const char *s, char *buf);

#endif  /* _ATK_HTK */
