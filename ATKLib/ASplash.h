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
/*      File: ASplash.h -    Interface for Splash Screen       */
/* ----------------------------------------------------------- */

/* !HVER!ASplash: 1.6.0 [SJY 01/06/07] */
#ifndef _ATK_ASplash
#define _ATK_ASplash

#include "AHTK.h"

class Textbox {
public:
   Textbox(HWin ww, string s, int x, int y, int w, int h);
   Boolean MouseInBox(HEventRec e);
   void ProcessEvent(HEventRec e);
   void Redraw();
   int x0,y0,x1,y1;
   HColour col;
   string text;
   HWin win;
};

Boolean SplashScreen(string& userid, string& hostid, string& mictype);

#endif
