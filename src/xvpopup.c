/*
 * xvpopup.c - pop up "Are you sure?  Yes/No/Maybe" sort of dialog box
 *
 * callable functions:
 *
 *   CenterMapWindow(win,x,y)  -  maps and centers a window around the mouse
 *   PopUp(str,...)        -  maps, sets up popW
 *   ErrPopUp(str,str)     -  maps, sets up popW
 *   GetStrPopUp(...)      -  opens a 1-line, editable text popup window
 *   GrabPopUp(*hide,*del) -  opens 'grab' popup dialog
 *   PadPopUp()            -  opens 'grab' popup dialog
 *   ClosePopUp()          -  closes pop-up or alert window, if open
 *   OpenAlert(str)        -  maps a button-less window
 *   CloseAlert()          -  closes a button-less window
 *   PUCheckEvent(event)   -  called by event handler
 */

#include "copyright.h"

#include "xv.h"

#define OMIT_ICON_BITS
#include "bits/icon"   /* icon_bits[] not used, but icon_width/height are */

#define PUWIDE 480
#define PUHIGH 170

#define PAD_PUWIDE 480
#define PAD_PUHIGH 215

#define BUTTH   24

static int  doPopUp       PARM((const char *, const char **, int, int, const char *));
static void attachPUD     PARM((void));
static void TextRect      PARM((Window, const char *, int, int, int, int, u_long));
static void createPUD     PARM((void));
static void drawPUD       PARM((int, int, int, int));
static void drawPadOMStr  PARM((void));
static void clickPUD      PARM((int, int));
static void doGetStrKey   PARM((int));
static int  doGSKey       PARM((int));
static void changedGSBuf  PARM((void));
static void drawGSBuf     PARM((void));
static void buildPadLists PARM((void));
static void build1PadList PARM((const char *, const char **, const char **, int *,
				const char **, const char **, int));


/* values 'popUp' can take */
#define ISPOPUP  1
#define ISALERT  2
#define ISGETSTR 3
#define ISGRAB   4
#define ISPAD    5

#define DELAYSTR "Delay:"
#define SECSTR   "seconds"
#define HIDESTR  "Hide XV windows"

/* local variables */
static Window      popW;
static int         nbts, selected, popUp=0, firsttime=1;
static int         puwide = PUWIDE;
static int         puhigh = PUHIGH;
static BUTT       *bts;
static const char *text;
static char        accel[8];

static char       *gsBuf;       /* stuff needed for GetStrPopUp() handling */
static const char *gsFilter;
static int         gsBufLen, gsAllow, gsCurPos, gsStPos, gsEnPos;
static int         gsx, gsy, gsw, gsh;

/* stuff for GrabPopUp */
static CBUTT ahideCB;


/*** stuff for PadPopUp ***/
static char   padSbuf[256], padBbuf[256], padLbuf[256], padBuf[256];
static char  *padInst, padSinst[200], padBinst[200], padLinst[200];
static MBUTT  padDfltMB, padMthdMB;
static BUTT   padDButt, padOMButt;
static int    padHaveDooDads = 0;
static int    padMode, padOMode;
static DIAL   padWDial, padHDial, padODial;

static int         padMthdLen=3;
static const char *padMthdNames[] = { "Solid Fill", "Run 'bggen'", "Load Image" };

static int         padColDefLen = 9;
static const char *padColDefNames[] = { "black", "red",  "yellow", "green",
					"cyan",  "blue", "magenta", "white",
					"50% gray" };

static const char *padColDefVals[]  = { "black", "red", "yellow", "green",
					"cyan",  "blue", "magenta", "white",
					"gray50" };

static int         padBgDefLen = 8;
static const char *padBgDefNames[] = { "Black->White",
				       "Blue Gradient",
				       "RGB Rainbow",
				       "Full Rainbow",
				       "Color Assortment",
				       "Green Tiles",
				       "Red Balls",
				       "Red+Yellow Diamonds" };

static const char *padBgDefVals[] = { "black white",
				      "100 100 255  50 50 150",
				      "red green blue",
				      "black red yellow green blue purple black",
				      "black white red black yellow white green black cyan white blue black magenta white red yellow green cyan blue magenta red",
				      "green black -r 30 -G 32x32",
				      "red black -r 45 -G 32x32",
				      "red yellow -r 45 -G 32x32" };


/* this should match with PAD_O* defs in xv.h */
static const char *padOMStr[] = { "RGB", "Int.", "Hue", "Sat." };

#define PAD_MAXDEFLEN 10
static int         padColLen = 0;
static const char *padColNames [PAD_MAXDEFLEN];
static const char *padColVals  [PAD_MAXDEFLEN];
static int         padBgLen = 0;
static const char *padBgNames  [PAD_MAXDEFLEN];
static const char *padBgVals   [PAD_MAXDEFLEN];
static int         padLoadLen = 0;
static const char *padLoadNames[PAD_MAXDEFLEN];
static const char *padLoadVals [PAD_MAXDEFLEN];


/***************************************************/
void CenterMapWindow(win, dx, dy, w, h)
     Window win;
     int    dx, dy, w, h;
{
  XSizeHints hints;
  Window       rW,cW;
  int          rx,ry,x,y,wx,wy;
  unsigned int mask;


  if (!XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
    /* couldn't query mouse.  just center on screen */
    wx = (dispWIDE-w)/2;   wy = (dispHIGH-h)/2;
  }
  else {
    wx = x - dx;
    wy = y - dy;
    if (wx<0) wx = 0;
    if (wy<0) wy = 0;
    if (wx + w > dispWIDE) wx = dispWIDE - w;
    if (wy + h > dispHIGH) wy = dispHIGH - h;
  }


  if (winCtrPosKludge) {
    wx -= (p_offx + ch_offx);
    wy -= (p_offy + ch_offy);
  }
  else {
    wx -= (ch_offx);
    wy -= (ch_offy);
  }

  if (!XGetNormalHints(theDisp, win, &hints)) hints.flags = 0;
  hints.width  = hints.min_width  = hints.max_width  = w;
  hints.height = hints.min_height = hints.max_height = h;
  hints.x = wx;  hints.y = wy;
  hints.flags  |= (USSize | PMinSize | PMaxSize | USPosition);
  XSetNormalHints(theDisp, win, &hints);

  XMoveWindow(theDisp, win, wx, wy);
  XMapRaised(theDisp, win);
}


/***************************************************/
int PopUp(txt, labels, n)
     const char *txt;
     const char *labels[];
     int         n;
{
  return doPopUp(txt, labels, n, ISPOPUP, "xv confirm");
}


/***************************************************/
static int doPopUp(txt, labels, n, poptyp, wname)
     const char *txt;
     const char *labels[];
     int         n, poptyp;
     const char *wname;
{
  int    i;
  XEvent event;

  if (firsttime) createPUD();

  if (poptyp != ISPAD) { puwide = PUWIDE;      puhigh = PUHIGH;     }
                  else { puwide = PAD_PUWIDE;  puhigh = PAD_PUHIGH; }


  /* attach controls to popW, now that it exists */
  if      (poptyp==ISGRAB) ahideCB.win = popW;
  else if (poptyp == ISPAD) {

    if (!padHaveDooDads) {
      DCreate(&padWDial, popW, 16,      puhigh-16-100-1,75,100,
	      1.0, 2048.0, (double)pWIDE, 1.0, 10.0,
	      infofg, infobg, hicol, locol, "Width", NULL);
      DCreate(&padHDial, popW, 16+1+75, puhigh-16-100-1,75,100,
	      1.0, 2048.0, (double)pHIGH, 1.0, 10.0,
	      infofg, infobg, hicol, locol, "Height", NULL);

      DCreate(&padODial, popW, 16+1+75+75+9, puhigh-16-100-1,75,100,
	      0.0, 100.0, 100.0, 1.0, 10.0,
	      infofg, infobg, hicol, locol, "Opaque", NULL);

      MBCreate(&padMthdMB, popW, 100-2+44, 10, 140, 19, NULL,
	       padMthdNames, padMthdLen, infofg, infobg, hicol, locol);
      padMthdMB.hascheck = 1;
      padMthdMB.flags[0] = 1;

      MBCreate(&padDfltMB, popW, 250-2+44, 10, 140, 19, "Defaults",
	       padColNames, padColLen, infofg, infobg, hicol, locol);

      BTCreate(&padDButt, popW, padHDial.x+padHDial.w-12, puhigh-140+6,
	       13,13, "", infofg, infobg, hicol, locol);

      BTCreate(&padOMButt, popW, padODial.x+padODial.w-12, puhigh-140+6,
	       13,13, "", infofg, infobg, hicol, locol);

      padHaveDooDads = 1;
    }

    XMapWindow(theDisp, padWDial.win);
    XMapWindow(theDisp, padHDial.win);
    XMapWindow(theDisp, padODial.win);
  }


  XResizeWindow(theDisp, popW, (u_int) puwide, (u_int) puhigh);
  XStoreName   (theDisp, popW, wname);
  XSetIconName (theDisp, popW, wname);
  attachPUD();

  bts = (BUTT *) malloc(n * sizeof(BUTT));
  if (!bts) FatalError("unable to malloc buttons in popup\n");
  nbts = n;
  selected = 0;
  text = txt;

  for (i=0; i<n; i++) {
    BTCreate(&bts[i], popW, puwide - (n-i) * (80 + 10), puhigh - 10 - BUTTH,
	     80, BUTTH, labels[i]+1, infofg, infobg, hicol, locol);
    accel[i] = labels[i][0];
  }


  if (poptyp == ISGRAB) {
    BTSetActive(&bts[0], (int) strlen(gsBuf));
    BTSetActive(&bts[1], (strlen(gsBuf)>(size_t)0 && atoi(gsBuf)>(size_t)0));
  }
  else if (poptyp == ISPAD) {
    BTSetActive(&bts[0], (int) strlen(gsBuf));
    i = pWIDE * 3;  RANGE(i,2048,9999);
    DSetRange(&padWDial, 1.0, (double)i, padWDial.val, 1.0, 10.0);
    i = pHIGH * 3;  RANGE(i,2048,9999);
    DSetRange(&padHDial, 1.0, (double)i, padHDial.val, 1.0, 10.0);

    DSetActive(&padWDial, (padMode!=PAD_LOAD));  /* DSetRange activates dial */
    DSetActive(&padHDial, (padMode!=PAD_LOAD));
    DSetActive(&padODial, 1);

    switch (padMode) {
    case PAD_SOLID:
      padDfltMB.list  = padColNames;
      padDfltMB.nlist = padColLen;
      break;
    case PAD_BGGEN:
      padDfltMB.list  = padBgNames;
      padDfltMB.nlist = padBgLen;
      break;
    case PAD_LOAD:
      padDfltMB.list  = padLoadNames;
      padDfltMB.nlist = padLoadLen;
      break;
    default: break;             /* shouldn't happen */
    }
  }

  /* center first button in window around mouse position, with constraint that
     window be fully on the screen */

  popUp = poptyp;
  if (startGrab == 2)
    startGrab = 4;
  else {
    CenterMapWindow(popW, 40 + bts[0].x, BUTTH/2 + bts[0].y, puwide, puhigh);

    /* MUST wait for VisibilityNotify event to come in, else we run the risk
       of UnMapping the window *before* the Map request completed.  This
       appears to be bad, (It leaves an empty window frame up.) though it
       generally only happens on slow servers.  Better safe than screwed... */

    XWindowEvent(theDisp, popW, VisibilityChangeMask, &event);
  }

  /* block until this window gets closed */
  while (popUp) {
    XNextEvent(theDisp, &event);
    HandleEvent(&event, &i);
  }

  /* free stuff */
  XUnmapWindow(theDisp, popW);
  free(bts);

  return(selected);
}


/***************************************************/
void ErrPopUp(txt, label)
     const char *txt;
     const char *label;
{
  /* simplified interface to PopUp.  Takes a string and the label for the
     (one) button */

  PopUp(txt, &label, 1);
}


/***************************************************/
int GetStrPopUp(txt, labels, n, buf, buflen, filstr, allow)
     const char *txt;
     const char *labels[];
     char *buf;
     const char *filstr;
     int   n, buflen, allow;
{
  /* pops up a window with a prompt string, a 1-line editable
     text thingy, and a row of buttons.  'txt' is the prompt
     string, 'labels' are the labels for the buttons, 'n' is the
     number of buttons, 'buf' is the buffer displayed and edited
     in the window, buflen is its length, filstr is a filter string, of
     characters to block from entry (or, if 'allow' is '1', a list
     of the *only* characters allowed for entry)

     It returns the index of the button clicked on.  Note that the
     button labels have 1-character accellerators at the front, same
     as in PopUp().  Note that it would be suboptimal to make any
     of the 1-character accellerators be the same character as one of
     the edit-text command keys

     Also note that the filter string should only contain normal printable
     characters (' ' through '\177'), as ctrl chars are pre-filtered
     (ie, interpreted as emacs-like commands) */

  gsBuf = buf;        gsBufLen = buflen;
  gsFilter = filstr;  gsAllow = allow;

  gsCurPos = strlen(gsBuf);
  gsStPos = gsEnPos = 0;

  gsh = LINEHIGH+5;
  gsx = 10 + icon_width + 20;
  gsy = 10+(PUHIGH-30-BUTTH-gsh)/2;

  if (strlen(txt) > (size_t) 60)
    gsy = PUHIGH - 10 - BUTTH - 10 - gsh - 20;

  gsw = PUWIDE - gsx - 10;

  changedGSBuf();      /* careful!  popW doesn't exist yet! */

  return doPopUp(txt, labels, n, ISGETSTR, "xv prompt");
}


/***************************************************/
int GrabPopUp(pHide, pDelay)
     int *pHide, *pDelay;
{
  /* pops up Grab options dialog box */

  int                rv;
  char               delaybuf[32], grabTxt[1024];
  static const char *grabLabels[] = { "\nGrab", "aAutoGrab", "\033Cancel" };

  sprintf(delaybuf,"%d", *pDelay);
  gsBuf = delaybuf;          gsBufLen = 3;
  gsFilter = "0123456789";   gsAllow = 1;

  gsCurPos = strlen(gsBuf);
  gsStPos = gsEnPos = 0;

  gsw = 32;
  gsh = LINEHIGH+5;
  gsx = 10 + StringWidth(DELAYSTR) + 5;
  gsy = (PUHIGH-BUTTH-10-5-gsh);

  changedGSBuf();      /* careful!  popW doesn't exist yet! */

  /* window value gets filled in in doPopUp() */
  CBCreate(&ahideCB, (Window) NULL,
	   PUWIDE-10-18-StringWidth(HIDESTR),
	   gsy+2, HIDESTR, infofg, infobg, hicol, locol);
  ahideCB.val = *pHide;

  sprintf(grabTxt, "Grab: after delay, Left button grabs a window, ");
  strcat (grabTxt, "Middle button ");
  strcat (grabTxt, "grabs a rectangular area, Right button cancels.\n\n");
  strcat (grabTxt, "AutoGrab: after delay, grabs ");
  strcat (grabTxt, "the window the cursor is positioned in.  ");
  strcat (grabTxt, "Delay must be non-zero.");

  rv = doPopUp(grabTxt, grabLabels, 3, ISGRAB, "xv grab");

  *pHide  = ahideCB.val;
  *pDelay = atoi(delaybuf);
  return rv;
}


/***************************************************/
int PadPopUp(pMode, pStr, pWide,pHigh, pOpaque, pOmode)
     int  *pMode, *pWide, *pHigh, *pOpaque, *pOmode;
     char **pStr;
{
  /* pops up 'Pad' options dialog box */

  int                rv, oldW, oldH, oldO;
  static int         firsttime=1;
  static const char *labels[] = { "\nOk", "\033Cancel" };

  if (firsttime) {
    padSbuf[0] = '\0';
    padBbuf[0] = '\0';
    padLbuf[0] = '\0';

    sprintf(padSinst, "Enter a color name ('orange'), %s%s",
	    "or an RGB color specification.  ",
	    "(e.g. 'r,g,b' or '0xrrggbb')");
    sprintf(padBinst, "Enter command line options for 'bggen'.  (%s)",
	    "No '-w', '-h', or '-g' options allowed.");
    sprintf(padLinst, "Enter a filename.  The padded image %s",
	    "will be the same size as the loaded image.");

    /* can't create MBUTT or DIALs here, parent window must exist first... */

    padMode = PAD_SOLID;
    padInst = padSinst;
    padOMode = PAD_ORGB;
    firsttime = 0;
  }


  buildPadLists();

  switch (padMode) {
  case PAD_SOLID:  strcpy(padBuf, padSbuf);  break;
  case PAD_BGGEN:  strcpy(padBuf, padBbuf);  break;
  case PAD_LOAD:   strcpy(padBuf, padLbuf);  break;
  }


  gsBuf    = padBuf;         gsBufLen = 256;
  gsFilter = "";             gsAllow  = 0;
  gsCurPos = strlen(gsBuf);
  gsStPos  = gsEnPos = 0;

  gsw = PAD_PUWIDE - 20;
  gsh = LINEHIGH+5;
  gsx = 10;
  gsy = 40;

  changedGSBuf();      /* careful!  popW doesn't exist yet! */

  if (padHaveDooDads) {
    oldW = (int)padWDial.val;
    oldH = (int)padHDial.val;
    oldO = (int)padODial.val;
  }
  else { oldW = pWIDE;  oldH = pHIGH;  oldO = 100; }



  rv = doPopUp("", labels, 2, ISPAD, "xv pad");



  if (rv == 0) {  /* copy padBuf to appropriate mode buffer */
    switch (padMode) {
    case PAD_SOLID:  strcpy(padSbuf, padBuf);  break;
    case PAD_BGGEN:  strcpy(padBbuf, padBuf);  break;
    case PAD_LOAD:   strcpy(padLbuf, padBuf);  break;
    }
  }

  if (rv == 1) {   /* cancelled:  restore normal values */
    DSetVal(&padWDial, (double)oldW);
    DSetVal(&padHDial, (double)oldH);
    DSetVal(&padODial, (double)oldO);
  }

  XUnmapWindow(theDisp, padWDial.win);
  XUnmapWindow(theDisp, padHDial.win);
  XUnmapWindow(theDisp, padODial.win);

  /* load up return values */
  *pMode   = padMode;
  *pStr    = padBuf;
  *pWide   = (int)padWDial.val;
  *pHigh   = (int)padHDial.val;
  *pOpaque = (int)padODial.val;
  *pOmode  = padOMode;

  return rv;
}


/***************************************************/
static void buildPadLists()
{
  /* generates padCol* and padBg* lists used in 'Defaults' MBUTT.  Grabs
     all the X resources values it can, and adds appropriate defaults */

  rd_str_cl("foo", "", 1);                    /* rebuild database */

  build1PadList("color", padColVals, padColNames, &padColLen,
		padColDefVals, padColDefNames, padColDefLen);

  build1PadList("bggen", padBgVals, padBgNames, &padBgLen,
		padBgDefVals, padBgDefNames, padBgDefLen);

  build1PadList("load", padLoadVals, padLoadNames, &padLoadLen,
		(const char **) NULL, (const char **) NULL, 0);
}


/***************************************************/
static void build1PadList(typstr, vals, nams, lenp, dvals, dnams, dlen)
     const char  *typstr;
     const char **vals, **nams;
     const char **dvals, **dnams;
     int         *lenp, dlen;
{
  int   i;
  char  resname[128];
  char *copy;

  for (i=0; i<*lenp; i++) {   /* kill old lists */
    free((char *) nams[i]);
    free((char *) vals[i]);
  }
  *lenp = 0;

  for (i=0; i<10; i++) {
    sprintf(resname, "pad.%s.val%d", typstr, i);
    if (rd_str_cl(resname, "Dialog.Menu.Slot",0)) {    /* got one! */
      copy = strdup(def_str);
      if (!copy) continue;
      vals[*lenp] = copy;

      sprintf(resname, "pad.%s.name%d", typstr, i);
      if (rd_str_cl(resname, "Dialog.Menu.Slot",0)) {  /* and it has a name! */
        copy = strdup(def_str);
	if (!copy) { free((char *) vals[*lenp]); continue; }
      }
      else {  /* it doesn't have a name.  fabricate one */
	copy = malloc((size_t) 32);
	if (!copy) { free((char *) vals[*lenp]); continue; }
	strncpy(copy, vals[*lenp], (size_t) 31);
	copy[31] = '\0';
      }
      if (strlen(copy) > (size_t) 20) {   /* fix long names */
	char *sp = copy + 18;

	*sp++ = '.';  *sp++ = '.';  *sp++ = '.';  *sp++ = '\0';
      }
      nams[*lenp] = copy;

      *lenp = (*lenp) + 1;
    }
  }


  /* add 'built-in' defaults to the lists */
  for (i=0; i<dlen && *lenp<PAD_MAXDEFLEN; i++) {
    copy = strdup(dvals[i]);
    if (!copy) break;
    vals[*lenp] = copy;

    copy = strdup(dnams[i]);
    if (!copy) { free((char *) vals[*lenp]); break; }
    nams[*lenp] = copy;

    *lenp = (*lenp) + 1;
  }
}



/***************************************************/
void ClosePopUp()
{
  /* closes popW:  if it's a pop-up, returns 'cancel'.  If it's an alert,
     simply closes it */

  if      (popUp == ISALERT) CloseAlert();
  else if (popUp == ISPOPUP) {
    popUp = 0;
    selected = nbts-1;
  }
}


/***************************************************/
void OpenAlert(txt)
     const char *txt;
{
  /* pops up a window with txt displayed in it (*no buttons*).
     returns immediately.  window is closed by 'CloseAlert()'.
     No 'PopUp()' calls are allowed while an Alert is displayed. */

  XEvent event;

  if (firsttime) createPUD();

  XStoreName(theDisp, popW, "xv notice");
  XSetIconName(theDisp, popW, "xv notice");
  attachPUD();

  nbts = 0;
  selected = 0;
  text = txt;

  puwide = PUWIDE;  puhigh = PUHIGH;
  XResizeWindow(theDisp, popW, (u_int) puwide, (u_int) puhigh);

  /* center last button in window around mouse position, with constraint that
     window be fully on the screen */

  CenterMapWindow(popW, puwide/2, puhigh/2, puwide, puhigh);
  popUp = ISALERT;

  /* MUST wait for VisibilityNotify event to come in, else we run the risk
     of UnMapping the window *before* the Map request completed.  This
     appears to be bad, (It leaves an empty window frame up.) though it
     generally only happens on slow servers.  Better safe than screwed... */

  XWindowEvent(theDisp, popW, VisibilityChangeMask, &event);
  drawPUD(0, 0, puwide, puhigh);
  XFlush(theDisp);
}


/***************************************************/
void CloseAlert()
{
  popUp = 0;
  XUnmapWindow(theDisp, popW);
}


/***************************************************/
int PUCheckEvent(xev)
     XEvent *xev;
{
  /* check event to see if it's for us.  If so, return 1, otherwise 0 */

  int rv = 0;

  if (!popUp) return(0);

  if (xev->type == Expose) {
    XExposeEvent *e = (XExposeEvent *) xev;
    if (e->window == popW) {
      drawPUD(e->x, e->y, e->width, e->height);
      rv = 1;
    }
    else if (popUp == ISPAD && padHaveDooDads && e->window == padWDial.win)
      { DRedraw(&padWDial);  rv = 1; }
    else if (popUp == ISPAD && padHaveDooDads && e->window == padHDial.win)
      { DRedraw(&padHDial);  rv = 1; }
    else if (popUp == ISPAD && padHaveDooDads && e->window == padODial.win)
      { DRedraw(&padODial);  rv = 1; }
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;

    if (e->button == Button1) {
      if (e->window == popW) {
	clickPUD(e->x,e->y);
	rv = 1;
      }
      else if (popUp == ISPAD && padHaveDooDads && e->window == padWDial.win)
	{ DTrack(&padWDial, e->x, e->y);  rv = 1; }
      else if (popUp == ISPAD && padHaveDooDads && e->window == padHDial.win)
	{ DTrack(&padHDial, e->x, e->y);  rv = 1; }
      else if (popUp == ISPAD && padHaveDooDads && e->window == padODial.win)
	{ DTrack(&padODial, e->x, e->y);  rv = 1; }
    }
  }


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;
    int stlen, i, shift, ck;

    stlen = XLookupString(e,buf,128,&ks,(XComposeStatus *) NULL);
    shift = e->state & ShiftMask;
    ck = CursorKey(ks, shift, 0);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    /* check cursor keys, which may or may not have a str assoc'd with them */
    if (popUp==ISGETSTR || popUp==ISGRAB || popUp==ISPAD) {
      if      (ck==CK_LEFT)  { doGetStrKey('\002'); rv = 1; }
      else if (ck==CK_RIGHT) { doGetStrKey('\006'); rv = 1; }
    }

    if (stlen && !rv) {      /* note: we accept kbd accel's in any win */
      if (buf[0] == '\r') buf[0] = '\n';

      /* search for character in accel table */
      for (i=0; i<nbts; i++) {
	if (buf[0] == accel[i] && buf[0] != ' ') {
	  FakeButtonPress(&bts[i]);
	  rv = 1;
	}
      }

      if (!rv && buf[0]=='\033' && nbts==1) { /* ESC accepted in 1-but pu's */
	FakeButtonPress(&bts[0]);
	rv = 1;
      }

      if (!rv && (popUp==ISGETSTR || popUp==ISGRAB || popUp==ISPAD)) {
	if (e->window == popW) { doGetStrKey(buf[0]);  rv = 1; }
      }
    }

    if (!stlen) rv = 1;  /* quietly eat mute keys */
  }


  else if (xev->type == ClientMessage) {
    Atom proto, delwin;
    XClientMessageEvent *client_event = (XClientMessageEvent *) xev;

    proto  = XInternAtom(theDisp, "WM_PROTOCOLS", FALSE);
    delwin = XInternAtom(theDisp, "WM_DELETE_WINDOW", FALSE);

    if (client_event->message_type == proto &&
	client_event->data.l[0]    == delwin) {
      /* it's a WM_DELETE_WINDOW event */

      if (client_event->window == popW) {
	FakeButtonPress(&bts[(nbts>1) ? nbts-1 : 0]);
	rv = 1;
      }
    }
  }

  if (rv==0 && (xev->type == KeyPress || xev->type == ButtonPress)) {
    XBell(theDisp, 0);
    rv = 1;            /* eat it */
  }

  return rv;
}



#define TR_MAXLN 10

/***************************************************/
static void TextRect(win, txt, x, y, w, h, fg)
     Window      win;
     const char *txt;
     int         x,y,w,h;
     u_long      fg;
{
  /* draws semi-complex strings in a rectangle */

  const char *sp;
  const char *ep;
  const char *oldep;
  const char *start[TR_MAXLN];
  int         i, inbreak, lineno, top, hardcr, maxln, len[TR_MAXLN];

  XSetForeground(theDisp, theGC, fg);

  sp = txt;  lineno = hardcr = 0;

  maxln = h / LINEHIGH;
  RANGE(maxln,0,TR_MAXLN);
  while (*sp && lineno<maxln) {

    /* drop off any leading spaces (except on first line or after \n) */
    if (sp!=txt && !hardcr) {
      while (*sp==' ') sp++;
    }

    hardcr = 0;   ep = sp;

    /* increment ep until we   A) get too wide, B) hit eos or
       C) hit a '\n' character */

    /* NOTE: ep points to the character AFTER the end of the line */

    while (XTextWidth(mfinfo, sp, (int)(ep-sp))<= w && *ep && *ep!='\n') ep++;
    if (*ep=='\n') { ep++;  hardcr=1; }   /* eat newline */

    /* if we got too wide, back off until we find a break position
       (last char before a space or a '/') */

    if (XTextWidth(mfinfo, sp, (int)(ep-sp)) > w) {
      oldep = ep;  inbreak = 0;
      while (ep!=sp) {
	ep--;
	if ( inbreak && *ep!=' ') { ep++;  break; }
	if (!inbreak && *ep==' ') inbreak = 1;
	if (*ep=='/') { ep++; break; }
      }
      if (ep==sp) ep = oldep-1;  /* can't break this line.  oh well */
    }

    start[lineno] = sp;  len[lineno] = ep-sp;

    /* make sure we don't print a trailing '\n' character! */
    if (len[lineno] > 0) {
      while (sp[len[lineno]-1] == '\n') len[lineno] = len[lineno] - 1;
    }

    sp = ep;
    lineno++;
  }

  top = y + h/2 + (ASCENT-DESCENT)/2 - ((lineno-1)*LINEHIGH)/2;
  if (top<y+ASCENT) top = y+ASCENT;

  for (i=0, y=top; i<lineno; i++, y+=LINEHIGH) {
    if (start[i][0] != '\n')
      XDrawString(theDisp, win, theGC, x, y, start[i], len[i]);
  }
}


/***************************************************/
static void createPUD()
{
  popW = CreateWindow("xv confirm", "XVconfirm", "+0+0",
		      PUWIDE, PUHIGH, infofg, infobg, 0);
  if (!popW) FatalError("can't create popup window!");

  XSelectInput(theDisp, popW, ExposureMask | ButtonPressMask | KeyPressMask
	       | VisibilityChangeMask);
  /* XSetTransientForHint(theDisp, popW, mainW); */

  XDefineCursor(theDisp, popW, arrow);
  bts = (BUTT *) NULL;
  nbts = selected = firsttime = 0;
}


/***************************************************/
static void attachPUD()
{
  /* used to make PUD a transient window of something.  Doesn't
     do anything anymore, as I got tired of having window layering
     shifted around everytime a popup window happened.  Screw the
     business about having the popup iconify when you iconify the
     appropriate XV window.  There generally ISN'T an appropriate
     XV window... */
}


/***************************************************/
static void drawPUD(x,y,w,h)
int x,y,w,h;
{
  int  i,xt,yt;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  if (popUp == ISGRAB) {
    xt = 10;  yt = 10;
    TextRect(popW, text, xt, yt, puwide-10-xt, gsy-20, infofg);
    drawGSBuf();

    XSetForeground(theDisp, theGC, infofg);
    DrawString(popW, 10,        gsy+ASCENT+4, DELAYSTR);
    DrawString(popW, gsx+gsw+5, gsy+ASCENT+4, SECSTR);

    CBRedraw(&ahideCB);
  }

  else if (popUp == ISPAD) {
    drawGSBuf();

    XSetForeground(theDisp, theGC, infofg);
    DrawString(popW, 10+44,10+ASCENT+4,"Pad Method:");

    MBRedraw(&padMthdMB);
    MBRedraw(&padDfltMB);
    DRedraw (&padWDial);
    DRedraw (&padHDial);
    BTRedraw(&padDButt);
    BTRedraw(&padOMButt);

    XSetForeground(theDisp, theGC, infofg);
    drawPadOMStr();

    XDrawRectangle(theDisp, popW, theGC, 10, puhigh-140, 16+2*74+84, 130);
    Draw3dRect(popW, 10+1, puhigh-140+1, 16+2*74+84-2, 130-2,
	       R3D_IN,2,hicol,locol,infobg);
    XSetForeground(theDisp, theGC, infofg);
    CenterString(popW, 16+1+75-13, puhigh-16-100-12, "New Image Size");

    if (ctrlColor) {
      XSetForeground(theDisp, theGC, locol);
      XDrawLine(theDisp, popW, theGC, 16+1+75+75+5, puhigh-140 + 6+8,
		16+1+75+75+5, puhigh-10-4);
    }


    XSetForeground(theDisp, theGC, infofg);
    XDrawRectangle(theDisp, popW, theGC, 268, puhigh-140,
		   (u_int) puwide - 10 - 268, 130-BUTTH-10);
    Draw3dRect(popW, 268+1, puhigh-140+1, (u_int) puwide -10-268-2,
	       130-2 - BUTTH-10, R3D_IN,2,hicol,locol,infobg);

    TextRect(popW,padInst,268+5, puhigh-140+3, puwide-10-268-10,
	     130-6 - BUTTH-10, infofg);
  }

  else {
    XCopyPlane(theDisp, iconPix, popW, theGC, 0,0, icon_width, icon_height,
	       10,10+(puhigh-30-BUTTH-icon_height)/2, 1L);

    xt = 10+icon_width+20;  yt = 10;

    if (popUp == ISGETSTR) {
      TextRect(popW, text, xt, yt, puwide-10-xt, gsy-20, infofg);
      drawGSBuf();
    }
    else TextRect(popW,text,xt,yt,puwide-10-xt,puhigh-10-BUTTH-20,infofg);
  }


  for (i=0; i<nbts; i++) BTRedraw(&bts[i]);
  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void drawPadOMStr()
{
  CenterString(popW, padODial.x + (padODial.w - 13)/2,
	       puhigh-16-100-12, padOMStr[padOMode]);
}

/***************************************************/
static void clickPUD(x,y)
     int x,y;
{
  int i;
  BUTT *bp = NULL;

  for (i=0; i<nbts; i++) {
    bp = &bts[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<nbts && bp && BTTrack(bp)) {
    popUp = 0;  selected = i;  return;
  }

  if (popUp==ISGRAB && CBClick(&ahideCB, x,y)) CBTrack(&ahideCB);

  else if (popUp == ISPAD) {
    if (PTINRECT(x, y, padDButt.x, padDButt.y, padDButt.w, padDButt.h)) {
      if (BTTrack(&padDButt)) {
	DSetVal(&padWDial, (double)pWIDE);
	DSetVal(&padHDial, (double)pHIGH);
      }
    }

    else if (PTINRECT(x,y,padOMButt.x,padOMButt.y,padOMButt.w,padOMButt.h)) {
      if (BTTrack(&padOMButt)) {
	XSetForeground(theDisp, theGC, infobg);
	drawPadOMStr();
	padOMode = (padOMode + 1) % PAD_OMAX;
	XSetForeground(theDisp, theGC, infofg);
	drawPadOMStr();
      }
    }


    else if (MBClick(&padMthdMB, x,y)) {
      i = MBTrack(&padMthdMB);
      if (i<0 || i==padMode) return;

      switch (i) {
      case PAD_SOLID:
	strcpy(padBuf, padSbuf);
	padDfltMB.list  = padColNames;
	padDfltMB.nlist = padColLen;
	padInst = padSinst;
	break;
      case PAD_BGGEN:
	strcpy(padBuf, padBbuf);
	padDfltMB.list  = padBgNames;
	padDfltMB.nlist = padBgLen;
	padInst = padBinst;
	break;
      case PAD_LOAD:
	strcpy(padBuf,  padLbuf);
	padDfltMB.list  = padLoadNames;
	padDfltMB.nlist = padLoadLen;
	padInst = padLinst;
	break;
      default: break;             /* shouldn't happen */
      }

      gsCurPos = strlen(gsBuf);
      gsStPos = gsEnPos = 0;
      changedGSBuf();
      if (ctrlColor)
	XClearArea(theDisp, popW, gsx+3,gsy+3,
		   (u_int)gsw-5, (u_int)gsh-5, False);
      else
	XClearArea(theDisp, popW, gsx+1,gsy+1,
		   (u_int)gsw-1, (u_int)gsh-1, False);
      drawGSBuf();

      BTSetActive(&bts[0], (int) strlen(gsBuf));

      MBSelect(&padMthdMB, i);
      MBSetActive(&padDfltMB, 1);
      DSetActive (&padWDial,  (i!=PAD_LOAD));
      DSetActive (&padHDial,  (i!=PAD_LOAD));

      XClearArea(theDisp, popW, 184+5, puhigh-140+3,
		 (u_int) puwide-10-184-10, 130-6 - BUTTH-10, True);

      padMode = i;
    }

    else if (MBClick(&padDfltMB, x,y)) {
      i = MBTrack(&padDfltMB);
      if (i<0) return;

      if      (padMode == PAD_SOLID) strcpy(padBuf, padColVals[i]);
      else if (padMode == PAD_BGGEN) strcpy(padBuf, padBgVals[i]);
      else if (padMode == PAD_LOAD)  strcpy(padBuf, padLoadVals[i]);

      gsCurPos = strlen(gsBuf);
      gsStPos = gsEnPos = 0;
      changedGSBuf();
      if (ctrlColor)
	XClearArea(theDisp, popW, gsx+3,gsy+3,
		   (u_int)gsw-5, (u_int)gsh-5, False);
      else
	XClearArea(theDisp, popW, gsx+1,gsy+1,
		   (u_int)gsw-1, (u_int)gsh-1, False);
      drawGSBuf();

      BTSetActive(&bts[0], (int) strlen(gsBuf));
    }
  }
}



/***************************************************/
static void doGetStrKey(c)
     int c;
{
  if (doGSKey(c)) XBell(theDisp, 0);
}


/***************************************************/
static int doGSKey(c)
     int c;
{
  /* handle characters typed at GetStrPopUp window.  Button accel. keys
     have already been checked for elsewhere.  Practical upshot is that
     we don't have to do anything with ESC or Return (as these will normally
     be Cancel and Ok buttons)

     Normally returns '0'.  Returns '1' if character wasn't accepted, for
     whatever reason. */

  int i, len, flen;

  len = strlen(gsBuf);
  if (gsFilter) flen = strlen(gsFilter);
           else flen = 0;


  if (c>=' ' && c<'\177') {              /* 'NORMAL' CHARACTERS */
    if (flen) {                          /* check filter string */
      for (i=0; i<flen && c!=gsFilter[i]; i++);
      if (!gsAllow && i< flen) return 1;    /* found in 'disallow' filter */
      if ( gsAllow && i==flen) return 1;    /* not found in 'allow' filter */
    }

    if (len >= gsBufLen-1) return 1;     /* at max length */

    xvbcopy(&gsBuf[gsCurPos], &gsBuf[gsCurPos+1], (size_t) len-gsCurPos+1);
    gsBuf[gsCurPos]=c;  gsCurPos++;
  }


  else if (c=='\010') {                 /* BS */
    if (gsCurPos==0) return 1;                     /* at beginning of str */
    xvbcopy(&gsBuf[gsCurPos], &gsBuf[gsCurPos-1], (size_t) len-gsCurPos+1);
    gsCurPos--;
  }

  else if (c=='\025') {                 /* ^U: clear entire line */
    gsBuf[0] = '\0';
    gsCurPos = 0;
  }

  else if (c=='\013') {                 /* ^K: clear to end of line */
    gsBuf[gsCurPos] = '\0';
  }

  else if (c=='\001') {                 /* ^A: move to beginning */
    gsCurPos = 0;
  }

  else if (c=='\005') {                 /* ^E: move to end */
    gsCurPos = len;
  }

  else if (c=='\004' || c=='\177') {    /* ^D or DEL: delete character at gsCurPos */
    if (gsCurPos==len) return 1;
    xvbcopy(&gsBuf[gsCurPos+1], &gsBuf[gsCurPos], (size_t) len-gsCurPos);
  }

  else if (c=='\002') {                 /* ^B: move backwards char */
    if (gsCurPos==0) return 1;
    gsCurPos--;
  }

  else if (c=='\006') {                 /* ^F: move forwards char */
    if (gsCurPos==len) return 1;
    gsCurPos++;
  }

  else return 1;                        /* unhandled character */

  changedGSBuf();      /* compute gsEnPos, gsStPos */

  if (ctrlColor)
    XClearArea(theDisp, popW, gsx+3,gsy+3, (u_int)gsw-5, (u_int)gsh-5, False);
  else
    XClearArea(theDisp, popW, gsx+1,gsy+1, (u_int)gsw-1, (u_int)gsh-1, False);

  drawGSBuf();

  if (popUp == ISGETSTR || popUp == ISPAD) {
    /* if we have a string of any sort, turn on the default '\n' button
       (if there is one) */
    for (i=0; i<nbts && accel[i]!='\n'; i++);
    if (i<nbts) BTSetActive(&bts[i], (strlen(gsBuf) > (size_t) 0));
  }
  else if (popUp == ISGRAB) {
    /* need a string of length 1 to enable Grab (bts[0]), and a string
       with an atoi() of at least '1' to enable AutoGrab (bts[1]) */
    BTSetActive(&bts[0], (strlen(gsBuf) > (size_t) 0));
    BTSetActive(&bts[1], (strlen(gsBuf)>(size_t)0 && atoi(gsBuf)>(size_t)0));
  }

  return(0);
}



/***************************************************/
static void changedGSBuf()
{
  /* cursor position (or whatever) may have changed.  adjust displayed
     portion of gsBuf */

  int len;

  len = strlen(gsBuf);

  if (gsCurPos < gsStPos) gsStPos = gsCurPos;
  if (gsCurPos > gsEnPos) gsEnPos = gsCurPos;

  if (gsStPos>len) gsStPos = (len>0) ? len-1 : 0;
  if (gsEnPos>len) gsEnPos = (len>0) ? len-1 : 0;

  /* while substring is shorter than window, inc enPos */

  while (XTextWidth(mfinfo, &gsBuf[gsStPos], gsEnPos-gsStPos) < (gsw-6)
	 && gsEnPos<len) { gsEnPos++; }

  /* while substring is longer than window, dec enpos, unless enpos==curpos,
     in which case, inc stpos */

  while (XTextWidth(mfinfo, &gsBuf[gsStPos], gsEnPos-gsStPos) > (gsw-6)) {
    if (gsEnPos != gsCurPos) gsEnPos--;
    else gsStPos++;
  }
}


/***************************************************/
static void drawGSBuf()
{
  /* draw edittext thingy in GetStrPopUp window */

  int cpos;

  XSetForeground(theDisp, theGC, infofg);
  XDrawRectangle(theDisp, popW, theGC, gsx, gsy, (u_int) gsw, (u_int) gsh);
  Draw3dRect(popW, gsx+1, gsy+1, (u_int) gsw-2, (u_int) gsh-2,
	     R3D_IN, 2, hicol,locol,infobg);

  XSetForeground(theDisp, theGC, infofg);

  if (gsStPos>0) {  /* draw a "there's more over here" doowah */
    XDrawLine(theDisp, popW, theGC, gsx+1, gsy+1, gsx+1, gsy + gsh-1);
    XDrawLine(theDisp, popW, theGC, gsx+2, gsy+1, gsx+2, gsy + gsh-1);
    XDrawLine(theDisp, popW, theGC, gsx+3, gsy+1, gsx+3, gsy + gsh-1);
  }

  if ((size_t) gsEnPos < strlen(gsBuf)) {
    /* draw a "there's more over here" doowah */
    XDrawLine(theDisp, popW, theGC, gsx+gsw-3, gsy+1, gsx+gsw-3, gsy+gsh-1);
    XDrawLine(theDisp, popW, theGC, gsx+gsw-2, gsy+1, gsx+gsw-2, gsy+gsh-1);
    XDrawLine(theDisp, popW, theGC, gsx+gsw-1, gsy+1, gsx+gsw-1, gsy+gsh-1);
  }

  XDrawString(theDisp, popW, theGC, gsx+4, gsy+ASCENT+4,
	      gsBuf+gsStPos, gsEnPos-gsStPos);

  cpos = gsx+XTextWidth(mfinfo, &gsBuf[gsStPos], gsCurPos-gsStPos);
  XDrawLine(theDisp,popW,theGC, 4+cpos, gsy+3,         4+cpos, gsy+2+CHIGH+1);
  XDrawLine(theDisp,popW,theGC, 4+cpos, gsy+2+CHIGH+1, 6+cpos, gsy+2+CHIGH+3);
  XDrawLine(theDisp,popW,theGC, 4+cpos, gsy+2+CHIGH+1, 2+cpos, gsy+2+CHIGH+3);
}
