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
 *   SetISTR(st, fmt, args) -  sprintf's into ISTR #st.  Redraws it in window
 *   char *GetISTR(st)      -  returns pointer to ISTR #st, or NULL if st bogus
 *   Pixmap ScalePixmap(src_pixmap, src_width, src_height) -  return a pixmap scaled to dpiMult
 */

#include "copyright.h"

#define  NEEDSARGS

#include "xv.h"

#include "bits/penn"
#include "bits/pennnet"


#define INFOWIDE (500 * dpiMult) /* (fixed) size of info window */
#define INFOHIGH (270 * dpiMult)

/* max length of an Info String */
#define ISTRLEN 256

/* baseline of top line of text */
#define TOPBASE (36*dpiMult + (penn_height*dpiMult)/2 + 4*dpiMult + 8*dpiMult + ASCENT)
#define STLEFT  (100*dpiMult)   /* left edge of strings */

static Pixmap pnetPix, pennPix;
static char istrs[NISTR][ISTRLEN];

static void drawStrings   PARM((void));
static void drawFieldName PARM((int));
static void redrawString  PARM((int));



/***************************************************/
void CreateInfo(geom)
     const char *geom;
{
  infoW = CreateWindow("xv info", "XVinfo", geom, INFOWIDE, INFOHIGH,
		       infofg, infobg, False);
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
Pixmap ScalePixmap(src_pixmap, src_width, src_height)
Pixmap src_pixmap;
int src_width, src_height;
{
  int dest_width = src_width * dpiMult;
  int dest_height = src_height * dpiMult;
  XImage *src_image, *dest_image;
  Pixmap dest_pixmap;

  dest_pixmap = XCreatePixmap(theDisp, rootW, dest_width, dest_height, dispDEEP);

  if (!dest_pixmap) {
    fprintf(stderr, "ScalePixmap, XCreatePixmap failed\n");
    return 0;
  }

  src_image = XGetImage(theDisp, src_pixmap, 0, 0, src_width, src_height, AllPlanes, ZPixmap);

  if (!src_image) {
    fprintf(stderr, "ScalePixmap, XGetImage failed\n");
    return 0;
  }

  dest_image = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, malloc(dest_width * dest_height * 4), dest_width, dest_height, 32, 0);

  if (!dest_image) {
    fprintf(stderr, "ScalePixmap, XCreateImage failed\n");
    return 0;
  }

  for (int y = 0; y < dest_height; y++) {
    for (int x = 0; x < dest_width; x++) {
      long pixel = XGetPixel(src_image, x / dpiMult, y / dpiMult);
      XPutPixel(dest_image, x, y, pixel);
    }
  }

  XPutImage(theDisp, dest_pixmap, theGC, dest_image, 0, 0, 0, 0, dest_width, dest_height);

  XDestroyImage(src_image);
  XDestroyImage(dest_image);

  return dest_pixmap;
}

/***************************************************/
void RedrawInfo(x,y,w,h)
     int x,y,w,h;
{
  int  i;
  Pixmap bigPix;

  XV_UNUSED(x); XV_UNUSED(y); XV_UNUSED(w); XV_UNUSED(h);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  /* draw the two icons */
  if (dpiMult > 1 && (bigPix = ScalePixmap(pennPix, penn_width, penn_height)) != 0) {
      XCopyArea(theDisp, bigPix, infoW, theGC, 0, 0, penn_width*dpiMult, penn_height*dpiMult,
	    36*dpiMult - (penn_width*dpiMult)/2, 36*dpiMult - (penn_height*dpiMult)/2);
      XFreePixmap(theDisp, bigPix);
  } else {
    XCopyArea(theDisp, pennPix, infoW, theGC, 0, 0, penn_width, penn_height,
	    36*dpiMult - penn_width/2, 36*dpiMult - penn_height/2);
  }
  if (dpiMult > 1 && (bigPix = ScalePixmap(pnetPix, pennnet_width, pennnet_height)) != 0) {
    XCopyArea(theDisp, bigPix, infoW, theGC, 0, 0, pennnet_width*dpiMult,
	    pennnet_height*dpiMult, INFOWIDE - 36*dpiMult - (pennnet_width*dpiMult)/2,
	    36*dpiMult - (pennnet_height*dpiMult)/2);
  } else {
    XCopyArea(theDisp, pnetPix, infoW, theGC, 0, 0, pennnet_width,
	    pennnet_height, INFOWIDE - 36*dpiMult - pennnet_width/2,
	    36*dpiMult - pennnet_height/2);
  }

  /* draw the credits */
  snprintf(dummystr, sizeof(dummystr), "XV   -   %s", REVDATE);
  CenterString(infoW, INFOWIDE/2, 36*dpiMult - LINEHIGH, dummystr);
  CenterString(infoW, INFOWIDE/2, 36*dpiMult,
	       "by John Bradley  (bradley@dccs.upenn.edu)");
  CenterString(infoW, INFOWIDE/2, 36*dpiMult + LINEHIGH,
	       "Copyright 1994, John Bradley  -  All Rights Reserved");


  /* draw the dividing lines */
  i = 36*dpiMult + (penn_height*dpiMult)/2 + 4*dpiMult + (LINEHIGH/3)*(dpiMult-1);

  XDrawLine(theDisp, infoW, theGC, 0, i, INFOWIDE, i);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH - 22*dpiMult, INFOWIDE, INFOHIGH - 22*dpiMult);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH - 42*dpiMult, INFOWIDE, INFOHIGH - 42*dpiMult);

  if (ctrlColor) {
    XSetForeground(theDisp, theGC, locol);
    XDrawLine(theDisp, infoW, theGC, 0, i + 1*dpiMult, INFOWIDE, i + 1*dpiMult);
    XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH - 21*dpiMult, INFOWIDE, INFOHIGH - 21*dpiMult);
    XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH - 41*dpiMult, INFOWIDE, INFOHIGH - 41*dpiMult);
  }

  if (ctrlColor) XSetForeground(theDisp, theGC, hicol);
  XDrawLine(theDisp, infoW, theGC, 0, i + 2*dpiMult, INFOWIDE, i + 2*dpiMult);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH - 20*dpiMult, INFOWIDE, INFOHIGH - 20*dpiMult);
  XDrawLine(theDisp, infoW, theGC, 0, INFOHIGH - 40*dpiMult, INFOWIDE, INFOHIGH - 40*dpiMult);

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
  static const char *fname[7] = { "Filename:", "Format:", "Resolution:",
	"Cropping:", "Expansion:", "Selection:", "Colors:" };

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  if (infoMode == INF_NONE || infoMode == INF_STR) return;
  if (infoMode == INF_PART && fnum>=3) return;

  XDrawString(theDisp, infoW, theGC, 10*dpiMult, TOPBASE + fnum*LINEHIGH,
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
    XFillRectangle(theDisp, infoW, theGC, 0, INFOHIGH - 39*dpiMult, INFOWIDE, 17*dpiMult);
    XSetForeground(theDisp, theGC, infofg);
    CenterString(infoW, INFOWIDE/2, INFOHIGH - 31*dpiMult, istrs[st]);
  }
  else if (st == ISTR_WARNING) {
    XSetForeground(theDisp, theGC, infobg);
    XFillRectangle(theDisp, infoW, theGC, 0, INFOHIGH - 19*dpiMult, INFOWIDE, 17*dpiMult);
    XSetForeground(theDisp, theGC, infofg);
    CenterString(infoW, INFOWIDE/2, INFOHIGH - 10*dpiMult, istrs[st]);
  }
  else {
    XSetForeground(theDisp, theGC, infobg);
    XFillRectangle(theDisp, infoW, theGC,
		   STLEFT, TOPBASE - ASCENT + (st-ISTR_FILENAME)*LINEHIGH,
		   (u_int) INFOWIDE-STLEFT, (u_int) LINEHIGH);
    XSetForeground(theDisp, theGC, infofg);
    XDrawString(theDisp, infoW, theGC, STLEFT,
		TOPBASE + (st-ISTR_FILENAME)*LINEHIGH, istrs[st],
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
    y2 = INFOHIGH - 43*dpiMult;

    XSetForeground(theDisp, theGC, infobg);

    XFillRectangle(theDisp,infoW,theGC,0,y1,
		   (u_int) INFOWIDE, (u_int) y2-y1);
    XFillRectangle(theDisp,infoW,theGC,0,INFOHIGH - 39*dpiMult,
		   (u_int) INFOWIDE, (u_int) 17*dpiMult);
    XFillRectangle(theDisp,infoW,theGC,0,INFOHIGH - 19*dpiMult,
		   (u_int) INFOWIDE, (u_int) 17*dpiMult);

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
    if (fmt) vsnprintf(istrs[stnum], sizeof(istrs[stnum]), fmt, args);
    else istrs[stnum][0] = '\0';
  }
  va_end(args);

  if (stnum == ISTR_COLOR) {
    snprintf(istrs[ISTR_INFO], sizeof(istrs[ISTR_INFO]), "%s  %s  %s",
	    formatStr, (picType==PIC8) ? "8-bit mode." : "24-bit mode.",
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
    sleep(1);  /* was 3, but _really_ slow for TIFFs with unknown tags... */
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


