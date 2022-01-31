/*
 * xvimage.c - image manipulation functions (crop,resize,rotate...) for XV
 *
 *  Contains:
 *            void Resize(int, int)
 *            void GenerateCpic()
 *            void GenerateEpic(w,h)
 *            void Crop()
 *            void UnCrop()
 *            void AutoCrop()
 *            void DoCrop(x,y,w,h)
 *            void Rotate(int)
 *            void RotatePic();
 *            void InstallNewPic(void);
 *            void DrawEpic(void);
 *            byte *FSDither()
 *            void CreateXImage()
 *            void Set824Menus( pictype );
 *            void Change824Mode( pictype );
 *            int  DoPad(mode, str, wide, high, opaque, omode);
 *            int  LoadPad(pinfo, fname);
 */

/* The following switch should better be provided at runtime for
 * comparison purposes.
 * At the moment it's only compile time, unfortunately.
 * Who can make adaptions for use as a runtime switch by a menu option?
 * [GRR 19980607:  now via do_fixpix_smooth global; macro renamed to ENABLE_]
 * [see http://sylvana.net/fixpix/ for home page, further info]
 */
/* #define ENABLE_FIXPIX_SMOOTH */   /* GRR 19980607:  moved into xv.h */

#define  NEEDSDIR             /* for S_IRUSR|S_IWUSR */
#include "copyright.h"

#include "xv.h"


static void flipSel           PARM((int));
static void do_zoom           PARM((int, int));
static void compute_zoom_rect PARM((int, int, int*, int*, int*, int*));
static void do_unzoom         PARM((void));
static void do_pan            PARM((int, int));
static void do_pan_calc       PARM((int, int, int *, int *));
static void crop1             PARM((int, int, int, int, int));
static int  doAutoCrop24      PARM((void));
static void floydDitherize1   PARM((XImage *, byte *, int, int, int,
				    byte *, byte *,byte *));
#if 0 /* NOTUSED */
static int  highbit           PARM((unsigned long));
#endif

static int  doPadSolid        PARM((char *, int, int, int, int));
static int  doPadBggen        PARM((char *, int, int, int, int));
static int  doPadLoad         PARM((char *, int, int, int, int));

static int  doPadPaste        PARM((byte *, int, int, int, int));
static int  ReadImageFile1    PARM((char *, PICINFO *));


/* The following array represents the pixel values for each shade
 * of the primary color components.
 * If 'p' is a pointer to a source image rgb-byte-triplet, we can
 * construct the output pixel value simply by 'oring' together
 * the corresponding components:
 *
 *	unsigned char *p;
 *	unsigned long pixval;
 *
 *	pixval  = screen_rgb[0][*p++];
 *	pixval |= screen_rgb[1][*p++];
 *	pixval |= screen_rgb[2][*p++];
 *
 * This is both efficient and generic, since the only assumption
 * is that the primary color components have separate bits.
 * The order and distribution of bits does not matter, and we
 * don't need additional variables and shifting/masking code.
 * The array size is 3 KBytes total and thus very reasonable.
 */

static unsigned long screen_rgb[3][256];

/* The following array holds the exact color representations
 * reported by the system.
 * This is useful for less than 24 bit deep displays as a base
 * for additional dithering to get smoother output.
 */

static byte screen_set[3][256];

/* The following routine initializes the screen_rgb and screen_set
 * arrays.
 * Since it is executed only once per program run, it does not need
 * to be super-efficient.
 *
 * The method is to draw points in a pixmap with the specified shades
 * of primary colors and then get the corresponding XImage pixel
 * representation.
 * Thus we can get away with any Bit-order/Byte-order dependencies.
 *
 * The routine uses some global X variables: theDisp, theScreen,
 * and dispDEEP. Adapt these to your application as necessary.
 * I've not passed them in as parameters, since for other platforms
 * than X these may be different (see vfixpix.c), and so the
 * screen_init() interface is unique.
 *
 * BUG: I've read in the "Xlib Programming Manual" from O'Reilly &
 * Associates, that the DefaultColormap in TrueColor might not
 * provide the full shade representation in XAllocColor.
 * In this case one had to provide a 'best' colormap instead.
 * However, my tests with Xaccel on a Linux-Box with a Mach64
 * card were fully successful, so I leave that potential problem
 * to you at the moment and would appreciate any suggestions...
 */

static void screen_init()
{
  static int init_flag; /* assume auto-init as 0 */
  Pixmap check_map;
  GC check_gc;
  XColor check_col;
  XImage *check_image;
  int ci, i;

  if (init_flag) return;
  init_flag = 1;

  check_map = XCreatePixmap(theDisp, RootWindow(theDisp,theScreen),
			    1, 1, dispDEEP);
  check_gc = XCreateGC(theDisp, check_map, 0, NULL);
  for (ci = 0; ci < 3; ci++) {
    for (i = 0; i < 256; i++) {
      check_col.red = 0;
      check_col.green = 0;
      check_col.blue = 0;
      /* Do proper upscaling from unsigned 8 bit (image data values)
	 to unsigned 16 bit (X color representation). */
      ((unsigned short *)&check_col.red)[ci] = (unsigned short)((i << 8) | i);
      if (theVisual->class == TrueColor)
	XAllocColor(theDisp, theCmap, &check_col);
      else
	xvAllocColor(theDisp, theCmap, &check_col);
      screen_set[ci][i] =
	(((unsigned short *)&check_col.red)[ci] >> 8) & 0xff;
      XSetForeground(theDisp, check_gc, check_col.pixel);
      XDrawPoint(theDisp, check_map, check_gc, 0, 0);
      check_image = XGetImage(theDisp, check_map, 0, 0, 1, 1,
			      AllPlanes, ZPixmap);
      if (check_image) {
	switch (check_image->bits_per_pixel) {
	case 8:
	  screen_rgb[ci][i] = *(CARD8 *)check_image->data;
	  break;
	case 16:
	  screen_rgb[ci][i] = *(CARD16 *)check_image->data;
	  break;
	case 24:
	  screen_rgb[ci][i] =
	    ((unsigned long)*(CARD8 *)check_image->data << 16) |
	    ((unsigned long)*(CARD8 *)(check_image->data + 1) << 8) |
	    (unsigned long)*(CARD8 *)(check_image->data + 2);
	  break;
	case 32:
	  screen_rgb[ci][i] = *(CARD32 *)check_image->data;
	  break;
	}
	XDestroyImage(check_image);
      }
    }
  }
  XFreeGC(theDisp, check_gc);
  XFreePixmap(theDisp, check_map);
}


#ifdef ENABLE_FIXPIX_SMOOTH

/* The following code is based in part on:
 *
 * jquant1.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains 1-pass color quantization (color mapping) routines.
 * These routines provide mapping to a fixed color map using equally spaced
 * color values.  Optional Floyd-Steinberg or ordered dithering is available.
 */

/* Declarations for Floyd-Steinberg dithering.
 *
 * Errors are accumulated into the array fserrors[], at a resolution of
 * 1/16th of a pixel count.  The error at a given pixel is propagated
 * to its not-yet-processed neighbors using the standard F-S fractions,
 *		...	(here)	7/16
 *		3/16	5/16	1/16
 * We work left-to-right on even rows, right-to-left on odd rows.
 *
 * We can get away with a single array (holding one row's worth of errors)
 * by using it to store the current row's errors at pixel columns not yet
 * processed, but the next row's errors at columns already processed.  We
 * need only a few extra variables to hold the errors immediately around the
 * current column.  (If we are lucky, those variables are in registers, but
 * even if not, they're probably cheaper to access than array elements are.)
 *
 * We provide (#columns + 2) entries per component; the extra entry at each
 * end saves us from special-casing the first and last pixels.
 */

typedef INT16 FSERROR;		/* 16 bits should be enough */
typedef int LOCFSERROR;		/* use 'int' for calculation temps */

typedef struct { byte    *colorset;
		 FSERROR *fserrors;
	       } FSBUF;

/* Floyd-Steinberg initialization function.
 *
 * It is called 'fs2_init' since it's specialized for our purpose and
 * could be embedded in a more general FS-package.
 *
 * Returns a malloced FSBUF pointer which has to be passed as first
 * parameter to subsequent 'fs2_dither' calls.
 * The FSBUF structure does not need to be referenced by the calling
 * application, it can be treated from the app like a void pointer.
 *
 * The current implementation does only require to free() this returned
 * pointer after processing.
 *
 * Returns NULL if malloc fails.
 *
 * NOTE: The FSBUF structure is designed to allow the 'fs2_dither'
 * function to work with an *arbitrary* number of color components
 * at runtime! This is an enhancement over the IJG code base :-).
 * Only fs2_init() specifies the (maximum) number of components.
 */

static FSBUF *fs2_init(width)
int width;
{
  FSBUF *fs;
  FSERROR *p;

  fs = (FSBUF *)
    malloc(sizeof(FSBUF) * 3 + ((size_t)width + 2) * sizeof(FSERROR) * 3);
  if (fs == 0) return fs;

  fs[0].colorset = screen_set[0];
  fs[1].colorset = screen_set[1];
  fs[2].colorset = screen_set[2];

  p = (FSERROR *)(fs + 3);
  memset(p, 0, ((size_t)width + 2) * sizeof(FSERROR) * 3);

  fs[0].fserrors = p;
  fs[1].fserrors = p + 1;
  fs[2].fserrors = p + 2;

  return fs;
}

/* Floyd-Steinberg dithering function.
 *
 * NOTE:
 * (1) The image data referenced by 'ptr' is *overwritten* (input *and*
 *     output) to allow more efficient implementation.
 * (2) Alternate FS dithering is provided by the sign of 'nc'. Pass in
 *     a negative value for right-to-left processing. The return value
 *     provides the right-signed value for subsequent calls!
 * (3) This particular implementation assumes *no* padding between lines!
 *     Adapt this if necessary.
 */

static int fs2_dither(fs, ptr, nc, num_rows, num_cols)
FSBUF *fs;
byte *ptr;
int nc, num_rows, num_cols;
{
  int abs_nc, ci, row, col;
  LOCFSERROR delta, cur, belowerr, bpreverr;
  byte *dataptr, *colsetptr;
  FSERROR *errorptr;

  if ((abs_nc = nc) < 0) abs_nc = -abs_nc;
  for (row = 0; row < num_rows; row++) {
    for (ci = 0; ci < abs_nc; ci++, ptr++) {
      dataptr = ptr;
      colsetptr = fs[ci].colorset;
      errorptr = fs[ci].fserrors;
      if (nc < 0) {
	dataptr += (num_cols - 1) * abs_nc;
	errorptr += (num_cols + 1) * abs_nc;
      }
      cur = belowerr = bpreverr = 0;
      for (col = 0; col < num_cols; col++) {
	cur += errorptr[nc];
	cur += 8; cur >>= 4;
	if ((cur += *dataptr) < 0) cur = 0;
	else if (cur > 255) cur = 255;
	*dataptr = cur & 0xff;
	cur -= colsetptr[cur];
	delta = cur << 1; cur += delta;
	bpreverr += cur; cur += delta;
	belowerr += cur; cur += delta;
	errorptr[0] = (FSERROR)bpreverr;
	bpreverr = belowerr;
	belowerr = delta >> 1;
	dataptr += nc;
	errorptr += nc;
      }
      errorptr[0] = (FSERROR)bpreverr;
    }
    ptr += (num_cols - 1) * abs_nc;
    nc = -nc;
  }
  return nc;
}

#endif /* ENABLE_FIXPIX_SMOOTH */


#define DO_CROP 0
#define DO_ZOOM 1


/***********************************/
void Resize(w,h)
int w,h;
{
  RANGE(w,1,maxWIDE);  RANGE(h,1,maxHIGH);

  if (HaveSelection()) DrawSelection(0);  /* turn off old rect */

  if (psUp) PSResize();   /* if PSDialog is open, mention size change  */

  /* if same size, and Ximage created, do nothing */
  if (w==eWIDE && h==eHIGH && theImage!=NULL) return;

  if (DEBUG) fprintf(stderr,"Resize(%d,%d)  eSIZE=%d,%d  cSIZE=%d,%d\n",
		     w,h,eWIDE,eHIGH,cWIDE,cHIGH);

  if (epicMode == EM_SMOOTH) {  /* turn off smoothing */
    epicMode = EM_RAW;  SetEpicMode();
  }

  GenerateEpic(w,h);
  CreateXImage();
}



/********************************************/
void GenerateCpic()
{
  /* called when 'pic' has been modified (different contents, *not* different
     size, orientation, etc.  Rebuilds cpic. */

  int   i, j, bperpix;
  byte *pp, *cp;

  if (cpic == pic) return;     /* no cropping, nothing to do */

  cp = cpic;
  bperpix = (picType == PIC8) ? 1 : 3;

  for (i=0; i<cHIGH; i++) {
    if ((i&63)==0) WaitCursor();
    pp = pic + (i+cYOFF) * (pWIDE*bperpix) + (cXOFF * bperpix);
    for (j=0; j<cWIDE*bperpix; j++)
      *cp++ = *pp++;
  }
}



/***********************************/
void GenerateEpic(w,h)
int w,h;
{
  int          cy,ex,ey,*cxarr, *cxarrp;
  byte        *clptr,*elptr,*epptr;

  WaitCursor();
  clptr = NULL;  cxarrp = NULL;  cy = 0;  /* shut up compiler */

  SetISTR(ISTR_EXPAND, "%.5g%% x %.5g%%  (%d x %d)",
	  100.0 * ((float) w) / cWIDE,
	  100.0 * ((float) h) / cHIGH, w, h);

  if (DEBUG)
    fprintf(stderr,"GenerateEpic(%d,%d) eSIZE=%d,%d cSIZE=%d,%d epicode=%d\n",
		     w,h,eWIDE,eHIGH,cWIDE,cHIGH, epicMode);


  FreeEpic();                   /* get rid of the old one */
  eWIDE = w;  eHIGH = h;


  if (epicMode == EM_SMOOTH) {
    if (picType == PIC8) {
      epic = SmoothResize(cpic, cWIDE, cHIGH, eWIDE, eHIGH,
			  rMap,gMap,bMap, rdisp,gdisp,bdisp, numcols);
    }
    else {  /* PIC24 */
      epic = Smooth24(cpic, 1, cWIDE, cHIGH, eWIDE, eHIGH, NULL, NULL, NULL);
    }

    if (epic) return;   /* success */
    else {
      /* failed.  Try to generate a *raw* image, at least... */
      epicMode = EM_RAW;  SetEpicMode();
      /* fall through to rest of code */
    }
  }


  /* generate a 'raw' epic, as we'll need it for ColorDither if EM_DITH */

  if (eWIDE==cWIDE && eHIGH==cHIGH) {  /* 1:1 expansion.  point epic at cpic */
    epic = cpic;
  }
  else {
    /* run the rescaling algorithm */
    int bperpix;

    bperpix = (picType == PIC8) ? 1 : 3;

    WaitCursor();

    /* create a new epic of the appropriate size */

    epic = (byte *) malloc((size_t) (eWIDE * eHIGH * bperpix));
    if (!epic) FatalError("GenerateEpic():  unable to malloc 'epic'");

    /* the scaling routine.  not really all that scary after all... */

    /* OPTIMIZATON:  Malloc an eWIDE array of ints which will hold the
       values of the equation px = (pWIDE * ex) / eWIDE.  Faster than doing
       a mul and a div for every point in picture */

    cxarr = (int *) malloc(eWIDE * sizeof(int));
    if (!cxarr) FatalError("unable to allocate cxarr");

    for (ex=0; ex<eWIDE; ex++)
      cxarr[ex] = bperpix * ((cWIDE * ex) / eWIDE);

    elptr = epptr = epic;

    for (ey=0;  ey<eHIGH;  ey++, elptr+=(eWIDE*bperpix)) {
      ProgressMeter(0, (eHIGH)-1, ey, "Resize");
      if ((ey&63) == 0) WaitCursor();
      cy = (cHIGH * ey) / eHIGH;
      epptr = elptr;
      clptr = cpic + (cy * cWIDE * bperpix);

      if (bperpix == 1) {
	for (ex=0, cxarrp = cxarr;  ex<eWIDE;  ex++, epptr++)
	  *epptr = clptr[*cxarrp++];
      }
      else {
	int j;  byte *cp;

	for (ex=0, cxarrp = cxarr; ex<eWIDE; ex++,cxarrp++) {
	  cp = clptr + *cxarrp;
	  for (j=0; j<bperpix; j++)
	    *epptr++ = *cp++;
	}
      }
    }
    free(cxarr);
  }


  /* at this point, we have a raw epic.  Potentially dither it */
  if (picType == PIC8 && epicMode == EM_DITH) {
    byte *tmp;

    tmp = DoColorDither(NULL, epic, eWIDE, eHIGH, rMap,gMap,bMap,
			rdisp,gdisp,bdisp, numcols);
    if (tmp) {  /* success */
      FreeEpic();
      epic = tmp;
    }
    else {  /* well... just use the raw image. */
      epicMode = EM_RAW;  SetEpicMode();
    }
  }
}



/***********************************/
void DoZoom(x,y,button)
     int          x, y;
     unsigned int button;
{
  if      (button == Button1) do_zoom(x,y);
  else if (button == Button2) do_pan(x,y);
  else if (button == Button3) do_unzoom();
  else XBell(theDisp,0);
}


/***********************************/
static void do_zoom(mx,my)
     int mx,my;
{
  int i;
  int rx,ry,rx2,ry2, orx, ory, orw, orh;
  int px,py,pw,ph,opx,opy,opw,oph,m;
  Window rW, cW;  unsigned int mask;  int rtx, rty;

  m = 0;

  XSetFunction(theDisp, theGC, GXinvert);
  XSetPlaneMask(theDisp, theGC, xorMasks[m]);

  compute_zoom_rect(mx, my, &px, &py, &pw, &ph);
  CoordP2E(px,    py,    &rx, &ry);
  CoordP2E(px+pw, py+ph, &rx2,&ry2);
  opx=px;  opy=py;  opw=pw;  oph=ph;
  orx = rx;  ory = ry;  orw = (rx2-rx);  orh = (ry2-ry);
  XDrawRectangle(theDisp,mainW,theGC,orx,ory, (u_int)orw, (u_int)orh);

  /* track until Button1 is released.  If ctrl is released, cancel */
  while (1) {
    if (!XQueryPointer(theDisp,mainW,&rW,&cW,&rtx,&rty,
		       &mx,&my,&mask)) continue;

    if (!(mask & ControlMask)) break;
    if (!(mask & Button1Mask)) break;  /* button released */

    compute_zoom_rect(mx, my, &px, &py, &pw, &ph);
    if (px!=opx || py!=opy) {
      XDrawRectangle(theDisp,mainW,theGC, orx,ory, (u_int)orw, (u_int)orh);
      opx = px;  opy = py;  opw = pw;  opy = py;
      CoordP2E(opx,     opy,     &rx, &ry);
      CoordP2E(opx+opw, opy+oph, &rx2,&ry2);
      orx = rx;  ory = ry;  orw = (rx2-rx);  orh = (ry2-ry);
      XDrawRectangle(theDisp,mainW,theGC, orx,ory, (u_int)orw, (u_int)orh);
    }
    else {
      XDrawRectangle(theDisp,mainW,theGC, orx,ory, (u_int)orw, (u_int)orh);
      m = (m+1)&7;
      XSetPlaneMask(theDisp, theGC, xorMasks[m]);
      XDrawRectangle(theDisp,mainW,theGC, orx,ory, (u_int)orw, (u_int)orh);
      XFlush(theDisp);
      Timer(100);
    }
  }

  if (!(mask & ControlMask)) {  /* cancelled */
    XDrawRectangle(theDisp, mainW, theGC, orx, ory, (u_int) orw, (u_int) orh);
    XSetFunction(theDisp, theGC, GXcopy);
    XSetPlaneMask(theDisp, theGC, AllPlanes);
    return;
  }


  for (i=0; i<4; i++) {
    XDrawRectangle(theDisp, mainW, theGC, orx, ory, (u_int) orw, (u_int) orh);
    XFlush(theDisp);
    Timer(100);
  }

  XSetFunction(theDisp, theGC, GXcopy);
  XSetPlaneMask(theDisp, theGC, AllPlanes);

  /* if rectangle is *completely* outside epic, don't zoom */
  if (orx+orw<0 || ory+orh<0 || orx>=eWIDE || ory>=eHIGH) return;


  crop1(opx, opy, opw, oph, DO_ZOOM);
}


/***********************************/
static void compute_zoom_rect(x, y, px, py, pw, ph)
     int x, y, *px, *py, *pw, *ph;
{
  /* given a mouse pos (in epic coords), return x,y,w,h PIC coords for
     a 'zoom in by 2x' rectangle to be tracked.  The rectangle stays
     completely within 'pic' boundaries, and moves in 'pic' increments */

  CoordE2P(x, y, px, py);
  *pw = (cWIDE+1)/2;
  *ph = (cHIGH+1)/2;

  *px = *px - (*pw)/2;
  *py = *py - (*ph)/2;

  RANGE(*px, 0, pWIDE - *pw);
  RANGE(*py, 0, pHIGH - *ph);
}


/***********************************/
static void do_unzoom()
{
  int x,y,w,h, x2,y2, ex,ey,ew,eh;

  /* compute a cropping rectangle (in pic coordinates) that's twice
     the size of eWIDE,eHIGH, centered around eWIDE/2, eHIGH/2, but no
     larger than pWIDE,PHIGH */

  if (!but[BUNCROP].active) {    /* not cropped, can't zoom out */
    XBell(theDisp, 0);
    return;
  }

  ex = -eWIDE/2;  ey = -eHIGH/2;
  ew =  eWIDE*2;  eh =  eHIGH*2;

  CoordE2P(ex, ey, &x, &y);
  CoordE2P(ex+ew, ey+eh, &x2, &y2);
  w = x2 - x;  h = y2 - y;

  RANGE(w, 1, pWIDE);
  RANGE(h, 1, pHIGH);

  if (x<0) x = 0;
  if (y<0) y = 0;
  if (x+w > pWIDE) x = pWIDE - w;
  if (y+h > pHIGH) y = pHIGH - h;

  crop1(x,y,w,h, DO_ZOOM);
}


/***********************************/
static void do_pan(mx,my)
     int mx,my;
{
  int i, ox,oy,offx,offy, rw,rh, px, py, dx, dy,m;
  Window rW, cW;  unsigned int mask;  int rx, ry;

  offx = ox = mx;
  offy = oy = my;
  rw = eWIDE-1;  rh = eHIGH-1;
  m = 0;

  XSetFunction(theDisp, theGC, GXinvert);
  XSetPlaneMask(theDisp, theGC, xorMasks[m]);

  XDrawRectangle(theDisp,mainW,theGC, mx-offx, my-offy, (u_int)rw, (u_int)rh);

  /* track until Button2 is released */
  while (1) {
    if (!XQueryPointer(theDisp, mainW, &rW, &cW, &rx, &ry,
		       &mx, &my, &mask)) continue;
    if (!(mask & ControlMask)) break;  /* cancelled */
    if (!(mask & Button2Mask)) break;  /* button released */

    if (mask & ShiftMask) {    /* constrain mx,my to horiz or vertical */
      if (abs(mx-offx) > abs(my-offy)) my = offy;
      else mx = offx;
    }

    do_pan_calc(offx, offy, &mx, &my);

    if (mx!=ox || my!=oy) {
      XDrawRectangle(theDisp, mainW, theGC, ox-offx, oy-offy,
		     (u_int) rw, (u_int) rh);
      XDrawRectangle(theDisp, mainW, theGC, mx-offx, my-offy,
		     (u_int) rw, (u_int) rh);
      ox = mx;  oy = my;
    }
    else {
      XDrawRectangle(theDisp, mainW, theGC, ox-offx, oy-offy,
		     (u_int) rw, (u_int) rh);
      m = (m+1)&7;
      XSetPlaneMask(theDisp, theGC, xorMasks[m]);
      XDrawRectangle(theDisp, mainW, theGC, ox-offx, oy-offy,
		     (u_int) rw, (u_int) rh);
      XFlush(theDisp);
      Timer(100);
    }
  }

  mx = ox;  my = oy;  /* in case mx,my changed on button release */

  if (!(mask & ControlMask)) {  /* cancelled */
    XDrawRectangle(theDisp, mainW, theGC, mx-offx, my-offy,
		   (u_int) rw, (u_int) rh);
    XSetFunction(theDisp, theGC, GXcopy);
    XSetPlaneMask(theDisp, theGC, AllPlanes);
    return;
  }


  for (i=0; i<4; i++) {
    XDrawRectangle(theDisp, mainW, theGC, mx-offx, my-offy,
		   (u_int) rw, (u_int) rh);
    XFlush(theDisp);
    Timer(100);
  }


  /* mx-offx, my-offy is top-left corner of pan rect, in epic coords */

  CoordE2P(mx-offx, my-offy, &px, &py);
  dx = px - cXOFF;  dy = py - cYOFF;

  if (dx==0 && dy==0) {  /* didn't pan anywhere */
    XDrawRectangle(theDisp, mainW, theGC, mx-offx, my-offy,
		   (u_int) rw, (u_int) rh);
    XSetFunction(theDisp, theGC, GXcopy);
    XSetPlaneMask(theDisp, theGC, AllPlanes);
    return;
  }


  XSetFunction(theDisp, theGC, GXcopy);
  XSetPlaneMask(theDisp, theGC, AllPlanes);

  crop1(cXOFF-dx, cYOFF-dy, cWIDE, cHIGH, DO_ZOOM);
}


/***********************************/
static void do_pan_calc(offx, offy, xp,yp)
     int offx, offy, *xp, *yp;
{
  /* given mouse coords (in xp,yp) and original offset, compute 'clipped'
     coords (returned in xp,yp) such that the 'pan window' remains entirely
     within the image boundaries */

  int mx, my, eprx, epry, eprw, eprh, pprx, ppry, pprw, pprh;

  mx = *xp;  my = *yp;

  /* compute corners of pan rect in eWIDE,eHIGH coords */
  eprx = offx - mx;
  epry = offy - my;
  eprw = eWIDE;
  eprh = eHIGH;

  /* compute corners of pan rect in pWIDE,pHIGH coords */
  CoordE2P(eprx, epry, &pprx, &ppry);
  pprw = cWIDE;
  pprh = cHIGH;

  /* if pan rect (in p* coords) is outside bounds of pic, move it inside */
  if (pprx<0) pprx = 0;
  if (ppry<0) ppry = 0;
  if (pprx + pprw > pWIDE) pprx = pWIDE-pprw;
  if (ppry + pprh > pHIGH) ppry = pHIGH-pprh;

  /* convert clipped pan rect back into eWIDE,eHIGH coords */
  CoordP2E(pprx, ppry, &eprx, &epry);

  *xp = offx - eprx;
  *yp = offy - epry;
}


/***********************************/
void Crop()
{
  int x, y, w, h;

  if (!HaveSelection()) return;

  GetSelRCoords(&x,&y,&w,&h);
  EnableSelection(0);
  crop1(x,y,w,h,DO_CROP);
}


/**********************************/
static void crop1(x,y,w,h,zm)
     int x,y,w,h,zm;
{
  int   oldew,oldeh,oldcx,oldcy;

  oldcx = cXOFF;  oldcy = cYOFF;
  oldew = eWIDE;  oldeh = eHIGH;
  DoCrop(x,y,w,h);
  if (zm == DO_ZOOM) { eWIDE = oldew;  eHIGH = oldeh; }

  GenerateEpic(eWIDE, eHIGH);

  if (useroot) DrawEpic();
  else {
    if (zm == DO_CROP) {
      WCrop(eWIDE, eHIGH, cXOFF-oldcx, cYOFF-oldcy);  /* shrink window */
      CreateXImage();
    }
    else DrawEpic();
  }
  SetCursors(-1);
}


/***********************************/
void UnCrop()
{
  int w,h;

  if (cpic == pic) return;     /* not cropped */

  BTSetActive(&but[BUNCROP],0);

  if (epicMode == EM_SMOOTH) {   /* turn off smoothing */
    epicMode = EM_RAW;  SetEpicMode();
  }

  /* dispose of old cpic and epic */
  FreeEpic();
  if (cpic && cpic !=  pic) free(cpic);
  cpic = NULL;


  w = (pWIDE * eWIDE) / cWIDE;   h = (pHIGH * eHIGH) / cHIGH;
  if (w>maxWIDE || h>maxHIGH) {
    /* return to 'normal' size */
    if (pWIDE>maxWIDE || pHIGH>maxHIGH) {
      double r,wr,hr;
      wr = ((double) pWIDE) / maxWIDE;
      hr = ((double) pHIGH) / maxHIGH;

      r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
      w = (int) ((pWIDE / r) + 0.5);
      h = (int) ((pHIGH / r) + 0.5);
    }
    else { w = pWIDE;  h = pHIGH; }
  }

  cpic = pic;  cXOFF = cYOFF = 0;  cWIDE = pWIDE;  cHIGH = pHIGH;


  /* generate an appropriate 'epic' */
  GenerateEpic(w,h);
  CreateXImage();


  WUnCrop();
  SetCropString();
}


/***********************************/
void AutoCrop()
{
  /* called when AutoCrop button is pressed */
  int oldcx, oldcy;

  oldcx = cXOFF;  oldcy = cYOFF;

  if (DoAutoCrop()) {
    if (useroot) DrawEpic();
    else {
      CreateXImage();
      WCrop(eWIDE, eHIGH, cXOFF-oldcx, cYOFF-oldcy);
    }
  }

  SetCursors(-1);
}


/***********************************/
int DoAutoCrop()
{
  /* returns '1' if any cropping was actually done. */

  byte *cp, *cp1;
  int  i, ctop, cbot, cleft, cright;
  byte bgcol;

  ctop = cbot = cleft = cright = 0;

  if (picType == PIC24) return( doAutoCrop24() );

  /* crop the top */
  cp = cpic;
  bgcol = cp[0];

  while (ctop+1 < cHIGH) {
    /* see if we can delete this line */
    for (i=0, cp1=cp; i<cWIDE && *cp1==bgcol; i++, cp1++);
    if (i==cWIDE) { cp += cWIDE;  ctop++; }
    else break;
  }


  /* crop the bottom */
  cp = cpic + (cHIGH-1) * cWIDE;
  bgcol = cp[0];

  while (ctop + cbot + 1 < cHIGH) {
    /* see if we can delete this line */
    for (i=0, cp1=cp; i<cWIDE && *cp1==bgcol; i++,cp1++);
    if (i==cWIDE) { cp -= cWIDE;  cbot++; }
    else break;
  }


  /* crop the left side */
  cp = cpic;
  bgcol = cp[0];

  while (cleft + 1 < cWIDE) {
    /* see if we can delete this line */
    for (i=0, cp1=cp; i<cHIGH && *cp1==bgcol; i++, cp1 += cWIDE);
    if (i==cHIGH) { cp++; cleft++; }
    else break;
  }


  /* crop the right side */
  cp = cpic + cWIDE-1;
  bgcol = cp[0];

  while (cleft + cright + 1 < cWIDE) {
    /* see if we can delete this line */
    for (i=0, cp1=cp; i<cHIGH && *cp1==bgcol; i++, cp1 += cWIDE);
    if (i==cHIGH) { cp--; cright++; }
    else break;
  }

  /* do the actual cropping */
  if (cleft || ctop || cbot || cright) {
    DoCrop(cXOFF+cleft, cYOFF+ctop,
	    cWIDE-(cleft+cright), cHIGH-(ctop+cbot));
    return 1;
  }

  return 0;
}


/***********************************/
static int doAutoCrop24()
{
  /* returns '1' if any cropping was actually done */

  byte *cp, *cp1;
  int  i, ctop, cbot, cleft, cright;
  byte bgR, bgG, bgB;
  int maxmiss, misses, half;
  int r, g, b, R, G, B, oldr, oldg, oldb;
# define EPSILON 39		/* up to 15% (39/256ths) variance okay */
# define NEIGHBOR 16		/* within 6% of neighboring pixels */
# define MISSPCT 6		/* and up to 6% that don't match */
# define inabsrange(a,n) ( (a) < n && (a) > -n )


  if (cHIGH<3 || cWIDE<3) return 0;

  ctop = cbot = cleft = cright = 0;

  if (picType != PIC24) FatalError("doAutoCrop24 called when pic!=PIC24");

  /* crop the top */
  cp = cpic;
  half = cWIDE/2 * 3;
  maxmiss = cWIDE * MISSPCT / 100;
  bgR = cp[half+0];  bgG = cp[half+1];  bgB = cp[half+2];

  while (ctop+1 < cHIGH) {  /* see if we can delete this line */
    oldr = bgR; oldg = bgG; oldb = bgB;

    for (i=0, misses=0, cp1=cp; i<cWIDE && misses<maxmiss; i++, cp1+=3) {
      r=cp1[0]-bgR;  g=cp1[1]-bgG;  b=cp1[2]-bgB;
      R=cp1[0]-oldr; G=cp1[1]-oldg; B=cp1[2]-oldb;
      if (!inabsrange(r-g, EPSILON) ||
	  !inabsrange(r-b, EPSILON) ||
	  !inabsrange(b-g, EPSILON) ||
	  !inabsrange(R-G, NEIGHBOR) ||
	  !inabsrange(R-B, NEIGHBOR) ||
	  !inabsrange(B-G, NEIGHBOR)) misses++;
      oldr=r; oldg=g; oldb=b;
    }

    if (i==cWIDE) { cp += cWIDE*3;  ctop++; }   /* OK, we can crop this line */
    else break;
  }


  /* crop the bottom */
  cp = cpic + (cHIGH-1) * cWIDE*3;
  bgR = cp[half+0];  bgG = cp[half+1];  bgB = cp[half+2];

  while (ctop + cbot + 1 < cHIGH) {  /* see if we can delete this line */
    oldr = bgR; oldg = bgG; oldb = bgB;

    for (i=0, misses=0, cp1=cp; i<cWIDE && misses<maxmiss; i++, cp1+=3) {
      r=cp1[0]-bgR;  g=cp1[1]-bgG;  b=cp1[2]-bgB;
      R=cp1[0]-oldr; G=cp1[1]-oldg; B=cp1[2]-oldb;
      if (!inabsrange(r-g, EPSILON) ||
	  !inabsrange(r-b, EPSILON) ||
	  !inabsrange(b-g, EPSILON) ||
	  !inabsrange(R-G, NEIGHBOR) ||
	  !inabsrange(R-B, NEIGHBOR) ||
	  !inabsrange(B-G, NEIGHBOR)) misses++;
    }

    if (i==cWIDE) { cp -= cWIDE*3;  cbot++; }
    else break;
  }


  /* crop the left side */
  cp = cpic;
  half = (cHIGH/2) * cWIDE * 3;
  maxmiss = cHIGH * MISSPCT / 100;
  bgR = cp[half+0];  bgG = cp[half+1];  bgB = cp[half+2];

  while (cleft + 1 < cWIDE) {  /* see if we can delete this line */
    oldr = bgR; oldg = bgG; oldb = bgB;

    for (i=0, misses=0, cp1=cp; i<cHIGH && misses<maxmiss;
	 i++, cp1 += (cWIDE * 3)) {
      r=cp1[0]-bgR;  g=cp1[1]-bgG;  b=cp1[2]-bgB;
      R=cp1[0]-oldr; G=cp1[1]-oldg; B=cp1[2]-oldb;
      if (!inabsrange(r-g, EPSILON) ||
	  !inabsrange(r-b, EPSILON) ||
	  !inabsrange(b-g, EPSILON) ||
	  !inabsrange(R-G, NEIGHBOR) ||
	  !inabsrange(R-B, NEIGHBOR) ||
	  !inabsrange(B-G, NEIGHBOR)) misses++;
    }

    if (i==cHIGH) { cp+=3; cleft++; }
    else break;
  }


  /* crop the right side */
  cp = cpic + (cWIDE-1) * 3;
  bgR = cp[half+0];  bgG = cp[half+1];  bgB = cp[half+2];

  while (cleft + cright + 1 < cWIDE) {  /* see if we can delete this line */
    oldr = bgR; oldg = bgG; oldb = bgB;

    for (i=0, misses=0, cp1=cp; i<cHIGH && misses<maxmiss;
	 i++, cp1 += (cWIDE*3)) {
      r=cp1[0]-bgR;  g=cp1[1]-bgG;  b=cp1[2]-bgB;
      R=cp1[0]-oldr; G=cp1[1]-oldg; B=cp1[2]-oldb;
      if (!inabsrange(r-g, EPSILON) ||
	  !inabsrange(r-b, EPSILON) ||
	  !inabsrange(b-g, EPSILON) ||
	  !inabsrange(R-G, NEIGHBOR) ||
	  !inabsrange(R-B, NEIGHBOR) ||
	  !inabsrange(B-G, NEIGHBOR)) misses++;
    }

    if (i==cHIGH) { cp-=3; cright++; }
    else break;
  }


  /* do the actual cropping */
  if (cleft || ctop || cbot || cright) {
    if (cWIDE - (cleft + cright) < 1 ||
	cHIGH - (ctop  + cbot  ) < 1) return 0;    /* sanity check */

    DoCrop(cXOFF+cleft, cYOFF+ctop,
	   cWIDE-(cleft+cright), cHIGH-(ctop+cbot));
    return 1;
  }

  return 0;
}


/*******************************/
void DoCrop(x,y,w,h)
     int x,y,w,h;
{
  /* given a cropping rectangle in PIC coordinates, it regens cpic
     and sticks likely values into eWIDE,eHIGH, assuming you wanted to
     crop.  epic is not regnerated (but is freed) */

  int     i, j, bperpix;
  byte   *cp, *pp;
  double  expw, exph;


  bperpix = (picType == PIC8) ? 1 : 3;

  /* get the cropping rectangle inside pic, if it isn't... */
  RANGE(x, 0, pWIDE-1);
  RANGE(y, 0, pHIGH-1);
  if (w<1) w=1;
  if (h<1) h=1;
  if (x+w > pWIDE) w = pWIDE-x;
  if (y+h > pHIGH) h = pHIGH-y;


  FreeEpic();
  if (cpic && cpic !=  pic) free(cpic);
  cpic = NULL;

  expw = (double) eWIDE / (double) cWIDE;
  exph = (double) eHIGH / (double) cHIGH;

  cXOFF = x;  cYOFF = y;  cWIDE = w;  cHIGH = h;

  if (DEBUG) fprintf(stderr,"DoCrop(): cropping to %dx%d rectangle at %d,%d\n",
		     cWIDE, cHIGH, cXOFF, cYOFF);

  if (cWIDE == pWIDE && cHIGH == pHIGH) {   /* not really cropping */
    cpic = pic;
    cXOFF = cYOFF = 0;
  }
  else {
    /* at this point, we want to generate cpic, which will contain a
       cWIDE*cHIGH subsection of 'pic', top-left at cXOFF,cYOFF */

    cpic = (byte *) malloc((size_t) (cWIDE * cHIGH * bperpix));

    if (cpic == NULL) {
      fprintf(stderr,"%s: unable to allocate memory for cropped image\n", cmd);
      WUnCrop();
      cpic = pic;  cXOFF = cYOFF = 0;  cWIDE = pWIDE;  cHIGH = pHIGH;
      SetCropString();
      return;
    }

    /* copy relevant pixels from pic to cpic */
    cp = cpic;
    for (i=0; i<cHIGH; i++) {
      pp = pic + (i+cYOFF) * (pWIDE*bperpix) + (cXOFF * bperpix);
      for (j=0; j<cWIDE*bperpix; j++)
	*cp++ = *pp++;
    }
  }


  SetCropString();
  BTSetActive(&but[BUNCROP], (cpic!=pic));

  eWIDE = (int) (cWIDE * expw);
  eHIGH = (int) (cHIGH * exph);

  if (eWIDE>maxWIDE || eHIGH>maxHIGH) {  /* make 'normal' size */
    if (cWIDE>maxWIDE || cHIGH>maxHIGH) {
      double r,wr,hr;
      wr = ((double) cWIDE) / maxWIDE;
      hr = ((double) cHIGH) / maxHIGH;

      r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
      eWIDE = (int) ((cWIDE / r) + 0.5);
      eHIGH = (int) ((cHIGH / r) + 0.5);
    }
    else { eWIDE = cWIDE;  eHIGH = cHIGH; }
  }


  if (eWIDE<1) eWIDE = 1;
  if (eHIGH<1) eHIGH = 1;

  SetCursors(-1);
}



/***********************************/
void Rotate(dir)
     int dir;
{
  /* called when rotate CW and rotate CCW controls are clicked */
  /* dir=0: clockwise, else counter-clockwise */

  if (HaveSelection()) EnableSelection(0);

  DoRotate(dir);
  CreateXImage();
  WRotate();
}


/***********************************/
void DoRotate(dir)
     int dir;
{
  int i;

  /* dir=0: 90 degrees clockwise, else 90 degrees counter-clockwise */
  WaitCursor();

  RotatePic(pic, picType, &pWIDE, &pHIGH, dir);

  /* rotate clipped version and modify 'clip' coords */
  if (cpic != pic && cpic != NULL) {
    if (!dir) {
      i = pWIDE - (cYOFF + cHIGH);      /* have to rotate offsets */
      cYOFF = cXOFF;
      cXOFF = i;
    }
    else {
      i = pHIGH - (cXOFF + cWIDE);
      cXOFF = cYOFF;
      cYOFF = i;
    }
    WaitCursor();
    RotatePic(cpic, picType, &cWIDE, &cHIGH,dir);
  }
  else { cWIDE = pWIDE;  cHIGH = pHIGH; }

  /* rotate expanded version */
  if (epic != cpic && epic != NULL) {
    WaitCursor();
    RotatePic(epic, picType, &eWIDE, &eHIGH,dir);
  }
  else { eWIDE = cWIDE;  eHIGH = cHIGH; }


  SetISTR(ISTR_RES,"%d x %d",pWIDE,pHIGH);

  SetISTR(ISTR_EXPAND, "%.5g%% x %.5g%%  (%d x %d)",
	  100.0 * ((float) eWIDE) / cWIDE,
	  100.0 * ((float) eHIGH) / cHIGH, eWIDE, eHIGH);
}


/************************/
void RotatePic(pic, ptype, wp, hp, dir)
     byte *pic;
     int  *wp, *hp;
     int   ptype, dir;
{
  /* rotates a w*h array of bytes 90 deg clockwise (dir=0)
     or counter-clockwise (dir != 0).  swaps w and h */

  byte        *pic1, *pix1, *pix;
  int          i,j,bperpix;
  unsigned int w,h;

  bperpix = (ptype == PIC8) ? 1 : 3;

  w = *wp;  h = *hp;
  pix1 = pic1 = (byte *) malloc((size_t) (w*h*bperpix));
  if (!pic1) FatalError("Not enough memory to rotate!");

  /* do the rotation */
  if (dir==0) {
    for (i=0; i<w; i++) {       /* CW */
      if (bperpix == 1) {
	for (j=h-1, pix=pic+(h-1)*w + i;  j>=0;  j--, pix1++, pix-=w)
	  *pix1 = *pix;
      }
      else {
	int bperlin = w*bperpix;
	int k;

	for (j=h-1, pix=pic+(h-1)*w*bperpix + i*bperpix;
	     j>=0;  j--, pix -= bperlin)
	  for (k=0; k<bperpix; k++) *pix1++ = pix[k];
      }
    }
  }
  else {
    for (i=w-1; i>=0; i--) {    /* CCW */
      if (bperpix == 1) {
	for (j=0, pix=pic+i; j<h; j++, pix1++, pix+=w)
	  *pix1 = *pix;
      }
      else {
	int k;
	int bperlin = w*bperpix;

	for (j=0, pix=pic+i*bperpix; j<h; j++, pix+=bperlin)
	  for (k=0; k<bperpix; k++) *pix1++ = pix[k];
      }
    }
  }


  /* copy the rotated buffer into the original buffer */
  xvbcopy((char *) pic1, (char *) pic, (size_t) (w*h*bperpix));

  free(pic1);

  /* swap w and h */
  *wp = h;  *hp = w;
}



/***********************************/
void Flip(dir)
     int dir;
{
  /* dir=0: flip horizontally, else vertically
   *
   * Note:  flips pic, cpic, and epic.  Doesn't touch Ximage, nor does it draw
   */

  WaitCursor();

  if (HaveSelection()) {            /* only flip selection region */
    flipSel(dir);
    return;
  }

  FlipPic(pic, pWIDE, pHIGH, dir);

  /* flip clipped version */
  if (cpic && cpic != pic) {
    WaitCursor();
    FlipPic(cpic, cWIDE, cHIGH, dir);
  }

  /* flip expanded version */
  if (epic && epic != cpic) {
    WaitCursor();
    FlipPic(epic, eWIDE, eHIGH, dir);
  }
}


/************************/
void FlipPic(pic, w, h, dir)
     byte *pic;
     int w, h;
     int dir;
{
  /* flips a w*h array of bytes horizontally (dir=0) or vertically (dir!=0) */

  byte *plin;
  int   i,j,k,l,bperpix,bperlin;

  bperpix = (picType == PIC8) ? 1 : 3;
  bperlin = w * bperpix;

  if (dir==0) {                /* horizontal flip */
    byte *leftp, *rightp;

    for (i=0; i<h; i++) {
      plin   = pic + i*bperlin;
      leftp  = plin;
      rightp = plin + (w-1)*bperpix;

      for (j=0; j<w/2; j++, rightp -= (2*bperpix)) {
	for (l=0; l<bperpix; l++, leftp++, rightp++) {
	  k = *leftp;  *leftp = *rightp;  *rightp = k;
	}
      }
    }
  }

  else {                      /* vertical flip */
    byte *topp, *botp;

    for (i=0; i<w; i++) {
      topp = pic + i*bperpix;
      botp = pic + (h-1)*bperlin + i*bperpix;

      for (j=0; j<h/2; j++, topp+=(w-1)*bperpix, botp-=(w+1)*bperpix) {
	for (l=0; l<bperpix; l++, topp++, botp++) {
	  k = *topp;  *topp = *botp;  *botp = k;
	}
      }
    }
  }
}


/************************/
static void flipSel(dir)
     int dir;
{
  /* flips selected area in 'pic', regens cpic and epic appropriately */

  int   x,y,w,h;
  byte *plin;
  int   i,j,k,l,bperpix;

  GetSelRCoords(&x,&y,&w,&h);
  CropRect2Rect(&x,&y,&w,&h, 0,0,pWIDE,pHIGH);
  if (w<1) w=1;
  if (h<1) h=1;

  bperpix = (picType == PIC8) ? 1 : 3;

  if (dir==0) {                /* horizontal flip */
    byte *leftp, *rightp;

    for (i=y; i<y+h; i++) {
      plin   = pic + (i*pWIDE + x) * bperpix;
      leftp  = plin;
      rightp = plin + (w-1)*bperpix;

      for (j=0; j<w/2; j++, rightp -= (2*bperpix)) {
	for (l=0; l<bperpix; l++, leftp++, rightp++) {
	  k = *leftp;  *leftp = *rightp;  *rightp = k;
	}
      }
    }
  }

  else {                      /* vertical flip */
    byte *topp, *botp;

    for (i=x; i<x+w; i++) {
      topp = pic + ( y      * pWIDE + i) * bperpix;
      botp = pic + ((y+h-1) * pWIDE + i) * bperpix;

      for (j=0; j<h/2; j++, topp+=(pWIDE-1)*bperpix, botp-=(pWIDE+1)*bperpix) {
	for (l=0; l<bperpix; l++, topp++, botp++) {
	  k = *topp;  *topp = *botp;  *botp = k;
	}
      }
    }
  }

  GenerateCpic();
  GenerateEpic(eWIDE,eHIGH);
}


/************************/
void InstallNewPic()
{
  /* given a new pic and colormap, (or new 24-bit pic) installs everything,
     regens cpic and epic, and redraws image */

  /* toss old cpic and epic, if any */
  FreeEpic();
  if (cpic && cpic != pic) free(cpic);
  cpic = NULL;

  /* toss old colors, and allocate new ones */
  NewPicGetColors(0,0);

  /* generate cpic,epic,theImage from new 'pic' */
  crop1(cXOFF, cYOFF, cWIDE, cHIGH, DO_ZOOM);
  HandleDispMode();
}



/***********************************/
void DrawEpic()
{
  /* given an 'epic', builds a new Ximage, and draws it.  Basically
     called whenever epic is changed, or whenever color allocation
     changes (ie, the created X image will look different for the
     same epic) */

  CreateXImage();

  if (useroot) MakeRootPic();
  else DrawWindow(0,0,eWIDE,eHIGH);

  if (HaveSelection()) DrawSelection(0);
}


/************************************/
void KillOldPics()
{
  /* throw away all previous images */

  FreeEpic();
  if (cpic && cpic != pic) free(cpic);
  if (pic) free(pic);
  xvDestroyImage(theImage);   theImage = NULL;
  pic = egampic = epic = cpic = NULL;

  if (picComments) free(picComments);
  picComments = (char *) NULL;
  ChangeCommentText();
}



/************************/
static void floydDitherize1(ximage,pic824,ptype, wide, high, rmap, gmap, bmap)
     XImage *ximage;
     byte   *pic824, *rmap, *gmap, *bmap;
     int     ptype, wide, high;
{
  /* does floyd-steinberg ditherizing algorithm.
   *
   * takes a wide*high input image, of type 'ptype' (PIC8, PIC24)
   *     (if PIC8, colormap is specified by rmap,gmap,bmap)
   *
   * output is a 1-bit per pixel XYBitmap, packed 8 pixels per byte
   *
   * Note: this algorithm is *only* used when running on a 1-bit display
   */

  register byte   pix8, bit;
  int            *thisline, *nextline;
  int            *thisptr, *nextptr, *tmpptr;
  int             i, j, err, bperpix, bperln, order;
  byte           *pp, *image, w1, b1, w8, b8, rgb[256];


  if (ptype == PIC8) {   /* monoify colormap */
    for (i=0; i<256; i++)
      rgb[i] = MONO(rmap[i], gmap[i], bmap[i]);
  }


  image   = (byte *) ximage->data;
  bperln  = ximage->bytes_per_line;
  order   = ximage->bitmap_bit_order;
  bperpix = (ptype == PIC8) ? 1 : 3;


  thisline = (int *) malloc(wide * sizeof(int));
  nextline = (int *) malloc(wide * sizeof(int));
  if (!thisline || !nextline)
    FatalError("ran out of memory in floydDitherize1()\n");


  /* load up first line of picture */
  pp = pic824;
  if (ptype == PIC24) {
    for (j=0, tmpptr = nextline; j<wide; j++, pp+=3)
      *tmpptr++ = fsgamcr[MONO(pp[0], pp[1], pp[2])];
  }
  else {
    for (j=0, tmpptr = nextline; j<wide; j++, pp++)
      *tmpptr++ = fsgamcr[rgb[*pp]];
  }


  w1 = white&0x1;  b1=black&0x1;
  w8 = w1<<7;  b8 = b1<<7;        /* b/w bit in high bit */


  for (i=0; i<high; i++) {
    if ((i&0x3f) == 0) WaitCursor();

    /* get next line of image */
    tmpptr = thisline;  thisline = nextline;  nextline = tmpptr;  /* swap */
    if (i!=high-1) {
      pp = pic824 + (i+1) * wide * bperpix;
      if (ptype == PIC24) {
	for (j=0, tmpptr = nextline; j<wide; j++, pp+=3)
	  *tmpptr++ = fsgamcr[MONO(pp[0], pp[1], pp[2])];
      }
      else {
	for (j=0, tmpptr = nextline; j<wide; j++, pp++)
	  *tmpptr++ = fsgamcr[rgb[*pp]];
      }
    }

    thisptr = thisline;  nextptr = nextline;

    pp  = image + i*bperln;


    if (order==LSBFirst) {
      bit = pix8 = 0;
      for (j=0; j<wide; j++, thisptr++, nextptr++) {
	if (*thisptr<128) { err = *thisptr;     pix8 |= b8; }
	             else { err = *thisptr-255; pix8 |= w8; }

	if (bit==7) { *pp++ = pix8;  bit=pix8=0; }
	       else { pix8 >>= 1;  bit++; }

	if (j<wide-1) thisptr[1] += ((err*7)/16);

	if (i<high-1) {
	  nextptr[0] += ((err*5)/16);
	  if (j>0)      nextptr[-1] += ((err*3)/16);
	  if (j<wide-1) nextptr[ 1] += (err/16);
	}
      }
      if (bit) *pp++ = pix8>>(7-bit);  /* write partial byte at end of line */
    }

    else {   /* order==MSBFirst */
      bit = pix8 = 0;
      for (j=0; j<wide; j++, thisptr++, nextptr++) {
	if (*thisptr<128) { err = *thisptr;     pix8 |= b1; }
	             else { err = *thisptr-255; pix8 |= w1; }

	if (bit==7) { *pp++ = pix8;  bit=pix8=0; }
 	       else { pix8 <<= 1; bit++; }

	if (j<wide-1) thisptr[1] += ((err*7)/16);

	if (i<high-1) {
	  nextptr[0] += ((err*5)/16);
	  if (j>0)       nextptr[-1] += ((err*3)/16);
	  if (j<wide-1)  nextptr[ 1] += (err/16);
	}
      }
      if (bit) *pp++ = pix8<<(7-bit);  /* write partial byte at end of line */
    }
  }


  free(thisline);  free(nextline);
}





/************************/
byte *FSDither(inpic, intype, w, h, rmap, gmap, bmap,
	      bval, wval)
     byte *inpic, *rmap, *gmap, *bmap;
     int   w,h, intype, bval, wval;
{
  /* takes an input pic of size w*h, and type 'intype' (PIC8 or PIC24),
   *                (if PIC8, colormap specified by rmap,gmap,bmap)
   * and does the floyd-steinberg dithering algorithm on it.
   * generates (mallocs) a w*h 1-byte-per-pixel 'outpic', using 'bval'
   * and 'wval' as the 'black' and 'white' pixel values, respectively
   */

  int    i, j, err, w1, h1, npixels, linebufsize;
  byte  *pp, *outpic, rgb[256];
  int   *thisline, *nextline, *thisptr, *nextptr, *tmpptr;


  npixels = w * h;
  linebufsize = w * sizeof(int);
  if (w <= 0 || h <= 0 || npixels/w != h || linebufsize/w != sizeof(int)) {
    SetISTR(ISTR_WARNING, "Invalid image dimensions for dithering");
    return (byte *)NULL;
  }

  outpic = (byte *) malloc((size_t) npixels);
  if (!outpic) return outpic;


  if (intype == PIC8) {       /* monoify colormap */
    for (i=0; i<256; i++)
      rgb[i] = MONO(rmap[i], gmap[i], bmap[i]);
  }


  thisline = (int *) malloc(linebufsize);
  nextline = (int *) malloc(linebufsize);
  if (!thisline || !nextline)
    FatalError("ran out of memory in FSDither()\n");


  w1 = w-1;  h1 = h-1;

  /* load up first line of picture */
  pp = inpic;
  if (intype == PIC24) {
    for (j=0, tmpptr=nextline; j<w; j++, pp+=3)
      *tmpptr++ = fsgamcr[MONO(pp[0], pp[1], pp[2])];
  }
  else {
    for (j=0, tmpptr=nextline; j<w; j++, pp++)
      *tmpptr++ = fsgamcr[rgb[*pp]];
  }


  for (i=0; i<h; i++) {
    if ((i&0x3f) == 0) WaitCursor();

    /* get next line of picture */
    tmpptr = thisline;  thisline = nextline;  nextline = tmpptr;  /* swap */
    if (i!=h1) {
      if (intype == PIC24) {
	pp = inpic + (i+1) * w * 3;
	for (j=0, tmpptr=nextline; j<w; j++, pp+=3)
	  *tmpptr++ = fsgamcr[MONO(pp[0], pp[1], pp[2])];
      }
      else {
	pp = inpic + (i+1) * w;
	for (j=0, tmpptr = nextline; j<w; j++, pp++)
	  *tmpptr++ = fsgamcr[rgb[*pp]];
      }
    }

    pp  = outpic + i * w;
    thisptr = thisline;  nextptr = nextline;

    if ((i&1) == 0) {  /* go right */
      for (j=0; j<w; j++, pp++, thisptr++, nextptr++) {
	if (*thisptr<128) { err = *thisptr;     *pp = (byte) bval; }
	             else { err = *thisptr-255; *pp = (byte) wval; }

	if (j<w1) thisptr[1] += ((err*7)/16);

	if (i<h1) {
	  nextptr[0] += ((err*5)/16);
	  if (j>0)  nextptr[-1] += ((err*3)/16);
	  if (j<w1) nextptr[ 1] += (err/16);
	}
      }
    }

    else {   /* go left */
      pp += (w-1);  thisptr += (w-1);  nextptr += (w-1);
      for (j=w-1; j>=0; j--, pp--, thisptr--, nextptr--) {
	if (*thisptr<128) { err = *thisptr;     *pp = (byte) bval; }
	             else { err = *thisptr-255; *pp = (byte) wval; }

	if (j>0) thisptr[-1] += ((err*7)/16);

	if (i<h1) {
	  nextptr[0] += ((err*5)/16);
	  if (j>0)  nextptr[-1] += (err/16);
	  if (j<w1) nextptr[ 1] += ((err*3)/16);
	}
      }
    }
  }

  free(thisline);  free(nextline);
  return outpic;
}






/***********************************/
void CreateXImage()
{
  xvDestroyImage(theImage);   theImage = NULL;

  if (!epic) GenerateEpic(eWIDE, eHIGH);  /* shouldn't happen... */

  if (picType == PIC24) {  /* generate egampic */
    if (egampic && egampic != epic) free(egampic);
    egampic = GammifyPic24(epic, eWIDE, eHIGH);
    if (!egampic) egampic = epic;
  }


  if (picType == PIC8)
    theImage = Pic8ToXImage(epic,     (u_int) eWIDE, (u_int) eHIGH,
			    cols, rMap, gMap, bMap);
  else if (picType == PIC24)
    theImage = Pic24ToXImage(egampic, (u_int) eWIDE, (u_int) eHIGH);
}




/***********************************/
XImage *Pic8ToXImage(pic8, wide, high, xcolors, rmap, gmap, bmap)
     byte          *pic8, *rmap, *gmap, *bmap;
     unsigned int   wide, high;
     unsigned long *xcolors;
{
  /*
   * this has to do the tricky bit of converting the data in 'pic8'
   * into something usable for X.
   *
   */


  int     i;
  unsigned long xcol;
  XImage *xim;
  byte   *dithpic;

  xim = (XImage *) NULL;
  dithpic = (byte *) NULL;

  if (!pic8) return xim;  /* shouldn't happen */

  if (DEBUG > 1)
    fprintf(stderr,"Pic8ToXImage(): creating a %dx%d Ximage, %d bits deep\n",
	    wide, high, dispDEEP);


  /* special case: 1-bit display */
  if (dispDEEP == 1) {
    byte  *imagedata;

    xim = XCreateImage(theDisp, theVisual, dispDEEP, XYPixmap, 0, NULL,
		       wide, high, 32, 0);
    if (!xim) FatalError("couldn't create xim!");

    imagedata = (byte *) malloc((size_t) (xim->bytes_per_line * high));
    if (!imagedata) FatalError("couldn't malloc imagedata");

    xim->data = (char *) imagedata;
    floydDitherize1(xim, pic8, PIC8, (int) wide, (int) high, rmap, gmap, bmap);
    return xim;
  }


  /* if ncols==0, do a 'black' and 'white' dither */
  if (ncols == 0) {
    /* note that if dispDEEP > 8, dithpic will just have '0' and '1' instead
       of 'black' and 'white' */

    dithpic = FSDither(pic8, PIC8, (int) wide, (int) high, rmap, gmap, bmap,
		       (int) ((dispDEEP <= 8) ? black : 0),
		       (int) ((dispDEEP <= 8) ? white : 1));
  }



  switch (dispDEEP) {

  case 8: {
    byte  *imagedata, *ip, *pp;
    int   j, imWIDE, nullCount;

    nullCount = (4 - (wide % 4)) & 0x03;  /* # of padding bytes per line */
    imWIDE = wide + nullCount;

    /* Now create the image data - pad each scanline as necessary */
    imagedata = (byte *) malloc((size_t) (imWIDE * high));
    if (!imagedata) FatalError("couldn't malloc imagedata");

    pp = (dithpic) ? dithpic : pic8;

    for (i=0, ip=imagedata; i<high; i++) {
      if (((i+1)&0x7f) == 0) WaitCursor();

      if (dithpic) {
	for (j=0; j<wide; j++, ip++, pp++) *ip = *pp;  /* pp already is Xval */
      }
      else {
	for (j=0; j<wide; j++, ip++, pp++) *ip = (byte) xcolors[*pp];
      }

      for (j=0; j<nullCount; j++, ip++) *ip = 0;
    }

    xim = XCreateImage(theDisp,theVisual,dispDEEP,ZPixmap,0,
		       (char *) imagedata,  wide,  high,
		       32, imWIDE);
    if (!xim) FatalError("couldn't create xim!");
  }
    break;



    /*********************************/

  case 4: {
    byte  *imagedata, *ip, *pp;
    byte *lip;
    int  bperline, half, j;

    xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
		        wide,  high, 8, 0);
    if (!xim) FatalError("couldn't create xim!");

    bperline = xim->bytes_per_line;
    imagedata = (byte *) malloc((size_t) (bperline * high));
    if (!imagedata) FatalError("couldn't malloc imagedata");
    xim->data = (char *) imagedata;


    pp = (dithpic) ? dithpic : pic8;

    if (xim->bits_per_pixel == 4) {
      for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	if (((i+1)&0x7f) == 0) WaitCursor();

	for (j=0, ip=lip, half=0; j<wide; j++,pp++,half++) {
	  xcol = ((dithpic) ? *pp : xcolors[*pp]) & 0x0f;

	  if (ImageByteOrder(theDisp) == LSBFirst) {
	    if (half&1) { *ip = *ip + (xcol<<4);  ip++; }
	    else *ip = xcol;
	  }
	  else {
	    if (half&1) { *ip = *ip + xcol;  ip++; }
	    else *ip = xcol << 4;
	  }
	}
      }
    }

    else if (xim->bits_per_pixel == 8) {
      for (i=wide*high, ip=imagedata; i>0; i--,pp++,ip++) {
	if (((i+1)&0x1ffff) == 0) WaitCursor();
	*ip = (dithpic) ? *pp : (byte) xcolors[*pp];
      }
    }

    else FatalError("This display's too bizarre.  Can't create XImage.");
  }
    break;


    /*********************************/

  case 2: {  /* by M.Kossa@frec.bull.fr (Marc Kossa) */
             /* MSBFirst mods added by dale@ntg.com (Dale Luck) */
             /* additional fixes by  evol@infko.uni-koblenz.de
		(Randolf Werner) for NeXT 2bit grayscale with MouseX */

    byte  *imagedata, *ip, *pp;
    byte *lip;
    int  bperline, half, j;

    xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
		        wide,  high, 8, 0);
    if (!xim) FatalError("couldn't create xim!");

    bperline = xim->bytes_per_line;
    imagedata = (byte *) malloc((size_t) (bperline * high));
    if (!imagedata) FatalError("couldn't malloc imagedata");
    xim->data = (char *) imagedata;

    pp = (dithpic) ? dithpic : pic8;

    if (xim->bits_per_pixel == 2) {
      for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	if (((i+1)&0x7f) == 0) WaitCursor();
	for (j=0, ip=lip, half=0; j<wide; j++,pp++,half++) {
	  xcol = ((dithpic) ? *pp : xcolors[*pp]) & 0x03;

	  if (xim->bitmap_bit_order == LSBFirst) {
	    if      (half%4==0) *ip  = xcol;
	    else if (half%4==1) *ip |= (xcol<<2);
	    else if (half%4==2) *ip |= (xcol<<4);
	    else              { *ip |= (xcol<<6); ip++; }
	  }

	  else {  /* MSBFirst.  NeXT, among others */
	    if      (half%4==0) *ip  = (xcol<<6);
	    else if (half%4==1) *ip |= (xcol<<4);
	    else if (half%4==2) *ip |= (xcol<<2);
	    else              { *ip |=  xcol;     ip++; }
	  }
	}
      }
    }

    else if (xim->bits_per_pixel == 4) {
      for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	if (((i+1)&0x7f) == 0) WaitCursor();

	for (j=0, ip=lip, half=0; j<wide; j++,pp++,half++) {
	  xcol = ((dithpic) ? *pp : xcolors[*pp]) & 0x0f;

	  if (xim->bitmap_bit_order == LSBFirst) {
	    if (half&1) { *ip |= (xcol<<4);  ip++; }
	    else *ip = xcol;
	  }

	  else { /* MSBFirst */
	    if (half&1) { *ip |= xcol;  ip++; }
	    else *ip = xcol << 4;
	  }
	}
      }
    }

    else if (xim->bits_per_pixel == 8) {
      for (i=wide*high, ip=imagedata; i>0; i--,pp++,ip++) {
	if (((i+1)&0x1ffff) == 0) WaitCursor();
	*ip = (dithpic) ? *pp : (byte) xcolors[*pp];
      }
    }

    else FatalError("This display's too bizarre.  Can't create XImage.");
  }
    break;


  /*********************************/

  case 5:
  case 6: {
    byte  *imagedata, *ip, *pp;
    int  bperline;

    xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
		        wide,  high, 8, 0);
    if (!xim) FatalError("couldn't create xim!");

    if (xim->bits_per_pixel != 8)
      FatalError("This display's too bizarre.  Can't create XImage.");

    bperline = xim->bytes_per_line;
    imagedata = (byte *) malloc((size_t) (bperline * high));
    if (!imagedata) FatalError("couldn't malloc imagedata");
    xim->data = (char *) imagedata;

    pp = (dithpic) ? dithpic : pic8;

    for (i=wide*high, ip=imagedata; i>0; i--,pp++,ip++) {
      if (((i+1)&0x1ffff) == 0) WaitCursor();
      *ip = (dithpic) ? *pp : (byte) xcolors[*pp];
    }
  }
    break;


  /*********************************/

  case 12:
  case 15:
  case 16: {
    byte  *imagedata, *ip, *pp;

    imagedata = (byte *) malloc((size_t) (2*wide*high));
    if (!imagedata) FatalError("couldn't malloc imagedata");

    xim = XCreateImage(theDisp,theVisual,dispDEEP,ZPixmap,0,
		       (char *) imagedata,  wide,  high, 16, 0);
    if (!xim) FatalError("couldn't create xim!");

    if (dispDEEP == 12 && xim->bits_per_pixel != 16) {
      char buf[128];
      sprintf(buf,"No code for this type of display (depth=%d, bperpix=%d)",
	      dispDEEP, xim->bits_per_pixel);
      FatalError(buf);
    }

    pp = (dithpic) ? dithpic : pic8;

    if (xim->byte_order == MSBFirst) {
      for (i=wide*high, ip=imagedata; i>0; i--,pp++) {
	if (((i+1)&0x1ffff) == 0) WaitCursor();

	if (dithpic) xcol = ((*pp) ? white : black) & 0xffff;
		else xcol = xcolors[*pp] & 0xffff;

	*ip++ = (xcol>>8) & 0xff;
	*ip++ = (xcol) & 0xff;
      }
    }
    else {   /* LSBFirst */
      for (i=wide*high, ip=imagedata; i>0; i--,pp++) {
	if (((i+1)&0x1ffff) == 0) WaitCursor();

	if (dithpic) xcol = ((*pp) ? white : black) & 0xffff;
	        else xcol = xcolors[*pp];

	*ip++ = (xcol) & 0xff;
	*ip++ = (xcol>>8) & 0xff;
      }
    }
  }
    break;


    /*********************************/

  case 24:
  case 32: {
    byte  *imagedata, *ip, *pp, *tip;
    int    j, do32;

    imagedata = (byte *) malloc((size_t) (4*wide*high));
    if (!imagedata) FatalError("couldn't malloc imagedata");

    xim = XCreateImage(theDisp,theVisual,dispDEEP,ZPixmap,0,
		       (char *) imagedata,  wide,  high, 32, 0);
    if (!xim) FatalError("couldn't create xim!");

    do32 = (xim->bits_per_pixel == 32);

    pp = (dithpic) ? dithpic : pic8;

    if (xim->byte_order == MSBFirst) {
      for (i=0, ip=imagedata; i<high; i++) {
	if (((i+1)&0x7f) == 0) WaitCursor();
	for (j=0, tip=ip; j<wide; j++, pp++) {
	  xcol = (dithpic) ? ((*pp) ? white : black) : xcolors[*pp];

	  if (do32) *tip++ = 0;
	  *tip++ = (xcol>>16) & 0xff;
	  *tip++ = (xcol>>8) & 0xff;
	  *tip++ =  xcol & 0xff;
	}
	ip += xim->bytes_per_line;
      }
    }

    else {  /* LSBFirst */
      for (i=0, ip=imagedata; i<high; i++) {
	if (((i+1)&0x7f) == 0) WaitCursor();
	for (j=0, tip=ip; j<wide; j++, pp++) {
	  xcol = (dithpic) ? ((*pp) ? white : black) : xcolors[*pp];

	  *tip++ =  xcol & 0xff;
	  *tip++ = (xcol>>8) & 0xff;
	  *tip++ = (xcol>>16) & 0xff;
	  if (do32) *tip++ = 0;
	}
	ip += xim->bytes_per_line;
      }
    }
  }
    break;


    /*********************************/

  default:
    sprintf(dummystr,"no code to handle this display type (%d bits deep)",
	    dispDEEP);
    FatalError(dummystr);
    break;
  }


  if (dithpic) free(dithpic);

  return(xim);
}



/***********************************/
XImage *Pic24ToXImage(pic24, wide, high)
     byte          *pic24;
     unsigned int   wide, high;
{
  /*
   * This has to do the none-too-simple bit of converting the data in 'pic24'
   * into something usable by X.
   *
   * There are two major approaches:  if we're displaying on a TrueColor
   * or DirectColor display, we've got all the colors we're going to need,
   * and 'all we have to do' is convert 24-bit RGB pixels into whatever
   * variation of RGB the X device in question wants.  No color allocation
   * is involved.
   *
   * Alternately, if we're on a PseudoColor, GrayScale, StaticColor or
   * StaticGray display, we're going to continue to operate in an 8-bit
   * mode.  (In that by this point, a 3/3/2 standard colormap has been
   * created for our use (though all 256 colors may not be unique...), and
   * we're just going to display the 24-bit picture by dithering with those
   * colors.)
   *
   */

  int     i,j;
  XImage *xim;

  xim     = (XImage *) NULL;

  if (!pic24) return xim;  /* shouldn't happen */


  /* special case: 1-bit display.  Doesn't much matter *what* the visual is */
  if (dispDEEP == 1) {
    byte  *imagedata;

    xim = XCreateImage(theDisp, theVisual, dispDEEP, XYPixmap, 0, NULL,
		        wide,  high, 32, 0);
    if (!xim) FatalError("couldn't create xim!");

    imagedata = (byte *) malloc((size_t) (xim->bytes_per_line * high));
    if (!imagedata) FatalError("couldn't malloc imagedata");

    xim->data = (char *) imagedata;
    floydDitherize1(xim, pic24,PIC24, (int) wide, (int) high, NULL,NULL,NULL);

    return xim;
  }




  if (theVisual->class == TrueColor || theVisual->class == DirectColor) {

    /************************************************************************/
    /* Non-ColorMapped Visuals:  TrueColor, DirectColor                     */
    /************************************************************************/

    unsigned long xcol;
    int           bperpix, bperline;
    byte         *imagedata, *lip, *ip, *pp;


    xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
		        wide,  high, 32, 0);
    if (!xim) FatalError("couldn't create X image!");

    bperline = xim->bytes_per_line;
    bperpix  = xim->bits_per_pixel;

    imagedata = (byte *) malloc((size_t) (high * bperline));
    if (!imagedata) FatalError("couldn't malloc imagedata");

    xim->data = (char *) imagedata;

    if (bperpix != 8 && bperpix != 16 && bperpix != 24 && bperpix != 32) {
      char buf[128];
      sprintf(buf,"Sorry, no code written to handle %d-bit %s",
	      bperpix, "TrueColor/DirectColor displays!");
      FatalError(buf);
    }

    screen_init();

#ifdef ENABLE_FIXPIX_SMOOTH
    if (do_fixpix_smooth) {
#if 0
      /* If we wouldn't have to save the original pic24 image data,
       * the following code would do the dither job by overwriting
       * the image data, and the normal render code would then work
       * without any change on that data.
       * Unfortunately, this approach would hurt the xv assumptions...
       */
      if (bperpix < 24) {
        FSBUF *fs = fs2_init(wide);
        if (fs) {
	  fs2_dither(fs, pic24, 3, high, wide);
	  free(fs);
        }
      }
#else
      /* ...so we have to take a different approach with linewise
       * dithering/rendering in a loop using a temporary line buffer.
       */
      if (bperpix < 24) {
        FSBUF *fs = fs2_init(wide);
        if (fs) {
	  byte *row_buf = malloc((size_t)wide * 3);
	  if (row_buf) {
	    int nc = 3;
	    byte *picp = pic24;  lip = imagedata;

	    switch (bperpix) {
	      case 8:
	        for (i=0; i<high; i++, lip+=bperline, picp+=(size_t)wide*3) {
	          memcpy(row_buf, picp, (size_t)wide * 3);
	          nc = fs2_dither(fs, row_buf, nc, 1, wide);
	          for (j=0, ip=lip, pp=row_buf; j<wide; j++) {
	            xcol  = screen_rgb[0][*pp++];
	            xcol |= screen_rgb[1][*pp++];
	            xcol |= screen_rgb[2][*pp++];
		    *ip++ = xcol & 0xff;
	          }
	        }
		break;

	      case 16:
	        for (i=0; i<high; i++, lip+=bperline, picp+=(size_t)wide*3) {
	          CARD16 *ip16 = (CARD16 *)lip;
	          memcpy(row_buf, picp, (size_t)wide * 3);
	          nc = fs2_dither(fs, row_buf, nc, 1, wide);
	          for (j=0, pp=row_buf; j<wide; j++) {
	            xcol  = screen_rgb[0][*pp++];
	            xcol |= screen_rgb[1][*pp++];
	            xcol |= screen_rgb[2][*pp++];
		    *ip16++ = (CARD16)xcol;
	          }
	        }
		break;
	    } /* end switch */

	    free(row_buf);
	    free(fs);

	    return xim;
	  }
	  free(fs);
        }
      }
#endif /* 0? */
    }
#endif /* ENABLE_FIXPIX_SMOOTH */

    lip = imagedata;  pp = pic24;

    switch (bperpix) {
      case 8:
        for (i=0; i<high; i++, lip+=bperline) {
          for (j=0, ip=lip; j<wide; j++) {
	    xcol  = screen_rgb[0][*pp++];
	    xcol |= screen_rgb[1][*pp++];
	    xcol |= screen_rgb[2][*pp++];
	    *ip++ = xcol & 0xff;
          }
        }
        break;

      case 16:
        for (i=0; i<high; i++, lip+=bperline) {
          CARD16 *ip16 = (CARD16 *)lip;
          for (j=0; j<wide; j++) {
	    xcol  = screen_rgb[0][*pp++];
	    xcol |= screen_rgb[1][*pp++];
	    xcol |= screen_rgb[2][*pp++];
	    *ip16++ = (CARD16)xcol;
          }
        }
        break;

      case 24:
        for (i=0; i<high; i++, lip+=bperline) {
          for (j=0, ip=lip; j<wide; j++) {
	    xcol  = screen_rgb[0][*pp++];
	    xcol |= screen_rgb[1][*pp++];
	    xcol |= screen_rgb[2][*pp++];
#ifdef USE_24BIT_ENDIAN_FIX
	    if (border == MSBFirst) {
	      *ip++ = (xcol>>16) & 0xff;
	      *ip++ = (xcol>>8)  & 0xff;
	      *ip++ =  xcol      & 0xff;
	    }
	    else {  /* LSBFirst */
	      *ip++ =  xcol      & 0xff;
	      *ip++ = (xcol>>8)  & 0xff;
	      *ip++ = (xcol>>16) & 0xff;
	    }
#else /* GRR:  this came with the FixPix patch, but I don't think it's right */
	    *ip++ = (xcol >> 16) & 0xff;    /* (no way to test, however, so  */
	    *ip++ = (xcol >> 8)  & 0xff;    /* it's left enabled by default) */
	    *ip++ =  xcol        & 0xff;
#endif
          }
        }
        break;

      case 32:
        for (i=0; i<high; i++, lip+=bperline) {
          CARD32 *ip32 = (CARD32 *)lip;
          for (j=0; j<wide; j++) {
	    xcol  = screen_rgb[0][*pp++];
	    xcol |= screen_rgb[1][*pp++];
	    xcol |= screen_rgb[2][*pp++];
	    *ip32++ = (CARD32)xcol;
          }
        }
        break;
    } /* end switch */
  }

  else {

    /************************************************************************/
    /* CMapped Visuals:  PseudoColor, GrayScale, StaticGray, StaticColor... */
    /************************************************************************/

    byte *pic8;
    int   bwdith;

    /* in all cases, make an 8-bit version of the image, either using
       'black' and 'white', or the stdcmap */

    bwdith = 0;

    if (ncols == 0 && dispDEEP != 1) {   /* do 'black' and 'white' dither */
      /* note that if dispDEEP > 8, pic8 will just have '0' and '1' instead
	 of 'black' and 'white' */

      pic8 = FSDither(pic24, PIC24, (int) wide, (int) high, NULL, NULL, NULL,
		      (int) ((dispDEEP <= 8) ? black : 0),
		      (int) ((dispDEEP <= 8) ? white : 1));
      bwdith = 1;
    }

    else {                               /* do color dither using stdcmap */
      pic8 = Do332ColorDither(pic24, NULL, (int) wide, (int) high,
			      NULL, NULL, NULL,
			      stdrdisp, stdgdisp, stdbdisp, 256);
    }

    if (!pic8) FatalError("out of memory in Pic24ToXImage()\n");


    /* DISPLAY-DEPENDENT code follows... */


    switch (dispDEEP) {


    case 8: {
      byte  *imagedata, *ip, *pp;
      int   j, imWIDE, nullCount;

      nullCount = (4 - (wide % 4)) & 0x03;  /* # of padding bytes per line */
      imWIDE = wide + nullCount;

      /* Now create the image data - pad each scanline as necessary */
      imagedata = (byte *) malloc((size_t) (imWIDE * high));
      if (!imagedata) FatalError("couldn't malloc imagedata");

      for (i=0, pp=pic8, ip=imagedata; i<high; i++) {
	if (((i+1)&0x7f) == 0) WaitCursor();

	if (bwdith)
	  for (j=0; j<wide; j++, ip++, pp++) *ip = *pp;
	else
	  for (j=0; j<wide; j++, ip++, pp++) *ip = stdcols[*pp];

	for (j=0; j<nullCount; j++, ip++)  *ip = 0;
      }

      xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0,
			 (char *) imagedata,  wide,  high,
			 32, imWIDE);
      if (!xim) FatalError("couldn't create xim!");
    }
      break;


      /*********************************/

    case 4: {
      byte         *imagedata, *ip, *pp;
      byte         *lip;
      int           bperline, half, j;
      unsigned long xcol;

      xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
			  wide,  high, 32, 0);
      if (!xim) FatalError("couldn't create xim!");

      bperline = xim->bytes_per_line;
      imagedata = (byte *) malloc((size_t) (bperline * high));
      if (!imagedata) FatalError("couldn't malloc imagedata");
      xim->data = (char *) imagedata;

      pp = pic8;

      if (xim->bits_per_pixel == 4) {
	for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	  if (((i+1)&0x7f) == 0) WaitCursor();

	  for (j=0, ip=lip, half=0; j<wide; j++,pp++,half++) {
	    xcol = ((bwdith) ? *pp : stdcols[*pp]) & 0x0f;

	    if (xim->byte_order == LSBFirst) {
	      if (half&1) { *ip = *ip + (xcol<<4);  ip++; }
	      else *ip = xcol;
	    }
	    else {
	      if (half&1) { *ip = *ip + xcol;  ip++; }
	      else *ip = xcol << 4;
	    }
	  }
	}
      }

      else if (xim->bits_per_pixel == 8) {
	for (i=0,lip=imagedata; i<high; i++,lip+=bperline) {
	  if (((i+1)&0x7f)==0) WaitCursor();
	  for (j=0,ip=lip; j<wide; j++,pp++,ip++) {
	    *ip = (bwdith) ? *pp : (byte) stdcols[*pp];
	  }
	}
      }

      else FatalError("This display's too bizarre.  Can't create XImage.");
    }
      break;



      /*********************************/

    case 2: {  /* by M.Kossa@frec.bull.fr (Marc Kossa) */
               /* MSBFirst mods added by dale@ntg.com (Dale Luck) */
               /* additional fixes by  evol@infko.uni-koblenz.de
		  (Randolf Werner) for NeXT 2bit grayscale with MouseX */

      byte  *imagedata, *ip, *pp;
      byte *lip;
      int  bperline, half, j;
      unsigned long xcol;

      xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
			  wide,  high, 32, 0);
      if (!xim) FatalError("couldn't create xim!");

      bperline = xim->bytes_per_line;
      imagedata = (byte *) malloc((size_t) (bperline * high));
      if (!imagedata) FatalError("couldn't malloc imagedata");
      xim->data = (char *) imagedata;

      pp = pic8;

      if (xim->bits_per_pixel == 2) {
	for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	  if (((i+1)&0x7f) == 0) WaitCursor();
	  for (j=0, ip=lip, half=0; j<wide; j++,pp++,half++) {
	    xcol = ((bwdith) ? *pp : stdcols[*pp]) & 0x03;

	    if (xim->bitmap_bit_order == LSBFirst) {
	      if      (half%4==0) *ip  = xcol;
	      else if (half%4==1) *ip |= (xcol<<2);
	      else if (half%4==2) *ip |= (xcol<<4);
	      else              { *ip |= (xcol<<6); ip++; }
	    }

	    else {  /* MSBFirst.  NeXT, among others */
	      if      (half%4==0) *ip  = (xcol<<6);
	      else if (half%4==1) *ip |= (xcol<<4);
	      else if (half%4==2) *ip |= (xcol<<2);
	      else              { *ip |=  xcol;     ip++; }
	    }
	  }
	}
      }

      else if (xim->bits_per_pixel == 4) {
	for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	  if (((i+1)&0x7f) == 0) WaitCursor();

	  for (j=0, ip=lip, half=0; j<wide; j++,pp++,half++) {
	    xcol = ((bwdith) ? *pp : stdcols[*pp]) & 0x0f;

	    if (xim->bitmap_bit_order == LSBFirst) {
	      if (half&1) { *ip |= (xcol<<4);  ip++; }
	      else *ip = xcol;
	    }

	    else { /* MSBFirst */
	      if (half&1) { *ip |= xcol;  ip++; }
	      else *ip = xcol << 4;
	    }
	  }
	}
      }

      else if (xim->bits_per_pixel == 8) {
	for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	  if (((i+1)&0x7f) == 0) WaitCursor();

	  for (j=0, ip=lip; j<wide; j++,pp++,ip++) {
	    *ip = ((bwdith) ? *pp : stdcols[*pp]) & 0xff;
	  }
	}
      }

      else FatalError("This display's too bizarre.  Can't create XImage.");
    }
      break;


      /*********************************/

    case 6: {
      byte  *imagedata, *lip, *ip, *pp;
      int  bperline;

      xim = XCreateImage(theDisp, theVisual, dispDEEP, ZPixmap, 0, NULL,
			  wide,  high, 32, 0);
      if (!xim) FatalError("couldn't create xim!");

      if (xim->bits_per_pixel != 8)
	FatalError("This display's too bizarre.  Can't create XImage.");

      bperline = xim->bytes_per_line;
      imagedata = (byte *) malloc((size_t) (bperline * high));
      if (!imagedata) FatalError("couldn't malloc imagedata");
      xim->data = (char *) imagedata;

      pp = pic8;

      for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	if (((i+1)&0x7f) == 0) WaitCursor();

	for (j=0, ip=lip; j<wide; j++,pp++,ip++) {
	  *ip = ((bwdith) ? *pp : stdcols[*pp]) & 0x3f;

	}
      }
    }
      break;


      /*********************************/

    case 15:
    case 16: {
      unsigned short  *imagedata, *ip, *lip;
      byte   *pp;
      int     bperline;
      unsigned long xcol;

      imagedata = (unsigned short *) malloc((size_t) (2*wide*high));
      if (!imagedata) FatalError("couldn't malloc imagedata");

      xim = XCreateImage(theDisp,theVisual,dispDEEP,ZPixmap,0,
			 (char *) imagedata, wide, high, 32, 0);
      if (!xim) FatalError("couldn't create xim!");
      bperline = xim->bytes_per_line;

      pp = pic8;

      if (xim->byte_order == MSBFirst) {
	for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	  if (((i+1)&0x7f) == 0) WaitCursor();

	  for (j=0, ip=lip; j<wide; j++,pp++) {
	    *ip++ = ((bwdith) ? *pp : stdcols[*pp]) & 0xffff;
	  }
	}
      }

      else {   /* LSBFirst */
	for (i=0, lip=imagedata; i<high; i++, lip+=bperline) {
	  if (((i+1)&0x7f) == 0) WaitCursor();

	  for (j=0, ip=lip; j<wide; j++,pp++) {
	    xcol = ((bwdith) ? *pp : stdcols[*pp]) & 0xffff;
	    /* WAS *ip++ = ((xcol>>8) & 0xff) | ((xcol&0xff) << 8);  */
	    *ip++ = (unsigned short) (xcol);
	  }
	}
      }
    }
      break;


      /*********************************/

      /* this wouldn't seem likely to happen, but what the heck... */

    case 24:
    case 32: {
      byte  *imagedata, *ip, *pp;
      unsigned long xcol;
      int bperpix;

      imagedata = (byte *) malloc((size_t) (4*wide*high));
      if (!imagedata) FatalError("couldn't malloc imagedata");

      xim = XCreateImage(theDisp,theVisual,dispDEEP,ZPixmap,0,
			 (char *) imagedata, wide, high, 32, 0);
      if (!xim) FatalError("couldn't create xim!");

      bperpix = xim->bits_per_pixel;

      pp = pic8;

      if (xim->byte_order == MSBFirst) {
	for (i=wide*high, ip=imagedata; i>0; i--,pp++) {
	  if (((i+1)&0x1ffff) == 0) WaitCursor();
	  xcol = (bwdith) ? *pp : stdcols[*pp];

	  if (bperpix == 32) *ip++ = 0;
	  *ip++ = (xcol>>16) & 0xff;
	  *ip++ = (xcol>>8)  & 0xff;
	  *ip++ =  xcol      & 0xff;
	}
      }

      else {  /* LSBFirst */
	for (i=wide*high, ip=imagedata; i>0; i--,pp++) {
	  xcol = (bwdith) ? *pp : stdcols[*pp];

	  if (((i+1)&0x1ffff) == 0) WaitCursor();
	  *ip++ =  xcol      & 0xff;
	  *ip++ = (xcol>>8)  & 0xff;
	  *ip++ = (xcol>>16) & 0xff;
	  if (bperpix == 32) *ip++ = 0;
	}
      }
    }
      break;

    }   /* end of the switch */

    free(pic8);  /* since we ALWAYS make a copy of it into imagedata */
  }


  return xim;
}



/***********************************************************/
void Set824Menus(mode)
     int mode;
{
  /* move checkmark */
  conv24MB.flags[CONV24_8BIT]  = (mode==PIC8);
  conv24MB.flags[CONV24_24BIT] = (mode==PIC24);

  if (mode == PIC24) {
    dispMB.dim[DMB_COLNORM] = 1;
    dispMB.dim[DMB_COLPERF] = 1;
    dispMB.dim[DMB_COLOWNC] = 1;

    /* turn off RAW/DITH/SMOOTH buttons (caused by picType) */
    epicMode = EM_RAW;
    SetEpicMode();

    /* turn off autoapply mode */
    /* GamSetAutoApply(0); */       /* or not! */
  }

  else if (mode == PIC8) {
    dispMB.dim[DMB_COLNORM] = 0;
    dispMB.dim[DMB_COLPERF] = (dispMode == RMB_WINDOW) ? 0 : 1;
    dispMB.dim[DMB_COLOWNC] = (dispMode == RMB_WINDOW) ? 0 : 1;

    /* turn on RAW/DITH/SMOOTH buttons */
    epicMode = EM_RAW;
    SetEpicMode();

    /* possibly turn autoapply back on */
    /* GamSetAutoApply(-1); */  /* -1 means 'back to default setting' */
  }

  SetDirSaveMode(F_COLORS, -1);    /* enable/disable REDUCED COLOR */
}


/***********************************************************/
void Change824Mode(mode)
     int mode;
{
  if (mode == picType) return;   /* same mode, do nothing */

  Set824Menus(mode);

  if (!pic) {  /* done all we wanna do when there's no pic */
    picType = mode;
    return;
  }

  /* should probably actually *do* something involving colors, regenrating
     pic's, drawing an Ximage, etc. */

  if (mode == PIC24) {
    byte *pic24;

    WaitCursor();
    pic24 = Conv8to24(pic, pWIDE, pHIGH, rorg,gorg,borg);
    if (!pic24) FatalError("Ran out of memory in Change824Mode()\n");

    KillOldPics();
    pic = pic24;  picType = PIC24;

    Set824Menus(picType);            /* RAW/DITH/SMOOTH buttons change */
    InstallNewPic();
  }


  else if (mode == PIC8) {
    byte *pic8;

    WaitCursor();
    pic8 = Conv24to8(pic, pWIDE, pHIGH, ncols, rMap,gMap,bMap);
    if (!pic8) FatalError("Ran out of memory in Change824Mode()\n");

    KillOldPics();
    pic = pic8;  picType = PIC8;

    Set824Menus(picType);            /* RAW/DITH/SMOOTH buttons change */
    InstallNewPic();
  }

  /* may have to explicitly redraw image window if not using root */
}


/***********************************************************/
void FreeEpic()
{
  if (egampic && egampic != epic) free(egampic);
  if (epic && epic != cpic) free(epic);
  epic = egampic = NULL;
}


/***********************************************************/
void InvertPic24(pic24, w, h)
     byte *pic24;
     int   w,h;
{
  int i;

  for (i=w*h*3; i; i--, pic24++) *pic24 = 255 - *pic24;
}




/***********************/
#if 0 /* NOTUSED */
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
#endif /* 0 - NOTUSED */



/***********************************************************/
byte *XVGetSubImage(pic, ptype, w,h, sx,sy,sw,sh)
     byte *pic;
     int   ptype, w,h, sx,sy,sw,sh;
{
  /* mallocs and returns the selected subimage (sx,sy,sw,sh) of pic.
     selection is guaranteed to be within pic boundaries.
     NEVER RETURNS NULL */

  byte *rpic, *sp, *dp;
  int   bperpix,x,y;

  /* sanity check: */
  if (sx<0 || sy<0 || sx+sw > w || sy+sh > h || sw<1 || sh<1) {
    fprintf(stderr,"XVGetSubImage:  w,h=%d,%d  sel = %d,%d %dx%d\n",
	    w, h, sx, sy, sw, sh);
    FatalError("XVGetSubImage:  value out of range (shouldn't happen!)");
  }


  bperpix = (ptype==PIC8) ? 1 : 3;
  rpic = (byte *) malloc((size_t) bperpix * sw * sh);
  if (!rpic) FatalError("out of memory in XVGetSubImage");

  for (y=0; y<sh; y++) {
    sp = pic  + ((y+sy)*w + sx) * bperpix;
    dp = rpic + (y * sw) * bperpix;
    for (x=0; x<(sw*bperpix); x++, dp++, sp++) *dp = *sp;
  }

  return rpic;
}




static byte *padPic = (byte *) NULL;
static byte  padmapR[256], padmapG[256], padmapB[256];
static int   padType, padWide, padHigh;

static char *holdcomment = (char *) NULL;
static char *holdfname   = (char *) NULL;

/***********************************/
int DoPad(mode, str, wide, high, opaque, omode)
     int   mode, wide, high, opaque, omode;
     char *str;
{
  /* parses/verifies user-entered string.  If valid, does the thing, and
     installs the new pic and all that...  Returns '0' on failure */

  int   rv;

  if (padPic)      free(padPic);
  if (holdcomment) free(holdcomment);
  if (holdfname)   free(holdcomment);
  padPic = (byte *) NULL;
  holdcomment = holdfname = (char *) NULL;

  rv = 1;

  if ((mode != PAD_LOAD) && (wide == cWIDE && high == cHIGH && opaque==100)) {
    ErrPopUp("Padding to same size as pic while fully opaque has no effect.",
	     "\nI see");
    return 0;
  }

  WaitCursor();

  if      (mode == PAD_SOLID) rv = doPadSolid(str, wide, high, opaque,omode);
  else if (mode == PAD_BGGEN) rv = doPadBggen(str, wide, high, opaque,omode);
  else if (mode == PAD_LOAD)  rv = doPadLoad (str, wide, high, opaque,omode);

  SetCursors(-1);

  if (!rv) return 0;

  if (picComments) {
    holdcomment = (char *) malloc(strlen(picComments) + 1);
    if (holdcomment) strcpy(holdcomment, picComments);
  }

  holdfname = (char *) malloc(strlen(fullfname) + 1);
  if (holdfname) strcpy(holdfname, fullfname);

  return 1;
}


/***********************************/
int LoadPad(pinfo, fname)
     PICINFO *pinfo;
     char    *fname;
{
  /* loads up into XV structures results of last 'pad' command.
     returns 0 on failure, 1 on success */

  int i;

  if (!padPic) return 0;
  pinfo->type = padType;

  for (i=0; i<256; i++) {
    pinfo->r[i] = padmapR[i];
    pinfo->g[i] = padmapG[i];
    pinfo->b[i] = padmapB[i];
  }

  pinfo->pic = padPic;
  pinfo->w   = padWide;
  pinfo->h   = padHigh;
  pinfo->frmType = -1;
  pinfo->colType = -1;

  sprintf(pinfo->fullInfo, "<%s internal>",
	  (pinfo->type == PIC8) ? "8-bit" : "24-bit");

  sprintf(pinfo->shrtInfo, "%dx%d image", padWide, padHigh);

  pinfo->comment = holdcomment;

  if (holdfname) {
    strcpy(fname, holdfname);
    free(holdfname);
    holdfname = (char *) NULL;
  }
  else strcpy(fname, "");

  padPic      = (byte *) NULL;
  holdcomment = (char *) NULL;

  return 1;
}



/*******************************/
static int doPadSolid(str, wide, high, opaque,omode)
     char *str;
     int   wide, high, opaque,omode;
{
  /* returns 0 on error, 1 if successful */

  byte *pic24, *pp;
  int   i, solidRGB, r,g,b,rgb;
  char  errstr[256];

  solidRGB = 0;


  /* PARSE STRING:  'r,g,b'  'r g b'  '0xrrggbb' or <name> */

  if ((sscanf(str, "%d,%d,%d", &r,&g,&b) == 3) &&
      (r>=0 && r<=255 && g>=0 && g<=255 && b>=0 && b<=255)) {
    solidRGB = ((r&0xff)<<16) | ((g&0xff)<<8) | (b&0xff);
  }
  else if ((sscanf(str, "%d %d %d", &r,&g,&b) == 3) &&
	   (r>=0 && r<=255 && g>=0 && g<=255 && b>=0 && b<=255)) {
    solidRGB = ((r&0xff)<<16) | ((g&0xff)<<8) | (b&0xff);
  }
  else if (sscanf(str, "0x%x", &rgb) && rgb>=0 && rgb<=0xffffff) {
    solidRGB = rgb;
  }
  else {   /* assume a colorname */
    XColor ecdef, sdef;
    if (XLookupColor(theDisp, theCmap, str, &ecdef, &sdef)) {
      solidRGB = (((ecdef.red  >>8)&0xff) << 16) |
	         (((ecdef.green>>8)&0xff) <<  8) |
	         (((ecdef.blue >>8)&0xff));
    }
    else {
      sprintf(errstr, "Error:  Color specification '%s' not recognized.",str);
      ErrPopUp(errstr, "\nOk");
      return 0;
    }
  }



  pic24 = (byte *) malloc(wide * high * 3 * sizeof(byte));
  if (!pic24) {
    sprintf(errstr,"Error:  Can't alloc memory for %d x %d image.",
	    wide, high);
    ErrPopUp(errstr, "\n:-(");
    return 0;
  }


  /* fill pic24 with solidRGB */
  for (i=0,pp=pic24; i<wide*high; i++, pp+=3) {
    pp[0] = (solidRGB>>16) & 0xff;
    pp[1] = (solidRGB>>8)  & 0xff;
    pp[2] = (solidRGB)     & 0xff;
  }

  i = doPadPaste(pic24, wide, high, opaque,omode);

  return i;
}



/*******************************/
static int doPadBggen(str, wide, high, opaque,omode)
     char *str;
     int   wide, high, opaque,omode;
{
#ifndef USE_MKSTEMP
  int tmpfd;
#endif
  int i;
  byte *bgpic24;
  char syscmd[512], fname[128], errstr[512];
  PICINFO pinfo;

  /* returns 0 on error, 1 if successful */

  if (xv_strstr(str, "-h") || xv_strstr(str, "-w") || xv_strstr(str,"-g") ||
      xv_strstr(str, ">")) {
    ErrPopUp(
	 "Error:  No redirection or '-h', '-w' or '-g' options are allowed.",
	     "\nOk");
    return 0;
  }


#ifndef VMS
  sprintf(fname, "%s/xvXXXXXX", tmpdir);
#else
  strcpy(fname, "Sys$Disk:[]xvuXXXXXX");
#endif
#ifdef USE_MKSTEMP
  close(mkstemp(fname));
#else
  mktemp(fname);
  tmpfd = open(fname, O_WRONLY|O_CREAT|O_EXCL,S_IRWUSR);
  if (tmpfd < 0) {
    sprintf(errstr, "Error: can't create temporary file %s", fname);
    ErrPopUp(errstr, "\nDoh!");
    return 0;
  }
  close(tmpfd);
#endif

  /* run bggen to generate the background */
  sprintf(syscmd, "bggen -g %dx%d %s > %s", wide, high, str, fname);
  SetISTR(ISTR_INFO, "Running 'bggen %s'...", str);

  i = system(syscmd);
#ifdef VMS
  i = !i;       /* VMS returns 1 on success */
#endif
  if (i) {
    unlink(fname);
    sprintf(errstr, "Error:  Running '%s' failed, for some reason.", syscmd);
    ErrPopUp(errstr, "\nDoh!");
    return 0;
  }


  /* read the file that's been created */
  if (!ReadImageFile1(fname, &pinfo)) {
    unlink(fname);
    return 0;
  }

  unlink(fname);

  if (pinfo.comment) free(pinfo.comment);
  pinfo.comment = (char *) NULL;

  if (pinfo.type == PIC24) bgpic24 = pinfo.pic;
  else {
    bgpic24 = Conv8to24(pinfo.pic,pinfo.w,pinfo.h, pinfo.r,pinfo.g,pinfo.b);
    free(pinfo.pic);  pinfo.pic = (byte *) NULL;
  }

  if (!bgpic24) {
    ErrPopUp("Couldn't generate background image.  malloc() failure?", "\nOk");
    return 0;
  }


  i = doPadPaste(bgpic24, pinfo.w, pinfo.h, opaque,omode);

  return i;
}


/*******************************/
static int doPadLoad(str, wide, high, opaque,omode)
     char *str;
     int   wide, high, opaque,omode;
{
  int i;
  byte *bgpic24;
  char loadName[256];
  PICINFO pinfo;

  /* returns 0 on error, 1 if successful */

  /* use first word as filename to load. */
  if (sscanf(str, "%s", loadName) != 1) {
    ErrPopUp("Error:  The entered string is not valid.", "\nOk");
    return 0;
  }

  if (!ReadImageFile1(loadName, &pinfo)) return 0;

  if (pinfo.comment) free(pinfo.comment);
  pinfo.comment = (char *) NULL;

  if (pinfo.type == PIC24) bgpic24 = pinfo.pic;
  else {
    bgpic24 = Conv8to24(pinfo.pic,pinfo.w,pinfo.h, pinfo.r,pinfo.g,pinfo.b);
    free(pinfo.pic);  pinfo.pic = (byte *) NULL;
  }

  if (!bgpic24) {
    ErrPopUp("Couldn't generate background image.  malloc() failure?", "\nOk");
    return 0;
  }


  i = doPadPaste(bgpic24, pinfo.w, pinfo.h, opaque,omode);

  return i;
}


/*******************************/
static int doPadPaste(pic24, wide, high, opaque,omode)
     byte *pic24;
     int   wide, high, opaque,omode;
{
  /* copies 'pic' onto the given 24-bit background image, converts back to
     8-bit (if necessary), and loads up pad* variables.
     frees pic24 if necessary */

  byte *pp, *p24;
  int py,px, p24y,p24x, sx,sy;
  int fg, bg;
  int rval,gval,bval, r, g, b;


  fg = opaque;
  bg = 100 - opaque;


  /* copy 'pic' centered onto pic24.  */

  sx = (wide - cWIDE) / 2;
  sy = (high - cHIGH) / 2;

  for (py = 0; py<cHIGH; py++) {
    ProgressMeter(0, cHIGH-1, py, "Pad");
    if ((py & 0x1f)==0) WaitCursor();

    p24y = sy + py;
    if (p24y >= 0 && p24y < high) {
      for (px=0; px<cWIDE; px++) {
	p24x = sx + px;
	if (p24x >= 0 && p24x < wide) {
	  p24 = pic24 + (p24y*wide  + p24x)*3;


	  if (picType == PIC24) {                       /* src is PIC24 */
	    pp  = cpic + (py * cWIDE + px)  *3;
	    r = pp[0];  g = pp[1];  b = pp[2];
	  }
	  else {                                        /* src is PIC8 */
	    pp  = cpic + (py*cWIDE + px);
	    r = rMap[*pp];  g = gMap[*pp];  b = bMap[*pp];
	  }

	  if (omode == PAD_ORGB) {
	    rval = (r * fg) / 100 + ((int) p24[0] * bg) / 100;
	    gval = (g * fg) / 100 + ((int) p24[1] * bg) / 100;
	    bval = (b * fg) / 100 + ((int) p24[2] * bg) / 100;
	  }
	  else {       /* one of the HSV modes */
	    double fh,fs,fv,fw, bh,bs,bv,bw, h,s,v;
	    fw = fg / 100.0;  bw = bg / 100.0;
	    rgb2hsv(r,g,b, &fh,&fs,&fv);
	    rgb2hsv((int) p24[0], (int) p24[1], (int) p24[2], &bh,&bs,&bv);

	    h = s = v = 0.0;

	    if (omode == PAD_OINT) {
	      h = fh;
	      s = fs;
	      /* v = (fv * fg) / 100.0 + (bv * bg) / 100.0; */
	      v = (fv * bv * bw) + (fv * fw);
	    }
	    else if (omode == PAD_OSAT) {
	      if (fh<0) fs = 0.0;   /* NOHUE is unsaturated */
	      if (bh<0) bs = 0.0;
	      h = fh;
	      /* s = (fs * fg) / 100.0 + (bs * bg) / 100.0; */
	      s = (fs * bs * bw) + (fs * fw);
	      v = fv;
	    }
	    else if (omode == PAD_OHUE) {   /* the hard one! */
	      int fdeg,bdeg;

	      fdeg = (fh<0) ? -1 : (int) floor(fh + 0.5);
	      bdeg = (bh<0) ? -1 : (int) floor(bh + 0.5);

	      if (fdeg>=0 && bdeg>=0) {           /* both are colors */
		/* convert H,S onto x,y coordinates on the colorwheel for
		   constant V */

		double fx,fy, bx,by, ox,oy;

		if (fg == 100 || bg == 100) {   /* E-Z special case */
		  if (fg==100) { h = fh;  s = fs;  v=fv; }
		  else         { h = bh;  s = fs;  v=fv; }
		}
		else {  /* general case */

		  fh *= (3.14159 / 180.0);    /* -> radians */
		  bh *= (3.14159 / 180.0);

		  fx = fs * cos(fh);  fy = fs * sin(fh);
		  bx = bs * cos(bh);  by = bs * sin(bh);

		  /* compute pt. on line between fx,fy and bx,by */
		  ox = (fx * (fg/100.0)) + (bx * (bg/100.0));
		  oy = (fy * (fg/100.0)) + (by * (bg/100.0));

		  /* convert ox,oy back into hue,sat */
		  s = sqrt((ox * ox) + (oy * oy));
		  if (ox == 0.0) {
		    h = (oy>=0.0) ? 90.0 : 270.0;
		  }
		  else {
		    h = atan(oy / ox);
		    h *= (180.0 / 3.14159);
		    if (ox<0.0) h += 180.0;
		    while (h<0.0) h += 360.0;
		    while (h>=360.0) h -= 360.0;
		  }

		  v = fv;
		}
	      }

	      else if (fdeg<0 && bdeg<0) {        /* both are NOHUE */
		h = -1.0;
		s = fs;
		v = fv;
	      }

	      else if (bdeg<0) {                  /* backgrnd is white */
		h = fh;
		v = fv;
		s = (fs * fg) / 100.0;
	      }

	      else {                              /* foregrnd is white */
		h = bh;
		v = fv;
		s = (bs * bg) / 100.0;
	      }
	    }

	    v = (fv * bv * bw) + (fv * fw);
	    hsv2rgb(h,s,v, &rval,&gval,&bval);
	  }

	  RANGE(rval, 0, 255);  RANGE(gval, 0, 255);  RANGE(bval, 0, 255);
	  *p24++ = rval;  *p24++ = gval;  *p24++ = bval;
	}
      }
    }
  }


  /* build 'padPic' appropriately */
  if (picType == PIC8) {   /* put back to 8-bit */
    padPic = Conv24to8(pic24, wide, high, ncols, padmapR, padmapG, padmapB);
    free(pic24);
    if (!padPic) {
      SetCursors(-1);
      ErrPopUp("Failure occured in 24to8 conversion\n","\nDamn!");
      return 0;
    }
    padType = PIC8;
    padWide = wide;
    padHigh = high;
  }
  else {                    /* PIC24 */
    padPic  = pic24;
    padType = PIC24;
    padWide = wide;
    padHigh = high;
  }

  return 1;
}


/*******************************/
static int ReadImageFile1(name, pinfo)
     char    *name;
     PICINFO *pinfo;
{
  int  i, ftype;
  char uncompname[128], errstr[256], *uncName, *readname;
#ifdef VMS
  char basefname[128];
#endif

  ftype = ReadFileType(name);

  if ((ftype == RFT_COMPRESS) || (ftype == RFT_BZIP2)) {  /* handle .Z,gz,bz2 */
#ifdef VMS
    basefname[0] = '\0';
    strcpy(basefname, name);     /* remove trailing .Z */
    *rindex(basefname, '.') = '\0';
    uncName = basefname;
#else
    uncName = name;
#endif

    if (UncompressFile(uncName, uncompname, ftype)) {
      ftype = ReadFileType(uncompname);
      readname = uncompname;
    }
    else {
      sprintf(errstr, "Error:  Couldn't uncompress file '%s'", name);
      ErrPopUp(errstr, "\nOk");
      return 0;
    }
  }


  if (ftype == RFT_ERROR) {
    sprintf(errstr, "Couldn't open file '%s'\n\n  %s.", name, ERRSTR(errno));
    ErrPopUp(errstr, "\nOk");
    return 0;
  }
  else if (ftype == RFT_UNKNOWN) {
    sprintf(errstr, "Error:  File '%s' not in a recognized format.", name);
    ErrPopUp(errstr, "\nOk");
    return 0;
  }
  else {                         /* try to read it */
    i = ReadPicFile(name, ftype, pinfo, 1);
    KillPageFiles(pinfo->pagebname, pinfo->numpages);

    if (!i || (i && (pinfo->w<=0 || pinfo->h<=0))) {
      if (i) {
	if (pinfo->pic)     free(pinfo->pic);
	if (pinfo->comment) free(pinfo->comment);
      }
      sprintf(errstr, "Couldn't load file '%s'.", name);
      ErrPopUp(errstr, "\nOk");
      return 0;
    }

    /* got it! */
  }

  return 1;
}
