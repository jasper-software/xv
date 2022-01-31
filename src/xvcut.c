/*
 * xvcut.c  -  xv cut-n-paste (and eventually drag-n-drop) related functions
 *
 * Contains:
 *             int  CutAllowed();
 *             int  PasteAllowed();
 *             void DoImgCopy();
 *             void DoImgCut();
 *             void DoImgClear();
 *             void DoImgPaste();
 *
 *      static byte *getSelection     ();
 *      static byte *getFromClip      ();
 *             void  SaveToClip       (data);
 *      static void  clearSelectedArea();
 *      static void  makeClipFName    ();
 *      static int   countcols24      (byte *, int,int, int,int,int,int));
 *      static int   countNewCols     (byte*, int, int, byte*, int,
 *                                     int, int, int, int);
 *
 *             void  InitSelection  ();
 *             int   HaveSelection  ();
 *             int   GetSelType     ();
 *             void  GetSelRCoords  (&x, &y, &w, &h);
 *             void  EnableSelection(onoff);
 *             void  DrawSelection  (onoff);
 *             int   DoSelection    (XButtonEvent *);
 *      static int   dragHandle     (XButtonEvent *);
 *      static void  dragSelection  (XButtonEvent *, u_int bmask, dndrop);
 *      static void  rectSelection  (XButtonEvent *);
 *             void  MoveGrowSelection(dx,dy,dw,dh);
 *
 *             void  BlinkSelection (cnt);
 *             void  FlashSelection (cnt);
 *
 *      static void  makePasteSel    (data);
 *
 *             void  CropRect2Rect    (int*,int*,int*,int*, int,int,int,int);
 *
 * coordinate system transforms between pic, cpic, and epic coords
 *             void  CoordE2C(ex, ey, &cx, &cy);
 *             void  CoordC2E(cx, cy, &ex, &ey);
 *             void  CoordP2C(px, py, &cx, &cy);
 *             void  CoordC2P(cx, cy, &px, &py);
 *             void  CoordE2P(ex, ey, &px, &py);
 *             void  CoordP2E(px, py, &ex, &ey);
 */


#define  NEEDSDIR               /* for stat() */
#include "copyright.h"
#include "xv.h"

#include "bits/cut"
#include "bits/cutm"
#include "bits/copy"
#include "bits/copym"

#define DBLCLKTIME 500
#define CLIPPROP   "XV_CLIPBOARD"



/***
 *** local functions
 ***/

static void  doPaste           PARM((byte *));
static void  buildCursors      PARM((void));
static byte *getSelection      PARM((void));
static byte *getFromClip       PARM((void));
static void clearSelectedArea  PARM((void));
static void makeClipFName      PARM((void));
static int  countcols24        PARM((byte *, int, int, int, int, int, int));
static int  countNewCols       PARM((byte *, int, int, byte *, int,
				     int, int, int, int));
static int  dragHandle         PARM((XButtonEvent *));
static void dragSelection      PARM((XButtonEvent *, u_int, int));
static void rectSelection      PARM((XButtonEvent *));
static void makePasteSel       PARM((byte *));

static void CoordE2Prnd        PARM((int, int, int *, int *));


/***
 *** local data
 ***/

static char *clipfname = (char *) NULL;
static char  clipfnstr[MAXPATHLEN];

static int   selrx, selry, selrw, selrh;   /* PIC coords of sel. rect */
static int   haveSel;                          /* have a selection? */
static int   selType;                          /* type of sel. (RECT, etc) */
static int   selFilled;                        /* if true flash whole sel */
static int   selColor;                         /* selection 'mask' 0-7 */
static int   selTracking;                      /* in a 'tracking' loop */

static Cursor dragcurs = (Cursor) 0;
static Cursor copycurs = (Cursor) 0;
static Cursor cutcurs  = (Cursor) 0;




/********************************************/
int CutAllowed()
{
  /* returns '1' if cut/copy/clear commands should be enabled (ie, if
     there's a selection of some sort */

  return (but[BCROP].active);
}


/********************************************/
int PasteAllowed()
{
  Atom        clipAtom;
  struct stat st;

  /* returns '1' if there's something on the clipboard to be pasted */

  /* also, can't paste if we're in a root mode */
  if (useroot) return 0;

  /* see if there's a CLIPPROP associated with the root window,
     if there is, we'll use that as the clipboard:  return '1' */

  clipAtom = XInternAtom(theDisp, CLIPPROP, True);
  if (clipAtom != None) return 1;   /* clipboard property exists: can paste */


  /* barring that, see if the CLIPFILE exists. if so, we can paste */
  if (!clipfname) makeClipFName();
  if (stat(clipfname, &st)==0) return 1;       /* file exists: can paste */

  return 0;
}


/********************************************/
void DoImgCopy()
{
  /*
   * called when 'Copy' command is issued.  does entire user interface,
   * such as it is, and puts appropriate data onto the clipboard.
   * Does all complaining on any errors
   */

  byte *cimg;

  if (!HaveSelection())     return;
  cimg = getSelection();
  if (!cimg) return;

  SaveToClip(cimg);
  free(cimg);

  FlashSelection(2);
  SetCursors(-1);
}



/********************************************/
void DoImgCut()
{
  /*
   * called when 'Cut' command is issued.  does entire user interface,
   * such as it is, and puts appropriate data onto the clipboard.
   * Does all complaining on any errors
   */

  byte *cimg;

  if (!HaveSelection()) return;
  FlashSelection(2);

  cimg = getSelection();
  if (!cimg) return;

  SaveToClip(cimg);
  free(cimg);

  clearSelectedArea();
  SetCursors(-1);
}



/********************************************/
void DoImgClear()
{
  /* called when 'Clear' command is issued */

  if (!HaveSelection()) return;
  FlashSelection(2);
  clearSelectedArea();
  SetCursors(-1);
}



/********************************************/
void DoImgPaste()
{
  byte *cimg;

  if (!PasteAllowed()) { XBell(theDisp, 0);  return; }

  cimg = getFromClip();
  if (!cimg) return;

  /* if there's no selection, make one! */
  if (!HaveSelection()) makePasteSel(cimg);
                   else doPaste(cimg);

  free(cimg);
  SetCursors(-1);
}


/********************************************/
static void doPaste(cimg)
     byte *cimg;
{
  /*
   * This is fairly hairy.
   */

  byte *dp, *dpic, *clippic, *clipcmap;
  int   clipw, cliph, clipis24, len, istran, trval;
  int   i, j, sx,sy,sw,sh, cx,cy,cw,ch, dx,dy,dw,dh;


  /*
   * verify clipboard data
   */

  if (!cimg) return;

  len = ((int)  cimg[CIMG_LEN + 0])      |
        ((int) (cimg[CIMG_LEN + 1]<<8))  |
	((int) (cimg[CIMG_LEN + 2]<<16)) |
	((int) (cimg[CIMG_LEN + 3]<<24));

  if (len < CIMG_PIC24) return;

  istran    = cimg[CIMG_TRANS];
  trval     = cimg[CIMG_TRVAL];
  clipis24  = cimg[CIMG_24];
  clipw     = cimg[CIMG_W] | ((int) (cimg[CIMG_W+1]<<8));
  cliph     = cimg[CIMG_H] | ((int) (cimg[CIMG_H+1]<<8));
  if (clipw<1 || cliph<1) return;

  if (( clipis24 && len != CIMG_PIC24 + clipw * cliph * 3) ||
      (!clipis24 && len != CIMG_PIC8  + clipw * cliph)        ) {
    ErrPopUp("The clipboard data is not in a recognized format!", "\nMoof!");
    goto exit;
  }

  clippic  = cimg + ((clipis24) ? CIMG_PIC24 : CIMG_PIC8);
  clipcmap = cimg + CIMG_CMAP;


  /* determine if we're going to promote 'pic' to 24-bits (if it isn't
   * already, because if we *are*, we'd prefer to do any clipboard rescaling
   * in 24-bit space for the obvious reasons.
   *
   * possibilities:
   *   PIC24  -  easy, do clipboard rescale in 24-bit space
   *   PIC8, and clipboard is 8 bits, (or 24-bits, but with <=256 colors)
   *      and total unique colors < 256:
   *         do 8-bit rescale & paste, don't ask anything
   *   PIC8, and clipboard is 24 bits, or 8-bits but total # of cols >256:
   *         *ask* if they want to promote pic.  if so, do it, otherwise
   *         do clipboard rescale in 8-bits, and do the best we can...
   */


  GetSelRCoords(&sx, &sy, &sw, &sh);  /* sel rect in pic coords */

  /* dx,dy,dw,dh is the rectangle (in PIC coords) where the paste will occur
     (cropped to be entirely within PIC */

  dx = sx;  dy = sy;  dw = sw;  dh = sh;
  CropRect2Rect(&dx, &dy, &dw, &dh, 0, 0, pWIDE, pHIGH);


  /* cx,cy,cw,ch is the rectangle of the clipboard data (in clipboard coords)
     that will actually be used in the paste operation */

  cx = (sx>=0) ? 0 : ((-sx) * clipw) / sw;
  cy = (sy>=0) ? 0 : ((-sy) * cliph) / sh;
  cw = (dw * clipw) / sw;
  ch = (dh * cliph) / sh;


  /* see if we have to ask any questions */

  if (picType == PIC8) {
    int ncc, keep8;
    char buf[512];

    if (clipis24) { /* pasting in a 24-bit image that *requires* promotion */
      static const char *labels[] = { "\nOkay", "\033Cancel" };

      strcpy(buf, "Warning:  Pasting this 24-bit image will require ");
      strcat(buf, "promoting the current image to 24 bits.");

      if (PopUp(buf, labels, 2)) goto exit;   /* Cancelled */
      else Change824Mode(PIC24);              /* promote pic to 24 bits */
    }

    else {   /* clip is 8 bits */
      ncc = countNewCols(clippic,clipw,cliph,clipcmap,clipis24,cx,cy,cw,ch);

      if (ncc + numcols > 256) {
	static const char *labels[] = { "\nPromote", "8Keep 8-bit", "\033Cancel" };

	strcpy(buf,"Warning:  The image and the clipboard combine to have ");
	strcat(buf,"more than 256 unique colors.  Promoting the ");
	strcat(buf,"image to 24 bits is recommended, otherwise the contents ");
	strcat(buf,"of the clipboard will probably lose some colors.");

	keep8 = PopUp(buf, labels, 3);
	if      (keep8==2) goto exit;              /* Cancel */
	else if (keep8==0) Change824Mode(PIC24);   /* promote pic to 24 bits */
      }
    }
  }





  /* legal possibilities at this point:
   *   pic is PIC24:  clip is 8 or 24
   *   pic is PIC8:   clip is 8, or clip is 24 but has 256 or fewer colors
   */



  if (picType == PIC8) {
    int   clx, cly, r,g,b,k,mind,close,newcols;
    byte *cp, *clp, *pp, newr[256], newg[256], newb[256], remap[256];
    byte  order[256], trans[256];
    int   bperpix, dpncols;

    dpic = (byte *) malloc((size_t) dw * dh);
    if (!dpic) FatalError("Out of memory in DoImgPaste()\n");

    bperpix = (clipis24) ? 3 : 1;
    newcols = 0;

    /* dpic = a scaled, 8-bit representation of clippic[cx,cy,cw,ch] */

    if (!clipis24) {   /* copy colormap from clip data into newr,g,b[] */
      for (i=0; i<256; i++) {
	newr[i] = clipcmap[i*3];
	newg[i] = clipcmap[i*3 + 1];
	newb[i] = clipcmap[i*3 + 2];
      }
    }

    for (i=0; i<dh; i++) {                     /* un-smooth 8-bit resize */
      dp = dpic + i*dw;
      cly = cy + (i * ch) / dh;
      clp = clippic + (cly*clipw * bperpix);

      for (j=0; j<dw; j++, dp++) {
	/* get appropriate pixel from clippic */
	clx = cx + (j * cw) / dw;
	cp = clp + (clx * bperpix);

	if (!clipis24) *dp = *cp;
	else {                            /* build colormap as we go... */
	  r = *cp++;  g = *cp++;  b = *cp++;

	  /* look it up in new colormap, add if not there */
	  for (k=0; k<newcols && (r!=newr[k] || g!=newg[k] ||b!=newb[k]); k++);
	  if (k==newcols && k<256) {
	    newr[k]=r;  newg[k]=g;  newb[k]=b;  newcols++;
	  }

	  *dp = (byte) (k & 0xff);
	}
      }
    }


    SortColormap(dpic, dw, dh, &dpncols, newr,newg,newb, order, trans);
    for (i=0, dp=dpic; i<dw*dh; i++, dp++) *dp = trans[*dp];

    if (istran) {
      if (!clipis24) trval = trans[trval];
      else {
	for (i=0; i<dpncols; i++) {
	  if (cimg[CIMG_TRR] == newr[i] &&
	      cimg[CIMG_TRG] == newg[i] &&
	      cimg[CIMG_TRB] == newb[i]) { trval = i;  break; }
	}
      }
    }



    /* COLORMAP MERGING */

    newcols = 0;

    for (i=0; i<dpncols; i++) {
      if (istran && i==trval) continue;

      for (j=0; j<numcols; j++) {              /* look for an exact match */
	if (rMap[j]==newr[i] && gMap[j]==newg[i] && bMap[j]==newb[i]) break;
      }
      if (j<numcols) remap[i] = j;
      else {                                   /* no exact match */
	newcols++;

	if (numcols < 256) {
	  rMap[numcols] = newr[i];
	  gMap[numcols] = newg[i];
	  bMap[numcols] = newb[i];
	  remap[i] = numcols;
	  numcols++;
	}
	else {  /* map to closest in image colormap */
	  r = newr[i];  g=newg[i];  b=newb[i];
	  mind = 256*256 + 256*256 + 256*256;
	  for (j=close=0; j<numcols; j++) {
	    k = ((rMap[j]-r) * (rMap[j]-r)) +
	      ((gMap[j]-g) * (gMap[j]-g)) +
		((bMap[j]-b) * (bMap[j]-b));
	    if (k<mind) { mind = k;  close = j; }
	  }
	  remap[i] = (byte) close;
	}
      }
    }


    /* copy the data into PIC */

    dp = dpic;
    for (i=dy; i<dy+dh; i++) {
      pp = pic + (i*pWIDE) + dx;
      for (j=dx; j<dx+dw; j++) {
	if (istran && *dp==trval) { pp++;  dp++; }
	                     else { *pp++ = remap[*dp++]; }
      }
    }
    free(dpic);

    if (newcols) InstallNewPic();      /* does color reallocation, etc. */
    else {
      GenerateCpic();
      GenerateEpic(eWIDE, eHIGH);
      DrawEpic();
    }
  }


  /******************** PIC24 handling **********************/


  else {
    byte *tmppic, *cp, *pp, *clp;
    int   bperpix;
    int   trr, trg, trb, clx, cly;

    trr = trg = trb = 0;
    if (istran) {
      if (clipis24) {
	trr = cimg[CIMG_TRR];
	trg = cimg[CIMG_TRG];
	trb = cimg[CIMG_TRB];
      }
      else {
	trr = clipcmap[trval*3];
	trg = clipcmap[trval*3+1];
	trb = clipcmap[trval*3+2];
      }
    }

    bperpix = (clipis24) ? 3 : 1;

    if (!istran && (cw != dw || ch != dh)) {  /* need to resize, can smooth */
      byte rmap[256], gmap[256], bmap[256];

      tmppic = (byte *) malloc((size_t) cw * ch * bperpix);
      if (!tmppic) FatalError("Out of memory in DoImgPaste()\n");

      /* copy relevant hunk of clippic into tmppic (Smooth24 only works on
	 complete images */

      for (i=0; i<ch; i++) {
	dp = tmppic + i*cw*bperpix;
	cp = clippic + ((i+cy)*clipw + cx) * bperpix;
	for (j=0; j<cw*bperpix; j++) *dp++ = *cp++;
      }

      if (!clipis24) {
	for (i=0; i<256; i++) {
	  rmap[i] = clipcmap[i*3];
	  gmap[i] = clipcmap[i*3+1];
	  bmap[i] = clipcmap[i*3+2];
	}
      }

      dpic = Smooth24(tmppic, clipis24, cw,ch, dw,dh, rmap,gmap,bmap);
      if (!dpic) FatalError("Out of memory (2) in DoImgPaste()\n");
      free(tmppic);

      /* copy the resized, smoothed, 24-bit data into 'pic' */

      /* XXX: (deal with smooth-resized transparent imgs) */

      dp = dpic;
      for (i=dy; i<dy+dh; i++) {
	pp = pic + (i*pWIDE + dx) * 3;
	for (j=0; j<dw; j++) {
	  if (istran && dp[0]==trr && dp[1]==trg && dp[2]==trb) {
	    pp +=3;  dp += 3;
	  } else {
	    *pp++ = *dp++;  *pp++ = *dp++;  *pp++ = *dp++;
	  }
	}
      }
      free(dpic);
    }


    else {   /* can't do smooth resize.  Do non-smooth resize (if any!) */
      for (i=0; i<dh; i++) {
	pp = pic + ((i+dy)*pWIDE + dx) * 3;
	cly = cy + (i * ch) / dh;
	clp = clippic + (cly*clipw * bperpix);

	for (j=0; j<dw; j++, pp+=3) {
	  clx = cx + (j * cw) / dw;
	  cp = clp + (clx * bperpix);

	  if (clipis24) {
	    if (!istran || cp[0]!=trr || cp[1]!=trg || cp[2]==trb) {
	      pp[0] = *cp++;  pp[1] = *cp++;  pp[2] = *cp++;
	    }
	  }
	  else {   /* clip is 8 bit */
	    if (!istran || *cp != trval) {
	      pp[0]  = clipcmap[*cp * 3];
	      pp[1]  = clipcmap[*cp * 3 + 1];
	      pp[2]  = clipcmap[*cp * 3 + 2];
	    }
	  }
	}
      }
    }


    GenerateCpic();
    GenerateEpic(eWIDE, eHIGH);
    DrawEpic();
  }


 exit:
  SetCursors(-1);
}



/********************************************/
static void buildCursors()
{
  Pixmap p1,p2,p3,p4;
  XColor cfg, cbg;

  dragcurs = XCreateFontCursor(theDisp, XC_fleur);
  p1 = XCreatePixmapFromBitmapData(theDisp, rootW, (char *) cut_bits,
				   cut_width,  cut_height, 1L, 0L, 1);
  p2 = XCreatePixmapFromBitmapData(theDisp, rootW, (char *) cutm_bits,
				   cutm_width, cutm_height, 1L, 0L, 1);
  p3 = XCreatePixmapFromBitmapData(theDisp, rootW, (char *) copy_bits,
				   copy_width,  copy_height, 1L, 0L, 1);
  p4 = XCreatePixmapFromBitmapData(theDisp, rootW, (char *) copym_bits,
				   copym_width, copym_height, 1L, 0L, 1);
  if (p1 && p2 && p3 && p4) {
    cfg.red = cfg.green = cfg.blue = 0;
    cbg.red = cbg.green = cbg.blue = 0xffff;
    cutcurs = XCreatePixmapCursor(theDisp, p1,p2, &cfg, &cbg,
				  cut_x_hot, cut_y_hot);
    copycurs = XCreatePixmapCursor(theDisp, p3,p4, &cfg, &cbg,
				  copy_x_hot, copy_y_hot);
    if (!cutcurs || !copycurs) FatalError("can't create cut/copy cursors...");
  }

  if (p1) XFreePixmap(theDisp, p1);
  if (p2) XFreePixmap(theDisp, p2);
  if (p3) XFreePixmap(theDisp, p3);
  if (p4) XFreePixmap(theDisp, p4);

}


/********************************************/
static byte *getSelection()
{
  /* alloc's and builds image with values based on currently selected
   * portion of the image.  Returns NULL on failure
   *
   * also note: getSelection will always fill trans,r,g,b with 0, for now
   * as you can't 'select' transparent regions.  Other code (TextPaste()),
   * *can* generate semi-transparent objects to be pasted
   */

  byte *pp, *dp, *cimg;
  int   i, j, k, x, y, w, h, do24, len;

  if (!CutAllowed()) {  XBell(theDisp, 0);  return (byte *) NULL; }
  if (!HaveSelection()) return (byte *) NULL;

  GetSelRCoords(&x,&y,&w,&h);
  CropRect2Rect(&x,&y,&w,&h, 0,0,pWIDE,pHIGH);

  /* make selection be entirely within image */
  EnableSelection(0);
  selrx = x;  selry = y;  selrw = w;  selrh = h;
  EnableSelection(1);


  if (picType == PIC24 && countcols24(pic,pWIDE,pHIGH, x,y,w,h)>256) {
    do24=1;
    len = CIMG_PIC24 + w*h*3;
  }
  else {
    do24=0;
    len = CIMG_PIC8 + w*h;
  }

  cimg = (byte *) malloc((size_t) len);
  if (!cimg) {
    ErrPopUp("Unable to malloc() temporary space for the selection.",
	     "\nByte Me!");
    return (byte *) NULL;
  }

  cimg[CIMG_LEN  ] =  len      & 0xff;
  cimg[CIMG_LEN+1] = (len>> 8) & 0xff;
  cimg[CIMG_LEN+2] = (len>>16) & 0xff;
  cimg[CIMG_LEN+3] = (len>>24) & 0xff;

  cimg[CIMG_W  ] =  w     & 0xff;
  cimg[CIMG_W+1] = (w>>8) & 0xff;

  cimg[CIMG_H  ] =  h     & 0xff;
  cimg[CIMG_H+1] = (h>>8) & 0xff;

  cimg[CIMG_24]    = do24;
  cimg[CIMG_TRANS] = 0;


  if (picType == PIC24 && !do24) {                  /* 24-bit data as 8-bit */
    int nc,pr,pg,pb;
    byte *cm;

    nc = 0;
    dp = cimg + CIMG_PIC8;

    for (i=y; i<y+h; i++) {
      pp = pic + i*pWIDE*3 + x*3;
      for (j=x; j<x+w; j++, pp+=3) {
	pr = pp[0];  pg = pp[1];  pb = pp[2];

	cm = cimg + CIMG_CMAP;
	for (k=0; k<nc; k++,cm+=3) {
	  if (pr==cm[0] && pg==cm[1] && pb==cm[2]) break;
	}
	if (k==nc) {
	  nc++;
	  cimg[CIMG_CMAP + nc*3    ] = pr;
	  cimg[CIMG_CMAP + nc*3 + 1] = pg;
	  cimg[CIMG_CMAP + nc*3 + 2] = pb;
	}

	*dp++ = (byte) k;
      }
    }
  }


  else if (picType == PIC24) {                     /* 24-bit data as 24-bit */
    dp = cimg + CIMG_PIC24;
    for (i=y; i<y+h; i++) {
      pp = pic + i*pWIDE*3 + x*3;
      for (j=x; j<x+w; j++) {
	*dp++ = *pp++;
	*dp++ = *pp++;
	*dp++ = *pp++;
      }
    }
  }


  else if (picType == PIC8) {                       /* 8-bit selection */
    byte *cm = cimg + CIMG_CMAP;
    for (i=0; i<256; i++) {                         /* copy colormap */
      if (i<numcols) {
	*cm++ = rMap[i];
	*cm++ = gMap[i];
	*cm++ = bMap[i];
      }
    }

    dp = cimg + CIMG_PIC8;
    for (i=y; i<y+h; i++) {                         /* copy image */
      pp = pic + i*pWIDE + x;
      for (j=x; j<x+w; j++) *dp++ = *pp++;
    }
  }

  return cimg;
}




/********************************************/
static byte *getFromClip()
{
  /* gets whatever data is on the clipboard, in CIMG_* format */

  Atom          clipAtom, actType;
  int           i, actFormat, len;
  unsigned long nitems, nleft;
  byte         *data, *data1, lbuf[4];

  FILE *fp;
  char  str[512];


  if (forceClipFile) {                           /* remove property, if any */
    clipAtom = XInternAtom(theDisp, CLIPPROP, True);
    if (clipAtom != None) XDeleteProperty(theDisp, rootW, clipAtom);
  }


  clipAtom = XInternAtom(theDisp, CLIPPROP, True);             /* find prop */
  if (clipAtom != None) {

    /* try to retrieve the length of the data in the property */
    i = XGetWindowProperty(theDisp, rootW, clipAtom, 0L, 1L, False, XA_STRING,
		       &actType, &actFormat, &nitems, &nleft,
		       (unsigned char **) &data);

    if (i==Success && actType==XA_STRING && actFormat==8 && nleft>0) {
      /* got some useful data */
      len  = data[0];
      len |= ((int) data[1])<<8;
      len |= ((int) data[2])<<16;
      len |= ((int) data[3])<<24;

      XFree((void *) data);

      /* read the rest of the data (len bytes) */
      i = XGetWindowProperty(theDisp, rootW, clipAtom, 1L,
			     (long) ((len-4)+3)/4,
			     False, XA_STRING, &actType, &actFormat, &nitems,
			     &nleft, (unsigned char **) &data);

      if (i==Success) {
	/* copy data into regular 'malloc'd space, so we won't have to use
	   XFree() to get rid of it in calling procs */

	data1 = (byte *) malloc((size_t) len);
	if (!data1) {
	  XFree((void *) data);
	  ErrPopUp("Insufficient memory to retrieve clipboard!", "\nShucks!");
	  return (byte *) NULL;
	}

	data1[0] =  len      & 0xff;
	data1[1] = (len>> 8) & 0xff;
	data1[2] = (len>>16) & 0xff;
	data1[3] = (len>>24) & 0xff;
	xvbcopy((char *) data, (char *) data1+4, (size_t) len-4);

	XFree((void *) data);
	return data1;
      }
    }
  }


  /* if we're still here, then the prop method was less than successful.
     use the file method, instead */

  if (!clipfname) makeClipFName();

  fp = fopen(clipfname, "r");
  if (!fp) {
    unlink(clipfname);
    sprintf(str, "Can't read clipboard file '%s'\n\n  %s.",
	    clipfname, ERRSTR(errno));
    ErrPopUp(str,"\nBletch!");
    return (byte *) NULL;
  }

  /* get data length */
  if (fread((char *) lbuf, (size_t) 1, (size_t) 4, fp) != 4) {
    fclose(fp);
    unlink(clipfname);
    sprintf(str, "Error occurred while reading clipboard file.\n\n  %s.",
	    ERRSTR(errno));
    ErrPopUp(str,"\nGlork!");
    return (byte *) NULL;
  }

  len  = lbuf[0];
  len |= ((int) lbuf[1])<<8;
  len |= ((int) lbuf[2])<<16;
  len |= ((int) lbuf[3])<<24;

  data = (byte *) malloc((size_t) len);
  if (!data) {
    ErrPopUp("Insufficient memory to retrieve clipboard!", "\nShucks!");
    return (byte *) NULL;
  }


  data[0] =  len      & 0xff;
  data[1] = (len>> 8) & 0xff;
  data[2] = (len>>16) & 0xff;
  data[3] = (len>>24) & 0xff;

  /* get data */
  if (fread((char *) data+4, (size_t) 1, (size_t) len-4, fp) != len-4) {
    fclose(fp);
    free(data);
    unlink(clipfname);
    sprintf(str, "Error occurred while reading clipboard file.\n\n  %s.",
	    ERRSTR(errno));
    ErrPopUp(str,"\nNertz!");
    return (byte *) NULL;
  }

  fclose(fp);

  return data;
}



/********************************************/
void SaveToClip(cimg)
     byte *cimg;
{
  /* takes the 'thing' pointed to by data and sticks it on the clipboard.
     always tries to use the property method.  If it gets a BadAlloc
     error (the X server ran out of memory (ie, probably an X terminal)),
     it deletes the property, and falls back to using the file method */

  Atom  clipAtom;
  FILE *fp;
  char  str[512];
  int   len;

  if (!cimg) return;

  len = ((int)  cimg[CIMG_LEN + 0])      |
        ((int) (cimg[CIMG_LEN + 1]<<8))  |
	((int) (cimg[CIMG_LEN + 2]<<16)) |
	((int) (cimg[CIMG_LEN + 3]<<24));


  if (forceClipFile) {                           /* remove property, if any */
    clipAtom = XInternAtom(theDisp, CLIPPROP, True);
    if (clipAtom != None) XDeleteProperty(theDisp, rootW, clipAtom);
  }


  if (!forceClipFile) {
    clipAtom = XInternAtom(theDisp, CLIPPROP, False);  /* find or make prop */
    if (clipAtom != None) {
      /* try to store the data in the property */

      xerrcode = 0;
      XChangeProperty(theDisp, rootW, clipAtom, XA_STRING, 8, PropModeReplace,
		      cimg, len);
      XSync(theDisp, False);                         /* make it happen *now* */
      if (!xerrcode) return;                         /* success! */

      /* failed, use file method */
      XDeleteProperty(theDisp, rootW, clipAtom);
    }
  }


  /* if we're still here, try the file method */

  if (!clipfname) makeClipFName();

  fp = fopen(clipfname, "w");
  if (!fp) {
    unlink(clipfname);
    sprintf(str, "Can't write clipboard file '%s'\n\n  %s.",
	    clipfname, ERRSTR(errno));
    ErrPopUp(str,"\nBletch!");
    return;
  }

  if (fwrite((char *) cimg, (size_t) 1, (size_t) len, fp) != len) {
    fclose(fp);
    unlink(clipfname);
    sprintf(str, "Error occurred while writing to clipboard file.\n\n  %s.",
	    ERRSTR(errno));
    ErrPopUp(str,"\nGlork!");
    return;
  }

  fclose(fp);
}



/********************************************/
static void clearSelectedArea()
{
  /* called by 'Cut' or 'Clear' functions.  fills the selected area of the
     image with either clearR,clearG,clearB (in PIC24 mode), or editColor
     (in PIC8 mode).  Regens and redraws the image */

  int   i,j,x,y,w,h;
  byte *pp;

  if (!HaveSelection()) return;
  GetSelRCoords(&x,&y,&w,&h);
  CropRect2Rect(&x,&y,&w,&h, 0,0,pWIDE,pHIGH);

  if (picType == PIC24) {
    for (i=y; i<y+h && i<pHIGH; i++) {
      pp = pic + i*pWIDE*3 + x*3;
      for (j=x; j<x+w && j<pWIDE; j++) {
	*pp++ = clearR;
	*pp++ = clearG;
	*pp++ = clearB;
      }
    }
  }

  else {  /* PIC8 */
    for (i=y; i<y+h && i<pHIGH; i++) {
      pp = pic + i*pWIDE + x;
      for (j=x; j<x+w && j<pWIDE; j++) *pp++ = editColor;
    }
  }

  GenerateCpic();
  GenerateEpic(eWIDE,eHIGH);
  DrawEpic();        /* redraws selection, also */
}


/********************************************/
static void makeClipFName()
{
  const char *homedir;

  if (clipfname) return;

#ifdef VMS
  sprintf(clipfnstr, "SYS$LOGIN:%s", CLIPFILE);
#else
  homedir = (char *) getenv("HOME");
  if (!homedir) homedir = ".";
  sprintf(clipfnstr, "%s/%s", homedir, CLIPFILE);
#endif

  clipfname = clipfnstr;
}





/********************************************/
static int countcols24(pic, pwide, phigh, x, y, w, h)
     byte *pic;
     int   pwide, phigh, x,y,w,h;
{
  /* counts the # of unique colors in a selected rect of a 24-bit image
     returns '0-256' or >256 */

  int   i, j, k, nc;
  u_int rgb[257], col;
  byte *pp;

  nc = 0;

  for (i=y; nc<257 && i<y+h; i++) {
    pp = pic + i*pwide*3 + x*3;
    for (j=x; nc<257 && j<x+w; j++, pp+=3) {
      col = (((u_int) pp[0])<<16) + (((u_int) pp[1])<<8) + pp[2];

      for (k=0; k<nc && col != rgb[k]; k++);
      if (k==nc) rgb[nc++] = col;  /* not found, add it */
    }
  }

  return nc;
}


/********************************************/
static int countNewCols(newpic, w,h, newcmap, is24, cx,cy,cw,ch)
     byte *newpic, *newcmap;
     int   w,h, is24, cx,cy,cw,ch;
{
  /* computes the number of NEW colors in the specified region of the
   * new pic, with respect to 'pic'.  returns 0-257 (where 257 means
   * 'some unknown # greater than 256')
   */

  int   i, j, k, nc, r,g,b;
  byte *pp;
  byte  newr[257], newg[257], newb[257];

  if (picType != PIC8) return 0;           /* shouldn't happen */

  nc = 0;    /* # of new colors */

  if (is24) {
    for (i=cy; i<cy+ch; i++) {
      pp = newpic + i*w*3 + cx*3;
      for (j=cx; j<cx+cw; j++) {
	r = *pp++;  g = *pp++;  b = *pp++;

	/* lookup r,g,b in 'pic's colormap and the newcolors colormap */
	for (k=0; k<nc && (r!=newr[k] || g!=newg[k] || b!=newb[k]); k++);
	if (k==nc) {
	  for (k=0; k<numcols && (r!=rMap[k] || g!=gMap[k] || b!=bMap[k]);k++);
	  if (k==numcols) {  /* it's a new color, alright */
	    newr[nc] = r;  newg[nc] = g;  newb[nc] = b;
	    nc++;
	    if (nc==257) return nc;
	  }
	}
      }
    }
  }

  else {      /* newpic is an 8-bit pic */
    int coluse[256];
    for (i=0; i<256; i++) coluse[i] = 0;

    /* figure out which colors in newcmap are used */
    for (i=cy; i<cy+ch; i++) {
      pp = newpic + i*w + cx;
      for (j=cx; j<cx+cw; j++, pp++) coluse[*pp] = 1;
    }

    /* now see which of the used colors are new */
    for (i=0, nc=0; i<256; i++) {
      if (!coluse[i]) continue;

      r = newcmap[i*3];
      g = newcmap[i*3+1];
      b = newcmap[i*3+2];

      /* lookup r,g,b in pic's colormap */
      for (k=0; k<numcols && (r!=rMap[k] || g!=gMap[k] || b!=bMap[k]);k++);
      if (k==numcols) {  /* it's a new color, alright */
	newr[nc] = r;  newg[nc] = g;  newb[nc] = b;
	nc++;
      }
    }
  }

  return nc;
}


/********************************************/
void CropRect2Rect(xp,yp,wp,hp, cx,cy,cw,ch)
    int *xp, *yp, *wp, *hp, cx, cy, cw, ch;
{
  /* crops rect xp,yp,wp,hp to be entirely within bounds of cx,cy,cw,ch */

  int x1,y1,x2,y2;

  x1 = *xp;            y1 = *yp;
  x2 = *xp + *wp - 1;  y2 = *yp + *hp - 1;
  RANGE(x1, cx, cx+cw-1);
  RANGE(y1, cy, cy+ch-1);
  RANGE(x2, cx, cx+cw-1);
  RANGE(y2, cy, cy+ch-1);

  if (x2<x1) x2=x1;
  if (y2<y1) y2=y1;

  *xp = x1;           *yp = y1;
  *wp = (x2 - x1)+1;  *hp = (y2 - y1)+1;
}


/********************************************/
/* SELECTION manipulation functions         */
/********************************************/


/********************************************/
void InitSelection()
{
  selrx = selry = selrw = selrh = 0;
  haveSel     = 0;
  selType     = SEL_RECT;
  selFilled   = 0;
  selColor    = 0;
  selTracking = 0;
}


/********************************************/
int HaveSelection()
{
  return haveSel;
}


/********************************************/
int GetSelType()
{
  return selType;
}


/********************************************/
void GetSelRCoords(xp, yp, wp, hp)
     int *xp, *yp, *wp, *hp;
{
  /* returns selection rectangle x,y,w,h in pic coordinates */

  /* NOTE:  SELECTION IS *NOT* GUARANTEED to be within the bounds of 'pic'.
     It is only guaranteed to *intersect* pic. */

  *xp = selrx;  *yp = selry;
  *wp = selrw;  *hp = selrh;
}


/********************************************/
void EnableSelection(enab)
     int enab;
{
  haveSel = enab;
  BTSetActive(&but[BCROP], enab);
  BTSetActive(&but[BCUT],  enab);
  BTSetActive(&but[BCOPY], enab);
  BTSetActive(&but[BCLEAR],enab);

  if (dirUp == BSAVE) CBSetActive(&saveselCB, enab);

  SetSelectionString();
  DrawSelection(0);
}


/***********************************/
int DoSelection(ev)
     XButtonEvent *ev;
{
  int          px, py, rv;
  static Time  lastClickTime   = 0;
  static int   lastClickButton = Button3;


  /* actually, this handles all selection-oriented manipulation
   * if B1 clicked outside sel (or if no sel) remove sel and draw new one
   * if B1 clicked inside sel, drag sel around
   * if B1 clicked in sel handles, resize sel
   * if B1 dbl-clicked in sel, remove sel
   * if B1 dbl-clicked ouside of sel, make a pic-sized sel
   * if B2 clicked in sel, do a drag-n-drop operation
   * B3 not used
   *
   * returns '1' if event was handled, '0' otherwise
   */


  /* make sure it's even vaguely relevant */
  if (ev->type   != ButtonPress) return 0;
  if (ev->window != mainW)       return 0;

  rv = 0;

  CoordE2P(ev->x, ev->y, &px, &py);

  if (ev->button == Button1) {
    /* double clicked B1 ? */
    if (lastClickButton==Button1 && (ev->time - lastClickTime) < DBLCLKTIME) {
      lastClickButton=Button3;
      if (HaveSelection() && PTINRECT(px, py, selrx, selry, selrw, selrh)) {
	EnableSelection(0);
	rv = 1;
      }
      else {
	selrx = cXOFF;  selry = cYOFF;  selrw = cWIDE;  selrh = cHIGH;
	EnableSelection(1);
	rv = 1;
      }
    }

    else if (HaveSelection() && PTINRECT(px,py,selrx,selry,selrw,selrh)) {
      if (dragHandle(ev)) {}
      else dragSelection(ev, Button1Mask, 0);
      rv = 1;
    }

    else if (!HaveSelection() || !PTINRECT(px,py,selrx,selry,selrw,selrh)) {
      if (HaveSelection()) EnableSelection(0);
      rectSelection(ev);
    }
  }

  else if (ev->button == Button2) {      /* do a drag & drop operation */
    if (HaveSelection() && PTINRECT(px,py,selrx,selry,selrw,selrh)) {
      /* clip selection rect to pic */
      EnableSelection(0);
      CropRect2Rect(&selrx, &selry, &selrw, &selrh, 0, 0, pWIDE, pHIGH);

      if (selrw<1 || selrh<1) rv = 0;
      else {
	EnableSelection(1);
	dragSelection(ev, Button2Mask, 1);
	rv = 1;
      }
    }
  }

  lastClickTime   = ev->time;
  lastClickButton = ev->button;
  return rv;
}


/********************************************/
static int dragHandle(ev)
     XButtonEvent *ev;
{
  /* called on a B1 press inside the selection area.  if mouse clicked on
   * one of the selection handles, drag the handle until released.
   * Selection may be dragged outside of 'pic' boundaries
   * holding SHIFT constrains selection to be square,
   * holding CTRL  constrains selection to keep original aspect ratio
   */

  int          mex, mey, mpx, mpy, offx,offy;
  int          sex, sey, sex2, sey2, sew, seh, sew2, seh2, hs, h2;
  int          istp, isbt, islf, isrt, isvm, ishm;
  int          cnstsq, cnstasp;
  double       orgaspect;
  Window       rW, cW;
  int          mx, my, RX, RY;
  unsigned int mask;

  mex = ev->x;  mey = ev->y;

  CoordP2E(selrx,       selry,       &sex,  &sey);
  CoordP2E(selrx+selrw, selry+selrh, &sex2, &sey2);
  sew  = sex2-sex;
  seh  = sey2-sey;
  sew2 = sew/2;
  seh2 = seh/2;
  sex2--;  sey2--;

  if      (sew>=35 && seh>=35) hs=7;
  else if (sew>=20 && seh>=20) hs=5;
  else if (sew>= 9 && seh>= 9) hs=3;
  else return 0;

  h2 = hs/2;

  istp = isbt = islf = isrt = isvm = ishm = 0;

  /* figure out which, if any, handle the mouse is in */
  if      (mex >= sex         && mex < sex+hs)      islf++;
  else if (mex >= sex+sew2-h2 && mex < sex+sew2+h2) ishm++;
  else if (mex >= sex+sew-hs  && mex < sex+sew)     isrt++;
  else return 0;

  if      (mey >= sey         && mey < sey+hs)      istp++;
  else if (mey >= sey+seh2-h2 && mey < sey+seh2+h2) isvm++;
  else if (mey >= sey+seh-hs  && mey < sey+seh)     isbt++;
  else return 0;


  offx = offy = 0;
  if (islf) offx = sex  - mex;
  if (isrt) offx = sex2+1 - mex;
  if (istp) offy = sey  - mey;
  if (isbt) offy = sey2+1 - mey;

  if (ishm && isvm) return 0;   /* clicked in middle.  doesn't count */

  if (selrh==0) orgaspect = 1.0;
  else orgaspect = (double) selrw / (double) selrh;


  /* it's definitely in a handle...  track 'til released */

  DrawSelection(0);
  selFilled   = 1;
  selTracking = 1;
  DrawSelection(0);

  CoordE2P(mex, mey, &mpx, &mpy);

  while (1) {
    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&RX,&RY,&mx,&my,&mask)) continue;
    if (~mask & Button1Mask) break;
    cnstsq  = (mask & ShiftMask);
    cnstasp = (mask & ControlMask);

    CoordE2Prnd(mx+offx, my+offy, &mpx, &mpy);  /* mouse pos in PIC coords */

    sex = selrx;  sey = selry;  sew = selrw;  seh = selrh;

    /* compute new selection rectangle based on *what* handle is dragged */
    if (islf) {
      if (mpx>=selrx+selrw) mpx = selrx+selrw-1;
      sex = mpx;  sew = (selrx + selrw) - mpx;
    }

    if (isrt) {
      if (mpx<=selrx) mpx=selrx+1;
      sew = mpx - selrx;
    }

    if (istp) {
      if (mpy>=selry+selrh) mpy = selry+selrh-1;
      sey = mpy;  seh = (selry + selrh) - mpy;
    }

    if (isbt) {
      if (mpy<=selry) mpy=selry+1;
      seh = mpy - selry;
    }



    if (cnstsq || cnstasp) {
      int newwide, newhigh, chwide, chhigh;

      chwide = chhigh = newwide = newhigh = 0;

      if (cnstsq) { /* constrain to a square */
	if (islf || isrt) { chhigh=1;  newhigh = sew; }
	             else { chwide=1;  newwide = seh; }
      }
      else {         /* constrain to same aspect ratio */
	double asp;
	if (seh==0) { chwide=1; newwide=0; }
	else {
	  asp = (double) sew / (double) seh;
	  if (islf || isrt) { chhigh=1;  newhigh = (int) (sew/orgaspect); }
                       else { chwide=1;  newwide = (int) (seh*orgaspect); }
	}
      }

      if (chwide) {
	if (islf) { sex = (sex+sew) - newwide; }
	sew = newwide;
      }

      if (chhigh) {
	if (istp) { sey = (sey+seh) - newhigh; }
	seh = newhigh;
      }
    }

    if (sew<1) sew=1;
    if (seh<1) seh=1;

    if (sex!=selrx || sey!=selry || sew!=selrw || seh!=selrh) {
      DrawSelection(0);
      selrx = sex;  selry = sey;  selrw = sew;  selrh = seh;
      DrawSelection(1);
      if (infoUp) SetSelectionString();
      XSync(theDisp, False);
    }
    else {
      DrawSelection(0);
      DrawSelection(1);
      XSync(theDisp, False);
      Timer(100);
    }
  }

  EnableSelection(0);

  selFilled   = 0;
  selTracking = 0;

  /* only 'enable' the selection if it intersects CPIC */
  if (selrx < cXOFF+cWIDE && selrx+selrw > cXOFF &&
      selry < cYOFF+cHIGH && selry+selrh > cYOFF) EnableSelection(1);

  return 1;
}


/********************************************/
static void dragSelection(ev, bmask, dragndrop)
     XButtonEvent *ev;
     unsigned int bmask;
     int          dragndrop;
{
  /* called on a button press inside the selection area.  drags selection
   * around until button has been released.  Selection may be dragged outside
   * of 'pic' boundaries.  Holding SHIFT constrains movement to 90-degree
   * angles
   *
   * if 'dragndrop', changes cursor, monitors CTRL status
   */

  int          mpx, mpy, offx, offy;
  int          newsx, newsy, orgsx, orgsy, cnstrain, docopy, lastdocopy;
  Window       rW, cW;
  int          mx, my, RX, RY;
  unsigned int mask;

  if (!dragcurs) buildCursors();

  if (dragndrop) XDefineCursor(theDisp, mainW, cutcurs);
            else XDefineCursor(theDisp, mainW, dragcurs);

  CoordE2P(ev->x, ev->y, &mpx, &mpy);
  offx = mpx - selrx;  offy = mpy - selry;

  /* track rectangle until we get a release */

  DrawSelection(0);
  selFilled   = 1;
  selTracking = 1;
  DrawSelection(0);

  orgsx = selrx;  orgsy = selry;  docopy = lastdocopy = 0;

  while (1) {
    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&RX,&RY,&mx,&my,&mask)) continue;
    if (~mask & bmask) break;
    cnstrain = (mask & ShiftMask);
    docopy   = (mask & ControlMask);

    if (dragndrop && docopy != lastdocopy) {
      XDefineCursor(theDisp, mainW, (docopy) ? copycurs : cutcurs);
      lastdocopy = docopy;
    }

    CoordE2P(mx, my, &mpx, &mpy);    /* mouse pos in PIC coord system */

    newsx = mpx - offx;
    newsy = mpy - offy;

    if (cnstrain) {
      int dx, dy;
      dx = newsx - orgsx;  dy = newsy - orgsy;
      if      (abs(dx) > abs(dy)) dy = 0;
      else if (abs(dy) > abs(dx)) dx = 0;

      newsx = orgsx + dx;  newsy = orgsy + dy;
    }

    if (newsx != selrx || newsy != selry) {    /* mouse moved */
      DrawSelection(0);
      selrx = newsx;  selry = newsy;
      DrawSelection(1);
      if (infoUp) SetSelectionString();
      XSync(theDisp, False);
    }
    else {
      DrawSelection(0);
      DrawSelection(1);
      XSync(theDisp, False);
      Timer(100);
    }
  }

  EnableSelection(0);

  selFilled   = 0;
  selTracking = 0;

  SetCursors(-1);

  /* only do <whatever> if the selection intersects CPIC */

 if (selrx < cXOFF+cWIDE && selrx+selrw > cXOFF &&
      selry < cYOFF+cHIGH && selry+selrh > cYOFF) {

    EnableSelection(1);

    if (dragndrop) {
      int   tmpsx, tmpsy;
      byte *data;

      tmpsx = selrx;  tmpsy = selry;
      selrx = orgsx;  selry = orgsy;

      data = getSelection();         /* copy old data */
      if (data) {
	if (!docopy) clearSelectedArea();
	selrx = tmpsx;  selry = tmpsy;
	doPaste(data);
	free(data);
      }

      EnableSelection(0);
      CropRect2Rect(&selrx, &selry, &selrw, &selrh, 0,0,pWIDE,pHIGH);
      EnableSelection(1);
    }
  }
}


/***********************************/
static void rectSelection(ev)
     XButtonEvent *ev;
{
  Window       rW,cW;
  int          rx,ry,ox,oy,x,y,active, x1, y1, x2, y2, cnstrain;
  int          i, px,py,px2,py2,pw,ph;
  unsigned int mask;

  /* called on a B1 press in mainW to draw a new rectangular selection.
   * any former selection has already been removed.  holding shift down
   * while tracking constrains selection to a square
   */

  active = 0;

  x1 = ox = ev->x;  y1 = oy = ev->y;               /* nail down one corner */
  selrx = selry = selrw = selrh = 0;
  selTracking = 1;

  while (1) {
    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&rx,&ry,&x,&y,&mask)) continue;
    if (!(mask & Button1Mask)) break;      /* button released */
    cnstrain = (mask & ShiftMask);

    if (x!=ox || y!=oy) {                  /* moved.  erase and redraw (?) */
      x2 = x;  y2 = y;

      /* x1,y1,x2,y2 are in epic coords.  sort, convert to pic coords,
	 and if changed, erase+redraw */

      CoordE2P(x1, y1, &px,  &py);
      CoordE2P(x2, y2, &px2, &py2);
      if (px>px2) { i=px; px=px2; px2=i; }
      if (py>py2) { i=py; py=py2; py2=i; }
      pw = px2-px+1;  ph=py2-py+1;

      /* keep px,py,pw,ph inside 'pic' */

      if (px<0) { pw+=px;  px=0; }
      if (py<0) { ph+=py;  py=0; }
      if (px>pWIDE-1) px = pWIDE-1;
      if (py>pHIGH-1) py = pHIGH-1;

      if (pw<0) pw=0;
      if (ph<0) ph=0;
      if (px+pw>pWIDE) pw = pWIDE - px;
      if (py+ph>pHIGH) ph = pHIGH - py;

      if (cnstrain) {          /* make a square at smaller of w,h */
	if      (ph>pw) { if (y2<y1) py += (ph-pw);  ph=pw; }
	else if (pw>ph) { if (x2<x1) px += (pw-ph);  pw=ph; }
      }

      /* put x,y,w,h -> selr{x,y,w,h}
	 if the rectangle has changed, erase old and draw new */

      if (px!=selrx || py!=selry || pw!=selrw || ph!=selrh) {
	DrawSelection(0);
	selrx = px;  selry = py;  selrw = pw;  selrh = ph;
	DrawSelection(1);

	haveSel = active = (pw>0 && ph>0);
	if (infoUp) SetSelectionString();
	XFlush(theDisp);
      }
      else {
	DrawSelection(0);
	DrawSelection(1);
	XFlush(theDisp);
	Timer(100);
      }
    }
  }


  DrawSelection(0);                 /* erase */

  selTracking = 0;
  if (active) EnableSelection(1);
}


/***********************************/
void DrawSelection(newcol)
     int newcol;
{
  /* doesn't affect 'haveSel', as when moving/resizing/tracking the
     selection we need to erase it and redraw it.  If 'chcol' is
     set, pick a new 'color' to invert the selection with */

  int   x,y,x1,y1,w,h;

  if (newcol) selColor = (selColor+1) & 0x7;

  /* convert selr{x,y,w,h} into epic coords */
  CoordP2E(selrx, selry, &x, &y);
  CoordP2E(selrx+selrw, selry+selrh, &x1, &y1);

  w = (x1-x)-1;
  h = (y1-y)-1;
  if (w<1 || h<1) return;

  XSetPlaneMask(theDisp, theGC, xorMasks[selColor]);
  XSetFunction(theDisp,theGC,GXinvert);

  if (w<=2 || h<=2) {
    XFillRectangle(theDisp, mainW, theGC, x,y,(u_int) w, (u_int) h);

    XSetFunction(theDisp,theGC,GXcopy);
    XSetPlaneMask(theDisp, theGC, AllPlanes);
    return;
  }


  /* if selection completely encloses the image (ie, the selection rect
     itself isn't visible) draw something that *is* visible */

  /* if only one (or zero) sides of the sel rect is visible, draw
     appropriate lines to indicate where the rect is */

  if (x<0 && x+w>eWIDE && selFilled!=1)
    XDrawLine(theDisp, mainW, theGC, eWIDE/2, y, eWIDE/2, y+h);

  if (y<0 && y+h>eHIGH && selFilled!=1)
    XDrawLine(theDisp, mainW, theGC, x, eHIGH/2, x+w, eHIGH/2);


  if (selFilled==0 || selFilled == 1) {
    /* one little kludge:  if w or h == eWIDE or eHIGH, make it one smaller */
    if (x+w == eWIDE) w--;
    if (y+h == eHIGH) h--;
    XDrawRectangle(theDisp,mainW,theGC, x,y, (u_int) w, (u_int) h);

    if (selFilled==0 && !selTracking) {  /* draw 'handles' */
      int hs, h1, h2;

      if      (w>=35 && h>=35) { hs=7;  h1=6; h2=3; }
      else if (w>=20 && h>=20) { hs=5;  h1=4; h2=2; }
      else if (w>= 9 && h>= 9) { hs=3;  h1=2; h2=1; }
      else hs=h1=h2=0;

      if (hs) {
	XFillRectangle(theDisp,mainW,theGC,x+1,     y+1,  (u_int)h1,(u_int)h1);
	XFillRectangle(theDisp,mainW,theGC,x+w/2-h2,y+1,  (u_int)hs,(u_int)h1);
	XFillRectangle(theDisp,mainW,theGC,x+w-h1,  y+1,  (u_int)h1,(u_int)h1);

	XFillRectangle(theDisp,mainW,theGC,x+1,   y+h/2-h2,
		       (u_int)h1, (u_int)hs);
	XFillRectangle(theDisp,mainW,theGC,x+w-h1,y+h/2-h2,
		       (u_int)h1, (u_int)hs);

	XFillRectangle(theDisp,mainW,theGC,x+1,     y+h-h1,
		       (u_int)h1,(u_int)h1);
	XFillRectangle(theDisp,mainW,theGC,x+w/2-h2,y+h-h1,
		       (u_int)hs,(u_int)h1);
	XFillRectangle(theDisp,mainW,theGC,x+w-h1,  y+h-h1,
		       (u_int)h1,(u_int)h1);
      }
    }

    if (selFilled==1) {
      XDrawLine(theDisp, mainW, theGC, x+1, y+1,   x+w-1, y+h-1);
      XDrawLine(theDisp, mainW, theGC, x+1, y+h-1, x+w-1, y+1);
    }
  }
  else if (selFilled==2) {
    XFillRectangle(theDisp, mainW, theGC, x,y,(u_int) w, (u_int) h);
  }


  XSetFunction(theDisp,theGC,GXcopy);
  XSetPlaneMask(theDisp, theGC, AllPlanes);
}


/********************************************/
void MoveGrowSelection(dx,dy,dw,dh)
     int dx,dy,dw,dh;
{
  /* moves and/or grows the selection by the specified amount
     (in pic coords).  keeps the selection entirely within 'pic'.
     (called by 'CropKey()') */

  int x,y,w,h;

  if (!HaveSelection()) return;

  GetSelRCoords(&x, &y, &w, &h);

  x += dx;  y += dy;  w += dw;  h += dh;

  CropRect2Rect(&x,&y,&w,&h, 0,0,pWIDE,pHIGH);
  if (w<1) w=1;
  if (h<1) h=1;

  /* put x,y,w,h -> selr{x,y,w,h}
     if the rectangle has changed, erase old and draw new */

  if (x!=selrx || y!=selry || w!=selrw || h!=selrh) {
    DrawSelection(0);
    selrx = x;  selry = y;  selrw = w;  selrh = h;
    EnableSelection(1);
  }
}


/***********************************/
void BlinkSelection(cnt)
     int cnt;
{
  if (!HaveSelection()) return;

  for ( ; cnt>0; cnt--) {
    DrawSelection(0);
    XFlush(theDisp);
    Timer(100);
  }
}


/***********************************/
void FlashSelection(cnt)
     int cnt;
{
  int i;

  if (!HaveSelection()) return;
  i = selFilled;

  for ( ; cnt>0; cnt--) {
    selFilled = 2;
    DrawSelection(0);
    XFlush(theDisp);
    Timer(100);
  }

  selFilled = i;
}


/********************************************/
static void makePasteSel(cimg)
     byte *cimg;
{
  /* makes a selection rectangle the size of the beastie on the clipboard,
   * centered on cpic.  selection is allowed to be bigger than pic
   */

  int          clipw, cliph;

  if (!cimg) return;
  clipw = cimg[CIMG_W] | ((int) (cimg[CIMG_W+1]<<8));
  cliph = cimg[CIMG_H] | ((int) (cimg[CIMG_H+1]<<8));
  if (clipw<1 || cliph<1) return;

  selrw = clipw;  selrh = cliph;

  selrx = (cXOFF + cWIDE/2) - selrw/2;
  selry = (cYOFF + cHIGH/2) - selrh/2;

  EnableSelection(1);
}




/********************************************/
/* COORDINATE SYSTEM TRANSLATION FUNCTIONS  */
/********************************************/


void CoordE2C(ex, ey, cx_ret, cy_ret)
     int ex, ey, *cx_ret, *cy_ret;
{
  /* the weirdness causes everything to round towards neg infinity */
  *cx_ret = ((ex*cWIDE) / eWIDE) + ((ex<0) ? -1 : 0);
  *cy_ret = ((ey*cHIGH) / eHIGH) + ((ey<0) ? -1 : 0);
}


void CoordC2E(cx, cy, ex_ret, ey_ret)
     int cx, cy, *ex_ret, *ey_ret;
{
  /* this makes positive #s round to +inf, and neg # round to -inf */
  if (cx>=0) *ex_ret = (cx*eWIDE + (cWIDE-1)) / cWIDE;
        else *ex_ret = (cx*eWIDE - (cWIDE-1)) / cWIDE;
  if (cy>=0) *ey_ret = (cy*eHIGH + (cHIGH-1)) / cHIGH;
        else *ey_ret = (cy*eHIGH - (cHIGH-1)) / cHIGH;
}


void CoordP2C(px, py, cx_ret, cy_ret)
     int px, py, *cx_ret, *cy_ret;
{
  *cx_ret = px - cXOFF;
  *cy_ret = py - cYOFF;
}


void CoordC2P(cx, cy, px_ret, py_ret)
     int cx, cy, *px_ret, *py_ret;
{
  *px_ret = cx + cXOFF;
  *py_ret = cy + cYOFF;
}


void CoordP2E(px, py, ex_ret, ey_ret)
     int px, py, *ex_ret, *ey_ret;
{
  int cx, cy;
  CoordP2C(px, py, &cx, &cy);
  CoordC2E(cx, cy, ex_ret, ey_ret);
}


void CoordE2P(ex, ey, px_ret, py_ret)
     int ex, ey, *px_ret, *py_ret;
{
  int cx, cy;
  CoordE2C(ex, ey, &cx, &cy);
  CoordC2P(cx, cy, px_ret, py_ret);
}


static void CoordE2Prnd(ex, ey, px_ret, py_ret)
     int ex, ey, *px_ret, *py_ret;
{
  int cx, cy;
  cx = ((ex*cWIDE + (eWIDE/2)) / eWIDE) + ((ex<0) ? -1 : 0);
  cy = ((ey*cHIGH + (eHIGH/2)) / eHIGH) + ((ey<0) ? -1 : 0);

  CoordC2P(cx, cy, px_ret, py_ret);
}


