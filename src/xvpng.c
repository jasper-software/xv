/*
 * xvpng.c - load and write routines for 'PNG' format pictures
 *
 * callable functions
 *
 *    CreatePNGW()
 *    PNGDialog(vis)
 *    PNGCheckEvent(xev)
 *    PNGSaveParams(fname, col)
 *    LoadPNG(fname, pinfo)
 *    VersionInfoPNG()
 */

/*#include "copyright.h"*/

/* (c) 1995 by Alexander Lehmann <lehmann@mathematik.th-darmstadt.de>
 *   This file is a suplement to xv and is supplied under the same copying
 *   conditions (except the shareware part).
 *   The copyright will be passed on to JB at some future point if he
 *   so desires.
 *
 * Modified by Andreas Dilger <adilger@enel.ucalgary.ca> to fix
 *   error handling for bad PNGs, add dialogs for interlacing and
 *   compression selection, and upgrade to libpng-0.89.
 *
 * Modified by Greg Roelofs, TenThumbs and others to fix bugs and add
 *   features.  See README.jumbo for details.
 */

#include "xv.h"

#ifdef HAVE_PNG

#include "png.h"

/*** Stuff for PNG Dialog box ***/
#define PWIDE 318
#define PHIGH 215

#define DISPLAY_GAMMA 2.20  /* default display gamma */
#define COMPRESSION   6     /* default zlib compression level, not max
                               (Z_BEST_COMPRESSION) */

#define HAVE_tRNS  (info_ptr->valid & PNG_INFO_tRNS)

#define DWIDE    86
#define DHIGH    104
#define PFX      (PWIDE-93)
#define PFY      44
#define PFH      20

#define P_BOK    0
#define P_BCANC  1
#define P_NBUTTS 2

#define BUTTH    24

#define LF       10   /* a.k.a. '\n' on ASCII machines */
#define CR       13   /* a.k.a. '\r' on ASCII machines */

/*** local functions ***/
static    void drawPD         PARM((int, int, int, int));
static    void clickPD        PARM((int, int));
static    void doCmd          PARM((int));
static    void writePNG       PARM((void));
static    int  WritePNG       PARM((FILE *, byte *, int, int, int,
                                    byte *, byte *, byte *, int));

static    void png_xv_error   PARM((png_structp png_ptr,
                                    png_const_charp message));
static    void png_xv_warning PARM((png_structp png_ptr,
                                    png_const_charp message));

/*** local variables ***/
static char *filename;
static const char *fbasename;
static int   colorType;
static int   read_anything;
static double Display_Gamma = DISPLAY_GAMMA;

static DIAL  cDial, gDial;
static BUTT  pbut[P_NBUTTS];
static CBUTT interCB;
static CBUTT FdefCB, FnoneCB, FsubCB, FupCB, FavgCB, FPaethCB;


#ifdef PNG_NO_STDIO
/* NOTE:  Some sites configure their version of the PNG Library without
 *        Standard I/O Library interfaces in order to avoid unnecessary inter-
 * library dependencies at link time for applications that don't need Standard
 * I/O.  If your site is one of these, the following skeletal stubs, copied
 * from libpng code, should be enough for this module.  --Scott B. Marovich,
 * Hewlett-Packard Laboratories, March 2001.
 */
static void
png_default_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{

   /* fread() returns 0 on error, so it is OK to store this in a png_size_t
    * instead of an int, which is what fread() actually returns.
    */
   if (fread(data,1,length,(FILE *)png_ptr->io_ptr) != length)
     png_error(png_ptr, "Read Error");
}

static void
png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   if (fwrite(data, 1, length, (FILE *)png_ptr->io_ptr) != length)
     png_error(png_ptr, "Write Error");
}
#endif /* PNG_NO_STDIO */


/**************************************************************************/
/* PNG SAVE DIALOG ROUTINES ***********************************************/
/**************************************************************************/


/*******************************************/
void CreatePNGW()
{
  pngW = CreateWindow("xv png", "XVPNG", NULL,
                      PWIDE, PHIGH, infofg, infobg, 0);
  if (!pngW) FatalError("can't create PNG window!");

  XSelectInput(theDisp, pngW, ExposureMask | ButtonPressMask | KeyPressMask);

  DCreate(&cDial, pngW,  12, 25, DWIDE, DHIGH, (double)Z_NO_COMPRESSION,
          (double)Z_BEST_COMPRESSION, COMPRESSION, 1.0, 3.0,
          infofg, infobg, hicol, locol, "Compression", NULL);

  DCreate(&gDial, pngW, DWIDE+27, 25, DWIDE, DHIGH, 1.0, 3.5,DISPLAY_GAMMA,0.01,0.2,
          infofg, infobg, hicol, locol, "Disp. Gamma", NULL);

  CBCreate(&interCB, pngW,  DWIDE+30, DHIGH+3*LINEHIGH+2, "interlace",
           infofg, infobg, hicol, locol);

  CBCreate(&FdefCB,   pngW, PFX, PFY, "Default",
           infofg, infobg, hicol, locol);
  FdefCB.val = 1;

  CBCreate(&FnoneCB,  pngW, PFX, FdefCB.y + PFH + 4, "none",
           infofg, infobg, hicol, locol);
  CBCreate(&FsubCB,   pngW, PFX, FnoneCB.y + PFH, "sub",
           infofg, infobg, hicol, locol);
  CBCreate(&FupCB,    pngW, PFX, FsubCB.y  + PFH, "up",
           infofg, infobg, hicol, locol);
  CBCreate(&FavgCB,   pngW, PFX, FupCB.y   + PFH, "average",
           infofg, infobg, hicol, locol);
  CBCreate(&FPaethCB, pngW, PFX, FavgCB.y  + PFH, "Paeth",
           infofg, infobg, hicol, locol);

  FnoneCB.val = FsubCB.val = FupCB.val = FavgCB.val = FPaethCB.val = 1;
  CBSetActive(&FnoneCB, !FdefCB.val);
  CBSetActive(&FsubCB, !FdefCB.val);
  CBSetActive(&FupCB, !FdefCB.val);
  CBSetActive(&FavgCB, !FdefCB.val);
  CBSetActive(&FPaethCB, !FdefCB.val);

  BTCreate(&pbut[P_BOK], pngW, PWIDE-180-1, PHIGH-10-BUTTH-1, 80, BUTTH,
          "Ok", infofg, infobg, hicol, locol);
  BTCreate(&pbut[P_BCANC], pngW, PWIDE-90-1, PHIGH-10-BUTTH-1, 80, BUTTH,
          "Cancel", infofg, infobg, hicol, locol);

  XMapSubwindows(theDisp, pngW);
}


/*******************************************/
void PNGDialog(vis)
     int vis;
{
  if (vis) {
    CenterMapWindow(pngW, pbut[P_BOK].x + (int) pbut[P_BOK].w/2,
                          pbut[P_BOK].y + (int) pbut[P_BOK].h/2,
                    PWIDE, PHIGH);
  }
  else XUnmapWindow(theDisp, pngW);
  pngUp = vis;
}


/*******************************************/
int PNGCheckEvent(xev)
     XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!pngUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x; y = e->y; w = e->width; h = e->height;

    /* throw away excess expose events for 'dumb' windows */
    if (e->count > 0 && (e->window == cDial.win)) {}

    else if (e->window == pngW)        drawPD(x, y, w, h);
    else if (e->window == cDial.win)   DRedraw(&cDial);
    else if (e->window == gDial.win)   DRedraw(&gDial);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == pngW)       clickPD(x,y);
      else if (e->window == cDial.win)  DTrack(&cDial,x,y);
      else if (e->window == gDial.win)  DTrack(&gDial,x,y);
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */

  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;
    int stlen;

    stlen = XLookupString(e,buf,128,&ks,(XComposeStatus *) NULL);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    if (e->window == pngW) {
      if (stlen) {
        if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
          FakeButtonPress(&pbut[P_BOK]);
        }
        else if (buf[0] == '\033') {            /* ESC */
          FakeButtonPress(&pbut[P_BCANC]);
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


/*******************************************/
void PNGSaveParams(fname, col)
     char *fname;
     int col;
{
  filename = fname;
  colorType = col;
}


/*******************************************/
static void drawPD(x, y, w, h)
     int x, y, w, h;
{
  const char *title   = "Save PNG file...";

  char ctitle1[20];
  const char *ctitle2 = "Useful range";
  const char *ctitle3 = "is 2 - 7.";
  const char *ctitle4 = "Uncompressed = 0";

  const char *ftitle  = "Row Filters:";

  char gtitle[20];

  int i;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<P_NBUTTS; i++) BTRedraw(&pbut[i]);

  DrawString(pngW,       15,  6+ASCENT,                          title);

  sprintf(ctitle1, "Default = %d", COMPRESSION);
  DrawString(pngW,       18,  6+DHIGH+cDial.y+ASCENT,            ctitle1);
  DrawString(pngW,       17,  6+DHIGH+cDial.y+ASCENT+LINEHIGH,   ctitle2);
  DrawString(pngW,       17,  6+DHIGH+cDial.y+ASCENT+2*LINEHIGH, ctitle3);
  DrawString(pngW,       17,  6+DHIGH+cDial.y+ASCENT+3*LINEHIGH, ctitle4);

  sprintf(gtitle, "Default = %g", DISPLAY_GAMMA);
  DrawString(pngW, DWIDE+30,  6+DHIGH+gDial.y+ASCENT,            gtitle);

  ULineString(pngW, FdefCB.x, FdefCB.y-3-DESCENT, ftitle);
  XDrawRectangle(theDisp, pngW, theGC, FdefCB.x-11, FdefCB.y-LINEHIGH-3,
                                       93, 8*LINEHIGH+15);
  CBRedraw(&FdefCB);
  XDrawLine(theDisp, pngW, theGC, FdefCB.x-11, FdefCB.y+LINEHIGH+4,
                                  FdefCB.x+82, FdefCB.y+LINEHIGH+4);

  CBRedraw(&FnoneCB);
  CBRedraw(&FupCB);
  CBRedraw(&FsubCB);
  CBRedraw(&FavgCB);
  CBRedraw(&FPaethCB);

  CBRedraw(&interCB);

  XSetClipMask(theDisp, theGC, None);
}


/*******************************************/
static void clickPD(x,y)
     int x,y;
{
  int i;
  BUTT *bp;

  /* check BUTTs */

  for (i=0; i<P_NBUTTS; i++) {
    bp = &pbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<P_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }

  /* check CBUTTs */

  else if (CBClick(&FdefCB,x,y)) {
    int oldval = FdefCB.val;

    CBTrack(&FdefCB);

    if (oldval != FdefCB.val)
    {
      CBSetActive(&FnoneCB, !FdefCB.val);
      CBSetActive(&FsubCB, !FdefCB.val);
      CBSetActive(&FupCB, !FdefCB.val);
      CBSetActive(&FavgCB, !FdefCB.val);
      CBSetActive(&FPaethCB, !FdefCB.val);

      CBRedraw(&FnoneCB);
      CBRedraw(&FupCB);
      CBRedraw(&FsubCB);
      CBRedraw(&FavgCB);
      CBRedraw(&FPaethCB);
    }
  }
  else if (CBClick(&FnoneCB,x,y))  CBTrack(&FnoneCB);
  else if (CBClick(&FsubCB,x,y))   CBTrack(&FsubCB);
  else if (CBClick(&FupCB,x,y))    CBTrack(&FupCB);
  else if (CBClick(&FavgCB,x,y))   CBTrack(&FavgCB);
  else if (CBClick(&FPaethCB,x,y)) CBTrack(&FPaethCB);
  else if (CBClick(&interCB,x,y))  CBTrack(&interCB);
}


/*******************************************/
static void doCmd(cmd)
     int cmd;
{
  switch (cmd) {
    case P_BOK:
      {
        char *fullname;

        writePNG();
        PNGDialog(0);

        fullname = GetDirFullName();
        if (!ISPIPE(fullname[0])) {
          XVCreatedFile(fullname);
          StickInCtrlList(0);
        }
      }
      break;

    case P_BCANC:
      PNGDialog(0);
      break;

    default:
      break;
  }
}


/*******************************************/
static void writePNG()
{
  FILE       *fp;
  int         w, h, nc, rv, ptype, pfree;
  byte       *inpix, *rmap, *gmap, *bmap;

  fp = OpenOutFile(filename);
  if (!fp) return;

  fbasename = BaseName(filename);

  WaitCursor();
  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  rv = WritePNG(fp, inpix, ptype, w, h, rmap, gmap, bmap, nc);

  SetCursors(-1);

  if (CloseOutFile(fp, filename, rv) == 0) DirBox(0);

  if (pfree) free(inpix);
}


/*******************************************/
int WritePNG(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols)
     FILE *fp;
     byte *pic;
     int   ptype, w, h;
     byte *rmap, *gmap, *bmap;
     int   numcols;
     /* FIXME?  what's diff between picComments and WriteGIF's comment arg? */
{
  png_struct *png_ptr;
  png_info   *info_ptr;
  png_color   palette[256];
  png_textp   text;
  byte        r1[256], g1[256], b1[256];  /* storage for deduped palette */
  byte        pc2nc[256];  /* for duplicated-color remapping (1st level) */
  byte        remap[256];  /* for bw/grayscale remapping (2nd level) */
  int         i, j, numuniqcols=0, filter, linesize, pass;
  byte       *p, *png_line;
  char        software[256];
  char       *savecmnt;

  if ((png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
       png_xv_error, png_xv_warning)) == NULL) {
    sprintf(software, "png_create_write_struct() failure in WritePNG");
    FatalError(software);
  }

  if ((info_ptr = png_create_info_struct(png_ptr)) == NULL)
  {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    sprintf(software, "png_create_info_struct() failure in WritePNG");
    FatalError(software);
  }

  if (setjmp(png_ptr->jmpbuf)) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
  }

#ifdef PNG_NO_STDIO
  png_set_write_fn(png_ptr, fp, png_default_write_data, NULL);
  png_set_error_fn(png_ptr, NULL, png_xv_error, png_xv_warning);
#else
  png_init_io(png_ptr, fp);
#endif

  png_set_compression_level(png_ptr, (int)cDial.val);

  /* Don't bother filtering if we aren't compressing the image */
  if (FdefCB.val)
  {
    if ((int)cDial.val == 0)
      png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
  }
  else
  {
    filter  = FnoneCB.val  ? PNG_FILTER_NONE  : 0;
    filter |= FsubCB.val   ? PNG_FILTER_SUB   : 0;
    filter |= FupCB.val    ? PNG_FILTER_UP    : 0;
    filter |= FavgCB.val   ? PNG_FILTER_AVG   : 0;
    filter |= FPaethCB.val ? PNG_FILTER_PAETH : 0;

    png_set_filter(png_ptr, 0, filter);
  }

  info_ptr->width = w;
  info_ptr->height = h;
  if (w <= 0 || h <= 0) {
    SetISTR(ISTR_WARNING, "%s:  image dimensions out of range (%dx%d)",
      fbasename, w, h);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
  }

  info_ptr->interlace_type = interCB.val ? 1 : 0;

  linesize = 0;   /* quiet a compiler warning */


  /* GRR 20070331:  remap palette to eliminate duplicated colors (as in
   *   xvgifwr.c) */
  if (ptype == PIC8) {
    for (i=0; i<256; ++i) {
      pc2nc[i] = r1[i] = g1[i] = b1[i] = 0;
    }

    /* compute number of unique colors */
    numuniqcols = 0;

    for (i=0; i<numcols; ++i) {
      /* see if color #i is already used */
      for (j=0; j<i; ++j) {
        if (rmap[i] == rmap[j]  &&  gmap[i] == gmap[j]  &&  bmap[i] == bmap[j])
          break;
      }

      if (j==i) {  /* wasn't found */
        pc2nc[i] = numuniqcols;
        r1[numuniqcols] = rmap[i];  /* i.e., r1[pc2nc[i]] == rmap[i] */
        g1[numuniqcols] = gmap[i];
        b1[numuniqcols] = bmap[i];
        ++numuniqcols;
      }
      else pc2nc[i] = pc2nc[j];
    }
  }


  /* Appendix G.2 of user manual claims colorType will never be F_REDUCED... */
  if (colorType == F_FULLCOLOR || colorType == F_REDUCED) {
    if (ptype == PIC24) {
      linesize = 3*w;
      if (linesize/3 < w) {
        SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
          fbasename, w, h);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return -1;
      }
      info_ptr->color_type = PNG_COLOR_TYPE_RGB;
      info_ptr->bit_depth = 8;
    } else /* ptype == PIC8 */ {
      linesize = w;
      info_ptr->color_type = PNG_COLOR_TYPE_PALETTE;
      if (numuniqcols <= 2)
        info_ptr->bit_depth = 1;
      else
      if (numuniqcols <= 4)
        info_ptr->bit_depth = 2;
      else
      if (numuniqcols <= 16)
        info_ptr->bit_depth = 4;
      else
        info_ptr->bit_depth = 8;

      for (i = 0; i < numuniqcols; i++) {
        palette[i].red   = r1[i];
        palette[i].green = g1[i];
        palette[i].blue  = b1[i];
      }
      info_ptr->num_palette = numuniqcols;
      info_ptr->palette = palette;
      info_ptr->valid |= PNG_INFO_PLTE;
    }
  }

  else if (colorType == F_GREYSCALE || colorType == F_BWDITHER) {
    info_ptr->color_type = PNG_COLOR_TYPE_GRAY;
    if (colorType == F_BWDITHER) {
      /* shouldn't happen */
      if (ptype == PIC24) FatalError("PIC24 and B/W Stipple in WritePNG()");

      info_ptr->bit_depth = 1;
      if (MONO(r1[0], g1[0], b1[0]) > MONO(r1[1], g1[1], b1[1])) {
        remap[0] = 1;
        remap[1] = 0;
      }
      else {
        remap[0] = 0;
        remap[1] = 1;
      }
      linesize = w;
    }
    else /* F_GREYSCALE */ {
      if (ptype == PIC24) {
        linesize = 3*w;
        if (linesize/3 < w) {
          SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
            fbasename, w, h);
          png_destroy_write_struct(&png_ptr, &info_ptr);
          return -1;
        }
        info_ptr->bit_depth = 8;
      }
      else /* ptype == PIC8 */ {
        int low_precision;

        linesize = w;

        /* NOTE:  currently remap[] is the _secondary_ remapping of "palette"
         *   colors; its values are the final color/grayscale values, and,
         *   like r1/g1/b1[], it is _indexed_ by pc2nc[] (which is why its
         *   values come from r1/g1/b1[] and not from rmap/gmap/bmap[]).
         *
         * FIXME (probably):  MONO() will create new duplicates; ideally should
         *   do extra round of dup-detection (and preferably collapse all
         *   remapping levels into single LUT).  This stuff is a tad confusing.
         */
        for (i = 0; i < numuniqcols; i++)
          remap[i] = MONO(r1[i], g1[i], b1[i]);

        for (; i < 256; i++)
          remap[i]=0;  /* shouldn't be necessary, but... */

        info_ptr->bit_depth = 8;

        /* Note that this fails most of the time because of gamma */
           /* (and that would be a bug:  GRR FIXME) */
        /* try to adjust to 4-bit precision grayscale */

        low_precision=1;

        for (i = 0; i < numuniqcols; i++) {
          if ((remap[i] & 0x0f) * 0x11 != remap[i]) {
            low_precision = 0;
            break;
          }
        }

        if (low_precision) {
          for (i = 0; i < numuniqcols; i++) {
            remap[i] &= 0xf;
          }
          info_ptr->bit_depth = 4;

          /* try to adjust to 2-bit precision grayscale */

          for (i = 0; i < numuniqcols; i++) {
            if ((remap[i] & 0x03) * 0x05 != remap[i]) {
              low_precision = 0;
              break;
            }
          }
        }

        if (low_precision) {
          for (i = 0; i < numuniqcols; i++) {
            remap[i] &= 3;
          }
          info_ptr->bit_depth = 2;

          /* try to adjust to 1-bit precision grayscale */

          for (i = 0; i < numuniqcols; i++) {
            if ((remap[i] & 0x01) * 0x03 != remap[i]) {
              low_precision = 0;
              break;
            }
          }
        }

        if (low_precision) {
          for (i = 0; i < numuniqcols; i++) {
            remap[i] &= 1;
          }
          info_ptr->bit_depth = 1;
        }
      }
    }
  }

  else
    png_error(png_ptr, "Unknown colorstyle in WritePNG");

  if ((text = (png_textp)malloc(sizeof(png_text)))) {
    sprintf(software, "XV %s", REVDATE);

    text->compression = -1;
    text->key = "Software";
    text->text = software;
    text->text_length = strlen(text->text);

    info_ptr->max_text = 1;
    info_ptr->num_text = 1;
    info_ptr->text = text;
  }

  Display_Gamma = gDial.val;  /* Save the current gamma for loading */

// GRR FIXME:  add .Xdefaults option to omit writing gamma (size, cumulative errors when editing)--alternatively, modify save box to include "omit" checkbox
  info_ptr->gamma = 1.0/gDial.val;
  info_ptr->valid |= PNG_INFO_gAMA;

  png_write_info(png_ptr, info_ptr);

  if (info_ptr->bit_depth < 8)
    png_set_packing(png_ptr);

  pass=png_set_interlace_handling(png_ptr);

  if ((png_line = malloc(linesize)) == NULL)
    png_error(png_ptr, "cannot allocate temp image line");
    /* FIXME:  should be FatalError() */

  for (i = 0; i < pass; ++i) {
    int j;
    p = pic;
    for (j = 0; j < h; ++j) {
      if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY) {
        int k;
        for (k = 0; k < w; ++k)
          png_line[k] = ptype==PIC24 ? MONO(p[k*3], p[k*3+1], p[k*3+2]) :
                                       remap[pc2nc[p[k]]];
        png_write_row(png_ptr, png_line);
      } else if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
        int k;
        for (k = 0; k < w; ++k)
          png_line[k] = pc2nc[p[k]];
        png_write_row(png_ptr, png_line);
      } else {  /* PNG_COLOR_TYPE_RGB */
        png_write_row(png_ptr, p);
      }
      if ((j & 0x1f) == 0) WaitCursor();
      p += linesize;
    }
  }

  free(png_line);

  savecmnt = NULL;   /* quiet a compiler warning */

  if (text) {
    if (picComments && strlen(picComments) &&
        (savecmnt = (char *)malloc((strlen(picComments) + 1)*sizeof(char)))) {
      png_textp tp;
      char *comment, *key;

      strcpy(savecmnt, picComments);
      key = savecmnt;
      tp = text;
      info_ptr->num_text = 0;

      comment = strchr(key, ':');

      do  {
        /* Allocate a larger structure for comments if necessary */
        if (info_ptr->num_text >= info_ptr->max_text)
        {
          if ((tp =
              realloc(text, (info_ptr->num_text + 2)*sizeof(png_text))) == NULL)
          {
            break;
          }
          else
          {
            text = tp;
            tp = &text[info_ptr->num_text];
            info_ptr->max_text += 2;
          }
        }

        /* See if it looks like a PNG keyword from LoadPNG */
        /* GRR: should test for strictly < 80, right? (key = 1-79 chars only) */
        if (comment && comment[1] == ':' && comment - key <= 80) {
          *(comment++) = '\0';
          *(comment++) = '\0';

          /* If the comment is the 'Software' chunk XV writes, we remove it,
             since we have already stored one */
          if (strcmp(key, "Software") == 0 && strncmp(comment, "XV", 2) == 0) {
            key = strchr(comment, '\n');
            if (key)
              key++; /* skip \n */
            comment = strchr(key, ':');
          }
          /* We have another keyword and/or comment to write out */
          else {
            tp->key = key;
            tp->text = comment;

            /* We have to find the end of this comment, and the next keyword
               if there is one */
            for (; NULL != (key = comment = strchr(comment, ':')); comment++)
              if (key[1] == ':')
                break;

            /* It looks like another keyword, go backward to the beginning */
            if (key) {
              while (key > tp->text && *key != '\n')
                key--;

              if (key > tp->text && comment - key <= 80) {
                *key = '\0';
                key++;
              }
            }

            tp->text_length = strlen(tp->text);

            /* We don't have another keyword, so remove the last newline */
            if (!key && tp->text[tp->text_length - 1] == '\n')
            {
              tp->text[tp->text_length] = '\0';
              tp->text_length--;
            }

            tp->compression = tp->text_length > 640 ? 0 : -1;
            info_ptr->num_text++;
            tp++;
          }
        }
        /* Just a generic comment:  make sure line-endings are valid for PNG */
        else {
          char *p=key, *q=key;     /* only deleting chars, not adding any */

          while (*p) {
            if (*p == CR) {        /* lone CR or CR/LF:  EOL either way */
              *q++ = LF;           /* LF is the only allowed PNG line-ending */
              if (p[1] == LF)      /* get rid of any original LF */
                ++p;
            } else if (*p == LF)   /* lone LF */
              *q++ = LF;
            else
              *q++ = *p;
            ++p;
          }
          *q = '\0';               /* unnecessary...but what the heck */
          tp->key = "Comment";
          tp->text = key;
          tp->text_length = q - key;
          tp->compression = tp->text_length > 750 ? 0 : -1;
          info_ptr->num_text++;
          key = NULL;
        }
      } while (key && *key);
    }
    else {
      info_ptr->num_text = 0;
    }
  }
  info_ptr->text = text;

  png_convert_from_time_t(&(info_ptr->mod_time), time(NULL));
  info_ptr->valid |= PNG_INFO_tIME;

  png_write_end(png_ptr, info_ptr);
  fflush(fp);   /* just in case we core-dump before finishing... */

  if (text) {
    free(text);
    /* must do this or png_destroy_write_struct() 0.97+ will free text again: */
    info_ptr->text = (png_textp)NULL;
    if (savecmnt)
    {
      free(savecmnt);
      savecmnt = (char *)NULL;
    }
  }

  png_destroy_write_struct(&png_ptr, &info_ptr);

  return 0;
}


/*******************************************/
int LoadPNG(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  FILE  *fp;
  png_struct *png_ptr;
  png_info *info_ptr;
  png_color_16 my_background;
  int i,j;
  int linesize, bufsize;
  int filesize;
  int pass;
  int gray_to_rgb;
  size_t commentsize;

  fbasename = BaseName(fname);

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  read_anything=0;

  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp) {
    SetISTR(ISTR_WARNING,"%s:  can't open file", fname);
    return 0;
  }

  /* find the size of the file */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                   png_xv_error, png_xv_warning);
  if (!png_ptr) {
    fclose(fp);
    FatalError("malloc failure in LoadPNG");
  }

  info_ptr = png_create_info_struct(png_ptr);

  if (!info_ptr) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    FatalError("malloc failure in LoadPNG");
  }

  if (setjmp(png_ptr->jmpbuf)) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    if (!read_anything) {
      if (pinfo->pic) {
        free(pinfo->pic);
        pinfo->pic = NULL;
      }
      if (pinfo->comment) {
        free(pinfo->comment);
        pinfo->comment = NULL;
      }
    }
    return read_anything;
  }

#ifdef PNG_NO_STDIO
  png_set_read_fn(png_ptr, fp, png_default_read_data);
  png_set_error_fn(png_ptr, NULL, png_xv_error, png_xv_warning);
#else
  png_init_io(png_ptr, fp);
#endif
  png_read_info(png_ptr, info_ptr);

  pinfo->w = pinfo->normw = info_ptr->width;
  pinfo->h = pinfo->normh = info_ptr->height;
  if (pinfo->w <= 0 || pinfo->h <= 0) {
    SetISTR(ISTR_WARNING, "%s:  image dimensions out of range (%dx%d)",
      fbasename, pinfo->w, pinfo->h);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return read_anything;
  }

  pinfo->frmType = F_PNG;

  sprintf(pinfo->fullInfo, "PNG, %d bit ",
          info_ptr->bit_depth * info_ptr->channels);

  switch(info_ptr->color_type) {
    case PNG_COLOR_TYPE_PALETTE:
      strcat(pinfo->fullInfo, "palette color");
      break;

    case PNG_COLOR_TYPE_GRAY:
      strcat(pinfo->fullInfo, "grayscale");
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
      strcat(pinfo->fullInfo, "grayscale+alpha");
      break;

    case PNG_COLOR_TYPE_RGB:
      strcat(pinfo->fullInfo, "truecolor");
      break;

    case PNG_COLOR_TYPE_RGB_ALPHA:
      strcat(pinfo->fullInfo, "truecolor+alpha");
      break;
  }

  sprintf(pinfo->fullInfo + strlen(pinfo->fullInfo),
	  ", %sinterlaced. (%d bytes)",
	  info_ptr->interlace_type ? "" : "non-", filesize);

  sprintf(pinfo->shrtInfo, "%lux%lu PNG", info_ptr->width, info_ptr->height);

  if (info_ptr->bit_depth < 8)
      png_set_packing(png_ptr);

  if (info_ptr->valid & PNG_INFO_gAMA)
    png_set_gamma(png_ptr, Display_Gamma, info_ptr->gamma);
/*
 *else
 *  png_set_gamma(png_ptr, Display_Gamma, 0.45);
 */

  gray_to_rgb = 0;   /* quiet a compiler warning */

  if (have_imagebg) {
    if (info_ptr->bit_depth == 16) {
      my_background.red   = imagebgR;
      my_background.green = imagebgG;
      my_background.blue  = imagebgB;
      my_background.gray = imagebgG;   /* used only if all three equal... */
    } else {
      my_background.red   = (imagebgR >> 8);
      my_background.green = (imagebgG >> 8);
      my_background.blue  = (imagebgB >> 8);
      my_background.gray = my_background.green;
    }
    png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN,
                       0, Display_Gamma);
    if ((info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
         (info_ptr->color_type == PNG_COLOR_TYPE_GRAY && HAVE_tRNS)) &&
        (imagebgR != imagebgG || imagebgR != imagebgB))  /* i.e., colored bg */
    {
      png_set_gray_to_rgb(png_ptr);
      png_set_expand(png_ptr);
      gray_to_rgb = 1;
    }
  } else {
    if (info_ptr->valid & PNG_INFO_bKGD) {
      png_set_background(png_ptr, &info_ptr->background,
                         PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    } else {
      my_background.red = my_background.green = my_background.blue =
        my_background.gray = 0;
      png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN,
                         0, Display_Gamma);
    }
  }

  if (info_ptr->bit_depth == 16)
    png_set_strip_16(png_ptr);

  if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
      info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
  {
    if (info_ptr->bit_depth == 1)
      pinfo->colType = F_BWDITHER;
    else
      pinfo->colType = F_GREYSCALE;
    png_set_expand(png_ptr);
  }

  pass=png_set_interlace_handling(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  if (info_ptr->color_type == PNG_COLOR_TYPE_RGB ||
     info_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA || gray_to_rgb)
  {
    linesize = 3 * pinfo->w;
    if (linesize/3 < pinfo->w) {   /* know pinfo->w > 0 (see above) */
      SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
        fbasename, pinfo->w, pinfo->h);
      png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
      return read_anything;
    }
    pinfo->colType = F_FULLCOLOR;
    pinfo->type = PIC24;
  } else {
    linesize = pinfo->w;
    pinfo->type = PIC8;
    if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
       info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      for (i = 0; i < 256; i++)
        pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
    } else {
      pinfo->colType = F_FULLCOLOR;
      for (i = 0; i < info_ptr->num_palette; i++) {
        pinfo->r[i] = info_ptr->palette[i].red;
        pinfo->g[i] = info_ptr->palette[i].green;
        pinfo->b[i] = info_ptr->palette[i].blue;
      }
    }
  }

  bufsize = linesize * pinfo->h;
  if (bufsize/linesize < pinfo->h) {  /* know linesize, pinfo->h > 0 (above) */
    SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
      fbasename, pinfo->w, pinfo->h);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return read_anything;
  }
  pinfo->pic = calloc((size_t)bufsize, (size_t)1);

  if (!pinfo->pic) {
    png_error(png_ptr, "can't allocate space for PNG image");
  }

  png_start_read_image(png_ptr);

  for (i = 0; i < pass; i++) {
    byte *p = pinfo->pic;
    for (j = 0; j < pinfo->h; j++) {
      png_read_row(png_ptr, p, NULL);
      read_anything = 1;
      if ((j & 0x1f) == 0) WaitCursor();
      p += linesize;
    }
  }

  png_read_end(png_ptr, info_ptr);

  if (info_ptr->num_text > 0) {
    commentsize = 1;

    for (i = 0; i < info_ptr->num_text; i++)
      commentsize += strlen(info_ptr->text[i].key) + 1 +
                     info_ptr->text[i].text_length + 2;

    if ((pinfo->comment = malloc(commentsize)) == NULL) {
      png_warning(png_ptr,"can't allocate comment string");
    }
    else {
      pinfo->comment[0] = '\0';
      for (i = 0; i < info_ptr->num_text; i++) {
        strcat(pinfo->comment, info_ptr->text[i].key);
        strcat(pinfo->comment, "::");
        strcat(pinfo->comment, info_ptr->text[i].text);
        strcat(pinfo->comment, "\n");
      }
    }
  }

  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

  fclose(fp);

  return 1;
}


/*******************************************/
static void
png_xv_error(png_ptr, message)
     png_structp png_ptr;
     png_const_charp message;
{
  SetISTR(ISTR_WARNING,"%s:  libpng error: %s", fbasename, message);

  longjmp(png_ptr->jmpbuf, 1);
}


/*******************************************/
static void
png_xv_warning(png_ptr, message)
     png_structp png_ptr;
     png_const_charp message;
{
  if (!png_ptr)
    return;

  SetISTR(ISTR_WARNING,"%s:  libpng warning: %s", fbasename, message);
}


/*******************************************/
void
VersionInfoPNG()	/* GRR 19980605 */
{
  fprintf(stderr, "   Compiled with libpng %s; using libpng %s.\n",
    PNG_LIBPNG_VER_STRING, png_libpng_ver);
  fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n",
    ZLIB_VERSION, zlib_version);
}

#endif /* HAVE_PNG */
