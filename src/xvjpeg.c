/*
 * xvjpeg.c - i/o routines for 'jpeg' format pictures
 */

/* based in part on 'example.c' from the IJG JPEG distribution */


#include "copyright.h"
#include "xv.h"

#ifdef HAVE_JPEG

#include <setjmp.h>

#include "jpeglib.h"   /* currently defines JPEG_APP0 but not JPEG_APP1 */
#include "jerror.h"

#ifndef JPEG_APP1
#  define JPEG_APP1 (JPEG_APP0 + 1)   /* EXIF marker */
#endif

#define CREATOR_STR "CREATOR: "

#if BITS_IN_JSAMPLE != 8
  Sorry, this code copes only with 8-bit JSAMPLEs. /* deliberate syntax err */
#endif


/*** Stuff for JPEG Dialog box ***/
#define JWIDE 400
#define JHIGH 200
#define J_NBUTTS 2
#define J_BOK    0
#define J_BCANC  1
#define BUTTH    24

/* Minimum size compression when doing a 'quick' image load.  (Of course, if
   the image *is* smaller than this, you'll get whatever size it actually is.)
   This is currently hardcoded to be twice the size of a schnauzer icon, as
   the schnauzer's the only thing that does a quick load... */

#define QUICKWIDE 160
#define QUICKHIGH 120

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf               setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;


/*** local functions ***/
static    void         drawJD             PARM((int, int, int, int));
static    void         clickJD            PARM((int, int));
static    void         doCmd              PARM((int));
static    void         writeJPEG          PARM((void));
#if JPEG_LIB_VERSION > 60
METHODDEF(void)        xv_error_exit      PARM((j_common_ptr));
METHODDEF(void)        xv_error_output    PARM((j_common_ptr));
METHODDEF(void)        xv_prog_meter      PARM((j_common_ptr));
#else
METHODDEF void         xv_error_exit      PARM((j_common_ptr));
METHODDEF void         xv_error_output    PARM((j_common_ptr));
METHODDEF void         xv_prog_meter      PARM((j_common_ptr));
#endif
static    unsigned int j_getc             PARM((j_decompress_ptr));
#if JPEG_LIB_VERSION > 60
METHODDEF(boolean)     xv_process_comment PARM((j_decompress_ptr));
METHODDEF(boolean)     xv_process_app1    PARM((j_decompress_ptr));
#else
METHODDEF boolean      xv_process_comment PARM((j_decompress_ptr));
METHODDEF boolean      xv_process_app1    PARM((j_decompress_ptr));
#endif
static    int          writeJFIF          PARM((FILE *, byte *, int,int,int));



/*** local variables ***/
static char *filename;
static const char *fbasename;
static char *comment;
static byte *exifInfo;
static int   exifInfoSize;   /* not a string => must track size explicitly */
static int   colorType;

static DIAL  qDial, smDial;
static BUTT  jbut[J_NBUTTS];




/***************************************************************************/
/* JPEG SAVE DIALOG ROUTINES ***********************************************/
/***************************************************************************/


/***************************************************/
void CreateJPEGW()
{
  jpegW = CreateWindow("xv jpeg","XVjpeg",NULL,JWIDE,JHIGH,infofg,infobg,0);
  if (!jpegW) FatalError("can't create jpeg window!");

  XSelectInput(theDisp, jpegW, ExposureMask | ButtonPressMask | KeyPressMask);

  DCreate(&qDial, jpegW, 10, 10, 80, 100, 1.0, 100.0, 75.0, 1.0, 5.0,
	  infofg, infobg, hicol, locol, "Quality", "%");

  DCreate(&smDial, jpegW, 120, 10, 80, 100, 0.0, 100.0, 0.0, 1.0, 5.0,
	  infofg, infobg, hicol, locol, "Smoothing", "%");

  BTCreate(&jbut[J_BOK], jpegW, JWIDE-180-1, JHIGH-10-BUTTH-1, 80, BUTTH,
	   "Ok", infofg, infobg, hicol, locol);

  BTCreate(&jbut[J_BCANC], jpegW, JWIDE-90-1, JHIGH-10-BUTTH-1, 80, BUTTH,
	   "Cancel", infofg, infobg, hicol, locol);

  XMapSubwindows(theDisp, jpegW);
}


/***************************************************/
void JPEGDialog(vis)
     int vis;
{
  if (vis) {
    CenterMapWindow(jpegW, jbut[J_BOK].x + (int) jbut[J_BOK].w/2,
		    jbut[J_BOK].y + (int) jbut[J_BOK].h/2, JWIDE, JHIGH);
  }
  else     XUnmapWindow(theDisp, jpegW);
  jpegUp = vis;
}


/***************************************************/
int JPEGCheckEvent(xev)
     XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!jpegUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    /* throw away excess expose events for 'dumb' windows */
    if (e->count > 0 && (e->window == qDial.win ||
			 e->window == smDial.win)) {}

    else if (e->window == jpegW)       drawJD(x, y, w, h);
    else if (e->window == qDial.win)   DRedraw(&qDial);
    else if (e->window == smDial.win)  DRedraw(&smDial);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == jpegW)      clickJD(x,y);
      else if (e->window == qDial.win)  DTrack(&qDial,  x,y);
      else if (e->window == smDial.win) DTrack(&smDial, x,y);
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

    if (e->window == jpegW) {
      if (stlen) {
	if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
	  FakeButtonPress(&jbut[J_BOK]);
	}
	else if (buf[0] == '\033') {            /* ESC */
	  FakeButtonPress(&jbut[J_BCANC]);
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
void JPEGSaveParams(fname, col)
     char *fname;
     int col;
{
  filename = fname;
  colorType = col;
}


/***************************************************/
static void drawJD(x,y,w,h)
     int x,y,w,h;
{
  const char *title  = "Save JPEG file...";
  const char *title1 = "Quality value determines";
  const char *title2 = "compression rate: higher";
  const char *title3 = "quality = bigger file.";
  const char *title4 = "Use smoothing if saving";
  const char *title5 = "an 8-bit image (eg, a GIF).";

  const char *qtitle1 = "Default = 75.";
  const char *qtitle2 = "Useful range";
  const char *qtitle3 = "is 5-95.";

  const char *smtitle1 = "Default = 0 (none).";
  const char *smtitle2 = "10-30 is enough";
  const char *smtitle3 = "for typical GIFs.";

  int  i;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<J_NBUTTS; i++) BTRedraw(&jbut[i]);

  DrawString(jpegW, 220, 10+ASCENT,                   title);
  DrawString(jpegW, 230, 10+ASCENT+LINEHIGH*1,        title1);
  DrawString(jpegW, 230, 10+ASCENT+LINEHIGH*2,        title2);
  DrawString(jpegW, 230, 10+ASCENT+LINEHIGH*3,        title3);
  DrawString(jpegW, 230, 10+ASCENT+LINEHIGH*4,        title4);
  DrawString(jpegW, 230, 10+ASCENT+LINEHIGH*5,        title5);

  DrawString(jpegW,  15, 10+100+10+ASCENT,            qtitle1);
  DrawString(jpegW,  15, 10+100+10+ASCENT+LINEHIGH,   qtitle2);
  DrawString(jpegW,  15, 10+100+10+ASCENT+LINEHIGH*2, qtitle3);

  DrawString(jpegW, 115, 10+100+10+ASCENT+LINEHIGH*0, smtitle1);
  DrawString(jpegW, 115, 10+100+10+ASCENT+LINEHIGH*1, smtitle2);
  DrawString(jpegW, 115, 10+100+10+ASCENT+LINEHIGH*2, smtitle3);

  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void clickJD(x,y)
     int x,y;
{
  int i;
  BUTT *bp;

  /* check BUTTs */

  for (i=0; i<J_NBUTTS; i++) {
    bp = &jbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<J_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }
}



/***************************************************/
static void doCmd(cmd)
     int cmd;
{

  switch (cmd) {
  case J_BOK: {
    char *fullname;

    writeJPEG();
    JPEGDialog(0);

    fullname = GetDirFullName();
    if (!ISPIPE(fullname[0])) {
      XVCreatedFile(fullname);
      StickInCtrlList(0);
    }
  }
    break;

  case J_BCANC:  JPEGDialog(0);  break;

  default:        break;
  }
}





/*******************************************/
static void writeJPEG()
{
  FILE          *fp;
  int            i, nc, rv, w, h, npixels, ptype, pfree;
  register byte *ip, *ep;
  byte          *inpix, *rmap, *gmap, *bmap;
  byte          *image8, *image24;

  /* get the XV image into a format that the JPEG software can grok on.
     Also, open the output file, so we don't waste time doing this format
     conversion if we won't be able to write it out */


  fp = OpenOutFile(filename);
  if (!fp) return;

  fbasename = BaseName(filename);

  WaitCursor();
  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  /* this case may not be possible to trigger, but not totally clear, so... */
  npixels = w*h;
  if (w <= 0 || h <= 0 || npixels/w < h) {
    SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
            fbasename, w, h);
    return;
  }

  image8 = image24 = (byte *) NULL;


  /* monocity: see if the image is mono, save it that way to save space */
  if (colorType != F_GREYSCALE) {
    if (ptype == PIC8) {
      for (i=0; i<nc && rmap[i]==gmap[i] && rmap[i]==bmap[i]; i++);
      if (i==nc) colorType = F_GREYSCALE;    /* made it all the way through */
    }
    else {  /* PIC24 */
      for (i=0,ip=inpix; i<npixels && ip[0]==ip[1] && ip[1]==ip[2]; i++,ip+=3);
      if (i==npixels) colorType = F_GREYSCALE;  /* all the way through */
    }
  }


  /* first thing to do is build an 8/24-bit Greyscale/TrueColor image
     (meaning: non-colormapped) */

  if (colorType == F_GREYSCALE) {   /* build an 8-bit Greyscale image */
    image8 = (byte *) malloc((size_t) npixels);
    if (!image8) FatalError("writeJPEG: unable to malloc image8\n");

    if (ptype == PIC8) {
      for (i=0,ip=image8,ep=inpix; i<npixels; i++, ip++, ep++)
	*ip = MONO(rmap[*ep], gmap[*ep], bmap[*ep]);
    }
    else {  /* PIC24 */
      for (i=0,ip=image8,ep=inpix; i<npixels; i++, ip++, ep+=3)
	*ip = MONO(ep[0],ep[1],ep[2]);
    }
  }

  else {    /* *not* F_GREYSCALE */
    if (ptype == PIC8) {
      int count = 3*npixels;

      /* already know npixels > 0 (above) */
      if (count/3 < npixels) {
        SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
                fbasename, w, h);
        return;
      }

      image24 = (byte *) malloc((size_t) count);
      if (!image24) {  /* this simply isn't going to work */
	FatalError("writeJPEG: unable to malloc image24\n");
      }

      for (i=0, ip=image24, ep=inpix; i<npixels; i++, ep++) {
	*ip++ = rmap[*ep];
	*ip++ = gmap[*ep];
	*ip++ = bmap[*ep];
      }
    }

    else {  /* PIC24 */
      image24 = inpix;
    }
  }


  /* in any event, we've got some valid image.  Do the JPEG Thing */
  rv = writeJFIF(fp, (colorType==F_GREYSCALE) ? image8 : image24,
		 w, h, colorType);

  if      (colorType == F_GREYSCALE) free(image8);
  else if (ptype == PIC8)            free(image24);

  if (pfree) free(inpix);

  if (CloseOutFile(fp, filename, rv) == 0) DirBox(0);
  SetCursors(-1);
}






/***************************************************************************/
/* JPEG INTERFACE ROUTINES *************************************************/
/***************************************************************************/



/**************************************************/
#if JPEG_LIB_VERSION > 60
METHODDEF(void) xv_error_exit(cinfo)
#else
METHODDEF void  xv_error_exit(cinfo)
#endif
     j_common_ptr cinfo;
{
  my_error_ptr myerr;

  myerr = (my_error_ptr) cinfo->err;
  (*cinfo->err->output_message)(cinfo);     /* display error message */
  longjmp(myerr->setjmp_buffer, 1);         /* return from error */
}


/**************************************************/
#if JPEG_LIB_VERSION > 60
METHODDEF(void) xv_error_output(cinfo)
#else
METHODDEF void  xv_error_output(cinfo)
#endif
     j_common_ptr cinfo;
{
  my_error_ptr myerr;
  char         buffer[JMSG_LENGTH_MAX];

  myerr = (my_error_ptr) cinfo->err;
  (*cinfo->err->format_message)(cinfo, buffer);

  SetISTR(ISTR_WARNING, "%s: %s", fbasename, buffer);   /* send it to XV */
}


/**************************************************/
#if JPEG_LIB_VERSION > 60
METHODDEF(void) xv_prog_meter(cinfo)
#else
METHODDEF void  xv_prog_meter(cinfo)
#endif
     j_common_ptr cinfo;
{
  struct jpeg_progress_mgr *prog;

  prog = cinfo->progress;

  if ((prog->pass_counter & 0x3f)==0) WaitCursor();

#ifdef FOO
  fprintf(stderr,"xv_prog_meter: cnt=%ld, maxcnt=%ld, pass=%d, maxpass=%d\n",
	  prog->pass_counter, prog->pass_limit, prog->completed_passes,
	  prog->total_passes);
#endif
}



/***************************************************************************/
/* LOAD ROUTINES ***********************************************************/
/***************************************************************************/


/*******************************************/
int LoadJFIF(fname, pinfo, quick)
     char    *fname;
     PICINFO *pinfo;
     int      quick;
{
  /* returns '1' on success, '0' on failure */

  struct jpeg_decompress_struct    cinfo;
  struct jpeg_progress_mgr         prog;
  struct my_error_mgr              jerr;
  JSAMPROW                         rowptr[1];
  FILE                            *fp;
  const char                      *colorspace_name = "Color";
  byte                            *pic, *pic_end;
  long                             filesize;
  int                              i,w,h,bperpix,bperline,count;


  fbasename = BaseName(fname);
  pic       = (byte *) NULL;
  comment   = (char *) NULL;
  exifInfo  = (byte *) NULL;

  pinfo->type  = PIC8;

  if ((fp = xv_fopen(fname, "r")) == NULL) return 0;

  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);


  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit     = xv_error_exit;
  jerr.pub.output_message = xv_error_output;

  if (setjmp(jerr.setjmp_buffer)) {
L1:
    /* if we're here, it blowed up... */
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    if (pic)      free(pic);
    if (comment)  free(comment);
    if (exifInfo) free(exifInfo);

    pinfo->pic      = (byte *) NULL;
    pinfo->comment  = (char *) NULL;
    pinfo->exifInfo = (byte *) NULL;
    pinfo->exifInfoSize = 0;

    comment  = (char *) NULL;
    exifInfo = (byte *) NULL;
    exifInfoSize = 0;

    return 0;
  }


  jpeg_create_decompress(&cinfo);
  jpeg_set_marker_processor(&cinfo, JPEG_COM, xv_process_comment);
  jpeg_set_marker_processor(&cinfo, JPEG_APP1, xv_process_app1);

  /* hook up progress meter */
  prog.progress_monitor = xv_prog_meter;
  cinfo.progress = &prog;

  jpeg_stdio_src(&cinfo, fp);
  (void) jpeg_read_header(&cinfo, TRUE);



  /* do various cleverness regarding decompression parameters & such... */



  jpeg_calc_output_dimensions(&cinfo);
  pinfo->normw = w = cinfo.output_width;
  pinfo->normh = h = cinfo.output_height;

  if (quick) {
    int wfac, hfac, fac;
    wfac = w / QUICKWIDE;
    hfac = h / QUICKHIGH;

    fac = wfac;  if (fac > hfac) fac = hfac;
    if      (fac > 8) fac = 8;
    else if (fac > 4) fac = 4;
    else if (fac > 2) fac = 2;
    else fac = 1;

    cinfo.scale_num   = 1;
    cinfo.scale_denom = fac;
    cinfo.dct_method = JDCT_FASTEST;
    cinfo.do_fancy_upsampling = FALSE;

    pinfo->normw = w;  pinfo->normh = h;

    jpeg_calc_output_dimensions(&cinfo);
    w = cinfo.output_width;
    h = cinfo.output_height;
  }


  cinfo.quantize_colors = FALSE;     /* default: give 24-bit image to XV */
  switch (cinfo.num_components) {
    case 1:
      cinfo.out_color_space = JCS_GRAYSCALE;
      colorspace_name = "Greyscale";
      for (i=0; i<256; i++) pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
      break;

    case 3:
      cinfo.out_color_space = JCS_RGB;
      goto L2;

    case 4:
      cinfo.out_color_space = JCS_CMYK;
      colorspace_name = "4-Plane Color";
L2:
      if (!quick && picType == PIC8 && conv24MB.flags[CONV24_LOCK] == 1) {
        /*
         * we're locked into 8-bit mode:
         *   if CONV24_FAST, use JPEG's one-pass quantizer
         *   if CONV24_SLOW, use JPEG's two-pass quantizer
         *   if CONV24_BEST, or other, ask for 24-bit image and hand it to XV
         */
        cinfo.desired_number_of_colors = 256;

        if (conv24 == CONV24_FAST || conv24 == CONV24_SLOW) {
          cinfo.quantize_colors = TRUE;
          state824 = 1; /* image was converted from 24 to 8 bits */
          cinfo.two_pass_quantize = (conv24 == CONV24_SLOW);
        }
      }
      break;

    default:
      SetISTR(ISTR_WARNING, "%s:  can't read %d-plane JPEG file!",
              fbasename, cinfo.output_components);
      goto L1;
  }
  SetISTR(ISTR_INFO, "Loading %dx%d %s JPEG (%ld bytes)...", w, h,
          colorspace_name, filesize);

  jpeg_calc_output_dimensions(&cinfo);   /* note colorspace changes... */


  bperpix = cinfo.output_components;
  pinfo->type = (bperpix == 1) ? PIC8 : PIC24;

  bperline = w * bperpix;
  count = h * bperline;
  if (w <= 0 || h <= 0 || bperline/w < bperpix || count/h < bperline) {
    SetISTR(ISTR_WARNING, "%s:  image dimensions too large (%dx%d)",
            fbasename, w, h);
    goto L1;
  }

  pic = (byte *) malloc((size_t) count);
  if (!pic) {
    SetISTR(ISTR_WARNING, "%s:  can't read JPEG file - out of memory",
	    fbasename);
    goto L1;
  }
  pic_end = pic + count;

  jpeg_start_decompress(&cinfo);

  while (cinfo.output_scanline < cinfo.output_height) {
    if (cinfo.output_scanline < 0) {   /* should never happen, but... */
      SetISTR(ISTR_WARNING, "%s:  invalid negative scanline (%d)",
              fbasename, cinfo.output_scanline);
      goto L1;
    }
    rowptr[0] = (JSAMPROW) &pic[cinfo.output_scanline * w * bperpix];
    (void) jpeg_read_scanlines(&cinfo, rowptr, (JDIMENSION) 1);
  }


  /* Convert CMYK to RGB color space */

  if (bperpix > 3) {
    register byte *p = pic;

    /* According to documentation accompanying the IJG JPEG Library, it appears
     * that some versions of Adobe Systems' "Photoshop" write inverted CMYK
     * data, where Byte 0 represents 100% ink coverage instead of 0% ink as
     * you'd expect.  The JPEG Library's implementors made a policy decision
     * not to correct for this in the Library, but instead force applications
     * to deal with it; so we try to do that here:
     */
    if (cinfo.saw_Adobe_marker) { /* assume inverted data */
      register byte *q = pic;

      do {
        register int cmy, k = 255 - q[3];

        if ((cmy = *q++ - k) < 0) cmy = 0; *p++ = cmy; /* R */
        if ((cmy = *q++ - k) < 0) cmy = 0; *p++ = cmy; /* G */
        if ((cmy = *q++ - k) < 0) cmy = 0; *p++ = cmy; /* B */
      } while (++q <= pic_end);
    }
    else { /* assume normal data */
      register byte *q = pic;

      do {
        register int cmy, k = 255 - q[3];

        if ((cmy = k - *q++) < 0) cmy = 0; *p++ = cmy; /* R */
        if ((cmy = k - *q++) < 0) cmy = 0; *p++ = cmy; /* G */
        if ((cmy = k - *q++) < 0) cmy = 0; *p++ = cmy; /* B */
      } while (++q <= pic_end);
    }
    pic = realloc(pic,p-pic); /* Release extra storage */
  }



  /* return 'PICINFO' structure to XV */

  pinfo->pic = pic;
  pinfo->w = w;
  pinfo->h = h;
  pinfo->frmType = F_JPEG;

  if (cinfo.out_color_space == JCS_GRAYSCALE) {
    pinfo->colType = F_GREYSCALE;

    for (i=0; i<256; i++) pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
  }
  else {
    pinfo->colType = F_FULLCOLOR;

    if (cinfo.quantize_colors) {
      switch (bperpix) {
        case 3:
          for (i = 0; i < cinfo.actual_number_of_colors; i++) {
            pinfo->r[i] = cinfo.colormap[0][i];
            pinfo->g[i] = cinfo.colormap[1][i];
            pinfo->b[i] = cinfo.colormap[2][i];
          }
          break;

        case 4:
          for (i = 0; i < cinfo.actual_number_of_colors; i++) {
            register int cmy, k = cinfo.colormap[3][i];

            if ((cmy = 255 - cinfo.colormap[0][i] - k) < 0) cmy = 0;
            pinfo->r[i] = cmy;
            if ((cmy = 255 - cinfo.colormap[1][i] - k) < 0) cmy = 0;
            pinfo->g[i] = cmy;
            if ((cmy = 255 - cinfo.colormap[2][i] - k) < 0) cmy = 0;
            pinfo->b[i] = cmy;
          }
          break;
      }
    }
  }

  sprintf(pinfo->fullInfo, "%s JPEG. (%ld bytes)", colorspace_name, filesize);
  sprintf(pinfo->shrtInfo, "%dx%d %s JPEG. ", w, h, colorspace_name);

  pinfo->comment      = comment;
  pinfo->exifInfo     = exifInfo;
  pinfo->exifInfoSize = exifInfoSize;

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(fp);

  /* ownership transferred to pinfo */
  comment  = (char *) NULL;
  exifInfo = (byte *) NULL;
  exifInfoSize = 0;

  return 1;
}




/**************************************************/
static unsigned int j_getc(cinfo)
     j_decompress_ptr cinfo;
{
  struct jpeg_source_mgr *datasrc = cinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (! (*datasrc->fill_input_buffer) (cinfo))
      ERREXIT(cinfo, JERR_CANT_SUSPEND);
  }
  datasrc->bytes_in_buffer--;
  return GETJOCTET(*datasrc->next_input_byte++);
}


/**************************************************/
#if JPEG_LIB_VERSION > 60
METHODDEF(boolean) xv_process_comment(cinfo)
#else
METHODDEF boolean  xv_process_comment(cinfo)
#endif
     j_decompress_ptr cinfo;
{
  int          length, hasnull;
  unsigned int ch;
  char         *oldsp, *sp;

  length  = j_getc(cinfo) << 8;
  length += j_getc(cinfo);
  length -= 2;                  /* discount the length word itself */

  if (!comment) {
    comment = (char *) malloc((size_t) length + 1);
    if (comment) comment[0] = '\0';
  }
  else comment = (char *) realloc(comment, strlen(comment) + length + 1);
  if (!comment) FatalError("out of memory in xv_process_comment");

  oldsp = sp = comment + strlen(comment);
  hasnull = 0;

  while (length-- > 0) {
    ch = j_getc(cinfo);
    *sp++ = (char) ch;
    if (ch == 0) hasnull = 1;
  }

  if (hasnull) sp = oldsp;       /* swallow comment blocks that have nulls */
  *sp++ = '\0';

  return TRUE;
}


/**************************************************/
#if JPEG_LIB_VERSION > 60
METHODDEF(boolean) xv_process_app1(cinfo)   /* Geoff H. Kuenning 20030331 */
#else
METHODDEF boolean  xv_process_app1(cinfo)
#endif
     j_decompress_ptr cinfo;
{
  int          length;
  unsigned int ch;
  byte         *sp;

  length  = j_getc(cinfo) << 8;
  length += j_getc(cinfo);
  length -= 2;                  /* discount the length word itself */

  if (!exifInfo) {
    exifInfo = (byte *) malloc((size_t) length);
    exifInfoSize = 0;
  }
  else exifInfo = (byte *) realloc(exifInfo, exifInfoSize + length);
  if (!exifInfo) FatalError("out of memory in xv_process_app1 (EXIF info)");
  
  sp = exifInfo + exifInfoSize;
  exifInfoSize += length;

  while (length-- > 0) {
    ch = j_getc(cinfo);
    *sp++ = (byte) ch;
  }

  return TRUE;
}




/***************************************************************************/
/* WRITE ROUTINES **********************************************************/
/***************************************************************************/

static int writeJFIF(fp, pic, w,h, coltype)
     FILE *fp;
     byte *pic;
     int   w,h,coltype;
{
  struct     jpeg_compress_struct cinfo;
  struct     jpeg_progress_mgr    prog;
  struct     my_error_mgr         jerr;
  JSAMPROW                        rowptr[1];
  int                             i, bperpix;
  char                            xvcmt[256];

  comment = (char *) NULL;

  cinfo.err               = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit     = xv_error_exit;
  jerr.pub.output_message = xv_error_output;

  if (setjmp(jerr.setjmp_buffer)) {
    /* if we're here, it blowed up... */
    jpeg_destroy_compress(&cinfo);
    if (picComments && comment) free(comment);
    return 1;
  }


  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, fp);

  cinfo.image_width  = w;
  cinfo.image_height = h;
  if (coltype == F_GREYSCALE) {
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
  }
  else {
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
  }

  bperpix = cinfo.input_components;


  prog.progress_monitor = xv_prog_meter;
  cinfo.progress = &prog;


  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, (int)qDial.val, TRUE);
  cinfo.smoothing_factor = (int)smDial.val;


  jpeg_start_compress(&cinfo, TRUE);


  /*** COMMENT HANDLING ***/

  sprintf(xvcmt, "%sXV %s  Quality = %d, Smoothing = %d\n",
	  CREATOR_STR, REVDATE, (int)qDial.val, (int)smDial.val);

  if (picComments) {   /* append XV comment */
    char *sp, *sp1;  int done;

    i   = strlen(picComments);
    comment = (char *) malloc(i + strlen(xvcmt) + 2 + 1);
    if (!comment) FatalError("out of memory in writeJFIF()");

    strcpy(comment, picComments);

    /* see if there's a line that starts with 'CREATOR: ' in the
       comments.  If there is, rip it out. */

    sp = comment;  done = 0;
    while (!done && *sp) {
      if (strncmp(sp, CREATOR_STR, strlen(CREATOR_STR)) == 0) {
	sp1 = sp;
	while (*sp1 && *sp1 != '\n') sp1++;    /* find end of this line */
	if (*sp1 == '\n') sp1++;               /* move past \n */

	/* move comments from sp1 and on down to sp */
	xvbcopy(sp1, sp, strlen(sp1) + 1);   /* +1 to copy the trailing \0 */

	done = 1;
      }
      else {   /* skip ahead to next line */
	while (*sp && *sp != '\n') sp++;
	if (*sp == '\n') sp++;
      }
    }

    /* count # of \n's at end of comment.
       If none, add 2.   If one, add 1.  If two or more, add none. */

    sp = comment + strlen(comment);
    for (i=0; i<3 && ((size_t) i < strlen(comment)); i++) {
      sp--;
      if (*sp != '\n') break;
    }

    for ( ; i<2; i++) strcat(comment, "\n");
    strcat(comment, xvcmt);
  }
  else comment = xvcmt;


  jpeg_write_marker(&cinfo, JPEG_COM, (byte *)comment, (u_int)strlen(comment));

  if (picExifInfo) jpeg_write_marker(&cinfo, JPEG_APP1, (byte *)picExifInfo,
                                     (u_int)picExifInfoSize);

  while (cinfo.next_scanline < cinfo.image_height) {
    rowptr[0] = (JSAMPROW) &pic[cinfo.next_scanline * w * bperpix];
    (void) jpeg_write_scanlines(&cinfo, rowptr, (JDIMENSION) 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  return 0;
}





/*******************************************/
void
VersionInfoJPEG()	/* GRR 19980605, 19980607 */
{
  int major = JPEG_LIB_VERSION / 10;
  int minor = JPEG_LIB_VERSION % 10;
  char minoralpha[2];

  if (minor) {
    minoralpha[0] = (char)(minor - 1 + 'a');
    minoralpha[1] = '\0';
  } else
    minoralpha[0] = '\0';

/* fprintf(stderr, "   Compiled with libjpeg %d.%d.\n", major, minor); */
  fprintf(stderr, "   Compiled with libjpeg %d%s.\n", major, minoralpha);
}





#endif  /* HAVE_JPEG */
