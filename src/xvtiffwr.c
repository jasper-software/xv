/*
 * xvtiffwr.c - write routine for TIFF pictures
 *
 * WriteTIFF(fp,pic,w,h,r,g,b,numcols,style,raw,fname,comment)
 */

#define NEEDSARGS
#include "xv.h"

#ifdef HAVE_TIFF

#include <tiffio.h>    /* has to be after xv.h, as it needs varargs/stdarg */


#define ALLOW_JPEG 0  /* set to '1' to allow 'JPEG' choice in dialog box */


static void setupColormap   PARM((TIFF *, byte *, byte *, byte *));
static int  WriteTIFF       PARM((FILE *, byte *, int, int, int,
				  byte *, byte *, byte *, int, int,
				  char *, int, char *));



/*********************************************/
static void setupColormap(tif, rmap, gmap, bmap)
     TIFF *tif;
     byte *rmap, *gmap, *bmap;
{
  short red[256], green[256], blue[256];
  int i;

  /* convert 8-bit colormap to 16-bit */
  for (i=0; i<256; i++) {
#define	SCALE(x)	((((int)x)*((1L<<16)-1))/255)
    red[i] = SCALE(rmap[i]);
    green[i] = SCALE(gmap[i]);
    blue[i] = SCALE(bmap[i]);
  }
  TIFFSetField(tif, TIFFTAG_COLORMAP, red, green, blue);
}



/*******************************************/
/* Returns '0' if successful. */
static int WriteTIFF(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,
		     fname,comp,comment)
     FILE *fp;
     byte *pic;
     int   ptype,w,h,comp;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
     char *fname, *comment;
{
  TIFF *tif;
  byte *pix;
  int   i,j;
  int   npixels = w*h;

  if (w <= 0 || h <= 0 || npixels/w != h) {
    SetISTR(ISTR_WARNING, "%s: image dimensions too large", fname);
    /* TIFFError(fname, "Image dimensions too large"); */
    return -1;
  }

#ifndef VMS
  tif = TIFFOpen(fname, "w");
#else
  tif = TIFFFdOpen(dup(fileno(fp)), fname, "w");
#endif

  if (!tif) return -1;   /* GRR:  was 0 */

  WaitCursor();

  TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
  TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
  TIFFSetField(tif, TIFFTAG_COMPRESSION, comp);

  if (comment && strlen(comment) > (size_t) 0) {
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, comment);
  }

  if (comp == COMPRESSION_CCITTFAX3)
      TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS,
	  GROUP3OPT_2DENCODING+GROUP3OPT_FILLBITS);

  if (comp == COMPRESSION_LZW)
      TIFFSetField(tif, TIFFTAG_PREDICTOR, 2);

  TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
  TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, h);

  TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (int)2);
  TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float) 72.0);
  TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float) 72.0);


  /* write the image data */

  if (ptype == PIC24) {  /* only have to deal with FULLCOLOR or GREYSCALE */
    if (colorstyle == F_FULLCOLOR) {
      int count = 3*npixels;

      if (count/3 != npixels) {  /* already know w, h, npixels > 0 */
        /* SetISTR(ISTR_WARNING, "%s: image dimensions too large", fname); */
        TIFFError(fname, "Image dimensions too large");
        return -1;
      }

      TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
      TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   8);
      TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_RGB);

      TIFFWriteEncodedStrip(tif, 0, pic, count);
    }

    else {  /* colorstyle == F_GREYSCALE */
      byte *tpic, *tp, *sp;

      TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
      TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   8);
      TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,    PHOTOMETRIC_MINISBLACK);

      tpic = (byte *) malloc((size_t) npixels);
      if (!tpic) FatalError("unable to malloc in WriteTIFF()");

      for (i=0, tp=tpic, sp=pic; i<npixels; i++, sp+=3)
	*tp++ = MONO(sp[0],sp[1],sp[2]);

      TIFFWriteEncodedStrip(tif, 0, tpic, npixels);

      free(tpic);
    }
  }

  else {  /* PIC8 */
    if (colorstyle == F_FULLCOLOR) {                  /* 8bit Palette RGB */
      TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
      TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
      setupColormap(tif, rmap, gmap, bmap);
      TIFFWriteEncodedStrip(tif, 0, pic, npixels);
    }

    else if (colorstyle == F_GREYSCALE) {             /* 8-bit greyscale */
      byte rgb[256];
      byte *tpic = (byte *) malloc((size_t) npixels);
      byte *tp = tpic;
      TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
      TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      for (i=0; i<numcols; i++) rgb[i] = MONO(rmap[i],gmap[i],bmap[i]);
      for (i=0, pix=pic; i<npixels; i++,pix++) {
	if ((i&0x7fff)==0) WaitCursor();
	*tp++ = rgb[*pix];
      }
      TIFFWriteEncodedStrip(tif, 0, tpic, npixels);
      free(tpic);
    }

    else if (colorstyle == F_BWDITHER) {             /* 1-bit B/W stipple */
      int bit,k,flipbw;
      byte *tpic, *tp;
      tsize_t stripsize;  /* signed */

      flipbw = (MONO(rmap[0],gmap[0],bmap[0]) > MONO(rmap[1],gmap[1],bmap[1]));
      TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 1);
      TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      stripsize = TIFFStripSize(tif);
      if (stripsize <= 0) {
        TIFFError(fname, "Image dimensions too large");
        return -1;
      }
      tpic = (byte *) malloc((size_t) stripsize);
      if (tpic == 0) {
        TIFFError(fname, "No space for strip buffer");
        return -1;
      }
      tp = tpic;
      for (i=0, pix=pic; i<h; i++) {
	if ((i&15)==0) WaitCursor();
	for (j=0, bit=0, k=0; j<w; j++, pix++) {
	  k = (k << 1) | *pix;
	  bit++;
	  if (bit==8) {
	    if (flipbw) k = ~k;
	    *tp++ = (byte) (k&0xff);
	    bit = k = 0;
	  }
	} /* j */
	if (bit) {
	  k = k << (8-bit);
	  if (flipbw) k = ~k;
	  *tp++ = (byte) (k & 0xff);
	}
      }
      TIFFWriteEncodedStrip(tif, 0, tpic, stripsize);
      free(tpic);
    }
  }


  TIFFClose(tif);

  return 0;
}




/**** Stuff for TIFFDialog box ****/

#define TWIDE 280
#define THIGH 160
#define T_NBUTTS 2
#define T_BOK    0
#define T_BCANC  1
#define BUTTH    24

static void drawTD    PARM((int, int, int, int));
static void clickTD   PARM((int, int));
static void doCmd     PARM((int));
static void writeTIFF PARM((void));


/* local variables */
static char *filename;
static int   colorType;
static BUTT  tbut[T_NBUTTS];
static RBUTT *compRB;



/***************************************************/
void CreateTIFFW()
{
  int	     y;

  tiffW = CreateWindow("xv tiff", "XVtiff", NULL,
		       TWIDE, THIGH, infofg, infobg, 0);
  if (!tiffW) FatalError("can't create tiff window!");

  XSelectInput(theDisp, tiffW, ExposureMask | ButtonPressMask | KeyPressMask);

  BTCreate(&tbut[T_BOK], tiffW, TWIDE-140-1, THIGH-10-BUTTH-1, 60, BUTTH,
	   "Ok", infofg, infobg, hicol, locol);

  BTCreate(&tbut[T_BCANC], tiffW, TWIDE-70-1, THIGH-10-BUTTH-1, 60, BUTTH,
	   "Cancel", infofg, infobg, hicol, locol);

  y = 55;
  compRB = RBCreate(NULL, tiffW, 36, y,   "None", infofg, infobg,hicol,locol);
  RBCreate(compRB, tiffW, 36, y+18,       "LZW", infofg, infobg,hicol,locol);
  RBCreate(compRB, tiffW, 36, y+36,       "PackBits", infofg, infobg,
           hicol, locol);
  RBCreate(compRB, tiffW, TWIDE/2, y,     "CCITT Group3", infofg, infobg,
	   hicol, locol);
  RBCreate(compRB, tiffW, TWIDE/2, y+18,  "CCITT Group4", infofg, infobg,
	   hicol, locol);
  if (ALLOW_JPEG) {
    RBCreate(compRB, tiffW, TWIDE/2, y+36,  "JPEG", infofg, infobg,
	     hicol, locol);
  }

  XMapSubwindows(theDisp, tiffW);
}


/***************************************************/
void TIFFDialog(vis)
int vis;
{
  if (vis) {
    CenterMapWindow(tiffW, tbut[T_BOK].x + (int) tbut[T_BOK].w/2,
		    tbut[T_BOK].y + (int) tbut[T_BOK].h/2, TWIDE, THIGH);
  }
  else     XUnmapWindow(theDisp, tiffW);
  tiffUp = vis;
}


/***************************************************/
int TIFFCheckEvent(xev)
XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!tiffUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    if (e->window == tiffW)       drawTD(x, y, w, h);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == tiffW)     clickTD(x,y);
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;  XComposeStatus status;
    int stlen;

    stlen = XLookupString(e,buf,128,&ks,&status);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    if (e->window == tiffW) {
      if (stlen) {
	if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
	  FakeButtonPress(&tbut[T_BOK]);
	}
	else if (buf[0] == '\033') {            /* ESC */
	  FakeButtonPress(&tbut[T_BCANC]);
	}
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
void TIFFSaveParams(fname, col)
char *fname;
int col;
{
  int cur;
  cur = RBWhich(compRB);

  filename = fname;
  colorType = col;
  if (colorType == F_BWDITHER) {
    RBSetActive(compRB,3,1);
    RBSetActive(compRB,4,1);
    if (ALLOW_JPEG) {
      RBSetActive(compRB,5,0);
      if (cur == 5) RBSelect(compRB,3);
    }
  }
  else {
    RBSetActive(compRB,3,0);
    RBSetActive(compRB,4,0);
    if (ALLOW_JPEG) RBSetActive(compRB,5,1);
    if (cur == 3 || cur == 4) RBSelect(compRB,0);
  }
}


/***************************************************/
static void drawTD(x,y,w,h)
int x,y,w,h;
{
  const char *title  = "Save TIFF file...";
  int  i;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<T_NBUTTS; i++) BTRedraw(&tbut[i]);

  ULineString(tiffW, compRB->x-16, compRB->y-3-DESCENT, "Compression");
  RBRedraw(compRB, -1);

  DrawString(tiffW, 20, 29, title);

  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void clickTD(x,y)
int x,y;
{
  int i;
  BUTT *bp;

  /* check BUTTs */

  /* check the RBUTTS first, since they don't DO anything */
  if ( (i=RBClick(compRB, x,y)) >= 0) {
    (void) RBTrack(compRB, i);
    return;
  }


  for (i=0; i<T_NBUTTS; i++) {
    bp = &tbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<T_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }
}



/***************************************************/
static void doCmd(cmd)
int cmd;
{
  switch (cmd) {
  case T_BOK: {
    char *fullname;

    writeTIFF();
    TIFFDialog(0);

    fullname = GetDirFullName();
    if (!ISPIPE(fullname[0])) {
      XVCreatedFile(fullname);
      StickInCtrlList(0);
    }
  }
    break;

  case T_BCANC:	TIFFDialog(0);   break;

  default:	break;
  }
}


/*******************************************/
static void writeTIFF()
{
  FILE *fp;
  int   w, h, nc, rv, comp, ptype, pfree;
  byte *inpix, *rmap, *gmap, *bmap;


  /* see if we can open the output file before proceeding */
  fp = OpenOutFile(filename);
  if (!fp) return;

  WaitCursor();
  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  if (colorType == F_REDUCED) colorType = F_FULLCOLOR;

  switch (RBWhich(compRB)) {
  case 0: comp = COMPRESSION_NONE;      break;
  case 1: comp = COMPRESSION_LZW;       break;
  case 2: comp = COMPRESSION_PACKBITS;  break;
  case 3: comp = COMPRESSION_CCITTFAX3; break;
  case 4: comp = COMPRESSION_CCITTFAX4; break;
  case 5: comp = COMPRESSION_JPEG;      break;
  default: comp = COMPRESSION_NONE;     break;
  }

  rv = WriteTIFF(fp, inpix, ptype, w, h,
		 rmap, gmap, bmap, nc, colorType, filename, comp, picComments);

  SetCursors(-1);

  if (CloseOutFile(fp, filename, rv) == 0) DirBox(0);

  if (pfree) free(inpix);
}


#endif /* HAVE_TIFF */
