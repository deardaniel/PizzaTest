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
/*   File: ASplash.cpp -   Implementation of Splashscreen      */
/* ----------------------------------------------------------- */

char * asplash_version="!HVER!ASplash: 1.6.0 [SJY 01/06/07]";
#include "ASplash.h"

Textbox::Textbox(HWin ww, string s, int x, int y, int w, int h)
{
   text = s; x0=x; y0=y; x1=x0+w; y1=y0+h;
   win = ww;
   col = BLACK;
   HSetColour(win,col);
   Redraw();
}

void Textbox::Redraw()
{
   HSetColour(win,WHITE);
   HFillRectangle(win,x0,y0,x1,y1);
   HSetColour(win,col);
   HPrintf(win,x0+3,y1-2,text.c_str());
}

Boolean Textbox::MouseInBox(HEventRec e)
{
   Boolean b = IsInRect(e.x,e.y,x0,y0,x1,y1);
   if (b && col==BLACK){
      col = RED; Redraw();
   }
   if (!b && col==RED){
      col = BLACK; Redraw();
   }
   return b;
}

void Textbox::ProcessEvent(HEventRec e)
{
   string s = " ";
   if (e.event== HKEYPRESS){
      switch (e.ktype){
      case NORMALKEY:
         if (text.length()<25){
            s[0]=e.c; text += s;
         }
         break;
      case DELKEY:
         if (text.length()>0)
            text.erase(text.length()-1,1);
         break;
      }
   }
   Redraw();
}

Boolean SplashScreen(string& userid, string& hostid, string& mictype)
{
   int y1,y2,y3,y4,y5,y6;
   int x1,x2,x3,x4,x5,x6;
   Boolean startPressed = FALSE;
   Boolean quitPressed = FALSE;
   HButton b,q;
   HEventRec e;
   int id;
   HWin win;

   win = MakeHWin("tHIS",200,100,400,200,1);
   if (win==NULL) HError(999,"SplashScreen cannot create splash window\n");
   HSetColour(win,WHITE);
   HFillRectangle(win,0,0,400,200);
   HSetColour(win,BLACK);


   b.x = 70; b.y = 150; b.w = 100; b.h=30;
   b.fg = DARK_GREY; b.bg = LIGHT_GREY; b.lit=FALSE;
   b.active = TRUE; b.toggle = FALSE; b.fontSize = 0;
   b.str = "START"; b.id = 1; b.win = win; b.next = &q; b.action = 0;

   q.x = 230; q.y = 150; q.w = 100; q.h=30;
   q.fg = DARK_GREY; q.bg = LIGHT_GREY; q.lit=FALSE;
   q.active = TRUE; q.toggle = FALSE; q.fontSize = 0;
   q.str = "QUIT"; q.id = 2; q.win = win; q.next = NULL; q.action = 0;

   HSetFontSize(win,24);
   HPrintf(win,44,30,"CUED ATK/HIS Dialog System");
   HSetFontSize(win,0);
   HDrawPanel(win,10,50,390,130,56);
   HSetColour(win,BLACK);
   HPrintf(win,30,74,"USER:"); Textbox ubox(win,userid,100,55,260,22);
   HPrintf(win,30,98,"HOST:"); Textbox hbox(win,hostid,100,79,260,22);
   HPrintf(win,30,122,"MIC :");Textbox mbox(win,mictype,100,103,260,22);
   RedrawHButtonList(&b);

   while(!startPressed && !quitPressed) {
      e = HGetEvent(NULL,NULL);
      // service window messages
      switch(e.event){
         case HWINCLOSE:
            quitPressed = TRUE;
            break;
         case HMOUSEDOWN:
            id = TrackButtons(&b,e);
            if (id>0){
               if (id==1) startPressed = TRUE;
               if (id==2) quitPressed = TRUE;
            }
            break;
         default:
            if (ubox.MouseInBox(e))
               ubox.ProcessEvent(e);
            if (hbox.MouseInBox(e))
               hbox.ProcessEvent(e);
            if (mbox.MouseInBox(e))
               mbox.ProcessEvent(e);
            break;
      }
   }
   CloseHWin(win);
   userid = ubox.text;
   hostid = hbox.text;
   mictype = mbox.text;
   return startPressed;
}

