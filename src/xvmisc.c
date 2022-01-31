/*
 * xvmisc.c - random 'handy' routines used in XV
 *
 *  Contains:
 *     Window CreateWindow(name, clname, geom, w, h, fg, bg, usesize)
 *     void   CenterString(win, str, x, y)
 *     void   ULineString(win, str, x, y)
 *     int    StringWidth(str)
 *     void   FakeButtonPress(bptr)
 *     void   FakeKeyPress(win, keysym);
 *     void   GenExpose(win, x, y, w, h);
 *     void   RemapKeyCheck(ks, buf, stlen)
 *     void   xvDestroyImage(XImage *);
 *     void   DimRect(win, x, y, w, h, bg);
 *     void   Draw3dRect(win, x,y,w,h, inout, bwidth, hicol, locol);
 *     void   SetCropString()
 *     void   SetSelectionString()
 *     void   Warning()
 *     void   FatalError(str)
 *     void   Quit(int)
 *     void   LoadFishCursors()
 *     void   WaitCursor()
 *     void   SetCursors(int)
 *     const char  *BaseName(const char *)
 *     void   DrawTempGauge(win, x,y,w,h, percent, fg,bg,hi,lo, str)
 *     void   ProgressMeter(min, max, val, str);
 *     void   xvbcopy(src, dst, length)
 *     int    xvbcmp (s1,  s2,  length)
 *     void   xvbzero(s, length)
 *     char  *xv_strstr(s1, s2)
 *     FILE  *xv_fopen(str, str)
 *     void   xv_mktemp(str)
 *     void   Timer(milliseconds)
 */

#include "copyright.h"

#define NEEDSTIME
#include "xv.h"

#ifdef __linux__	/* probably others, too, but being conservative */
#  include <unistd.h>	/* getwd() */
#endif

#include "bits/fc_left"
#include "bits/fc_leftm"
#include "bits/fc_left1"
#include "bits/fc_left1m"
#include "bits/fc_mid"
#include "bits/fc_midm"
#include "bits/fc_right1"
#include "bits/fc_right1m"
#include "bits/fc_right"
#include "bits/fc_rightm"

static void set_cursors PARM((Cursor, Cursor));

static Atom atom_DELWIN = 0;
static Atom atom_PROTOCOLS = 0;

/***************************************************/
void StoreDeleteWindowProp (win)
     Window win;
{
  if (! atom_DELWIN)
    atom_DELWIN = XInternAtom (theDisp, "WM_DELETE_WINDOW", FALSE);

  /* the following fakes 'XSetWMProtocols(theDisp, win, &atom_DELWIN, 1);' */

  if (! atom_PROTOCOLS)
    atom_PROTOCOLS = XInternAtom (theDisp, "WM_PROTOCOLS", False);

  if (atom_PROTOCOLS == None) return;

  XChangeProperty(theDisp, win, atom_PROTOCOLS, XA_ATOM, 32,
		  PropModeReplace, (unsigned char *) &atom_DELWIN, 1);
}


/***************************************************/
Window CreateWindow(name,clname,geom,defw,defh,fg,bg,usesize)
     const char   *name;
     const char   *clname;
     const char   *geom;
     int           defw,defh;
     unsigned long fg, bg;
     int           usesize;
{
  Window               win;
  XSetWindowAttributes xswa;
  unsigned long        xswamask;
  XWMHints             xwmh;
  XSizeHints           hints;
  int                  i,x,y,w,h;
  XClassHint           classh;

  /* note: if usesize, w,h are extracted from geom spec, otherwise
     defw,defh are used */

  x = y = 1;
  i = XParseGeometry(geom,&x,&y, (unsigned int *) &w, (unsigned int *) &h);

  if ((i&XValue || i&YValue)) hints.flags = USPosition;
                         else hints.flags = PPosition;

  if (!usesize || !(i&WidthValue))  w = defw;
  if (!usesize || !(i&HeightValue)) h = defh;

  hints.flags |= USSize | PWinGravity;

  hints.win_gravity = NorthWestGravity;
  if (i&XValue && i&XNegative) {
    hints.win_gravity = NorthEastGravity;
    x = dispWIDE - (w + 2 * bwidth) - abs(x);
  }
  if (i&YValue && i&YNegative) {
    hints.win_gravity = (hints.win_gravity == NorthWestGravity) ?
      SouthWestGravity : SouthEastGravity;
    y = dispHIGH - (h + 2 * bwidth) - abs(y);
  }


#define VROOT_TRANS
#ifdef VROOT_TRANS
  if (vrootW != rootW) { /* virtual window manager running */
    int x1,y1;
    Window child;
    XTranslateCoordinates(theDisp, rootW, vrootW, x, y, &x1, &y1, &child);
    if (DEBUG) fprintf(stderr,"translate:  %d,%d -> %d,%d\n",x,y,x1,y1);
    x = x1;  y = y1;
  }
#endif

  hints.x = x;                  hints.y = y;
  hints.width = w;              hints.height = h;

  if (!usesize) {
    hints.min_width  = w;         hints.min_height = h;
    hints.max_width  = w;         hints.max_height = h;
    hints.flags |= PMaxSize | PMinSize;
  }

  xswa.background_pixel = bg;
  xswa.border_pixel     = fg;
  xswa.colormap         = theCmap;
  xswa.bit_gravity      = StaticGravity;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;
  if (!usesize) xswamask |= CWBitGravity;

  win = XCreateWindow(theDisp, rootW, x, y, (u_int) w, (u_int) h,
		      (u_int) bwidth, (int) dispDEEP, InputOutput,
		      theVisual, xswamask, &xswa);
  if (!win) return(win);   /* leave immediately if couldn't create */


  xwmh.input = True;
  xwmh.flags = InputHint;
  if (iconPix) { xwmh.icon_pixmap = iconPix;  xwmh.flags |= IconPixmapHint; }

  if (clname && strlen(clname)) {
    classh.res_name = "xv";
    classh.res_class = (char *) clname;
    StoreDeleteWindowProp(win);
  }

  XmbSetWMProperties(theDisp, win, name, name, NULL, 0, &hints, &xwmh,
      clname ? &classh : NULL);

  return(win);
}



/**************************************************/
void DrawString(win,x,y,str)
     Window      win;
     int         x,y;
     const char *str;
{
  XDrawString(theDisp, win, theGC, x, y, str, (int) strlen(str));
}


/**************************************************/
void CenterString(win,x,y,str)
     Window      win;
     int         x,y;
     const char *str;
{
  DrawString(win, CENTERX(mfinfo, x, str), CENTERY(mfinfo, y), str);
}


/**************************************************/
void ULineString(win,x,y,str)
     Window      win;
     int         x,y;
     const char *str;
{
  DrawString(win, x, y, str);
  XDrawLine(theDisp, win, theGC, x, y+DESCENT-1,
	    x+StringWidth(str), y+DESCENT-1);
}


/**************************************************/
int StringWidth(str)
     const char *str;
{
  return(XTextWidth(mfinfo, str, (int) strlen(str)));
}


/**************************************************/
int CursorKey(ks, shift, dotrans)
     KeySym ks;
     int    shift, dotrans;
{
  /* called by the KeyPress/KeyRelease event handler to determine if a
     given keypress is a cursor key.  More complex than you'd think, since
     certain Sun Keyboards generate a variety of odd keycodes, and not all
     keyboards *have* all these keys.  Note that 'shifted' arrow keys
     are not translated to PageUp/Down, since it's up to the particular
     case whether that translation should occur */

  /* if we don't have the new R5-R6 'XK_KP_*' cursor movement
     symbols, make them. */

#ifndef XK_KP_Up
#  define XK_KP_Up         XK_Up
#  define XK_KP_Down       XK_Down
#  define XK_KP_Left       XK_Left
#  define XK_KP_Right      XK_Right
#  define XK_KP_Prior      XK_Prior
#  define XK_KP_Next       XK_Next
#  define XK_KP_Home       XK_Home
#  define XK_KP_End        XK_End
#endif



  int  i = CK_NONE;

  if      (ks==XK_Up    || ks==XK_KP_Up    ||
			   ks==XK_F28)             i=CK_UP;

  else if (ks==XK_Down  || ks==XK_KP_Down  ||
			   ks==XK_F34)             i=CK_DOWN;

  else if (ks==XK_Left  || ks==XK_KP_Left  ||
			   ks==XK_F30)             i=CK_LEFT;

  else if (ks==XK_Right || ks==XK_KP_Right ||
			   ks==XK_F32)             i=CK_RIGHT;

  else if (ks==XK_Prior || ks==XK_KP_Prior ||
			   ks==XK_F29)             i=CK_PAGEUP;

  else if (ks==XK_Next  || ks==XK_KP_Next  ||
			   ks==XK_F35)             i=CK_PAGEDOWN;

  else if (ks==XK_Home  || ks==XK_KP_Home  ||
			   ks==XK_F27)             i=CK_HOME;

  else if (ks==XK_End   || ks==XK_KP_End   ||
			   ks==XK_F33)             i=CK_END;

  else i = CK_NONE;

  if (dotrans && shift) {
    if      (i==CK_PAGEUP)   i=CK_HOME;
    else if (i==CK_PAGEDOWN) i=CK_END;
    else if (i==CK_UP)       i=CK_PAGEUP;
    else if (i==CK_DOWN)     i=CK_PAGEDOWN;
  }

  return i;
}


/***********************************/
void FakeButtonPress(bp)
BUTT *bp;
{
  /* called when a button keyboard equivalent has been pressed.
     'fakes' a ButtonPress event in the button, which A) makes the button
     blink, and B) falls through to ButtonPress command dispatch code */

  Window       rW, cW;
  int          x, y, rx, ry;
  unsigned int mask;
  XButtonEvent ev;

  ev.type = ButtonPress;
  ev.send_event = True;
  ev.display = theDisp;
  ev.window = bp->win;
  ev.root = rootW;
  ev.subwindow = (Window) NULL;
  ev.x = bp->x;
  ev.y = bp->y;
  ev.state = 0;
  ev.button = Button1;
  XSendEvent(theDisp, bp->win, False, NoEventMask, (XEvent *) &ev);

  /* if button1 is pressed, loop until RELEASED to avoid probs in BTTrack */
  while (XQueryPointer(theDisp, rootW, &rW,&cW,&rx,&ry,&x,&y,&mask)) {
    if (!(mask & Button1Mask)) break;    /* button released */
  }
}


/************************************************************************/
void FakeKeyPress(win, ksym)
     Window win;
     KeySym ksym;
{
  XKeyEvent ev;

  ev.type = KeyPress;
  ev.send_event = True;
  ev.display = theDisp;
  ev.window = win;
  ev.root = rootW;
  ev.subwindow = (Window) NULL;
  ev.time = CurrentTime;
  ev.x = ev.y = ev.x_root = ev.y_root = 0;
  ev.state = 0;
  ev.keycode = XKeysymToKeycode(theDisp, ksym);
  ev.same_screen = True;
  XSendEvent(theDisp, win, False, NoEventMask, (XEvent *) &ev);
  XFlush(theDisp);
}


/***********************************/
void GenExpose(win, x, y, w, h)
     Window       win;
     int          x, y;
     unsigned int w, h;
{
  /* generates an expose event on 'win' of the specified rectangle.  Unlike
     XClearArea(), it doesn't clear the rectangular region */

  XExposeEvent ev;

  ev.type = Expose;
  ev.send_event = True;
  ev.display = theDisp;
  ev.window = win;
  ev.x = x;  ev.y = y;  ev.width = w;  ev.height = h;
  ev.count = 0;

  XSendEvent(theDisp, win, False, NoEventMask, (XEvent *) &ev);
}


/***********************************/
void RemapKeyCheck(ks, buf, stlen)
     KeySym ks;
     char   *buf;
     int    *stlen;
{
  /* remap weirdo keysyms into normal key events */
  if (ks == 0x1000ff00) {  /* map 'Remove' key on DEC keyboards -> ^D */
    buf[0] = '\004';
    buf[1] = '\0';
    *stlen  = 1;
  }
}





/***********************************/
void xvDestroyImage(image)
     XImage *image;
{
  /* called in place of XDestroyImage().  Explicitly destroys *BOTH* the
     data and the structure.  XDestroyImage() doesn't seem to do this on all
     systems.  Also, can be called with a NULL image pointer */

  if (image) {
    /* free data by hand, since XDestroyImage is vague about it */
    if (image->data) free(image->data);
    image->data = NULL;
    XDestroyImage(image);
  }
}


/***********************************/
void DimRect(win, x, y, w, h, bg)
     Window win;
     int    x, y;
     u_int  w, h;
     u_long bg;
{
  /* stipple a rectangular region by drawing 'bg' where there's 1's
     in the stipple pattern */

  XSetFillStyle (theDisp, theGC, FillStippled);
  XSetStipple   (theDisp, theGC, dimStip);
  XSetForeground(theDisp, theGC, bg);
  XFillRectangle(theDisp, win,   theGC, x, y, (u_int) w, (u_int) h);
  XSetFillStyle (theDisp, theGC, FillSolid);
}



/**************************************************/
void Draw3dRect(win, x,y,w,h, inout, bwidth, hi, lo, bg)
     Window        win;
     int           x,y,inout,bwidth;
     unsigned int  w,h;
     unsigned long hi, lo, bg;
{
  int i, x1, y1;

  x1 = x + (int) w;
  y1 = y + (int) h;

  if (ctrlColor) {
    /* draw top-left */
    XSetForeground(theDisp, theGC, (inout==R3D_OUT) ? hi : lo);

    for (i=0; i<bwidth; i++) {
      XDrawLine(theDisp, win, theGC, x+i, y1-i, x+i,  y+i);
      XDrawLine(theDisp, win, theGC, x+i, y+i,  x1-i, y+i);
    }

    /* draw bot-right */
    XSetForeground(theDisp, theGC, (inout==R3D_OUT) ? lo : hi);

    for (i=0; i<bwidth; i++) {
      XDrawLine(theDisp, win, theGC, x+i+1, y1-i, x1-i, y1-i);
      XDrawLine(theDisp, win, theGC, x1-i,  y1-i, x1-i, y+i+1);
    }

    /* draw diagonals */
    XSetForeground(theDisp, theGC, bg);

    for (i=0; i<bwidth; i++) {
      XDrawPoint(theDisp, win, theGC, x+i,  y1-i);
      XDrawPoint(theDisp, win, theGC, x1-i, y+i);
    }
  }
}



/**************************************************/
void SetCropString()
{
  /* sets the crop string in the info box to be correct.  should
     be called whenever 'but[BCROP].active', cXOFF,cYOFF,cWIDE,cHIGH
     are changed */

  if (cpic != pic)
    SetISTR(ISTR_CROP, "%dx%d rectangle starting at %d,%d",
	    cWIDE, cHIGH, cXOFF, cYOFF);
  else SetISTR(ISTR_CROP, "<none>");
}


/**************************************************/
void SetSelectionString()
{
  /* sets the Selection string in the info box to be correct.  should
     be called whenever the selection may have changed */

  if (HaveSelection()) {
    int x,y,w,h;
    GetSelRCoords(&x,&y,&w,&h);
    SetISTR(ISTR_SELECT, "%dx%d rectangle starting at %d,%d", w, h, x, y);
  }
  else SetISTR(ISTR_SELECT, "<none>");
}


/***********************************/
void Warning()
{
  char *st;

  /* give 'em time to read message */
  if (infoUp || ctrlUp || anyBrowUp) sleep(3);
  else {
    st = GetISTR(ISTR_INFO);
    OpenAlert(st);
    sleep(3);
    CloseAlert();
  }
}


/***********************************/
void FatalError (identifier)
      const char *identifier;
{
  fprintf(stderr, "%s: %s\n",cmd, identifier);
  Quit(-1);
}


/***********************************/
void Quit(i)
     int i;
{
  /* called when the program exits.  frees everything explictly created
     EXCEPT allocated colors.  This is used when 'useroot' is in operation,
     as we have to keep the alloc'd colors around, but we don't want anything
     else to stay */

#ifdef AUTO_EXPAND
  chdir(initdir);
  Vdsettle();
#endif

  if (!theDisp) exit(i);   /* called before connection opened */

  if (useroot && i==0) {   /* save the root info */
    SaveRootInfo();

    /* kill the various windows, since we're in RetainPermanent mode now */
    if (dirW)  XDestroyWindow(theDisp, dirW);
    if (infoW) XDestroyWindow(theDisp, infoW);
    if (ctrlW) XDestroyWindow(theDisp, ctrlW);
    if (gamW)  XDestroyWindow(theDisp, gamW);

    KillBrowseWindows();

    if (psW)   XDestroyWindow(theDisp, psW);

#ifdef HAVE_JPEG
    if (jpegW) XDestroyWindow(theDisp, jpegW);
#endif

#ifdef HAVE_JP2K
    if (jp2kW) XDestroyWindow(theDisp, jp2kW);
#endif

#ifdef HAVE_TIFF
    if (tiffW) XDestroyWindow(theDisp, tiffW);
#endif

#ifdef HAVE_PNG
    if (pngW)  XDestroyWindow(theDisp, pngW);
#endif

#ifdef HAVE_PCD
    if (pcdW)  XDestroyWindow(theDisp, pcdW);
#endif

#ifdef HAVE_PIC2
    if (pic2W) XDestroyWindow(theDisp, pic2W);
#endif

#ifdef HAVE_MGCSFX
    if (mgcsfxW) XDestroyWindow(theDisp, mgcsfxW);
#endif

    /* if NOT using stdcmap for images, free stdcmap */
    if (colorMapMode != CM_STDCMAP) {
      int j;
      for (j=0; j<stdnfcols; j++)
	xvFreeColors(theDisp, theCmap, &stdfreecols[j], 1, 0L);
    }

    /* free browCmap, if any */
    if (browPerfect && browCmap) XFreeColormap(theDisp, browCmap);

    XFlush(theDisp);
  }

  KillPageFiles(pageBaseName, numPages);


  if (autoDelete) {  /* delete all files listed on command line */
    int j;  char str[MAXPATHLEN+1];

    for (j=0; j<orignumnames; j++) {
      if (origlist[j][0] != '/') {  /* relative path, prepend 'initdir' */
	sprintf(str,"%s/%s", initdir, origlist[j]);
	if (unlink(str)) {
	  fprintf(stderr,"%s: can't delete '%s' - %s\n",
		  cmd, str, ERRSTR(errno));
	}
      }
      else {
	if (unlink(origlist[j])) {
	  fprintf(stderr,"%s: can't delete '%s' - %s\n",
		  cmd, origlist[j], ERRSTR(errno));
	}
      }
    }
  }

  XSync(theDisp, False);
  exit(i);
}


static Cursor flcurs, fl1curs, fmcurs, fr1curs, frcurs;

/***********************************/
void LoadFishCursors()
{
#define fc_w 16
#define fc_h 16

  Pixmap flpix,flmpix,fmpix,fmmpix,frpix,frmpix;
  Pixmap fl1pix, fl1mpix, fr1pix, fr1mpix;
  XColor fg, bg;

  flcurs = fl1curs = fmcurs = fr1curs = frcurs = (Pixmap) NULL;

  flpix = XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_left_bits,
	     fc_w, fc_h, 1L, 0L, 1);
  flmpix= XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_leftm_bits,
	     fc_w, fc_h, 1L, 0L, 1);

  fl1pix = XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_left1_bits,
	     fc_w, fc_h, 1L, 0L, 1);
  fl1mpix= XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_left1m_bits,
	     fc_w, fc_h, 1L, 0L, 1);

  fmpix = XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_mid_bits,
	     fc_w, fc_h, 1L, 0L, 1);
  fmmpix= XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_midm_bits,
	     fc_w, fc_h, 1L, 0L, 1);

  fr1pix = XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_right1_bits,
	     fc_w, fc_h, 1L, 0L, 1);
  fr1mpix = XCreatePixmapFromBitmapData(theDisp, ctrlW,
					(char *) fc_right1m_bits,
	     fc_w, fc_h, 1L, 0L, 1);

  frpix = XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_right_bits,
	     fc_w, fc_h, 1L, 0L, 1);
  frmpix = XCreatePixmapFromBitmapData(theDisp, ctrlW, (char *) fc_rightm_bits,
	     fc_w, fc_h, 1L, 0L, 1);

  if (!flpix || !flmpix || !fmpix || !fmmpix || !frpix || !frmpix
      || !fl1pix || !fl1mpix || !fr1pix || !fr1mpix) return;

  fg.red = fg.green = fg.blue = 0;
  bg.red = bg.green = bg.blue = 0xffff;

  flcurs = XCreatePixmapCursor(theDisp, flpix, flmpix, &fg, &bg, 8,8);
  fl1curs= XCreatePixmapCursor(theDisp, fl1pix,fl1mpix,&fg, &bg, 8,8);
  fmcurs = XCreatePixmapCursor(theDisp, fmpix, fmmpix, &fg, &bg, 8,8);
  fr1curs= XCreatePixmapCursor(theDisp, fr1pix,fr1mpix,&fg, &bg, 8,8);
  frcurs = XCreatePixmapCursor(theDisp, frpix, frmpix, &fg, &bg, 8,8);

  if (!flcurs || !fmcurs || !frcurs || !fl1curs || !fr1curs)
    { flcurs = fmcurs = frcurs = (Cursor) NULL; }
}


static int    fishno=0;
static int    waiting=0;
static time_t lastwaittime;


/***********************************/
void WaitCursor()
{
  XWMHints xwmh;
  time_t   nowT;

  if (!waiting) {
    time(&lastwaittime);
    waiting=1;
    xwmh.input       = True;
    xwmh.icon_pixmap = riconPix;
    xwmh.icon_mask   = riconmask;
    xwmh.flags = (InputHint | IconPixmapHint | IconMaskHint) ;
    if (!useroot && mainW) XSetWMHints(theDisp, mainW, &xwmh);
    if ( useroot && ctrlW) XSetWMHints(theDisp, ctrlW, &xwmh);
  }
  else {    /* waiting */
    time(&nowT);
    if (nowT == lastwaittime) return;
    lastwaittime = nowT;
  }

  SetCursors(fishno);
  fishno = (fishno+1) % 8;
}


/***********************************/
void SetCursors(n)
     int n;
{
  Cursor c;
  XWMHints xwmh;

  c = cross;
  /* if n < 0   sets normal cursor in all windows
     n = 0..6   cycles through fish cursors */

  if (n<0) {
    if (waiting) {
      waiting=0;
      xwmh.input       = True;
      xwmh.icon_pixmap = iconPix;
      xwmh.icon_mask   = iconmask;
      xwmh.flags = (InputHint | IconPixmapHint | IconMaskHint) ;
      if (!useroot && mainW) XSetWMHints(theDisp, mainW, &xwmh);
      if ( useroot && ctrlW) XSetWMHints(theDisp, ctrlW, &xwmh);
    }

    if (showzoomcursor) set_cursors(zoom, arrow);
                   else set_cursors(arrow, arrow);
    fishno = 0;
  }

  else if (flcurs) {    /* was able to load the cursors */
    switch (n%8) {
    case 0: c = flcurs;   break;
    case 1: c = fl1curs;  break;
    case 2: c = fmcurs;   break;
    case 3: c = fr1curs;  break;
    case 4: c = frcurs;   break;
    case 5: c = fr1curs;  break;
    case 6: c = fmcurs;   break;
    case 7: c = fl1curs;  break;
    }

    set_cursors(c,c);
  }

  XFlush(theDisp);
}


static void set_cursors(mainc, otherc)
     Cursor mainc, otherc;
{
  if (!useroot && mainW) XDefineCursor(theDisp, mainW, mainc);
  if (infoW) XDefineCursor(theDisp, infoW, otherc);
  if (ctrlW) XDefineCursor(theDisp, ctrlW, otherc);
  if (dirW)  XDefineCursor(theDisp, dirW, otherc);
  if (gamW)  XDefineCursor(theDisp, gamW, otherc);
  if (psW)   XDefineCursor(theDisp, psW, otherc);

  SetBrowseCursor(otherc);
  SetTextCursor(otherc);

#ifdef HAVE_JPEG
  if (jpegW) XDefineCursor(theDisp, jpegW, otherc);
#endif

#ifdef HAVE_JP2K
  if (jp2kW) XDefineCursor(theDisp, jp2kW, otherc);
#endif 

#ifdef HAVE_TIFF
  if (tiffW) XDefineCursor(theDisp, tiffW, otherc);
#endif

#ifdef HAVE_PNG
  if (pngW)  XDefineCursor(theDisp, pngW, otherc);
#endif

#ifdef HAVE_PNG
  if (pngW)  XDefineCursor(theDisp, pngW, otherc);
#endif

#ifdef HAVE_PCD
  if (pcdW)  XDefineCursor(theDisp, pcdW, otherc);
#endif

#ifdef HAVE_PIC2
  if (pic2W) XDefineCursor(theDisp, pic2W, otherc);
#endif

#ifdef HAVE_MGCSFX
  if (mgcsfxW) XDefineCursor(theDisp, mgcsfxW, otherc);
#endif
}


/***************************************************/
const char *BaseName(fname)
     const char *fname;
{
  const char *basname;

  /* given a complete path name ('/foo/bar/weenie.gif'), returns just the
     'simple' name ('weenie.gif').  Note that it does not make a copy of
     the name, so don't be modifying it... */

  basname = (const char *) rindex(fname, '/');
  return basname? basname+1 : fname;
}


/***************************************************/
void DrawTempGauge(win, x,y,w,h, ratio, fg,bg,hi,lo, str)
     Window      win;
     int         x,y,w,h;
     double      ratio;
     u_long      fg,bg,hi,lo;
     const char *str;
{
  /* draws a 'temprature'-style horizontal progress meter in the specified
     window, at the specified location */

  int barwide, maxwide, numchars, tw;

  if (!win) return;

  numchars = tw = 0;
  if (str && str[0] != '\0') {
    numchars = (int) strlen(str);
    while ( (XTextWidth(mfinfo, str, numchars) > w) && numchars>0) numchars--;
  }
  if (numchars) tw = XTextWidth(mfinfo, str, numchars);

  XSetForeground(theDisp, theGC, fg);
  XDrawRectangle(theDisp, win, theGC, x,y, (u_int) w, (u_int) h);

  if (ctrlColor) {
    Draw3dRect(win, x+1,y+1, (u_int) w-2, (u_int) h-2, R3D_IN, 2, hi, lo, bg);
    maxwide = w-4;
    barwide = (int) (maxwide * ratio + 0.5);

    XSetForeground(theDisp, theGC, fg);
    XFillRectangle(theDisp,win,theGC, x+3, y+3, (u_int) barwide, (u_int) h-5);

    if (numchars) {      /* do string */
      if (barwide < maxwide) {
	XSetForeground(theDisp, theGC, bg);
	XFillRectangle(theDisp, win, theGC, x+3+barwide, y+3,
		       (u_int) (maxwide-barwide), (u_int) (h-5));
      }

      XSetFunction(theDisp, theGC, GXinvert);
      XSetPlaneMask(theDisp, theGC, fg ^ bg);

      XDrawString(theDisp, win, theGC, CENTERX(mfinfo, (x+w/2), str),
		  CENTERY(mfinfo, (y+h/2)), str, numchars);

      XSetFunction(theDisp, theGC, GXcopy);
      XSetPlaneMask(theDisp, theGC, AllPlanes);
    }

    else if (barwide < maxwide) {
      XDrawLine(theDisp,win,theGC,x+3+barwide, y+h/2 + 0, x+w-3, y+h/2 + 0);

      XSetForeground(theDisp, theGC, lo);
      XDrawLine(theDisp,win,theGC,x+3+barwide, y+h/2 + 1, x+w-3, y+h/2 + 1);

      XSetForeground(theDisp, theGC, hi);
      XDrawLine(theDisp,win,theGC,x+3+barwide, y+h/2 + 2, x+w-3, y+h/2 + 2);

      XSetForeground(theDisp, theGC, bg);
      XFillRectangle(theDisp, win, theGC, x+3+barwide, y+3,
		     (u_int) (maxwide-barwide), (u_int) (h/2 - 3));

      XFillRectangle(theDisp, win, theGC, x+3+barwide, y+h/2 + 3,
		     (u_int) (maxwide-barwide),(u_int)((h-3) - (h/2+3)) + 1);
    }
  }

  else {
    maxwide = w-1;
    barwide = (int) (maxwide * ratio + 0.5);

    XSetForeground(theDisp, theGC, fg);
    XFillRectangle(theDisp,win,theGC, x+1, y+1, (u_int) barwide, (u_int) h-1);

    if (numchars) {
      if (barwide < maxwide) {
	XSetForeground(theDisp, theGC, bg);
	XFillRectangle(theDisp, win, theGC, x+1+barwide, y+1,
		       (u_int) (maxwide-barwide), (u_int) (h-1));
      }

      XSetFunction(theDisp, theGC, GXinvert);
      XSetPlaneMask(theDisp, theGC, fg ^ bg);

      XDrawString(theDisp, win, theGC, CENTERX(mfinfo, (x+w/2), str),
		  CENTERY(mfinfo, (y+h/2)), str, numchars);

      XSetFunction(theDisp, theGC, GXcopy);
      XSetPlaneMask(theDisp, theGC, AllPlanes);
    }

    else if (barwide < maxwide) {
      XDrawLine(theDisp, win, theGC, x+1+barwide, y+h/2, x+w-1, y+h/2);

      XSetForeground(theDisp, theGC, bg);
      XFillRectangle(theDisp, win, theGC, x+1+barwide, y+1,
		     (u_int) (maxwide-barwide), (u_int) (h/2 - 1));

      XFillRectangle(theDisp, win, theGC, x+1+barwide, y+h/2 + 1,
		     (u_int)(maxwide-barwide),(u_int)(((h-1) - (h/2+1))+1));
    }
  }

  XFlush(theDisp);
}



/***************************************************/
void ProgressMeter(min, max, val, str)
     int         min, max, val;
     const char *str;
{
  /* called during 'long' operations (algorithms, smoothing, etc.) to
     give some indication that the program will ever finish.  Draws a
     temperature gauge in either mainW (if not useRoot), or ctrlW.
     Tries to be clever:  only draws gauge if it looks like the operation is
     going to take more than a few seconds.  Calling with val == max removes
     the gauge. */

  static int    waiting  = 0;
  static time_t lastTime = 0;
  time_t        nowTime;
  double        doneness;
  Window        win;
  int           xpos,ypos;

  if (useroot) { win=ctrlW;  xpos=10;  ypos=3; }
          else { win=mainW;  xpos=5;   ypos=5; }
  if (!win) return;

  if (min >= max) return;
  if (val==max && lastTime==0) return;  /* already erased meter */

  doneness = ((double) (val-min)) / ((double) (max - min));

  time(&nowTime);

  if (lastTime == 0) {   /* first time we're calling for this meter */
    lastTime = nowTime;
    waiting = 1;
  }
  else if (waiting) {    /* haven't drawn meter yet... */
    if ((nowTime - lastTime >= 2) && doneness < .5) waiting = 0;
  }

  if (!waiting) {     /* not waiting (or not waiting any longer) */
    if (nowTime == lastTime && val<max) return;  /* max one draw per second */
    lastTime = nowTime;
    DrawTempGauge(win, xpos, ypos, 100,19, doneness,
		    infofg,infobg,hicol,locol,str);

    if (val >= max) {            /* remove temp gauge */
      XClearArea(theDisp, win, xpos,ypos, 100+1,19+1, True);
      lastTime = 0;
    }
  }
}



/***************************************************/
void XVDeletedFile(fullname)
     char *fullname;
{
  /* called whenever a file has been deleted.  Updates browser & dir windows,
     if necessary */

  BRDeletedFile(fullname);
  DIRDeletedFile(fullname);
}


/***************************************************/
void XVCreatedFile(fullname)
     char *fullname;
{
  /* called whenever a file has been created.  Updates browser & dir windows,
     if necessary */

  BRCreatedFile(fullname);
  DIRCreatedFile(fullname);
}


/***************************************************/
void xvbcopy(src, dst, len)
     const char *src;
     char *dst;
     size_t  len;
{
  /* Modern OS's (Solaris, etc.) frown upon the use of bcopy(),
   * and only want you to use memcpy().  However, memcpy() is broken,
   * in the sense that it doesn't properly handle overlapped regions
   * of memory.  This routine does, and also has its arguments in
   * the same order as bcopy() did, without using bcopy().
   */

  /* determine if the regions overlap
   *
   * 3 cases:  src=dst, src<dst, src>dst
   *
   * if src=dst, they overlap completely, but nothing needs to be moved
   * if src<dst and src+len>dst then they overlap
   * if src>dst and src<dst+len then they overlap
   */

  if (src==dst || len<=0) return;    /* nothin' to do */

  if (src<dst && src+len>dst) {  /* do a backward copy */
    src = src + len - 1;
    dst = dst + len - 1;
    for ( ; len>0; len--, src--, dst--) *dst = *src;
  }

  else {  /* they either overlap (src>dst) or they don't overlap */
    /* do a forward copy */
    for ( ; len>0; len--, src++, dst++) *dst = *src;
  }
}


/***************************************************/
int xvbcmp (s1, s2, len)
     const char   *s1, *s2;
     size_t  len;
{
  for ( ; len>0; len--, s1++, s2++) {
    if      (*s1 < *s2) return -1;
    else if (*s1 > *s2) return 1;
  }
  return 0;
}

/***************************************************/
void xvbzero(s, len)
     char   *s;
     size_t  len;
{
  for ( ; len>0; len--) *s++ = 0;
}


/***************************************************/
void xv_getwd(buf, buflen)
     char   *buf;
     size_t  buflen;
{
  /* Gets the current working directory and puts it in buf.  No trailing '/'. */

  const char *rv;

#ifdef USE_GETCWD
  rv = (const char *) getcwd(buf, buflen);
#else
  rv = (const char *) getwd(buf);
#endif

  if (!rv || strlen(rv)==0) {
    if (((rv=(const char *) getenv("PWD"))==NULL) &&
	((rv=(const char *) getenv("cwd"))==NULL)) rv = "./";
    strcpy(buf, rv);
  }
#ifdef AUTO_EXPAND
  Vdtodir(buf);
#endif
}



/***************************************************/

/*
 *	Source code for the "strstr" library routine.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

char *xv_strstr(string, substring)
     const char *string;        /* String to search. */
     const char *substring;	/* Substring to try to find in string. */
{
  const char *a;
  const char *b;

  /* First scan quickly through the two strings looking for a
   * single-character match.  When it's found, then compare the
   * rest of the substring.
   */

  b = substring;
  if (*b == 0) return (char *) string;

  for ( ; *string != 0; string += 1) {
    if (*string != *b) continue;

    a = string;
    while (1) {
      if (*b == 0) return (char *) string;
      if (*a++ != *b++) break;
    }
    b = substring;
  }
  return (char *) 0;
}



/***************************************************/

/***************************************************/
FILE *xv_fopen(fname, mode)
     const char *fname;
     const char *mode;
{
  FILE *fp;

#ifndef VMS
  fp = fopen(fname, mode);
#else
  fp = fopen(fname, mode, "ctx=stm");
#endif

  return fp;
}


/***************************************************/
/* GRR 20050320:  added actual mk[s]temp() call... */
void xv_mktemp(buf, fname)
     char       *buf;
     const char *fname;
{
#ifndef VMS
  sprintf(buf, "%s/%s", tmpdir, fname);
#else
  sprintf(buf, "Sys$Disk:[]%s", fname);
#endif
#ifdef USE_MKSTEMP
  close(mkstemp(buf));
#else
  mktemp(buf);
#endif
}


/***************************************************/
void Timer(msec)   /* waits for 'n' milliseconds */
     int  msec;
{
  long usec;

  if (msec <= 0) return;

  usec = (long) msec * 1000;


#ifdef USLEEP
  usleep(usec);
  /* return */
#endif


#if defined(VMS) && !defined(USLEEP)
  {
    float ftime;
    ftime = msec / 1000.0;
    lib$wait(&ftime);
    /* return */
  }
#endif


#if defined(sgi) && !defined(USLEEP)
  {
    float ticks_per_msec;
    long ticks;
    ticks_per_msec = (float) CLK_TCK / 1000.0;
    ticks = (long) ((float) msec * ticks_per_msec);
    sginap(ticks);
    /* return */
  }
#endif


/* does SGI define SVR4?  not sure... */
#if (defined(SVR4) || defined(sco)) && !defined(sgi) && !defined(USLEEP)
  {
    struct pollfd dummy;
    poll(&dummy, 0, msec);
    /* return */
  }
#endif


#if !defined(USLEEP) && !defined(VMS) && !defined(sgi) && !defined(SVR4) && !defined(sco) && !defined(NOTIMER)
  {
    /* default/fall-through Timer() method now uses 'select()', which
     * probably works on all systems *anyhow* (except for VMS...) */

    struct timeval time;

    time.tv_sec = usec / 1000000L;
    time.tv_usec = usec % 1000000L;
    select(0, XV_FDTYPE NULL, XV_FDTYPE NULL, XV_FDTYPE NULL, &time);
    /* return */
  }
#endif


  /* NOTIMER case, fallthroughs, etc. ... but we return void, so who cares */
  /* return */
}

