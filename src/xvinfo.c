/* 
 * xvinfo.c - 'Info' box handling functions
 *
 * callable functions:
 *
 *   CreateInfo(geom)       -  creates the infoW window.  Doesn't map it.
 *   InfoBox(vis)           -  random processing based on value of 'vis'
 *                             maps/unmaps window, etc.
 *   RedrawInfo(x,y,w,h)    -  called by 'expose' events
 *   SetInfoMode(mode)      -  changes amount of info Info window shows
 *   SetISTR(st, fmt, args) - sprintf's into ISTR #st.  Redraws it in window 
 *   char *GetISTR(st)      - returns pointer to ISTR #st, or NULL if st bogus
 */

#include "copyright.h"

#define  NEEDSARGS

#include "xv.h"

#include "bits/penn"
#include "bits/pennnet"


#define INFOWIDE 500               /* (fixed) size of info window */
#define INFOHIGH 270

/* max length of an Info String */
#define ISTRLEN 80

/* baseline of top line of text */
#define TOPBASE (36 + penn_height/2 + 4 + 8 + ASCENT)
#define STLEFT  100   /* left edge of strings */

static Pixmap pnetPix, pennPix;
static char istrs[NISTR][ISTRLEN];

static void drawStrings   PARM((void));
static void drawFieldName PARM((int));
static void redrawString  PARM((int));



/***************************************************/
void CreateInfo(geom)
char *geom;
{
  infoW = CreateWindow("xv info", "XVinfo", geom, INFOWIDE, INFOHIGH, 
		       infofg, infobg, 0);
  if (!infoW) FatalError("can't create info window!");
  
  pennPix = XCreatePixmapFromBitmapData(theDisp, infoW, 
	(char *) penn_bits, penn_width, penn_height, infofg, infobg, dispDEEP);

  pnetPix = XCreatePixmapFromBitmapData(theDisp,infoW,
	(char *) pennnet_bits, pennnet_width, pennnet_height, 
	infofg, infobg, dispDEEP);
}
  

/***************************************************/
void InfoBox(vis)
     int vis;
{
  if (vis) XMapRaised(theDisp, infoW);
  else     XUnmapWindow(theDisp, infoW);
  
  infoUp = vis;
}


/***************************************************/
void RedrawInfo(x,y,w,h)
     int x,y,w,h;
{
  int  i;
  
  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  /* draw the two icons */
  XCopyArea(theDisp, pennPix, infoW, theGC, 0, 0, penn_width, penn_height,
	    36 - penn_width/2, 36 - penn_height/2);
  XCopyArea(theDisp, pnetPix, infoW, theGC, 0, 0, pennnet_width, 
	    pennnet_height, INFOWIDE - 36 - pennnet_width/2, 
	    36 - pennnet_height/2);

  /* draw the credits */
  sprintf(str,"XV   -   %s",REVDATE);
  CenterString(infoW, INFOWIDE/2, 36-LINEHIGH, str);
  CenterString(infoW, INFOWIDE/2, 36,
	       "by John Bradley  (bradley@dccs.upenn.edu)");
  CenterString(infoW, INFOWIDE/2, 36+LINEHIGH, 
	       "Copyright 1994, John Bradley  -  All Rights Reserved");


  /* draw the dividing lines */
  i = 36 + penn_height/2 + 4;

  XDrawLine(theDisp, infoW, theGC, 0, i, INFOWIDE, i);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH-22, INFOWIDE, INFOHIGH-22);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH-42, INFOWIDE, INFOHIGH-42);

  if (ctrlColor) {
    XSetForeground(theDisp, theGC, locol);
    XDrawLine(theDisp, infoW, theGC, 0, i+1, INFOWIDE, i+1);
    XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH-21, INFOWIDE, INFOHIGH-21);
    XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH-41, INFOWIDE, INFOHIGH-41);
  }

  if (ctrlColor) XSetForeground(theDisp, theGC, hicol);
  XDrawLine(theDisp, infoW, theGC, 0, i+2, INFOWIDE, i+2);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH-20, INFOWIDE, INFOHIGH-20);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH-40, INFOWIDE, INFOHIGH-40);

  drawStrings();
}


/***************************************************/
static void drawStrings()
{
  int i;
  for (i=0; i<7; i++)     drawFieldName(i);  /* draw the field titles */
  for (i=0; i<NISTR; i++) redrawString(i);   /* draw the field values */
  XFlush(theDisp);
}


/***************************************************/
static void drawFieldName(fnum)
     int fnum;
{
  static char *fname[7] = { "Filename:", "Format:", "Resolution:", "Cropping:",
			    "Expansion:", "Selection:", "Colors:" };

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  if (infoMode == INF_NONE || infoMode == INF_STR) return;
  if (infoMode == INF_PART && fnum>=3) return;
  
  XDrawString(theDisp, infoW, theGC, 10, TOPBASE + fnum*LINEHIGH, 
	      fname[fnum], (int) strlen(fname[fnum]));
}


/***************************************************/
static void redrawString(st)
     int st;
{
  /* erase area of string, and draw it with new contents */
  
  if (infoMode == INF_NONE) return;
  if (infoMode == INF_STR && st > ISTR_WARNING) return;
  if (infoMode == INF_PART && st > ISTR_RES) return;


  if (st == ISTR_INFO) {
    XSetForeground(theDisp, theGC, infobg);
    XFillRectangle(theDisp, infoW, theGC, 0, INFOHIGH-39, INFOWIDE, 17);
    XSetForeground(theDisp, theGC, infofg);
    CenterString(infoW, INFOWIDE/2, INFOHIGH-31, istrs[st]);
  }
  else if (st == ISTR_WARNING) {
    XSetForeground(theDisp, theGC, infobg);
    XFillRectangle(theDisp, infoW, theGC, 0, INFOHIGH-19, INFOWIDE, 17);
    XSetForeground(theDisp, theGC, infofg);
    CenterString(infoW, INFOWIDE/2, INFOHIGH-10, istrs[st]);
  }
  else {
    XSetForeground(theDisp, theGC, infobg);
    XFillRectangle(theDisp, infoW, theGC, 
		   STLEFT, TOPBASE - ASCENT + (st-ISTR_FILENAME)*LINEHIGH, 
		   (u_int) INFOWIDE-STLEFT, (u_int) LINEHIGH);
    XSetForeground(theDisp, theGC, infofg);
    XDrawString(theDisp, infoW, theGC, STLEFT,
		TOPBASE	+ (st-ISTR_FILENAME)*LINEHIGH,	istrs[st], 
		(int) strlen(istrs[st]));
  }
}



/***************************************************/
void SetInfoMode(mode)
     int mode;
{
  int y1, y2;
  
  infoMode = mode;
  if (infoUp) {   /* only do this if window is mapped */
    y1 = TOPBASE - ASCENT;
    y2 = INFOHIGH-43;
    
    XSetForeground(theDisp, theGC, infobg);
    
    XFillRectangle(theDisp,infoW,theGC,0,y1, 
		   (u_int) INFOWIDE, (u_int) y2-y1);
    XFillRectangle(theDisp,infoW,theGC,0,INFOHIGH-39, 
		   (u_int) INFOWIDE, (u_int) 17);
    XFillRectangle(theDisp,infoW,theGC,0,INFOHIGH-19, 
		   (u_int) INFOWIDE, (u_int) 17);
    
    drawStrings();
  }
}


/***************************************************/
/* SetISTR( ISTR, format, arg1, arg2, ...)	   */

#if defined(__STDC__) && !defined(NOSTDHDRS)
void SetISTR(int stnum, ...)
{
  va_list args;
  char     *fmt;

  va_start(args, stnum);
#else
/*VARARGS0*/
void SetISTR(va_alist)
va_dcl
{
  va_list args;
  char    *fmt;
  int     stnum;

  va_start(args);

  stnum = va_arg(args, int);
#endif

  if (stnum>=0 && stnum < NISTR) {
    fmt = va_arg(args, char *);
    if (fmt) vsprintf(istrs[stnum], fmt, args);
    else istrs[stnum][0] = '\0';
  }
  va_end(args);
  
  if (stnum == ISTR_COLOR) {
    sprintf(istrs[ISTR_INFO], "%s  %s  %s", formatStr, 
	    (picType==PIC8) ? "8-bit mode." : "24-bit mode.",
	    istrs[ISTR_COLOR]);
  }

  if (infoUp) {
    redrawString(stnum);
    if (stnum == ISTR_COLOR) redrawString(ISTR_INFO);
    XFlush(theDisp);
  }

  if (ctrlUp && (stnum == ISTR_INFO || stnum == ISTR_WARNING || 
		 stnum == ISTR_COLOR)) {
    DrawCtrlStr();
    XFlush(theDisp);
  }

  if (anyBrowUp && (stnum == ISTR_WARNING || stnum == ISTR_INFO) 
      && strlen(istrs[stnum])) {
    SetBrowStr(istrs[stnum]);
    XFlush(theDisp);
  }

  if (stnum == ISTR_WARNING && !ctrlUp && !infoUp && !anyBrowUp && 
      strlen(istrs[stnum])) {
    OpenAlert(istrs[stnum]);
    sleep(3);
    CloseAlert();
  }
}


/***************************************************/
char *GetISTR(stnum)
int stnum;
{
  /* returns pointer to ISTR string */
  if (stnum < 0 || stnum>=NISTR) return(NULL);
  return (istrs[stnum]);
}


