/* ----------------------------------------------------------- */
/*           _ ___   	     ___                               */
/*          |_| | |_/	  |_| | |_/    SPEECH                  */
/*          | | | | \  +  | | | | \    RECOGNITION             */
/*          =========	  =========    SOFTWARE                */
/*                                                             */
/* ================> ATK COMPATIBLE VERSION <================= */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                            */
/*      Machine Intelligence Laboratory (Speech Group)         */
/*      Cambridge University Engineering Department            */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/*      Entropic Cambridge Research Laboratory                 */
/*      (now part of Microsoft)                                */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright: Microsoft Corporation                    */
/*          1995-2000 Redmond, Washington USA                  */
/*                    http://www.microsoft.com                 */
/*                                                             */
/*          2001-2007 Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*        File: HGraf.c -   Portable graphics interface        */
/* ----------------------------------------------------------- */


/* ----------------------------------------------------------- */
/*       Original Win32 port Peter Silsbee                     */
/*       Updated for multiple windows by SJY Feb 01            */
/*       Linux support added Feb04 MNS                         */
/*       Message queueing reimplemented Jul04 MNS              */
/* ----------------------------------------------------------- */

char *hgraf_version = "!HVER!HGraf: 1.6.0 [SJY 01/06/07]";

/* HAUDOUT event added - SJY 23/08/05 */

#if !defined WINGRAFIX && !defined XGRAFIX && !defined NOGRAFIX
#define NOGRAFIX
#endif

/* define CAPTURE_ALT to allow application to respond to Alt-key presses. */
/* "Normal" windows application behavior is to allow the system to handle it. */
#define CAPTURE_ALT

#include "HShell.h"
#include "HMem.h"
#include "HMath.h"
#include "HGraf.h"

#ifdef WINGRAFIX
#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <memory.h>
#endif

#ifdef XGRAFIX
#include <X11/Xlib.h>      /* the X11 stuff makes string.h also available */
#include <X11/Xutil.h>
#include <X11/Xos.h>
#define XK_MISCELLANY      /* use miscellaneous keysym defs */
#include <X11/keysymdef.h>
#define MAX_GC 4  /* we need 1 GC for each transfer mode since changing
                     the transfer mode in the current GC is unreliable on
                     some X-servers that we have tested */
#define GSTP 4
#endif

#include "HThreads.h"

#define DEF_FONTSIZE 16           /* default font size */
#define MAX_POINT    64           /* max number of points for polygons */

/* -------------------- Window Records  ------------------------*/

typedef struct _HWindowRec{
  char winName[100];   /* Name of window */
  Boolean isActive;    /* TRUE if active ie not destroyed */
  Boolean writingToMeta;  /* TRUE if spooling this win to a metafile */
  int lineWidth;       /* Current line width */
  MemHeap btnHeap;     /* heap for HButton structures */
  HWin next;           /* Next window in list */
#ifdef WINGRAFIX
  HWND theWindow;      /* A handle to the graphics window */
  HDC memDC;           /* A handle to the memory device context */
  HDC DCSaved;         /* to store win->memDC when saving Metafile */
  HBITMAP theBitMap;   /* Internal representation of window contents */
  RECT  clientRect;    /* Absolute location of the window */
  HBRUSH theBrush;     /* Used to fill solid areas */
  HPEN thePen;         /* Used to draw lines */
  HPEN thinPen;        /* Always width=1, for outlining filled shapes */
  HFONT theFont;       /* Current font for text output */
  COLORREF curColour;  /* Current draw colour */
#endif
#ifdef XGRAFIX
  Display       *theDisp;
  Window        rootW, theWindow;
  int           theScreen;
  GC            theGC;
  GC            gcs[MAX_GC];
  Visual        *theVisual;
  XSizeHints    hints;
  Pixmap        thePixmap;
  XFontStruct  *CurrentFont;
  Bool          redraw;
  HThread       theThread;
#endif
} HWindowRec;

static HWin wroot = NULL;     /* list of created windows */

/* -------------------- global information ----------------------*/

static char *winClassName = "hgraf";  /* ... and its name */

#ifdef XGRAFIX
static long int colours[MAX_COLOURS];
#endif
#ifdef WINGRAFIX
static unsigned char colours[MAX_COLOURS][3];    /* r,g,b */
#endif
static long int greys[MAX_GREYS];
static int dispDEEP=0;         /* display characteristics */
static int dispWIDE=0;
static int dispHIGH=0;

static char *FONTNAME = "Helvetica";

#ifdef CAPTURE_ALT
enum _AltState {ALT_UP,ALT_DOWN}; /* keep track of Alt key */
typedef enum _AltState AltState;
static AltState AltKeyState = ALT_UP;
#endif

#ifdef WINGRAFIX
static WNDCLASS windowClass;          /* Global window class */
static POINT MousePos;   /* updated when a WM_MOUSEMOVE occurs */
         /* Win32 does not support direct querying of the mouse position */

LRESULT CALLBACK HGWinFunc(HWND theWindow, unsigned int msg,
                           WPARAM wParam, LPARAM lParam);

KeyType HGetKeyType(char c);    /* Map windows Virtual Key Codes */
#endif

#ifdef XGRAFIX
static char  *XColArray[] = {
   "white","yellow","orange","red",
   "plum", "purple","blue","lightblue",
   "darkgreen","palegreen", "brown","tan",
   "lightgray", "gray","darkslategray", "black"
};

static int TFuncX11[4] = {
   GXcopy, GXor, GXxor, GXinvert
};

void HandleExpose(XExposeEvent *xexpose);

void startXThread();

/* global things to put into window */
static Colormap   theCmap;
static unsigned long black, white;
static Display *globDisp;
static long int eMask;

#define NO_OF_FONTS 9
#define FONTS_AVAILABLE 9
static char *FontNm[NO_OF_FONTS] = {
   "-*-Medium-R-Normal-*-8-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-10-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-12-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-14-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-15-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-16-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-18-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-20-*-*-*-*-*-*-*",
   "-*-Medium-R-Normal-*-24-*-*-*-*-*-*-*" };

static char *FontNmBold[NO_OF_FONTS] = {
   "-*-Bold-R-Normal-*-8-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-10-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-12-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-14-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-15-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-16-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-18-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-20-*-*-*-*-*-*-*",
   "-*-Bold-R-Normal-*-24-*-*-*-*-*-*-*" };

static int FontSize[NO_OF_FONTS] = {8, 10, 12, 14, 15, 16, 18, 20, 24};
static XFontStruct  *DefaultFont, *CurrentFont, *FontInfo[NO_OF_FONTS], *FontInfoBold[NO_OF_FONTS];
#define FONT1 "-*-lucida-medium-r-*-*-12-*"
#define FONT2 "-*-helvetica-medium-r-*-*-12-*"
#define FONT3 "6x13"
#endif


/* --------------------------- Initialisation ---------------------- */

static ConfParam *cParm[MAXGLOBS];      /* config parameters */
static int nParm = 0;
static int trace = 0;                   /* Just for consistency */
static Boolean noGraph=FALSE;           /* for terminal-only modes */


/* LoadColours:  create basic colour map */
void LoadColours(void)
{
#ifdef WINGRAFIX
  /* WHITE */
   colours[0][0] = 255; colours[0][1] = 255; colours[0][2] = 255;
   /* YELLOW */
   colours[1][0] = 255; colours[1][1] = 255; colours[1][2] = 0;
   /* ORANGE */
   colours[2][0] = 255; colours[2][1] = 128; colours[2][2] = 0;
   /* RED */
   colours[3][0] = 255; colours[3][1] = 0;   colours[3][2] = 0;
   /* MAUVE */
   colours[4][0] = 196; colours[4][1] = 100; colours[4][2] = 255;
   /* PURPLE */
   colours[5][0] = 128; colours[5][1] = 0;   colours[5][2] = 128;
   /* DARK_BLUE */
   colours[6][0] = 0;   colours[6][1] = 0;   colours[6][2] = 196;
   /* LIGHT_BLUE (CYAN) */
   colours[7][0] = 0;   colours[7][1] = 255; colours[7][2] = 255;
   /* DARK_GREEN */
   colours[8][0] = 0;   colours[8][1] = 128; colours[8][2] = 0;
   /* LIGHT_GREEN */
   colours[9][0] = 0;   colours[9][1] = 255; colours[9][2] = 0;
   /* DARK_BROWN */
   colours[10][0] = 128;colours[10][1] = 64; colours[10][2] = 64;
   /* LIGHT_BROWN */
   colours[11][0] = 196;colours[11][1] = 140;colours[11][2] = 140;
   /* LIGHT_GREY */
   colours[12][0] = 196;colours[12][1] = 196;colours[12][2] = 196;
   /* GREY */
   colours[13][0] = 128;colours[13][1] = 128;colours[13][2] = 128;
   /* DARK_GREY */
   colours[14][0] = 64; colours[14][1] = 64; colours[14][2] = 64;
   /* BLACK */
   colours[15][0] = 0;  colours[15][1] = 0;  colours[15][2] = 0;
#endif
#ifdef XGRAFIX
   int pixVal=0;
   int c;
   XColor colourDef;

   for (c = 0; c < MAX_COLOURS; c++){
     /* get colour from the X11 database */
     if (!XParseColor(globDisp, theCmap, XColArray[c], &colourDef))
       HError(6870,"InstallColours: Colour name %s not in X11 database",
	      XColArray[c]);
     if (!XAllocColor(globDisp, theCmap, &colourDef))
       HError(-6870,"InstallColours: Cannot allocate colour %s", XColArray[c]);
     else
       pixVal = colourDef.pixel;
     colours[c] = pixVal;
   }
#endif
}

/* LoadGreys:  create basic grey map */
void LoadGreys(void)
{
#ifdef WINGRAFIX
   int step,c,pixVal=0;

   step = 256/MAX_GREYS;
   for (c = 0; c < MAX_GREYS; c++, pixVal+= step){
      greys[c] = pixVal;
   }
#endif
#ifdef XGRAFIX
   int c, f, ggap, steps[GSTP] = {8, 4, 2, 1};
   XColor greyDef, whiteDef, blackDef, colourDef;
   short RGBval, step;
   int pixVal=0;

   white     = colours[0];
   black     = colours[15];
   if (dispDEEP == 1){
     /* map all grey levels onto b/w */
     printf("single level display depth\n");
     for (c = 0; c < MAX_GREYS/2; c++)
       greys[c] = white;
     for (c = MAX_GREYS/2; c < MAX_GREYS; c++)
       greys[c] = black;
   } else {
     /* then the grey levels */
     whiteDef.pixel = white; XQueryColor(globDisp, theCmap, &whiteDef);
     blackDef.pixel = black; XQueryColor(globDisp, theCmap, &blackDef);
     ggap = ((int)(whiteDef.red - blackDef.red))/MAX_GREYS;
     for (f = 0; f < GSTP; f++){
       step = steps[f]*ggap;
       for (c = 0; c < (MAX_GREYS/steps[f]); c++){
	 RGBval = blackDef.red + c*step;
	 greyDef.red = RGBval;
	 greyDef.green = RGBval;
	 greyDef.blue  = RGBval;
	 if (!XAllocColor(globDisp, theCmap, &greyDef))
	   HError(-6870, "InstallColours: Cannot allocate grey level %d",
		  c*steps[f]);
	 else
	   pixVal = greyDef.pixel;
	 greys[c] = pixVal;
       }
     }
   }
#endif
}
/*------------------- Font Handling Routines -----------------------*/

#ifdef XGRAFIX

/* InstallFonts: install the HGraf font set */
static void InstallFonts(void)
{
   int  i;
   int nfonts, j;
   char **pfinfo;

   /* pfinfo=(char**)New(&gcheap,sizeof(char)*100*100);*/
   /* load user fonts */
   for (i=0; i < FONTS_AVAILABLE; i++) {
     if ((FontInfo[i] = XLoadQueryFont(globDisp, FontNm[i])) == NULL) {
        pfinfo=XListFonts(globDisp,  FontNm[i], 100, &nfonts);
	j=0;
	printf("%d matching fonts \n", nfonts);
	if(nfonts>0)
	  while(((FontInfo[i] = XLoadQueryFont(globDisp, pfinfo[j])) == NULL) && j<nfonts) {
	    j++;
	  }
	if(j==nfonts||nfonts==0)
	  HError(-6870, "InstallFonts: Cannot load font %s", FontNm[i]);
	XFreeFontNames(pfinfo);
     }
     if ((FontInfoBold[i] = XLoadQueryFont(globDisp, FontNmBold[i])) == NULL) {
        pfinfo=XListFonts(globDisp,  FontNmBold[i], 100, &nfonts);
	j=0;
	printf("%d matching fonts \n", nfonts);
	if(nfonts>0)
	  while(((FontInfo[i] = XLoadQueryFont(globDisp, pfinfo[j])) == NULL) && j<nfonts) {
	    j++;
	  }
	if(j==nfonts||nfonts==0)
	  HError(-6870, "InstallFonts: Cannot load font %s", FontNmBold[i]);
     	XFreeFontNames(pfinfo);
     }
   }
   /* load the default font */
   if ((DefaultFont = XLoadQueryFont(globDisp, FONT1))==NULL  &&
       (DefaultFont = XLoadQueryFont(globDisp, FONT2))==NULL  &&
       (DefaultFont = XLoadQueryFont(globDisp, FONT3))==NULL)
     HError(6870, "InstallFonts: Cannot load default font");
}
#endif

/* EXPORT->InitGraf: initialise memory and configuration parameters */
void InitGraf(Boolean noGraphics)
{
   int i;
   int screen;
   Boolean b;

   Register(hgraf_version);
   noGraph = noGraphics;
   nParm = GetConfig("HGRAF", TRUE, cParm, MAXGLOBS);
   if (nParm>0){
      if (GetConfInt(cParm,nParm,"TRACE",&i)) trace = i;
      if (GetConfBool(cParm,nParm,"NOGRAPHICS",&b)) noGraph = b;
   }
   if(!noGraph){
#ifdef WINGRAFIX
     windowClass.hInstance = GetModuleHandle(NULL);
     windowClass.lpszClassName = winClassName;
     windowClass.lpfnWndProc = HGWinFunc;
     windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
     windowClass.hIcon = NULL;
     windowClass.hCursor = LoadCursor(NULL,IDC_ARROW);
     windowClass.lpszMenuName = NULL;
     windowClass.cbClsExtra = 0;
     windowClass.cbWndExtra = 0;
     windowClass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
     RegisterClass(&windowClass);
     dispWIDE  = GetSystemMetrics(SM_CXSCREEN);
     dispHIGH  = GetSystemMetrics(SM_CYSCREEN);
#endif

#ifdef XGRAFIX
     globDisp=XOpenDisplay(0);  /*global display connection */
     if(globDisp==NULL)
       HError(9999, "Error opening display\n");
     screen=DefaultScreen(globDisp);
     InstallFonts();
     theCmap   = DefaultColormap(globDisp, screen);
     dispWIDE  = DisplayWidth(globDisp, screen);
     dispHIGH  = DisplayHeight(globDisp, screen);
     dispDEEP  = DisplayPlanes(globDisp, screen);
     white     = WhitePixel(globDisp,screen);
     black     = BlackPixel(globDisp,screen);
     eMask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
       ButtonReleaseMask | PointerMotionHintMask| PointerMotionMask;
#endif
     LoadColours(); LoadGreys();
#ifdef XGRAFIX
     startXThread();
#endif
   }
}

/* ----------------------------- Event Handling ------------------------------ */

#if defined(XGRAFIX) || (defined(UNIX) && defined(NOGRAFIX))
#ifdef XGRAFIX
HEventRec convertXevToHEventRec(XEvent xev);
HWin FindWindowRec(Window w);
#endif

void postEventToQueue(HThread thread, HEventRec r)
{
    QEntry qentry;
    HEventRec *hevent;

    if(thread==NULL)
      HError(9999,"NULL thread passed to postEventToQueue");

    hevent= malloc(sizeof (HEventRec));
    qentry = malloc(sizeof (QEntryRec));
    /* copy the event information */
    hevent->event=r.event; hevent->x=r.x; hevent->y=r.y;
    hevent->c=r.c; hevent->ktype=r.ktype; hevent->win=r.win;
    qentry->next=NULL;
    pthread_mutex_lock(&(thread->xeq.mux));
    /* if queue is empty */
    if (thread->xeq.tail == NULL) {
      thread->xeq.head = thread->xeq.tail = qentry;
    }
    else { /* add event to the tail of the queue */
      thread->xeq.tail->next = qentry;
      thread->xeq.tail = qentry;
    }
    qentry->next = NULL;
    qentry->hev = (void*)hevent;

    pthread_cond_signal(&(thread->xeq.cond));
    pthread_mutex_unlock(&(thread->xeq.mux));
}

/* return the number of events on the queue */
int eventsOnQueue(HThread thread)
{
   QEntry n;
   int i;

   i=0;
   if(thread==NULL)
      HError(9999,"NULL thread passed to eventsOnQueue");
   pthread_mutex_lock(&(thread->xeq.mux));
   n=thread->xeq.head;
   while (n!=NULL) {
      i++; n=n->next;
   }
   pthread_mutex_unlock(&(thread->xeq.mux));
   return i;
}


/* Pull an event from the thread event queue */
HEventRec getEventFromQueue()
{
   HEventRec r;
   HEventRec *rp;
   HThread t;
   QEntry qentry;
   t=HThreadSelf();

   HFlush();

   pthread_mutex_lock(&(t->xeq.mux));
   while (t->xeq.head == NULL) {
      pthread_cond_wait(&(t->xeq.cond), &(t->xeq.mux));
   }
   qentry = t->xeq.head;
   t->xeq.head = t->xeq.head->next;
   if (t->xeq.tail == qentry)
      t->xeq.tail = NULL;
   pthread_mutex_unlock(&(t->xeq.mux));

   rp=(HEventRec *)qentry->hev;

   r.event=rp->event;  r.x=rp->x;r.y=rp->y;
   r.c=rp->c;  r.ktype=rp->ktype;
   r.win=rp->win;

   free(qentry->hev);
   free(qentry);
   return r;
}

#ifdef XGRAFIX
TASKTYPE TASKMOD XEventThread(void *data)
{
   XEvent xev;
   HEventRec r;
   HWin win;
   HThread t;

   do{
      XNextEvent(globDisp, &xev);
      r=convertXevToHEventRec(xev);
      if(r.event != HIGNORE) {
         win=FindWindowRec(xev.xany.window);
         t=win->theThread;
         postEventToQueue(t, r);
      }
      HFlush();
   }
   while(True);
}

void startXThread()
{
   HThread t;
   int status;

   t = HCreateThread("XEventThread",10,HPRIO_NORM,XEventThread,(void *)0);
}


/* DecodeKeyPress: decode the given keypress into char+modifier */
static void DecodeKeyPress(XKeyEvent *xkev, HEventRec *hev)
{
   char buf[20];
   int n;
   KeySym key;
   XComposeStatus compose;

   n = XLookupString(xkev,buf,20,&key,&compose);
   hev->c = buf[0];
   switch (key) {
   case XK_Shift_L:
   case XK_Shift_R:
      hev->ktype = SHIFTKEY;
      break;
   case XK_Control_L:
   case XK_Control_R:
      hev->ktype = CONTROLKEY;
      break;
   case XK_Meta_L:
   case XK_Meta_R:
   case XK_Alt_L:
   case XK_Alt_R:
      hev->ktype = COMMANDKEY;
      break;
   case XK_Return:
   case XK_KP_Enter:
      hev->ktype = ENTERKEY;
      break;
   case XK_Escape:
      hev->ktype = ESCKEY;
      break;
   case XK_BackSpace:
   case XK_Delete:
      hev->ktype = DELKEY;
      break;
   default:
      hev->ktype = NORMALKEY;
   }
}

HWin FindWindowRec(Window w)
{
   HWin p;

   for (p=wroot; p!=NULL; p=p->next)
      if (p->theWindow == w)
	return p;
   return NULL;
}

HEventRec convertXevToHEventRec(XEvent xev)
{
  HEventRec r;
  switch (xev.type) {
  case ButtonPress:
    r.event = HMOUSEDOWN;
    r.x = xev.xbutton.x;
    r.y = xev.xbutton.y;
    break;
  case ButtonRelease:
    r.event = HMOUSEUP;
    r.x = xev.xbutton.x;
    r.y = xev.xbutton.y;
    break;
  case MotionNotify:
    r.event = HMOUSEMOVE;
    r.x = xev.xmotion.x;
    r.y = xev.xmotion.y;
    break;
  case KeyPress:
    r.event = HKEYPRESS;
    r.x = xev.xkey.x;
    r.y = xev.xkey.y;
    DecodeKeyPress(&(xev.xkey), &r);
    break;
  case KeyRelease:
    r.event = HKEYRELEASE;
    r.x = xev.xkey.x;
    r.y = xev.xkey.y;
    DecodeKeyPress(&(xev.xkey), &r);
    break;
  case ClientMessage:
    if(xev.xclient.data.l[0]==XInternAtom(globDisp, "WM_DELETE_WINDOW",True)) {
      r.event = HWINCLOSE;
      HSendAllThreadsCloseEvent();
    }
    else
      r.event = HIGNORE;
    break;
  case Expose:
    /*handle expose event */
    HandleExpose(&xev.xexpose);
    r.event = HREDRAW;  /* must return event, app can ignore */
    break;              /* since we have own handler */
  default:              /* but otherwise HEventsPending reports falsely*/
    r.event = HIGNORE;
    break;
  }
  return r;
}
#endif
#endif

/* EXPORT->HGetEvent: return next relevant event in event queue */
HEventRec HGetEvent(HWin win, void (*action)(void))
{
  HEventRec r={0,0,0,0,0};
  static KeyType SavedKeyType;
  Boolean hasEvent = FALSE;
#ifdef WINGRAFIX
  HWND theWindow = NULL;
  MSG msg;

  if (win!=NULL) theWindow=win->theWindow;
  do {
     if (action==NULL) {
        if(GetMessage(&msg,theWindow,0,0)==0){
           r.event=HQUIT;
           return r;
        }
        hasEvent=TRUE;
     } else {
        hasEvent=PeekMessage(&msg,theWindow,0,0,PM_REMOVE);
     }

     if (hasEvent) {
        TranslateMessage(&msg);
        switch (msg.message) {
      case WM_HTMONUPD:     /* sent when monitor display needs updating */
         r.event = HTMONUPD;
         break;
      case WM_AUDOUT:
         r.event = HAUDOUT;
         break;
      case WM_HTBUFFER:        /* sent when status of a buffer changes */
         r.event = HTBUFFER;
         r.c = (unsigned char) msg.wParam; /* id supplied by a listener */
         break;
      case WM_LBUTTONDOWN:
         r.event = HMOUSEDOWN;
	r.x = MousePos.x = LOWORD(msg.lParam);
	r.y = MousePos.y = HIWORD(msg.lParam);
	SetCapture(theWindow);
	break;
      case WM_LBUTTONUP:
	r.event = HMOUSEUP;
	r.x = MousePos.x = LOWORD(msg.lParam);
	r.y = MousePos.y = HIWORD(msg.lParam);
	ReleaseCapture();
	break;
      case WM_MOUSEMOVE:
	r.event = HMOUSEMOVE;
	r.x = MousePos.x = LOWORD(msg.lParam);
	r.y = MousePos.y = HIWORD(msg.lParam);
	break;
#ifdef CAPTURE_ALT     /* alt key events are normally intended for the system */
      case WM_SYSKEYDOWN:
	r.c = (unsigned char) msg.wParam;
	if (r.c == VK_MENU) {
	  r.ktype = COMMANDKEY;
	  r.event = HKEYPRESS;
	  r.x = MousePos.x;
	  r.y = MousePos.y;
	  AltKeyState = ALT_DOWN;
	  break;
	} /* else fall through to regular keydown */
#endif
      case WM_KEYDOWN:
	r.event = HKEYPRESS;
	r.x = MousePos.x;
	r.y = MousePos.y;
	r.c = (unsigned char) msg.wParam;
	SavedKeyType = r.ktype = HGetKeyType(r.c);
	if ((r.c != VK_DELETE) &&
	    (r.ktype != LEFTARROWKEY) &&
	    (r.ktype != RIGHTARROWKEY) &&
	    (r.ktype != CONTROLKEY) &&
	    (r.ktype != COMMANDKEY) &&
	    (r.ktype != SHIFTKEY))
	  hasEvent = FALSE; /* other keys will be processed by WM_CHAR */
	/* message which should arrive shortly */
	break;
#ifdef CAPTURE_ALT
      case WM_SYSCHAR:
#endif
      case WM_CHAR:
	r.event = HKEYPRESS;
	r.x = MousePos.x;
	r.y = MousePos.y;
	r.c = (unsigned char) msg.wParam;
	r.ktype = SavedKeyType; //HGetKeyType(r.c);
	break;
#ifdef CAPTURE_ALT
      case WM_SYSKEYUP:
	r.c = (unsigned char) msg.wParam;
	if (r.c == VK_MENU) {
	  r.ktype = COMMANDKEY;
	  r.event = HKEYRELEASE;
	  r.x = MousePos.x;
	  r.y = MousePos.y;
	  AltKeyState = ALT_UP;
	  break;
	} /* else fall through to regular keydown */
#endif
      case WM_KEYUP:
	r.event = HKEYRELEASE;
	r.x = MousePos.x;
	r.y = MousePos.y;
	r.c = (unsigned char) msg.wParam;
	r.ktype = HGetKeyType(r.c);
            break;
      case WM_PAINT:
	DispatchMessage(&msg);    /* force Win32 to remove message from   */
	/* queue even though application is dispatching this message.     */
	/* Applications should actually be able to ignore HREDRAW events. */
	/* Fall through to next messages */
      case WM_SIZING:
      case WM_MOVING:
      case WM_EXITSIZEMOVE:
         r.event = HREDRAW;
         break;
      case WM_NCLBUTTONDOWN:
         if ((int)msg.wParam == HTCLOSE) {
            r.event = HWINCLOSE;
            HSendAllThreadsCloseEvent();
         }else
            r.event = HIGNORE;
         DispatchMessage(&msg);
         break;
      case WM_HREQCLOSE:        /* sent when close event has been trapped */
         r.event = HWINCLOSE;
         break;
      default:
         r.event = HIGNORE;
         DispatchMessage(&msg); /* Win32 should handle other messages */
        }
     }
    else if (action != NULL) {
      (*action)();
    }
  } while (!hasEvent);
#endif
#if defined(XGRAFIX) || defined(NOGRAFIX)
  Boolean found,dummy;
   found = FALSE;
   do {
/*     if(win!=NULL)
       HError(9999,"Xwindows window-specific queues not supported (yet)\n");
*/     if( (eventsOnQueue(HThreadSelf())!=0) || (action==NULL)) {
       r=getEventFromQueue();
       if(r.event != HIGNORE)
	 found=TRUE;
     }
     else {
       (*action)();
       HFlush();
     }
     HFlush();
   }
   while(found==FALSE);
#endif
   return r;
}

/* EXPORT->HEventsPending: Return number of events pending           */
/* This doesn't seem to be supported in Win32. It is possible        */
/* to see if the queue is empty, but there is no way to see if       */
/* there is just one event or if there are many. This function       */
/* should probably return a Boolean value. Currently it returns 1    */
/* if there are one or more events pending, and 0 if there are none. */
int HEventsPending(HWin win)
{
#ifdef WINGRAFIX
   MSG msg;
   HWND theWindow = NULL;

   if (win!=NULL) theWindow=win->theWindow;
   if (PeekMessage(&msg,theWindow,0,0,PM_NOREMOVE)){
      return 1;
   }
#endif
#if defined(XGRAFIX)
   Display *display;
   display=globDisp;
/*   if(win!=NULL)
 HError(9999, "No support for number of events for window pending");
 */
#endif
#if defined(XGRAFIX) || defined(NOGRAFIX)
   return eventsOnQueue(HThreadSelf());
#endif
 return 0;
}

/* EXPORT->HMousePos: return mouse pos in x, y, returns TRUE if the pointer
   is on the window */
/* Win32: We only get mouse position information when (a) there is an event  */
/* when the mouse is positioned over the window, or (b) when we are capturing */
/* the mouse. We only capture the mouse when the mouse button is depressed.   */
/* Thus, the mouse position determined from this function may not be up to    */
/* date. This shouldn't be a problem since the "focus" is not determined by   */
/* mouse position in Win32. That is, keyboard events that take place when the */
/* mouse is outside the window still really do belong to our window, if the   */
/* event has been directed to our application.                                */

Boolean HMousePos(HWin win, int *x, int *y)
{
#ifdef WINGRAFIX
   *x = MousePos.x;
   *y = MousePos.y;
   return (Boolean) IsInRect(*x,*y,win->clientRect.left,win->clientRect.top,
                             win->clientRect.right,win->clientRect.bottom);
#elif defined(XGRAFIX)
   Window root,child;
   int rx,ry;
   unsigned int keys;
   Boolean retval;


   retval=XQueryPointer(win->theDisp, win->theWindow, &root, &child, &rx, &ry, x, y, &keys);

   return retval;
#endif
}

/* EXPORT: IsInRect: return TRUE iff (x,y) is in the rectangle (x0,y0,x1,y1) */
Boolean IsInRect(int x, int y, int x0, int y0, int x1, int y1)
{
   return (x >= x0 && x<=x1 && y >= y0 && y <= y1);
}

/* ------------------------- Colour Handling ------------------------------ */

#ifdef WINGRAFIX
/* InstallPalette: install given palette in given dc */
static void InstallPalette(HPALETTE hpal, HDC dc)
{
   HPALETTE old;

   old = SelectPalette(dc,hpal,FALSE);
   RealizePalette(dc);
   SelectPalette(dc,old,FALSE);
}

/* CompressColours: if display has limited colour depth */
static void CompressColours(HWin win)
{
   int c;
   LOGPALETTE *pal;
   HPALETTE hpal;
   HDC dc;

   pal = (LOGPALETTE *)
   New(&gcheap,2*sizeof(WORD) + (MAX_GREYS + MAX_COLOURS)*sizeof(PALETTEENTRY));
   pal->palVersion = 0x300;
   pal->palNumEntries = MAX_GREYS + MAX_COLOURS;

   /* most important colors should be first in list. Black, White,
      the rest of the colors, then the greys. */
   pal->palPalEntry[0].peRed=pal->palPalEntry[0].peGreen=pal->palPalEntry[0].peBlue = 0;
   pal->palPalEntry[1].peRed=pal->palPalEntry[1].peGreen=pal->palPalEntry[1].peBlue = 255;
   pal->palPalEntry[1].peFlags = pal->palPalEntry[0].peFlags = 0;;
   for (c=2;c<MAX_COLOURS;c++) {
      pal->palPalEntry[c].peRed = colours[c-1][0];
      pal->palPalEntry[c].peGreen = colours[c-1][1];
      pal->palPalEntry[c].peBlue = colours[c-1][2];
      pal->palPalEntry[c].peFlags = 0;
   }
   for (c=0;c<MAX_GREYS;c++) {
      pal->palPalEntry[MAX_COLOURS+c].peRed = greys[c];
      pal->palPalEntry[MAX_COLOURS+c].peGreen = greys[c];
      pal->palPalEntry[MAX_COLOURS+c].peBlue = greys[c];
      pal->palPalEntry[MAX_COLOURS+c].peFlags = 0;
   }
   hpal = CreatePalette(pal);
   InstallPalette(hpal,win->memDC);
   dc = GetDC(win->theWindow);
   InstallPalette(hpal,dc);
   ReleaseDC(win->theWindow,dc);
   DeleteObject(hpal);
}
#endif

/* EXPORT-> HSetColour: Set current colour to c */
void HSetColour(HWin win, HColour c)
{
#ifdef WINGRAFIX
   win->curColour = RGB(colours[c][0],colours[c][1],colours[c][2]);
   if (win->theBrush) DeleteObject(win->theBrush);
   win->theBrush = CreateSolidBrush(win->curColour);
   if (win->thePen) DeleteObject(win->thePen);
   win->thePen = CreatePen(PS_SOLID,win->lineWidth,win->curColour);
   if (win->thinPen) DeleteObject(win->thinPen);
   win->thinPen = CreatePen(PS_SOLID,1,win->curColour);
#endif
#ifdef XGRAFIX
   XferMode  xf;


   for (xf = GCOPY; xf < GINVERT; xf=(XferMode) (xf+1))   /* change all GCs except GINVERT*/
     XSetForeground(win->theDisp, win->gcs[(int) xf], colours[(int) c]);
   /*  XUnlockDisplay(win->theDisp); */
#endif
}

/* EXPORT-> HSetGrey: Set current colour to grey level g */
void HSetGrey(HWin win, int g)
{
#ifdef WINGRAFIX
   win->curColour = RGB(greys[g],greys[g],greys[g]);
   if (win->theBrush) DeleteObject(win->theBrush);
   win->theBrush = CreateSolidBrush(win->curColour);
   if (win->thePen) DeleteObject(win->thePen);
   win->thePen = CreatePen(PS_SOLID,win->lineWidth,win->curColour);
   if (win->thinPen) DeleteObject(win->thinPen);
   win->thinPen = CreatePen(PS_SOLID,1,win->curColour);
#endif
#ifdef XGRAFIX
   XferMode  xf;

   for (xf = GCOPY; xf < GINVERT; xf=(XferMode) (xf+1))   /* change all GCs except GINVERT*/
     XSetForeground(win->theDisp, win->gcs[(int) xf], greys[g]);

#endif

}

/* CheckCorners: make sure (x0,y0) is north-west of (x1,y1) */
static void CheckCorners(int *x0, int *y0, int *x1, int *y1)
{
   int a,b,c,d;

   if (*x0<*x1) {a=*x0; c=*x1;} else {a=*x1; c=*x0;}
   if (*y0<*y1) {b=*y0; d=*y1;} else {b=*y1; d=*y0;}
   *x0=a; *y0=b; *x1=c; *y1=d;
}

/* EXPORT-> HDrawLines: Draw multiple lines */
void HDrawLines(HWin win, HPoint *points, int n)
{
#ifdef WINGRAFIX
   POINT winPoints[MAX_POINT];
   int i;
   HDC dc;
   HGDIOBJ oldObject;

   if (n>MAX_POINT)
      HError(6815, "HDrawLines: can only specify up to %d points",MAX_POINT);
   for(i=0; i<n; i++) {
      winPoints[i].x=points[i].x;
      winPoints[i].y=points[i].y;
   }
   dc = GetDC(win->theWindow);
   oldObject = SelectObject(win->memDC,win->thePen);
   Polyline(win->memDC, winPoints, n);
   SelectObject(win->memDC,oldObject);
   oldObject = SelectObject(dc,win->thePen);
   Polyline(dc, winPoints, n);
   SelectObject(dc,oldObject);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX

    XDrawLines(win->theDisp, win->theWindow, win->theGC, (XPoint *) points, n, CoordModeOrigin);
    XDrawLines(win->theDisp, win->thePixmap, win->theGC, (XPoint *) points, n, CoordModeOrigin);
    win->redraw=TRUE;

#endif
}

/* EXPORT-> HDrawRectangle: draw a rectangle */
void HDrawRectangle(HWin win, int x0, int y0, int x1, int y1)
{
#ifdef WINGRAFIX
   POINT points[5];
   HGDIOBJ oldObject = SelectObject(win->memDC,win->thePen);
   HDC dc = GetDC(win->theWindow);

   CheckCorners(&x0,&y0,&x1,&y1);
   points[0].x = x0; points[0].y = y0;
   points[1].x = x0; points[1].y = y1;
   points[2].x = x1; points[2].y = y1;
   points[3].x = x1; points[3].y = y0;
   points[4].x = x0; points[4].y = y0;

   Polyline(win->memDC, points, 5);
   SelectObject(win->memDC,oldObject);
   oldObject = SelectObject(dc,win->thePen);
   Polyline(dc, points, 5);
   SelectObject(dc,oldObject);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX
   CheckCorners(&x0,&y0,&x1,&y1);

   XDrawRectangle(win->theDisp, win->theWindow, win->theGC, x0, y0, x1 - x0, y1 - y0);
   XDrawRectangle(win->theDisp, win->thePixmap, win->theGC, x0, y0, x1 - x0, y1 - y0);
   win->redraw=TRUE;

#endif

}

/* EXPORT-> HFillRectangle: fill a rectangle */
void HFillRectangle(HWin win, int x0, int y0, int x1, int y1)
{
#ifdef WINGRAFIX
   HDC dc = GetDC(win->theWindow);
   HGDIOBJ oldBrush = SelectObject(win->memDC,win->theBrush);
   HGDIOBJ oldPen = SelectObject(win->memDC,win->thinPen);

   CheckCorners(&x0,&y0,&x1,&y1);
   Rectangle(win->memDC,x0,y0,x1,y1);
   SelectObject(win->memDC,oldBrush);
   SelectObject(win->memDC,oldPen);

   oldBrush = SelectObject(dc,win->theBrush);
   oldPen = SelectObject(dc,win->thinPen);
   Rectangle(dc,x0,y0,x1,y1);
   SelectObject(dc,oldBrush);
   SelectObject(dc,oldPen);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX
   CheckCorners(&x0,&y0,&x1,&y1);

   XFillRectangle(win->theDisp, win->theWindow, win->theGC, x0, y0, x1 - x0, y1 - y0);
   XFillRectangle(win->theDisp, win->thePixmap, win->theGC, x0, y0, x1 - x0, y1 - y0);

   win->redraw=TRUE;
#endif
}

/* EXPORT-> HDrawLine: Draw one line */
void HDrawLine(HWin win, int x0, int y0, int x1, int y1)
{
#ifdef WINGRAFIX
   HDC dc = GetDC(win->theWindow);
   HGDIOBJ oldObject = SelectObject(win->memDC,win->thePen);

   MoveToEx(win->memDC,x0,y0,NULL);
   LineTo(win->memDC,x1,y1);
   SelectObject(win->memDC,oldObject);
   oldObject = SelectObject(dc,win->thePen);
   MoveToEx(dc,x0,y0,NULL);
   LineTo(dc,x1,y1);
   SelectObject(dc,oldObject);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX

   XDrawLine(win->theDisp, win->theWindow, win->theGC, x0, y0, x1, y1);
   XDrawLine(win->theDisp, win->thePixmap, win->theGC, x0, y0, x1, y1);

   win->redraw=TRUE;
#endif
}

/* EXPORT-> HFillPolygon: fill a convex polygon */
void HFillPolygon(HWin win, HPoint *points, int n)
{
#ifdef WINGRAFIX
   POINT winPoints[MAX_POINT];
   int i;
   HDC dc;
   HGDIOBJ oldPen;
   HGDIOBJ oldBrush;

   if (n>MAX_POINT)
      HError(6815, "HFillPolygon: can only specify up to %d points",MAX_POINT);
   for(i=0; i<n; i++) {
      winPoints[i].x=points[i].x;
      winPoints[i].y=points[i].y;
   }
   dc = GetDC(win->theWindow);
   oldPen = SelectObject(win->memDC,win->thinPen);
   oldBrush = SelectObject(win->memDC,win->theBrush);

   Polygon(win->memDC,winPoints,n);

   SelectObject(win->memDC,oldBrush);
   SelectObject(win->memDC,oldPen);
   oldBrush = SelectObject(dc,win->theBrush);
   oldPen = SelectObject(dc,win->thinPen);
   Polygon(dc,winPoints,n);
   SelectObject(dc,oldBrush);
   SelectObject(dc,oldPen);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX

   XFillPolygon(win->theDisp, win->theWindow, win->theGC, (XPoint *) points, n, Convex, CoordModeOrigin);
   XFillPolygon(win->theDisp, win->thePixmap, win->theGC, (XPoint *) points, n, Convex, CoordModeOrigin);

   win->redraw=TRUE;
#endif
}

/* EXPORT-> HDrawPanel: draw a raised panel */
void HDrawPanel(HWin win, int x0, int y0, int x1, int y1, int grlev)
{
   HSetGrey(win,grlev/3);
   HDrawRectangle(win,x0,y0,x1,y1);
   HSetGrey(win,63);
   HDrawLine(win,x0,y0,x1,y0);
   HDrawLine(win,x0,y0,x0,y1);
   HSetGrey(win,grlev);
   HFillRectangle(win,x0+1,y0+1,x1-1,y1-1);
}

/* EXPORT-> HDrawArc: Draw arc from stAngle thru arcAngle degrees */
void HDrawArc(HWin win, int x0, int y0, int x1, int y1, int stAngle, int arcAngle)
{
#ifdef WINGRAFIX
   int Center_x = (x0+x1)/2;
   int Center_y = (y0+y1)/2;
   int StartArc_x, StartArc_y;
   int EndArc_x, EndArc_y;
   int radius; /* major axis */
   double startAngle, endAngle,convrt = PI/180; /* degrees to radians */
   HGDIOBJ oldObject = SelectObject(win->memDC,win->thePen);
   HDC dc = GetDC(win->theWindow);
   CheckCorners(&x0,&y0,&x1,&y1);
   startAngle = stAngle*convrt;
   endAngle = (arcAngle+stAngle)*convrt;
   radius = (((x1-x0) > (y1-y0)) ? x1-x0 : y1-y0)/2;
   StartArc_x = Center_x + (int) (radius * cos((double) startAngle));
   StartArc_y = Center_y - (int) (radius * sin((double) startAngle));
   EndArc_x = Center_x + (int) (radius * cos((double) endAngle));
   EndArc_y = Center_y - (int) (radius * sin((double) endAngle));
   Arc(win->memDC,x0,y0,x1,y1,StartArc_x,StartArc_y,EndArc_x,EndArc_y);
   SelectObject(win->memDC,oldObject);
   oldObject = SelectObject(dc,win->thePen);
   Arc(dc,x0,y0,x1,y1,StartArc_x,StartArc_y,EndArc_x,EndArc_y);
   SelectObject(dc,oldObject);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX
  unsigned int rw, rh;

   CheckCorners(&x0,&y0,&x1,&y1);
   /* calculate width and height */
   rw = abs(x1 - x0); rh = abs(y1 - y0);
   /* the angles are signed integers in 64ths of a degree */
   stAngle *=64; arcAngle*=64;

   XDrawArc(win->theDisp, win->theWindow, win->theGC, x0, y0, rw, rh, stAngle, arcAngle);
 XDrawArc(win->theDisp, win->thePixmap, win->theGC, x0, y0, rw, rh, stAngle, arcAngle);

   win->redraw=TRUE;
#endif
}

/* EXPORT-> HFillArc: Draw filled arc from stAngle thru arcAngle degrees */
void HFillArc(HWin win, int x0,int y0,int x1,int y1,int stAngle,int arcAngle)
{
#ifdef WINGRAFIX
   int radius;
   int Center_x = (x0+x1)/2;
   int Center_y = (y0+y1)/2;
   int StartArc_x,StartArc_y;
   int EndArc_x,EndArc_y;
   HGDIOBJ oldBrush = SelectObject(win->memDC,win->theBrush);
   HGDIOBJ oldPen = SelectObject(win->memDC,win->thinPen);
   HDC dc = GetDC(win->theWindow);
   double startAngle, endAngle,convrt = PI/180; /* degrees to radians */

   CheckCorners(&x0,&y0,&x1,&y1);
   /* calculate point locations */
   startAngle = stAngle*convrt;
   endAngle = (stAngle+arcAngle)*convrt;
   radius = (((x1-x0) > (y1-y0)) ? x1-x0 : y1-y0)/2;
   StartArc_x = Center_x + (int) (radius * cos((double) startAngle));
   StartArc_y = Center_y - (int) (radius * sin((double) startAngle));
   EndArc_x = Center_x + (int) (radius * cos((double) endAngle));
   EndArc_y = Center_y - (int) (radius * sin((double) endAngle));
   Pie(win->memDC,x0,y0,x1,y1,StartArc_x,StartArc_y,EndArc_x,EndArc_y);
   SelectObject(win->memDC,oldBrush);
   SelectObject(win->memDC,oldPen);
   oldBrush = SelectObject(dc,win->theBrush);
   oldPen = SelectObject(dc,win->thinPen);
   Pie(dc,x0,y0,x1,y1,StartArc_x,StartArc_y,EndArc_x,EndArc_y);
   SelectObject(dc,oldBrush);
   SelectObject(dc,oldPen);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX
  unsigned int rw, rh;

   CheckCorners(&x0,&y0,&x1,&y1);
   /* calculate width and height */
   rw = abs(x1 - x0); rh = abs(y1 - y0);
   /* the angles are signed integers in 64ths of a degree */
   stAngle *=64; arcAngle*=64;

   XFillArc(win->theDisp, win->theWindow, win->theGC, x0, y0, rw, rh, stAngle, arcAngle);
   XFillArc(win->theDisp, win->thePixmap, win->theGC, x0, y0, rw, rh, stAngle, arcAngle);

   win->redraw=TRUE;
#endif
}

/* EXPORT-> HPrintf: works as printf on the graphics window at (x,y) */
void HPrintf(HWin win, int x, int y, const char *format, ...)
{
#ifdef WINGRAFIX
   va_list arg;
   char s[256];
   HGDIOBJ oldObject = SelectObject(win->memDC,win->theFont);
   HDC dc = GetDC(win->theWindow);

   va_start(arg, format);
   vsprintf(s, format, arg);
   SetTextColor(win->memDC,win->curColour);
   TextOut(win->memDC,x,y,s,strlen(s));
   SelectObject(win->memDC,oldObject);

   oldObject = SelectObject(dc,win->theFont);
   SetTextColor(dc,win->curColour);
   TextOut(dc,x,y,s,strlen(s));
   SelectObject(dc,oldObject);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX
   va_list arg;
   char s[256];

   va_start(arg, format);
   vsprintf(s, format, arg);

   XDrawString(win->theDisp, win->theWindow, win->theGC, x, y, s, strlen(s));
   XDrawString(win->theDisp, win->thePixmap, win->theGC, x, y, s, strlen(s));

   win->redraw=TRUE;
#endif
}

/* EXPORT-> copy rectangular area of the drawable */
void HCopyArea(HWin win, int srcx, int srcy, int width, int height,
	       int destx, int desty)
{
#ifdef WINGRAFIX
   HDC dc = GetDC(win->theWindow);

   BitBlt(win->memDC,destx,desty,width,height,win->memDC,srcx,srcy,SRCCOPY);
   BitBlt(dc,destx,desty,width,height,dc,srcx,srcy,SRCCOPY);
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX

   XCopyArea(win->theDisp, win->thePixmap, win->theWindow, win->theGC, srcx, srcy, width,
	     height, destx, desty);
   XCopyArea(win->theDisp, win->thePixmap, win->thePixmap, win->theGC, srcx, srcy, width,
	     height, destx, desty);

   win->redraw=TRUE;
#endif
}

/* EXPORT-> HPlotVector: plot vector v in given rectangle */
void HPlotVector(HWin win, int x0, int y0, int x1, int y1, Vector v,
		 int st, int en, float ymax, float ymin)
{
   float yScale, yOffset, xInc, x;
   int   xOld, yOld, ix, iy, i;

   if (st >= en || st < 1 || en > VectorSize(v))
      HError(6815, "HPlotVector: Plot indices %d -> %d out of range", st, en);
   x = (x1 - x0 - 1); xInc = x/(en - st);
   yScale  = (y1 - y0)/(ymin - ymax);
   yOffset = y0 - ymax*yScale;
   x = x0; xOld = x; yOld = v[st]*yScale + yOffset;
   for (i = st+1; i <= en; i++){
      x += xInc; ix = x;
      iy = v[i]*yScale + yOffset;
      HDrawLine(win, xOld,yOld,ix,iy);
      xOld = ix; yOld = iy;
   }
}

/* ----------------------------- Global Settings ------------------------------- */

/* EXPORT-> HSetFontSize: Set font size in points, 0 selects the default font
            if size -ve then weight is normal, else weight is bold */
void HSetFontSize(HWin win, int size)
{
#ifdef WINGRAFIX
   int weight = FW_BOLD;
   int fontSize = size;
   if (size==0) fontSize = DEF_FONTSIZE;
   if (size<0) { fontSize = -size; weight = FW_NORMAL; }

   if (win->theFont) DeleteObject(win->theFont);
   win->theFont = CreateFont(fontSize,
                        0,0,0,weight,
                        0,0,0,ANSI_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY,
                        DEFAULT_PITCH | FF_SWISS,
                        FONTNAME);
#endif
#ifdef XGRAFIX
   int i, bestf, d, min_d, abSize;
   XferMode  xf;

   abSize=abs(size);
   if (size==0)
     win->CurrentFont = DefaultFont;
   else {
     min_d = INT_MAX; bestf = 0;
     for (i=0; i < FONTS_AVAILABLE; i++) {
       d=abs(FontSize[i] - abSize);
       if (d < min_d) {
	 min_d = d; bestf = i;
       }
     }
     if(size<0) {
       if( FontInfo[bestf] ==NULL)
	 HError(9999, "Did not load font %s \n", FontNm[bestf]);
       else
	 win->CurrentFont = FontInfo[bestf];
     }
     else {
       if( FontInfo[bestf] ==NULL)
	 HError(9999, "Did not load font %s \n", FontNmBold[bestf]);
       else
	 win->CurrentFont = FontInfoBold[bestf];
     }
   }
   for (xf = GCOPY; xf <= GINVERT; xf=(XferMode) (xf+1))   /* change all GCs */
     XSetFont(win->theDisp, win->gcs[(int) xf], win->CurrentFont->fid);

#endif
}

/* EXPORT-> HSetLineWidth: set the line width */
void HSetLineWidth(HWin win, int w)
{
#ifdef WINGRAFIX
   win->lineWidth = w;

   if (win->thePen) DeleteObject(win->thePen);
   win->thePen = CreatePen(PS_SOLID,win->lineWidth,win->curColour);
#endif
#ifdef XGRAFIX
   XferMode xf;

   for (xf = GCOPY; xf <= GINVERT; xf=(XferMode) (xf+1))   /* change all GCs */
     XSetLineAttributes(win->theDisp,win->gcs[(int)xf],w,LineSolid,JoinRound,FillSolid);

#endif
}

/* EXPORT-> HSetXMode: Set current transfer mode */
void HSetXMode(HWin win, XferMode m)
{
#ifdef WINGRAFIX
   HDC dc = GetDC(win->theWindow);
   switch(m) {
   case GCOPY:
      SetROP2(win->memDC,R2_COPYPEN);
      SetROP2(dc,R2_COPYPEN);
      break;
   case GOR:
      SetROP2(win->memDC,R2_MERGEPEN);
      SetROP2(dc,R2_MERGEPEN);
      break;
   case GXOR:
      SetROP2(win->memDC,R2_XORPEN);
      SetROP2(dc,R2_XORPEN);
      break;
   case GINVERT:
      SetROP2(win->memDC,R2_NOT);
      SetROP2(dc,R2_NOT);
      break;
   default: /* GCOPY */
      SetROP2(win->memDC,R2_COPYPEN);
      SetROP2(dc,R2_COPYPEN);
      break;
   }
   ReleaseDC(win->theWindow,dc);
#endif
#ifdef XGRAFIX
   win->theGC = win->gcs[(int) m];
#endif
}

/* EXPORT-> CentreX: return position at which the the h-center of str will be at x */
int CentreX(HWin win, int x, char *str)
{
#ifdef WINGRAFIX
   SIZE size;
   HGDIOBJ oldObject = SelectObject(win->memDC,win->theFont);
   GetTextExtentPoint32(win->memDC,str,strlen(str),&size);
   SelectObject(win->memDC,oldObject);
   return  (x-size.cx/2);
#endif
#ifdef XGRAFIX

   return (x - XTextWidth(win->CurrentFont, str, strlen(str))/2);

#endif
}

/* EXPORT-> CentreY: return position at which the the v-center of str will be at y */
int CentreY(HWin win, int y, char *str)
{
#ifdef WINGRAFIX
   HDC dc;
   HGDIOBJ obj;
   TEXTMETRIC tm;
   int pos;

   dc = GetDC(win->theWindow);
   obj = SelectObject(dc,win->theFont);
   GetTextMetrics(dc,&tm);
   pos = (y - ((tm.tmAscent + tm.tmDescent)/2) + tm.tmAscent);
   SelectObject(dc,obj);
   return pos;
#endif
#ifdef XGRAFIX
   return (y - ((win->CurrentFont->ascent + win->CurrentFont->descent)/2) + win->CurrentFont->ascent);
#endif
}

/* EXPORT HTextWidth: return the width of s in pixels */
int HTextWidth(HWin win, const char *str)
{
#ifdef WINGRAFIX
   SIZE size;

   HGDIOBJ oldObject = SelectObject(win->memDC,win->theFont);
   GetTextExtentPoint32(win->memDC,str,strlen(str),&size);
   SelectObject(win->memDC,oldObject);
   return  (size.cx);
#endif
#ifdef XGRAFIX

   return XTextWidth(win->CurrentFont, str, strlen(str));

#endif

}

/* EXPORT HTextHeight: return the height of s in pixels */
int HTextHeight(HWin win, const char *str)
{
#ifdef WINGRAFIX
   SIZE size;

   HGDIOBJ oldObject = SelectObject(win->memDC,win->theFont);
   GetTextExtentPoint32(win->memDC,str,strlen(str),&size);
   SelectObject(win->memDC,oldObject);
   return  (size.cy);
#endif
#ifdef XGRAFIX
  return win->CurrentFont->ascent + win->CurrentFont->descent;
#endif

}

/* --------------------------- Misc/Button Routines -----------------------------*/

/* EXPORT->HDrawImage: draw grey scale image stored in p */
void HDrawImage(HWin win, unsigned char *p, int x, int y, int width, int height)
{
#ifdef WINGRAFIX
   HDC tdc = GetDC(win->theWindow);
   HDC dc = CreateCompatibleDC(win->memDC);
   HBITMAP bm = CreateCompatibleBitmap(tdc,width,height);
   HGDIOBJ OldObject;

   char *data = New(&gcheap,sizeof(BITMAPINFOHEADER) +
                    sizeof(RGBQUAD)*MAX_GREYS);
   BITMAPINFOHEADER *BitmapHeader = (BITMAPINFOHEADER *) data;
   RGBQUAD *ColorTable = (RGBQUAD *) (data + sizeof(BITMAPINFOHEADER));
   BITMAPINFO *Info = (BITMAPINFO *) data;

   int i,j;

   /* if the length of the scan line is not a */
   /* multiple of four, the bitmap must be reshaped. */
   /* SetDIBits() expects scan lines to start on word boundaries. */

   int ScanLineLen = 4*(1+(width-1)/4);
   unsigned char *reshaped = NULL;

   BitmapHeader->biSize = sizeof(BITMAPINFOHEADER);
   BitmapHeader->biWidth = width;
   BitmapHeader->biHeight = -height;
   BitmapHeader->biPlanes = 1;
   BitmapHeader->biBitCount = 8;
   BitmapHeader->biCompression = 0;
   BitmapHeader->biSizeImage = 0;
   BitmapHeader->biXPelsPerMeter = 0;
   BitmapHeader->biYPelsPerMeter = 0;
   BitmapHeader->biClrUsed = MAX_GREYS;
   BitmapHeader->biClrImportant = MAX_GREYS;
   for (i=0;i<MAX_GREYS;i++) {
      ColorTable[i].rgbRed =
         ColorTable[i].rgbBlue =
         ColorTable[i].rgbGreen = greys[i];
      ColorTable[i].rgbReserved = 0;
   }

   if (ScanLineLen != width) {
      reshaped = (unsigned char *) New(&gcheap,height*ScanLineLen);
      for (i=0;i<height;i++) {
         for (j=0;j<width;j++) {
            reshaped[i*ScanLineLen+j] = p[i*width+j];
         }
      }
      SetDIBits(win->memDC,bm,0,height,reshaped,Info,DIB_RGB_COLORS);
      Dispose(&gcheap,reshaped);
   }
   else {
      SetDIBits(win->memDC,bm,0,height,p,Info,DIB_RGB_COLORS);
   }

   OldObject = SelectObject(dc,bm);
   BitBlt(win->memDC,x,y,width,height,dc,0,0,SRCCOPY);
   if (win->writingToMeta) { /* bitmap source location differs */
      BitBlt(tdc,x,y,width,height,dc,x,y,SRCCOPY);
   }
   else {
      BitBlt(tdc,x,y,width,height,dc,0,0,SRCCOPY);
   }

   DeleteDC(dc);
   DeleteObject(bm);
   ReleaseDC(win->theWindow,tdc);
   Dispose(&gcheap,data);
#endif
#ifdef XGRAFIX
   static XImage *xi = NULL;
   static unsigned char *mem = NULL;
   unsigned char *pix;
   int i, j;


   if (mem != p){
     if (xi != NULL)
       XDestroyImage(xi);
     xi = XGetImage(win->theDisp,win->theWindow,x,y,width,height,AllPlanes,XYPixmap);
     pix = mem = p;
     for (j = 0; j < height; j++)
       for (i = 0; i < width; i++)
	 XPutPixel(xi, i, j, greys[(int) (*pix++)]);
   }
   XPutImage(win->theDisp, win->theWindow, win->theGC, xi, 0, 0, x, y, width, height);
   XPutImage(win->theDisp, win->thePixmap, win->theGC, xi, 0, 0, x, y, width, height);


#endif
}

/* EXPORT->HFlush flush any pending draw operations */
void HFlush()
{
#ifdef XGRAFIX
  if(!noGraph)
    XFlush(globDisp);
#endif
}

/* EXPORT-> HSpoolGraf: start saving an image of window in fname */
/* must be balanced by a call to HEndSpoolGraf() */
void HSpoolGraf(HWin win, char *fname)
{
#ifdef WINGRAFIX
   int wmm,hmm; /* width and height in millimeters */
   int wpx,hpx; /* width and height in pixels */
   char *description = "Created by HGraf";
   RECT r;
   HDC dc = GetDC(win->theWindow);
   int er;

   wmm = GetDeviceCaps(dc, HORZSIZE);
   hmm = GetDeviceCaps(dc, VERTSIZE);
   wpx = GetDeviceCaps(dc, HORZRES);
   hpx = GetDeviceCaps(dc, VERTRES);

   r.left = (win->clientRect.left * wmm * 100)/wpx;
   r.top = (win->clientRect.top * hmm * 100)/hpx;
   r.right = (win->clientRect.right * wmm * 100)/wpx;
   r.bottom = (win->clientRect.bottom * hmm * 100)/hpx;

   win->DCSaved = win->memDC;
   win->memDC = CreateEnhMetaFile(dc,fname,&r,description);
   er = GetLastError();
   ReleaseDC(win->theWindow,dc);
   win->writingToMeta = TRUE;
#endif
#ifdef XGRAFIX

#endif
}

/* EXPORT-> HEndSpoolGraf: close file opened in HSpoolGraf() */
/* It is recommended to redraw the window after this call.   */
void HEndSpoolGraf(HWin win)
{
#ifdef WINGRAFIX
   win->writingToMeta = FALSE;
   CloseEnhMetaFile(win->memDC);
   win->memDC = win->DCSaved;
#endif
#ifdef XGRAFIX

#endif
}


/* EXPORT->HDumpGraf: dump a BMP image of window into fname */
void HDumpGraf(HWin win, char *fname)
{
#ifdef WINGRAFIX
   BITMAPFILEHEADER FileHeader;
   BITMAPINFOHEADER BitmapHeader;
   BITMAPINFO *Info;
   int ColorTableSize;
   int ImageSize;
   FILE *fp;
   char *img;
   HDC dc = GetDC(win->theWindow);
   HBITMAP temp = CreateCompatibleBitmap(win->memDC,1,1);

   SelectObject(win->memDC,temp);

   /* retrieve information about the bitmap */
   BitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
   BitmapHeader.biBitCount = 0;
   GetDIBits(win->memDC,win->theBitMap,0,0,NULL,(LPBITMAPINFO)&BitmapHeader,BI_RGB);

   switch (BitmapHeader.biCompression) {
   case BI_RGB:
      if (BitmapHeader.biBitCount > 8) {
         ColorTableSize = 0;
      }
      else {
         ColorTableSize = BitmapHeader.biClrUsed*sizeof(RGBQUAD);
      }
      break;
   case BI_RLE8:
   case BI_RLE4:
      ColorTableSize = BitmapHeader.biClrUsed*sizeof(RGBQUAD);
      break;
   case BI_BITFIELDS:
      ColorTableSize = 3*sizeof(DWORD);
   }

   Info = (BITMAPINFO *) New(&gcheap,sizeof(BITMAPINFOHEADER) + ColorTableSize);
   memcpy(Info,&BitmapHeader,sizeof(BITMAPINFOHEADER));

   ImageSize = BitmapHeader.biSizeImage;
   img = New(&gcheap,ImageSize);

   GetDIBits(win->memDC,win->theBitMap,0,win->clientRect.bottom,img,Info,BI_RGB);

   FileHeader.bfType = 0x4d42;  /* 'BM' */
   FileHeader.bfSize = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER) +
      ImageSize + ColorTableSize;
   FileHeader.bfReserved1 = FileHeader.bfReserved2 = 0;
   FileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + ColorTableSize;

   fp = fopen(fname,"wb");
   fwrite(&FileHeader,1,sizeof(BITMAPFILEHEADER),fp);
   fwrite(Info,1,sizeof(BITMAPINFOHEADER) + ColorTableSize,fp);
   fwrite(img,1,ImageSize,fp);
   fclose(fp);

   SelectObject(win->memDC,win->theBitMap);
   DeleteObject(temp);
   Dispose(&gcheap,Info);
   Dispose(&gcheap,img);
#endif
#ifdef XGRAFIX

#endif
}

#define BTN_WAIT    200        /* 200 milliseconds minimum button press */
#define BTN_LINE_WIDTH 1       /* the line width for button drawing */

/* EXPORT->CreateHButton: create a button object with the specified parameters */
HButton *CreateHButton(HWin win, HButton *btnlst, ButtonId btnid, int x, int y, int w,
                       int h, char *str, HColour fg, HColour bg, int fontSize,
                       void (*action)(void))
{
   HButton *btn, *btnptr;

   btn = New(&(win->btnHeap), sizeof(HButton));
   /* initialise the fields of the structure */
   btn->id = btnid;  btn->win = win; btn->fontSize = fontSize;
   btn->x = x;   btn->y = y;   btn->w = w;   btn->h = h;
   btn->str = str;
   btn->fg = fg;   btn->bg = bg;
   btn->lit = FALSE; btn->active = TRUE; btn->toggle = FALSE;
   btn->next = NULL;
   /* append it to the end of the list if the list already exists */
   if (btnlst!=NULL){
      btnptr = btnlst;
      while (btnptr->next != NULL) btnptr = btnptr->next;
      btnptr->next = btn;
   }
   btn->action = action;
   /* return ptr to the newly created button */
   return btn;
}

/* EXPORT->RedrawHButton: readraw a single button object */
void RedrawHButton(HButton *btn)
{
   int pad = 2;
   int x, y, w, h, r, s, pos;
   HPoint poly[9], shad[4];
   char sbuf[256], nullchar = '\0';

   x = btn->x;   y=btn->y;   w=btn->w;   h=btn->h;   r=3; s=1;
   /* set up the polygon */
   poly[0].x = x;         poly[0].y = y+r;
   poly[1].x = x;         poly[1].y = y+h-r;
   poly[2].x = x+r;       poly[2].y = y+h;
   poly[3].x = x+w-r;     poly[3].y = y+h;
   poly[4].x = x+w;       poly[4].y = y+h-r;
   poly[5].x = x+w;       poly[5].y = y+r;
   poly[6].x = x+w-r;     poly[6].y = y;
   poly[7].x = x+r;       poly[7].y = y;
   poly[8].x = x;         poly[8].y = y+r;
   /* set up the extra lines for the shadow */
   shad[0].x = x+r+s;     shad[0].y = y+h+s;
   shad[1].x = x+w-r+s;   shad[1].y = y+h+s;
   shad[2].x = x+w+s;     shad[2].y = y+h-r+s;
   shad[3].x = x+w+s;     shad[3].y = y+r+s;
   if (btn->lit)
      HSetColour(btn->win, btn->fg);
   else
      HSetColour(btn->win, btn->bg);
   HFillPolygon(btn->win, poly, 9);
   HSetColour(btn->win, btn->fg);
   HDrawLines(btn->win, poly, 9);
   HDrawLines(btn->win, shad, 4);
   if (btn->active)
      if (btn->lit)
         HSetColour(btn->win, btn->bg);
      else
         HSetColour(btn->win, btn->fg);
   else
      HSetGrey(btn->win, 30);
   HSetFontSize(btn->win,btn->fontSize);
   strcpy(sbuf, btn->str);
   pos = strlen(sbuf);
   while(HTextWidth(btn->win, sbuf) > (w - 2*pad))
      sbuf[--pos]=nullchar;
   HPrintf(btn->win, CentreX(btn->win,x+w/2, sbuf), CentreY(btn->win,y+h/2, sbuf), "%s", sbuf);
}

/* EXPORT->RedrawHButtonList: redraw the whole list of buttons */
void RedrawHButtonList(HButton *btnlst)
{
   HButton *btnptr;

   for (btnptr=btnlst; btnptr!=NULL; btnptr=btnptr->next){
      HSetLineWidth(btnptr->win, BTN_LINE_WIDTH);
      RedrawHButton(btnptr);
   }
}

/* EXPORT->FindButton: find button given name */
HButton *FindButton(HButton *btnlst, ButtonId key)
{
   HButton *btnptr;

   for (btnptr=btnlst; btnptr!=NULL; btnptr=btnptr->next)
      if (btnptr->id==key)
         return btnptr;
   return NULL;
}

/* EXPORT->SetActive: set active field in button list */
void SetActive(HButton *btnlst, Boolean active)
{
   HButton *btnptr;

   for (btnptr=btnlst; btnptr!=NULL; btnptr=btnptr->next)
      btnptr->active = active;
}

/* EXPORT->CheckButtonList: find within which button the point(x,y) is */
HButton *CheckButtonList(HButton *btnlst, int x, int y)
{
   HButton *btn;

   for (btn=btnlst; btn!=NULL; btn=btn->next)
      if (IsInRect(x, y, btn->x, btn->y, btn->x + btn->w, btn->y + btn->h) && btn->active)
         return btn;
   return NULL;

}

/* EXPORT->SetButtonLit: show button press */
void SetButtonLit(HButton *btn, Boolean lit)
{
   if (btn->lit != lit){
      btn->lit = lit;
      RedrawHButton(btn);
   }
}

/* EXPORT->TrackButtons: tracks the buttons until the mouse button is released */
ButtonId TrackButtons(HButton *btnlist, HEventRec hev)
{
   HButton *pressed, *released;
   Boolean done;

   pressed = CheckButtonList(btnlist, hev.x, hev.y);
   if (pressed != NULL){
      SetButtonLit(pressed, TRUE);
      done = FALSE;
#ifdef USE_TIMER
      HFlush(); usleep(BTN_WAIT*1000);
#endif
      do {
         hev = HGetEvent(NULL, pressed->action);
         done = (hev.event==HMOUSEUP);
      } while (!done);
      released = CheckButtonList(btnlist, hev.x, hev.y);
      SetButtonLit(pressed, FALSE);
      if ( pressed == released)
         return pressed->id;
   }
   return 0;
}

/* -------------------- Window Creation/Destruction --------------------- */

#ifdef WINGRAFIX
void InitialiseDC(HDC dc)
{
   SetArcDirection(dc,AD_COUNTERCLOCKWISE);
   SetPolyFillMode(dc,WINDING);
   SetTextAlign(dc,TA_BASELINE | TA_LEFT);
   SetBkMode(dc,TRANSPARENT);
}
#endif
#ifdef XGRAFIX

void InitGCs(HWin win)
{
   XferMode   xf;
   XGCValues  values;
   unsigned   long mask;

   mask = GCLineWidth | GCFunction | GCForeground;
   for (xf = GCOPY; xf < GINVERT; xf=(XferMode) (xf+1)){
      values.line_width = 0;
      values.function = TFuncX11[(int) xf];
      values.foreground = black;
      win->gcs[(int) xf] = XCreateGC(win->theDisp, win->theWindow, mask, &values);
      XSetGraphicsExposures(win->theDisp,win->gcs[(int) xf], False);
   }
   mask = GCLineWidth | GCFunction | GCPlaneMask | GCForeground;
   values.line_width = 0;
   values.function = GXxor;
   values.foreground = ~0;
   values.plane_mask = black ^ white;
   win->gcs[(int) GINVERT] = XCreateGC(win->theDisp, win->theWindow, mask, &values);
}

#endif

/* EXPORT-> MakeHWin: Create and open window, initialization */
HWin MakeHWin(char *wname, int x, int y, int w, int h, int bw)
     /* in WINGRAFIX bw is ignored. */
{
   HWin win = NULL;
#ifdef WINGRAFIX
   char sbuf[256];
   HDC dc;

   if(noGraph)
     HError(9999, "Trying to create window when NOGRAPHICS specified in config file\n");

   win = (HWin)malloc(sizeof(HWindowRec));
   win->next = wroot; wroot = win;
   strcpy(win->winName, wname);
   win->theWindow =
     CreateWindow(winClassName, win->winName, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                   x,y, w,h, HWND_DESKTOP, NULL,
                   windowClass.hInstance,NULL);
   /* adjust window size so that the client rectangle is the size requested */
   /* Win32 interprets w and h as the dimensions of the overall window. */
   GetClientRect(win->theWindow,&(win->clientRect));
   MoveWindow(win->theWindow,x,y,w+w-win->clientRect.right,h+h-win->clientRect.bottom,TRUE);
   GetClientRect(win->theWindow,&(win->clientRect));

   /* Obtain and initialize device contexts */
   dc = GetDC(win->theWindow);
   win->memDC = CreateCompatibleDC(dc);
   InitialiseDC(dc); InitialiseDC(win->memDC);

   /* create a bitmap and select it into win->memDC */
   win->theBitMap = CreateCompatibleBitmap(dc,w,h);
   SelectObject(win->memDC,win->theBitMap);
   ReleaseDC(win->theWindow,dc);
   /* create button storage */
   strcpy(sbuf, wname);  strcat(sbuf, ":");  strcat(sbuf, "buttons");
   CreateHeap(&(win->btnHeap), sbuf, MHEAP, sizeof(HButton), 1.0, 100, 100);
   /* fix colours and paint the window white */
   if (dispDEEP==0) dispDEEP = GetDeviceCaps(win->memDC,BITSPIXEL);
   if (dispDEEP<=8) CompressColours(win);
   win->lineWidth = 1; win->curColour = RGB(0,0,0);
   win->theBrush = NULL; win->thePen = NULL; win->thinPen = NULL;
   HSetColour(win,WHITE); HSetFontSize(win,0);
   HFillRectangle(win,0,0,win->clientRect.right,win->clientRect.bottom);
   HSetColour(win,BLACK);
   /* finally, set initial window status */
   win->writingToMeta = FALSE;
   win->isActive = TRUE;
#endif
#ifdef XGRAFIX
   char sbuf[256], *hgraf = "HGraf";
   Display *display;
   XSetWindowAttributes setwinattr;
   unsigned long vmask;
   HEventRec report;
   Atom del_atom;
   display=globDisp;

   win = (HWin)malloc(sizeof(HWindowRec));

   win->theDisp=display;
   win->theThread=HThreadSelf();
   win->next = wroot; wroot = win; /* push into record list */
   strcpy(win->winName, wname);

   /* make a window + enter OS specific data */
   win->theScreen=DefaultScreen(win->theDisp);

   win->rootW=RootWindow(win->theDisp, win->theScreen);
   win->theWindow = XCreateSimpleWindow(win->theDisp, win->rootW, x, y, w, h, bw, black, white );
   /* allow for backing up the contents of the window */
   vmask = CWBackingStore;  setwinattr.backing_store = WhenMapped;
   XChangeWindowAttributes(win->theDisp, win->theWindow, vmask, &setwinattr);

   /* create a pixmap for backing store */
   win->thePixmap=XCreatePixmap(win->theDisp, win->theWindow, w, h, dispDEEP);

   /* set the size hints for the window manager */
   win->hints.flags = PPosition | PSize | PMaxSize | PMinSize;
   win->hints.y = y;              win->hints.x = x;
   win->hints.width  = w;         win->hints.height = h;
   win->hints.min_width  = w;     win->hints.min_height = h;
   win->hints.max_width  = w;     win->hints.max_height = h;

   /* compose the name of the window */

   XSetStandardProperties(win->theDisp, win->theWindow, wname, hgraf, None, NULL, 0, &(win->hints));

   /* select events to receive */
   XSelectInput(win->theDisp, win->theWindow, eMask);

   /* explicitly request to get toplevel window kill events */
   if ((del_atom = XInternAtom(win->theDisp, "WM_DELETE_WINDOW",True)) != None)
   XSetWMProtocols(win->theDisp, win->theWindow, &del_atom, 1);

   XMapWindow(win->theDisp, win->theWindow);
   InitGCs(win);
   /* create buttons storage */
   strcpy(sbuf, wname);  strcat(sbuf, ":");  strcat(sbuf, "buttons");
   CreateHeap(&(win->btnHeap), sbuf, MHEAP, sizeof(HButton), 1.0, 100, 100);
   HSetXMode(win, GCOPY);


   do {
     report=HGetEvent(NULL, NULL);
     /*     report=getEventFromQueue();*/
     /*  postEventToQueue(win->theThread, report);*/
   }
     while
     (report.event!=HREDRAW);

   win->redraw=TRUE;
   HSetColour(win,WHITE);

   XFillRectangle(win->theDisp, win->thePixmap,win->theGC, 0,0, w,h);


   HSetLineWidth(win,0);
   HSetColour(win,WHITE); HSetFontSize(win,0);
   HSetColour(win,BLACK);

#endif
   return win;
}

/* EXPORT->CloseHWin: close the given window */
void CloseHWin(HWin win){
#ifdef WINGRAFIX
  win->isActive = FALSE;
  if (win->theBrush) DeleteObject(win->theBrush);
  if (win->thePen)  DeleteObject(win->thePen);
  if (win->thinPen) DeleteObject(win->thinPen);
  if (win->theFont) DeleteObject(win->theFont);
  if (win->theBitMap) DeleteObject(win->theBitMap);
  DestroyWindow(win->theWindow);
#endif
#ifdef XGRAFIX
  XferMode   xf;
  for (xf = GCOPY; xf < GINVERT; xf=(XferMode) (xf+1)){
    XFreeGC(win->theDisp, win->gcs[(int) xf]);
  }
  XFreePixmap(win->theDisp, win->thePixmap);
  XDestroyWindow(win->theDisp, win->theWindow);

#endif
#ifndef NOGRAFIX
  DeleteHeap(&(win->btnHeap));
#endif

}

/* EXPORT->TermHGraf: Terminate Graphics (also called via at_exit) */
void TermHGraf()
{
  HWin win;

  for (win=wroot; win!=NULL; win=win->next)
     CloseHWin(win);
}

#ifdef WINGRAFIX

/* ----------------------------- Win32 Event Handling ---------------------------- */

/* FindWindowRec: find window rec given a window handle */
HWin FindWindowRec(HWND w)
{
   HWin p;

   for (p=wroot; p!=NULL; p=p->next)
      if (p->theWindow == w) return p;
   return NULL;
}

/* Called by the window manager */
LRESULT CALLBACK HGWinFunc(HWND theWindow, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
   HWin win;
   HDC dc;
   PAINTSTRUCT ps;

   win = FindWindowRec(theWindow);
   if (win==NULL)
      return DefWindowProc(theWindow, msg, wParam, lParam);
   switch (msg) {
   case WM_SIZING: /* for some reason we have to repaint when the window moves */
   case WM_MOVING:
      InvalidateRect(theWindow,&(win->clientRect),FALSE);
      return TRUE;
   case WM_EXITSIZEMOVE:
      InvalidateRect(theWindow,&(win->clientRect),FALSE);
      return 0;
   case WM_PAINT:
      dc = BeginPaint(theWindow,&ps);
      BitBlt(dc,0,0,win->clientRect.right,win->clientRect.bottom,win->memDC,0,0,SRCCOPY);
      EndPaint(theWindow,&ps);
      return 0;
   default:
      return DefWindowProc(theWindow, msg, wParam, lParam);
   }
}
#endif

#ifdef XGRAFIX

/* X11 damage control - from DrawImage */
void HandleExpose(XExposeEvent *xexpose)
{
  HWin win;

  win=FindWindowRec(xexpose->window);
  if(win==NULL)
    HError(9999,"Can't find window for expose event!");
  /* is this last Expose event? */
  if(xexpose->count==0) {

    XCopyArea(win->theDisp, win->thePixmap, win->theWindow, win->theGC, 0, 0, win->hints.width, win->hints.height, 0,0);

  }
}
#endif


/* HGetKeyType: map the given character code to a keytype */
KeyType HGetKeyType(char c)
{
#ifdef WINGRAFIX
   switch ((int) c) {
   case VK_ESCAPE:
      return ESCKEY;
   case VK_DELETE:
   case VK_BACK:
      return DELKEY;
   case VK_RETURN:
      return ENTERKEY;
   case VK_CONTROL:
      return CONTROLKEY;
   case VK_MENU:
      return COMMANDKEY;
   case VK_SHIFT:
      return SHIFTKEY;
   case VK_LEFT:
     return LEFTARROWKEY;
   case VK_RIGHT:
     return RIGHTARROWKEY;
   default:
      return NORMALKEY;
   }
#endif
}


/* ------------------------------ End of HGraf.c ------------------------- */
