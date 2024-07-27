/*
 * xvwebp.c - load and write routines for 'WEBP' format pictures
 *
 * callable functions
 *
 *    WriteWEBP(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
 *    VersionInfoWEBP()
 *    LoadWEBP(fname, pinfo)
 *
 * Uses Google's WebP library
 *
 * https://developers.google.com/speed/webp/docs/api
 *
 * Installed on ubuntu with: apt-get install libwebp-dev
 */

/*
 * Assembled by Stephen Conley - @tanabi on GitHub
 *
 * I blatantly re-used code from xvpng.c -- credit and ownership to those
 * original authors (see that file for details).  My contribution is
 * provided freely.
 *
 */

#include "xv.h"

#ifdef HAVE_WEBP

#include "webp/decode.h"
#include "webp/encode.h"

/* Used for xv to hand off save info to our 'library' */
static char *filename;
static int   colorType;

/*** Stuff for WEBP Dialog box ***/
#define WEBPWIDE (288*dpiMult)
#define WEBPHIGH (185*dpiMult)

#define QUALITY   70     /* default quality */

#define DWIDE    (86*dpiMult)
#define DHIGH    (104*dpiMult)

#define P_BOK    0
#define P_BCANC  1
#define P_NBUTTS 2

#define BUTTH    (24*dpiMult)

static DIAL  qDial;
static BUTT  pbut[P_NBUTTS];
static CBUTT FlosslessCB;

static void drawWEBPD PARM((int x, int y, int w, int h));

static void drawWEBPD(x, y, w, h)
     int x, y, w, h;
{
  const char *title   = "Save WEBP file...";

  char ctitle1[20];
  const char *ctitle2 = "Quality value determines";
  const char *ctitle3 = "compression rate: higher";
  const char *ctitle4 = "quality = bigger file.";

  int i;
  int dx, dy;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<P_NBUTTS; i++) BTRedraw(&pbut[i]);

  DrawString(webpW, 15*dpiMult, 6*dpiMult + ASCENT, title);

  dx = 110*dpiMult;
  dy = 6*dpiMult + qDial.y + ASCENT*dpiMult;

  sprintf(ctitle1, "Default = %d", QUALITY);
  DrawString(webpW,      dx,  dy,            ctitle1);
  DrawString(webpW,      dx,  dy+LINEHIGH,   ctitle2);
  DrawString(webpW,      dx,  dy+2*LINEHIGH, ctitle3);
  DrawString(webpW,      dx,  dy+3*LINEHIGH, ctitle4);

  CBRedraw(&FlosslessCB);

  XSetClipMask(theDisp, theGC, None);
}

/*******************************************/

static void writeWEBP PARM((void));

static void writeWEBP()
{
  FILE       *fp;
  int         w, h, nc, rv, ptype, pfree;
  byte       *inpix, *rmap, *gmap, *bmap;
  int         unused_numcols;

  fp = OpenOutFile(filename);
  if (!fp) return;

  WaitCursor();
  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  unused_numcols = 0;
  rv = WriteWEBP(fp, inpix, ptype, w, h, rmap, gmap, bmap, unused_numcols, colorType);

  SetCursors(-1);

  if (CloseOutFile(fp, filename, rv) == 0) DirBox(0);

  if (pfree) free(inpix);
}

/*******************************************/

static void doCmd PARM((int cmd));

static void doCmd(cmd)
     int cmd;
{
  switch (cmd) {
    case P_BOK:
      {
        char *fullname;

        writeWEBP();
        WEBPDialog(0);

        fullname = GetDirFullName();
        if (!ISPIPE(fullname[0])) {
          XVCreatedFile(fullname);
          StickInCtrlList(0);
        }
      }
      break;

    case P_BCANC:
      WEBPDialog(0);
      break;

    default:
      break;
  }
}

/*******************************************/

static void clickWEBPD PARM((int x, int y));

static void clickWEBPD(x,y)
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

  else if (CBClick(&FlosslessCB,x,y)) {
    int oldval = FlosslessCB.val;

    CBTrack(&FlosslessCB);

    if (oldval != FlosslessCB.val)
    {
      DSetActive(&qDial, !FlosslessCB.val);
      DRedraw(&qDial); /* necessary? */
    }
  }
}

/*******************************************/
void CreateWEBPW()
{
  webpW = CreateWindow("xv webp", "XVWEBP", NULL,
                      WEBPWIDE, WEBPHIGH, infofg, infobg, 0);
  if (!webpW) FatalError("can't create WEBP window!");

  XSelectInput(theDisp, webpW, ExposureMask | ButtonPressMask | KeyPressMask);

  DCreate(&qDial, webpW,  12*dpiMult, 25*dpiMult, DWIDE, DHIGH, 0.0,
          100.0, QUALITY, 1.0, 3.0,
          infofg, infobg, hicol, locol, "Quality", NULL);

  CBCreate(&FlosslessCB,   webpW, 110*dpiMult, 6*dpiMult + qDial.y + ASCENT + 4*LINEHIGH, "Lossless",
           infofg, infobg, hicol, locol);
  FlosslessCB.val = 0;

  BTCreate(&pbut[P_BOK], webpW, WEBPWIDE - 180*dpiMult - 1*dpiMult, WEBPHIGH - 10*dpiMult - BUTTH - 1*dpiMult, 80*dpiMult, BUTTH,
          "Ok", infofg, infobg, hicol, locol);
  BTCreate(&pbut[P_BCANC], webpW, WEBPWIDE - 90*dpiMult - 1*dpiMult, WEBPHIGH - 10*dpiMult - BUTTH - 1*dpiMult, 80*dpiMult, BUTTH,
          "Cancel", infofg, infobg, hicol, locol);

  XMapSubwindows(theDisp, webpW);
}


/*******************************************/
void WEBPDialog(vis)
     int vis;
{
  if (vis) {
    CenterMapWindow(webpW, pbut[P_BOK].x + (int) pbut[P_BOK].w/2,
                          pbut[P_BOK].y + (int) pbut[P_BOK].h/2,
                    WEBPWIDE, WEBPHIGH);
  }
  else XUnmapWindow(theDisp, webpW);
  webpUp = vis;
}


/*******************************************/
int WEBPCheckEvent(xev)
     XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!webpUp) return 0;
  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x; y = e->y; w = e->width; h = e->height;

    /* throw away excess expose events for 'dumb' windows */
    if (e->count > 0 && (e->window == qDial.win)) {}

    else if (e->window == webpW)       drawWEBPD(x, y, w, h);
    else if (e->window == qDial.win)   DRedraw(&qDial);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == webpW)      clickWEBPD(x,y);
      else if (e->window == qDial.win)  DTrack(&qDial, x, y);
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

    if (e->window == webpW) {
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

/*
 * This is how xv hands off save parameters to us.
 *
 * Takes a file name and the color type.
 */
void WEBPSaveParams(fname, col)
     char *fname;
     int col;
{
  filename = fname;
  colorType = col;
}

/*
 * Method to handle writing ('encoding') a WEBP file.
 *
 * So there's a lot of potential options here.  At first, I was going to
 * use the dialog box approach used by PNG, JPEG, TIFF and some others to
 * offer these options.
 *
 * However, the amount of work involved there -- and believe me, I know,
 * I went pretty far down the rabbit hole -- is a magnitude more than
 * doing it the easy way.  Not only is the XV UI side much more complex,
 * but to expose the options is way more complex on the libwebp side as
 * well.  It goes from a single well-documented function call, to a bunch
 * of not well documented function calls and a need to write callbacks and
 * such.
 *
 * I came to realize that I, personally, am unlikely to ever save a webp
 * file and that I just don't care enough to put that level of work into
 * it.
 *
 * I'm sorry! :)
 *
 * Thus, this just saves lossless format with default settings.  I figure
 * that is the most useful webp to save.  We could make a seperate
 * webp (lossless) vs. webp (lossy) save list items and treat them like
 * 2 different formats to avoid the dialog box.  Or if someone comes along
 * later and cares to implement the dialog box, more power to you.
 *
 * xvpng.c has a very good example of how to do it, and you'll have to
 * remove this WriteWEBP call from xvdir.c and grep around for HAVE_PNG
 * in the rest of the code -- particularly xv.c, xvdir.c and xvevent.c -- to do
 * it.  It's not super hard, but it'll take this from an afternoon project
 * to a "few days" project if I do that, and that's more than I can do
 * right now.
 *
 * Sorry for writing a diary journal in comments!  But hopefully this is
 * helpful for someone else who may come along and care about this problem
 * more than I do.
 *
 * - @tanabi on GITHUB
 */
int
WriteWEBP(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
     FILE *fp;
     byte *pic;
     int   ptype,w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
{
  uint8_t*  webp_in = NULL;
  uint8_t*  webp_out = NULL;
  byte      mono_byte = 0;
  int       free_me = 0; /* if 1, we need to free webp_in */
  int       linesize = w*3;
  int       i = 0;
  int       final_size = 0;

  XV_UNUSED(numcols);

  /*
   * First, we have to prepare our data in such a fashion that we can
   * write it out.  webp only supports 24 or 32 bit formats, so any
   * other formats will have to be massaged into 24/32 bit.
   *
   * Greyscale is a bit of a pill.  WEBP doesn't have a 'greyscale' format
   * that just uses intensities, so we'll have to make our own.
   */
  if (ptype == PIC24) {
    if (colorstyle == F_GREYSCALE) {
        /* Convert the pic to greyscale */
        webp_in = (uint8_t*)malloc(linesize*h);
        free_me = 1;

        for (i = 0; i < linesize*h; i+=3) {
          mono_byte = MONO(pic[i], pic[i+1], pic[i+2]);

          webp_in[i] = webp_in[i+1] = webp_in[i+2] = mono_byte;
        }
    } else {
      /* nothing to do, we're already good to go */
      webp_in = (uint8_t*)pic;
    }
  } else {
    webp_in = (uint8_t*)malloc(linesize*h);
    free_me = 1;

    for (i = 0; i < (w*h); i++) {
      if (colorstyle == F_GREYSCALE) {
        mono_byte = MONO(rmap[pic[i]], gmap[pic[i]], bmap[pic[i]]);
        webp_in[i*3] = mono_byte;
        webp_in[(i*3)+1] = mono_byte;
        webp_in[(i*3)+2] = mono_byte;
      } else {
        webp_in[i*3] = rmap[pic[i]];
        webp_in[(i*3)+1] = gmap[pic[i]];
        webp_in[(i*3)+2] = bmap[pic[i]];
      }
    }
  }

  /* Try encoding it */
  final_size = WebPEncodeLosslessRGB(webp_in, w, h, linesize, &webp_out);

  if (!final_size) {
    if (free_me) {
      free(webp_in);
    }

    /* I'm not sure if this is necessary */
    if (webp_out) {
      WebPFree(webp_out);
    }

    return -1;
  }

  /* Write it out */
  fwrite(webp_out, final_size, 1, fp);

  /* Clean up */
  if (free_me) {
    free(webp_in);
  }

  WebPFree(webp_out);

  if (ferror(fp)) {
    return -1;
  }

  return 0;
}

/*
 * Output version information to stderr
 */
void
VersionInfoWEBP()
{
  int decoder_version;
  int encoder_version;

  decoder_version = WebPGetDecoderVersion();
  encoder_version = WebPGetEncoderVersion();

  fprintf(stderr, "   Compiled with libwebp; decoder version: %hhu.%hhu.%hhu\n",
    (unsigned char)decoder_version,
    ((unsigned char*)&decoder_version)[1],
    ((unsigned char*)&decoder_version)[2]
  );
  fprintf(stderr, "                          encoder version: %hhu.%hhu.%hhu\n",
    (unsigned char)encoder_version,
    ((unsigned char*)&encoder_version)[1],
    ((unsigned char*)&encoder_version)[2]
  );
}

/*
 * Loader for WebP Images.
 *
 * Takes file name and PICINFO structure.
 *
 * Returns 1 on success, 0 on failure
 *
 */
int LoadWEBP(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
{
  FILE                  *fp;
  int                   filesize;
  int                   bufsize;
  int                   linesize;
  int                   w, h;
  unsigned int          npixels;
  uint8_t               *raw_data, *rgba, alpha;
  WebPBitstreamFeatures features;
  VP8StatusCode         status;

  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp) {
    SetISTR(ISTR_WARNING,"%s:  can't open file", fname);
    return 0;
  }

  /* find the size of the file */
  fseek(fp, 0L, SEEK_END);
  filesize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  /* Try to make our buffer */
  raw_data = (uint8_t*)malloc(filesize);

  if (!raw_data) {
    FatalError("malloc failure in LoadWEBP");
    fclose(fp);
    return 0;
  }

  /* Load it up */
  if (!fread(raw_data, filesize, 1, fp)) {
    free(raw_data);
    fclose(fp);
    SetISTR(ISTR_WARNING, "%s:  couldn't read entire file", fname);
    return 0;
  }

  /* Shouldn't need the file anymore */
  fclose(fp);

  /* Get basic information about the image */
  status = WebPGetFeatures(raw_data, filesize, &features);

  if (status != VP8_STATUS_OK) {
    free(raw_data);
    SetISTR(ISTR_WARNING, "failed to load WebP Features");
    return 0;
  }

  /* Null out some default stuff */
  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  /* Set sizing information */
  pinfo->w = pinfo->normw = features.width;
  pinfo->h = pinfo->normh = features.height;
  npixels = pinfo->h * pinfo->w;

  /* Basic sanity checking */
  if (pinfo->w <= 0 || pinfo->h <= 0) {
    free(raw_data);
    SetISTR(ISTR_WARNING, "%s:  image dimensions out of range (%dx%d)",
      fname, pinfo->w, pinfo->h);
    return 0;
  }

  /* Set file type if we've gotten this far, along with other type
   * informations
   */
  pinfo->frmType = F_WEBP;
  pinfo->colType = F_FULLCOLOR;
  pinfo->type = PIC24;

  /* Compile fullInfo - note that this is a 128 byte buffer */
  strcpy(pinfo->fullInfo, "WebP");

  if (features.has_alpha) {
    strcat(pinfo->fullInfo, ", with alpha channel");
  }

  if (features.has_animation) {
    strcat(pinfo->fullInfo, ", animated");
  }

  switch (features.format) {
    case 1:
      strcat(pinfo->fullInfo, ", lossy");
      break;

    case 2:
      strcat(pinfo->fullInfo, ", lossless");
      break;

    default:
      strcat(pinfo->fullInfo, ", mixed loss/lossless");
  }

  /* Set short info (128 byte buffer as well) */
  sprintf(pinfo->shrtInfo, "%dx%d WEBP", pinfo->w, pinfo->h);

  /* Calculate datasize and load */
  linesize = 3 * pinfo->w;
  bufsize = linesize * pinfo->h;

  /* Sanity check */
  if (((linesize / 3) < pinfo->w) || ((bufsize / linesize) < pinfo->h)) {
    SetISTR(ISTR_WARNING, "image dimensions too large (%dx%d)",
      pinfo->w, pinfo->h);
    free(raw_data);
    return 0;
  }

  /*
   * Getting data seems to fail on animations -- probably because I"m
   * not allocating enough memory, or maybe WebPDecodeRGBInto doesn't
   * support it.
   *
   * xv doesn't really support animations anyway I don't think, so we
   * will give a bummer message.
   */
  if (features.has_animation) {
    free(raw_data);
    SetISTR(ISTR_WARNING, "xv doesn't support animations (yet)");
    return 0;
  }

  /*
   * Get data. Since we're reading RGBA values, we can no longer
   * decode directly in to pinfo->pic, which expects RGB. So read
   * into a separate buffer, and then we'll copy the appropriate
   * pixel values over afterwards.
   */
  if ((rgba = WebPDecodeRGBA(raw_data, filesize, &w, &h)) == NULL)
  {
    free(raw_data);
    SetISTR(ISTR_WARNING, "failed to decode WEBP data");
    return 0;
  }

  /* Check that the image we read is the size we expected */
  if (w != pinfo->w || h != pinfo->h)
  {
    free(raw_data);
    WebPFree(rgba);
    SetISTR(ISTR_WARNING, "image size mismatch (expected %ux%u, got %ux%u)",
	    pinfo->w, pinfo-h, w, h);
    return 0;
  }

  /* Allocate the xv picture buffer */
  pinfo->pic = (byte*)malloc(npixels * 3);

  if (!pinfo->pic) {
    free(raw_data);
    WebPFree(rgba);
    FatalError("malloc failure in LoadWEBP");
    return 0;
  }

  /*** We can't show transparency, but we can simulate it by
  **** proportionally reducing the intensity of the relevant
  **** pixel, giving an effect as if the image had an alpha
  **** channel and was overlaid on a black background. This
  **** will almost always be preferable to just throwing
  **** away the transparency information in the image.
  ***/

  for (int i=0; i < npixels; i++)
  {
    alpha = *(rgba + i*4 + 3);
    pinfo->pic[i*3 + 0] = *(rgba + i*4 + 0) * alpha/255;
    pinfo->pic[i*3 + 1] = *(rgba + i*4 + 1) * alpha/255;
    pinfo->pic[i*3 + 2] = *(rgba + i*4 + 2) * alpha/255;
  }

  free(raw_data);
  WebPFree(rgba);
  return 1;
}

#endif
