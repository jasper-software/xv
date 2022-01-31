/*
 * xvgraf.c - GRAF window handling functions
 *
 * callable functions:
 *
 *   CreateGraf()       -  creates a GRAF window
 *   InitGraf()         -  inits GRAF handles to reasonable defaults
 *   RedrawGraf()       -  called by 'expose' events
 *   ClickGraf()        -  called when B1 clicked in GRAF window
 *   GrafKey()          -  called when keypress in GRAF window
 *   Graf2Str()         -  copies current GRAF settings to a string
 *   Str2Graf()         -  parses an xrdb string into GRAF settings
 *   GetGrafState()     -  copies GRAF data into GRAF_STATE structure
 *   SetGrafState()     -  sets GRAF data based on GRAF_STATE
 *   InitSpline()       -  called to generate y' table for EvalSpline
 *   EvalSpline()       -  evalutes spline function at given point
 */

#include "copyright.h"

#include "xv.h"

#include "bits/gf1_addh"
#include "bits/gf1_delh"
#include "bits/gf1_line"
#include "bits/gf1_spln"
#include "bits/gf1_rst"
#include "bits/gf1_gamma"


#define GHIGH 147
#define GBHIGH 23
#define GBWIDE 30
#define GWIDE (128 + 3 + 4 + GBWIDE)

#define PW gf1_addh_width
#define PH gf1_addh_height


static int    pixmaps_built = 0;
static Pixmap gfbpix[N_GFB];


static void drawGraf    PARM((GRAF *, int));
static void drawHandPos PARM((GRAF *, int));



/***************************************************/
void CreateGraf(gp, parent, x, y, fg, bg, title)
     GRAF *gp;
     Window parent;
     int x,y;
     unsigned long fg,bg;
     const char *title;
{
  /* NOTE:  CreateGraf does not initialize hands[], nhands, or spline,
     as these could be initialized by X resources (or whatever),
     which takes place long before we can create the windows.

     InitGraf() sets those fields of a GRAF to likely enough values */

  int i;

  if (!pixmaps_built) {
    gfbpix[GFB_ADDH]   = MakePix1(parent, gf1_addh_bits, PW, PH);
    gfbpix[GFB_DELH]   = MakePix1(parent, gf1_delh_bits, PW, PH);
    gfbpix[GFB_LINE]   = MakePix1(parent, gf1_line_bits, PW, PH);
    gfbpix[GFB_SPLINE] = MakePix1(parent, gf1_spln_bits, PW, PH);
    gfbpix[GFB_RESET]  = MakePix1(parent, gf1_rst_bits,  PW, PH);
    gfbpix[GFB_GAMMA]  = MakePix1(parent, gf1_gamma_bits,
				  gf1_gamma_width, gf1_gamma_height);

    for (i=0; i<N_GFB && gfbpix[i] != (Pixmap) NULL; i++);
    if (i<N_GFB) FatalError("can't create graph pixmaps");

    pixmaps_built = 1;
  }

  gp->fg     = fg;
  gp->bg     = bg;
  gp->str    = title;
  gp->entergamma = 0;
  gp->drawobj = NULL;

  sprintf(gp->gvstr, "%.5g", gp->gamma);

  gp->win = XCreateSimpleWindow(theDisp, parent, x,y, GWIDE, GHIGH, 1, fg,bg);
  if (!gp->win) FatalError("can't create graph (main) window");

  gp->gwin = XCreateSimpleWindow(theDisp, gp->win, 2, GHIGH-132,
				 128, 128, 1, fg,bg);
  if (!gp->gwin) FatalError("can't create graph (sub) window");

  for (i=0; i<N_GFB; i++) {
    BTCreate(&gp->butts[i], gp->win, GWIDE-GBWIDE-2, 1+i * (GBHIGH + 1),
	     GBWIDE, GBHIGH, (char *) NULL, fg, bg, hicol, locol);
    gp->butts[i].pix = gfbpix[i];
    gp->butts[i].pw = PW;
    gp->butts[i].ph = PH;
  }

  gp->butts[GFB_SPLINE].toggle = 1;
  gp->butts[GFB_LINE  ].toggle = 1;

  gp->butts[GFB_SPLINE].lit =  gp->spline;
  gp->butts[GFB_LINE  ].lit = !gp->spline;

  if (gp->nhands == 2)          gp->butts[GFB_DELH].active = 0;
  if (gp->nhands == MAX_GHANDS) gp->butts[GFB_ADDH].active = 0;

  GenerateGrafFunc(gp,0);
  XSelectInput(theDisp, gp->win, ExposureMask | ButtonPressMask |
	       KeyPressMask);
  XSelectInput(theDisp, gp->gwin, ExposureMask);

  XMapSubwindows(theDisp, gp->win);

}


/***************************************************/
void InitGraf(gp)
GRAF *gp;
{
  gp->nhands = 4;
  gp->spline = 1;
  gp->hands[0].x =   0;  gp->hands[0].y =   0;
  gp->hands[1].x =  64;  gp->hands[1].y =  64;
  gp->hands[2].x = 192;  gp->hands[2].y = 192;
  gp->hands[3].x = 255;  gp->hands[3].y = 255;

  gp->gammamode = 0;     gp->gamma = 1.0;
}


/***************************************************/
void RedrawGraf(gp, gwin)
GRAF *gp;
int gwin;
{
  int i;

  /* if gwin, only redraw the graf window, otherwise, only redraw the
     title and buttons */

  if (gwin) drawGraf(gp,0);
  else {
    Draw3dRect(gp->win, 0,0, GWIDE-1, GHIGH-1, R3D_OUT, 1, hicol, locol,
	       gp->bg);

    XSetForeground(theDisp, theGC, gp->fg);
    XSetBackground(theDisp, theGC, gp->bg);
    DrawString(gp->win, 2, 1+ASCENT, gp->str);

    for (i=0; i<N_GFB; i++) BTRedraw(&gp->butts[i]);
  }
}


/***************************************************/
static void drawGraf(gp,erase)
GRAF *gp;
int   erase;
{
  int i,x,y;
  XPoint  pts[129], *pt;


  if (gp->entergamma) {
    const char *str1 = "Enter gamma";
    const char *str2 = "value: ";

    XSetForeground(theDisp, theGC, gp->fg);
    XSetBackground(theDisp, theGC, gp->bg);

    XClearWindow(theDisp,gp->gwin);
    DrawString(gp->gwin, 10, 30+ASCENT,         str1);
    DrawString(gp->gwin, 10, 30+ASCENT+CHIGH+3, str2);

    x = 10 + StringWidth(str2) + 8;
    y = 30 + ASCENT + CHIGH + 3;
    i = StringWidth(gp->gvstr);
    if (gp->entergamma < 0 && strlen(gp->gvstr)) {
      /* show string highlited */
      XFillRectangle(theDisp, gp->gwin, theGC, x-1, y-ASCENT-1,
		     (u_int) i+2, (u_int) CHIGH+2);
      XSetForeground(theDisp, theGC, gp->bg);
    }
    else
      XDrawLine(theDisp, gp->gwin, theGC, x+i, y-ASCENT, x+i, y+DESCENT);

    DrawString(gp->gwin, x,y, gp->gvstr);

    return;
  }

  if (erase) XSetForeground(theDisp, theGC, gp->bg);
        else XSetForeground(theDisp, theGC, gp->fg);

  for (i=0, pt=pts; i<256; i+=2,pt++) {
    pt->x = i/2;  pt->y = 127 - (gp->func[i]/2);
    if (i==0) i = -1;   /* kludge to get sequence 0,1,3,5, ... 253,255 */
  }
  XDrawLines(theDisp, gp->gwin, theGC, pts, 129, CoordModeOrigin);

  if (erase) return;   /* don't erase handles */


  /* redraw handles */

  XSetForeground(theDisp, theGC, gp->bg);

  for (i=0; i<gp->nhands; i++) {   /* clear inside rectangles */
    x = gp->hands[i].x/2;  y = 127 - gp->hands[i].y/2;
    XFillRectangle(theDisp, gp->gwin, theGC, x-2, y-2, 5,5);
  }

  XSetForeground(theDisp,theGC,gp->fg);

  for (i=0; i<gp->nhands; i++) {  /* draw center dots */
    x = gp->hands[i].x/2;  y = 127 - gp->hands[i].y/2;
    XDrawPoint(theDisp, gp->gwin, theGC, x, y);
  }

  for (i=0; i<gp->nhands; i++) {   /* draw rectangles */
    x = gp->hands[i].x/2;  y = 127 - gp->hands[i].y/2;
    XDrawRectangle(theDisp, gp->gwin, theGC, x-3, y-3, 6,6);
  }

}



/***************************************************/
int ClickGraf(gp,child,mx,my)
GRAF *gp;
Window child;
int mx,my;
{
  /* returns '1' if GrafFunc was changed, '0' otherwise */

  int          i, j, rv;
  byte         oldfunc[256];
  BUTT        *bp;
  Window       rW, cW;
  int          x, y, rx, ry, firsttime=1;
  unsigned int mask;

  rv = 0;

  while (1) {   /* loop until Button1 up and ShiftKey up */
    if (!XQueryPointer(theDisp,gp->win,&rW,&cW,&rx,&ry,
		       &mx,&my,&mask)) continue;
    if (!firsttime && !(mask & (Button1Mask | ShiftMask))) break;

    /* if it's not the first time, wait for Button1 to be pressed */
    if (!firsttime && !(mask & Button1Mask)) continue;

    firsttime = 0;

    for (i=0; i<N_GFB; i++) {
      bp = &gp->butts[i];
      if (PTINRECT(mx, my, bp->x, bp->y, bp->w, bp->h)) break;
    }

    if (i<N_GFB) {  /* found one */
      if (BTTrack(bp)) {  /* it was selected */
	switch (i) {
	case GFB_SPLINE:
	case GFB_LINE:
	  gp->gammamode = 0;

	  if ((i==GFB_SPLINE && !gp->spline) ||
	      (i==GFB_LINE   &&  gp->spline)) {
	    gp->spline = !gp->spline;
	    gp->butts[GFB_SPLINE].lit =  gp->spline;
	    gp->butts[GFB_LINE].lit   = !gp->spline;
	    BTRedraw(&gp->butts[GFB_SPLINE]);
	    BTRedraw(&gp->butts[GFB_LINE]);

	    for (i=0; i<256; i++) oldfunc[i] = gp->func[i];
	    GenerateGrafFunc(gp,1);
	    for (i=0; i<256 && abs(oldfunc[i] - gp->func[i])<2; i++);
	    if (i<256) rv = 1;
	  }
	  else {
	    gp->butts[i].lit = 1;
	    BTRedraw(&gp->butts[i]);
	  }
	  break;

	case GFB_RESET:
	  for (j=0; j<gp->nhands; j++) {
	    gp->hands[j].y = gp->hands[j].x;
	  }
	  gp->gammamode = 0;

	  for (i=0; i<256; i++) oldfunc[i] = gp->func[i];
	  GenerateGrafFunc(gp,1);
	  for (i=0; i<256 && abs(oldfunc[i] - gp->func[i])<2; i++);
	  if (i<256) rv = 1;

	  break;

	case GFB_GAMMA:
	  gp->entergamma = -1;
	  drawGraf(gp,1);
	  break;

	case GFB_ADDH:
	  if (gp->nhands < MAX_GHANDS) {
	    /* find largest x-gap in handles, put new handle in mid */
	    int lgap, lpos, x, y;

	    lgap = gp->hands[1].x - gp->hands[0].x;
	    lpos = 1;
	    for (j=1; j<gp->nhands-1; j++)
	      if ((gp->hands[j+1].x - gp->hands[j].x) > lgap) {
		lgap = gp->hands[j+1].x - gp->hands[j].x;
		lpos = j+1;
	      }

	    /* open up position in hands[] array */
	    xvbcopy((char *) &gp->hands[lpos], (char *) &gp->hands[lpos+1],
		    (gp->nhands - lpos) * sizeof(XPoint));

	    x = gp->hands[lpos-1].x + lgap/2;
	    y = gp->func[x];
	    gp->hands[lpos].x = x;
	    gp->hands[lpos].y = y;
	    gp->nhands++;

	    for (i=0; i<256; i++) oldfunc[i] = gp->func[i];
	    GenerateGrafFunc(gp,1);
	    for (i=0; i<256 && abs(oldfunc[i] - gp->func[i])<2; i++);
	    if (i<256) rv = 1;

	    if (gp->nhands==MAX_GHANDS)   /* turn off 'add' button */
	      BTSetActive(&gp->butts[GFB_ADDH], 0);

	    if (gp->nhands==3)            /* turn on 'del' button */
	      BTSetActive(&gp->butts[GFB_DELH], 1);
	  }
	  break;

	case GFB_DELH:
	  if (gp->nhands > 2) {
	    /* find (middle) point whose x-distance to previous
	       and next points is minimal.  Delete that point */
	    int dist, mdist, mpos;

	    mdist = (gp->hands[1].x - gp->hands[0].x) +
	            (gp->hands[2].x - gp->hands[1].x);
	    mpos = 1;

	    for (j=2; j<gp->nhands-1; j++) {
	      dist = (gp->hands[j  ].x - gp->hands[j-1].x) +
		(gp->hands[j+1].x - gp->hands[j].x);
	      if (dist < mdist) {
		mdist = dist;  mpos = j;
	      }
	    }

	    /* delete position 'mpos' in hands[] array */
	    xvbcopy((char *) &gp->hands[mpos+1], (char *) &gp->hands[mpos],
		    (gp->nhands-mpos-1) * sizeof(XPoint));

	    gp->nhands--;
	    for (i=0; i<256; i++) oldfunc[i] = gp->func[i];
	    GenerateGrafFunc(gp,1);
	    for (i=0; i<256 && abs(oldfunc[i] - gp->func[i])<2; i++);
	    if (i<256) rv = 1;

	    if (gp->nhands==MAX_GHANDS-1) /* turn on 'add' button */
	      BTSetActive(&gp->butts[GFB_ADDH], 1);

	    if (gp->nhands==2)            /* turn off 'del' button */
	      BTSetActive(&gp->butts[GFB_DELH], 0);
	  }
	  break;
	}
      }
    }


    else if (cW == gp->gwin) {  /* clicked in graph */
      int h, vertonly, offx, offy;

      XTranslateCoordinates(theDisp, gp->win, gp->gwin,mx,my,&mx,&my,&cW);

      /* see if x,y is within any of the handles */
      for (h=0; h<gp->nhands; h++) {
	if (PTINRECT(mx*2,(127-my)*2,
		     gp->hands[h].x-5,gp->hands[h].y-5,11,11)) break;
      }

      if (h==gp->nhands) {     /* not found.  wait 'til mouseup */
	while (XQueryPointer(theDisp,gp->gwin,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	  if (!(mask & Button1Mask)) break;    /* button released */
	}
      }

      else {  /* track handle */
	int origx, origy, orighx, orighy, dx, dy, olddx, olddy, grab;

	drawHandPos(gp, h);

	/* keep original mouse position in 'mx,my', and warp mouse to center
	   of screen */
	grab = !XGrabPointer(theDisp, gp->gwin, False, 0, GrabModeAsync,
			  GrabModeAsync, None, inviso, (Time) CurrentTime);
	XWarpPointer(theDisp, None, rootW, 0,0,0,0,
		     (int) dispWIDE/2, (int) dispHIGH/2);

	origx = dispWIDE/2;  origy = dispHIGH/2;
	orighx = gp->hands[h].x;  orighy = gp->hands[h].y;

	gp->gammamode = 0;
	offx = gp->hands[h].x - origx;
	offy = gp->hands[h].y - origy;

	vertonly = (h==0 || h==(gp->nhands-1));

	olddx = 0;  olddy = 0;

	while (XQueryPointer(theDisp,rootW,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	  int newx, newy;

	  if (!(mask & Button1Mask)) break;    /* button released */

	  /* x,y are in screen coordinates */
	  if (vertonly) x = origx;    /* no sidewards motion */

	  dx = x - origx;  dy = origy - y;   /* flip y axis */

	  /* new (virt) position of handle is (desired)
	     orighx + dx, orighy + dy */

	  if (!vertonly) { /* keep this handle between its neighbors */
	    if (dx+orighx <= gp->hands[h-1].x) dx=(gp->hands[h-1].x+1)-orighx;
	    if (dx+orighx >= gp->hands[h+1].x) dx=(gp->hands[h+1].x-1)-orighx;
	  }

	  newx = dx + orighx;  newy = dy + orighy;
	  RANGE(newy, 0, 255);

	  if (newx != gp->hands[h].x || newy != gp->hands[h].y) {
	    /* this handle has moved... */
	    XSetForeground(theDisp, theGC, gp->bg);
	    XFillRectangle(theDisp, gp->gwin, theGC,
		     (gp->hands[h].x/2)-3, ((255-gp->hands[h].y)/2)-3, 7,7);

	    gp->hands[h].x = newx;  gp->hands[h].y = newy;
	    drawGraf(gp,1);           /* erase old trace */
	    GenerateGrafFunc(gp,0);
	    drawGraf(gp,0);
	    drawHandPos(gp, h);
	    rv = 1;
	    olddx = dx;  olddy = dy;

	    if (gp->drawobj) (gp->drawobj)();
	  }
	}

	drawHandPos(gp, -1);
	XWarpPointer(theDisp, None, gp->gwin, 0,0,0,0,
		     gp->hands[h].x/2, (255-gp->hands[h].y)/2);
	if (grab) XUngrabPointer(theDisp, (Time) CurrentTime);
      }
    }
  }

  return rv;
}


static void drawHandPos(gp, hnum)
     GRAF *gp;
     int   hnum;
{
  int w;
  const char *tstr = "888,888";

  /* if hnum < 0, clears the text area */

  XSetFont(theDisp, theGC, monofont);
  w = XTextWidth(monofinfo, tstr, (int) strlen(tstr));

  if (hnum >= 0) sprintf(dummystr,"%3d,%3d",gp->hands[hnum].x,gp->hands[hnum].y);
            else sprintf(dummystr,"       ");

  XSetForeground(theDisp, theGC, gp->fg);
  XSetBackground(theDisp, theGC, gp->bg);
  XDrawImageString(theDisp, gp->win, theGC, 130-w, 1+ASCENT,
		   dummystr, (int) strlen(dummystr));

  XSetFont(theDisp, theGC, mfont);
}


/***************************************************/
int GrafKey(gp,str)
GRAF *gp;
char *str;
{
  int len, ok;

  /* returns '1' if str was 'eaten', '2' if CR was hit, '0' otherwise. */

  if (!gp->entergamma) {   /* not entering a value yet */
    if (*str == 'g') {
      gp->entergamma = -1;   /* special 'show old value highlited' */
      drawGraf(gp,1);
      str++;
    }
    else return 0;
  }

  while (*str) {
    if (gp->entergamma == -1 &&
	(*str != '\012' && *str != '\015' && *str != '\033')) {
      gp->entergamma = 1;
      gp->gvstr[0] = '\0';
      drawGraf(gp,1);
    }

    ok = 0;
    len = strlen(gp->gvstr);

    if (*str>= '0' && *str <= '9') {
      if (len < GVMAX) {
	gp->gvstr[len++] = *str;
  	gp->gvstr[len] = '\0';
	ok = 1;
      }
    }

    else if (*str == '.') {
      if (len < GVMAX && ((char *) index(gp->gvstr,'.'))==NULL) {
	gp->gvstr[len++] = *str;
	gp->gvstr[len] = '\0';
	ok = 1;
      }
    }

    else if (*str == '-') {   /* only allowed as first char */
      if (len==0) {
	gp->gvstr[len++] = *str;
	gp->gvstr[len]   = '\0';
	ok = 1;
      }
    }

    else if (*str == '\010' || *str == '\177') {   /* BS or DEL */
      if (len > 0) gp->gvstr[--len] = '\0';
      ok = 1;
    }

    else if (*str == '\025' || *str == '\013') {   /* ^U or ^K clear line */
      gp->gvstr[0] = '\0';  len = 0;  ok = 1;
    }

    else if (*str == '\012' || *str == '\015' || *str == '\033') {
      /* CR, LF or ESC*/
      if (len>0 && *str != '\033') {   /* 'Ok' processing */
	if (sscanf(gp->gvstr, "%lf", &(gp->gamma))==1) {
	  if (gp->gamma >=  1000.0) gp->gamma =  1000.0;
	  if (gp->gamma <= -1000.0) gp->gamma = -1000.0;
	  gp->gammamode = 1;
	  gp->entergamma = 0;
	  GenerateGrafFunc(gp,1);
	  return 2;
	}
      }

      else {
	gp->entergamma = 0;
	GenerateGrafFunc(gp,1);
      }
      break;   /* out of *str loop */
    }

    if (!ok) XBell(theDisp, 0);
    else {
      XClearWindow(theDisp,gp->gwin);
      drawGraf(gp,1);
    }

    str++;
  }

  return 1;
}



/*********************/
void GenerateGrafFunc(gp,redraw)
GRAF *gp;
int redraw;
{
  /* generate new gp->func data (ie, handles have moved, or line/spline
     setting has changed) and redraw the entire graph area */

  int i,j,k;

  /* do sanity check.  (x-coords must be sorted (strictly increasing)) */

  for (i=0; i<gp->nhands; i++) {
    RANGE(gp->hands[i].x, 0, 255);
    RANGE(gp->hands[i].y, 0, 255);
  }

  gp->hands[0].x = 0;  gp->hands[gp->nhands-1].x = 255;
  for (i=1; i<gp->nhands-1; i++) {
    if (gp->hands[i].x < i)  gp->hands[i].x = i;
    if (gp->hands[i].x > 256-gp->nhands+i)
        gp->hands[i].x = 256-gp->nhands+i;

    if (gp->hands[i].x <= gp->hands[i-1].x)
      gp->hands[i].x = gp->hands[i-1].x + 1;
  }

  /* recompute the function */

  if (gp->gammamode) {  /* do gamma function instead of interpolation */
    double y, invgam;
    if (gp->gamma < 0.0) {
      invgam = 1.0 / -gp->gamma; /* invgram is now positive */
      for (i=0; i<256; i++) {
	y = pow( ((double) i / 255.0), invgam) * 255.0;
	j = (int) floor(y + 0.5);
	RANGE(j,0,255);
	gp->func[255-i] = j;    /* store the entries in reverse order */
      }
    }
    else if (gp->gamma > 0.0) {
      invgam = 1.0 / gp->gamma;
      for (i=0; i<256; i++) {
	y = pow( ((double) i / 255.0), invgam) * 255.0;
	j = (int) floor(y + 0.5);
	RANGE(j,0,255);
	gp->func[i] = j;
      }
    }
    else {   /* gp->gamma == 0.0 */
      for (i=0; i<256; i++) gp->func[i] = 0;
    }


    for (i=0; i<gp->nhands; i++) {
      gp->hands[i].y = gp->func[gp->hands[i].x];
    }
  }

  else if (!gp->spline) {  /* do linear interpolation */
      int y,x1,y1,x2,y2;
      double yd;

      for (i=0; i<gp->nhands-1; i++) {
	x1 = gp->hands[ i ].x;  y1 = gp->hands[ i ].y;
	x2 = gp->hands[i+1].x;  y2 = gp->hands[i+1].y;

	for (j=x1,k=0; j<=x2; j++,k++) {  /* x2 <= 255 */
	  yd = ((double) k * (y2 - y1)) / (x2 - x1);
	  y = y1 + (int) floor(yd + 0.5);
	  RANGE(y,0,255);
	  gp->func[j] = y;
	}
      }
    }

  else {  /* splinear interpolation */
    static int x[MAX_GHANDS], y[MAX_GHANDS];
    double yf[MAX_GHANDS];
    double yd;

    for (i=0; i<gp->nhands; i++) {
      x[i] = gp->hands[i].x;  y[i] = gp->hands[i].y;
    }

    InitSpline(x, y, gp->nhands, yf);

    for (i=0; i<256; i++) {
      yd = EvalSpline(x, y, yf, gp->nhands, (double) i);
      j = (int) floor(yd + 0.5);
      RANGE(j,0,255);
      gp->func[i] = j;
    }
  }


  if (redraw) {  /* redraw graph */
    XClearWindow(theDisp, gp->gwin);
    drawGraf(gp,0);
  }
}


/*********************/
void Graf2Str(gp, str)
GRAF_STATE *gp;
char *str;
{
  /* generates strings of the form: "S 3 : 0,0 : 63,63 : 255,255",
     (meaning SPLINE, 3 points, and the 3 sets of handle coordinates)
     This is the string that you'd put in the 'xv.preset1.vgraph:' resource,
     ferinstance */

  /* maximum length of string generated is 164 characters.  str better be
     able to hold it... */

  int i;
  char cstr[16];

  if (gp->gammamode) {
    sprintf(str,"G %g", gp->gamma);
  }
  else {
    sprintf(str, "%c %d", gp->spline ? 'S' : 'L', gp->nhands);
    for (i=0; i<gp->nhands; i++) {
      sprintf(cstr," : %d,%d", gp->hands[i].x, gp->hands[i].y);
      strcat(str, cstr);
    }
  }
}


/*********************/
int Str2Graf(gp, str)
     GRAF_STATE *gp;
     const char *str;
{
  /* parses strings of the form: "S 3 : 0,0 : 63,63 : 255,255",
     (meaning SPLINE, 3 points, and the 3 sets of handle coordinates)
     This is the string that you'd put in the 'xv.preset1.igraf:' resource,
     ferinstance */

  /* returns '1' if unable to parse.  Note:  does NOT redraw the graf, as
     it may be called before the graph window has even been created */

  /* NOTE: I deliberately avoid using '*dp++ = *sp++', as this sort of
     thing tends to break optimizers */

  char   tstr[256], tstr1[256], *sp, *dp;
  const char *csp;
  XPoint coords[MAX_GHANDS];
  int    spline, nhands, i, x, y;

  if (!str) return 1;  /* NULL strings don't parse well! */

  /* first, strip all pesky whitespace from str */
  for (csp=str, dp=tstr; *csp; csp++)
    if (*csp > ' ') { *dp = *csp;  dp++; }
  *dp = '\0';

  /* check for 'gamma'-style str */
  if (*tstr == 'G' || *tstr == 'g') {
    if (sscanf(tstr+1, "%lf", &(gp->gamma)) == 1 &&
	gp->gamma >= -1000.0 && gp->gamma <= 1000.0) {
      gp->gammamode = 1;
      sprintf(gp->gvstr, "%.5g", gp->gamma);
      return 0;
    }
    else return 1;
  }

  /* read Spline, or Line (S/L) character */
  sp = tstr;
  if      (*sp == 'S' || *sp == 's') spline = 1;
  else if (*sp == 'L' || *sp == 'l') spline = 0;
  else return 1;

  /* read 'nhands' */
  sp++;  dp = tstr1;
  while (*sp && *sp != ':') { *dp = *sp;  dp++;  sp++; }
  *dp++ = '\0';
  nhands = atoi(tstr1);
  if (nhands>MAX_GHANDS || nhands<2) return 1;

  /* read nhands coordinate pairs */
  for (i=0; i<nhands && *sp; i++) {
    sp++;  dp = tstr1;
    while (*sp && *sp != ':') {*dp = *sp;  dp++;  sp++; }
    *dp++ = '\0';
    if (sscanf(tstr1,"%d,%d",&x, &y) != 2) return 1;
    if (x < 0 || x > 255 ||
	y < 0 || y > 255) return 1;  /* out of range */
    coords[i].x = x;  coords[i].y = y;
  }

  if (i<nhands) return 1;  /* string terminated early */

  gp->nhands = nhands;  gp->spline = spline;
  for (i=0; i<nhands; i++) {
    gp->hands[i].x = coords[i].x;
    gp->hands[i].y = coords[i].y;
  }

  return 0;
}



/*********************/
void GetGrafState(gp, gsp)
GRAF *gp;
GRAF_STATE *gsp;
{
  int i;

  gsp->spline = gp->spline;
  gsp->entergamma= gp->entergamma;
  gsp->gammamode = gp->gammamode;
  gsp->gamma = gp->gamma;
  gsp->nhands = gp->nhands;
  strcpy(gsp->gvstr, gp->gvstr);
  for (i=0; i<MAX_GHANDS; i++) {
    gsp->hands[i].x = gp->hands[i].x;
    gsp->hands[i].y = gp->hands[i].y;
  }
}


/*********************/
int SetGrafState(gp, gsp)
GRAF *gp;
GRAF_STATE *gsp;
{
#define IFSET(a,b) if ((a) != (b)) { a = b;  rv++; }
  int i;
  int rv = 0;

  IFSET(gp->spline,     gsp->spline);
  IFSET(gp->entergamma, gsp->entergamma);
  IFSET(gp->gammamode,  gsp->gammamode);
  IFSET(gp->gamma,      gsp->gamma);
  IFSET(gp->nhands,     gsp->nhands);

  if (strcmp(gp->gvstr, gsp->gvstr))
    { strcpy(gp->gvstr, gsp->gvstr);  rv++; }

  for (i=0; i<gp->nhands; i++) {
    IFSET(gp->hands[i].x, gsp->hands[i].x);
    IFSET(gp->hands[i].y, gsp->hands[i].y);
  }

  gp->butts[GFB_DELH].active = (gp->nhands > 2);
  gp->butts[GFB_ADDH].active = (gp->nhands < MAX_GHANDS);

  gp->butts[GFB_SPLINE].lit =  gp->spline;
  gp->butts[GFB_LINE].lit   = !gp->spline;

  if (rv) {
    XClearWindow(theDisp,gp->gwin);
    GenerateGrafFunc(gp,0);
    RedrawGraf(gp,0);
    RedrawGraf(gp,1);
  }

  return rv;
}


/*********************/
void InitSpline(x,y,n,y2)
     int *x, *y, n;
     double *y2;
{
  /* given arrays of data points x[0..n-1] and y[0..n-1], computes the
     values of the second derivative at each of the data points
     y2[0..n-1] for use in the splint function */

  int i,k;
  double p,qn,sig,un,u[MAX_GHANDS];

  y2[0] = u[0] = 0.0;

  for (i=1; i<n-1; i++) {
    sig = ((double) x[i]-x[i-1]) / ((double) x[i+1] - x[i-1]);
    p = sig * y2[i-1] + 2.0;
    y2[i] = (sig-1.0) / p;
    u[i] = (((double) y[i+1]-y[i]) / (x[i+1]-x[i])) -
           (((double) y[i]-y[i-1]) / (x[i]-x[i-1]));
    u[i] = (6.0 * u[i]/(x[i+1]-x[i-1]) - sig*u[i-1]) / p;
  }
  qn = un = 0.0;

  y2[n-1] = (un-qn*u[n-2]) / (qn*y2[n-2]+1.0);
  for (k=n-2; k>=0; k--)
    y2[k] = y2[k]*y2[k+1]+u[k];
}



/*********************/
double EvalSpline(xa,ya,y2a,n,x)
double y2a[],x;
int n,xa[],ya[];
{
  int klo,khi,k;
  double h,b,a;

  klo = 0;
  khi = n-1;
  while (khi-klo > 1) {
    k = (khi+klo) >> 1;
    if (xa[k] > x) khi = k;
    else klo = k;
  }
  h = xa[khi] - xa[klo];
  if (h==0.0) FatalError("bad xvalues in splint\n");
  a = (xa[khi]-x)/h;
  b = (x-xa[klo])/h;
  return (a*ya[klo] + b*ya[khi] + ((a*a*a-a)*y2a[klo] +(b*b*b-b)*y2a[khi])
	  * (h*h) / 6.0);
}



