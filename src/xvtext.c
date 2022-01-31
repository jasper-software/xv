/*
 *  xvtext.c  -  text file display window routines
 * 
 *  includes:
 *      void CreateTextWins(geom, cmtgeom);
 *      void OpenTextView(text, textlen, title, freeonclose);
 *      void OpenCommentText();
 *      void CloseCommentText();
 *      void ChangeCommentText();
 *      void HideTextWindows();
 *      void UnHideTextWindows();
 *      void RaiseTextWindows();
 *      void SetTextCursor(Cursor);
 *      void KillTextWindows();
 *      int  TextCheckEvent(evt, int *retval, int *done);
 *
 */

#include "copyright.h"

#include "xv.h"


#define BUTTW 80
#define BUTTH 24

#define TOPMARGIN 30       /* from top of window to top of text window */
#define BOTMARGIN (5+BUTTH+5) /* room for a row of buttons at bottom */
#define LRMARGINS 5        /* left and right margins */

#define MAXTVWIN    2      /* total # of windows */
#define MAXTEXTWIN  1      /* # of windows for text file viewing */
#define CMTWIN      1      /* cmtwin is reserved for image comments */

/* button/menu indicies */
#define TV_ASCII    0
#define TV_HEX      1
#define TV_CLOSE    2
#define TV_NBUTTS   3


#define TITLELEN 128

/* data needed per text window */
typedef struct {  Window win, textW;
		  int    vis, wasvis;
		  char  *text;             /* text to be displayed */
		  int    freeonclose;      /* free text when closing win */
		  int    textlen;          /* length of text */
		  char   title[TITLELEN];  /* name of file being displayed */
		  char **lines;            /* ptr to array of line ptrs */
		  int    numlines;         /* # of lines in text */
		  int    hexlines;         /* # of lines in HEX mode */
		  int    maxwide;          /* length of longest line (ascii) */
		  int    wide, high;       /* size of outer window (win)   */
		  int    twWide, twHigh;   /* size of inner window (textW) */
		  int    chwide, chhigh;   /* size of textW, in chars */
		  int    hexmode;          /* true if disp Hex, else Ascii */
		  SCRL   vscrl, hscrl;
		  BUTT   but[TV_NBUTTS], nopBut;
		} TVINFO;


static TVINFO   tinfo[MAXTVWIN];
static int      hasBeenSized = 0;
static int      haveWindows  = 0;
static int      mfwide, mfhigh, mfascent;   /* size of chars in mono font */
static int     *event_retP, *event_doneP;   /* used in tvChkEvent() */


static void closeText       PARM((TVINFO *));
static int  tvChkEvent      PARM((TVINFO *, XEvent *));
static void resizeText      PARM((TVINFO *, int, int));
static void computeScrlVals PARM((TVINFO *));
static void doCmd           PARM((TVINFO *, int));
static void drawTextView    PARM((TVINFO *));
static void drawNumLines    PARM((TVINFO *));
static void eraseNumLines   PARM((TVINFO *));
static void drawTextW       PARM((int, SCRL *));
static void clickText       PARM((TVINFO *, int, int));
static void keyText         PARM((TVINFO *, XKeyEvent *));
static void textKey         PARM((TVINFO *, int));
static void doHexAsciiCmd   PARM((TVINFO *, int));
static void computeText     PARM((TVINFO *));


/* HEXMODE output looks like this:
0x00000000: 00 11 22 33 44 55 66 77 - 88 99 aa bb cc dd ee ff  0123456789abcdef
0x00000010: 00 11 22 33 44 55 66 77 - 88 99 aa bb cc dd ee ff  0123456789abcdef
etc.
 */

/***************************************************************/
void CreateTextWins(geom, cmtgeom)
     char *geom, *cmtgeom;
{
  int                   i, defwide, defhigh, cmthigh;
  XSizeHints            hints;
  XSetWindowAttributes  xswa;
  TVINFO               *tv;
  int                   gx,gy,gw,gh,gset,gx1,gy1;
  

  mfwide = monofinfo->max_bounds.width;
  mfhigh = monofinfo->ascent + monofinfo->descent;
  mfascent = monofinfo->ascent;

  /* compute default size of textview windows.  should be big enough to
     hold an 80x24 text window */

  defwide = 80 * mfwide + 2*LRMARGINS + 8 + 20;   /* -ish */
  defhigh = 24 * mfhigh + TOPMARGIN + BOTMARGIN + 8 + 20;   /* ish */
  cmthigh = 6  * mfhigh + TOPMARGIN + BOTMARGIN + 8 + 20;   /* ish */

  /* creates *all* textview windows at once */

  for (i=0; i<MAXTVWIN; i++) tinfo[i].win = (Window) NULL;

  for (i=0; i<MAXTVWIN; i++) {
    tv = &tinfo[i];

    tv->win = CreateWindow((i<CMTWIN) ? "xv text viewer" : "xv image comments",
			   "XVtextview", 
			   (i<CMTWIN) ? geom : cmtgeom, 
			   defwide, 
			   (i<CMTWIN) ? defhigh : cmthigh, 
			   infofg, infobg, 1);
    if (!tv->win) FatalError("can't create textview window!");

    haveWindows = 1;
    tv->vis = tv->wasvis = 0;

    if (ctrlColor) XSetWindowBackground(theDisp, tv->win, locol);
              else XSetWindowBackgroundPixmap(theDisp, tv->win, grayTile);

    /* note: everything is sized and positioned in resizeText() */

    tv->textW = XCreateSimpleWindow(theDisp, tv->win, 1,1, 100,100, 
				     1,infofg,infobg);
    if (!tv->textW) FatalError("can't create textview text window!");

    SCCreate(&(tv->vscrl), tv->win, 0,0, 1,100, 0,0,0,0, 
	     infofg, infobg, hicol, locol, drawTextW);

    SCCreate(&(tv->hscrl), tv->win, 0,0, 0,100, 0,0,0,0, 
	     infofg, infobg, hicol, locol, drawTextW);

    if (XGetNormalHints(theDisp, tv->win, &hints)) 
      hints.flags |= PMinSize;
    else
      hints.flags = PMinSize;

    hints.min_width  = 380;
    hints.min_height = 200;
    XSetNormalHints(theDisp, tv->win, &hints);


#ifdef BACKING_STORE
    xswa.backing_store = WhenMapped;
    XChangeWindowAttributes(theDisp, tv->textW, CWBackingStore, &xswa);
#endif

    XSelectInput(theDisp, tv->textW, ExposureMask | ButtonPressMask);

    
    BTCreate(&(tv->but[TV_ASCII]), tv->win, 0,0,BUTTW,BUTTH,
	     "Ascii",infofg,infobg,hicol,locol);
    BTCreate(&(tv->but[TV_HEX]), tv->win, 0,0,BUTTW,BUTTH,
	     "Hex",infofg,infobg,hicol,locol);
    BTCreate(&(tv->but[TV_CLOSE]), tv->win, 0,0,BUTTW,BUTTH,
	     "Close",infofg,infobg,hicol,locol);

    BTCreate(&(tv->nopBut), tv->win, 0,0, (u_int) tv->vscrl.tsize+1,
	     (u_int) tv->vscrl.tsize+1, "", infofg, infobg, hicol, locol);
    tv->nopBut.active = 0;

    XMapSubwindows(theDisp, tv->win);

    tv->text = (char *) NULL;
    tv->textlen = 0;
    tv->title[0] = '\0';
  }


  for (i=0; i<MAXTVWIN; i++) {
    resizeText(&tinfo[i], defwide, (i<CMTWIN) ? defhigh : cmthigh);

    XSelectInput(theDisp, tinfo[i].win, ExposureMask | ButtonPressMask | 
		 KeyPressMask | StructureNotifyMask);
  }

  hasBeenSized = 1;  /* we can now start looking at textview events */

}


/***************************************************************/
void TextView(fname)
     char *fname;
{
  /* given a filename, attempts to read in the file and open a textview win */

  int   i;
  long  textlen;
  char *text, buf[512], title[128], rfname[MAXPATHLEN+1];
  char *basefname[128];  /* just current fname, no path */
  FILE *fp;

  basefname[0] = '\0';
  strcpy(rfname, fname);

  /* see if this file is compressed.  if it is, uncompress it, and view
     the uncompressed version */

  if (ReadFileType(fname) == RFT_COMPRESS) {
#ifndef VMS
    if (!UncompressFile(fname, rfname)) return;    /* failed to uncompress */
#else
    /* chop off trailing '.Z' from friendly displayed basefname, if any */
    strcpy (basefname, fname);
    *rindex (basefname, '.') = '\0';
    if (!UncompressFile(basefname, rfname)) return;/* failed to uncompress */
#endif
  }
      


  fp = fopen(rfname, "r");
  if (!fp) {
    sprintf(buf,"Couldn't open '%s':  %s", rfname, ERRSTR(errno));
    ErrPopUp(buf,"\nOh well");
    return;
  }


  fseek(fp, 0L, 2);
  textlen = ftell(fp);
  fseek(fp, 0L, 0);

  if (!textlen) {
    sprintf(buf, "File '%s' contains no data.  (Zero length file.)", rfname);
    ErrPopUp(buf, "\nOk");
    fclose(fp);
    return;
  }

  text = (char *) malloc((size_t) textlen);
  if (!text) {
    sprintf(buf, "Couldn't malloc %ld bytes to read file '%s'", 
	    textlen, rfname);
    ErrPopUp(buf, "\nSo what!");
    fclose(fp);
    return;
  }

  if (fread(text, (size_t) 1, (size_t) textlen, fp) != textlen) {
    sprintf(buf, "Warning:  Couldn't read all of '%s'.  Possibly truncated.",
	    rfname);
    ErrPopUp(buf, "\nHmm...");
  }

  fclose(fp);

  sprintf(title, "File: '%s'", BaseName(fname));
  OpenTextView(text, (int) textlen, title, 1);

  /* note:  text gets freed when window gets closed */
}

    
    
/***************************************************************/
void OpenTextView(text, len, title, freeonclose)
     char *text, *title;
     int   len,   freeonclose;
{
  /* opens up a textview window */

  int     i, oldone;
  TVINFO *tv;

  tv = &tinfo[0];

  /* kill off old text info */
  if (tv->freeonclose && tv->text) free(tv->text);
  if (tv->lines) free(tv->lines);
  tv->text = (char *) NULL;
  tv->lines = (char **) NULL;
  tv->numlines = tv->textlen = tv->hexmode = 0;


  tv->text        = text;
  tv->textlen     = len;
  tv->freeonclose = freeonclose;
  strncpy(tv->title, title, (size_t) TITLELEN-1);

  computeText(tv);      /* compute # lines and linestarts array */

  anyTextUp = 1;
  if (!tv->vis) XMapRaised(theDisp, tv->win);
  else {
    XClearArea(theDisp, tv->win, 0, 0, (u_int) tv->wide, (u_int) 30, False);
    drawTextView(tv);
  }
  tv->vis = 1;

  SCSetVal(&(tv->vscrl), 0);
  SCSetVal(&(tv->hscrl), 0);
  computeScrlVals(tv);
}



/***************************************************************/
void OpenCommentText()
{
  /* opens up the reserved 'comment' textview window */

  int     i;
  TVINFO *tv;

  tv = &tinfo[CMTWIN];
  commentUp = 1;
  XMapRaised(theDisp, tv->win);
  tv->vis = 1;

  ChangeCommentText();
}


/***************************************************************/
void CloseCommentText()
{
  /* closes the reserved 'comment' textview window */

  closeText(&tinfo[CMTWIN]);
  commentUp = 0;
}


/***************************************************************/
void ChangeCommentText()
{
  /* called when 'picComments' changes */

  TVINFO *tv;

  tv = &tinfo[CMTWIN];

  tv->text        = picComments;
  tv->textlen     = (tv->text) ? strlen(tv->text) : 0;
  tv->freeonclose = 0;

  if (strlen(fullfname)) 
    sprintf(tv->title, "File: '%s'", BaseName(fullfname));
  else 
    sprintf(tv->title, "<no file loaded>");

  computeText(tv);      /* compute # lines and linestarts array */

  if (tv->vis) {
    XClearArea(theDisp, tv->win, 0, 0, (u_int) tv->wide, (u_int) 30, False);
    drawTextView(tv);
  }

  SCSetVal(&(tv->vscrl), 0);
  SCSetVal(&(tv->hscrl), 0);

  computeScrlVals(tv);
}


/***************************************************************/
void HideTextWindows()
{
  int i;

  for (i=0; i<MAXTVWIN; i++) {
    if (tinfo[i].vis) {
      XUnmapWindow(theDisp, tinfo[i].win);
      tinfo[i].wasvis = 1;
      tinfo[i].vis = 0;
    }
  }
}


/***************************************************************/
void UnHideTextWindows()
{
  int i;

  for (i=0; i<MAXTVWIN; i++) {
    if (tinfo[i].wasvis) {
      XMapRaised(theDisp, tinfo[i].win);
      tinfo[i].wasvis = 0;
      tinfo[i].vis = 1;
    }
  }
}


/***************************************************************/
void RaiseTextWindows()
{
  int i;

  for (i=0; i<MAXTEXTWIN; i++) {
    if (tinfo[i].vis) {
      XRaiseWindow(theDisp, tinfo[i].win);
    }
  }
}


/***************************************************************/
void SetTextCursor(c)
     Cursor c;
{
  int i;

  for (i=0; i<MAXTVWIN; i++) {
    if (haveWindows && tinfo[i].win) XDefineCursor(theDisp, tinfo[i].win, c);
  }
}


/***************************************************************/
void KillTextWindows()
{
  int i;

  for (i=0; i<MAXTVWIN; i++) {
    if (haveWindows && tinfo[i].win) XDestroyWindow(theDisp, tinfo[i].win);
  }
}


/***************************************************************/
int TextCheckEvent(xev, retP, doneP)
     XEvent *xev;
     int *retP, *doneP;
{
  int i;

  event_retP  = retP;     /* so don't have to pass these all over the place */
  event_doneP = doneP;

  for (i=0; i<MAXTVWIN; i++) {
    if (tvChkEvent(&tinfo[i], xev)) break;
  }

  if (i<MAXTVWIN) return 1;
  return 0;
}


/***************************************************************/
int TextDelWin(win)
     Window win;
{
  /* got a delete window request.  see if the window is a textview window,
     and close accordingly.  Return 1 if event was eaten */

  int i;

  for (i=0; i<MAXTVWIN; i++) {
    if (tinfo[i].win == win) {
      closeText(&tinfo[i]);
      return 1;
    }
  }

  return 0;
}





/**************************************************************/
/***                    INTERNAL FUNCTIONS                  ***/
/**************************************************************/





/***************************************************************/
static void closeText(tv)
     TVINFO *tv;
{
  /* closes specified textview window */

  int i;

  XUnmapWindow(theDisp, tv->win);
  tv->vis = 0;

  for (i=0; i<MAXTEXTWIN && !tinfo[i].vis; i++);
  if (i==MAXTEXTWIN) anyTextUp = 0;

  /* free all info for this textview window */
  if (tv->freeonclose && tv->text)  free(tv->text);
  if (tv->lines) free(tv->lines);

  tv->text  = (char *) NULL;  
  tv->lines = (char **) NULL;
  tv->numlines = tv->textlen = tv->hexmode = 0;
}


/***************************************************************/
static int tvChkEvent(tv, xev)
     TVINFO *tv;
     XEvent *xev;
{
  /* checks event to see if it's a text-window related thing.  If it
     is, it eats the event and returns '1', otherwise '0'. */

  int i, rv;

  rv = 1;

  if (!hasBeenSized) return 0;  /* ignore evrythng until we get 1st Resize */

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    /* throw away excess redraws for 'dumb' windows */
    if (e->count > 0 && (e->window == tv->vscrl.win ||
			 e->window == tv->hscrl.win)) {}

    else if (e->window == tv->vscrl.win) SCRedraw(&(tv->vscrl));
    else if (e->window == tv->hscrl.win) SCRedraw(&(tv->hscrl));

    else if (e->window == tv->win || e->window == tv->textW) { /* smart wins */
      /* group individual expose rects into a single expose region */
      int           count;
      Region        reg;
      XRectangle    rect;
      XEvent        evt;

      xvbcopy((char *) e, (char *) &evt, sizeof(XEvent));
      reg = XCreateRegion();
      count = 0;

      do {
	if (DEBUG) fprintf(stderr,"   expose: %s %d,%d %dx%d\n",
			   (e->window == tv->win) ? "tv win" : "text win",
			   rect.x, rect.y, rect.width, rect.height);

	rect.x      = evt.xexpose.x;
	rect.y      = evt.xexpose.y;
	rect.width  = evt.xexpose.width;
	rect.height = evt.xexpose.height;
	XUnionRectWithRegion(&rect, reg, reg);
	count++;
      } while (XCheckWindowEvent(theDisp, evt.xexpose.window,
				 ExposureMask, &evt));
      
      XClipBox(reg, &rect);  /* bounding box of region */
      XSetRegion(theDisp, theGC, reg);

      if (DEBUG) {
	fprintf(stderr,"win = %lx, tv->win = %lx, textW = %lx\n",
		e->window, tv->win, tv->textW);
	fprintf(stderr,"grouped %d expose events into %d,%d %dx%d rect\n",
		count, rect.x, rect.y, rect.width, rect.height);
      }
      
      if      (e->window == tv->win)   drawTextView(tv);
      else if (e->window == tv->textW) drawTextW(0, &(tv->vscrl));

      XSetClipMask(theDisp, theGC, None);
      XDestroyRegion(reg);
    }

    else rv = 0;
  }


  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int i,x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == tv->win)       clickText(tv,x,y);
      else if (e->window == tv->vscrl.win) SCTrack(&(tv->vscrl),x,y);
      else if (e->window == tv->hscrl.win) SCTrack(&(tv->hscrl),x,y);
      else if (e->window == tv->textW) { }
      else rv = 0;
    }
    else rv = 0;
  }


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    if (e->window == tv->win) keyText(tv, e);
    else rv = 0;
  }


  else if (xev->type == ConfigureNotify) {
    XConfigureEvent *e = (XConfigureEvent *) xev;

    if (e->window == tv->win) {
      if (DEBUG)
	fprintf(stderr,"textview got a configure event (%dx%d)\n",
		e->width, e->height);

      if (tv->wide != e->width || tv->high != e->height) {
	if (DEBUG) fprintf(stderr,"Forcing a redraw!  (from configure)\n");
	XClearArea(theDisp, tv->win, 0, 0, 
		   (u_int) e->width, (u_int) e->height, True);
	resizeText(tv, e->width, e->height);
      }
    }
    else rv = 0;
  }
  else rv = 0;

  return rv;
}


/***************************************************************/
static void resizeText(tv,w,h)
     TVINFO *tv;
     int     w,h;
{
  int        i, maxw, maxh, hmax, hpage, vmax, vpage;
  XSizeHints hints;

  if (tv->wide == w && tv->high == h) return;  /* no change in size */

  if (XGetNormalHints(theDisp, tv->win, &hints)) {
    hints.width  = w;
    hints.height = h;
    hints.flags |= USSize;
    XSetNormalHints(theDisp, tv->win, &hints);
  }

  tv->wide = w;  tv->high = h;

  /* compute maximum size of text window */
  maxw = tv->wide - (2*LRMARGINS) - (tv->vscrl.tsize+1) - 2;
  maxh = tv->high - (TOPMARGIN + BOTMARGIN) - (tv->hscrl.tsize+1) - 2;

  tv->chwide = ((maxw - 6) / mfwide);
  tv->chhigh = ((maxh - 6) / mfhigh);

  tv->twWide = tv->chwide * mfwide + 6;
  tv->twHigh = tv->chhigh * mfhigh + 6;

  XMoveResizeWindow(theDisp, tv->textW, LRMARGINS, TOPMARGIN, 
		    (u_int) tv->twWide, (u_int) tv->twHigh);

  for (i=0; i<TV_NBUTTS; i++) {
    tv->but[i].x = tv->wide - (TV_NBUTTS-i) * (BUTTW+5);
    tv->but[i].y = tv->high - BUTTH - 5;
  }

  computeScrlVals(tv);

  tv->nopBut.x = LRMARGINS + tv->twWide + 1;
  tv->nopBut.y = TOPMARGIN + tv->twHigh + 1;
}




/***************************************************************/
static void computeScrlVals(tv)
     TVINFO *tv;
{
  int hmax, hpag, vmax, vpag;

  if (tv->hexmode) {
    hmax = 80 - tv->chwide;
    vmax = tv->hexlines - tv->chhigh;
  }
  else {   /* ASCII mode */
    hmax = tv->maxwide - tv->chwide;
    vmax = tv->numlines - tv->chhigh - 1;
  }

  hpag = tv->chwide / 4;
  vpag = tv->chhigh - 1;

  
  SCChange(&tv->vscrl, LRMARGINS + tv->twWide+1, TOPMARGIN, 
	   1, tv->twHigh, 0, vmax, tv->vscrl.val, vpag);

  SCChange(&tv->hscrl, LRMARGINS, TOPMARGIN + tv->twHigh + 1,
	   0, tv->twWide, 0, hmax, tv->hscrl.val, hpag);
}



/***************************************************************/
static void doCmd(tv, cmd)
     TVINFO *tv;
     int     cmd;
{
  switch (cmd) {
  case TV_ASCII:   doHexAsciiCmd(tv, 0);  break;
  case TV_HEX:     doHexAsciiCmd(tv, 1);  break;

  case TV_CLOSE:   if (tv == &tinfo[CMTWIN]) CloseCommentText();
                   else closeText(tv);
                   break;
  }
}



/***************************************************************/
static void drawTextView(tv)
     TVINFO *tv;
{
  /* redraw the outer window */

  int i, y;

  if (strlen(tv->title)) {    /* draw the title */
    y = 5;

    XSetForeground(theDisp, theGC, infobg);
    XFillRectangle(theDisp, tv->win, theGC, 5+1, y+1, 
		   (u_int) StringWidth(tv->title)+6, (u_int) CHIGH+4);

    XSetForeground(theDisp, theGC, infofg);
    XDrawRectangle(theDisp, tv->win, theGC, 5, y, 
		   (u_int) StringWidth(tv->title)+7, (u_int) CHIGH+5);

    Draw3dRect(tv->win, 5+1, y+1, (u_int) StringWidth(tv->title)+5,
	       (u_int) CHIGH+3, R3D_IN, 2, hicol, locol, infobg);

    XSetForeground(theDisp, theGC, infofg);
    DrawString(tv->win, 5+3, y+ASCENT+3, tv->title);
  }

  drawNumLines(tv);

  /* draw the buttons */
  for (i=0; i<TV_NBUTTS; i++) BTRedraw(&(tv->but[i]));

  BTRedraw(&tv->nopBut);
}


/***************************************************************/
static void drawNumLines(tv)
     TVINFO *tv;
{
  int x, y, w, nl;
  char tmpstr[128];

  if (tv->hexmode) nl = tv->hexlines;
  else {
    if (tv->numlines>0 && 
	tv->lines[tv->numlines-1] - tv->lines[tv->numlines-2] == 1) 
      nl = tv->numlines - 2;      /* line after last \n has zero length */
    else nl = tv->numlines - 1;
  }
  if (nl<0) nl = 0;

  sprintf(tmpstr, "%d byte%s, %d line%s", 
	  tv->textlen, (tv->textlen!=1) ? "s" : "", 
	  nl, (nl!=1) ? "s" : "");

  w = StringWidth(tmpstr) + 7;  /* width of frame */
  x = LRMARGINS + tv->twWide + tv->vscrl.tsize+1;     /* right align point */
  y = 6;

  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp, tv->win, theGC, (x-w)+1, y+1, 
		 (u_int) (w-1), (u_int) CHIGH+4);

  XSetForeground(theDisp, theGC, infofg);
  XDrawRectangle(theDisp, tv->win, theGC, x-w, y, (u_int) w, (u_int) CHIGH+5);

  Draw3dRect(tv->win, (x-w)+1, y+1, (u_int) (w-2), (u_int) CHIGH+3, 
	     R3D_IN,2,hicol,locol,infobg);

  XSetForeground(theDisp, theGC, infofg);
  DrawString(tv->win, (x-w)+3, y+ASCENT+3, tmpstr);
}


/***************************************************************/
static void eraseNumLines(tv)
     TVINFO *tv;
{
  int x, y, w, nl;
  char tmpstr[64];

  nl = (tv->hexmode) ? tv->hexlines : tv->numlines-1;                 

  sprintf(tmpstr, "%d byte%s, %d line%s", 
	  tv->textlen, (tv->textlen>1) ? "s" : "", 
	  nl, (nl>1) ? "s" : "");

  w = StringWidth(tmpstr) + 7;  /* width of frame */
  x = LRMARGINS + tv->twWide + tv->vscrl.tsize+1;     /* right align point */
  y = 5;

  XClearArea(theDisp, tv->win, x-w, y, (u_int) w+1, (u_int) CHIGH+7, False);
}


/***************************************************************/
static void drawTextW(delta, sptr)
     int   delta;
     SCRL *sptr;
{
  int     i, j, lnum, hpos, cpos, extrach, lwide;
  TVINFO *tv;
  char    linestr[512];
  u_char  *sp, *ep, *lp;

  /* figure out TVINFO pointer from SCRL pointer */
  for (i=0; i<MAXTVWIN && sptr != &tinfo[i].vscrl 
       && sptr != &tinfo[i].hscrl; i++);
  if (i==MAXTVWIN) return;   /* didn't find one */

  tv = &tinfo[i];

  /* make sure we've been sized.  Necessary, as creating/modifying the
     scrollbar calls this routine directly, rather than through 
     TextCheckEvent() */

  if (!hasBeenSized) return;

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);
  XSetFont(theDisp, theGC, monofont);

  hpos = tv->hscrl.val;
  lwide = (tv->chwide < 500) ? tv->chwide : 500;

  /* draw text */
  if (!tv->hexmode) {     /* ASCII mode */
    for (i=0; i<tv->chhigh; i++) {    /* draw each line */
      lnum = i + tv->vscrl.val;
      if (lnum < tv->numlines-1) {
	
	/* find start of displayed portion of line.  This is *wildly*
	   complicated by the ctrl-character and tab expansion... */

	sp = (byte *) tv->lines[lnum];
	ep = (byte *) tv->lines[lnum+1] - 1;  /* ptr to last disp ch in line */

	extrach = 0;

	for (cpos=0; cpos<hpos && sp<ep; cpos++) {
	  if (!extrach) {
	    if (*sp == '\011') {   /* tab to next multiple of 8 */
	      extrach = ((cpos+8) & (~7)) - cpos;
	      extrach--;
	    }
	    else if (*sp == '\015') {   /* ^M not displayed */
	      cpos--;  sp++;
	    }
	    else if (*sp < 32) extrach = 1;
	    else if (*sp > 127) extrach = 3;
	    else sp++;
	  }
	  else {
	    extrach--;
	    if (!extrach) sp++;
	  }
	}

	/* at this point, 'sp' is pointing to the first char to display.
	   if sp is a 'special' character, extrach is the # of chars
	   left to display of the 'expanded' version.  If sp>=ep, a blank
	   line should be printed */

	/* build up the linestr buffer, which is the current line, padded
	   with blanks to a width of exactly tv->chwide chars */
	for (cpos=0, lp=(byte *) linestr; cpos<lwide; cpos++, lp++) {
	  if (sp>=ep) *lp = ' ';
	  else {
	    if (*sp == '\011') {   /* tab to next multiple of 8 */
	      if (!extrach) extrach = ((cpos+hpos+8) & (~7)) - (cpos+hpos);

	      if (extrach) *lp = ' ';
	    }

	    else if (*sp == '\015') {  /* don't show ^M */
	      cpos--;  lp--;  sp++;
	    }

	    else if (*sp < 32) {
	      if (!extrach) extrach = 2;
	      if      (extrach == 2) *lp = '^';
	      else if (extrach == 1) *lp = *sp + 64;
	    }
      
	    else if (*sp > 127) {
	      if (!extrach) extrach = 4;
	      if      (extrach == 4) *lp = '\\';
	      else if (extrach == 3) *lp = ((u_char)(*sp & 0700) >> 6) + '0';
	      else if (extrach == 2) *lp = ((u_char)(*sp & 0070) >> 3) + '0';
	      else if (extrach == 1) *lp = ((*sp & 0007)) + '0';
	    }

	    else *lp = *sp++;

	    if (extrach) {
	      extrach--;
	      if (!extrach) sp++;
	    }
	  }
	}
      }

      else {  /* below bottom of file.  Just build a blank str */
	for (cpos = 0; cpos<lwide; cpos++) linestr[cpos]=' ';
      }

      /* draw the line */
      XDrawImageString(theDisp, tv->textW, theGC, 
		       3, i*mfhigh + 3 + mfascent, linestr, lwide);
    }  /* for i ... */
  }  /* if hexmode */


  else { /* HEX MODE */
    for (i=0; i<tv->chhigh; i++) {    /* draw each line */
      lnum = i + tv->vscrl.val;
      if (lnum < tv->hexlines) {
	
	char hexstr[80], tmpstr[16];

	/* generate hex for this line */
	sprintf(hexstr, "0x%08x: ", lnum * 0x10);

	sp = (byte *) tv->text + lnum * 0x10;
	ep = (byte *) tv->text + tv->textlen;      /* ptr to end of buffer */

	for (j=0; j<16; j++) {
	  if (sp+j < ep) sprintf(tmpstr,"%02x ", sp[j]);
	            else sprintf(tmpstr,"   ");
	  strcat(hexstr, tmpstr);

	  if (j==7) {
	    if (sp+8<ep) strcat(hexstr,"- ");
	            else strcat(hexstr,"  ");
	  }
	}
	strcat(hexstr," ");
	lp = (byte *) hexstr + strlen(hexstr);

	for (j=0; j<16; j++) {
	  if (sp+j < ep) {
	    if (sp[j] >= 32 && sp[j] <= 127) *lp++ = sp[j];
	    else *lp++ = '.';
	  }
	  else *lp++ = ' ';
	}
	*lp = '\0';


	/* at this point, 'hexstr' contains an 80 column hex thingy.
	   now build 'linestr', which is going to have hexstr shifted
	   and/or padded with blanks  (ie, the displayed portion or hexstr) */

	/* skip obscured beginning of line, if any */ 
	for (cpos=0, sp=(byte *) hexstr; cpos<hpos && *sp;  cpos++, sp++);

	for (cpos=0, lp=(byte *)linestr;  cpos<lwide; cpos++, lp++) {
	  if (*sp) { *lp = *sp++; }
	  else *lp = ' ';
	}
      }
      else {   /* below bottom of file.  just build blank str */
	for (cpos=0; cpos<lwide; cpos++) linestr[cpos]=' ';
      }

      /* draw the line */
      XDrawImageString(theDisp, tv->textW, theGC, 
		       3, i*mfhigh + 3 + mfascent, linestr, lwide);
    }  /* for i ... */
  }  /* else hexmode */
    


  XSetFont(theDisp, theGC, mfont);

  Draw3dRect(tv->textW, 0, 0, (u_int) (tv->twWide-1), (u_int) (tv->twHigh-1), 
	     R3D_IN, 2, hicol, locol, infobg);
}



/***************************************************************/
static void clickText(tv, x,y)
     TVINFO *tv;
     int     x,y;
{
  int   i;
  BUTT *bp;

  for (i=0, bp=tv->but; i<TV_NBUTTS; i++, bp++) {
    if (PTINRECT(x,y,bp->x,bp->y,bp->w,bp->h)) break;
  }

  if (i<TV_NBUTTS) {
    if (BTTrack(bp)) doCmd(tv, i);
    return;
  }
}




/***************************************************************/
static void keyText(tv, kevt)
     TVINFO    *tv;
     XKeyEvent *kevt;
{
  char buf[128];
  KeySym ks;
  int stlen, shift, dealt, ck;

  stlen = XLookupString(kevt, buf, 128, &ks, (XComposeStatus *) NULL);
  shift = kevt->state & ShiftMask;
  ck    = CursorKey(ks, shift, 1);
  dealt = 1;  

  RemapKeyCheck(ks, buf, &stlen);

  /* check for arrow keys, Home, End, PgUp, PgDown, etc. */
  if (ck!=CK_NONE) textKey(tv,ck);
  else dealt = 0;

  if (dealt || !stlen) return;

  /* keyboard equivalents */
  switch (buf[0]) {
  case '\001': doCmd(tv, TV_ASCII);   break;      /* ^A = Ascii */
  case '\010': doCmd(tv, TV_HEX);     break;      /* ^H = Hex   */

  case '\033': doCmd(tv, TV_CLOSE);   break;      /* ESC = Close window */

  default:     break;
  }
}


/***************************************************/
static void textKey(tv, key)
     TVINFO *tv;
     int     key;
{
  int i,j;

  if (!tv->textlen) return;

  /* an arrow key (or something like that) was pressed in icon window.
     change selection/scrollbar accordingly */

  if (key == CK_UP)     SCSetVal(&tv->vscrl, tv->vscrl.val - 1);
  if (key == CK_DOWN)   SCSetVal(&tv->vscrl, tv->vscrl.val + 1);

  if (key == CK_LEFT)   SCSetVal(&tv->hscrl, tv->hscrl.val - 1);
  if (key == CK_RIGHT)  SCSetVal(&tv->hscrl, tv->hscrl.val + 1);

  if (key == CK_PAGEUP)   SCSetVal(&tv->vscrl, tv->vscrl.val - tv->vscrl.page);
  if (key == CK_PAGEDOWN) SCSetVal(&tv->vscrl, tv->vscrl.val + tv->vscrl.page);
  if (key == CK_HOME)     SCSetVal(&tv->vscrl, tv->vscrl.min);
  if (key == CK_END)      SCSetVal(&tv->vscrl, tv->vscrl.max);
}


/***************************************************/
static void doHexAsciiCmd(tv, hexval)
     TVINFO *tv;
     int hexval;
{
  int i, oldvscrl, pos;

  if (hexval == tv->hexmode) return;    /* already ascii */
  eraseNumLines(tv);
  tv->hexmode = hexval;
  drawNumLines(tv);

  oldvscrl = tv->vscrl.val;

  /* compute vals, as width and length of text has changed */
  computeScrlVals(tv);

  /* try to show same area of file */
  if (hexval) {  /* switched to hex mode */
    if (oldvscrl < tv->numlines-1) {
      pos = tv->lines[oldvscrl] - tv->text;

      SCSetVal(&tv->vscrl, pos / 16);
    }
  }
  else {  /* switch to ascii mode */
    pos = oldvscrl * 16;
    for (i=0; i<tv->numlines-1; i++) {
      if (tv->lines[i+1] - tv->text > pos && 
	  tv->lines[i]   - tv->text <= pos) break;
    }
    if (i<tv->numlines-1) SCSetVal(&tv->vscrl, i);
  }

  drawTextW(0, &tv->vscrl);
}


/***************************************************/
static void computeText(tv)
     TVINFO *tv;
{
  /* compute # of lines and linestarts array for given text */

  int   i,j,wide,maxwide,space;
  byte *sp;

  if (!tv->text) { 
    tv->numlines = tv->hexlines = 0;  
    tv->lines = (char **) NULL; 
    return;
  }

  /* count the # of newline characters in text */
  for (i=0, sp=(byte *) tv->text, tv->numlines=0; i<tv->textlen; i++, sp++) {
    if (*sp == '\n') tv->numlines++;
  }

  /* +1 for start of line after last \n char, +1 to mark end of that line */
  tv->numlines += 2;

  /* build lines array */
  tv->lines = (char **) malloc(tv->numlines * sizeof(char *));
  if (!tv->lines) FatalError("out of memory in computeText()");

  j = 0;
  tv->lines[j++] = tv->text;
  for (i=0, sp=(byte *) tv->text; i<tv->textlen; i++, sp++) {
    if (*sp == '\n') tv->lines[j++] = (char *) (sp + 1);
  }

  tv->lines[tv->numlines - 1] = tv->text + tv->textlen + 1;

  /* each line has a trailing '\n' character, except for the last line, 
     which has a trailing '\0' character.  In any case, all lines can 
     be printed by printing ((lines[n+1] - lines[n]) - 1) characters,
     starting with lines[n].

     Note that there is one more lines[] entry than the actual # of lines,
     so as to mark the end of the last line in the same way as all the
     others */

  /* compute length of longest line, when shown in 'ascii' mode.  Takes
     into account the fact that non-printing chars (<32 or >127) will be
     shown in an 'expanded' form.  (<32 chars will be shown as '^A' 
     (or whatever), and >127 chars will be shown as octal '\275') */

  maxwide = 0;
  for (i=0; i<tv->numlines-1; i++) {
    /* compute displayed width of line #i */
    for (sp=(byte *) tv->lines[i], wide=0; sp<(byte *) tv->lines[i+1]-1; 
	 sp++) {
      if (*sp == '\011') {   /* tab to next multiple of 8 */
	space = ((wide+8) & (~7)) - wide;
	wide += space;
      }
      else if (*sp <  32) wide += 2;
      else if (*sp > 127) wide += 4;
      else wide++;
    }
    if (wide > maxwide) maxwide = wide;
  }
  tv->maxwide = maxwide;

  tv->hexlines = (tv->textlen + 15) / 16;
}


/**********************************************************************/
/* BUILT-IN TEXT FILES ************************************************/
/**********************************************************************/



static char license[10240];

/***************************************************************/
void ShowLicense()
{
  license[0] = '\0';

  /* build the license text */
#ifdef LC
#undef LC
#endif
#define LC(x) (strcat(license, x), strcat(license, "\n"))

  LC("(Note: This has been changed, and hopefully clarified, from the 3.00");
  LC("version of this info.  Please read it.)");
  LC("");
  LC("Thank you for acquiring a copy of XV, a pretty nifty X program.");
  LC("I hope you enjoy using it, as I've enjoyed writing it.");
  LC("");
  LC("The latest version of XV (or at least a pointer to it) is available");
  LC("via anonymous ftp on ftp.cis.upenn.edu, in the directory pub/xv.  If");
  LC("you're not sure if you have the latest version, or you are missing the");
  LC("source or documentation for XV, PLEASE pick up the latest version of");
  LC("the xv distribution.  Do *not* send mail unless absolutely necessary");
  LC("(ie, you don't have ftp capability).");
  LC("");
  LC("Note:  The docs (xvdocs.ps) may be installed in '/usr/local/lib'.");
  LC("");
  LC("If you're viewing this information via the 'About XV' command, and");
  LC("you'd like to print it out, a copy of this info can be found in the ");
  LC("README file in the top-level XV source directory.  Print that.  If you");
  LC("don't have it, see the previous paragraph.");
  LC("");
  LC("");
  LC("XV Licensing Information");
  LC("------------------------");
  LC("XV IS SHAREWARE FOR PERSONAL USE ONLY.  ");
  LC("");
  LC("You may use XV for your own amusement, and if you find it nifty,");
  LC("useful, generally cool, or of some value to you, your registration fee");
  LC("would be greatly appreciated.  $25 is the standard registration fee,");
  LC("though of course, larger amounts are quite welcome.  Folks who donate");
  LC("$40 or more can receive a printed, bound copy of the XV manual for no");
  LC("extra charge.  If you want one, just ask.  Be sure to specify the");
  LC("version of xv that you are using!");
  LC("");
  LC("COMMERCIAL, GOVERNMENT, AND INSTITUTIONAL USERS MUST REGISTER THEIR");
  LC("COPIES OF XV.");
  LC("");
  LC("This does *not* mean that you are required to register XV just because");
  LC("you play with it on the workstation in your office.  This falls under");
  LC("the heading of 'personal use'.  If you are a sysadmin, you can put XV");
  LC("up in a public directory for your users amusement.  Again, 'personal");
  LC("use', albeit plural.");
  LC("");
  LC("On the other hand, if you use XV in the course of doing your work,");
  LC("whatever your 'work' may happen to be, you *must* register your");
  LC("copy of XV.  (Note:  If you are a student, and you use XV to do");
  LC("classwork or research, you should get your professor/teacher/advisor");
  LC("to purchase an appropriate number of copies.)");
  LC("");
  LC("XV licenses are $25 each.  You should purchase one license per");
  LC("workstation, or one per XV user, whichever is the smaller number.  XV");
  LC("is *not* sold on a 'number of concurrent users' basis.  If XV was some");
  LC("$1000 program, yes, that would be a reasonable request, but at $25,");
  LC("it's not.  Also, given that XV is completely unlocked, there is no way");
  LC("to enforce any 'number of concurrent users' limits, so it isn't sold");
  LC("that way.");
  LC("");
  LC("Printed and bound copies of the 100-odd page XV manual are available");
  LC("for $15 each.  Note that manuals are *only* sold with, at minimum, an");
  LC("equal number of licenses.  (e.g.  if you purchase 5 licenses, you can");
  LC("also purchase *up to* 5 copies of the manual)");
  LC("");
  LC("The source code to the program can be had (as a compressed 'tar' file");
  LC("split over a couple 3.5\" MS-DOS formatted disks) for $15, for those");
  LC("who don't have ftp capabilities.");
  LC("");
  LC("Orders outside the US and Canada must add an additional $5 per manual");
  LC("ordered to cover the additional shipping charges.");
  LC("");
  LC("Checks, money orders, and purchase orders are accepted.  Credit cards");
  LC("are not.  All forms of payment must be payable in US Funds.  Checks");
  LC("must be payable through a US bank (or a US branch of a non-US bank).");
  LC("Purchase orders for less than $50, while still accepted, are not");
  LC("encouraged. ");
  LC("");
  LC("All payments should be payable to 'John Bradley', and mailed to:");
  LC("        John Bradley");
  LC("        1053 Floyd Terrace");
  LC("        Bryn Mawr, PA  19010");
  LC("        USA");
  LC("");
  LC("");
  LC("Site Licenses");
  LC("-------------");
  LC("If you are planning to purchase 10 or more licenses, site licenses are");
  LC("available, at a substantial discount.  Site licenses let you run XV on");
  LC("any and all computing equipment at the site, for any purpose");
  LC("whatsoever.  The site license covers the current version of XV, and");
  LC("any versions released within one year of the licensing date.  You are");
  LC("also allowed to duplicate and distribute an unlimited number of copies");
  LC("of the XV manual, but only for use within the site.  Covered versions");
  LC("of the software may be run in perpetuity.");
  LC("");
  LC("Also, it should be noted that a 'site' can be defined as anything");
  LC("you'd like.  It can be a physical location (a room, building,");
  LC("location, etc.), an organizational grouping (a workgroup, department,");
  LC("division, etc.) or any other logical grouping (\"the seventeen");
  LC("technical writers scattered about our company\", etc.).");
  LC("");
  LC("The site license cost will be based on your estimate of the number of");
  LC("XV users or workstations at your site, whichever is the smaller");
  LC("number.");
  LC("");
  LC("If you are interested in obtaining a site license, please contact the");
  LC("author via electronic mail or FAX (see below for details).  Send");
  LC("information regarding your site (the name or definition of the 'site',");
  LC("a physical address, a fax number, and an estimate of the number of");
  LC("users or workstations), and we'll get a site license out to you for");
  LC("your examination.");
  LC("");
  LC("");
  LC("Copyright Notice");
  LC("----------------");
  LC("XV is Copyright 1989, 1994 by John Bradley");
  LC("");
  LC("Permission to copy and distribute XV in its entirety, for");
  LC("non-commercial purposes, is hereby granted without fee, provided that");
  LC("this license information and copyright notice appear in all copies.");
  LC("");
  LC("If you redistribute XV, the *entire* contents of this distribution");
  LC("must be distributed, including the README, and INSTALL files, the");
  LC("sources, and the complete contents of the 'docs' directory.");
  LC("");
  LC("Note that distributing XV 'bundled' in with any product is considered");
  LC("to be a 'commercial purpose'.");
  LC("");
  LC("Also note that any copies of XV that are distributed MUST be built");
  LC("and/or configured to be in their 'unregistered copy' mode, so that it");
  LC("is made obvious to the user that XV is shareware, and that they should");
  LC("consider registering, or at least reading this information.");
  LC("");
  LC("The software may be modified for your own purposes, but modified");
  LC("versions may not be distributed without prior consent of the author.");
  LC("");
  LC("This software is provided 'as-is', without any express or implied");
  LC("warranty.  In no event will the author be held liable for any damages");
  LC("arising from the use of this software.");
  LC("");
  LC("If you would like to do something with XV that this copyright");
  LC("prohibits (such as distributing it with a commercial product, using");
  LC("portions of the source in some other program, distributing registered");
  LC("copies, etc.), please contact the author (preferably via email).");
  LC("Arrangements can probably be worked out.");
  LC("");
  LC("");
  LC("The author may be contacted via:");
  LC("    US Mail:  John Bradley");
  LC("              1053 Floyd Terrace");
  LC("              Bryn Mawr, PA  19010");
  LC("");
  LC("    FAX:     (610) 520-2042");
  LC("");
  LC("Electronic Mail regarding XV should be sent to one of these addresses:");
  LC("     xv@devo.dccs.upenn.edu               - general XV questions");
  LC("     xvbiz@devo.dccs.upenn.edu            - all XV licensing questions");
  LC("     xvtech@devo.dccs.upenn.edu           - bug reports, technical questions");
  LC("");
  LC("Please do *not* send electronic mail directly to the author, as he");
  LC("gets more than enough as it is.");
  LC("");

#undef LC

  OpenTextView(license, (int) strlen(license), "About XV", 0);
}



static char keyhelp[10240];

/***************************************************************/
void ShowKeyHelp()
{
  keyhelp[0] = '\0';

#undef LC
#define LC(x) (strcat(keyhelp, x), strcat(keyhelp, "\n"))

  LC("XV Mouse and Keyboard Usage");
  LC("===========================");
  LC("");
  LC("Part 1:  Mouse Usage in the Image Window");
  LC("----------------------------------------");
  LC("                 Button1 - draws a selection rectangle");
  LC("                 Button2 - pixel values, measures dist., picks color");
  LC("                 Button3 - opens/closes the 'xv controls' window");
  LC("");
  LC("          ctrl + Button1 - zooms in");
  LC("          ctrl + Button2 - pans");
  LC("          ctrl + Button3 - zooms out");
  LC("");
  LC("  shift        + Button1 - draws a square selection rectangle");
  LC("  shift        + Button2 - freehand drawing tool");
  LC("  shift + ctrl + Button2 - draws lines");
  LC("  shift        + Button3 - smudging tool");
  LC("  ");
  LC("Part 1a:  Mouse Usage in Selection Rectangle");
  LC("--------------------------------------------");
  LC("                 Button1 - moves selection rectangle");
  LC("  shift        + Button1 - moves selection, constrains motion");
  LC("                 Button2 - 'drag-and-drop' cut and paste");
  LC("  shift        + Button2 - 'drag-and-drop' cut and paste, constrain");
  LC("          ctrl + Button2 - 'drag-and-drop' copy and paste");
  LC("  shift + ctrl + Button2 - 'drag-and-drop' copy and paste, constrain");
  LC("");
  LC("");
  LC("");
  LC("Part 2:  Normal Keyboard Equivalents");
  LC("------------------------------------");
  LC("The following keys can be used in most xv windows, including the ");
  LC("image, controls, and color editor windows, but *not* in the visual");
  LC("schnauzer windows.");
  LC("");
  LC("  Tab or ");
  LC("  Space         - 'Next' command");
  LC("");
  LC("  Return        - reload currently displayed image file");
  LC("");
  LC("  Del or ");
  LC("  Backspace     - 'Prev' command");
  LC("");
  LC("  ctrl+'l'      - 'Load' command");
  LC("  ctrl+'s'      - 'Save' command");
  LC("  ctrl+'p'      - 'Print' command");
  LC("  ctrl+'d'      - 'Delete' command");
  LC("");
  LC("  'q' or ");
  LC("  ctrl+'q'      - 'Quit' command");
  LC("");
  LC("  meta+'x'      - 'cut' command");
  LC("  meta+'c'      - 'copy' command");
  LC("  meta+'v'      - 'paste' command");
  LC("  meta+'d'      - 'clear' command");
  LC("");
  LC("  'n'           - reset image to normal (unexpanded) size");
  LC("  'm'           - maximum image size");
  LC("  'M'           - maximum image size, maintaining aspect ratio");
  LC("  '>'           - double image size");
  LC("  '<'           - half image size");
  LC("  '.'           - make image 10% larger");
  LC("  ','           - make image 10% smaller");
  LC("  'S'           - set image to specified size/expansion");
  LC("  'a'           - reset image to normal aspect ratio");
  LC("  '4'           - make image have a 4x3 width/height ratio");
  LC("  'I'           - round image size to integer expand/compres ratios");
  LC("");
  LC("  't'           - turn image 90 degrees clockwise");
  LC("  'T'           - turn image 90 degrees counter-clockwise");
  LC("  'h'           - flip image horizontally");
  LC("  'v'           - flip image vertically");
  LC("");
  LC("  'P'           - pad image");
  LC("  'A'           - image annotation");
  LC("  'c'           - 'Crop' command");
  LC("  'u'           - 'UnCrop' command");
  LC("  'C'           - 'AutoCrop' command");
  LC("");
  LC("  'r'           - raw mode");
  LC("  'd'           - dithered mode");
  LC("  's'           - smooth mode");
  LC("  meta+'8'      - toggle 8/24 bit mode");
  LC("");
  LC("  'V' or ");
  LC("  ctrl+'v'      - Visual Schnauzer");
  LC("  'e'           - Color Editor");
  LC("  'i'           - Image Info");
  LC("  ctrl+'c'      - Image Comments");
  LC("  ctrl+'t'      - Text View");
  LC("");
  LC("  ctrl+'g'      - 'Grab' command");
  LC("  ctrl+'a'      - 'About XV' command");
  LC("");
  LC("  meta+'b'      - Blur algorithm");
  LC("  meta+'s'      - Sharpen algorithm");
  LC("  meta+'e'      - Edge Detection algorithm");
  LC("  meta+'m'      - Emboss algorithm");
  LC("  meta+'o'      - Oil Paint algorithm");
  LC("  meta+'B'      - Blend algorithm");
  LC("  meta+'t'      - Copy rotate algorithm");
  LC("  meta+'T'      - Clear rotate algorithm");
  LC("  meta+'p'      - Pixelize algorithm");
  LC("  meta+'S'      - Spread algorithm");
  LC("");
  LC("  'R' or");
  LC("  meta+'r' or ");
  LC("  meta+'0'      - 'Reset' command in color editor");
  LC("");
  LC("  meta+'1'      - select preset 1 in color editor");
  LC("  meta+'2'      - select preset 2 in color editor");
  LC("  meta+'3'      - select preset 3 in color editor");
  LC("  meta+'4'      - select preset 4 in color editor");
  LC("  meta+'a'      - 'Apply' command in color editor");
  LC("");
  LC("");
  LC("");
  LC("Part 2a:  Image Window Keys");
  LC("---------------------------");
  LC("The following keys can *only* be used inside the image window.");
  LC("");
  LC("  ctrl + Up     - crops 1 pixel off the bottom of the image");
  LC("  ctrl + Down   - crops 1 pixel off the top of the image");
  LC("  ctrl + Left   - crops 1 pixel off the right side of the image");
  LC("  ctrl + Right  - crops 1 pixel off the left side of the image");
  LC("");
  LC("  If you're viewing a multi-page document:");
  LC("  'p'           -  opens a 'go to page #' dialog box");
  LC("");
  LC("  PageUp, or");
  LC("  Prev, or ");
  LC("  shift+Up      - previous page");
  LC("");
  LC("  PageDown, or");
  LC("  Next, or ");
  LC("  shift+Down    - next page");
  LC("");
  LC("");
  LC("  If a selection rectangle is active");
  LC("  Up            - move rectangle up 1 pixel");
  LC("  Down          - move rectangle down 1 pixel");
  LC("  Left          - move rectangle left 1 pixel");
  LC("  Right         - move rectangle right 1 pixel");
  LC("  shift+Up      - shrink rectangle vertically by 1 pixel");
  LC("  shift+Down    - expand rectangle vertically by 1 pixel");
  LC("  shift+Left    - shrink rectangle horizontally by 1 pixel");
  LC("  shift+Right   - expand rectangle horizontally by 1 pixel");
  LC("");
  LC("");
  LC("Part 2b:  Visual Schnauzer Keys");
  LC("-------------------------------");
  LC("The following keys can only be used in the visual schnauzer windows.");
  LC("");
  LC("  ctrl+'d'      - delete file(s)");
  LC("  ctrl+'n'      - create new directory");
  LC("  ctrl+'r'      - rename file");
  LC("  ctrl+'s'      - rescan directory");
  LC("  ctrl+'w'      - open new window");
  LC("  ctrl+'u'      - update icons");
  LC("  ctrl+'g'      - generate icons for selected files");
  LC("  ctrl+'a'      - select all files");
  LC("  ctrl+'t'      - view selected file as text");
  LC("  ctrl+'q'      - quit XV");
  LC("  ctrl+'c'      - change directory");
  LC("  ctrl+'f'      - select filenames");
  LC("  ctrl+'e'      - recursive update");
  LC("  Esc           - close window");
  LC("  Return        - load currently selected file(s)");
  LC("  Space         - load next file");
  LC("  shift+Space   - load next file, keeping previous file(s) selected");
  LC("  Backspace     - load previous file");

#undef LC
  OpenTextView(keyhelp, (int) strlen(keyhelp), "XV Help", 0);
}






