/*
 * xvevent.c - EventLoop and support routines for XV
 *
 *  Author:    John Bradley, University of Pennsylvania
 *                (bradley@cis.upenn.edu)
 *
 *  Contains:
 *            int  EventLoop()
 *            void DrawWindow(x,y,w,h)
 *            void WResize(w,h)
 *            void WRotate()
 *            void WCrop(w,h,dx,dy)
 *            void WUnCrop()
 *            void GetWindowPos(&xwa)
 *            void SetWindowPos(&xwa)
 *            void SetEpicMode()
 *            int  xvErrorHandler(disp, err)
 */

#include "copyright.h"

#define NEEDSTIME    /* for -wait handling in eventloop */

#include "xv.h"

#include "bits/dropper"
#include "bits/dropperm"
#include "bits/pen"
#include "bits/penm"
#include "bits/blur"
#include "bits/blurm"

static int   rotatesLeft = 0;
static int   origcropx, origcropy, origcropvalid=0;
static int   canstartwait;
static int   frominterrupt = 0;
static char *xevPriSel = NULL;
static Time  lastEventTime;
static Cursor dropper = 0, pen = 0, blur = 0;


static void SelectDispMB       PARM((int));
static void Select24to8MB      PARM((int));
static void SelectRootMB       PARM((int));
static void SelectWindowMB     PARM((int));
static void SelectSizeMB       PARM((int));

static void DoPrint            PARM((void));
static void debugEvent         PARM((XEvent *));
static const char *win2name    PARM((Window));
static void handleButtonEvent  PARM((XEvent *, int *, int *));
static void handleKeyEvent     PARM((XEvent *, int *, int *));
static void zoomCurs           PARM((u_int));
static void textViewCmd        PARM((void));
static void setSizeCmd         PARM((void));
static void WMaximize          PARM((void));
static void CropKey            PARM((int, int, int, int));
static void TrackPicValues     PARM((int, int));
static int  CheckForConfig     PARM((void));
static Bool IsConfig           PARM((Display *, XEvent *, char *));
static void onInterrupt        PARM((int));

static void   Paint            PARM((void));
static void   paintPixel       PARM((int, int));
static void   paintLine        PARM((int, int, int, int));
static void   paintXLine       PARM((int, int, int, int, int));
static void   BlurPaint        PARM((void));
static int    highbit          PARM((u_long));
static u_long RGBToXColor      PARM((int, int, int));
static void   blurPixel        PARM((int, int));

static void   annotatePic      PARM((void));

static int    debkludge_offx;
static int    debkludge_offy;

/****************/
int EventLoop()
/****************/
{
  XEvent event;
  int    retval,done,waiting;
#ifdef USE_TICKS
  clock_t waitsec_ticks=0L, orgtime_ticks=0L, curtime_ticks;
  clock_t elapsed_ticks=0L, remaining_interval;
#else
  time_t orgtime=0L, curtime;
#endif


#ifndef NOSIGNAL
  signal(SIGQUIT, onInterrupt);
#endif

  if (startGrab == 1) {
     startGrab = 2;
     FakeButtonPress(&but[BGRAB]);
     FakeKeyPress(ctrlW, XK_Return);
     return(1);
  }

  /* note: there's no special event handling if we're using the root window.
     if we're using the root window, we will recieve NO events for mainW */

  /* note: 'canstartwait' is magically turned 'true' in HandleEvent when I
     think I've finally gotten 'mainW' drawn.  It does not necessarily
     mean that any waiting is/will be done.  Also note that if we're
     using a root mode, canstartwait is instantly turned on, as we aren't
     going to be getting Expose/Configure events on the root window */

  done = retval = waiting = canstartwait = 0;

  if (useroot) canstartwait = 1;
  else if (mainW) {           /* if mainW iconified, start wait now */
    XWindowAttributes xwa;
    XSync(theDisp, False);
    if (XGetWindowAttributes(theDisp, mainW, &xwa)) {
      if (xwa.map_state != IsViewable) canstartwait = 1;
    }
  }

  while (!done) {

    if (waitsec >= 0.0 && canstartwait && !waiting && XPending(theDisp)==0) {
      /* we wanna wait, we can wait, we haven't started waiting yet, and
	 all pending events (ie, drawing the image the first time)
	 have been dealt with:  START WAITING */
#ifdef USE_TICKS
      waitsec_ticks = (clock_t)(waitsec * CLK_TCK);
      orgtime_ticks = times(NULL);  /* unclear if NULL valid, but OK on Linux */
#else
      orgtime = time(NULL);
#endif
      waiting = 1;
    }


    /* if there's an XEvent pending *or* we're not doing anything
       in real-time (polling, flashing the selection, etc.) get next event */
    if ((waitsec<0.0 && !polling && !HaveSelection()) || XPending(theDisp)>0)
    {
      XNextEvent(theDisp, &event);
      retval = HandleEvent(&event,&done);
    }

    else {                      /* no events.  check wait status */
      if (HaveSelection()) {
	DrawSelection(0);
	DrawSelection(1);
	XFlush(theDisp);
	Timer(200);             /* milliseconds */
      }

      if (polling) {
	if (CheckPoll(2)) return POLLED;
	else if (!XPending(theDisp)) sleep(1);
      }

      if (waitsec>=0.0 && waiting) {
#ifdef USE_TICKS
        curtime_ticks = times(NULL);   /* value in ticks */
        if (curtime_ticks < orgtime_ticks) {
          /* clock ticks rolled over:  need to correct for that (i.e.,
           *  curtime_ticks is presumably quite small, while orgtime_ticks
           *  should be close to LONG_MAX, so do math accordingly--any way
           *  to check whether clock_t is *not* a signed long?) */
          elapsed_ticks = curtime_ticks + (LONG_MAX - orgtime_ticks);
        } else
          elapsed_ticks = curtime_ticks - orgtime_ticks;
        remaining_interval = waitsec_ticks - elapsed_ticks;
        if (remaining_interval >= (clock_t)(1 * CLK_TCK))
          sleep(1);
        else {
          /* less than one second remaining:  do delay in msec, then return */
          Timer((remaining_interval * 1000L) / CLK_TCK);  /* can't overflow */
          return waitloop? NEXTLOOP : NEXTQUIT;
        }
#else
        curtime = time(NULL);          /* value in seconds */
	if (curtime - orgtime < (time_t)waitsec)
          sleep(1);
	else
          return waitloop? NEXTLOOP : NEXTQUIT;
#endif
      }
    }
  }  /* while (!done) */

  if (!useroot && origcropvalid) WUnCrop();
  origcropvalid = 0;

  return(retval);
}



/****************/
int HandleEvent(event, donep)
     XEvent *event;
     int    *donep;
{
  static int wasInfoUp=0, wasCtrlUp=0, wasDirUp=0, wasGamUp=0, wasPsUp=0;
#ifdef HAVE_JPEG
  static int wasJpegUp=0;
#endif
#ifdef HAVE_JP2K
  static int wasJp2kUp=0;
#endif
#ifdef HAVE_TIFF
  static int wasTiffUp=0;
#endif
#ifdef HAVE_PNG
  static int wasPngUp=0;
#endif
#ifdef HAVE_PCD
  static int wasPcdUp=0;
#endif
#ifdef HAVE_PIC2
  static int wasPic2Up=0;
#endif
#ifdef HAVE_MGCSFX
  static int wasMgcSfxUp=0;
#endif

  static int mainWKludge=0;  /* force first mainW expose after a mainW config
				to redraw all of mainW */

  int done=0, retval=0;


  if (DEBUG) debugEvent((XEvent *) event);


  switch (event->type) {

  case ButtonPress:
    lastEventTime = ((XButtonEvent *) event)->time;
    handleButtonEvent(event, &done, &retval);
    break;


  case KeyRelease:
  case KeyPress:
    lastEventTime = ((XKeyEvent *) event)->time;
    handleKeyEvent(event, &done, &retval);
    break;


  case Expose: {
    XExposeEvent *exp_event = (XExposeEvent *) event;
    int x,y,w,h;
    Window win;

#ifdef VMS
    static int borders_sized = 0;

    if (!borders_sized  && !useroot && exp_event->window == mainW) {
      /*
       * Initial expose of main window, find the size of the ancestor
       * window just prior to the root window and adjust saved size
       * of display so that maximize functions will allow for window
       * decorations.
       */
      int status, count, mwid, mhgt, x, y, w, h, b, d, mbrd;
      Window root, parent, *children, crw = exp_event->window;
      borders_sized = 1;
      status = XGetGeometry(theDisp, crw,
			    &root, &x, &y, &mwid, &mhgt, &mbrd, &d);

      for ( parent = crw, w=mwid, h=mhgt;
	   status && (parent != root) && (parent != vrootW); ) {
	crw = parent;
	status = XQueryTree ( theDisp, crw, &root, &parent,
			     &children, &count );
	if ( children != NULL ) XFree ( children );
      }
      status = XGetGeometry(theDisp, crw, &root, &x, &y, &w, &h, &b, &d);
      if ( status ) {
	dispWIDE = dispWIDE + mwid - w + (2*b);
	dispHIGH = dispHIGH + mhgt - h + b;
	/*printf("New display dims: %d %d\n", dispWIDE, dispHIGH ); */
      }
    }
#endif


    win = exp_event->window;
    x = exp_event->x;      y = exp_event->y;
    w = exp_event->width;  h = exp_event->height;

    if (PUCheckEvent  (event)) break;   /* event has been processed */
    if (PSCheckEvent  (event)) break;   /* event has been processed */

#ifdef HAVE_JPEG
    if (JPEGCheckEvent(event)) break;   /* event has been processed */
#endif

#ifdef HAVE_JP2K
    if (JP2KCheckEvent(event)) break;   /* event has been processed */
#endif

#ifdef HAVE_TIFF
    if (TIFFCheckEvent(event)) break;   /* event has been processed */
#endif

#ifdef HAVE_PNG
    if (PNGCheckEvent (event)) break;   /* event has been processed */
#endif

    if (PCDCheckEvent(event)) break;    /* event has been processed */

#ifdef HAVE_PIC2
    if (PIC2CheckEvent(event)) break;   /* event has been processed */
#endif

#ifdef HAVE_PCD
    if (PCDCheckEvent (event)) break;   /* event has been processed */
#endif

#ifdef HAVE_MGCSFX
    if (MGCSFXCheckEvent(event)) break; /* event has been processed */
#endif

#ifdef TV_MULTILINGUAL
    if (CharsetCheckEvent(event)) break; /* event has been processed */
#endif

    if (GamCheckEvent (event)) break;   /* event has been processed */
    if (BrowseCheckEvent (event, &retval, &done)) break;   /* event eaten */
    if (TextCheckEvent   (event, &retval, &done)) break;   /* event eaten */

    /* if the window doesn't do intelligent redraw, drop but last expose */
    if (exp_event->count>0 &&
	win != mainW && win != ctrlW &&	win != dirW && win != infoW) break;



    if (win == mainW && CheckForConfig()) {
      if (DEBUG) fprintf(stderr,"Expose mainW ignored.  Config pending.\n");
      break;
    }




    if (win==mainW || win==ctrlW || win==dirW || win==infoW) {
      /* must be a 'smart' window.  group exposes into an expose 'region' */
      int           count;
      Region        reg;
      XRectangle    rect;
      XEvent        evt;

      xvbcopy((char *) exp_event, (char *) &evt, sizeof(XEvent));
      reg = XCreateRegion();
      count = 0;

      do {
	rect.x      = evt.xexpose.x;
	rect.y      = evt.xexpose.y;
	rect.width  = evt.xexpose.width;
	rect.height = evt.xexpose.height;
	XUnionRectWithRegion(&rect, reg, reg);
	count++;

	if (DEBUG) debugEvent((XEvent *) &evt);

      } while (XCheckWindowEvent(theDisp,evt.xexpose.window,
				 ExposureMask, &evt));

      XClipBox(reg, &rect);  /* bounding box of region */
      XSetRegion(theDisp, theGC, reg);

      x = rect.x;      y = rect.y;
      w = rect.width;  h = rect.height;

      if (DEBUG) fprintf(stderr,"window: 0x%08x  collapsed %d expose events\n",
			 (u_int) exp_event->window, count);
      if (DEBUG) fprintf(stderr,"        region bounding box: %d,%d %dx%d\n",
			 x, y, w, h);


      if (win == mainW) {
	if (DEBUG) fprintf(stderr,"EXPOSE:  ");
	if (!CheckForConfig()) {

	  if (mainWKludge) {
	    if (DEBUG) fprintf(stderr, "Using mainWKludge\n");
	    x = 0; y = 0;  w = eWIDE;  h = eHIGH;
	    XSetClipMask(theDisp, theGC, None);
	    mainWKludge = 0;
	  }

	  if (DEBUG) fprintf(stderr,"No configs pending.\n");
	  /* if (DEBUG) XClearArea(theDisp, mainW, x,y,w,h, False); */
	  DrawWindow(x,y,w,h);

	  if (HaveSelection()) DrawSelection(0);

	  canstartwait = 1;  /* finished drawing */
	  XSync(theDisp, False);
	}
	else
	  if (DEBUG) fprintf(stderr,"Ignored.  Config pending\n");
      }

      else if (win == infoW)          RedrawInfo(x,y,w,h);
      else if (win == ctrlW)          RedrawCtrl(x,y,w,h);
      else if (win == dirW)           RedrawDirW(x,y,w,h);

      XSetClipMask(theDisp, theGC, None);
      XDestroyRegion(reg);
    }

    else if (win == nList.win)      LSRedraw(&nList,0);
    else if (win == nList.scrl.win) SCRedraw(&nList.scrl);
    else if (win == dList.win)      LSRedraw(&dList,0);
    else if (win == dList.scrl.win) SCRedraw(&dList.scrl);
    else if (win == dnamW)          RedrawDNamW();
  }
    break;



  case ClientMessage: {
    Atom proto, delwin;
    XClientMessageEvent *client_event = (XClientMessageEvent *) event;

    if (PUCheckEvent (event)) break;   /* event has been processed */

    proto = XInternAtom(theDisp, "WM_PROTOCOLS", FALSE);
    delwin = XInternAtom(theDisp, "WM_DELETE_WINDOW", FALSE);

    if (client_event->message_type == proto &&
	client_event->data.l[0]    == delwin) {
      /* it's a WM_DELETE_WINDOW event */

      if (BrowseDelWin(client_event->window)) break;
      if (TextDelWin(client_event->window)) break;
#ifdef TV_MULTILINGUAL
      if (CharsetDelWin(client_event->window)) break;
#endif

      if      (client_event->window == infoW) InfoBox(0);
      else if (client_event->window == gamW)  GamBox(0);
      else if (client_event->window == ctrlW) CtrlBox(0);
      else if (client_event->window == dirW)  DirBox(0);
      else if (client_event->window == psW)   PSDialog(0);

#ifdef HAVE_JPEG
      else if (client_event->window == jpegW) JPEGDialog(0);
#endif

#ifdef HAVE_JP2K
      else if (client_event->window == jp2kW) JP2KDialog(0);
#endif

#ifdef HAVE_TIFF
      else if (client_event->window == tiffW) TIFFDialog(0);
#endif

#ifdef HAVE_PNG
      else if (client_event->window == pngW)  PNGDialog(0);
#endif

      else if (client_event->window == pcdW)  PCDDialog(0);

#ifdef HAVE_PIC2
      else if (client_event->window == pic2W) PIC2Dialog(0);
#endif

#ifdef HAVE_PCD
      else if (client_event->window == pcdW)  PCDDialog(0);
#endif

#ifdef HAVE_MGCSFX
      else if (client_event->window == mgcsfxW) MGCSFXDialog(0);
#endif

      else if (client_event->window == mainW) Quit(0);
    }
  }
    break;


  case ConfigureNotify: {
    XConfigureEvent *cevt = (XConfigureEvent *) event;
    Window           win  = cevt->window;

    if (BrowseCheckEvent (event, &retval, &done)) break;   /* event eaten */
    if (TextCheckEvent   (event, &retval, &done)) break;   /* event eaten */

    /*
     * if it's a configure of one of the 'major' xv windows, update the
     * NORMAL position hints for this window appropriately (so it can be
     * iconified and deiconified correctly)
     */

    if (win==ctrlW || win==gamW || win==infoW || win==mainW || win==dirW) {
      XSizeHints hints;

#define BAD_IDEA
#ifdef BAD_IDEA
      /*
       * if there is a virtual window manager running (e.g., tvtwm / olvwm),
       * we're going to get 'cevt' values in terms of the
       * 'real' root window (the one that is the size of the screen).
       * We'll want to translate them into values that are in terms of
       * the 'virtual' root window (the 'big' one)
       */

      if (vrootW != rootW) {
	int x1,y1;
	Window child;

	XTranslateCoordinates(theDisp, rootW, vrootW, cevt->x, cevt->y,
			      &x1, &y1, &child);
	if (DEBUG) fprintf(stderr,"  CONFIG trans %d,%d root -> %d,%d vroot\n",
			   cevt->x, cevt->y, x1, y1);
	cevt->x = x1;  cevt->y = y1;
      }
#endif

#ifndef VMS
      /* read hints for this window and adjust any position hints, but
         only if this is a 'synthetic' event sent to us by the WM
	 ('real' events from the server have useless x,y info, since the
	 mainW has been reparented by the WM) */

      if (cevt->send_event &&
	  XGetNormalHints(theDisp, cevt->window, &hints)) {

	if (DEBUG) fprintf(stderr,"  CONFIG got hints (0x%x  %d,%d)\n",
			   (u_int) hints.flags, hints.x, hints.y);

	hints.x = cevt->x;
	hints.y = cevt->y;
	XSetNormalHints(theDisp, cevt->window, &hints);

	if (DEBUG) fprintf(stderr,"  CONFIG set hints (0x%x  %d,%d)\n",
		(u_int) hints.flags, hints.x, hints.y);
      }
#endif
    }


    if (cevt->window == mainW) {

      /* when we load a new image, we may get *2* configure events from
       * the 'XMoveResizeWindow()' call.  One at the 'old' size, in the
       * new position, and one at the new size at the new position.
       * We want to *IGNORE* the one at the old size, as we don't want to
       * call Resize() for images that we won't need.
       *
       * Note that we may also only get *one* Configure event (if the window
       * didn't move), and we may also only get *one* Configure event in
       * some unasked for size (if the asked for size wasn't acceptable to
       * the window manager), and we may also get *no* Configure event
       * if the window doesn't move or change size
       *
       * This sucks!
       *
       * So, if we have just loaded an image, and we get a Synthetic conf
       * that is not the desired size (eWIDExeHIGH), ignore it, as it's
       * just the conf generated by moving the old window.  And stop
       * ignoring further config events
       *
       * EVIL KLUDGE:  do *not* ignore configs that are <100x100.  Not
       * ignoring them won't be a big performance problem, and it'll get
       * around the 'I only got one config in the wrong size' problem when
       * initially displaying small images
       */

      if (cevt->width<100 && cevt->height < 100 ) ignoreConfigs = 0;

/* fprintf(stderr,"***mainw, ignore=%d, send_event=%d, evtSize=%d,%d, size=%d,%d\n", ignoreConfigs, cevt->send_event, cevt->width, cevt->height, eWIDE, eHIGH); */

      if (ignoreConfigs==1 && cevt->send_event &&
	  (cevt->width != eWIDE || cevt->height != eHIGH)) {
	ignoreConfigs=0;        /* ignore this one only */
	break;
      }

      ignoreConfigs = 0;


      if (!rotatesLeft) {
	if (CheckForConfig()) {
	  if (DEBUG) fprintf(stderr,"more configs pending.  ignored\n");
	}

	else {
	  XEvent xev;
	  if (DEBUG) fprintf(stderr,"No configs pend.");

	  if (cevt->width == eWIDE && cevt->height == eHIGH) {
	    if (DEBUG) fprintf(stderr,"No redraw\n");
	  }
	  else {
	    if (DEBUG) fprintf(stderr,"Do full redraw\n");

	    Resize(cevt->width, cevt->height);

	    /* eat any pending expose events and do a full redraw */
	    while (XCheckTypedWindowEvent(theDisp, mainW, Expose, &xev)) {
	      XExposeEvent *exp = (XExposeEvent *) &xev;

	      if (DEBUG)
		fprintf(stderr,"  ate expose (%s) (count=%d) %d,%d %dx%d\n",
			exp->send_event ? "synth" : "real", exp->count,
			exp->x, exp->y, exp->width, exp->height);
	    }

	    DrawWindow(0,0,cevt->width, cevt->height);

	    if (HaveSelection()) DrawSelection(0);
            canstartwait=1;
	    XSync(theDisp, False);
	    SetCursors(-1);
	  }
	}
      }

      if (rotatesLeft>0) {
	rotatesLeft--;
	if (!rotatesLeft) mainWKludge = 1;
      }
      if (!rotatesLeft) SetCursors(-1);
    }

  }
    break;



  case CirculateNotify:
  case DestroyNotify:
  case GravityNotify:       break;

  case MapNotify: {
    XMapEvent *map_event = (XMapEvent *) event;

    if (map_event->window == mainW ||
	(map_event->window == ctrlW && dispMode != RMB_WINDOW)) {
      if (DEBUG) fprintf(stderr,"map event received on mainW/ctrlW\n");

      if (autoclose) {
	if (autoclose>1) autoclose -= 2;  /* grab kludge */
	if (wasInfoUp) { InfoBox(wasInfoUp);     wasInfoUp=0; }
	if (wasCtrlUp) { CtrlBox(wasCtrlUp);     wasCtrlUp=0; }
	if (wasDirUp)  { DirBox(wasDirUp);       wasDirUp=0; }
	UnHideBrowseWindows();
	UnHideTextWindows();
	if (wasGamUp)  { GamBox(wasGamUp);       wasGamUp=0; }
	if (wasPsUp)   { PSDialog(wasPsUp);      wasPsUp=0; }
#ifdef HAVE_JPEG
	if (wasJpegUp) { JPEGDialog(wasJpegUp);  wasJpegUp=0; }
#endif
#ifdef HAVE_JP2K
	if (wasJp2kUp) { JP2KDialog(wasJpegUp);  wasJp2kUp=0; }
#endif
#ifdef HAVE_TIFF
	if (wasTiffUp) { TIFFDialog(wasTiffUp);  wasTiffUp=0; }
#endif
#ifdef HAVE_PNG
	if (wasPngUp)  { PNGDialog(wasPngUp);    wasPngUp=0; }
#endif
#ifdef HAVE_PCD
	if (wasPcdUp)  { PCDDialog(wasPcdUp);    wasPcdUp=0; }
#endif
#ifdef HAVE_PIC2
	if (wasPic2Up) { PIC2Dialog(wasPic2Up);  wasPic2Up=0; }
#endif
#ifdef HAVE_MGCSFX
	if (wasMgcSfxUp) { MGCSFXDialog(wasMgcSfxUp);  wasMgcSfxUp=0; }
#endif
      }
    }
  }
    break;


  case UnmapNotify: {
    XUnmapEvent *unmap_event = (XUnmapEvent *) event;

    if (unmap_event->window == mainW ||
	(unmap_event->window == ctrlW && dispMode != RMB_WINDOW)) {
      if (DEBUG) fprintf(stderr,"unmap event received on mainW/ctrlW\n");
      if (DEBUG) fprintf(stderr,"dispMode = %d\n", dispMode);

      /* don't do it if we've just switched to a root mode */
      if ((unmap_event->window == mainW && dispMode == 0) ||
	  (unmap_event->window == ctrlW && dispMode != 0)) {

	if (autoclose) {
	  if (autoclose>1) autoclose -= 2;  /* grab kludge */

	  if (unmap_event->window == mainW) {
	    if (ctrlUp) { wasCtrlUp = ctrlUp;  CtrlBox(0); }
	  }

	  if (infoUp) { wasInfoUp = infoUp;  InfoBox(0); }
	  if (dirUp)  { wasDirUp  = dirUp;   DirBox(0); }
	  HideBrowseWindows();
	  HideTextWindows();
	  if (gamUp)  { wasGamUp  = gamUp;   GamBox(0); }
	  if (psUp)   { wasPsUp   = psUp;    PSDialog(0); }
#ifdef HAVE_JPEG
	  if (jpegUp) { wasJpegUp = jpegUp;  JPEGDialog(0); }
#endif
#ifdef HAVE_JP2K
	  if (jp2kUp) { wasJp2kUp = jp2kUp;  JP2KDialog(0); }
#endif
#ifdef HAVE_TIFF
	  if (tiffUp) { wasTiffUp = tiffUp;  TIFFDialog(0); }
#endif
#ifdef HAVE_PNG
	  if (pngUp)  { wasPngUp  = pngUp;   PNGDialog(0); }
#endif
#ifdef HAVE_PCD
	  if (pcdUp)  { wasPcdUp = pcdUp;    PCDDialog(0); }
#endif
#ifdef HAVE_PIC2
	  if (pic2Up) { wasPic2Up = pic2Up;  PIC2Dialog(0); }
#endif
#ifdef HAVE_MGCSFX
	  if (mgcsfxUp) { wasMgcSfxUp = mgcsfxUp;  MGCSFXDialog(0); }
#endif
	}
      }
    }
  }
    break;

  case ReparentNotify: {
    XReparentEvent *reparent_event = (XReparentEvent *) event;

    if (DEBUG) {
      fprintf(stderr,"Reparent: mainW=%x ->win=%x ->ev=%x  ->parent=%x  ",
	      (u_int) mainW,                 (u_int) reparent_event->window,
	      (u_int) reparent_event->event, (u_int) reparent_event->parent);
      fprintf(stderr,"%d,%d\n", reparent_event->x, reparent_event->y);
    }

    if (reparent_event->window == mainW) {
      ch_offx = reparent_event->x;  /* offset required for ChangeAttr call */
      ch_offy = reparent_event->y;

      p_offx = p_offy = 0;          /* topleft correction for WMs titlebar */

      if (ch_offx == 0 && ch_offy == 0) {
	/* looks like the user is running MWM or OLWM */

	XWindowAttributes xwa;

	/* first query the attributes of mainW.  x,y should be the offset
	   from the parent's topleft corner to the windows topleft.
	   OLWM puts the info here */

	XSync(theDisp, False);
	XGetWindowAttributes(theDisp, mainW, &xwa);

	if (DEBUG)
	  fprintf(stderr,"XGetAttr: mainW %d,%d %dx%d\n", xwa.x, xwa.y,
		  xwa.width, xwa.height);

	if (xwa.x == 0 && xwa.y == 0) {
	  /* MWM, at least mine, puts 0's in those fields.  To get the
	     info, we'll have to query the parent window */

	  XSync(theDisp, False);
	  XGetWindowAttributes(theDisp, reparent_event->parent, &xwa);

	  if (DEBUG)
	    fprintf(stderr,"XGetAttr: parent %d,%d %dx%d\n", xwa.x, xwa.y,
		    xwa.width, xwa.height);
	}
	else {
	  /* KLUDGE:  if we're running olwm, xwa.{x,y} won't be 0,0.
	     in olwm, the window drifts down and right each time
	     SetWindowPos() is called.  God knows why.  Anyway, I'm
	     inserting a kludge here to increase 'ch_offx' and 'ch_offy'
	     by bwidth so that this drifting won't happen.  No doubt this'll
	     screw up behavior on some *other* window manager, but it should
	     work with TWM, OLWM, and MWM (the big three) */
	  ch_offx += bwidth;
	  ch_offy += bwidth;
	}

	p_offx = xwa.x;
	p_offy = xwa.y;
      }

      /* Gather info to keep right border inside */
      {
	Window current;
	Window root_r;
	Window parent_r;
	Window *children_r;
	unsigned int nchildren_r;
	XWindowAttributes xwa;

	parent_r=mainW;
	current=mainW;
	do {
	  current=parent_r;
	  XQueryTree(theDisp, current, &root_r, &parent_r,
		     &children_r, &nchildren_r);
	  if (children_r!=NULL) {
	    XFree(children_r);
	  }
	} while (parent_r!=root_r && parent_r!=vrootW);
	XGetWindowAttributes(theDisp, current, &xwa);
	debkludge_offx = eWIDE-xwa.width+p_offx;
	debkludge_offy = eHIGH-xwa.height+p_offy;
      }

#if 0
      /* FIXME: if we want to do this, we first have to wait for a configure
       * notify to avoid a race condition because the location might be in-
       * correct if the window manager does placement after managing the window.
       */
      /* move window around a bit... */
      {
	XWindowAttributes xwa;

	GetWindowPos(&xwa);
	//fprintf(stderr, "RAC: orig window pos %d,%d\n", xwa.x, xwa.y);

	xwa.width = eWIDE;  xwa.height = eHIGH;
	//fprintf(stderr, "RAC: image size now %d,%d\n", xwa.width, xwa.height);

	/* try to keep the damned thing on-screen, if possible */
	if (xwa.x + xwa.width  > vrWIDE) xwa.x = vrWIDE - xwa.width;
	if (xwa.y + xwa.height > vrHIGH) xwa.y = vrHIGH - xwa.height;
	if (xwa.x < 0) xwa.x = 0;
	if (xwa.y < 0) xwa.y = 0;

	//fprintf(stderr, "RAC: moving window to %d,%d\n", xwa.x, xwa.y);
	SetWindowPos(&xwa);
      }
#endif
    }
  }
    break;


  case EnterNotify:
  case LeaveNotify: {
    XCrossingEvent *cross_event = (XCrossingEvent *) event;
    if (cross_event->window == mainW || 0
	/* (cross_event->window == gamW && cmapInGam) */ ) {

      if (cross_event->type == EnterNotify && cross_event->window == mainW) {
	zoomCurs(cross_event->state);
      }


      if (cross_event->type == EnterNotify && LocalCmap && !ninstall)
	XInstallColormap(theDisp,LocalCmap);

      if (cross_event->type == LeaveNotify && LocalCmap && !ninstall)
	XUninstallColormap(theDisp,LocalCmap);
    }
  }
    break;


  case SelectionClear:  break;

  case SelectionRequest:
    {
      XSelectionRequestEvent *xsrevt = (XSelectionRequestEvent *) event;
      XSelectionEvent  xse;

      if (xsrevt->owner     != ctrlW      ||
	  xsrevt->selection != XA_PRIMARY ||
	  xsrevt->target    != XA_STRING) {  /* can't do it. */
	xse.property = None;
      }
      else {
	if (xsrevt->property == None) xse.property = xsrevt->target;
	                         else xse.property = xsrevt->property;

	if (xse.property != None) {
          xerrcode = 0;
	  XChangeProperty(theDisp, xsrevt->requestor, xse.property,
			  XA_STRING, 8, PropModeReplace,
			  (byte *) ((xevPriSel) ? xevPriSel           : "\0"),
			  (int)    ((xevPriSel) ? strlen(xevPriSel)+1 : 1));
          XSync(theDisp, False);
          if (!xerrcode) xse.property = None;
	}
      }

      xse.type       = SelectionNotify;
      xse.send_event = True;
      xse.display    = theDisp;
      xse.requestor  = xsrevt->requestor;
      xse.selection  = xsrevt->selection;
      xse.target     = xsrevt->target;
      xse.time       = xsrevt->time;
      XSendEvent(theDisp, xse.requestor, False, NoEventMask, (XEvent *) &xse);
      XSync(theDisp, False);
    }
    break;



  default: break;		/* ignore unexpected events */
  }  /* switch */

  frominterrupt = 0;
  *donep = done;
  return(retval);
}


/***********************************/
static void SelectDispMB(i)
     int i;
{
  /* called to handle selection of a dispMB item */

  if (i<0 || i>=DMB_MAX) return;

  if (dispMB.dim[i]) return;    /* disabled */

  if (i>=DMB_RAW && i<=DMB_SMOOTH) {
    if      (i==DMB_RAW)  epicMode = EM_RAW;
    else if (i==DMB_DITH) epicMode = EM_DITH;
    else                  epicMode = EM_SMOOTH;

    SetEpicMode();
    GenerateEpic(eWIDE, eHIGH);
    DrawEpic();
    SetCursors(-1);
  }

  else if (i==DMB_COLRW) {   /* toggle rw on/off */
    dispMB.flags[i] = !dispMB.flags[i];
    allocMode = (dispMB.flags[i]) ? AM_READWRITE : AM_READONLY;
    ChangeCmapMode(colorMapMode, 1, 0);
  }

  else if (i>=DMB_COLNORM && i<=DMB_COLSTDC && !dispMB.flags[i]) {
    switch (i) {
    case DMB_COLNORM:
      ChangeCmapMode(CM_NORMAL, 1, 0);
      defaultCmapMode = CM_NORMAL;
      break;
    case DMB_COLPERF:
      ChangeCmapMode(CM_PERFECT,1, 0);
      defaultCmapMode = CM_PERFECT;
      break;
    case DMB_COLOWNC:
      ChangeCmapMode(CM_OWNCMAP,1, 0);
      defaultCmapMode = CM_OWNCMAP;
      break;
    case DMB_COLSTDC:
      ChangeCmapMode(CM_STDCMAP,1, 0);
      defaultCmapMode = CM_STDCMAP;
      break;
    }
  }
}


/***********************************/
static void SelectRootMB(i)
     int i;
{
  /* called to handle selection of a rootMB item */

  if (i<0 || i>=RMB_MAX) return;
  if (rootMB.flags[i])   return;
  if (rootMB.dim[i])     return;

  dispMode = i;

  /* move checkmark */
  for (i=RMB_WINDOW; i<RMB_MAX; i++) rootMB.flags[i] = 0;
  rootMB.flags[dispMode] = 1;

  HandleDispMode();
}


/***********************************/
static void Select24to8MB(i)
     int i;
{
  if (i<0 || i>=CONV24_MAX) return;

  if (conv24MB.dim[i]) return;

  if (i==CONV24_8BIT || i==CONV24_24BIT) {
    if (i != picType) {
      Change824Mode(i);
      if (i==CONV24_8BIT && state824==0) state824 = 1;  /* did 24->8 */
      else if (i==CONV24_24BIT && state824==1) {
	/* went 24->8->24 */
	char buf[512];

	sprintf(buf,"Warning:  You appear to have taken a 24-bit ");
	strcat(buf, "image, turned it to an 8-bit image, and turned ");
	strcat(buf, "it back into a 24-bit image.  Understand that ");
	strcat(buf, "image data has probably been lost in this ");
	strcat(buf, "transformation.  You *may* want to reload the ");
	strcat(buf, "original image to avoid this problem.");

	ErrPopUp(buf, "\nI Know!");

	state824 = 2;   /* shut up until next image is loaded */
      }
    }
  }

  else if (i==CONV24_LOCK) {
    conv24MB.flags[i] = !conv24MB.flags[i];
  }

  else if (i>=CONV24_FAST && i<=CONV24_BEST) {
    conv24 = i;
    for (i=CONV24_FAST; i<=CONV24_BEST; i++) {
      conv24MB.flags[i] = (i==conv24);
    }
  }
}


/***********************************/
static void SelectWindowMB(i)
     int i;
{
  if (i<0 || i>=WMB_MAX) return;
  if (windowMB.dim[i]) return;

  switch (i) {
  case WMB_BROWSE:
    if (strlen(searchdir)) chdir(searchdir);
    else chdir(initdir);
    OpenBrowse();
    break;

  case WMB_COLEDIT:  GamBox (!gamUp);   break;
  case WMB_INFO:     InfoBox(!infoUp);  break;

  case WMB_COMMENT:
    if (!commentUp) OpenCommentText();
    else CloseCommentText();
    break;

  case WMB_TEXTVIEW:  textViewCmd();  break;
  case WMB_ABOUTXV:   ShowLicense();  break;
  case WMB_KEYHELP:   ShowKeyHelp();  break;

  default:  break;
  }
}


/***********************************/
static void SelectSizeMB(i)
     int i;
{
  int w,h;

  if (i<0 || i>=SZMB_MAX) return;
  if (sizeMB.dim[i]) return;

  switch (i) {
  case SZMB_NORM:
    if (cWIDE>maxWIDE || cHIGH>maxHIGH) {
      double r,wr,hr;
      wr = ((double) cWIDE) / maxWIDE;
      hr = ((double) cHIGH) / maxHIGH;

      r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
      w = (int) ((cWIDE / r) + 0.5);
      h = (int) ((cHIGH / r) + 0.5);
    }
    else { w = cWIDE;  h = cHIGH; }

    WResize(w, h);
    break;

  case SZMB_MAXPIC:   WMaximize();  break;

  case SZMB_MAXPECT:
    {
      int w1,h1;
      w1 = eWIDE;  h1 = eHIGH;
      eWIDE = dispWIDE;  eHIGH = dispHIGH;
      FixAspect(0,&w,&h);
      eWIDE = w1;  eHIGH = h1;  /* play it safe */
      WResize(w,h);
    }
    break;

  case SZMB_DOUBLE:  WResize(eWIDE*2,      eHIGH*2);       break;
  case SZMB_HALF:    WResize(eWIDE/2,      eHIGH/2);       break;
  case SZMB_M10:     WResize((eWIDE*9)/10, (eHIGH*9)/10);  break;

  case SZMB_P10:
    w = (eWIDE*11)/10;  h = (eHIGH*11)/10;
    if (w==eWIDE) w++;
    if (h==eHIGH) h++;
    WResize(w,h);
    break;


  case SZMB_SETSIZE:  setSizeCmd();  break;
  case SZMB_ASPECT:   FixAspect(1, &w, &h);  WResize(w,h);  break;

  case SZMB_4BY3:
    w = eWIDE;  h = (w * 3) / 4;
    if (h>maxHIGH) { h = eHIGH;  w = (h*4)/3; }
    WResize(w,h);
    break;

  case SZMB_INTEXP:
    {
      /* round  (eWIDE/cWIDE),(eHIGH/cHIGH) to nearest
	 integer expansion/compression values */

      double w,h;

      if (eWIDE >= cWIDE) {
	w = ((double) eWIDE) / cWIDE;
	w = floor(w + 0.5);
	if (w < 1.0) w = 1.0;
      }
      else {     /* eWIDE < cWIDE */
	int i;  double t,d,min,pick;

	w = (double) eWIDE / (double) cWIDE;
	min = 1.0;  pick=1.0;
	for (i=1; i<cWIDE; i++) {
	  t = 1.0 / (double) i;
	  d = fabs(w - t);
	  if (d < min) { pick = t;  min = d; }
	  if (t < w) break;
	}
	w = pick;
      }

      if (eHIGH >= cHIGH) {
	h = ((double) eHIGH) / cHIGH;
	h = floor(h + 0.5);
	if (h<1.0) h = 1.0;
      }
      else {   /* eHIGH < cHIGH */
	int i;  double t,d,min,pick;

	h = (double) eHIGH / (double) cHIGH;
	min = 1.0;  pick=1.0;
	for (i=1; i<cHIGH; i++) {
	  t = 1.0 / (double) i;
	  d = fabs(h - t);
	  if (d < min) { pick = t;  min = d; }
	  if (t < h) break;
	}
	h = pick;
      }

      WResize((int) (w*cWIDE), (int) (h*cHIGH));
    }
    break;

  default: break;
  }
}


/***********************************/
static void DoPrint()
{
  /* pops open appropriate dialog boxes, issues print command */

  int                i;
  char               txt[512], str[PRINTCMDLEN + 10];
  static const char *labels[] = { "\03Color", "\07Grayscale", " B/W", "\033Cancel" };
                          /* ^B ("\02") already used for moving cursor back */

  strcpy(txt, "Print:  Enter a command that will read a PostScript file ");
  strcat(txt, "from stdin and print it to the desired printer.\n\n");
#ifndef VMS
  strcat(txt, "(e.g., 'lpr -Pname' on Unix systems)");
#else
  strcat(txt, "(e.g., 'Print /Queue = XV_Queue' on a VMS system)");
#endif

  i = GetStrPopUp(txt, labels, 4, printCmd, PRINTCMDLEN, "", 0);
  if (i == 3 || strlen(printCmd)==0) return;   /* CANCEL */

  if (dirUp == BLOAD) DirBox(0);

  SetDirSaveMode(F_FORMAT, F_PS);
  SetDirSaveMode(F_COLORS, i);

  if (printCmd[0] != '|' && printCmd[0] != '!')
    sprintf(str, "| %s", printCmd);
  else strcpy(str, printCmd);

  DirBox(BSAVE);
  SetDirFName(str);

  FakeButtonPress(&dbut[S_BOK]);
}


/***********************************/
static void debugEvent(e)
     XEvent *e;
{
  switch (e->type) {
  case ButtonPress:
  case ButtonRelease:
    fprintf(stderr,"DBGEVT: %s %s %d,%d, state=%x, button=%d\n",
	    (e->type == ButtonPress) ? "ButtonPress  " : "ButtonRelease",
	    win2name(e->xbutton.window), e->xbutton.x, e->xbutton.y,
	    e->xbutton.state, e->xbutton.button);
    break;

  case KeyPress:
  case KeyRelease:
    fprintf(stderr,"DBGEVT: %s %s state=%x, keycode=%d\n",
	    (e->type == KeyPress) ? "KeyPress  " : "KeyRelease",
	    win2name(e->xkey.window),
	    e->xkey.state, e->xkey.keycode);
    break;

  case Expose:
    fprintf(stderr,"DBGEVT: Expose %s %d,%d %dx%d count=%d\n",
	    win2name(e->xexpose.window), e->xexpose.x, e->xexpose.y,
	    e->xexpose.width, e->xexpose.height, e->xexpose.count);
    break;

  case ClientMessage:
    fprintf(stderr,"DBGEVT: ClientMessage %s\n",win2name(e->xclient.window));
    break;

  case ConfigureNotify:
    fprintf(stderr,"DBGEVT: ConfigureNotify %s %d,%d %dx%d  %s\n",
	    win2name(e->xconfigure.window), e->xconfigure.x, e->xconfigure.y,
	    e->xconfigure.width, e->xconfigure.height,
	    (e->xconfigure.send_event) ? "synthetic" : "real");
    break;

  case MapNotify:
    fprintf(stderr,"DBGEVT: MapNotify %s\n", win2name(e->xany.window));
    break;

  case ReparentNotify:
    fprintf(stderr,"DBGEVT: ReparentNotify %s\n", win2name(e->xany.window));
    break;

  case EnterNotify:
  case LeaveNotify:
    fprintf(stderr,"DBGEVT: %s %s\n",
	    (e->type==EnterNotify) ? "EnterNotify" : "LeaveNotify",
	    win2name(e->xany.window));
    break;

  default:
    fprintf(stderr,"DBGEVT: unknown event type (%d), window %s\n",
	    e->type, win2name(e->xany.window));
    break;
  }
}

static const char *win2name(win)
     Window win;
{
  static char foo[16];

  if      (win == mainW)  return "mainW";
  else if (win == rootW)  return "rootW";
  else if (win == vrootW) return "vrootW";
  else if (win == infoW)  return "infoW";
  else if (win == ctrlW)  return "ctrlW";
  else if (win == dirW)   return "dirW";
  else if (win == gamW)   return "gamW";
  else if (win == psW)    return "psW";

  else {
    sprintf(foo, "0x%08x", (u_int) win);
    return foo;
  }
}


/***********************************/
static void handleButtonEvent(event, donep, retvalp)
  XEvent *event;
  int    *donep, *retvalp;
{
  XButtonEvent *but_event;
  int i,x,y, done, retval, shift;
  Window win;

  but_event = (XButtonEvent *) event;
  done = *donep;  retval = *retvalp;

  win = but_event->window;
  x = but_event->x;  y = but_event->y;
  shift = but_event->state & ShiftMask;

  switch (event->type) {
  case ButtonPress:
    /* *always* check for pop-up events, as errors can happen... */
    if (PUCheckEvent  (event)) break;

    if (autoquit && win == mainW) Quit(0);

    if (viewonly) break;     /* ignore all other button presses */

    if (win == mainW && !useroot && showzoomcursor) {
      DoZoom(x, y, but_event->button);
      break;
    }

    if (PSCheckEvent  (event)) break;

#ifdef HAVE_JPEG
    if (JPEGCheckEvent(event)) break;
#endif

#ifdef HAVE_JP2K
    if (JP2KCheckEvent(event)) break;
#endif

#ifdef HAVE_TIFF
    if (TIFFCheckEvent(event)) break;
#endif

#ifdef HAVE_PNG
    if (PNGCheckEvent (event)) break;
#endif

#ifdef HAVE_PCD
    if (PCDCheckEvent (event)) break;	/* event has been processed */
#endif

#ifdef HAVE_PIC2
    if (PIC2CheckEvent(event)) break;
#endif

#ifdef HAVE_MGCSFX
    if (MGCSFXCheckEvent(event)) break;
#endif

#ifdef TV_MULTILINGUAL
    if (CharsetCheckEvent(event)) break;
#endif

    if (GamCheckEvent (event)) break;
    if (BrowseCheckEvent (event, &retval, &done)) break;
    if (TextCheckEvent   (event, &retval, &done)) break;

    switch (but_event->button) {

    case Button1:
      if      (win == mainW) DoSelection(but_event);

      else if (win == ctrlW) {
	if      (MBClick(&dispMB,   x,y)) SelectDispMB  (MBTrack(&dispMB)  );
	else if (MBClick(&conv24MB, x,y)) Select24to8MB (MBTrack(&conv24MB));
	else if (MBClick(&rootMB,   x,y)) SelectRootMB  (MBTrack(&rootMB)  );
	else if (MBClick(&windowMB, x,y)) SelectWindowMB(MBTrack(&windowMB));
	else if (MBClick(&sizeMB,   x,y)) SelectSizeMB  (MBTrack(&sizeMB)  );
	else if (MBClick(&algMB, x,y)) {
	  algMB.dim[ALG_BLEND] = !HaveSelection();
	  i = MBTrack(&algMB);
	  if (i>=0) DoAlg(i);
	  break;
	}

	i=ClickCtrl(x,y);

	switch (i) {
	case BNEXT:   retval= NEXTPIC;  done=1;     break;
	case BPREV:   retval= PREVPIC;  done=1;     break;
	case BLOAD:   DirBox(BLOAD);                break;
	case BSAVE:   DirBox(BSAVE);                break;
	case BDELETE: if (DeleteCmd()) { done = 1;  retval = DELETE; }  break;
	case BPRINT:  DoPrint();                    break;

	case BCOPY:   DoImgCopy();                  break;
	case BCUT:    DoImgCut();                   break;
	case BPASTE:  DoImgPaste();                 break;
	case BCLEAR:  DoImgClear();                 break;
	case BGRAB:   if (Grab()) {done=1; retval = GRABBED;}  break;
	case BUP10:   SelectSizeMB(SZMB_P10);       break;
	case BDN10:   SelectSizeMB(SZMB_M10);       break;
	case BROTL:   Rotate(1);                    break;
	case BROTR:   Rotate(0);                    break;
	case BFLIPH:  Flip(0);                      break;
	case BFLIPV:  Flip(1);                      break;

	case BCROP:   Crop();                       break;
	case BUNCROP: UnCrop();                     break;
	case BACROP:  AutoCrop();                   break;

	case BPAD:
	  {
	    int mode, wide, high, opaque, omode;  char *str;

	    while (PadPopUp(&mode, &str, &wide, &high, &opaque, &omode)==0) {
	      if (DoPad(mode, str, wide, high, opaque, omode)) {
		done = 1;  retval = PADDED;  break;
	      }
	    }
	  }
	  break;

	case BANNOT:  annotatePic();                break;

	case BABOUT:  SelectWindowMB(WMB_ABOUTXV);  break;
	case BXV:     retval = DFLTPIC;  done=1;    break;
	case BQUIT:   retval = QUIT;     done=1;    break;

	default:      break;
	}

	if (i==BFLIPH || i==BFLIPV) {
	  DrawEpic();
	  SetCursors(-1);
	}
      }

      else if (win == nList.win) {
	i=LSClick(&nList,but_event);
	if (curname<0) ActivePrevNext();
	if (i>=0) { done = 1;  retval = i; }
      }

      else if (win == nList.scrl.win) SCTrack(&nList.scrl, x, y);

      else if (win == dirW) {
	i=ClickDirW(x,y);

	switch (i) {
	case S_BOK:   if (dirUp == BLOAD) {
	    if (!DirCheckCD()) {
	      retval = LOADPIC;
	      done=1;
	    }
	  }
	  else if (dirUp == BSAVE) {
	    DoSave();
	  }
	  break;

	case S_BCANC: DirBox(0);  break;

	case S_BRESCAN:
	  WaitCursor();  LoadCurrentDirectory();  SetCursors(-1);
	  break;
	}
      }

      else if (win == dList.win) {
	i=LSClick(&dList,but_event);
	SelectDir(i);
      }

      else if (win == dList.scrl.win) SCTrack(&dList.scrl, x,y);
      else if (win == infoW)          InfoBox(0);  /* close info */

      break;


    case Button2:
      if (win == mainW && !useroot) {
	if (!shift && !DoSelection(but_event)) TrackPicValues(x,y);
	else if (shift) Paint();
      }
      break;

    case Button3:  /* if using root, MUST NOT get rid of ctrlbox. */
      if (!shift && !useroot) CtrlBox(!ctrlUp);
      else if (shift) BlurPaint();
      break;

    case Button4:   /* note min vs. max, + vs. - */
      if (win == ctrlW || win == nList.win || win == nList.scrl.win) {
	SCRL *sp=&nList.scrl;
	int  halfpage=sp->page/2;

	if (sp->val > sp->min+halfpage)
	  SCSetVal(sp,sp->val-halfpage);
	else
	  SCSetVal(sp,sp->min);
      }
      else if (win ==  dirW || win == dList.win || win == dList.scrl.win) {
	SCRL *sp=&dList.scrl;
	int  halfpage=sp->page/2;

	if (sp->val > sp->min+halfpage)
	  SCSetVal(sp,sp->val-halfpage);
	else
	  SCSetVal(sp,sp->min);
      }
      break;

    case Button5:   /* note max vs. min, - vs. + */
      if (win == ctrlW || win == nList.win || win == nList.scrl.win) {
	SCRL *sp=&nList.scrl;
	int  halfpage=sp->page/2;

	if (sp->val < sp->max-halfpage)
	  SCSetVal(sp,sp->val+halfpage);
	else
	  SCSetVal(sp,sp->max);
      }
      else if (win ==  dirW || win == dList.win || win == dList.scrl.win) {
	SCRL *sp=&dList.scrl;
	int  halfpage=sp->page/2;

	if (sp->val < sp->max-halfpage)
	  SCSetVal(sp,sp->val+halfpage);
	else
	  SCSetVal(sp,sp->max);
      }
      break;

    default:       break;
    }
  }

  *donep = done;  *retvalp = retval;
}


/***********************************/
static void handleKeyEvent(event, donep, retvalp)
  XEvent *event;
  int    *donep, *retvalp;
{
  /* handles KeyPress and KeyRelease events, called from HandleEvent */

  XKeyEvent *key_event;
  KeySym     ks;
  char       buf[128];
  int        stlen, dealt, done, retval, shift, ctrl, meta, ck;
  u_int      svkeystate;

  key_event = (XKeyEvent *) event;
  done      = *donep;
  retval    = *retvalp;

  switch (event->type) {
  case KeyRelease:
    if (viewonly) break;     /* ignore all user input */

    stlen = XLookupString(key_event,buf,128,&ks,(XComposeStatus *) NULL);
    dealt = 0;

    if (key_event->window == mainW) {
      u_int foo = key_event->state;

      if (ks == XK_Shift_L   || ks == XK_Shift_R)
	foo = foo & (u_int) (~ShiftMask);
      if (ks == XK_Control_L || ks == XK_Control_R)
	foo = foo & (u_int) (~ControlMask);
      if (ks == XK_Meta_L    || ks == XK_Meta_R)
	foo = foo & (u_int) (~Mod1Mask);
      if (ks == XK_Alt_L     || ks == XK_Alt_R)
	foo = foo & (u_int) (~Mod1Mask);

      zoomCurs(foo);
    }
    break;


  case KeyPress:
    svkeystate = key_event->state;
    key_event->state &= ~Mod1Mask;      /* remove meta from translation */
    stlen = XLookupString(key_event,buf,128,&ks,(XComposeStatus *) NULL);
    key_event->state = svkeystate;

    shift = key_event->state & ShiftMask;
    ctrl  = key_event->state & ControlMask;
    meta  = key_event->state & Mod1Mask;
    dealt = 0;

    RemapKeyCheck(ks, buf, &stlen);

    if (PUCheckEvent  (event)) break;          /* always check popups */

    if (autoquit && key_event->window == mainW) Quit(0);

    if (viewonly && !frominterrupt) break;     /* ignore all user input */

    if (PSCheckEvent  (event)) break;

    if (key_event->window == mainW) {
      u_int foo = key_event->state;

      if (ks == XK_Shift_L   || ks == XK_Shift_R)   foo = foo | ShiftMask;
      if (ks == XK_Control_L || ks == XK_Control_R) foo = foo | ControlMask;
      if (ks == XK_Meta_L    || ks == XK_Meta_R)    foo = foo | Mod1Mask;
      if (ks == XK_Alt_L     || ks == XK_Alt_R)     foo = foo | Mod1Mask;
      zoomCurs(foo);
    }

#ifdef HAVE_JPEG
    if (JPEGCheckEvent(event)) break;
#endif

#ifdef HAVE_JP2K
    if (JP2KCheckEvent(event)) break;
#endif

#ifdef HAVE_TIFF
    if (TIFFCheckEvent(event)) break;
#endif

#ifdef HAVE_PNG
    if (PNGCheckEvent (event)) break;
#endif

    if (PCDCheckEvent (event)) break;

#ifdef HAVE_PIC2
    if (PIC2CheckEvent(event)) break;
#endif

#ifdef HAVE_PCD
    if (PCDCheckEvent (event)) break;
#endif

#ifdef HAVE_MGCSFX
    if (MGCSFXCheckEvent(event)) break;
#endif

    if (GamCheckEvent (event)) break;
    if (BrowseCheckEvent (event, &retval, &done)) break;
    if (TextCheckEvent   (event, &retval, &done)) break;


    /* Support for multi-image files ("multipage docs").  Check for PgUp/PgDn
       or 'p' in any window but control or directory; PgUp/PgDn are already
       used to page through the file list in those windows.  If no cropping
       rectangle is active, shift-Up and shift-Down also work. */

    if (key_event->window != ctrlW && key_event->window != dirW) {
      dealt = 1;

      ck = CursorKey(ks, shift, 0);
      if (ck==CK_PAGEUP || (ck==CK_UP && shift && !but[BCROP].active)) {
	if (strlen(pageBaseName) && numPages>1) {
	  done = 1;  retval = OP_PAGEUP;
	}
	else XBell(theDisp,0);
      }

      else if (ck==CK_PAGEDOWN ||
	       (ck==CK_DOWN && shift && !but[BCROP].active)) {
	if (strlen(pageBaseName) && numPages>1) {
	  done = 1;  retval = OP_PAGEDN;
	}
	else XBell(theDisp,0);
      }

      else if (buf[0] == 'p' && stlen>0) {
	if (strlen(pageBaseName) && numPages>1) {
	  int                i,j, okay;
	  char               buf[64], txt[512];
	  static const char *labels[] = { "\nOk", "\033Cancel" };

	  /* ask what page to go to */
	  sprintf(txt, "Go to page number...   (1-%d)", numPages);
	  sprintf(buf, "%d", curPage + 1);

	  okay = 0;
	  do {
	    i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789", 1);
	    if (!i && strlen(buf) > (size_t) 0) {
	      /* hit 'Ok', had a string entered */
	      /* check for page in range */
	      j = atoi(buf);
	      if (j>=1 && j<=numPages) {
		curPage = j;   /* one page past desired page */
		done = 1;
		retval = OP_PAGEUP;
		okay=1;
	      }
	      else XBell(theDisp, 0);
	    }
	    else okay = 1;
	  } while (!okay);
	}
	else XBell(theDisp, 0);
      }

      else dealt = 0;

      if (dealt) break;
    }



    /* check for crop rect keys */
    if (key_event->window == mainW) {
      dealt = 1;
      ck = CursorKey(ks, shift, 0);
      if      (ck==CK_LEFT)  CropKey(-1, 0,shift,ctrl);
      else if (ck==CK_RIGHT) CropKey( 1, 0,shift,ctrl);
      else if (ck==CK_UP)    CropKey( 0,-1,shift,ctrl);
      else if (ck==CK_DOWN)  CropKey( 0, 1,shift,ctrl);
      else dealt = 0;
      if (dealt) break;
    }


    /* check for List keys */
    if (key_event->window == ctrlW || key_event->window == dirW) {
      LIST *theList;

      ck = CursorKey(ks, shift, 1);
      if (key_event->window == dirW) theList = &dList;
      else theList = &nList;

      dealt = 1;

      if      (ck==CK_PAGEUP)   LSKey(theList,LS_PAGEUP);
      else if (ck==CK_PAGEDOWN) LSKey(theList,LS_PAGEDOWN);
      else if (ck==CK_UP)       LSKey(theList,LS_LINEUP);
      else if (ck==CK_DOWN)     LSKey(theList,LS_LINEDOWN);
      else if (ck==CK_HOME)     LSKey(theList,LS_HOME);
      else if (ck==CK_END)      LSKey(theList,LS_END);
      else dealt = 0;

      if (theList == &nList && dealt && curname<0) ActivePrevNext();

      if (theList == &dList && dealt) {  /* changed dir selection */
	SelectDir(-1);  /* nothing was double-clicked */
      }

      if (dealt) break;
    }


    /* check dir filename arrows */
    if (key_event->window == dirW) {
      ck = CursorKey(ks, shift, 1);
      if (ck==CK_LEFT)  { DirKey('\002'); break; }
      if (ck==CK_RIGHT) { DirKey('\006'); break; }
    }


    /* check for preset keys     (meta-1, meta-2, meta-3, meta-4, meta-0)
     * and cut/copy/paste/clear  (meta-x, meta-c, meta-v, meta-d)
     * and 8/24 bit toggle       (meta-8)
     * and some algorithms       (meta-b, meta-t, meta-p, meta-l, etc.)
     */

    if (meta) {  /* meta is down */
      dealt = 1;
      if      (ks==XK_1) FakeButtonPress(&gbut[G_B1]);
      else if (ks==XK_2) FakeButtonPress(&gbut[G_B2]);
      else if (ks==XK_3) FakeButtonPress(&gbut[G_B3]);
      else if (ks==XK_4) FakeButtonPress(&gbut[G_B4]);
      else if (ks==XK_r || ks==XK_0)
	                 FakeButtonPress(&gbut[G_BRESET]);

      else if (ks==XK_x) FakeButtonPress(&but[BCUT]);
      else if (ks==XK_c) FakeButtonPress(&but[BCOPY]);
      else if (ks==XK_v) FakeButtonPress(&but[BPASTE]);
      else if (ks==XK_d) FakeButtonPress(&but[BCLEAR]);

      else if (ks==XK_u) DoAlg(ALG_NONE);
      else if (ks==XK_b) DoAlg(ALG_BLUR);
      else if (ks==XK_s) DoAlg(ALG_SHARPEN);
      else if (ks==XK_e) DoAlg(ALG_EDGE);
      else if (ks==XK_m) DoAlg(ALG_TINF);
      else if (ks==XK_o) DoAlg(ALG_OIL);
      else if (ks==XK_k) DoAlg(ALG_MEDIAN);

      else if ((ks==XK_B  || (ks==XK_b && shift)) && HaveSelection())
	                                        DoAlg(ALG_BLEND);

      else if (ks==XK_p)                        DoAlg(ALG_PIXEL);

      else if (ks==XK_S || (ks==XK_s && shift)) DoAlg(ALG_SPREAD);

      else if (ks==XK_t || ks==XK_T) {
	if (ctrl || shift || ks==XK_T)          DoAlg(ALG_ROTATE);
        else                                    DoAlg(ALG_ROTATECLR);
      }

      else if (ks==XK_a) FakeButtonPress(&gbut[G_BAPPLY]);

      else if (ks==XK_8) {
	if (picType==PIC8) Select24to8MB(CONV24_24BIT);
	              else Select24to8MB(CONV24_8BIT);
      }

      else dealt = 0;

      if (dealt) break;
    }

    /* Check for function keys */
    if (key_event->window == ctrlW || key_event->window == mainW) {
      if (ks >= XK_F1 && ks <= XK_F1 + FSTRMAX - 1) {
        int fkey = ks - XK_F1;
        if (fkeycmds[fkey] && fullfname[0]) {
#define CMDLEN 4096
          char cmd[CMDLEN];
          /* If a command begins with '@', we do not reload the current file */
          int noreload = (fkeycmds[fkey][0] == '@');
          int x = 0, y = 0, w = 0, h = 0;
          if (HaveSelection())
            GetSelRCoords(&x, &y, &w, &h);
          snprintf(cmd, CMDLEN, fkeycmds[fkey] + noreload, fullfname, x, y, w, h);
#undef CMDLEN
          if (DEBUG) fprintf(stderr, "Executing '%s'\n", cmd);
          WaitCursor();
          system(cmd);
          SetCursors(-1);
          if (!noreload) {
            retval = RELOAD;
            done = 1;
          }
          break;
        }
      }
    }

    if (!stlen) break;

    if (key_event->window == dirW) {
      if (DirKey(buf[0])) XBell(theDisp,0);
    }
    else {                               /* commands valid in any window */
      switch (buf[0]) {

	/* things in dispMB */
      case 'r':    SelectDispMB(DMB_RAW);           break;
      case 'd':    SelectDispMB(DMB_DITH);          break;
      case 's':    SelectDispMB(DMB_SMOOTH);        break;

	/* things in sizeMB */
      case 'n':    SelectSizeMB(SZMB_NORM);         break;
      case 'm':    SelectSizeMB(SZMB_MAXPIC);       break;
      case 'M':    SelectSizeMB(SZMB_MAXPECT);      break;
      case '>':    SelectSizeMB(SZMB_DOUBLE);       break;
      case '<':    SelectSizeMB(SZMB_HALF);         break;
      case '.':    SelectSizeMB(SZMB_P10);          break;
      case ',':    SelectSizeMB(SZMB_M10);          break;
      case 'S':    SelectSizeMB(SZMB_SETSIZE);      break;
      case 'a':    SelectSizeMB(SZMB_ASPECT);       break;
      case '4':    SelectSizeMB(SZMB_4BY3);         break;
      case 'I':    SelectSizeMB(SZMB_INTEXP);       break;

	/* things in windowMB */
      case '\026':
      case 'V':    SelectWindowMB(WMB_BROWSE);      break;  /* ^V or V */
      case 'e':    SelectWindowMB(WMB_COLEDIT);     break;  /*  e */
      case 'i':    SelectWindowMB(WMB_INFO);        break;  /*  i */
      case '\003': SelectWindowMB(WMB_COMMENT);     break;  /* ^C */
      case '\024': SelectWindowMB(WMB_TEXTVIEW);    break;  /* ^T */
      case '\001': SelectWindowMB(WMB_ABOUTXV);     break;  /* ^A */



	/* buttons in ctrlW */
      case '\t':
      case ' ':    FakeButtonPress(&but[BNEXT]);    break;

      case '\r':
      case '\n':
	if (nList.selected >= 0 && nList.selected < nList.nstr) {
	  done = 1;  retval = nList.selected;
	  if (frominterrupt) retval = RELOAD;
	}
	break;

      case '\010': FakeButtonPress(&but[BPREV]);    break;


      case '\014': FakeButtonPress(&but[BLOAD]);    break;  /* ^L */
      case '\023': FakeButtonPress(&but[BSAVE]);    break;  /* ^S */
      case '\020': FakeButtonPress(&but[BPRINT]);   break;  /* ^P */
      case '\177':
      case '\004': FakeButtonPress(&but[BDELETE]);  break;  /* ^D */

	/* BCOPY, BCUT, BPASTE, BCLEAR handled in 'meta' case */

      case '\007': FakeButtonPress(&but[BGRAB]);    break;  /* ^G */

	/* BUP10, BDN10 handled in sizeMB case */

      case 'T':    FakeButtonPress(&but[BROTL]);    break;
      case 't':    FakeButtonPress(&but[BROTR]);    break;
      case 'h':    FakeButtonPress(&but[BFLIPH]);   break;
      case 'v':    FakeButtonPress(&but[BFLIPV]);   break;
      case 'c':    FakeButtonPress(&but[BCROP]);    break;
      case 'u':    FakeButtonPress(&but[BUNCROP]);  break;
      case 'C':    FakeButtonPress(&but[BACROP]);   break;
      case 'P':    FakeButtonPress(&but[BPAD]);     break;
      case 'A':    FakeButtonPress(&but[BANNOT]);   break;

	/* BABOUT handled in windowMB case */

      case '\021': /* ^Q */
      case 'q':    FakeButtonPress(&but[BQUIT]);    break;

      case '?':    if (!useroot) CtrlBox(!ctrlUp);  break;

	/* things in color editor */
      case 'R':    FakeButtonPress(&gbut[G_BRESET]);   break;
      case 'H':    FakeButtonPress(&gbut[G_BHISTEQ]);  break;
      case 'N':    FakeButtonPress(&gbut[G_BMAXCONT]); break;

      default:     break;
      }
    }
  }

  *donep = done;  *retvalp = retval;
}


/***********************************/
static void zoomCurs(mask)
     u_int mask;
{
  int zc;
  zc = ((mask & ControlMask) && !(mask & ShiftMask) && !(mask & Mod1Mask));

  if (zc != showzoomcursor) {
    showzoomcursor = zc;
    SetCursors(-1);
  }
}


/***********************************/
static void textViewCmd()
{
  int   i;
  char *name;

  i = nList.selected;
  if (i<0 || i>=numnames) return;     /* shouldn't happen */

  if (namelist[i][0] != '/') {  /* prepend 'initdir' */
    name = (char *) malloc(strlen(namelist[i]) + strlen(initdir) + 2);
    if (!name) FatalError("malloc in textViewCmd failed");
    sprintf(name,"%s/%s", initdir, namelist[i]);
  }
  else name = namelist[i];

  TextView(name);

  if (name != namelist[i]) free(name);
}


/***********************************/
static void setSizeCmd()
{
  /* open 'set size' prompt window, get a string, parse it, and try to
     set the window size accordingly */

  int                i, arg1, arg2, numargs, pct1, pct2, state, neww, newh;
  char               txt[512], buf[64], *sp, ch;
  static const char *labels[] = { "\nOk", "\033Cancel" };

  sprintf(txt, "Enter new image display size (ex. '400 x 300'),\n");
  strcat (txt, "expansion ratio (ex. '75%'),\n");
  strcat (txt, "or expansion ratios (ex. '200% x 125%'):");

  sprintf(buf, "%d x %d", eWIDE, eHIGH);    /* current vals */

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789x% ", 1);

  if (i) return;     /* cancelled */
  if (strlen(buf) == 0) return;     /* no command */


  /* attempt to parse the string accordingly...
   * parses strings of the type: <num> [%] [ x <num> [%] ]
   * (-ish.  <num> all by itself isn't legal)
   * there may be any # of spaces between items, including zero
   */

  arg1 = arg2 = numargs = pct1 = pct2 = state = 0;
  sp = buf;

  do  {
    ch = *sp++;

    switch (state) {
    case 0:             /* haven't seen arg1 yet */
      if      (ch == ' ') {}
      else if (ch == '%' || ch == 'x' || ch == '\0') state = 99;  /* no arg1 */
      else { arg1  = (ch - '0');  state = 1; }
      break;

    case 1:             /* parsing arg1 */
      numargs = 1;
      if      (ch == ' ')  state = 2;
      else if (ch == '%')  state = 3;
      else if (ch == 'x')  state = 4;
      else if (ch == '\0') state = 99;
      else arg1 = (arg1 * 10) + (ch - '0');
      break;

    case 2:             /* got arg1 and whitespace */
      if      (ch == ' ') {}
      else if (ch == '%') state = 3;
      else if (ch == 'x') state = 4;
      else state = 99;
      break;

    case 3:             /* got arg1 % */
      pct1 = 1;
      if      (ch == ' ')  {}
      else if (ch == '%')  state = 99;
      else if (ch == 'x')  state = 4;
      else if (ch == '\0') state = 100;
      else state = 99;
      break;

    case 4:             /* got arg1 [%] x */
      if      (ch == ' ') {}
      else if (ch == '%' || ch == 'x' || ch == '\0') state = 99;
      else { arg2 = (ch - '0');  state = 5; }
      break;

    case 5:             /* parsing arg2 */
      numargs = 2;
      if      (ch == ' ')  state = 6;
      else if (ch == '%')  state = 7;
      else if (ch == 'x')  state = 99;
      else if (ch == '\0') state = 100;
      else arg2 = (arg2 * 10) + (ch - '0');
      break;

    case 6:             /* got arg2 and whitespace */
      if      (ch == ' ')  {}
      else if (ch == '%')  state = 7;
      else if (ch == 'x')  state = 99;
      else if (ch == '\0') state = 100;
      else state = 99;
      break;

    case 7:             /* got arg1 [%] x arg2 % */
      pct2  = 1;
      state = 100;
      break;

    case 99:            /* error in parsing */
      break;

    case 100:           /* successful parse */
      break;
    }
  } while (state!=99 && state!=100);

  /* done parsing... */
  if (state == 99) {
    ErrPopUp("Error:  The entered SetSize string is not valid.", "\nRight.");
    return;
  }

  if (DEBUG)
    fprintf(stderr,"setSize:  arg1=%d, arg2=%d, numargs=%d, pct=%d,%d\n",
	    arg1, arg2, numargs, pct1, pct2);

  /* otherwise... */
  if (numargs == 1) {
    if (pct1) {
      neww = (cWIDE * arg1) / 100;
      newh = (cHIGH * arg1) / 100;
    }
    else return;    /* shouldn't happen */
  }
  else {   /* numargs = 2; */
    neww = (pct1) ? (cWIDE * arg1) / 100 : arg1;
    newh = (pct2) ? (cHIGH * arg2) / 100 : arg2;
  }

  if (neww < 1 || newh < 1 || neww > 64000 || newh > 64000) {
    sprintf(txt, "The new desired image display size of '%d x %d' is %s",
	    neww, newh, "ludicrous.  Ignored.");
    ErrPopUp(txt, "\nSez you!");
    return;
  }

  WResize(neww, newh);
}


/***********************************/
void NewCutBuffer(str)
     char *str;
{
  /* called whenever contents of CUT_BUFFER0 and PRIMARY selection should
     be changed.  Only works for strings.  Copies the data, so the string
     doesn't have to be static. */

  if (!str) return;

  XStoreBytes(theDisp, str, (int) strlen(str));   /* CUT_BUFFER0 */
  XSetSelectionOwner(theDisp, XA_PRIMARY, ctrlW, lastEventTime);

  if (xevPriSel) free(xevPriSel);
  xevPriSel = (char *) malloc(strlen(str) + 1);
  if (xevPriSel) strcpy(xevPriSel, str);
}

/***********************************/
void DrawWindow(x,y,w,h)
     int x,y,w,h;
{
  if (x+w < eWIDE) w++;  /* add one for broken servers (?) */
  if (y+h < eHIGH) h++;

  if (theImage)
    XPutImage(theDisp,mainW,theGC,theImage,x,y,x,y, (u_int) w, (u_int) h);
  else
    if (DEBUG) fprintf(stderr,"Tried to DrawWindow when theImage was NULL\n");
}


/***********************************/
void WResize(w,h)
     int w,h;
{
  XWindowAttributes xwa;

  RANGE(w,1,maxWIDE);  RANGE(h,1,maxHIGH);

  if (useroot) {
    Resize(w,h);
    MakeRootPic();
    SetCursors(-1);
    return;
  }

  GetWindowPos(&xwa);

  /* determine if new size goes off edge of screen.  if so move window so it
     doesn't go off screen */
  if (xwa.x + w > vrWIDE) xwa.x = vrWIDE - w;
  if (xwa.y + h > vrHIGH) xwa.y = vrHIGH - h;
  if (xwa.x < 0) xwa.x = 0;
  if (xwa.y < 0) xwa.y = 0;

  if (DEBUG) fprintf(stderr,"%s: resizing window to %d,%d at %d,%d\n",
		     cmd,w,h,xwa.x,xwa.y);

  /* resize the window */
  xwa.width = w;  xwa.height = h;

  SetWindowPos(&xwa);
}




/***********************************/
static void WMaximize()
{
  if (useroot) WResize((int) dispWIDE, (int) dispHIGH);
  else {
    XWindowAttributes xwa;
    xvbzero((char *) &xwa, sizeof(XWindowAttributes));
    xwa.x = xwa.y = 0;
    xwa.width  = dispWIDE;
    xwa.height = dispHIGH;
    SetWindowPos(&xwa);
  }
}




/***********************************/
void WRotate()
{
  /* rotate the window and redraw the contents  */

  if (but[BCROP].active) BTSetActive(&but[BCROP],0);

  if (useroot || eWIDE == eHIGH) {
    /* won't see any configure events.  Manually redraw image */
    DrawEpic();
    SetCursors(-1);
    return;
  }
  else {
    rotatesLeft++;
    XClearWindow(theDisp, mainW);  /* get rid of old bits */
    GenExpose(mainW, 0, 0, (u_int) eWIDE, (u_int) eHIGH);
    { int ew, eh;
      ew = eWIDE;  eh = eHIGH;
      WResize(eWIDE, eHIGH);
      if (ew>maxWIDE || eh>maxHIGH) {   /* rotated pic too big, scale down */
	double r,wr,hr;
	wr = ((double) ew) / maxWIDE;
	hr = ((double) eh) / maxHIGH;

	r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
	ew = (int) ((ew / r) + 0.5);
	eh = (int) ((eh / r) + 0.5);
	WResize(ew,eh);
      }
    }
  }
}


/***********************************/
void WCrop(w,h,dx,dy)
     int w,h,dx,dy;
{
  int ex, ey;
  XWindowAttributes xwa;

  if (useroot) {
    MakeRootPic();
    SetCursors(-1);
  }

  else {
    /* we want to move window to old x,y + dx,dy (in pic coords) */
    GetWindowPos(&xwa);

    if (!origcropvalid) {  /* first crop.  remember win pos */
      origcropvalid = 1;
      origcropx = xwa.x;
      origcropy = xwa.y;
    }

    CoordC2E(dx, dy, &ex, &ey);

    xwa.x += ex;  xwa.y += ey;
    xwa.width = w;  xwa.height = h;
    GenExpose(mainW, 0, 0, (u_int) eWIDE, (u_int) eHIGH);
    SetWindowPos(&xwa);
  }
}


/***********************************/
void WUnCrop()
{
  int w,h;
  XWindowAttributes xwa;

  /* a proper epic has been generated.  eWIDE,eHIGH are the new window size */


  if (useroot) {
    MakeRootPic();
    SetCursors(-1);
  }

  else {   /* !useroot */
    GetWindowPos(&xwa);

    w = eWIDE;  h = eHIGH;

    /* restore to position when originally cropped */
    if (origcropvalid) {  /* *should* always be true... */
      origcropvalid = 0;
      xwa.x = origcropx;
      xwa.y = origcropy;
    }

    /* keep on screen */
    if (xwa.x + w > vrWIDE) xwa.x = vrWIDE - w;
    if (xwa.y + h > vrHIGH) xwa.y = vrHIGH - h;
    if (xwa.x < 0) xwa.x = 0;
    if (xwa.y < 0) xwa.y = 0;

    xwa.width = w;  xwa.height = h;

    if (!useroot) {
      SetWindowPos(&xwa);
      GenExpose(mainW, 0, 0, (u_int) eWIDE, (u_int) eHIGH);
    }
  }
}



/***********************************/
void GetWindowPos(xwa)
XWindowAttributes *xwa;
{
  Window child;

  /* returns the x,y,w,h coords of mainW.  x,y are relative to rootW
     the border is not included (x,y map to top-left pixel in window) */

  /* Get the window width/height */
  XGetWindowAttributes(theDisp,mainW,xwa);

  /* Get the window origin */
  XTranslateCoordinates(theDisp,mainW,rootW,0,0,&xwa->x,&xwa->y,&child);
}


/***********************************/
void SetWindowPos(xwa)
XWindowAttributes *xwa;
{
  /* sets window x,y,w,h values */
  XWindowChanges    xwc;

  /* Adjust from window origin, to border origin */
  xwc.x = xwa->x - xwa->border_width - ch_offx;
  xwc.y = xwa->y - xwa->border_width - ch_offy;

  if (!xwa->border_width) xwa->border_width = bwidth;
  xwc.border_width = xwa->border_width;

  /* if we're less than max size in one axis, allow window manager doohickeys
     on the screen */

  if (xwa->width  < dispWIDE && xwc.x < p_offx) xwc.x = p_offx;
  if (xwa->height < dispHIGH && xwc.y < p_offy) xwc.y = p_offy;

  /* Try to keep bottom right decorations inside */
#ifdef CRAP
  if (xwc.x+eWIDE-debkludge_offx>dispWIDE) {
    xwc.x=dispWIDE-eWIDE+debkludge_offx;
    if (xwc.x<0) xwc.x=0;
  }
  if (xwc.y+eHIGH-debkludge_offy>dispHIGH) {
    xwc.y=dispHIGH-eHIGH+debkludge_offy;
    if (xwc.y<0) xwc.y=0;
  }
#else
  if (xwc.x+eWIDE+p_offx>dispWIDE) {
    xwc.x=dispWIDE-(eWIDE+debkludge_offx);
    if (xwc.x<0) xwc.x=0;
  }
  if (xwc.y+eHIGH+p_offy>dispHIGH) {
    xwc.y=dispHIGH-(eHIGH+debkludge_offy);
    if (xwc.y<0) xwc.y=0;
  }
#endif

  xwc.width  = xwa->width;
  xwc.height = xwa->height;

#define BAD_IDEA
#ifdef BAD_IDEA
  /* if there is a virtual window manager running, then we should translate
     the coordinates that are in terms of 'real' screen into coordinates
     that are in terms of the 'virtual' root window
     from: Daren W. Latham <dwl@mentat.udev.cdc.com> */

  if (vrootW != rootW) { /* virtual window manager running */
    int x1,y1;
    Window child;

    XTranslateCoordinates(theDisp, rootW, vrootW, xwc.x, xwc.y, &x1, &y1, &child);
    if (DEBUG) fprintf(stderr,"SWP: translate: %d,%d -> %d,%d\n",
		       xwc.x, xwc.y, x1, y1);
    xwc.x = x1;  xwc.y = y1;
  }
#endif


  if (DEBUG) {
    fprintf(stderr,
	    "SWP: xwa=%d,%d %dx%d xwc=%d,%d %dx%d off=%d,%d bw=%d klg=%d,%d\n",
	    xwa->x, xwa->y, xwa->width, xwa->height,
	    xwc.x, xwc.y, xwc.width, xwc.height, p_offx, p_offy,
	    xwa->border_width, kludge_offx, kludge_offy);
  }

  xwc.x += kludge_offx;
  xwc.y += kludge_offy;

#if defined(DXWM) || defined(HAVE_XUI)
  /* dxwm seems to *only* pay attention to the hints */
  {
    XSizeHints hints;
    if (DEBUG) fprintf(stderr,"SWP: doing the DXWM thing\n");
    /* read hints for this window and adjust any position hints */
    if (XGetNormalHints(theDisp, mainW, &hints)) {
      hints.flags |= USPosition | USSize;
      hints.x = xwc.x;  hints.y = xwc.y;
      hints.width = xwc.width; hints.height = xwc.height;
      XSetNormalHints(theDisp, mainW, &hints);
    }

#ifndef MWM     /* don't do this if you're running MWM */
    xwc.x -= 5;   xwc.y -= 25;    /* EVIL KLUDGE */
#endif /* MWM */
  }
#endif

  /* all non-DXWM window managers (?) */
  /* Move/Resize the window. */
  XConfigureWindow(theDisp, mainW,
		   CWX | CWY | CWWidth | CWHeight /*| CWBorderWidth*/, &xwc);
}



/***********************************/
static void CropKey(dx,dy,grow,crop)
     int dx,dy,grow,crop;
{
  int ocx, ocy;

  if (crop) { /* chop off a pixel from the appropriate edge */
    int dealt=1;

    ocx = cXOFF;  ocy = cYOFF;
    if      (dx<0 && cWIDE>1) DoCrop(cXOFF,   cYOFF,   cWIDE-1, cHIGH);
    else if (dx>0 && cWIDE>1) DoCrop(cXOFF+1, cYOFF,   cWIDE-1, cHIGH);
    else if (dy<0 && cHIGH>1) DoCrop(cXOFF,   cYOFF,   cWIDE,   cHIGH-1);
    else if (dy>0 && cHIGH>1) DoCrop(cXOFF,   cYOFF+1, cWIDE,   cHIGH-1);
    else { dealt = 0;  XBell(theDisp, 0); }

    if (dealt) {
      if (useroot) DrawEpic();
      else {
	if (HaveSelection()) EnableSelection(0);
	CreateXImage();
	WCrop(eWIDE, eHIGH, cXOFF-ocx, cYOFF-ocy);
      }
    }
    return;
  }

  if (grow) MoveGrowSelection(0,  0,  dx, dy);
       else MoveGrowSelection(dx, dy, 0,  0);
}


/***********************************/
static void TrackPicValues(mx,my)
     int mx,my;
{
  Window       rW,cW;
  int          rx,ry,ox,oy,x,y, orgx,orgy;
  u_int        mask;
  u_long       wh, bl;
  int          ty, w, ecol, done1;
  char         foo[128];
  const char   *str  =
   "8888,8888 = 123,123,123  #123456  (123,123,123 HSV)  [-2345,-2345]";

  ecol = 0;  wh = infobg;  bl = infofg;

  if (!dropper) {
    Pixmap      pix, mask;
    XColor      cfg, cbg;

    cfg.red = cfg.green = cfg.blue = 0x0000;
    cbg.red = cbg.green = cbg.blue = 0xffff;

    pix = MakePix1(rootW, dropper_bits,  dropper_width,  dropper_height);
    mask= MakePix1(rootW, dropperm_bits, dropperm_width, dropperm_height);
    if (pix && mask)
      dropper = XCreatePixmapCursor(theDisp, pix, mask, &cfg, &cbg,
				    dropper_x_hot, dropper_y_hot);
    if (pix)  XFreePixmap(theDisp, pix);
    if (mask) XFreePixmap(theDisp, mask);
  }

  if (dropper) XDefineCursor(theDisp, mainW, dropper);

  /* do a colormap search for black and white if LocalCmap
     and use those colors instead of infobg and infofg */

  if (LocalCmap) {
    XColor ctab[256];   int  i;   long cval;

    for (i=0; i<nfcols; i++) ctab[i].pixel = freecols[i];
    XQueryColors(theDisp,LocalCmap,ctab,nfcols);

    /* find 'blackest' pixel */
    cval = 0x10000 * 3;
    for (i=0; i<nfcols; i++)
      if ((long)ctab[i].red + (long)ctab[i].green + (long)ctab[i].blue <cval) {
	cval = ctab[i].red + ctab[i].green + ctab[i].blue;
	bl = ctab[i].pixel;
      }

    /* find 'whitest' pixel */
    cval = -1;
    for (i=0; i<nfcols; i++)
      if ((long)ctab[i].red + (long)ctab[i].green + (long)ctab[i].blue >cval) {
	cval = ctab[i].red + ctab[i].green + ctab[i].blue;
	wh = ctab[i].pixel;
      }
  }


  XSetFont(theDisp, theGC, monofont);
  w = XTextWidth(monofinfo, str, (int) strlen(str));

  if (my > eHIGH/2) ty = 0;
               else ty = eHIGH-(monofinfo->ascent + mfinfo->descent)-4;

  XSetForeground(theDisp, theGC, bl);
  XFillRectangle(theDisp, mainW, theGC, 0, ty, (u_int) w + 8,
		 (u_int) (monofinfo->ascent+monofinfo->descent) + 4);
  XSetForeground(theDisp, theGC, wh);
  XSetBackground(theDisp, theGC, bl);
  foo[0] = '\0';


  done1 = ox = oy = orgx = orgy = 0;
  while (1) {
    int px, py, pix;

    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&rx,&ry,&x,&y,&mask)) continue;
    if (done1 && !(mask & Button2Mask)) break;    /* button released */

    CoordE2P(x,y, &px, &py);
    RANGE(px,0,pWIDE-1);
    RANGE(py,0,pHIGH-1);

    if (px!=ox || py!=oy || !done1) {  /* moved, or firsttime.  erase & draw */
      double h1, s1, v1;
      int    rval, gval, bval;

      if (picType == PIC8) {
	ecol = pix = pic[py * pWIDE + px];
	rval = rcmap[pix];  gval = gcmap[pix];  bval = bcmap[pix];
      }
      else {  /* PIC24 */
	rval = pic[py * pWIDE * 3 + px * 3];
	gval = pic[py * pWIDE * 3 + px * 3 + 1];
	bval = pic[py * pWIDE * 3 + px * 3 + 2];
      }

      clearR = rval;  clearG = gval;  clearB = bval;

      rgb2hsv(rval, gval, bval, &h1, &s1, &v1);
      if (h1<0.0) h1 = 0.0;   /* map 'NOHUE' to 0.0 */

      if (!done1) { orgx = px;  orgy = py; }

      sprintf(foo,
   "%4d,%4d = %3d,%3d,%3d  #%02x%02x%02x  (%3d %3d %3d HSV)  [%5d,%5d]",
	      px, py, rval, gval, bval, rval, gval, bval,
	      (int) h1, (int) (s1 * 100), (int) (v1 * 100),
	      px-orgx, py-orgy);

      XDrawImageString(theDisp,mainW,theGC, 4, ty + 2 + monofinfo->ascent,
		       foo, (int) strlen(foo));
      ox = px;  oy = py;
      done1 = 1;
    }
  }
  SetCursors(-1);


  if (foo[0]) {
    strcat(foo, "\n");
    NewCutBuffer(foo);
  }

  XSetFont(theDisp, theGC, mfont);
  DrawWindow(0,ty,eWIDE,(monofinfo->ascent+monofinfo->descent)+4);

  if (picType == PIC8 && ecol != editColor) ChangeEC(ecol);
}


/***********************************/
static Bool IsConfig(dpy, ev, arg)
     Display *dpy;
     XEvent  *ev;
     char    *arg;
{
  XConfigureEvent *cev;

  if (ev->type == ConfigureNotify) {
    cev = (XConfigureEvent *) ev;
    if (cev->window == mainW && (cev->width != eWIDE || cev->height != eHIGH))
      *arg = 1;
  }
  return False;
}

/***********************************/
static int CheckForConfig()
{
  XEvent ev;
  char   foo;

  /* returns true if there's a config event in which mainW changes size
     in the event queue */

  XSync(theDisp, False);
  foo = 0;
  XCheckIfEvent(theDisp, &ev, IsConfig, &foo);
  return foo;
}


/************************************************************************/
void SetEpicMode()
{
  if (epicMode == EM_RAW) {
    dispMB.dim[DMB_RAW]    = 1;
    dispMB.dim[DMB_DITH]   = !(ncols>0 && picType == PIC8);
    dispMB.dim[DMB_SMOOTH] = 0;
  }

  else if (epicMode == EM_DITH) {
    dispMB.dim[DMB_RAW]    = 0;
    dispMB.dim[DMB_DITH]   = 1;
    dispMB.dim[DMB_SMOOTH] = 0;
  }

  else if (epicMode == EM_SMOOTH) {
    dispMB.dim[DMB_RAW]    = 0;
    dispMB.dim[DMB_DITH]   = 1;
    dispMB.dim[DMB_SMOOTH] = 1;
  }
}


/************************************************************************/
int xvErrorHandler(disp, err)
     Display *disp;
     XErrorEvent *err;
{
  char buf[128];

  /* in case the error occurred during the Grab command... */
  XUngrabServer(theDisp);
  XUngrabButton(theDisp, (u_int) AnyButton, 0, rootW);

  xerrcode = err->error_code;

  /* non-fatal errors:   (sets xerrcode and returns)
   *    BadAlloc
   *    BadAccess errors on XFreeColors call
   *    Any error on the 'XKillClient()' call
   *    BadWindow errors (on a GetProperty call) (workaround SGI problem)
   *    BadLength errors on XChangeProperty
   *    BadMatch  errors on XGetImage
   */

  if ((xerrcode == BadAlloc)                                               ||
      (xerrcode == BadAccess && err->request_code==88 /* X_FreeColors */ ) ||
      (err->request_code == 113                       /* X_KillClient */ ) ||
      (xerrcode == BadLength && err->request_code==18 /* X_ChangeProp */ ) ||
      (xerrcode == BadMatch  && err->request_code==73 /* X_GetImage   */ ) ||
      (xerrcode == BadWindow && err->request_code==20 /* X_GetProperty*/))
    return 0;

  else {
    /* all other errors are 'fatal' */
    XGetErrorText(disp, xerrcode, buf, 128);
    fprintf(stderr,"X Error: %s\n",buf);
    fprintf(stderr,"  Major Opcode:  %d\n",err->request_code);

    if (DEBUG) {   /* crash 'n' burn for debugging purposes */
      char *str;
      str  = NULL;
      *str = '0';
    }

    exit(-1);
  }

  return 0;
}


/************************************************************************/
static void onInterrupt(i)
     int i;
{
  /* but first, if any input-grabbing popups are active, we have to 'cancel'
     them. */

  if (psUp) PSDialog(0);      /* close PS window */

#ifdef HAVE_JPEG
  if (jpegUp) JPEGDialog(0);  /* close jpeg window */
#endif

#ifdef HAVE_JP2K
  if (jp2kUp) JP2KDialog(0);  /* close jpeg 2000 window */
#endif

#ifdef HAVE_TIFF
  if (tiffUp) TIFFDialog(0);  /* close tiff window */
#endif

#ifdef HAVE_PNG
  if (pngUp) PNGDialog(0);    /* close png window */
#endif

  if (pcdUp) PCDDialog(0);    /* close pcd window */

#ifdef HAVE_PIC2
  if (pic2Up) PIC2Dialog(0);  /* close pic2 window */
#endif

#ifdef HAVE_PCD
  if (pcdUp)  PCDDialog(0);   /* close pcd window */
#endif

#ifdef HAVE_MGCSFX
  if (mgcsfxUp) MGCSFXDialog(0);  /* close mgcsfx window */
#endif

  ClosePopUp();

  /* make the interrupt signal look like a '\n' keypress in ctrlW */
  FakeKeyPress(ctrlW, XK_Return);

  frominterrupt = 1;
}





/***********************************/
static void Paint()
{
  Window  rW,cW;
  int     rx,ry, x,y, px,py, px1,py1, state;
  int     lx, ly, line, seenRelease;
  u_int   mask, nmask;

  /* paint pixels in either editCol (PIC8) or clear{R,G,B} (PIC24) until
     'shift' key is released.  beep on button presses other than B2.
     When shift is released, regen all pics (ala 'clearSelectedArea()') */


  if (!pen) {
    Pixmap      pix, pmask;
    XColor      cfg, cbg;

    cfg.red = cfg.green = cfg.blue = 0x0000;
    cbg.red = cbg.green = cbg.blue = 0xffff;

    pix = MakePix1(rootW, pen_bits,  pen_width,  pen_height);
    pmask= MakePix1(rootW, penm_bits, penm_width, penm_height);
    if (pix && pmask)
      pen = XCreatePixmapCursor(theDisp, pix, pmask, &cfg, &cbg,
				    pen_x_hot, pen_y_hot);
    if (pix)   XFreePixmap(theDisp, pix);
    if (pmask) XFreePixmap(theDisp, pmask);
  }

  if (pen) XDefineCursor(theDisp, mainW, pen);


  XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask
	       | StructureNotifyMask /* | ButtonPressMask */
	       | KeyReleaseMask | ColormapChangeMask
	       | EnterWindowMask | LeaveWindowMask );



  state = 0;
  line = lx = ly = seenRelease = 0;

  while (state<100) {
    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&rx,&ry,&x,&y,&mask)) continue;

    nmask = (~mask);
    px1 = px;  py1 = py;
    CoordE2P(x,y, &px, &py);

    switch (state) {
    case 0:               /* initial state:  make sure we do one pixel */
      px1 = lx = px;  py1 = ly = py;
      paintPixel(px, py);

      if      (nmask & ShiftMask  ) state = 99;
      else if (nmask & Button2Mask) state = 1;
      else if ( mask & ControlMask) state = 20;
      else                          state = 10;
      break;


    case 1:               /* waiting for click */
      if      (nmask & ShiftMask) state = 99;
      else if ( mask & Button2Mask) {
	paintPixel(px, py);
	if (mask & ControlMask) {
	  lx = px;  ly = py;
	  paintXLine(lx, ly, px, py, 1);
	  line = 1;
	  state = 20;
	}
	else state = 10;
      }
      break;


    case 10:               /* in freehand drawing mode */
      if      (nmask & ShiftMask  ) state = 99;
      else if (nmask & Button2Mask) state = 1;
      else if ( mask & ControlMask) {
	lx = px;  ly = py;
	paintXLine(lx, ly, px, py, 1);
	line = 1;
	state = 20;
      }
      else paintLine(px1,py1,px,py);
      break;


    case 20:               /* in line-drawing mode */
      if      (nmask & ShiftMask  ) state = 99;
      else if (nmask & ControlMask) {
	/* remove xor-line, switch to freehand drawing mode or click-wait */
	paintXLine(lx, ly, px1, py1, 0);
	line = 0;
	if (mask & Button2Mask) state = 10;
	                   else state = 1;
      }

      else if ((mask & Button2Mask) && seenRelease) {
	/* remove xor-line, draw line to pt, start xor-line from new pt */
	paintXLine(lx, ly, px1, py1, 0);
	paintLine (lx, ly, px1, py1);
	paintXLine(px1,py1,px,  py,  1);
	line = 1;
	lx = px1;  ly = py1;

	seenRelease = 0;
      }

      else {
	/* if moved, erase old xor-line, draw new xor-line */
	if (px != px1 || py != py1) {
	  paintXLine(lx, ly, px1, py1, 0);
	  paintXLine(lx, ly, px,  py,  1);
	  line = 1;
	}
	else {
	  paintXLine(lx, ly, px1, py1, 0);
	  paintXLine(lx, ly, px1, py1, 1);
	  XSync(theDisp, False);
	  Timer(100);
	}

	if (nmask & Button2Mask) seenRelease = 1;
      }
      break;

    case 99:              /* EXIT loop:  cleanup */
      if (line) { /* erase old xor-line */
	paintXLine(lx, ly, px1, py1, 0);
	line = 0;
      }
      state = 100;     /* exit while loop */
      break;
    }
  }


  WaitCursor();

  XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask
	       | StructureNotifyMask | ButtonPressMask
	       | KeyReleaseMask | ColormapChangeMask
	       | EnterWindowMask | LeaveWindowMask );

  GenerateCpic();
  WaitCursor();
  GenerateEpic(eWIDE,eHIGH);
  WaitCursor();
  DrawEpic();        /* redraws selection, also */
  SetCursors(-1);
}


/***********************/
static void paintPixel(x,y)
     int x,y;
{
  /* paints pixel x,y (pic coords) into pic in editColor (PIC8) or clearR,G,B
     (PIC24) and does appropriate screen feedback. */

  int ex,ey,ex1,ey1,ew,eh;

  if (!PTINRECT(x,y,0,0,pWIDE,pHIGH)) return;

  if (picType == PIC8) {
    pic[y * pWIDE + x] = editColor;
  }
  else {  /* PIC24 */
    byte *pp = pic + (y * pWIDE + x) * 3;
    pp[0] = clearR;  pp[1] = clearG;  pp[2] = clearB;
  }

  /* visual feedback */
  CoordP2E(x,   y,   &ex,  &ey);
  CoordP2E(x+1, y+1, &ex1, &ey1);

  ew = ex1-ex;  eh = ey1-ey;

  if (picType == PIC8) XSetForeground(theDisp, theGC, cols[editColor]);
  else XSetForeground(theDisp, theGC, RGBToXColor(clearR, clearG, clearB));

  if (ew>0 && eh>0)
    XFillRectangle(theDisp,mainW,theGC, ex,ey, (u_int) ew, (u_int) eh);
}


/***********************************/
static void paintLine(x,y,x1,y1)
  int x,y,x1,y1;
{
  int t,dx,dy,d,dd;

  dx = abs(x1-x);  dy = abs(y1-y);

  if (dx >= dy) {                       /* X is major axis */
    if (x > x1) {
       t = x; x = x1; x1 = t;
       t = y; y = y1; y1 = t;
     }
    d = dy + dy - dx;
    dd = y < y1 ? 1 : -1;
    while (x <= x1) {
      paintPixel(x,y);
      if (d > 0) {
        y += dd;
        d -= dx + dx;
      }
      ++x;
      d += dy + dy;
    }
  }

  else {                                /* Y is major axis */
    if (y > y1) {
       t = x; x = x1; x1 = t;
       t = y; y = y1; y1 = t;
     }
    d = dx + dx - dy;
    dd = x < x1 ? 1 : -1;
    while (y <= y1) {
      paintPixel(x,y);
      if (d > 0) {
        x += dd;
        d -= dy + dy;
      }
      ++y;
      d += dx + dx;
    }
  }


}


static int pntxlcol = 0;  /* index into xorMasks */

/***********************************/
static void paintXLine(x,y,x1,y1,newcol)
  int x,y,x1,y1,newcol;
{
  /* draws a xor'd line on image from x,y to x1,y1 (pic coords) */
  int ex,ey, ex1,ey1,  tx,ty,tx1,ty1;

  if (newcol) pntxlcol = (pntxlcol+1) & 0x7;

  CoordP2E(x,  y,  &tx, &ty);
  CoordP2E(x+1,y+1,&tx1,&ty1);
  ex = tx + (tx1 - tx)/2;
  ey = ty + (ty1 - ty)/2;

  CoordP2E(x1,  y1,  &tx, &ty);
  CoordP2E(x1+1,y1+1,&tx1,&ty1);
  ex1 = tx + (tx1 - tx)/2;
  ey1 = ty + (ty1 - ty)/2;

  if (ex==ex1 && ey==ey1) return;

  XSetPlaneMask(theDisp, theGC, xorMasks[pntxlcol]);
  XSetFunction(theDisp, theGC, GXinvert);
  XDrawLine(theDisp, mainW, theGC, ex, ey, ex1, ey1);
  XSetFunction(theDisp, theGC, GXcopy);
  XSetPlaneMask(theDisp, theGC, AllPlanes);
}


/***********************************/
static void BlurPaint()
{
  Window  rW,cW;
  int     rx,ry,ox,oy,x,y, px,py, done1, dragging;
  u_int   mask;

  /* blurs pixels in either editCol (PIC8) or clear{R,G,B} (PIC24) until
     'shift' key is released.  */


  /* if PIC8, uprev it to PIC24 */
  if (picType == PIC8) Select24to8MB(CONV24_24BIT);

  if (!blur) {
    Pixmap      pix, mask;
    XColor      cfg, cbg;

    cfg.red = cfg.green = cfg.blue = 0x0000;
    cbg.red = cbg.green = cbg.blue = 0xffff;

    pix = MakePix1(rootW, blur_bits,  blur_width,  blur_height);
    mask= MakePix1(rootW, blurm_bits, blurm_width, blurm_height);
    if (pix && mask)
      blur = XCreatePixmapCursor(theDisp, pix, mask, &cfg, &cbg,
				    blur_x_hot, blur_y_hot);
    if (pix)  XFreePixmap(theDisp, pix);
    if (mask) XFreePixmap(theDisp, mask);
  }

  if (blur) XDefineCursor(theDisp, mainW, blur);


  XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask
	       | StructureNotifyMask /* | ButtonPressMask */
	       | KeyReleaseMask | ColormapChangeMask
	       | EnterWindowMask | LeaveWindowMask );


  done1 = dragging = ox = oy = 0;
  while (1) {
    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&rx,&ry,&x,&y,&mask)) continue;
    if (done1 && !(mask & ShiftMask)) break;    /* Shift released */
    if (!(mask & Button3Mask)) { dragging = 0;  continue; }

    CoordE2P(x,y, &px, &py);

    if (!dragging || (dragging && (px!=ox || py!=oy))) {  /* click or drag */
      if (!dragging) blurPixel(px,py);
      else {
	int dx,dy,i,lx,ly;

	dx = px-ox;  dy = py-oy;   /* at least one will be non-zero */
	if (abs(dx) > abs(dy)) {   /* X is major axis */
	  for (i=0; i<=abs(dx); i++) {
	    lx = ox + (i * dx)/abs(dx);
	    ly = oy + (i * dy)/abs(dx);
	    blurPixel(lx,ly);
	  }
	} else {                   /* Y is major axis */
	  for (i=0; i<=abs(dy); i++) {
	    lx = ox + (i * dx)/abs(dy);
	    ly = oy + (i * dy)/abs(dy);
	    blurPixel(lx,ly);
	  }
	}
      }

      done1 = 1;  dragging = 1;  ox = px;  oy = py;
    }
  }

  WaitCursor();

  XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask
	       | StructureNotifyMask | ButtonPressMask
	       | KeyReleaseMask | ColormapChangeMask
	       | EnterWindowMask | LeaveWindowMask );

  GenerateCpic();
  WaitCursor();
  GenerateEpic(eWIDE,eHIGH);
  WaitCursor();
  DrawEpic();        /* redraws selection, also */
  SetCursors(-1);
}



/***********************/
static int highbit(ul)
     unsigned long ul;
{
  /* returns position of highest set bit in 'ul' as an integer (0-31),
     or -1 if none */

  int i;  unsigned long hb;

  hb = 0x80;  hb = hb << 24;   /* hb = 0x80000000UL */
  for (i=31; ((ul & hb) == 0) && i>=0;  i--, ul<<=1);
  return i;
}


/***********************/
static unsigned long RGBToXColor(r,g,b)
     int r,g,b;
{
  /* converts arbitrary rgb values (0-255) into an appropriate X color value,
     suitable for XSetForeground().  Works for ncols==0, all visual types,
     etc.  Note that it doesn't do the *best* job, it's really only for
     visual feedback during Painting, etc.  Should call GenEpic and such
     after modifying picture to redither, etc.  */

  unsigned long rv;

  if (picType == PIC8) {    /* simply find closest color in rgb map */
    int i,j,d,di;

    d = 3*(256*256);  j=0;
    for (i=0; i<numcols; i++) {
      di = ((r-rMap[i]) * (r-rMap[i])) +
	   ((g-gMap[i]) + (g-gMap[i])) +
           ((b-bMap[i]) * (b-bMap[i]));
      if (i==0 || di<d) { j=i;  d=di; }
    }

    rv = cols[j];
  }


  else {    /* PIC24 */
    if (theVisual->class==TrueColor || theVisual->class==DirectColor) {
      unsigned long rmask, gmask, bmask;
      int           rshift, gshift, bshift, cshift, maplen;

      /* compute various shifting constants that we'll need... */

      rmask = theVisual->red_mask;
      gmask = theVisual->green_mask;
      bmask = theVisual->blue_mask;

      rshift = 7 - highbit(rmask);
      gshift = 7 - highbit(gmask);
      bshift = 7 - highbit(bmask);

      if (theVisual->class == DirectColor) {
	maplen = theVisual->map_entries;
	if (maplen>256) maplen=256;
	cshift = 7 - highbit((u_long) (maplen-1));

	r = (u_long) directConv[(r>>cshift) & 0xff] << cshift;
	g = (u_long) directConv[(g>>cshift) & 0xff] << cshift;
	b = (u_long) directConv[(b>>cshift) & 0xff] << cshift;
      }


      /* shift the bits around */
      if (rshift<0) r = r << (-rshift);
      else r = r >> rshift;

      if (gshift<0) g = g << (-gshift);
      else g = g >> gshift;

      if (bshift<0) b = b << (-bshift);
      else b = b >> bshift;

      r = r & rmask;
      g = g & gmask;
      b = b & bmask;

      rv =r | g | b;
    }

    else {                          /* non-TrueColor/DirectColor visual */
      if (!ncols)
	rv = ((r + g + b >= 128*3) ? white : black);
      else                         /* use closest color in stdcmap */
	rv = stdcols[(r&0xe0) | ((g&0xe0)>>3) | ((b&0xc0) >> 6)];
    }
  }

  return rv;
}


/***********************/
static void blurPixel(x,y)
     int x,y;
{
  /* blurs pixel x,y (pic coords) into pic in editColor (PIC8) or clearR,G,B
     (PIC24) and does appropriate screen feedback.  Does a 3x3 average
     around the pixel, and replaces it with the average value (PIC24), or
     the closest existing color to the average value (PIC8) */

  byte *pp;
  int i, j, d, di;
  int ex,ey,ex1,ey1,ew,eh;
  int ar,ag,ab,ac;

  if (!PTINRECT(x,y,0,0,pWIDE,pHIGH)) return;

  ar = ag = ab = ac = 0;
  for (i=y-1; i<=y+1; i++) {
    for (j=x-1; j<=x+1; j++) {
      if (PTINRECT(j,i, 0,0,pWIDE,pHIGH)) {
	if (picType == PIC8) {
	  pp = pic + i * pWIDE + j;
	  ar += rMap[*pp];  ag += gMap[*pp];  ab += bMap[*pp];
	}
	else {
	  pp = pic + (i * pWIDE + j) * 3;
	  ar += pp[0];  ag += pp[1];  ab += pp[2];
	}
	ac++;
      }
    }
  }

  ar /= ac;  ag /= ac;  ab /= ac;


  if (picType == PIC8) {  /* find nearest actual color */
    d = 3*(256*256);  j=0;
    for (i=0; i<numcols; i++) {
      di = ((ar-rMap[i]) * (ar-rMap[i])) +
	   ((ag-gMap[i]) + (ag-gMap[i])) +
           ((ab-bMap[i]) * (ab-bMap[i]));
      if (i==0 || di<d) { j=i;  d=di; }
    }

    ac = j;
    pic[y * pWIDE + x] = ac;
  }
  else {  /* PIC24 */
    pp = pic + (y * pWIDE + x) * 3;
    pp[0] = ar;  pp[1] = ag;  pp[2] = ab;
  }

  /* visual feedback */
  CoordP2E(x,   y,   &ex,  &ey);
  CoordP2E(x+1, y+1, &ex1, &ey1);

  ew = ex1-ex;  eh = ey1-ey;

  if (picType == PIC8) XSetForeground(theDisp, theGC, cols[ac]);
  else XSetForeground(theDisp, theGC, RGBToXColor(ar, ag, ab));

  if (ew>0 && eh>0)
    XFillRectangle(theDisp,mainW,theGC, ex,ey, (u_int) ew, (u_int) eh);
}





/***********************/
static void annotatePic()
{
  int                i, w,h, len;
  byte              *cimg;
  char               txt[256];
  static char        buf[256] = {'\0'};
  static const char *labels[] = {"\nOk", "\033Cancel" };

  sprintf(txt, "Image Annotation:\n\n%s",
	  "Enter string to be placed on image.");

  i = GetStrPopUp(txt, labels, 2, buf, 256, "", 0);
  if (i==1 || strlen(buf)==0) return;


  /* build a 'cimg' array to be pasted on clipboard */
  w = strlen(buf) * 6 - 1;  h = 9;
  len = CIMG_PIC8 + w*h;

  cimg = (byte *) malloc((size_t) len);
  if (!cimg) {
    ErrPopUp("Error:  Unable to allocate memory for this operation.", "\nOk");
    return;
  }

  cimg[CIMG_LEN  ] =  len      & 0xff;
  cimg[CIMG_LEN+1] = (len>> 8) & 0xff;
  cimg[CIMG_LEN+2] = (len>>16) & 0xff;
  cimg[CIMG_LEN+3] = (len>>24) & 0xff;

  cimg[CIMG_W  ] =  w     & 0xff;
  cimg[CIMG_W+1] = (w>>8) & 0xff;

  cimg[CIMG_H  ] =  h     & 0xff;
  cimg[CIMG_H+1] = (h>>8) & 0xff;

  cimg[CIMG_24]    = 0;
  cimg[CIMG_TRANS] = 1;
  cimg[CIMG_TRVAL] = 0;

  xvbzero((char *) cimg + CIMG_PIC8, (size_t) w*h);

  if (picType == PIC8) {
    cimg[CIMG_CMAP + 3 + 0] = rMap[editColor];
    cimg[CIMG_CMAP + 3 + 1] = gMap[editColor];
    cimg[CIMG_CMAP + 3 + 2] = bMap[editColor];
  } else {
    cimg[CIMG_CMAP + 3 + 0] = clearR;
    cimg[CIMG_CMAP + 3 + 1] = clearG;
    cimg[CIMG_CMAP + 3 + 2] = clearB;
  }

  DrawStr2Pic(buf, w/2, h/2, cimg+CIMG_PIC8, w,h, 1);

  SaveToClip(cimg);
  free(cimg);

  /* if (HaveSelection()) EnableSelection(0); */
  DoImgPaste();
}



