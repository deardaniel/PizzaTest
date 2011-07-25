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
/*      File: AMonitor.h -    Interface for Monitor Class      */
/* ----------------------------------------------------------- */

/* !HVER!AMonitor: 1.6.0 [SJY 01/06/07] */

// Modification history:
//   9/12/02 - added display of thread print buffers
//   8/05/05 - termination cleaned up - SJY

#ifndef _ATK_Monitor
#define _ATK_Monitor

#include "AHTK.h"
#include "AComponent.h"

// Configuration variables (Defaults as shown)
// AMONITOR: DISPXORIGIN = 30
// AMONITOR: DISPYORIGIN = 20
// AMONITOR: DISPWIDTH   = 300

// ---------------- Component Monitor Class -----------------

// This class represents a system component consisting of a
// private thread maintaining a simple display and control
// interface for each active ATK component.  Only one instance
// of this class is allowed.

#define CMDBUFSIZE 256

class CompMonitor {    // monitor for individual component
public:
  CompMonitor(AComponentPtr comp, HWin win,
	      int x, int y, int width);
  void Redraw(int lev);
  void ProcessEvent(HEventRec e);
  int x0,y0;            // origin of displaylet rectangle
  int w,d;              // width and depth of displaylet
private:
  AComponentPtr theComp;  // the component being monitored
  HWin theWin;          // the window
  int x1,x2,x3,x4,x5,x6;// x anchor pts
  int y1,y2,y3,y4,y5,y6;// y anchor pts
  char cname[100];      // comp name as c string
  Boolean cmdActive;    // TRUE when cmd is being entered
  char buf[CMDBUFSIZE]; // cmd buffer
  int cmdLength;        // num chars in command
  int cursorPos;        // position of cursor (0=before 1st char)
  int msgSize;          // num lines in msg box
  int msgFont;          // font for msgs
  int cmdFont;          // font for commands
  int msgLineHeight;    // line height for msgs
};

typedef CompMonitor *CompMonPtr;

// ------------------- Main Monitor Class ------------------

class AMonitor {       // a system wide task monitor
public:
  AMonitor();
  void AddComponent(AComponentPtr comp);
  AComponentPtr FindComponent(const string& name);
  void Start();
  void Terminate();
private:
  friend TASKTYPE TASKMOD Monitor_Task(void * monp);
  void Redraw(int lev);
  void PassEvent(HEventRec e);
  list<AComponentPtr> compList;   // list of components
  HWin  theWin;        // the window
  int winx0,winy0;     // window origin
  int dlwidth;         // width of displaylet rectangle
  int width;           // window width
  int depth;           // window depth
  int margin;          // size of margin around displaylets
  int spacing;         // spacing between displaylets
  int dlx0,dlx1;       // x range of displaylets
  int dly0,dly1;       // y range of displaylets
  list<CompMonPtr> dispList; // list of displaylets
  int msgFont;          // font for msgs
  int msgLineHeight;   // line height for msgs
  int msgy0;           // start of main message area
  int msgy1;           // end of main message area
  Boolean terminated;  // enable to terminate monitor
};

#endif
/*  ---------------------- End of AMonitor.h ----------------------- */



