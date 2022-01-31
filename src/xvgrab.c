/*
 * xvgrab.c - image grabbing (from the display) functions for XV
 *
 *  Author:    John Bradley, University of Pennsylvania
 *
 *  Contains:
 *     int Grab()             - handles the GRAB command
 *     int LoadGrab();        - 'loads' the pic from the last succesful Grab
 *
 */

#include "copyright.h"

#define NEEDSTIME
#include "xv.h"

/* Allow flexibility in use of buttons JPD */
#define WINDOWGRABMASK Button1Mask  /* JPD prefers Button2Mask */
#define RECTGTRACKMASK Button2Mask  /* JPD prefers Button1Mask*/
#define CANCELGRABMASK Button3Mask

#define DO_GRABFLASH  /* JPD prefers not to do that; just a loss of time ... */


union swapun {
  CARD32 l;
  CARD16 s;
  CARD8  b[sizeof(CARD32)];
};


struct rectlist {
  int x,y,w,h;
  struct rectlist *next;
};


static byte            *grabPic = (byte *) NULL;
static int              gptype;
static byte             grabmapR[256], grabmapG[256], grabmapB[256];
static int              gXOFF, gYOFF, gWIDE,gHIGH;
static int              grabInProgress=0;
static int              hidewins = 0;
static GC               rootGC;
static struct rectlist *regrabList;


static void   flashrect           PARM((int, int, int, int, int));
static void   startflash          PARM((void));
static void   endflash            PARM((void));
static void   ungrabX             PARM((void));
static int    lowbitnum           PARM((unsigned long));
static int    getxcolors          PARM((XWindowAttributes *, XColor **));

static void   printWinTree        PARM((Window, int));
static void   errSpace            PARM((int));

static int    grabRootRegion      PARM((int, int, int, int));
static int    grabWinImage        PARM((Window, VisualID, Colormap, int));
static int    convertImageAndStuff PARM((XImage *, XColor *, int,
					 XWindowAttributes *,
					 int,int,int,int));

static int    RectIntersect       PARM((int,int,int,int, int,int,int,int));

static int    CountColors24       PARM((byte *, int, int,
					int, int, int, int));

static int    Trivial24to8        PARM((byte *, int, int, byte *,
					byte *, byte *, byte *, int));

/***********************************/
int Grab()
{
  /* does UI part of Grab command.  returns '1' if a new image was grabbed,
     0 if cancelled */

  int          i, x, y, x1, y1, x2, y2, ix, iy, iw, ih, rv;
  int          rx, ry, GotButton, autograb;
  int          cancelled = 0;
  Window       rW, cW, clickWin;
  unsigned int mask;
#ifdef RECOLOR_GRAB_CURSOR
  XColor       fc, bc;
#endif

  GotButton = 0;

  if (grabInProgress) return 0;      /* avoid recursive grabs during delay */

  /* do the dialog box thing */
  i = GrabPopUp(&hidewins, &grabDelay);
  if (i==2) return 0;    /* cancelled */
  autograb = (i==1);

  if (hidewins) {                   /* unmap all XV windows */
    autoclose += 2;                 /* force it on once */
    if (mainW && dispMode==RMB_WINDOW) XUnmapWindow(theDisp, mainW);
    else if (ctrlW) CtrlBox(0);
  }


  XSync(theDisp, False);

  if (grabDelay>0) {    /* instead of sleep(), handle events while waiting */
    time_t startT, t;
    int  done;

    grabInProgress = 1; /* guard against recursive grabs during delay */
    time(&startT);
    while (1) {
      time(&t);
      if (t >= startT + grabDelay) break;
      if (XPending(theDisp)>0) {
	XEvent evt;
	XNextEvent(theDisp, &evt);
	i = HandleEvent(&evt, &done);
	if (done) {                    /* only 'new image' cmd accepted=quit */
	  if (i==QUIT) Quit(0);
	  else XBell(theDisp, 0);
	}
      }
      Timer(100);
    }
    grabInProgress = 0;
  }


  rootGC   = DefaultGC(theDisp, theScreen);

  if (grabPic) {  /* throw away previous 'grabbed' pic, if there is one */
    free(grabPic);  grabPic = (byte *) NULL;
  }

  /* recolor cursor to indicate that grabbing is active? */
  /* Instead, change cursor JPD */
#ifdef RECOLOR_GRAB_CURSOR
  fc.flags = bc.flags = DoRed | DoGreen | DoBlue;
  fc.red = fc.green = fc.blue = 0xffff;
  bc.red = bc.green = bc.blue = 0x0000;
  XRecolorCursor(theDisp, tcross, &fc, &bc);
#endif


  XBell(theDisp, 0);		/* beep once at start of grab */

  /* Change cursor to top_left_corner JPD */
  XGrabPointer(theDisp, rootW, False,
     PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
     GrabModeAsync, GrabModeAsync, None, tlcorner, CurrentTime);

  if (!autograb) XGrabButton(theDisp, (u_int) AnyButton, 0, rootW, False, 0,
			     GrabModeAsync, GrabModeSync, None, tcross);

  if (autograb) {
    XGrabServer(theDisp);	 /* until we've done the grabImage */
    if (!XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x1,&y1,&mask)) {
      fprintf(stderr, "xv: XQueryPointer failed!\n");
      XUngrabServer(theDisp);
      rv = 0;
      goto exit;
    }
    else { GotButton = 1;  mask = WINDOWGRABMASK; }
  }

  else {   /* !autograb */
    /* wait for a button press */
    while (1) {
      XEvent evt;  int done;

      /* recolor cursor to indicate that grabbing is active? */

      if (XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x1,&y1,&mask)) {
	if (mask & (Button1Mask | Button2Mask | Button3Mask)) break;
      }

      /* continue to handle events while waiting... */
      XNextEvent(theDisp, &evt);
      i = HandleEvent(&evt, &done);
      if (done) {                    /* only 'new image' cmd accepted=quit */
	if (i==QUIT) {
	  XUngrabButton(theDisp, (u_int) AnyButton, 0, rootW);
	  Quit(0);
	}
	else XBell(theDisp, 0);
      }

    }
  }

  XUngrabPointer(theDisp, CurrentTime);
  /* Reset cursor to XC_tcross JPD */
  XGrabPointer(theDisp, rootW, False,
     PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
     GrabModeAsync, GrabModeAsync, None, tcross, CurrentTime);

  /***
   ***  got button click (or pretending we did, if autograb)
   ***/

  if (mask & CANCELGRABMASK || rW!=rootW) {        /* CANCEL GRAB */
    while (1) {      /* wait for button to be released */
      if (XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x1,&y1,&mask)) {
	if (!(mask & CANCELGRABMASK)) break;
      }
    }

    XUngrabButton(theDisp, (u_int) AnyButton, 0, rootW);
    XBell(theDisp, 0);
    XBell(theDisp, 0);
    rv = 0;
    cancelled = 1;
    goto exit;
  }


  if (mask & WINDOWGRABMASK) {  /* GRAB WINDOW (& FRAME, maybe)     */
    while (!GotButton) {  /* wait for button to be released, if clicked */
      int rx,ry,x1,y1;  Window rW, cW;
      if (XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x1,&y1,&mask)) {
	if (!(mask & WINDOWGRABMASK)) break;
      }
    }

  grabwin:

    clickWin = (cW) ? cW : rootW;

    if (clickWin == rootW) {   /* grab entire screen */
      if (DEBUG) fprintf(stderr,"Grab: clicked on root window.\n");
      ix = iy = 0;  iw = dispWIDE;  ih = dispHIGH;
    }
    else {
      int x,y;  Window win;   unsigned int rw,rh,rb,rd;

      if (XGetGeometry(theDisp,clickWin,&rW, &x, &y, &rw, &rh, &rb, &rd)) {
	iw = (int) rw;  ih = (int) rh;

	XTranslateCoordinates(theDisp, clickWin, rootW, 0, 0, &ix,&iy, &win);

	if (DEBUG) fprintf(stderr,"clickWin=0x%x: %d,%d %dx%d depth=%ud\n",
			   (u_int) clickWin, ix, iy, iw, ih, rd);
      }
      else {
	ix = iy = 0;  iw = dispWIDE;  ih = dispHIGH;  clickWin = rootW;
	if (DEBUG) fprintf(stderr,"XGetGeometry failed? (using root win)\n");
      }
    }

    /* range checking:  keep rectangle fully on-screen */
    if (ix<0) { iw += ix;  ix = 0; }
    if (iy<0) { ih += iy;  iy = 0; }
    if (ix+iw>dispWIDE) iw = dispWIDE-ix;
    if (iy+ih>dispHIGH) ih = dispHIGH-iy;


    if (DEBUG) fprintf(stderr,"using %d,%d (%dx%d)\n", ix, iy, iw, ih);

    /* flash the rectangle a bit... */
    startflash();
    for (i=0; i<5; i++) {
      flashrect(ix, iy, iw, ih, 1);
      XFlush(theDisp);  Timer(100);
      flashrect(ix, iy, iw, ih, 0);
      XFlush(theDisp);  Timer(100);
    }
    endflash();
  }

  else {  /* TRACK A RECTANGLE */
    int    origrx, origry;

    clickWin = rootW;
    origrx = ix = x2 = rx;
    origry = iy = y2 = ry;
    iw = ih = 0;

    XGrabServer(theDisp);
    startflash();

    /* Wait for button release while tracking rectangle on screen */
    while (1) {
      if (XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	if (!(mask & RECTGTRACKMASK)) break;
      }

      flashrect(ix, iy, iw, ih, 0);                /* turn off rect */

      if (rx!=x2 || ry!=y2) {                      /* resize rectangle */
	ix = (x1<rx) ? x1 : rx;
	iy = (y1<ry) ? y1 : ry;
	iw = abs(rx - x1);  ih = abs(ry - y1);
	x2 = rx;  y2 = ry;
      }

      if (iw>1 && ih>1) flashrect(ix,iy,iw,ih,1);  /* turn on rect */
    }

    flashrect(ix, iy, iw, ih, 0);                  /* turn off rect */

#ifdef DO_GRABFLASH
    /* flash the rectangle a bit... */
    for (i=0; i<5; i++) {
      flashrect(ix, iy, iw, ih, 1);
      XFlush(theDisp);  Timer(100);
      flashrect(ix, iy, iw, ih, 0);
      XFlush(theDisp);  Timer(100);
    }
#endif

    endflash();

    /* if rectangle has zero width or height, search for child window JPD */
    if (iw==0 && ih==0) {
       int xr, yr;
       Window childW = 0;
       if (rW && cW)
          XTranslateCoordinates(theDisp, rW, cW, rx, ry, &xr, &yr, &childW);
       if (childW)
          cW = childW;
       goto grabwin;
    }

    XUngrabServer(theDisp);
  }

  /***
   ***  now that clickWin,ix,iy,iw,ih are known, try to grab the bits :
   ***  grab screen area (ix,iy,iw,ih)
   ***/


  if (DEBUG>1) printWinTree(clickWin, 0);

  WaitCursor();

  if (!autograb) XGrabServer(theDisp);	 /* until we've done the grabImage */
  rv = grabRootRegion(ix, iy, iw, ih);   /* ungrabs the server & button */

  SetCursors(-1);

 exit:

  XUngrabPointer(theDisp, CurrentTime);
  XUngrabServer(theDisp);

  if (startGrab) {
    startGrab = 0;
    if (cancelled) Quit(0);
  }

  if (hidewins) {                   /* remap XV windows */
    autoclose += 2;                 /* force it on once */
    if (mainW && dispMode == RMB_WINDOW) {
      int state;

      XMapRaised(theDisp, mainW);
      XSync(theDisp, False);        /* get that damned window on screen */

      if (DEBUG) fprintf(stderr,"==remapped mainW.  waiting for Config.\n");

      /* sit here until we see a MapNotify on mainW followed by a
	 ConfigureNotify on mainW */

      state = 0;
      while (state != 3) {
	XEvent event;
	XNextEvent(theDisp, &event);
	HandleEvent(&event, &i);

	if (!(state&1) && event.type == MapNotify &&
	    event.xmap.window == mainW) state |= 1;

	if (!(state&2) && event.type == ConfigureNotify &&
	    event.xconfigure.window == mainW) state |= 2;
      }

      if (DEBUG) fprintf(stderr,"==after remapping mainW, GOT Config.\n");
    }

    else if (ctrlW) CtrlBox(1);
  }

  return rv;
}


/***********************************/
int LoadGrab(pinfo)
     PICINFO *pinfo;
{
  /* loads up (into XV structures) last image successfully grabbed.
     returns '0' on failure, '1' on success */

  int   i;

  if (!grabPic) return 0;   /* no image to use */

  pinfo->type = gptype;
  if (pinfo->type == PIC8) {
    for (i=0; i<256; i++) {
      pinfo->r[i] = grabmapR[i];
      pinfo->g[i] = grabmapG[i];
      pinfo->b[i] = grabmapB[i];
    }
  }

  pinfo->pic     = grabPic;
  pinfo->normw   = pinfo->w   = gWIDE;
  pinfo->normh   = pinfo->h   = gHIGH;
  pinfo->frmType = -1;
  pinfo->colType = -1;

  sprintf(pinfo->fullInfo,"<%s internal>",
	  (pinfo->type == PIC8) ? "8-bit" : "24-bit");

  sprintf(pinfo->shrtInfo,"%dx%d image.",gWIDE, gHIGH);

  pinfo->comment = (char *) NULL;

  grabPic = (byte *) NULL;

  return 1;
}


/***********************************/
static void flashrect(x,y,w,h,show)
     int x,y,w,h,show;
{
  static int isvis  = 0;
  static int maskno = 0;

  XSetPlaneMask(theDisp, rootGC, xorMasks[maskno]);

  if (!show) {     /* turn off rectangle */
    if (isvis)
      XDrawRectangle(theDisp, rootW, rootGC, x, y, (u_int) w-1, (u_int) h-1);

    isvis = 0;
  }
  else {           /* show rectangle */
    if (!isvis && w>1 && h>1) {
      maskno = (maskno + 1) & 7;
      XSetPlaneMask(theDisp, rootGC, xorMasks[maskno]);
      XDrawRectangle(theDisp, rootW, rootGC, x, y, (u_int) w-1, (u_int) h-1);
      isvis = 1;
    }
  }
}


/***********************************/
static void startflash()
{
  /* set up for drawing a flashing rectangle */
  XSetFunction(theDisp, rootGC, GXinvert);
  XSetSubwindowMode(theDisp, rootGC, IncludeInferiors);
}


/***********************************/
static void endflash()
{
  XSetFunction(theDisp, rootGC, GXcopy);
  XSetSubwindowMode(theDisp, rootGC, ClipByChildren);
  XSetPlaneMask(theDisp, rootGC, AllPlanes);
  XSync(theDisp, False);
}


/***********************************/
static void ungrabX()
{
  XUngrabServer(theDisp);
  XUngrabButton(theDisp, (u_int) AnyButton, 0, rootW);
}


/**************************************/
static int lowbitnum(ul)
     unsigned long ul;
{
  /* returns position of lowest set bit in 'ul' as an integer (0-31),
   or -1 if none */

  int i;
  for (i=0; ((ul&1) == 0) && i<32;  i++, ul>>=1);
  if (i==32) i = -1;
  return i;
}



/**********************************************/
/* getxcolors() function snarfed from 'xwd.c' */
/**********************************************/

#define lowbit(x) ((x) & (~(x) + 1))

static int getxcolors(win_info, colors)
     XWindowAttributes *win_info;
     XColor **colors;
{
  int i, ncolors;

  *colors = (XColor *) NULL;

  if (win_info->visual->class == TrueColor) {
    if (DEBUG>1) fprintf(stderr,"TrueColor visual:  no colormap needed\n");
    return 0;
  }

  else if (!win_info->colormap) {
    if (DEBUG>1) fprintf(stderr,"no colormap associated with window\n");
    return 0;
  }

  ncolors = win_info->visual->map_entries;
  if (DEBUG>1) fprintf(stderr,"%d entries in colormap\n", ncolors);

  if (!(*colors = (XColor *) malloc (sizeof(XColor) * ncolors)))
    FatalError("malloc failed in getxcolors()");


  if (win_info->visual->class == DirectColor) {
    Pixel red, green, blue, red1, green1, blue1;

    if (DEBUG>1) fprintf(stderr,"DirectColor visual\n");

    red = green = blue = 0;
    red1   = lowbit(win_info->visual->red_mask);
    green1 = lowbit(win_info->visual->green_mask);
    blue1  = lowbit(win_info->visual->blue_mask);
    for (i=0; i<ncolors; i++) {
      (*colors)[i].pixel = red|green|blue;
      (*colors)[i].pad = 0;
      red += red1;
      if (red > win_info->visual->red_mask)     red = 0;
      green += green1;
      if (green > win_info->visual->green_mask) green = 0;
      blue += blue1;
      if (blue > win_info->visual->blue_mask)   blue = 0;
    }
  }
  else {
    for (i=0; i<ncolors; i++) {
      (*colors)[i].pixel = i;
      (*colors)[i].pad = 0;
    }
  }

  XQueryColors(theDisp, win_info->colormap, *colors, ncolors);

  return(ncolors);
}



/*******************************************/
static void printWinTree(win,tab)
     Window win;
     int    tab;
{
  u_int             i, nchildren;
  Window            root, parent, *children, chwin;
  XWindowAttributes xwa;
  int               xr, yr;

  if (!XGetWindowAttributes(theDisp, win, &xwa)) {
    errSpace(tab);
    fprintf(stderr,"pWT: can't XGetWindowAttributes(%08x)\n", (u_int) win);
    return;
  }

  XTranslateCoordinates(theDisp, win, rootW, 0,0, &xr,&yr, &chwin);
  if (xwa.map_state==IsViewable) {
    errSpace(tab);
    fprintf(stderr,"%08x: %4d,%4d %4dx%4d vis: %02x  cm=%x  %s\n",
	    (u_int) win, xr,yr, xwa.width, xwa.height,
	    (u_int) XVisualIDFromVisual(xwa.visual),
	    (u_int) xwa.colormap,
	    ((xwa.map_state==IsUnmapped)      ? "unmapped  "  :
	     (xwa.map_state==IsUnviewable)    ? "unviewable"  :
	     (xwa.map_state==IsViewable)      ? "viewable  "  :
	     "<unknown> ")       );

    if (!XQueryTree(theDisp, win, &root, &parent, &children, &nchildren)) {
      errSpace(tab);
      fprintf(stderr,"pWT: XQueryTree(%08x) failed\n", (u_int) win);
      if (children) XFree((char *)children);
      return;
    }

    for (i=0; i<nchildren; i++) printWinTree(children[i], tab+1);
    if (children) XFree((char *)children);
  }

  return;
}


/***********************************/
static void errSpace(n)
     int n;
{
  for ( ; n>0; n--) putc(' ', stderr);
}




/***********************************/
static int grabRootRegion(x, y, w, h)
     int    x, y, w, h;
{
  /* attempts to grab the specified rectangle of the root window
     returns '1' on success */

  XWindowAttributes  xwa;
  int                i;

  regrabList = (struct rectlist *) NULL;

  /* range checking */
  if (x<0) { w += x;  x = 0; }
  if (y<0) { h += y;  y = 0; }
  if (x+w>dispWIDE) w = dispWIDE-x;
  if (y+h>dispHIGH) h = dispHIGH-y;

  if (w<=0 || h<=0) {  /* selected nothing */
    ungrabX();
    return 0;
  }


  /* grab this region, using the default (root's) visual */

  /* now for all top-level windows (children of root), in bottom->top order
     if they intersect the grabregion
       are they drawn entirely (including children) using default visual+cmap?
       yes: if they intersect 'regrab' list, grab'em - else skip'em
       no:  grab them, add their rectangle to 'regrab' list
   */


  /* make a 24bit grabPic */
  gptype = PIC24;
  gXOFF = x;  gYOFF = y;  gWIDE = w;  gHIGH = h;
  grabPic = (byte *) malloc((size_t) gWIDE * gHIGH * 3);
  if (!grabPic) {
    ungrabX();
    ErrPopUp("Unable to malloc() space for grabbed image!", "\nBite Me!");
    return 0;
  }

  if (!XGetWindowAttributes(theDisp, rootW, &xwa)) {
    ungrabX();
    ErrPopUp("Can't get window attributes for root window!", "\nBite Me!");
    return 0;
  }

  i = grabWinImage(rootW, XVisualIDFromVisual(xwa.visual), xwa.colormap,0);

  ungrabX();

  XBell(theDisp, 0);    /* beep twice at end of grab */
  XBell(theDisp, 0);

  { /* free regrabList */
    struct rectlist *rr, *tmprr;
    rr = regrabList;
    while (rr) {
      tmprr = rr->next;
      free((char *) rr);
      rr = tmprr;
    }
    regrabList = (struct rectlist *) NULL;
  }

  if (i) {
    ErrPopUp("Warning: Problems occurred during grab.","\nWYSInWYG!");
    return 0;
  }


  /* if 256 or fewer colors in grabPic, make it a PIC8 */
  i = CountColors24(grabPic, gWIDE, gHIGH, 0,0,gWIDE,gHIGH);
  if (i<=256) {
    byte *pic8;
    pic8 = (byte *) malloc((size_t) (gWIDE * gHIGH));
    if (pic8) {
      if (Trivial24to8(grabPic, gWIDE,gHIGH, pic8,
		       grabmapR,grabmapG,grabmapB,256)) {
	free((char *) grabPic);
	grabPic = pic8;
	gptype = PIC8;
      }
    }
  }

  return 1;  /* full success */
}


/***********************************/
static int grabWinImage(win, parentVid, parentCmap, toplevel)
     Window win;
     VisualID           parentVid;
     Colormap           parentCmap;
     int                toplevel;
{
  /* grabs area of window (and its children) that intersects
   * grab region (root coords: gXOFF,gYOFF,gWIDE,gHIGH), and stuffs
   * relevant bits into the grabPic (a gWIDE*gHIGH PIC24)
   *
   * Note: special kludge for toplevel windows (children of root):
   * since that's the only case where a window can be obscuring something
   * that isn't its parent
   *
   * returns 0 if okay, 1 if problems occurred
   */


  int               i, rv, dograb;
  int               wx, wy, ww, wh;      /* root coords of window */
  int               gx, gy, gw, gh;      /* root coords of grab region of win*/
  Window            chwin;               /* unused */
  u_int             nchildren;
  Window            root, parent, *children;
  XWindowAttributes xwa;

  /* first, quick checks to avoid recursing down useless branches */

  if (!XGetWindowAttributes(theDisp, win, &xwa)) {
    if (DEBUG) fprintf(stderr,"gWI: can't get win attr (%08x)\n", (u_int) win);
    return 1;
  }

  if (xwa.class == InputOnly || xwa.map_state != IsViewable) return 0;

  rv     = 0;
  dograb = 1;
  wx = 0;  wy = 0;  ww = (int) xwa.width;  wh = (int) xwa.height;

  /* if this window doesn't intersect, none of its children will, either */
  XTranslateCoordinates(theDisp, win, rootW, 0,0, &wx, &wy, &chwin);
  if (!RectIntersect(wx,wy,ww,wh, gXOFF,gYOFF,gWIDE,gHIGH)) return 0;

  gx = wx;  gy = wy;  gw = ww;  gh = wh;
  CropRect2Rect(&gx,&gy,&gw,&gh, gXOFF,gYOFF,gWIDE,gHIGH);

  if (win==rootW) {
    /* always grab */
  }

  else if (XVisualIDFromVisual(xwa.visual) == parentVid &&
	   ((xwa.visual->class==TrueColor) || xwa.colormap == parentCmap)) {

    /* note: if both visuals are TrueColor, don't compare cmaps */

    /* normally, if the vis/cmap info of a window is the same as its parent,
       no need to regrab window.  special case if this is a toplevel
       window, as it can be obscuring windows that *aren't* its parent */

    if (toplevel) {
      /* we probably already have this region.  Check it against regrabList
	 If it intersects none, no need to grab.
	 If it intersects one,  crop to that rectangle and grab
	 if it intersects >1,   don't crop, just grab gx,gy,gw,gh */

      struct rectlist *rr, *cr;

      i=0; cr=rr=regrabList;
      while (rr) {
	if (RectIntersect(gx,gy,gw,gh, rr->x,rr->y,rr->w,rr->h)) {
	  i++;  cr = rr;
	}
	rr = rr->next;
      }

      if (i==0) dograb=0;   /* no need to grab */

      if (i==1) CropRect2Rect(&gx,&gy,&gw,&gh, cr->x,cr->y,cr->w,cr->h);
    }
    else dograb = 0;
  }

  else {
    /* different vis/cmap from parent:
       add to regrab list, if not already fully contained in list */
    struct rectlist *rr;

    /* check to see if fully contained... */
    rr=regrabList;
    while (rr && RectIntersect(gx,gy,gw,gh, rr->x,rr->y,rr->w,rr->h)!=2)
      rr = rr->next;

    if (!rr) {   /* add to list */
      if (DEBUG)
	fprintf(stderr,"added to regrabList: %d,%d %dx%d\n",gx,gy,gw,gh);

      rr = (struct rectlist *) malloc(sizeof(struct rectlist));
      if (!rr) return 1;
      else {
	rr->x = gx;  rr->y = gy;  rr->w = gw;  rr->h = gh;
	rr->next = regrabList;
	regrabList = rr;
      }
    }
  }

  /* at this point, we have to grab gx,gy,gw,gh from 'win' */

  if (dograb) {
    int     ix, iy, ncolors;
    XColor *colors;
    XImage *image;

    XTranslateCoordinates(theDisp, rootW, win, gx, gy, &ix, &iy, &chwin);

    if (DEBUG)
      fprintf(stderr,"Grabbing win (%08x) %d,%d %dx%d\n",
	      (u_int) win, gx,gy,gw,gh);

    WaitCursor();

    xerrcode = 0;
    image = XGetImage(theDisp, win, ix, iy, (u_int) gw, (u_int) gh,
		      AllPlanes, ZPixmap);
    if (xerrcode || !image || !image->data) return 1;

    ncolors = getxcolors(&xwa, &colors);
    rv = convertImageAndStuff(image, colors, ncolors, &xwa,
			      gx - gXOFF, gy - gYOFF, gw, gh);
    XDestroyImage(image);   /* can't use xvDestroyImage: alloc'd by X! */
    if (colors) free((char *) colors);
  }


  /* recurse into children to see if any of them are 'different'... */

  if (!XQueryTree(theDisp, win, &root, &parent, &children, &nchildren)) {
    if (DEBUG) fprintf(stderr,"XQueryTree(%08x) failed\n", (u_int) win);
    if (children) XFree((char *)children);
    return rv+1;
  }

  for (i=0; i<nchildren; i++) {
    rv += grabWinImage(children[i], XVisualIDFromVisual(xwa.visual),
		       xwa.colormap, (win==rootW));
  }
  if (children) XFree((char *)children);

  return rv;
}



/**************************************/
static int convertImageAndStuff(image, colors, ncolors, xwap, gx,gy,gw,gh)
     XImage *image;
     XColor *colors;
     int     ncolors;
     XWindowAttributes *xwap;
     int     gx,gy,gw,gh;      /* position within grabPic (guaranteed OK) */
{
  /* attempts to convert the image from whatever weird-ass format it might
     be in into a 24-bit RGB image, and stuff it into grabPic
     Returns 0 on success, 1 on failure */

  /* this code owes a lot to 'xwdtopnm.c', part of the pbmplus package,
     written by Jef Poskanzer */

  int             i, j;
  CARD8          *bptr, tmpbyte;
  CARD16         *sptr, sval;
  CARD32         *lptr, lval;
  CARD8          *pptr, *lineptr;
  int            bits_used, bits_per_item, bit_shift, bit_order;
  int            bits_per_pixel, byte_order;
  CARD32         pixvalue, pixmask, rmask, gmask, bmask;
  int            rshift, gshift, bshift, r8shift, g8shift, b8shift;
  CARD32         rval, gval, bval;
  union swapun   sw;
  int            isLsbMachine, flipBytes;
  Visual         *visual;
  char            errstr[256];


  /* quiet compiler warnings */
  sval = 0;
  lval = 0;
  bit_shift = 0;
  pixvalue  = 0;
  rmask  = gmask  = bmask = 0;
  rshift = gshift = bshift = 0;
  r8shift = g8shift = b8shift = 0;

  /* determine byte order of the machine we're running on */
  sw.l = 1;
  isLsbMachine = (sw.b[0]) ? 1 : 0;

  visual = xwap->visual;


  if (DEBUG>1) {
    fprintf(stderr,"convertImage:\n");
    fprintf(stderr,"  %dx%d (offset %d), %s\n",
	    image->width, image->height, image->xoffset,
	    (image->format == XYBitmap || image->format == XYPixmap)
	    ? "XYPixmap" : "ZPixmap");

    fprintf(stderr,"byte_order = %s, bitmap_bit_order = %s, unit=%d, pad=%d\n",
	    (image->byte_order == LSBFirst) ? "LSBFirst" : "MSBFirst",
	    (image->bitmap_bit_order == LSBFirst) ? "LSBFirst" : "MSBFirst",
	    image->bitmap_unit, image->bitmap_pad);

    fprintf(stderr,"depth = %d, bperline = %d, bits_per_pixel = %d\n",
	    image->depth, image->bytes_per_line, image->bits_per_pixel);

    fprintf(stderr,"masks:  red %lx  green %lx  blue %lx\n",
	    image->red_mask, image->green_mask, image->blue_mask);

    if (isLsbMachine) fprintf(stderr,"This looks like an lsbfirst machine\n");
                 else fprintf(stderr,"This looks like an msbfirst machine\n");
  }


  if (image->bitmap_unit != 8 && image->bitmap_unit != 16 &&
      image->bitmap_unit != 32) {
    sprintf(errstr, "%s\nReturned image bitmap_unit (%d) non-standard.",
	    "Can't deal with this display.", image->bitmap_unit);
    ErrPopUp(errstr, "\nThat Sucks!");
    return 1;
  }

  if (!ncolors && visual->class != TrueColor) {
    sprintf(errstr, "%s\nOnly TrueColor displays can have no colormap.",
	    "Can't deal with this display.");
    ErrPopUp(errstr, "\nThat Sucks!");
    return 1;
  }


  if (visual->class == TrueColor || visual->class == DirectColor) {
    unsigned int tmp;

    /* compute various shifty constants we'll need */
    rmask = image->red_mask;
    gmask = image->green_mask;
    bmask = image->blue_mask;

    rshift = lowbitnum((unsigned long) rmask);
    gshift = lowbitnum((unsigned long) gmask);
    bshift = lowbitnum((unsigned long) bmask);

    r8shift = 0;  tmp = rmask >> rshift;
    while (tmp >= 256) { tmp >>= 1;  r8shift -= 1; }
    while (tmp < 128)  { tmp <<= 1;  r8shift += 1; }

    g8shift = 0;  tmp = gmask >> gshift;
    while (tmp >= 256) { tmp >>= 1;  g8shift -= 1; }
    while (tmp < 128)  { tmp <<= 1;  g8shift += 1; }

    b8shift = 0;  tmp = bmask >> bshift;
    while (tmp >= 256) { tmp >>= 1;  b8shift -= 1; }
    while (tmp < 128)  { tmp <<= 1;  b8shift += 1; }

    if (DEBUG>1)
      fprintf(stderr,"True/DirectColor: shifts=%d,%d,%d  8shifts=%d,%d,%d\n",
	      rshift, gshift, bshift, r8shift, g8shift, b8shift);
  }


  bits_per_item  = image->bitmap_unit;
  bits_per_pixel = image->bits_per_pixel;


  /* add code for freako 'exceed' server, where bitmapunit = 8
     and bitsperpix = 32 (and depth=24)... */

  if (bits_per_item < bits_per_pixel) {
    bits_per_item = bits_per_pixel;

    /* round bits_per_item up to next legal value, if necc */
    if      (bits_per_item <  8) bits_per_item = 8;
    else if (bits_per_item < 16) bits_per_item = 16;
    else                         bits_per_item = 32;
  }


  /* which raises the question:  how (can?) you ever have a 24 bits per pix,
     (ie, 3 bytes, no alpha/padding) */


  bits_used = bits_per_item;  /* so it will get a new item first time */

  if (bits_per_pixel == 32) pixmask = 0xffffffff;
  else pixmask = (((CARD32) 1) << bits_per_pixel) - 1;

  bit_order = image->bitmap_bit_order;
  byte_order = image->byte_order;

  /* if we're on an lsbfirst machine, or the image came from an lsbfirst
     machine, we should flip the bytes around.  NOTE:  if we're on an
     lsbfirst machine *and* the image came from an lsbfirst machine,
     *don't* flip bytes, as it should work out */

  flipBytes = ( isLsbMachine && byte_order != LSBFirst) ||
              (!isLsbMachine && byte_order == LSBFirst);

  for (i=0; i<image->height; i++) {
    pptr = grabPic + ((i+gy) * gWIDE + gx) * 3;

    lineptr = (byte *) image->data + (i * image->bytes_per_line);
    bptr = ((CARD8  *) lineptr) - 1;
    sptr = ((CARD16 *) lineptr) - 1;
    lptr = ((CARD32 *) lineptr) - 1;
    bits_used = bits_per_item;

    for (j=0; j<image->width; j++) {
      /* get the next pixel value from the image data */

      if (bits_used == bits_per_item) {  /* time to move on to next b/s/l */
	switch (bits_per_item) {
	case 8:
	  bptr++;  break;

	case 16:
	  sptr++;  sval = *sptr;
	  if (flipBytes) {   /* swap CARD16 */
	    sw.s = sval;
	    tmpbyte = sw.b[0];
	    sw.b[0] = sw.b[1];
	    sw.b[1] = tmpbyte;
	    sval = sw.s;
	  }
	  break;

	case 32:
	  lptr++;  lval = *lptr;
	  if (flipBytes) {   /* swap CARD32 */
	    sw.l = lval;
	    tmpbyte = sw.b[0];
	    sw.b[0] = sw.b[3];
	    sw.b[3] = tmpbyte;
	    tmpbyte = sw.b[1];
	    sw.b[1] = sw.b[2];
	    sw.b[2] = tmpbyte;
	    lval = sw.l;
	  }
	  break;
	}

	bits_used = 0;
	if (bit_order == MSBFirst) bit_shift = bits_per_item - bits_per_pixel;
	                      else bit_shift = 0;
      }

      switch (bits_per_item) {
      case 8:  pixvalue = (*bptr >> bit_shift) & pixmask;  break;
      case 16: pixvalue = ( sval >> bit_shift) & pixmask;  break;
      case 32: pixvalue = ( lval >> bit_shift) & pixmask;  break;
      }

      if (bit_order == MSBFirst) bit_shift -= bits_per_pixel;
                            else bit_shift += bits_per_pixel;
      bits_used += bits_per_pixel;


      /* okay, we've got the next pixel value in 'pixvalue' */

      if (visual->class == TrueColor || visual->class == DirectColor) {
	/* in either case, we have to take the pixvalue and
	   break it out into individual r,g,b components */
	rval = (pixvalue & rmask) >> rshift;
	gval = (pixvalue & gmask) >> gshift;
	bval = (pixvalue & bmask) >> bshift;

	if (visual->class == DirectColor) {
	  /* use rval, gval, bval as indicies into colors array */

	  *pptr++ = (rval < ncolors) ? (colors[rval].red   >> 8) : 0;
	  *pptr++ = (gval < ncolors) ? (colors[gval].green >> 8) : 0;
	  *pptr++ = (bval < ncolors) ? (colors[bval].blue  >> 8) : 0;
	}

	else {   /* TrueColor */
	  /* have to map rval,gval,bval into 0-255 range */
	  *pptr++ = (r8shift >= 0) ? (rval << r8shift) : (rval >> (-r8shift));
	  *pptr++ = (g8shift >= 0) ? (gval << g8shift) : (gval >> (-g8shift));
	  *pptr++ = (b8shift >= 0) ? (bval << b8shift) : (bval >> (-b8shift));
	}
      }

      else { /* all others: StaticGray, StaticColor, GrayScale, PseudoColor */
	/* use pixel value as an index into colors array */

	if (pixvalue >= ncolors) {
	  fprintf(stderr, "WARNING: convertImage(): pixvalue >= ncolors\n");
	  return 1;
	}

	*pptr++ = (colors[pixvalue].red)   >> 8;
	*pptr++ = (colors[pixvalue].green) >> 8;
	*pptr++ = (colors[pixvalue].blue)  >> 8;
      }
    }
  }

  return 0;
}



/***********************************/
static int RectIntersect(ax,ay,aw,ah, bx,by,bw,bh)
     int ax,ay,aw,ah, bx,by,bw,bh;
{
  /* returns 0 if rectangles A and B do not intersect
     returns 1 if A partially intersects B
     returns 2 if rectangle A is fully enclosed by B */

  int ax1,ay1, bx1,by1;

  ax1 = ax+aw-1;  ay1 = ay+ah-1;
  bx1 = bx+bw-1;  by1 = by+bh-1;

  if (ax1<bx || ax>bx1 || ay1<by || ay>by1) return 0;

  if (ax>=bx && ax1<=bx1 && ay>=by && ay1<=by) return 2;

  return 1;
}





/** stuff needed to make new xvgrab work in 3.10a. **/

/********************************************/
static int CountColors24(pic, pwide, phigh, x, y, w, h)
     byte *pic;
     int   pwide, phigh, x,y,w,h;
{
  /* counts the # of unique colors in a selected rect of a PIC24
     returns '0-256' or >256 */

  int    i, j, nc;
  int    low, high, mid;
  u_int  colors[257], col;
  byte   *pp;

  nc = 0;

  for (i=y; nc<257 && i<y+h; i++) {
    pp = pic + (i*pwide + x)*3;

    for (j=x; nc<257 && j<x+w; j++, pp+=3) {
      col = (((u_int) pp[0])<<16) + (((u_int) pp[1])<<8) + pp[2];

      /* binary search the 'colors' array to see if it's in there */
      low = 0;  high = nc-1;
      while (low <= high) {
	mid = (low+high)/2;
	if      (col < colors[mid]) high = mid - 1;
	else if (col > colors[mid]) low  = mid + 1;
	else break;
      }

      if (high < low) { /* didn't find color in list, add it. */
	xvbcopy((char *) &colors[low], (char *) &colors[low+1],
		(nc - low) * sizeof(u_int));
	colors[low] = col;
	nc++;
      }
    }
  }

  return nc;
}


/****************************/
static int Trivial24to8(pic24, w,h, pic8, rmap,gmap,bmap, maxcol)
     byte *pic24, *pic8, *rmap, *gmap, *bmap;
     int   w,h,maxcol;
{
  /* scans picture until it finds more than 'maxcol' different colors.  If it
     finds more than 'maxcol' colors, it returns '0'.  If it DOESN'T, it does
     the 24-to-8 conversion by simply sticking the colors it found into
     a colormap, and changing instances of a color in pic24 into colormap
     indicies (in pic8) */

  unsigned long colors[256],col;
  int           i, nc, low, high, mid;
  byte         *p, *pix;

  if (maxcol>256) maxcol = 256;

  /* put the first color in the table by hand */
  nc = 0;  mid = 0;

  for (i=w*h,p=pic24; i; i--) {
    col  = (((u_long) *p++) << 16);
    col += (((u_long) *p++) << 8);
    col +=  *p++;

    /* binary search the 'colors' array to see if it's in there */
    low = 0;  high = nc-1;
    while (low <= high) {
      mid = (low+high)/2;
      if      (col < colors[mid]) high = mid - 1;
      else if (col > colors[mid]) low  = mid + 1;
      else break;
    }

    if (high < low) { /* didn't find color in list, add it. */
      if (nc>=maxcol) return 0;
      xvbcopy((char *) &colors[low], (char *) &colors[low+1],
	      (nc - low) * sizeof(u_long));
      colors[low] = col;
      nc++;
    }
  }


  /* run through the data a second time, this time mapping pixel values in
     pic24 into colormap offsets into 'colors' */

  for (i=w*h,p=pic24, pix=pic8; i; i--,pix++) {
    col  = (((u_long) *p++) << 16);
    col += (((u_long) *p++) << 8);
    col +=  *p++;

    /* binary search the 'colors' array.  It *IS* in there */
    low = 0;  high = nc-1;
    while (low <= high) {
      mid = (low+high)/2;
      if      (col < colors[mid]) high = mid - 1;
      else if (col > colors[mid]) low  = mid + 1;
      else break;
    }

    if (high < low) {
      fprintf(stderr,"Trivial24to8:  impossible situation!\n");
      exit(1);
    }
    *pix = mid;
  }

  /* and load up the 'desired colormap' */
  for (i=0; i<nc; i++) {
    rmap[i] =  colors[i]>>16;
    gmap[i] = (colors[i]>>8) & 0xff;
    bmap[i] =  colors[i]     & 0xff;
  }

  return 1;
}
