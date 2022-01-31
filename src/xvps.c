/*
 * xvps.c - Postscript dialog box, file output functions
 *
 * callable functions:
 *
 *   CreatePSD(geom)           -  creates the psW window.  Doesn't map it.
 *   PSDialog()                -  maps psW
 *   PSCheckEvent(event)       -  called by event handler
 *   PSSaveParams(str,int)     -  tells PSDialog what to do when 'Ok' clicked
 *   PSResize()                -  called whenever ePic changes size
 *   LoadPS()                  -  attempts to load PS files using Ghostscript
 */

#include "copyright.h"

#define NEEDSDIR
#include "xv.h"

#define PSWIDE 431
#define PSHIGH 350
#define PMAX   200    /* size of square that a 'page' has to fit into */

#define PS_BOK    0
#define PS_BCANC  1
#define PS_BCENT  2
#define PS_BORG   3
#define PS_BMAX   4
#define PS_BPOSX  5
#define PS_BPOSY  6
#define PS_NBUTTS 7


/* paperRB indicies */
#define PSZ_NORM  0
#define PSZ_A4    1
#define PSZ_B5    2
#define PSZ_A3    3
#define PSZ_LEGAL 4
#define PSZ_BSIZE 5
#define PSZ_4BY5  6
#define PSZ_35MM  7

/* orientRB indicies */
#define ORNT_PORT 0
#define ORNT_LAND 1

#define BUTTH 24

#define IN2CM 2.54

#define PIX2INCH 72.0   /* # of pixels per inch, at 100% scaling */

static void drawPSD        PARM((int, int, int, int));
static void drawPosStr     PARM((void));
static void drawSizeStr    PARM((void));
static void drawResStr     PARM((void));
static void drawPage       PARM((void));
static void clickPSD       PARM((int, int));
static void clickPage      PARM((int, int));
static void doCmd          PARM((int));
static void changedScale   PARM((void));
static void setScale       PARM((void));
static void changedPaper   PARM((void));
static void setPaper       PARM((void));
static void drawIRect      PARM((int));
static void centerImage    PARM((void));
static void maxImage       PARM((void));
static void moveImage      PARM((double, double));
static void writePS        PARM((void));
static int  rle_encode     PARM((byte *, byte *, int));
static void psColorImage   PARM((FILE *));
static void psColorMap     PARM((FILE *fp, int, int, byte *, byte *, byte *));
static void psRleCmapImage PARM((FILE *, int));
static void epsPreview     PARM((FILE *, byte *, int, int, int, int,
				 byte *, byte *, byte *, int));
static int  writeBWStip    PARM((FILE *, byte *, const char *, int, int, int));

#ifdef GS_PATH
static void buildCmdStr    PARM((char *, char *, char *, int, int));
#endif


/* local variables */
static Window pageF;
static DIAL   xsDial, ysDial;
static RBUTT *orientRB, *paperRB;
static CBUTT  lockCB;
static BUTT   psbut[PS_NBUTTS];
static double sz_inx, sz_iny;     /* image size, in inches */
static double pos_inx, pos_iny;   /* top-left offset of image, in inches */
static int    dpix, dpiy;         /* # of image pixels per inch */
static int    tracking=0;         /* used in changedScale */
static int    posxType, posyType;

/* sizes of pages in inches */
static double paperSize[8][2] = { { 8.500, 11.000},   /* US NORMAL */
				  { 8.268, 11.693},   /* A4 210mm x 297mm */
				  { 7.205, 10.118},   /* B5 183mm x 257mm */
				  {11.693, 16.535},   /* A3 297mm x 420mm */
				  { 8.500, 14.000},   /* US LEGAL */
				  {11.000, 17.000},   /* B-size */
				  { 3.875,  4.875},   /* 4 by 5 */
				  { 0.945,  1.417}};  /* 35mm (24x36) */

/* size of l+r margin and t+b margin.  image is centered */
static double margins[8][2] = { { 1.000, 1.000},   /* US NORMAL */
				{ 1.000, 1.000},   /* A4 */
				{ 1.000, 1.000},   /* B5 */
				{ 1.000, 1.000},   /* A3 */
				{ 1.000, 1.000},   /* US LEGAL */
				{ 1.000, 1.000},   /* B-size */
				{ 0.275, 0.275},   /* 4 by 5 */
				{ 0.078, 0.078}};  /* 35mm (24x36) */


static double psizex, psizey;   /* current paper size, in inches */
static double in2pix;           /* inch to pixels in 'pageF' */
static XRectangle pageRect;     /* bounding rect of page, in screen coords */

static char *filename;          /* filename to save to */
static int   colorType;         /* value of 'Colors' rbutt in dir box */
static int   firsttime=1;       /* first time PSDialog being opened ? */


/***************************************************/
void CreatePSD(geom)
char *geom;
{
  psW = CreateWindow("xv postscript", "XVps", geom,
		     PSWIDE, PSHIGH, infofg, infobg, 0);
  if (!psW) FatalError("can't create postscript window!");

  pageF = XCreateSimpleWindow(theDisp, psW, 20,30, PMAX+1,PMAX+1,
			      1,infofg,infobg);
  if (!pageF) FatalError("couldn't create frame windows");

  XSetWindowBackgroundPixmap(theDisp, pageF, grayTile);

  XSelectInput(theDisp, pageF, ExposureMask | ButtonPressMask);
  XSelectInput(theDisp, psW,   ExposureMask | ButtonPressMask | KeyPressMask);

  CBCreate(&encapsCB, psW, 240, 7, "preview", infofg, infobg, hicol, locol);
  CBCreate(&pscompCB, psW, 331, 7, "compress", infofg, infobg, hicol, locol);

  DCreate(&xsDial, psW, 240, 30, 80, 100, 10.0, 800.0, 100.0, 0.5, 5.0,
	  infofg, infobg, hicol, locol, "Width", "%");
  DCreate(&ysDial, psW, 331, 30, 80, 100, 10.0, 800.0, 100.0, 0.5, 5.0,
	  infofg, infobg, hicol, locol, "Height", "%");
  xsDial.drawobj = changedScale;
  ysDial.drawobj = changedScale;

  CBCreate(&lockCB, psW, 318, 134, "", infofg, infobg, hicol, locol);
  lockCB.val = 1;

  orientRB = RBCreate(NULL, psW, 36, 240+18, "Portrait", infofg, infobg,
		      hicol, locol);
  RBCreate(orientRB, psW, 36+80, 240+18, "Landscape", infofg, infobg,
	   hicol, locol);

  paperRB = RBCreate(NULL, psW,36, 240+18+36, "8.5\"x11\"",
		     infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36+80,    240+18+36, "A4",
	   infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36+122,   240+18+36, "B5",
	   infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36+164,   240+18+36, "A3",
	   infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36,       240+36+36, "8.5\"x14\"",
	   infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36+80,    240+36+36, "11\"x17\"",
	   infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36,       240+54+36, "4\"x5\"",
	   infofg, infobg, hicol, locol);
  RBCreate(paperRB, psW, 36+80,    240+54+36, "35mm slide",
	   infofg, infobg, hicol, locol);

  BTCreate(&psbut[PS_BOK], psW, PSWIDE-180, PSHIGH-10-BUTTH, 80, BUTTH,
	   "Ok", infofg, infobg, hicol, locol);
  BTCreate(&psbut[PS_BCANC], psW, PSWIDE-90, PSHIGH-10-BUTTH, 80, BUTTH,
	   "Cancel", infofg, infobg, hicol, locol);

  BTCreate(&psbut[PS_BCENT], psW, 240, 154, 55, BUTTH-2,
	   "Center", infofg, infobg, hicol, locol);
  BTCreate(&psbut[PS_BORG], psW,  298, 154, 55, BUTTH-2,
	   "Origin", infofg, infobg, hicol, locol);
  BTCreate(&psbut[PS_BMAX], psW,  356, 154, 55, BUTTH-2,
	   "Max",    infofg, infobg, hicol, locol);

  BTCreate(&psbut[PS_BPOSX], psW, 256-14, 190+13-8, 8,8, "",
	   infofg, infobg, hicol, locol);
  BTCreate(&psbut[PS_BPOSY], psW, 256-14, 190+26-8, 8,8, "",
	   infofg, infobg, hicol, locol);

  posxType = posyType = 0;
  pos_inx = 1.0;  pos_iny = 1.0;   /* temporary bootstrapping... */
  setPaper();
  setScale();


  /*** handle PS dialog resources ***/

  if (rd_str("pspaper")) {                /* xv.pspaper:  default paper size */
    char buf[256];
    strcpy(buf, def_str);
    lower_str(buf);

    if      (!strcmp(buf, "8.5x11")) RBSelect(paperRB, PSZ_NORM);
    else if (!strcmp(buf, "a4"))     RBSelect(paperRB, PSZ_A4);
    else if (!strcmp(buf, "b5"))     RBSelect(paperRB, PSZ_B5);
    else if (!strcmp(buf, "a3"))     RBSelect(paperRB, PSZ_A3);
    else if (!strcmp(buf, "8.5x14")) RBSelect(paperRB, PSZ_LEGAL);
    else if (!strcmp(buf, "11x17"))  RBSelect(paperRB, PSZ_BSIZE);
    else if (!strcmp(buf, "4x5"))    RBSelect(paperRB, PSZ_4BY5);
    else if (!strcmp(buf, "35mm"))   RBSelect(paperRB, PSZ_35MM);
    else {
      fprintf(stderr,"%s: unknown 'pspaper' value:  %s\n",cmd,def_str);
      fprintf(stderr,
	      "\tValid choices:  8.5x11 A4 B5 A3 8.5x14 11x17 4x5 35mm\n");
    }

    setPaper();
  }

  if (rd_str("psorient")) {             /* xv.psorient:  default paper ornt. */
    char buf[256];
    strcpy(buf, def_str);
    lower_str(buf);

    if      (!strcmp(buf, "portrait"))  RBSelect(orientRB, ORNT_PORT);
    else if (!strcmp(buf, "landscape")) RBSelect(orientRB, ORNT_LAND);
    else {
      fprintf(stderr,"%s: unknown 'psorient' value:  %s\n",cmd,def_str);
      fprintf(stderr,
	      "\tValid choices:  portrait landscape\n");
    }

    setPaper();
  }

  if (rd_int("psres")) {             /* xv.psres:  default paper resolution */
    if (def_int >= 10 && def_int <= 720) {
      double v = (PIX2INCH * 100) / def_int;

      DSetVal(&xsDial, v);
      DSetVal(&ysDial, v);
    }
  }


  XMapSubwindows(theDisp, psW);
}


/***************************************************/
void PSDialog(vis)
int vis;
{
  if (vis) {
    if (picType == PIC24) {  /* no comp in 24-bit mode */
      pscompCB.val = 0;
      CBSetActive(&pscompCB, 0);
    }
    else CBSetActive(&pscompCB, 1);

    setScale();
    if (firsttime) centerImage();
    firsttime = 0;
    CenterMapWindow(psW, psbut[PS_BOK].x + (int) psbut[PS_BOK].w/2,
		         psbut[PS_BOK].y + (int) psbut[PS_BOK].h/2,
		    PSWIDE, PSHIGH);
  }
  else XUnmapWindow(theDisp, psW);
  psUp = vis;
}


/***************************************************/
int PSCheckEvent(xev)
XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!psUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    /* throw away excess expose events for 'dumb' windows */
    if (e->count > 0 &&
	(e->window == xsDial.win || e->window == ysDial.win ||
	 e->window == pageF)) {}

    else if (e->window == psW)         drawPSD(x, y, w, h);
    else if (e->window == xsDial.win)  DRedraw(&xsDial);
    else if (e->window == ysDial.win)  DRedraw(&ysDial);
    else if (e->window == pageF)       drawPage();
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == psW)   clickPSD(x,y);
      else if (e->window == pageF) clickPage(x,y);

      else if (e->window == xsDial.win || e->window == ysDial.win) {
	if (e->window == xsDial.win) {
	  tracking = 1;
	  DTrack(&xsDial, x,y);
	  tracking = 0;
	}

	else if (e->window == ysDial.win) {
	  tracking = 2;
	  DTrack(&ysDial, x,y);
	  tracking = 0;
	}
      }
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;
    int  stlen, shift, ck;

    stlen = XLookupString(e,buf,128,&ks,(XComposeStatus *) NULL);
    shift = e->state & ShiftMask;
    ck    = CursorKey(ks, shift, 0);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    if (e->window == psW) {
      double dx, dy;
      int movekey;

      movekey = 0;  dx = dy = 0.0;

      if      (ck==CK_LEFT)  { dx = -0.001;  movekey = 1; }
      else if (ck==CK_RIGHT) { dx =  0.001;  movekey = 1; }
      else if (ck==CK_UP)    { dy = -0.001;  movekey = 1; }
      else if (ck==CK_DOWN)  { dy =  0.001;  movekey = 1; }

      else if (stlen) {
	if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
	  FakeButtonPress(&psbut[PS_BOK]);
	}
	else if (buf[0] == '\033') {            /* ESC */
	  FakeButtonPress(&psbut[PS_BCANC]);
	}
      }

      if (movekey) {
	if (e->state & ShiftMask) { dx *= 10.0;  dy *= 10.0; }
	moveImage(pos_inx+dx, pos_iny+dy);
      }
    }
    else rv = 0;
  }
  else rv = 0;

  if (rv==0 && (xev->type == ButtonPress || xev->type == KeyPress)) {
    XBell(theDisp, 50);
    rv = 1;   /* eat it */
  }

  return rv;
}


/***************************************************/
void PSSaveParams(fname, col)
     char *fname;
     int col;
{
  filename = fname;
  colorType = col;
}


/***************************************************/
void PSResize()
{
  changedScale();
}






/***************************************************/
static void drawPSD(x,y,w,h)
int x,y,w,h;
{
  const char *title = "Save PostScript File...";
  int  i,cx;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  RBRedraw(orientRB,-1);
  RBRedraw(paperRB,-1);

  for (i=0; i<PS_NBUTTS; i++) BTRedraw(&psbut[i]);

  CBRedraw(&encapsCB);
  if (colorType != F_BWDITHER && picType!=PIC24) CBRedraw(&pscompCB);
  CBRedraw(&lockCB);

  ULineString(psW, orientRB->x-16, orientRB->y-3-DESCENT, "Orientation:");
  ULineString(psW, paperRB->x-16,   paperRB->y-3-DESCENT, "Paper Size:");

  /* draw 'lock' arrows */
  cx = 240 + 40;  /* center of xsDial */
  XDrawLine(theDisp, psW, theGC, lockCB.x, lockCB.y+6,  cx+2, lockCB.y+6);
  XDrawLine(theDisp, psW, theGC, cx+2, lockCB.y+6, cx+2, lockCB.y-2);

  XDrawLine(theDisp, psW, theGC, lockCB.x, lockCB.y+10,  cx-2, lockCB.y+10);
  XDrawLine(theDisp, psW, theGC, cx-2, lockCB.y+10, cx-2, lockCB.y-2);

  XDrawLine(theDisp, psW, theGC, cx-2-3, lockCB.y-2+3, cx, lockCB.y-2-2);
  XDrawLine(theDisp, psW, theGC, cx+2+3, lockCB.y-2+3, cx, lockCB.y-2-2);

  cx = 330 + 40;  /* center of ysDial */
  XDrawLine(theDisp, psW, theGC, lockCB.x+17, lockCB.y+6,  cx-2, lockCB.y+6);
  XDrawLine(theDisp, psW, theGC, cx-2, lockCB.y+6, cx-2, lockCB.y-2);

  XDrawLine(theDisp, psW, theGC, lockCB.x+17, lockCB.y+10,  cx+2, lockCB.y+10);
  XDrawLine(theDisp, psW, theGC, cx+2, lockCB.y+10, cx+2, lockCB.y-2);

  XDrawLine(theDisp, psW, theGC, cx-2-3, lockCB.y-2+3, cx, lockCB.y-2-2);
  XDrawLine(theDisp, psW, theGC, cx+2+3, lockCB.y-2+3, cx, lockCB.y-2-2);

  DrawString(psW, 10, 19, title);

  ULineString(psW, 240, 190,    "Position:");
  drawPosStr();
  ULineString(psW, 240, 190+45, "Size:");
  drawSizeStr();
  ULineString(psW, 240, 190+90, "Resolution:");
  drawResStr();


  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void drawPosStr()
{
  int         x,y;
  double      cmx, cmy, inx, iny;
  char        str[64], str1[64];
  const char *xst, *yst;

  x = 256;  y = 190 + 13;
  inx = iny = 0;
  xst = yst = (const char *) NULL;

  switch (posxType) {
  case 0:  xst = "Left: ";  inx = pos_inx;                      break;
  case 1:  xst = "Right:";  inx = psizex - (pos_inx + sz_inx);  break;
  case 2:  xst = "X Mid:";  inx = pos_inx + sz_inx/2;           break;
  }

  switch (posyType) {
  case 0:  yst = "Top:  ";  iny = pos_iny;                      break;
  case 1:  yst = "Bot:  ";  iny = psizey - (pos_iny + sz_iny);  break;
  case 2:  yst = "Y Mid:";  iny = pos_iny + sz_iny/2;           break;
  }

  cmx = inx * IN2CM;
  cmy = iny * IN2CM;

  sprintf(str,  "%s %.3f\" (%.2fcm)       ", xst, inx, cmx);
  sprintf(str1, "%s %.3f\" (%.2fcm)       ", yst, iny, cmy);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  XSetFont(theDisp, theGC, monofont);
  XDrawImageString(theDisp, psW, theGC, x, y,    str,  (int) strlen(str));
  XDrawImageString(theDisp, psW, theGC, x, y+13, str1, (int) strlen(str1));
  XSetFont(theDisp, theGC, mfont);
}


/***************************************************/
static void drawSizeStr()
{
  int x,y;
  double cmx, cmy;
  char   str[64], str1[64];

  x = 256;  y = 190+13+45;

  cmx = sz_inx * IN2CM;
  cmy = sz_iny * IN2CM;

  sprintf(str,  "%.3f\" x %.3f\"        ", sz_inx, sz_iny);
  sprintf(str1, "%.2fcm x %.2fcm        ", cmx, cmy);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);
  XSetFont(theDisp, theGC, monofont);

  XDrawImageString(theDisp, psW, theGC, x, y,    str,  (int) strlen(str));
  XDrawImageString(theDisp, psW, theGC, x, y+13, str1, (int) strlen(str1));
  XSetFont(theDisp, theGC, mfont);
}


/***************************************************/
static void drawResStr()
{
  int x,y;
  char   str[64];

  x = 256;  y = 190 + 13 + 90;

  sprintf(str,  "%ddpi x %ddpi        ", dpix, dpiy);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);
  XSetFont(theDisp, theGC, monofont);
  XDrawImageString(theDisp, psW, theGC, x, y, str, (int) strlen(str));
  XSetFont(theDisp, theGC, mfont);
}




/***************************************************/
static void drawPage()
{
  /* draw page */
  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp, pageF, theGC, pageRect.x+1, pageRect.y+1,
		 (u_int) pageRect.width-1, (u_int) pageRect.height-1);

  XSetForeground(theDisp, theGC, infofg);
  XDrawRectangle(theDisp, pageF, theGC, pageRect.x, pageRect.y,
		 (u_int) pageRect.width, (u_int) pageRect.height);

  drawIRect(1);
}


/***************************************************/
static void clickPSD(x,y)
int x,y;
{
  int i;
  BUTT *bp;

  /* check BUTTs */

  for (i=0; i<PS_NBUTTS; i++) {
    bp = &psbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<PS_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }

  /* check RBUTTs */

  else if ((i=RBClick(orientRB,x,y)) >= 0) {
    if (RBTrack(orientRB, i)) changedPaper();
  }

  else if ((i=RBClick(paperRB,x,y)) >= 0) {
    if (RBTrack(paperRB, i)) changedPaper();
  }

  /* check CBUTTs */

  else if (CBClick(&lockCB,x,y)) {
    if (CBTrack(&lockCB) && lockCB.val) {  /* turned on lock */
      DSetVal(&ysDial, xsDial.val);        /* copy xsDial.val to ysDial */
      changedScale();
    }
  }

  else if (CBClick(&encapsCB,x,y)) CBTrack(&encapsCB);
  else if (CBClick(&pscompCB,x,y)) CBTrack(&pscompCB);
}



/***************************************************/
static void clickPage(mx,my)
int mx,my;
{
  Window       rW,cW;
  int          rx,ry,x,y;
  unsigned int mask;
  double       offx, offy, newx, newy;

  /* compute offset (in inches) between 'drag point' and
     the top-left corner of the image */

  offx = ((mx - pageRect.x) / in2pix) - pos_inx;
  offy = ((my - pageRect.y) / in2pix) - pos_iny;

  /* if clicked outside of image rectangle, ignore */
  if (offx<0.0 || offy < 0.0 || offx>=sz_inx || offy >= sz_iny) return;

  while (1) {
    if (XQueryPointer(theDisp,pageF,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
      if (!(mask & Button1Mask)) break;    /* button released */

      /* compute new pos_inx, pos_iny based on x,y coords */
      newx = ((x-pageRect.x) / in2pix) - offx;
      newy = ((y-pageRect.y) / in2pix) - offy;

      moveImage(newx, newy);
    }
  }
}



/***************************************************/
static void doCmd(cmd)
     int cmd;
{
  char *fullname;

  switch (cmd) {
  case PS_BOK:    writePS();
                  PSDialog(0);
                  fullname = GetDirFullName();
                  if (!ISPIPE(fullname[0])) {
		    XVCreatedFile(fullname);
		    StickInCtrlList(0);
		  }
                  break;

  case PS_BCANC:  PSDialog(0);  break;

  case PS_BCENT:  drawIRect(0);
                  centerImage();
                  drawIRect(1);
                  drawPosStr();
                  break;

  case PS_BORG:   drawIRect(0);
                  pos_inx = 0.0;
                  pos_iny = (psizey - sz_iny);
                  drawIRect(1);
                  drawPosStr();
                  break;

  case PS_BMAX:   drawIRect(0);
                  maxImage();
                  drawIRect(1);
                  drawPosStr();
                  drawSizeStr();
                  drawResStr();
                  break;

  case PS_BPOSX:  posxType = (posxType + 1) % 3;
                  drawPosStr();
                  break;

  case PS_BPOSY:  posyType = (posyType + 1) % 3;
                  drawPosStr();
                  break;

  default:        break;
  }
}


/***************************************************/
static void changedScale()
{
  double oldx,oldy;

  drawIRect(0);

  if (lockCB.val) {
    if      (tracking == 1) DSetVal(&ysDial, xsDial.val);
    else if (tracking == 2) DSetVal(&xsDial, ysDial.val);
  }

  oldx = pos_inx;  oldy = pos_iny;
  setScale();

  drawIRect(1);

  if (pos_inx != oldx || pos_iny != oldy ||
      posxType != 0 || posyType != 0) drawPosStr();
  drawSizeStr();
  drawResStr();
  XFlush(theDisp);
}


/***************************************************/
static void setScale()
{
  double hsx, hsy;

  int w,h;

  GetSaveSize(&w, &h);

  sz_inx = (double) w / PIX2INCH * (xsDial.val / 100.0);
  sz_iny = (double) h / PIX2INCH * (ysDial.val / 100.0);

  /* round to integer .001ths of an inch */
  sz_inx = floor(sz_inx * 1000.0 + 0.5) / 1000.0;
  sz_iny = floor(sz_iny * 1000.0 + 0.5) / 1000.0;

  dpix = (int) (PIX2INCH / (xsDial.val / 100.0));
  dpiy = (int) (PIX2INCH / (ysDial.val / 100.0));

  /* make sure 'center' of image is still on page */
  hsx = sz_inx/2;  hsy = sz_iny/2;
  RANGE(pos_inx, -hsx, psizex-hsx);
  RANGE(pos_iny, -hsy, psizey-hsy);

  /* round to integer .001ths of an inch */
  pos_inx = floor(pos_inx * 1000.0 + 0.5) / 1000.0;
  pos_iny = floor(pos_iny * 1000.0 + 0.5) / 1000.0;

}


/***************************************************/
static void changedPaper()
{
  setPaper();
  XClearWindow(theDisp, pageF);
  centerImage();
  drawPosStr();
  drawPage();
}


/***************************************************/
static void setPaper()
{
  int    i;
  double tmp;

  i = RBWhich(paperRB);
  psizex = paperSize[i][0];
  psizey = paperSize[i][1];

  in2pix = (double) PMAX / psizey;

  if (RBWhich(orientRB)==ORNT_LAND) {
    tmp = psizex;  psizex = psizey;  psizey = tmp;
  }

  pageRect.x = (int) ((PMAX/2) - ((psizex/2.0) * in2pix));
  pageRect.y = (int) ((PMAX/2) - ((psizey/2.0) * in2pix));
  pageRect.width  = (int) (psizex * in2pix);
  pageRect.height = (int) (psizey * in2pix);
}


/***************************************************/
static void drawIRect(draw)
     int draw;
{
  int x,y,w,h;
  XRectangle xr;

  x = pageRect.x + (int) (pos_inx * in2pix);
  y = pageRect.y + (int) (pos_iny * in2pix);
  w = sz_inx * in2pix;
  h = sz_iny * in2pix;

  xr.x = pageRect.x + 1;
  xr.y = pageRect.y + 1;
  xr.width  = pageRect.width - 1;
  xr.height = pageRect.height - 1;

  if (draw) XSetForeground(theDisp, theGC, infofg);
       else XSetForeground(theDisp, theGC, infobg);

  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);
  XDrawRectangle(theDisp, pageF, theGC, x, y, (u_int) w, (u_int) h);
  XDrawLine(theDisp, pageF, theGC, x, y, x+w, y+h);
  XDrawLine(theDisp, pageF, theGC, x, y+h, x+w, y);
  XSetClipMask(theDisp, theGC, None);
}



/***************************************************/
static void centerImage()
{
  pos_inx = psizex/2 - sz_inx/2;
  pos_iny = psizey/2 - sz_iny/2;

  /* round to integer .001ths of an inch */
  pos_inx = floor(pos_inx * 1000.0 + 0.5) / 1000.0;
  pos_iny = floor(pos_iny * 1000.0 + 0.5) / 1000.0;
}


/***************************************************/
static void maxImage()
{
  double scx, scy;
  int w,h;

  GetSaveSize(&w, &h);

  sz_inx = psizex - margins[RBWhich(paperRB)][0];
  sz_iny = psizey - margins[RBWhich(paperRB)][1];

  /* choose the smaller scaling factor */
  scx = sz_inx / w;
  scy = sz_iny / h;

  if (scx < scy) { sz_iny = h * scx; }
            else { sz_inx = w * scy; }

  DSetVal(&xsDial, 100 * (sz_inx * PIX2INCH) / w);
  DSetVal(&ysDial, xsDial.val);

  sz_inx = (double) w / PIX2INCH * (xsDial.val / 100.0);
  sz_iny = (double) h / PIX2INCH * (ysDial.val / 100.0);

  /* round to integer .001ths of an inch */
  sz_inx = floor(sz_inx * 1000.0 + 0.5) / 1000.0;
  sz_iny = floor(sz_iny * 1000.0 + 0.5) / 1000.0;

  dpix = (int) (PIX2INCH / (xsDial.val / 100.0));
  dpiy = (int) (PIX2INCH / (ysDial.val / 100.0));

  pos_inx = psizex/2 - sz_inx/2;
  pos_iny = psizey/2 - sz_iny/2;

  /* round to integer .001ths of an inch */
  pos_inx = floor(pos_inx * 1000.0 + 0.5) / 1000.0;
  pos_iny = floor(pos_iny * 1000.0 + 0.5) / 1000.0;
}


/***************************************************/
static void moveImage(newx,newy)
double newx, newy;
{
  double hsx, hsy;

  hsx = sz_inx/2;  hsy = sz_iny/2;

  /* round to integer .001ths of an inch */
  newx = floor(newx * 1000.0 + 0.5) / 1000.0;
  newy = floor(newy * 1000.0 + 0.5) / 1000.0;

  /* keep center of image within page limits */
  RANGE(newx, -hsx, psizex-hsx);
  RANGE(newy, -hsy, psizey-hsy);

  if (newx != pos_inx || newy != pos_iny) {  /* moved */
    drawIRect(0);
    pos_inx = newx;
    pos_iny = newy;
    drawIRect(1);
    drawPosStr();
  }
}


/***************************************************/
static void writePS()
{
  FILE *fp;
  int   i, j, err, rpix, gpix, bpix, nc, ptype;
  int   iw, ih, ox, oy, slen, lwidth, bits, colorps, w, h, pfree;
  double iwf, ihf;
  byte *inpix, *rmap, *gmap, *bmap;

  slen = bits = colorps = 0;


  fp = OpenOutFile(filename);
  if (!fp) return;

  WaitCursor();

  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  if (w <= 0 || h <= 0 || w*2 < w) {
    SetISTR(ISTR_WARNING,"%s:  Image dimensions out of range", filename);
    CloseOutFile(fp, filename, 1);
    return;
  }


  /* printed image will have size iw,ih (in picas) */
  iw = (int) (sz_inx * 72.0 + 0.5);
  ih = (int) (sz_iny * 72.0 + 0.5);
  iwf = sz_inx * 72.0;
  ihf = sz_iny * 72.0;

  /* compute offset to bottom-left of image (in picas) */
  ox = (int) (pos_inx * 72.0 + 0.5);
  oy = (int) ((psizey - (pos_iny + sz_iny)) * 72.0 + 0.5);


  /*** write PostScript header ***/


  fprintf(fp,"%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(fp,"%%%%Title: %s\n",filename);
  fprintf(fp,"%%%%Creator: XV %s  -  by John Bradley\n",REVDATE);

  if (RBWhich(orientRB)==ORNT_LAND)   /* Landscape mode */
    fprintf(fp,"%%%%BoundingBox: %d %d %d %d\n",
	    (int) (pos_iny * 72.0 + 0.5),
	    (int) (pos_inx * 72.0 + 0.5),
	    (int) (pos_iny * 72.0 + 0.5) + ih,
	    (int) (pos_inx * 72.0 + 0.5) + iw);
  else
    fprintf(fp,"%%%%BoundingBox: %d %d %d %d\n", ox, oy, ox+iw, oy+ih);

  fprintf(fp,"%%%%Pages: 1\n");
  fprintf(fp,"%%%%DocumentFonts:\n");
  fprintf(fp,"%%%%EndComments\n");


  switch (colorType) {
  case F_FULLCOLOR:
  case F_REDUCED:   slen = w*3;      bits = 8;  colorps = 1;  break;
  case F_GREYSCALE: slen = w;        bits = 8;  colorps = 0;  break;
  case F_BWDITHER:  slen = (w+7)/8;  bits = 1;  colorps = 0;  break;
  default:  FatalError("unknown colorType in writePS()");   break;
  }

  if (encapsCB.val) epsPreview(fp, inpix, ptype, colorType, w, h,
			       rmap,gmap,bmap,
			       (RBWhich(orientRB)==ORNT_LAND) );

  fprintf(fp,"%%%%EndProlog\n\n");

  fprintf(fp,"%%%%Page: 1 1\n\n");

  fprintf(fp,"%% remember original state\n");
  fprintf(fp,"/origstate save def\n\n");

  fprintf(fp,"%% build a temporary dictionary\n");
  fprintf(fp,"20 dict begin\n\n");

  if (colorType == F_BWDITHER || ptype==PIC24 || !pscompCB.val) {
    fprintf(fp,"%% define string to hold a scanline's worth of data\n");
    fprintf(fp,"/pix %d string def\n\n", slen);
  }

  /*
   * Add loop invariant strings that should not be defined
   * over and over again inside a loop
   * K. Schnepper DLR FF-DR Oberpfaffenhofen, Mai 12. 1993
   */
  fprintf(fp,"%% define space for color conversions\n");
  fprintf(fp,"/grays %d string def  %% space for gray scale line\n", w);
  fprintf(fp,"/npixls 0 def\n");
  fprintf(fp,"/rgbindx 0 def\n\n");


  if (RBWhich(orientRB)==ORNT_LAND) {   /* Landscape mode */
    fprintf(fp,"%% print in landscape mode\n");
    fprintf(fp,"90 rotate 0 %d translate\n\n",(int) (-psizey*72.0));
  }

  if (RBWhich(paperRB) == PSZ_4BY5 ||
      RBWhich(paperRB) == PSZ_35MM) {
    fprintf(fp,"%% we're going to a 4x5 or a 35mm film recorder.\n");
    fprintf(fp,"%% clear page to black to avoid registration problems\n");
    fprintf(fp,"newpath\n");
    fprintf(fp,"  0 0 moveto\n");
    fprintf(fp,"  0 %d rlineto\n", (int) (psizey * 72.0));
    fprintf(fp,"  %d 0 rlineto\n", (int) (psizex * 72.0));
    fprintf(fp,"  0 %d rlineto\n", (int) (-psizey * 72.0));
    fprintf(fp,"  closepath\n");
    fprintf(fp,"  0 setgray\n");
    fprintf(fp,"  fill\n\n");
  }


  fprintf(fp,"%% lower left corner\n");
  fprintf(fp,"%d %d translate\n\n",ox,oy);

  fprintf(fp,"%% size of image (on paper, in 1/72inch coords)\n");
  fprintf(fp,"%.5f %.5f scale\n\n",iwf,ihf);

  if (colorType == F_BWDITHER) {   /* 1-bit dither code uses 'image' */
    int flipbw;

    /* set if color#0 is white */
    flipbw = (MONO(rmap[0],gmap[0],bmap[0]) > MONO(rmap[1],gmap[1],bmap[1]));

    fprintf(fp,"%% dimensions of data\n");
    fprintf(fp,"%d %d %d\n\n",w,h,bits);

    fprintf(fp,"%% mapping matrix\n");
    fprintf(fp,"[%d 0 0 %d 0 %d]\n\n", w, -h, h);

    fprintf(fp,"{currentfile pix readhexstring pop}\n");
    fprintf(fp,"image\n");

    /* write the actual image data */

    err = writeBWStip(fp, inpix, "", w, h, flipbw);
  }

  else {      /* all other formats */
    byte *rleline = (byte *) NULL;
    unsigned long outbytes = 0;

    /* if we're using color, make sure 'colorimage' is defined */
    if (colorps) psColorImage(fp);

    if (ptype==PIC8 && pscompCB.val) {  /* write cmap & rle-cmapped image fn */
      psColorMap(fp, colorps, nc, rmap, gmap, bmap);
      psRleCmapImage(fp, colorps);
    }

    fprintf(fp,"%d %d %d\t\t\t%% dimensions of data\n",w,h,bits);
    fprintf(fp,"[%d 0 0 %d 0 %d]\t\t%% mapping matrix\n", w, -h, h);

    if (ptype==PIC8 && pscompCB.val) fprintf(fp,"rlecmapimage\n");
    else {
      fprintf(fp,"{currentfile pix readhexstring pop}\n");
      if (colorps) fprintf(fp,"false 3 colorimage\n");
              else fprintf(fp,"image\n");
    }

    /* dump the image data to the file */
    err = 0;

    if (ptype==PIC8 && pscompCB.val) {
      rleline  = (byte *) malloc((size_t) w * 2);  /* worst-case */
      if (!rleline) FatalError("unable to malloc rleline in writePS()\n");
    }

    for (i=0; i<h && err != EOF; i++) {
      int rlen;
      lwidth = 0;
      putc('\n',fp);

      if ((i&0x1f) == 0) WaitCursor();

      if (ptype==PIC8 && pscompCB.val) { /* write rle-encoded cmapped image */
	rlen = rle_encode(inpix, rleline, w);
	inpix += w;
	outbytes += rlen;

	for (j=0; j<rlen && err != EOF; j++) {
	  err = fprintf(fp,"%02x", rleline[j]);
	  lwidth += 2;

	  if (lwidth>70) { putc('\n',fp); lwidth = 0; }
	}
      }

      else {  /* write non-rle raw (gray/rgb) image data */
	for (j=0; j<w && err != EOF; j++) {

	  if (ptype == PIC8) {
	    rpix = rmap[*inpix];
	    gpix = gmap[*inpix];
	    bpix = bmap[*inpix];
	  }
	  else {  /* PIC24 */
	    rpix = inpix[0];
	    gpix = inpix[1];
	    bpix = inpix[2];
	  }

	  if (colorps) {
	    err = fprintf(fp,"%02x%02x%02x",rpix,gpix,bpix);
	    lwidth+=6;
	  }

	  else {  /* greyscale */
	    err = fprintf(fp,"%02x", MONO(rpix,gpix,bpix));
	    lwidth+=2;
	  }

	  if (lwidth>70) { putc('\n',fp); lwidth = 0; }

	  inpix += (ptype==PIC24) ? 3 : 1;
	}
      }
    }

    if (ptype==PIC8 && pscompCB.val) {
      free(rleline);
      fprintf(fp,"\n\n");
      fprintf(fp,"%%\n");
      fprintf(fp,"%% Compression made this file %.2f%% %s\n",
	      100.0 * ((double) outbytes) /
	      ((double) eWIDE * eHIGH * ((colorps) ? 3 : 1)),
	      "of the uncompressed size.");
      fprintf(fp,"%%\n");
    }
  }


  fprintf(fp,"\n\nshowpage\n\n");

  fprintf(fp,"%% stop using temporary dictionary\n");
  fprintf(fp,"end\n\n");

  fprintf(fp,"%% restore original state\n");
  fprintf(fp,"origstate restore\n\n");
  fprintf(fp,"%%%%Trailer\n");

  if (pfree) free(inpix);

  if (CloseOutFile(fp, filename, (err==EOF)) == 0) {
    DirBox(0);
  }

  SetCursors(-1);
}


/**********************************************/
static int rle_encode(scanline, rleline, wide)
     byte *scanline, *rleline;
     int wide;
{
  /* generates a rle-compressed version of the scan line.
   * rle is encoded as such:
   *    <count> <value>                      # 'run' of count+1 equal pixels
   *    <count | 0x80> <count+1 data bytes>  # count+1 non-equal pixels
   *
   * count can range between 0 and 127
   *
   * returns length of the rleline vector
   */

  int  i, j, blocklen, isrun, rlen;
  byte block[256], pix;

  blocklen = isrun = rlen = 0;

  for (i=0; i<wide; i++) {
    /* there are 5 possible states:
     *   0: block empty.
     *   1: block not empty, block is  a run, current pix == previous pix
     *   2: block not empty, block is  a run, current pix != previous pix
     *   3: block not empty, block not a run, current pix == previous pix
     *   4: block not empty, block not a run, current pix != previous pix
     */

    pix = scanline[i];

    if (!blocklen) {                    /* case 0:  empty */
      block[blocklen++] = pix;
      isrun = 1;
    }

    else if (isrun) {
      if (pix == block[blocklen-1]) {   /* case 1:  isrun, prev==cur */
	block[blocklen++] = pix;
      }
      else {                            /* case 2:  isrun, prev!=cur */
	if (blocklen>1) {               /*   we have a run block to flush */
	  rleline[rlen++] = blocklen-1;
	  rleline[rlen++] = block[0];
	  block[0] = pix;               /*   start new run block with pix */
	  blocklen = 1;
	}
	else {
	  isrun = 0;                    /*   blocklen<=1, turn into non-run */
	  block[blocklen++] = pix;
	}
      }
    }

    else {   /* not a run */
      if (pix == block[blocklen-1]) {   /* case 3:  non-run, prev==cur */
	if (blocklen>1) {               /*  have a non-run block to flush */
	  rleline[rlen++] = (blocklen-1) | 0x80;
	  for (j=0; j<blocklen; j++)
	    rleline[rlen++] = block[j];

	  block[0] = pix;               /*  start new run block with pix */
	  blocklen = isrun = 1;
	}
	else {
	  isrun = 1;                    /*  blocklen<=1 turn into a run */
	  block[blocklen++] = pix;
	}
      }
      else {                            /* case 4:  non-run, prev!=cur */
	block[blocklen++] = pix;
      }
    }

    if (blocklen == 128) {   /* max block length.  flush */
      if (isrun) {
	rleline[rlen++] = blocklen-1;
	rleline[rlen++] = block[0];
      }

      else {
	rleline[rlen++] = (blocklen-1) | 0x80;
	for (j=0; j<blocklen; j++)
	  rleline[rlen++] = block[j];
      }

      blocklen = 0;
    }
  }

  if (blocklen) {   /* flush last block */
    if (isrun) {
      rleline[rlen++] = blocklen-1;
      rleline[rlen++] = block[0];
    }

    else {
      rleline[rlen++] = (blocklen-1) | 0x80;
      for (j=0; j<blocklen; j++)
	rleline[rlen++] = block[j];
    }
  }

  return rlen;
}


/**********************************************/
static void psColorImage(fp)
FILE *fp;
{
  /* spits out code that checks if the PostScript device in question
     knows about the 'colorimage' operator.  If it doesn't, it defines
     'colorimage' in terms of image (ie, generates a greyscale image from
     RGB data) */


  fprintf(fp,"%% define 'colorimage' if it isn't defined\n");
  fprintf(fp,"%%   ('colortogray' and 'mergeprocs' come from xwd2ps\n");
  fprintf(fp,"%%     via xgrab)\n");
  fprintf(fp,"/colorimage where   %% do we know about 'colorimage'?\n");
  fprintf(fp,"  { pop }           %% yes: pop off the 'dict' returned\n");
  fprintf(fp,"  {                 %% no:  define one\n");
  fprintf(fp,"    /colortogray {  %% define an RGB->I function\n");
  fprintf(fp,"      /rgbdata exch store    %% call input 'rgbdata'\n");
  fprintf(fp,"      rgbdata length 3 idiv\n");
  fprintf(fp,"      /npixls exch store\n");
  fprintf(fp,"      /rgbindx 0 store\n");
  fprintf(fp,"      0 1 npixls 1 sub {\n");
  fprintf(fp,"        grays exch\n");
  fprintf(fp,"        rgbdata rgbindx       get 20 mul    %% Red\n");
  fprintf(fp,"        rgbdata rgbindx 1 add get 32 mul    %% Green\n");
  fprintf(fp,"        rgbdata rgbindx 2 add get 12 mul    %% Blue\n");
  fprintf(fp,"        add add 64 idiv      %% I = .5G + .31R + .18B\n");
  fprintf(fp,"        put\n");
  fprintf(fp,"        /rgbindx rgbindx 3 add store\n");
  fprintf(fp,"      } for\n");
  fprintf(fp,"      grays 0 npixls getinterval\n");
  fprintf(fp,"    } bind def\n\n");

  fprintf(fp,"    %% Utility procedure for colorimage operator.\n");
  fprintf(fp,"    %% This procedure takes two procedures off the\n");
  fprintf(fp,"    %% stack and merges them into a single procedure.\n\n");

  fprintf(fp,"    /mergeprocs { %% def\n");
  fprintf(fp,"      dup length\n");
  fprintf(fp,"      3 -1 roll\n");
  fprintf(fp,"      dup\n");
  fprintf(fp,"      length\n");
  fprintf(fp,"      dup\n");
  fprintf(fp,"      5 1 roll\n");
  fprintf(fp,"      3 -1 roll\n");
  fprintf(fp,"      add\n");
  fprintf(fp,"      array cvx\n");
  fprintf(fp,"      dup\n");
  fprintf(fp,"      3 -1 roll\n");
  fprintf(fp,"      0 exch\n");
  fprintf(fp,"      putinterval\n");
  fprintf(fp,"      dup\n");
  fprintf(fp,"      4 2 roll\n");
  fprintf(fp,"      putinterval\n");
  fprintf(fp,"    } bind def\n\n");

  fprintf(fp,"    /colorimage { %% def\n");
  fprintf(fp,"      pop pop     %% remove 'false 3' operands\n");
  fprintf(fp,"      {colortogray} mergeprocs\n");
  fprintf(fp,"      image\n");
  fprintf(fp,"    } bind def\n");
  fprintf(fp,"  } ifelse          %% end of 'false' case\n");
  fprintf(fp,"\n\n\n");
}


/**********************************************/
static void psColorMap(fp, color, nc, rmap, gmap, bmap)
     FILE *fp;
     int color, nc;
     byte *rmap, *gmap, *bmap;
{
  /* spits out code for the colormap of the following image
     if !color, it spits out a mono-ized graymap */

  int i;

  fprintf(fp,"%% define the colormap\n");
  fprintf(fp,"/cmap %d string def\n\n\n", nc * ((color) ? 3 : 1));

  fprintf(fp,"%% load up the colormap\n");
  fprintf(fp,"currentfile cmap readhexstring\n");

  for (i=0; i<nc; i++) {
    if (color) fprintf(fp,"%02x%02x%02x ", rmap[i],gmap[i],bmap[i]);
    else fprintf(fp,"%02x ", MONO(rmap[i],gmap[i],bmap[i]));

    if ((i%10) == 9) fprintf(fp,"\n");
  }
  if (i%10) fprintf(fp,"\n");
  fprintf(fp,"pop pop   %% lose return values from readhexstring\n\n\n");

}


/**********************************************/
static void psRleCmapImage(fp, color)
FILE *fp;
int   color;
{
  /* spits out code that defines the 'rlecmapimage' operator */

  fprintf(fp,"%% rlecmapimage expects to have 'w h bits matrix' on stack\n");
  fprintf(fp,"/rlecmapimage {\n");
  fprintf(fp,"  /buffer 1 string def\n");
  fprintf(fp,"  /rgbval 3 string def\n");
  fprintf(fp,"  /block  384 string def\n\n");

  fprintf(fp,"  %% proc to read a block from file, and return RGB data\n");
  fprintf(fp,"  { currentfile buffer readhexstring pop\n");
  fprintf(fp,"    /bcount exch 0 get store\n");
  fprintf(fp,"    bcount 128 ge\n");
  fprintf(fp,"    {  %% it's a non-run block\n");
  fprintf(fp,"      0 1 bcount 128 sub\n");
  fprintf(fp,"      { currentfile buffer readhexstring pop pop\n\n");

  if (color) {
    fprintf(fp,"        %% look up value in color map\n");
    fprintf(fp,"%s/rgbval cmap buffer 0 get 3 mul 3 getinterval store\n\n",
	    "        ");
    fprintf(fp,"        %% and put it in position i*3 in block\n");
    fprintf(fp,"        block exch 3 mul rgbval putinterval\n");
    fprintf(fp,"      } for\n");
    fprintf(fp,"      block  0  bcount 127 sub 3 mul  getinterval\n");
    fprintf(fp,"    }\n\n");
  }
  else {
    fprintf(fp,"        %% look up value in gray map\n");
    fprintf(fp,"%s/rgbval cmap buffer 0 get 1 getinterval store\n\n",
	    "        ");
    fprintf(fp,"        %% and put it in position i in block\n");
    fprintf(fp,"        block exch rgbval putinterval\n");
    fprintf(fp,"      } for\n");
    fprintf(fp,"      block  0  bcount 127 sub  getinterval\n");
    fprintf(fp,"    }\n\n");
  }

  fprintf(fp,"    { %% else it's a run block\n");
  fprintf(fp,"      currentfile buffer readhexstring pop pop\n\n");

  if (color) {
    fprintf(fp,"      %% look up value in colormap\n");
    fprintf(fp,"%s/rgbval cmap buffer 0 get 3 mul 3 getinterval store\n\n",
	    "      ");
    fprintf(fp,"%s0 1 bcount { block exch 3 mul rgbval putinterval } for\n\n",
	    "      ");
    fprintf(fp,"      block 0 bcount 1 add 3 mul getinterval\n");
  }
  else {
    fprintf(fp,"      %% look up value in graymap\n");
    fprintf(fp,"      /rgbval cmap buffer 0 get 1 getinterval store\n\n");
    fprintf(fp,"      0 1 bcount { block exch rgbval putinterval } for\n\n");
    fprintf(fp,"      block 0 bcount 1 add getinterval\n");
  }

  fprintf(fp,"    } ifelse\n");
  fprintf(fp,"  } %% end of proc\n");

  if (color) fprintf(fp,"  false 3 colorimage\n");
        else fprintf(fp,"  image\n");

  fprintf(fp,"} bind def\n\n\n");
}



/**********************************************/
static void epsPreview(fp, pic, ptype, colorType, w, h, rmap,gmap,bmap,
		       landscape)
     FILE *fp;
     byte *pic;
     int   ptype, colorType;
     int   w, h, landscape;
     byte *rmap, *gmap, *bmap;
{
  byte *prev;
  int flipbw;


  if (landscape) {  /* generate a rotated version of the pic */
    int npixels, bufsize;
    byte *lpic;

    npixels = w * h;
    if (w <= 0 || h <= 0 || npixels/w != h) {
      SetISTR(ISTR_WARNING,"%s:  Image dimensions out of range", filename);
/*    CloseOutFile(fp, filename, 1);  can't do since caller still writing */
      return;
    }
    if (ptype == PIC8)
      bufsize = npixels;
    else {
      bufsize = 3*npixels;
      if (bufsize/3 != npixels) {
        SetISTR(ISTR_WARNING,"%s:  Image dimensions out of range", filename);
/*      CloseOutFile(fp, filename, 1);  can't do since caller still writing */
        return;
      }
    }

    lpic = (byte *) malloc((size_t) bufsize);
    if (!lpic) FatalError("can't alloc mem to rotate image in epsPreview");

    xvbcopy((char *) pic, (char *) lpic, (size_t) bufsize);
    RotatePic(lpic, ptype, &w, &h, 0);
    pic = lpic;
  }


  /* put in an EPSI preview */

  if (colorType != F_BWDITHER) { /* have to generate a preview */
    prev = FSDither(pic, ptype, w, h, rmap,gmap,bmap, 0, 1);

    if (!prev) {
      fprintf(stderr,"Unable to malloc in epsPreview\n");
      return;
    }

    flipbw = 0;
  }
  else {
    prev = pic;
    /* set if color#0 is white */
    flipbw = (MONO(rmap[0],gmap[0],bmap[0]) > MONO(rmap[1],gmap[1],bmap[1]));
  }


  fprintf(fp,"%%%%BeginPreview: %d %d %d %d\n", w, h, 1,
	  (w/(72*4) + 1) * h);

  writeBWStip(fp, prev, "% ", w, h, !flipbw);

  fprintf(fp,"%%%%EndPreview\n");

  if (colorType != F_BWDITHER) free(prev);
  if (landscape) free(pic);  /* lpic, actually... */
}


/***********************************/
static int writeBWStip(fp, pic, prompt, w, h, flipbw)
     FILE *fp;
     byte *pic;
     const char *prompt;
     int  w, h, flipbw;
{
  /* write the given 'pic' (B/W stippled, 1 byte per pixel, 0=blk,1=wht)
     out as hexadecimal, max of 72 hex chars per line.

     if 'flipbw', then 0=white, 1=black

     returns '0' if everythings fine, 'EOF' if writing failed */

  int err, i, j, lwidth;
  byte outbyte, bitnum, bit;

  err = 0;

  for (i=0; i<h && err != EOF; i++) {
    fprintf(fp, "%s", prompt);

    outbyte = bitnum = lwidth = 0;

    if ((i&0x3f) == 0) WaitCursor();

    for (j=0; j<w && err != EOF; j++) {
      bit = *pic;
      outbyte = (outbyte<<1) | ((bit)&0x01);
      bitnum++;

      if (bitnum==8) {
	if (flipbw) outbyte = ~outbyte & 0xff;
	err = fprintf(fp,"%02x",outbyte);
	lwidth+=2;
	outbyte = bitnum = 0;
      }

      if (lwidth>=72 && j+1<w) { fprintf(fp, "\n%s", prompt); lwidth = 0; }
      pic++;
    }

    if (bitnum) {   /* few bits left over... */
      for ( ; bitnum<8; bitnum++) outbyte <<= 1;
      if (flipbw) outbyte = ~outbyte & 0xff;
      err = fprintf(fp,"%02x",outbyte);
      lwidth+=2;
    }

    fprintf(fp, "\n");
  }

  return err;
}






/***********************************/
int LoadPS(fname, pinfo, quick)
     char    *fname;
     PICINFO *pinfo;
     int      quick;
{
  /* returns '1' if successful.  If the document is a single page, the
     a temporary PNM file is created, loaded, and deleted.  If the
     document is multiple pages, a series of PNM files are created, and
     the first one is loaded (but not deleted) */

#ifdef GS_PATH
  #define CMDSIZE	1024
  char tmp[512], gscmd[512], cmdstr[CMDSIZE], tmpname[64];
  int  gsresult, nump, i, filetype, doalert, epsf;
#endif

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

#ifdef GS_PATH

  doalert = (!quick && !ctrlUp && !infoUp);  /* open alert if no info wins */
  epsf    = 0;

#ifndef VMS
  sprintf(tmpname, "%s/xvpgXXXXXX", tmpdir);
#else
  sprintf(tmpname, "Sys$Scratch:xvpgXXXXXX");
#endif

#ifdef USE_MKSTEMP
  close(mkstemp(tmpname));
#else
  mktemp(tmpname);
#endif
  if (tmpname[0] == '\0') {   /* mktemp() or mkstemp() blew up */
    sprintf(dummystr,"LoadPS: Unable to create temporary filename???");
    ErrPopUp(dummystr, "\nHow unlikely!");
    return 0;
  }
  strcat(tmpname,".");


  /* build 'gscmd' string */

#ifndef VMS  /* VMS needs quotes around mixed case command lines */
  sprintf(gscmd, "%s -sDEVICE=%s -r%d -q -dSAFER -dNOPAUSE -sOutputFile=%s%%d ",
	  GS_PATH, gsDev, gsRes, tmpname);
#else
  sprintf(gscmd,
	  "%s \"-sDEVICE=%s\" -r%d -q \"-dNOPAUSE\" \"-sOutputFile=%s%%d\" ",
	  GS_PATH, gsDev, gsRes, tmpname);
#endif


#ifdef GS_LIB
#  ifndef VMS
     sprintf(tmp, "-I%s ", GS_LIB);
#  else
     sprintf(tmp, "\"-I%s\" ", GS_LIB);
#  endif
   strcat(gscmd, tmp);
#endif


   /* prevent some potential naughtiness... */
#ifndef VMS
   strcat(gscmd, "-dSAFER ");
#else
   strcat(gscmd, "\"-dSAFER\" ");
#endif


  if (gsGeomStr) {
    sprintf(tmp, "-g%s ", gsGeomStr);
    strcat(gscmd, tmp);
  }


  do {
    buildCmdStr(cmdstr, gscmd, fname, quick, epsf);

    if (DEBUG) fprintf(stderr,"LoadPS:  executing command '%s'\n", cmdstr);
    SetISTR(ISTR_INFO, "Running '%s'...", GS_PATH);
    sprintf(tmp, "Running %s", cmdstr);
    if (doalert && epsf==0) OpenAlert(tmp);  /* open alert first time only */

    WaitCursor();
    gsresult = system(cmdstr);
    WaitCursor();
#ifdef VMS
    gsresult = !gsresult;   /* VMS returns non-zero if OK */
#endif

    /* count # of files produced... */
    for (i=1; i<1000; i++) {
      struct stat st;
      sprintf(tmp, "%s%d", tmpname, i);
      if (stat(tmp, &st)!=0) break;
    }
    nump = i-1;
    WaitCursor();

    /* EPSF hack:  if gsresult==0 (OK) and 0 pages produced,
       try tacking a 'showpage' onto the end of the file, do it again... */

    if (!gsresult && !nump && !epsf) epsf++;
  } while (!gsresult && !nump && epsf<2);

  if (doalert) CloseAlert();


  WaitCursor();

  if (DEBUG) fprintf(stderr,"%s: %d pages found\n", fname, nump);

  if (gsresult) {
    SetISTR(ISTR_INFO, "");
    SetISTR(ISTR_WARNING,"Ghostscript interpreter returned error code %d.",
	    gsresult);
    KillPageFiles(tmpname, nump);
    SetCursors(-1);
    return 0;
  }
  else {
    if (nump<1) {
      SetISTR(ISTR_INFO, "Ghostscript: No pages produced.");
      if (!quick) Warning();
      SetCursors(-1);
      return 0;
    }

    SetISTR(ISTR_INFO, "Running '%s'...  Done.  (%d page%s)",
	    GS_PATH, nump, (nump==1) ? "" : "s");
  }


  /* from this point on, the 'gs' command was successful, and page files
     were produced.  Try to load the first (or only) page image created.
     Note that if there is only one page, the page file is deleted,
     as it won't be needed. */


  sprintf(tmp, "%s%d", tmpname, 1);
  filetype = ReadFileType(tmp);

  if (filetype == RFT_ERROR || filetype == RFT_UNKNOWN ||
      filetype == RFT_COMPRESS) {  /* shouldn't happen */
    SetISTR(ISTR_WARNING, "Couldn't load first page '%s'", tmp);
    KillPageFiles(tmpname, nump);
    SetCursors(-1);
    return 0;
  }


  i = ReadPicFile(tmp, filetype, pinfo, quick);
  if (nump == 1) unlink(tmp);

  if (!i) {  /* failed to read page 1 */
    SetISTR(ISTR_WARNING, "Couldn't load first page '%s'", tmp);
    KillPageFiles(tmpname, nump);
    SetCursors(-1);
    return 0;
  }


  /* SUCCESS! */

  if (nump>1) {
    strcpy(pinfo->pagebname, tmpname);
  }
  pinfo->numpages = nump;

#endif  /* GS_PATH */


  return 1;   /* can safely return '1' as this should never be called if
		 we don't have 'gs' package */
}



/******************************************************************/
#ifdef GS_PATH
void buildCmdStr(str, gscmd, xname, quick, epsf)
     char *str, *gscmd, *xname;
     int   quick, epsf;
{
  /* note 'epsf' set only on files that don't have a showpage cmd */
  char *x, *y, *fname;

  x = (char *) malloc((5 * strlen(xname))+3);
  if (!x)
	FatalError("malloc failure in xvps.c buildCmdStr");
  fname = x;
  *x++ = 0x27;

  for (y = xname; *y; ++y) {
     if (0x27 == *y) {
       strcpy(x, "'\"'\"'");
       x += strlen(x);
     } else *x++ = *y;
  }
  strcpy (x, "'");

#ifndef VMS

  if      (epsf)  snprintf(str, CMDSIZE, "echo '\n showpage ' | cat %s - | %s -",
			  fname, gscmd);

  else if (quick) snprintf(str, CMDSIZE, "echo %s | cat - %s | %s -",
			  "/showpage { showpage quit } bind def",
			  fname,  gscmd);

  else            snprintf(str, CMDSIZE, "%s -- %s", gscmd, fname);

#else /* VMS */
  /* VMS doesn't have pipes or an 'echo' command and GS doesn't like
     Unix-style filenames as input files in the VMS version */
  strcat(tmp, " -- ");
  rld = strrchr(fname, '/');     /* Pointer to last '/' */
  if (rld) rld++;                /* Pointer to filename */
  else rld = fname;              /* No path - use original string */
  strcat(tmp, rld);
#endif  /* VMS */
  free(fname);
}
#endif  /* GS_PATH */



