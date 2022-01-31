/*
 * xvtiff.c - load routine for 'TIFF' format pictures
 *
 * LoadTIFF(fname, numcols, quick)  -  load a TIFF file
 */

#ifndef va_start
#  define NEEDSARGS
#endif

#include "xv.h"

#ifdef HAVE_TIFF

#include "tiffio.h"     /* has to be after xv.h, as it needs varargs/stdarg */


/* Portions fall under the following copyright:
 *
 * Copyright (c) 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1992, 1993, 1994 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */


static int   copyTiff    PARM((TIFF *, char *));
static int   cpStrips    PARM((TIFF *, TIFF *));
static int   cpTiles     PARM((TIFF *, TIFF *));
static byte *loadPalette PARM((TIFF *, uint32, uint32, int, int, PICINFO *));
static byte *loadColor   PARM((TIFF *, uint32, uint32, int, int, PICINFO *));
static int   loadImage   PARM((TIFF *, uint32, uint32, byte *, int));
static void  _TIFFerr    PARM((const char *, const char *, va_list));
static void  _TIFFwarn   PARM((const char *, const char *, va_list));

static long  filesize;
static byte *rmap, *gmap, *bmap;
static const char *filename;

static int   error_occurred;


/*******************************************/
int LoadTIFF(fname, pinfo, quick)
     char    *fname;
     PICINFO *pinfo;
     int      quick;
/*******************************************/
{
  /* returns '1' on success, '0' on failure */

  TIFF  *tif;
  uint32 w, h;
  float  xres, yres;
  short	 bps, spp, photo, orient;
  FILE  *fp;
  byte  *pic8;
  char  *desc, oldpath[MAXPATHLEN+1], tmppath[MAXPATHLEN+1], *sp;
  char   tmp[256], tmpname[256];
  int    i, nump;

  error_occurred = 0;

  pinfo->type = PIC8;

  TIFFSetErrorHandler(_TIFFerr);
  TIFFSetWarningHandler(_TIFFwarn);

  /* open the stream to find out filesize (for info box) */
  fp = fopen(fname,"r");
  if (!fp) {
    TIFFError("LoadTIFF()", "couldn't open file");
    return 0;
  }

  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fclose(fp);



  rmap = pinfo->r;  gmap = pinfo->g;  bmap = pinfo->b;

  /* a kludge:  temporarily cd to the directory that the file is in (if
     the file has a path), and cd back when done, as I can't make the
     various TIFF error messages print the simple filename */

  filename = fname;    /* use fullname unless it all works out */
  oldpath[0] = '\0';
  if (fname[0] == '/') {
    xv_getwd(oldpath, sizeof(oldpath));
    strcpy(tmppath, fname);
    sp = (char *) BaseName(tmppath);  /* intentionally losing constness */
    if (sp != tmppath) {
      sp[-1] = '\0';     /* truncate before last '/' char */
      if (chdir(tmppath)) {
	oldpath[0] = '\0';
      }
      else filename = BaseName(fname);
    }
  }


  nump = 1;

  if (!quick) {
    /* see if there's more than 1 image in tiff file, to determine if we
       should do multi-page thing... */

    tif = TIFFOpen(filename, "r");
    if (!tif) return 0;
    while (TIFFReadDirectory(tif)) ++nump;
    TIFFClose(tif);
    if (DEBUG)
      fprintf(stderr,"LoadTIFF: %d page%s found\n", nump, nump==1 ? "" : "s");


    /* if there are multiple images, copy them out to multiple tmp files,
       and load the first one... */

    if (nump>1) {
      TIFF *in;

      /* GRR 20050320:  converted this fake mktemp() to use mktemp()/mkstemp()
         internally (formerly it simply prepended tmpdir to the string and
         returned immediately) */
      xv_mktemp(tmpname, "xvpgXXXXXX");

      if (tmpname[0] == '\0') {   /* mktemp() or mkstemp() blew up */
        sprintf(dummystr,"LoadTIFF: Unable to create temporary filename???");
        ErrPopUp(dummystr, "\nHow unlikely!");
        return 0;
      }

      /* GRR 20070506:  could clean up unappended tmpname-file here (Linux
         bug?), but "cleaner" (more general) to do so in KillPageFiles() */

      in = TIFFOpen(filename, "r");
      if (!in) return 0;
      for (i=1; i<=nump; i++) {
	sprintf(tmp, "%s%d", tmpname, i);
	if (!copyTiff(in, tmp)) {
	  SetISTR(ISTR_WARNING, "LoadTIFF:  Error writing page files!");
	  break;
	}

	if (!TIFFReadDirectory(in)) break;
      }
      TIFFClose(in);
      if (DEBUG)
	fprintf(stderr,"LoadTIFF: %d page%s written\n",
		i-1, (i-1)==1 ? "" : "s");

      sprintf(tmp, "%s%d", tmpname, 1);           /* start with page #1 */
      filename = tmp;
    }
  }  /* if (!quick) ... */


  tif = TIFFOpen(filename, "r");
  if (!tif) return 0;

  /* flip orientation so that image comes in X order */
  TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orient);
  switch (orient) {
  case ORIENTATION_TOPLEFT:
  case ORIENTATION_TOPRIGHT:
  case ORIENTATION_LEFTTOP:
  case ORIENTATION_RIGHTTOP:   orient = ORIENTATION_BOTLEFT;   break;

  case ORIENTATION_BOTRIGHT:
  case ORIENTATION_BOTLEFT:
  case ORIENTATION_RIGHTBOT:
  case ORIENTATION_LEFTBOT:    orient = ORIENTATION_TOPLEFT;   break;
  }

  TIFFSetField(tif, TIFFTAG_ORIENTATION, orient);

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
  TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bps);
  TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photo);
  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
  if ((TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres) == 1) &&
      (TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres) == 1)) {
    normaspect = yres / xres;
    if (DEBUG) fprintf(stderr,"TIFF aspect = %f\n", normaspect);
  }

  if (spp == 1) {
      pic8 = loadPalette(tif, w, h, photo, bps, pinfo);
  } else {
      pic8 = loadColor(tif, w, h, photo, bps, pinfo);
  }

  /* try to get comments, if any */
  pinfo->comment = (char *) NULL;

  desc = (char *) NULL;

  TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &desc);
  if (desc && strlen(desc) > (size_t) 0) {
    /* kludge:  tiff library seems to return bizarre comments */
    if (strlen(desc)==4 && strcmp(desc, "\367\377\353\370")==0) {}
    else {
      pinfo->comment = (char *) malloc(strlen(desc) + 1);
      if (pinfo->comment) strcpy(pinfo->comment, desc);
    }
  }

  TIFFClose(tif);

  /* un-kludge */
  if (oldpath[0] != '\0') chdir(oldpath);


  if (error_occurred) {
    if (pic8) free(pic8);
    if (pinfo->comment) free(pinfo->comment);
    pinfo->comment = (char *) NULL;
    if (!quick && nump>1) KillPageFiles(tmpname, nump);
    SetCursors(-1);
    return 0;
  }


  pinfo->pic = pic8;
  pinfo->w = w;  pinfo->h = h;
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
  pinfo->frmType = F_TIFF;

  if (nump>1) strcpy(pinfo->pagebname, tmpname);
  pinfo->numpages = nump;

  if (pinfo->pic) return 1;


  /* failed.  if we malloc'd a comment, free it */
  if (pinfo->comment) free(pinfo->comment);
  pinfo->comment = (char *) NULL;

  if (!quick && nump>1) KillPageFiles(tmpname, nump);
  SetCursors(-1);

  return 0;
}




/*******************************************/

#define CopyField(tag, v) \
  if (TIFFGetField(in, tag, &v))            TIFFSetField(out, tag, v)
#define CopyField2(tag, v1, v2) \
  if (TIFFGetField(in, tag, &v1, &v2))      TIFFSetField(out, tag, v1, v2)
#define CopyField3(tag, v1, v2, v3) \
  if (TIFFGetField(in, tag, &v1, &v2, &v3)) TIFFSetField(out, tag, v1, v2, v3)


/*******************************************/
static int copyTiff(in, fname)
     TIFF *in;
     char *fname;
{
  /* copies tiff (sub)image to given filename.  (Used only for multipage
     images.)  Returns 0 on error */

  TIFF   *out;
  short   bitspersample, samplesperpixel, shortv, *shortav;
  uint32  w, l;
  float   floatv, *floatav;
  char   *stringv;
  uint32  longv;
  uint16 *red, *green, *blue, shortv2;
  int     rv;

  out = TIFFOpen(fname, "w");
  if (!out) return 0;

  if (TIFFGetField(in, TIFFTAG_COMPRESSION, &shortv)){
    /* Currently, the TIFF Library cannot correctly copy TIFF version 6.0 (or
     * earlier) files that use "old" JPEG compression, so don't even try. */
    if (shortv == COMPRESSION_OJPEG) return 0;
    TIFFSetField(out, TIFFTAG_COMPRESSION, shortv);
  }
  CopyField (TIFFTAG_SUBFILETYPE,         longv);
  CopyField (TIFFTAG_TILEWIDTH,           w);
  CopyField (TIFFTAG_TILELENGTH,          l);
  CopyField (TIFFTAG_IMAGEWIDTH,          w);
  CopyField (TIFFTAG_IMAGELENGTH,         l);
  CopyField (TIFFTAG_BITSPERSAMPLE,       bitspersample);
  CopyField (TIFFTAG_PREDICTOR,           shortv);
  CopyField (TIFFTAG_PHOTOMETRIC,         shortv);
  CopyField (TIFFTAG_THRESHHOLDING,       shortv);
  CopyField (TIFFTAG_FILLORDER,           shortv);
  CopyField (TIFFTAG_ORIENTATION,         shortv);
  CopyField (TIFFTAG_SAMPLESPERPIXEL,     samplesperpixel);
  CopyField (TIFFTAG_MINSAMPLEVALUE,      shortv);
  CopyField (TIFFTAG_MAXSAMPLEVALUE,      shortv);
  CopyField (TIFFTAG_XRESOLUTION,         floatv);
  CopyField (TIFFTAG_YRESOLUTION,         floatv);
  CopyField (TIFFTAG_GROUP3OPTIONS,       longv);
  CopyField (TIFFTAG_GROUP4OPTIONS,       longv);
  CopyField (TIFFTAG_RESOLUTIONUNIT,      shortv);
  CopyField (TIFFTAG_PLANARCONFIG,        shortv);
  CopyField (TIFFTAG_ROWSPERSTRIP,        longv);
  CopyField (TIFFTAG_XPOSITION,           floatv);
  CopyField (TIFFTAG_YPOSITION,           floatv);
  CopyField (TIFFTAG_IMAGEDEPTH,          longv);
  CopyField (TIFFTAG_TILEDEPTH,           longv);
  CopyField2(TIFFTAG_EXTRASAMPLES,        shortv, shortav);
  CopyField3(TIFFTAG_COLORMAP,            red, green, blue);
  CopyField2(TIFFTAG_PAGENUMBER,          shortv, shortv2);
  CopyField (TIFFTAG_ARTIST,              stringv);
  CopyField (TIFFTAG_IMAGEDESCRIPTION,    stringv);
  CopyField (TIFFTAG_MAKE,                stringv);
  CopyField (TIFFTAG_MODEL,               stringv);
  CopyField (TIFFTAG_SOFTWARE,            stringv);
  CopyField (TIFFTAG_DATETIME,            stringv);
  CopyField (TIFFTAG_HOSTCOMPUTER,        stringv);
  CopyField (TIFFTAG_PAGENAME,            stringv);
  CopyField (TIFFTAG_DOCUMENTNAME,        stringv);
  CopyField2(TIFFTAG_JPEGTABLES,          longv, stringv);
  CopyField (TIFFTAG_YCBCRCOEFFICIENTS,   floatav);
  CopyField2(TIFFTAG_YCBCRSUBSAMPLING,    shortv,shortv2);
  CopyField (TIFFTAG_YCBCRPOSITIONING,    shortv);
  CopyField (TIFFTAG_REFERENCEBLACKWHITE, floatav);

  if (TIFFIsTiled(in)) rv = cpTiles (in, out);
                  else rv = cpStrips(in, out);

  TIFFClose(out);
  return rv;
}


/*******************************************/
static int cpStrips(in, out)
     TIFF *in, *out;
{
  tsize_t bufsize;
  byte *buf;

  bufsize = TIFFStripSize(in);
  if (bufsize <= 0) return 0;  /* tsize_t is signed */
  buf = (byte *) malloc((size_t) bufsize);
  if (buf) {
    tstrip_t s, ns = TIFFNumberOfStrips(in);
    uint32 *bytecounts;

    TIFFGetField(in, TIFFTAG_STRIPBYTECOUNTS, &bytecounts);
    for (s = 0; s < ns; s++) {
      if (bytecounts[s] > bufsize) {
	buf = (unsigned char *) realloc(buf, (size_t) bytecounts[s]);
	if (!buf) return (0);
	bufsize = bytecounts[s];
      }
      if (TIFFReadRawStrip (in,  s, buf, (tsize_t) bytecounts[s]) < 0 ||
	  TIFFWriteRawStrip(out, s, buf, (tsize_t) bytecounts[s]) < 0) {
	free(buf);
	return 0;
      }
    }
    free(buf);
    return 1;
  }
  return 0;
}


/*******************************/
static int cpTiles(in, out)
     TIFF *in, *out;
{
  tsize_t bufsize;
  byte   *buf;

  bufsize = TIFFTileSize(in);
  if (bufsize <= 0) return 0;  /* tsize_t is signed */
  buf = (unsigned char *) malloc((size_t) bufsize);
  if (buf) {
    ttile_t t, nt = TIFFNumberOfTiles(in);
    uint32 *bytecounts;

    TIFFGetField(in, TIFFTAG_TILEBYTECOUNTS, &bytecounts);
    for (t = 0; t < nt; t++) {
      if (bytecounts[t] > bufsize) {
	buf = (unsigned char *)realloc(buf, (size_t) bytecounts[t]);
	if (!buf) return (0);
	bufsize = bytecounts[t];
      }
      if (TIFFReadRawTile (in,  t, buf, (tsize_t) bytecounts[t]) < 0 ||
	  TIFFWriteRawTile(out, t, buf, (tsize_t) bytecounts[t]) < 0) {
	free(buf);
	return 0;
      }
    }
    free(buf);
    return 1;
  }
  return 0;
}


/*******************************************/
static byte *loadPalette(tif, w, h, photo, bps, pinfo)
     TIFF *tif;
     uint32 w,h;
     int   photo,bps;
     PICINFO *pinfo;
{
  byte *pic8;
  uint32 npixels;

  switch (photo) {
  case PHOTOMETRIC_PALETTE:
    pinfo->colType = F_FULLCOLOR;
    sprintf(pinfo->fullInfo, "TIFF, %u-bit, palette format.  (%ld bytes)",
	    bps, filesize);
    break;

  case PHOTOMETRIC_MINISWHITE:
  case PHOTOMETRIC_MINISBLACK:
    pinfo->colType = (bps==1) ? F_BWDITHER : F_GREYSCALE;
    sprintf(pinfo->fullInfo,"TIFF, %u-bit, %s format.  (%ld bytes)",
	    bps,
	    photo == PHOTOMETRIC_MINISWHITE ? "min-is-white" :
	    "min-is-black",
	    filesize);
    break;
  }

  sprintf(pinfo->shrtInfo, "%ux%u TIFF.",(u_int) w, (u_int) h);

  npixels = w*h;
  if (npixels/w != h) {
    /* SetISTR(ISTR_WARNING, "loadPalette() - image dimensions too large"); */
    TIFFError(filename, "Image dimensions too large");
    return (byte *) NULL;
  }

  pic8 = (byte *) malloc((size_t) npixels);
  if (!pic8) FatalError("loadPalette() - couldn't malloc 'pic8'");

  if (loadImage(tif, w, h, pic8, 0)) return pic8;

  return (byte *) NULL;
}


/*******************************************/
static byte *loadColor(tif, w, h, photo, bps, pinfo)
     TIFF *tif;
     uint32 w,h;
     int   photo,bps;
     PICINFO *pinfo;
{
  byte *pic24, *pic8;
  uint32 npixels, count;

  pinfo->colType = F_FULLCOLOR;
  sprintf(pinfo->fullInfo, "TIFF, %u-bit, %s format.  (%ld bytes)",
	  bps,
	  (photo == PHOTOMETRIC_RGB ?	"RGB" :
	   photo == PHOTOMETRIC_YCBCR ?	"YCbCr" :
	   "???"),
	  filesize);

  sprintf(pinfo->shrtInfo, "%ux%u TIFF.",(u_int) w, (u_int) h);

  npixels = w*h;
  count = 3*npixels;
  if (npixels/w != h || count/3 != npixels) {
    /* SetISTR(ISTR_WARNING, "loadPalette() - image dimensions too large"); */
    TIFFError(filename, "Image dimensions too large");
    return (byte *) NULL;
  }

  /* allocate 24-bit image */
  pic24 = (byte *) malloc((size_t) count);
  if (!pic24) FatalError("loadColor() - couldn't malloc 'pic24'");

  pic8 = (byte *) NULL;

  if (loadImage(tif, w, h, pic24, 0)) {
    pinfo->type = PIC24;
    pic8 = pic24;
  }
  else free(pic24);

  return pic8;
}


/*******************************************/
static void _TIFFerr(module, fmt, ap)
     const char *module;
     const char *fmt;
     va_list     ap;
{
  char buf[2048];
  char *cp = buf;

  if (module != NULL) {
    sprintf(cp, "%s: ", module);
    cp = (char *) index(cp, '\0');
  }

  vsprintf(cp, fmt, ap);
  strcat(cp, ".");

  SetISTR(ISTR_WARNING, "%s", buf);

  error_occurred = 1;
}


/*******************************************/
static void _TIFFwarn(module, fmt, ap)
     const char *module;
     const char *fmt;
     va_list     ap;
{
  char buf[2048];
  char *cp = buf;

  if (module != NULL) {
    sprintf(cp, "%s: ", module);
    cp = (char *) index(cp, '\0');
  }
  strcpy(cp, "Warning, ");
  cp = (char *) index(cp, '\0');
  vsprintf(cp, fmt, ap);
  strcat(cp, ".");

  SetISTR(ISTR_WARNING, "%s", buf);
}






typedef	byte RGBvalue;

static	u_short bitspersample;
static	u_short samplesperpixel;
static	u_short photometric;
static	u_short orientation;

/* colormap for pallete images */
static	u_short *redcmap, *greencmap, *bluecmap;
static	int      stoponerr;			/* stop on read error */

/* YCbCr support */
static	u_short YCbCrHorizSampling;
static	u_short YCbCrVertSampling;
static	float   *YCbCrCoeffs;
static	float   *refBlackWhite;

static	byte **BWmap;
static	byte **PALmap;

/* XXX Work around some collisions with the new library. */
#define tileContigRoutine _tileContigRoutine
#define tileSeparateRoutine _tileSeparateRoutine

typedef void (*tileContigRoutine)   PARM((byte*, u_char*, RGBvalue*,
					  uint32, uint32, int, int));

typedef void (*tileSeparateRoutine) PARM((byte*, u_char*, u_char*, u_char*,
                                         RGBvalue*, uint32, uint32, int, int));


static int    checkcmap             PARM((int, u_short*, u_short*, u_short*));

static int    gt                       PARM((TIFF *, uint32, uint32, byte *));
static uint32 setorientation           PARM((TIFF *, uint32));
static int    gtTileContig             PARM((TIFF *, byte *, RGBvalue *,
					     uint32, uint32, int));
static int    gtTileSeparate           PARM((TIFF *, byte *, RGBvalue *,
					     uint32, uint32, int));
static int    gtStripContig            PARM((TIFF *, byte *, RGBvalue *,
					     uint32, uint32, int));
static int    gtStripSeparate          PARM((TIFF *, byte *, RGBvalue *,
					     uint32, uint32, int));

static int    makebwmap                PARM((void));
static int    makecmap                 PARM((void));

static void   put8bitcmaptile          PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put4bitcmaptile          PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put2bitcmaptile          PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put1bitcmaptile          PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   putgreytile              PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put1bitbwtile            PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put2bitbwtile            PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put4bitbwtile            PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));
static void   put16bitbwtile           PARM((byte *, u_short *, RGBvalue *,
					     uint32, uint32, int, int));

static void   putRGBcontig8bittile     PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));

static void   putRGBcontig16bittile    PARM((byte *, u_short *, RGBvalue *,
					     uint32, uint32, int, int));

static void   putRGBseparate8bittile   PARM((byte *, u_char *, u_char *,
					     u_char *, RGBvalue *,
					     uint32, uint32, int, int));

static void   putRGBseparate16bittile  PARM((byte *, u_short *, u_short *,
					    u_short *, RGBvalue *,
					    uint32, uint32, int, int));


static void   initYCbCrConversion      PARM((void));

static void   putRGBContigYCbCrClump   PARM((byte *, u_char *, int, int,
					     uint32, int, int, int));

static void   putRGBSeparateYCbCrClump PARM((byte *, u_char *, u_char *,
					     u_char *, int, int, uint32, int,
					     int, int));
  
static void   putRGBSeparate16bitYCbCrClump PARM((byte *, u_short *, u_short *,
						  u_short *, int, int, uint32,
						  int, int, int));

static void   putcontig8bitYCbCrtile   PARM((byte *, u_char *, RGBvalue *,
					     uint32, uint32, int, int));

static void   putYCbCrseparate8bittile PARM((byte *, u_char *, u_char *, 
					     u_char *, RGBvalue *, 
					     uint32, uint32, int, int));

static void   putYCbCrseparate16bittile PARM((byte *, u_short *, u_short *, 
					      u_short *, RGBvalue *, 
					      uint32, uint32, int, int));

static tileContigRoutine   pickTileContigCase   PARM((RGBvalue *));
static tileSeparateRoutine pickTileSeparateCase PARM((RGBvalue *));


/*******************************************/
static int loadImage(tif, rwidth, rheight, raster, stop)
     TIFF *tif;
     uint32 rwidth, rheight;
     byte *raster;
     int stop;
{
  int ok;
  uint32 width, height;

  TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample);
  switch (bitspersample) {
  case 1:
  case 2:
  case 4:
  case 8:
  case 16:  break;

  default:
    TIFFError(TIFFFileName(tif),
	      "Sorry, cannot handle %d-bit pictures", bitspersample);
    return (0);
  }


  TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
  switch (samplesperpixel) {
  case 1:
  case 3:
  case 4:  break;

  default:
    TIFFError(TIFFFileName(tif),
	      "Sorry, cannot handle %d-channel images", samplesperpixel);
    return (0);
  }


  if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric)) {
    switch (samplesperpixel) {
    case 1:  photometric = PHOTOMETRIC_MINISBLACK;   break;

    case 3:
    case 4:  photometric = PHOTOMETRIC_RGB;          break;

    default:
      TIFFError(TIFFFileName(tif),
		"Missing needed \"PhotometricInterpretation\" tag");
      return (0);
    }

    TIFFWarning(TIFFFileName(tif),
	      "No \"PhotometricInterpretation\" tag, assuming %s\n",
	      photometric == PHOTOMETRIC_RGB ? "RGB" : "min-is-black");
  }

  /* XXX maybe should check photometric? */
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

  /* XXX verify rwidth and rheight against width and height */
  stoponerr = stop;
  BWmap = NULL;
  PALmap = NULL;
  ok = gt(tif, rwidth, height, raster + (rheight-height)*rwidth);
  if (BWmap)
    free((char *)BWmap);
  if (PALmap)
    free((char *)PALmap);
  return (ok);
}


/*******************************************/
static int checkcmap(n, r, g, b)
     int n;
     u_short *r, *g, *b;
{
  while (n-- >= 0)
    if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256) return (16);

  TIFFWarning(filename, "Assuming 8-bit colormap");
  return (8);
}


/*******************************************/
static int gt(tif, w, h, raster)
     TIFF   *tif;
     uint32 w, h;
     byte   *raster;
{
#ifdef USE_LIBJPEG_FOR_TIFF_YCbCr_RGB_CONVERSION
  u_short compression;
#endif
  u_short minsamplevalue, maxsamplevalue, planarconfig;
  RGBvalue *Map;
  int bpp = 1, e;
  int x, range;

#ifdef USE_LIBJPEG_FOR_TIFF_YCbCr_RGB_CONVERSION
  TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
#endif
  TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconfig);
  TIFFGetFieldDefaulted(tif, TIFFTAG_MINSAMPLEVALUE, &minsamplevalue);
  TIFFGetFieldDefaulted(tif, TIFFTAG_MAXSAMPLEVALUE, &maxsamplevalue);
  Map = NULL;

  switch (photometric) {
  case PHOTOMETRIC_YCBCR:
#ifdef USE_LIBJPEG_FOR_TIFF_YCbCr_RGB_CONVERSION
    if (compression == COMPRESSION_JPEG
#ifdef LIBTIFF_HAS_OLDJPEG_SUPPORT
                                        || compression == COMPRESSION_OJPEG
#endif
                                                                            ) {
      /* FIXME:  Remove the following test as soon as TIFF Library is fixed!
       *   (Currently [June 2002] this requires supporting patches in both
       *   tif_ojpeg.c and tif_jpeg.c in order to support subsampled YCbCr
       *   images having separated color planes.) */
      if (planarconfig == PLANARCONFIG_CONTIG) {
        /* can rely on libjpeg to convert to RGB (assuming newer libtiff,
         * compiled with appropriate forms of JPEG support) */
        TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        photometric = PHOTOMETRIC_RGB;
      } else {
        TIFFError(filename, "Cannot handle format");
        return (0);
      }
    } else
#endif // USE_LIBJPEG_FOR_TIFF_YCbCr_RGB_CONVERSION
    {
      TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRCOEFFICIENTS, &YCbCrCoeffs);
      TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRSUBSAMPLING,
			    &YCbCrHorizSampling, &YCbCrVertSampling);

      /* According to the TIFF specification, if no "ReferenceBlackWhite"
       * tag is present in the input file, "TIFFGetFieldDefaulted()" returns
       * default reference black and white levels suitable for PHOTOMETRIC_RGB;
       * namely:  <0,255,0,255,0,255>.  But for PHOTOMETRIC_YCBCR in JPEG
       * images, the usual default (e.g., corresponding to the behavior of the
       * IJG libjpeg) is:  <0,255,128,255,128,255>.  Since libtiff doesn't have
       * a clean, standard interface for making this repair, the following
       * slightly dirty code installs the default.  --Scott Marovich,
       * Hewlett-Packard Labs, 9/2001.
       */
      if (!TIFFGetField(tif, TIFFTAG_REFERENCEBLACKWHITE, &refBlackWhite)) {
        TIFFGetFieldDefaulted(tif, TIFFTAG_REFERENCEBLACKWHITE, &refBlackWhite);
        refBlackWhite[4] = refBlackWhite[2] = 1 << (bitspersample - 1);
      }
      TIFFGetFieldDefaulted(tif, TIFFTAG_REFERENCEBLACKWHITE, &refBlackWhite);
      initYCbCrConversion();
    }
    /* fall thru... */

  case PHOTOMETRIC_RGB:
    bpp *= 3;
    if (minsamplevalue == 0 && maxsamplevalue == 255)
      break;

    /* fall thru... */
  case PHOTOMETRIC_MINISBLACK:
  case PHOTOMETRIC_MINISWHITE:
    range = maxsamplevalue - minsamplevalue;
    Map = (RGBvalue *)malloc((range + 1) * sizeof (RGBvalue));
    if (Map == NULL) {
      TIFFError(filename, "No space for photometric conversion table");
      return (0);
    }

    if (photometric == PHOTOMETRIC_MINISWHITE) {
      for (x = 0; x <= range; x++)
	Map[x] = (255*(range-x))/range;
    } else {
      for (x = 0; x <= range; x++)
	Map[x] = (255*x)/range;
    }

    if (range<256) {
      for (x=0; x<=range; x++) rmap[x] = gmap[x] = bmap[x] = Map[x];
    } else {
      for (x=0; x<256; x++)
	rmap[x] = gmap[x] = bmap[x] = Map[(range*x)/255];
    }

    if (photometric != PHOTOMETRIC_RGB && bitspersample <= 8) {
      /*
       * Use photometric mapping table to construct
       * unpacking tables for samples <= 8 bits.
       */
      if (!makebwmap())
	return (0);
      /* no longer need Map, free it */
      free((char *)Map);
      Map = NULL;
    }
    break;

  case PHOTOMETRIC_PALETTE:
    if (!TIFFGetField(tif, TIFFTAG_COLORMAP,
		      &redcmap, &greencmap, &bluecmap)) {
      TIFFError(filename, "Missing required \"Colormap\" tag");
      return (0);
    }

    /*
     * Convert 16-bit colormap to 8-bit (unless it looks
     * like an old-style 8-bit colormap).
     */
    range = (1<<bitspersample)-1;
    if (checkcmap(range, redcmap, greencmap, bluecmap) == 16) {

#define	CVT(x)		((((int) x) * 255) / ((1L<<16)-1))

      for (x = range; x >= 0; x--) {
	rmap[x] = CVT(redcmap[x]);
	gmap[x] = CVT(greencmap[x]);
	bmap[x] = CVT(bluecmap[x]);
      }
    } else {
      for (x = range; x >= 0; x--) {
	rmap[x] = redcmap[x];
	gmap[x] = greencmap[x];
	bmap[x] = bluecmap[x];
      }
    }

    if (bitspersample <= 8) {
      /*
       * Use mapping table to construct
       * unpacking tables for samples < 8 bits.
       */
      if (!makecmap())
	return (0);
    }
    break;

  default:
    TIFFError(filename, "Unknown photometric tag %u", photometric);
    return (0);
  }

  if (planarconfig == PLANARCONFIG_SEPARATE && samplesperpixel > 1) {
    e = TIFFIsTiled(tif) ? gtTileSeparate (tif, raster, Map, h, w, bpp) :
                           gtStripSeparate(tif, raster, Map, h, w, bpp);
  } else {
    e = TIFFIsTiled(tif) ? gtTileContig (tif, raster, Map, h, w, bpp) :
                           gtStripContig(tif, raster, Map, h, w, bpp);
  }

  if (Map) free((char *)Map);
  return (e);
}


/*******************************************/
static uint32 setorientation(tif, h)
     TIFF *tif;
     uint32 h;
{
  /* note that orientation was flipped in LoadTIFF() (near line 175) */

  uint32 y;

  TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
  switch (orientation) {
  case ORIENTATION_BOTRIGHT:
  case ORIENTATION_RIGHTBOT:	/* XXX */
  case ORIENTATION_LEFTBOT:	/* XXX */
    TIFFWarning(filename, "using bottom-left orientation");
    orientation = ORIENTATION_BOTLEFT;

    /* fall thru... */
  case ORIENTATION_BOTLEFT:
    y = 0;
    break;

  case ORIENTATION_TOPRIGHT:
  case ORIENTATION_RIGHTTOP:	/* XXX */
  case ORIENTATION_LEFTTOP:	/* XXX */
  default:
    TIFFWarning(filename, "using top-left orientation");
    orientation = ORIENTATION_TOPLEFT;
    /* fall thru... */
  case ORIENTATION_TOPLEFT:
    /* GRR 20050319:  This may be wrong for tiled images (also stripped?);
     *   looks like we want to return th-1 instead of h-1 in at least some
     *   cases.  For now, just added quick hack (USE_TILED_TIFF_BOTLEFT_FIX)
     *   to gtTileContig().  (Note that, as of libtiff 3.7.1, tiffcp still
     *   has exactly the same bug.) */
    y = h-1;
    break;
  }
  return (y);
}




/*
 * Get a tile-organized image that has
 *	PlanarConfiguration contiguous if SamplesPerPixel > 1
 * or
 *	SamplesPerPixel == 1
 */
/*******************************************/
static int gtTileContig(tif, raster, Map, h, w, bpp)
     TIFF *tif;
     byte *raster;
     RGBvalue *Map;
     uint32 h, w;
     int bpp;
{
  uint32 col, row, y;
  uint32 tw, th;
  u_char *buf;
  int fromskew, toskew;
  u_int nrow;
  tileContigRoutine put;
  tsize_t bufsize;

  put = pickTileContigCase(Map);
  if (put == 0) return (0);

  bufsize = TIFFTileSize(tif);
  if (bufsize <= 0) return 0;  /* tsize_t is signed */
  buf = (u_char *) malloc((size_t) bufsize);
  if (buf == 0) {
    TIFFError(filename, "No space for tile buffer");
    return (0);
  }

  TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
  TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
  y = setorientation(tif, h);
#ifdef USE_TILED_TIFF_BOTLEFT_FIX  /* image _originally_ ORIENTATION_BOTLEFT */
  /* this fix causes tiles as a whole to be placed starting at the top,
   * regardless of orientation; the only difference is what happens within
   * a given tile (see toskew, below) */
  /* GRR FIXME:  apply globally in setorientation()? */
  if (orientation == ORIENTATION_TOPLEFT)
    y = th-1;
#endif
  /* toskew causes individual tiles to copy from bottom to top for
   * ORIENTATION_TOPLEFT and from top to bottom otherwise */
  toskew = (orientation == ORIENTATION_TOPLEFT ? -tw + -w : -tw + w);

  for (row = 0; row < h; row += th) {
    nrow = (row + th > h ? h - row : th);
    for (col = 0; col < w; col += tw) {
      /*
       * This reads the tile at (col,row) into buf.  "The data placed in buf
       * are returned decompressed and, typically, in the native byte- and
       * bit-ordering, but are otherwise packed."
       */
      if (TIFFReadTile(tif, buf, (uint32)col, (uint32)row, 0, 0) < 0
	  && stoponerr) break;

      if (col + tw > w) {
	/*
	 * Tile is clipped horizontally.  Calculate
	 * visible portion and skewing factors.
	 */
	uint32 npix = w - col;
	fromskew = tw - npix;
	(*put)(raster + (y*w + col)*bpp, buf, Map, npix, (uint32) nrow,
	       fromskew, (int) ((toskew + fromskew)*bpp) );
      } else
	(*put)(raster + (y*w + col)*bpp, buf, Map, tw,   (uint32) nrow,
	       0, (int) (toskew*bpp));
    }

#ifdef USE_TILED_TIFF_BOTLEFT_FIX  /* image _originally_ ORIENTATION_BOTLEFT */
    y += nrow;
#else
    y += (orientation == ORIENTATION_TOPLEFT ? -nrow : nrow);
#endif
  }
  free(buf);
  return (1);
}




/*
 * Get a tile-organized image that has
 *	 SamplesPerPixel > 1
 *	 PlanarConfiguration separated
 * We assume that all such images are RGB.
 */

/*******************************************/
static int gtTileSeparate(tif, raster, Map, h, w, bpp)
     TIFF *tif;
     byte *raster;
     RGBvalue *Map;
     uint32 h, w;
     int bpp;
{
  uint32 tw, th;
  uint32 col, row, y;
  u_char *buf;
  u_char *r, *g, *b;
  tsize_t tilesize;
  uint32 bufsize;
  int fromskew, toskew;
  u_int nrow;
  tileSeparateRoutine put;

  put = pickTileSeparateCase(Map);
  if (put == 0) return (0);

  tilesize = TIFFTileSize(tif);
  bufsize = 3*tilesize;
  if (tilesize <= 0 || bufsize/3 != tilesize) {  /* tsize_t is signed */
    TIFFError(filename, "Image dimensions too large");
    return 0;
  }
  buf = (u_char *) malloc((size_t) bufsize);
  if (buf == 0) {
    TIFFError(filename, "No space for tile buffer");
    return (0);
  }
  r = buf;
  g = r + tilesize;
  b = g + tilesize;
  TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
  TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
  y = setorientation(tif, h);
  toskew = (orientation == ORIENTATION_TOPLEFT ? -tw + -w : -tw + w);

  for (row = 0; row < h; row += th) {
    nrow = (row + th > h ? h - row : th);
    for (col = 0; col < w; col += tw) {
      tsample_t band;

      band = 0;
      if (TIFFReadTile(tif, r, (uint32) col, (uint32) row,0, band) < 0
	  && stoponerr) break;

      band = 1;
      if (TIFFReadTile(tif, g, (uint32) col, (uint32) row,0, band) < 0
	  && stoponerr) break;

      band = 2;
      if (TIFFReadTile(tif, b, (uint32) col, (uint32) row,0, band) < 0
	  && stoponerr) break;

      if (col + tw > w) {
	/*
	 * Tile is clipped horizontally.  Calculate
	 * visible portion and skewing factors.
	 */
	uint32 npix = w - col;
	fromskew = tw - npix;
	(*put)(raster + (y*w + col)*bpp, r, g, b, Map, npix, (uint32) nrow,
	       fromskew, (int) ((toskew + fromskew)*bpp));
      } else
	(*put)(raster + (y*w + col)*bpp, r, g, b, Map, tw, (uint32) nrow,
	       0, (int) (toskew*bpp));
    }
    y += (orientation == ORIENTATION_TOPLEFT ? -nrow : nrow);
  }
  free(buf);
  return (1);
}

/*
 * Get a strip-organized image that has
 *	PlanarConfiguration contiguous if SamplesPerPixel > 1
 * or
 *	SamplesPerPixel == 1
 */
/*******************************************/
static int gtStripContig(tif, raster, Map, h, w, bpp)
     TIFF *tif;
     byte *raster;
     RGBvalue *Map;
     uint32 h, w;
     int bpp;
{
  uint32 row, y, nrow;
  u_char *buf;
  tileContigRoutine put;
  uint32 rowsperstrip;
  uint32 imagewidth;
  int scanline;
  int fromskew, toskew;
  tsize_t bufsize;

  put = pickTileContigCase(Map);
  if (put == 0)
    return (0);

  bufsize = TIFFStripSize(tif);
  if (bufsize <= 0) return 0;  /* tsize_t is signed */
  buf = (u_char *) malloc((size_t) bufsize);
  if (buf == 0) {
    TIFFError(filename, "No space for strip buffer");
    return (0);
  }
  y = setorientation(tif, h);
  toskew = (orientation == ORIENTATION_TOPLEFT ? -w + -w : -w + w);
  TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imagewidth);
  scanline = TIFFScanlineSize(tif);
  fromskew = (w < imagewidth ? imagewidth - w : 0);
  for (row = 0; row < h; row += rowsperstrip) {
    nrow = (row + rowsperstrip > h ? h - row : rowsperstrip);
    if (TIFFReadEncodedStrip(tif,
			     TIFFComputeStrip(tif, (uint32) row,(tsample_t) 0),
			     (tdata_t) buf, (tsize_t)(nrow*scanline)) < 0
	&& stoponerr) break;

    (*put)(raster + y*w*bpp, buf, Map, w, nrow, fromskew, toskew*bpp);

    y += (orientation == ORIENTATION_TOPLEFT ? -nrow : nrow);
  }
  free(buf);
  return (1);
}


/*
 * Get a strip-organized image with
 *	 SamplesPerPixel > 1
 *	 PlanarConfiguration separated
 * We assume that all such images are RGB.
 */
static int gtStripSeparate(tif, raster, Map, h, w, bpp)
     TIFF *tif;
     byte *raster;
     register RGBvalue *Map;
     uint32 h, w;
     int bpp;
{
  uint32 nrow, row, y;
  u_char *buf;
  u_char *r, *g, *b;
  tsize_t stripsize;
  uint32 bufsize;
  int fromskew, toskew;
  int scanline;
  tileSeparateRoutine put;
  uint32 rowsperstrip;
  uint32 imagewidth;

  stripsize = TIFFStripSize(tif);
  bufsize = 3*stripsize;
  if (stripsize <= 0 || bufsize/3 != stripsize) {  /* tsize_t is signed */
    TIFFError(filename, "Image dimensions too large");
    return 0;
  }
  buf = (u_char *) malloc((size_t) bufsize);
  if (buf == 0) {
    TIFFError(filename, "No space for strip buffer");
    return (0);
  }
  r = buf;
  g = r + stripsize;
  b = g + stripsize;
  put = pickTileSeparateCase(Map);
  if (put == 0) {
    TIFFError(filename, "Cannot handle format");
    return (0);
  }
  y = setorientation(tif, h);
  toskew = (orientation == ORIENTATION_TOPLEFT ? -w + -w : -w + w);
  TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imagewidth);
  scanline = TIFFScanlineSize(tif);
  fromskew = (w < imagewidth ? imagewidth - w : 0);
  for (row = 0; row < h; row += rowsperstrip) {
    tsample_t band;

    nrow = (row + rowsperstrip > h ? h - row : rowsperstrip);
    band = 0;
    if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, (uint32) row, band),
			     (tdata_t) r, (tsize_t)(nrow*scanline)) < 0
	&& stoponerr) break;

    band = 1;
    if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, (uint32) row, band),
			     (tdata_t) g, (tsize_t)(nrow*scanline)) < 0
	&& stoponerr) break;

    band = 2;
    if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, (uint32) row, band),
			     (tdata_t) b, (tsize_t)(nrow*scanline)) < 0
	&& stoponerr) break;

    (*put)(raster + y*w*bpp, r, g, b, Map, w, nrow, fromskew, toskew*bpp);

    y += (orientation == ORIENTATION_TOPLEFT ? -nrow : nrow);
  }
  free(buf);
  return (1);
}


/*
 * Greyscale images with less than 8 bits/sample are handled
 * with a table to avoid lots of shifts and masks.  The table
 * is set up so that put*bwtile (below) can retrieve 8/bitspersample
 * pixel values simply by indexing into the table with one
 * number.
 */
static int makebwmap()
{
  register int i;
  int nsamples = 8 / bitspersample;
  register byte *p;

  BWmap = (byte **)malloc(
			  256*sizeof (byte *)+(256*nsamples*sizeof(byte)));
  if (BWmap == NULL) {
    TIFFError(filename, "No space for B&W mapping table");
    return (0);
  }
  p = (byte *)(BWmap + 256);
  for (i = 0; i < 256; i++) {
    BWmap[i] = p;
    switch (bitspersample) {
#define	GREY(x)	*p++ = x;
    case 1:
      GREY(i>>7);
      GREY((i>>6)&1);
      GREY((i>>5)&1);
      GREY((i>>4)&1);
      GREY((i>>3)&1);
      GREY((i>>2)&1);
      GREY((i>>1)&1);
      GREY(i&1);
      break;
    case 2:
      GREY(i>>6);
      GREY((i>>4)&3);
      GREY((i>>2)&3);
      GREY(i&3);
      break;
    case 4:
      GREY(i>>4);
      GREY(i&0xf);
      break;
    case 8:
      GREY(i);
      break;
    }
#undef	GREY
  }
  return (1);
}


/*
 * Palette images with <= 8 bits/sample are handled with
 * a table to avoid lots of shifts and masks.  The table
 * is set up so that put*cmaptile (below) can retrieve
 * (8/bitspersample) pixel-values simply by indexing into
 * the table with one number.
 */
static int makecmap()
{
  register int i;
  int nsamples = 8 / bitspersample;
  register byte *p;

  PALmap = (byte **)malloc(
			   256*sizeof (byte *)+(256*nsamples*sizeof(byte)));
  if (PALmap == NULL) {
    TIFFError(filename, "No space for Palette mapping table");
    return (0);
  }
  p = (byte *)(PALmap + 256);
  for (i = 0; i < 256; i++) {
    PALmap[i] = p;
#define	CMAP(x)	*p++ = x;
    switch (bitspersample) {
    case 1:
      CMAP(i>>7);
      CMAP((i>>6)&1);
      CMAP((i>>5)&1);
      CMAP((i>>4)&1);
      CMAP((i>>3)&1);
      CMAP((i>>2)&1);
      CMAP((i>>1)&1);
      CMAP(i&1);
      break;
    case 2:
      CMAP(i>>6);
      CMAP((i>>4)&3);
      CMAP((i>>2)&3);
      CMAP(i&3);
      break;
    case 4:
      CMAP(i>>4);
      CMAP(i&0xf);
      break;
    case 8:
      CMAP(i);
      break;
    }
#undef CMAP
  }
  return (1);
}


/*
 * The following routines move decoded data returned
 * from the TIFF library into rasters filled with packed
 * ABGR pixels (i.e., suitable for passing to lrecwrite.)
 *
 * The routines have been created according to the most
 * important cases and optimized.  pickTileContigCase and
 * pickTileSeparateCase analyze the parameters and select
 * the appropriate "put" routine to use.
 */

#define	REPEAT8(op)	REPEAT4(op); REPEAT4(op)
#define	REPEAT4(op)	REPEAT2(op); REPEAT2(op)
#define	REPEAT2(op)	op; op
#define	CASE8(x,op)				\
	switch (x) {				\
	case 7: op; case 6: op; case 5: op;	\
	case 4: op; case 3: op; case 2: op;	\
	case 1: op;				\
	}
#define	CASE4(x,op)	switch (x) { case 3: op; case 2: op; case 1: op; }

#define	UNROLL8(w, op1, op2) {		\
	uint32 x;	                \
	for (x = w; x >= 8; x -= 8) {	\
		op1;			\
		REPEAT8(op2);		\
	}				\
	if (x > 0) {			\
		op1;			\
		CASE8(x,op2);		\
	}				\
}

#define	UNROLL4(w, op1, op2) {		\
	uint32 x;		        \
	for (x = w; x >= 4; x -= 4) {	\
		op1;			\
		REPEAT4(op2);		\
	}				\
	if (x > 0) {			\
		op1;			\
		CASE4(x,op2);		\
	}				\
}

#define	UNROLL2(w, op1, op2) {		\
	uint32 x;		        \
	for (x = w; x >= 2; x -= 2) {	\
		op1;			\
		REPEAT2(op2);		\
	}				\
	if (x) {			\
		op1;			\
		op2;			\
	}				\
}


#define	SKEW(r,g,b,skew)	{ r += skew; g += skew; b += skew; }



/*
 * 8-bit palette => colormap/RGB
 */
static void put8bitcmaptile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  while (h-- > 0) {
    UNROLL8(w, , *cp++ = PALmap[*pp++][0]);
    cp += toskew;
    pp += fromskew;
  }
}

/*
 * 4-bit palette => colormap/RGB
 */
static void put4bitcmaptile(cp, pp, Map, w, h, fromskew, toskew)
     byte     *cp;
     u_char   *pp;
     RGBvalue *Map;
     uint32    w, h;
     int       fromskew, toskew;
{
  register byte *bw;

  fromskew /= 2;
  while (h-- > 0) {
    UNROLL2(w, bw = PALmap[*pp++], *cp++ = *bw++);
    cp += toskew;
    pp += fromskew;
  }
}


/*
 * 2-bit palette => colormap/RGB
 */
static void put2bitcmaptile(cp, pp, Map, w, h, fromskew, toskew)
     byte     *cp;
     u_char   *pp;
     RGBvalue *Map;
     uint32    w, h;
     int       fromskew, toskew;
{
  register byte *bw;

  fromskew /= 4;
  while (h-- > 0) {
    UNROLL4(w, bw = PALmap[*pp++], *cp++ = *bw++);
    cp += toskew;
    pp += fromskew;
  }
}

/*
 * 1-bit palette => colormap/RGB
 */
static void put1bitcmaptile(cp, pp, Map, w, h, fromskew, toskew)
	byte     *cp;
	u_char   *pp;
	RGBvalue *Map;
	uint32    w, h;
	int       fromskew, toskew;
{
  register byte *bw;

  fromskew /= 8;
  while (h-- > 0) {
    UNROLL8(w, bw = PALmap[*pp++], *cp++ = *bw++)
    cp += toskew;
    pp += fromskew;
  }
}


/*
 * 8-bit greyscale => colormap/RGB
 */
static void putgreytile(cp, pp, Map, w, h, fromskew, toskew)
     register byte *cp;
     register u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  while (h-- > 0) {
    register uint32 x;
    for (x = w; x-- > 0;)
      *cp++ = BWmap[*pp++][0];
    cp += toskew;
    pp += fromskew;
  }
}


/*
 * 1-bit bilevel => colormap/RGB
 */
static void put1bitbwtile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  register byte *bw;

  fromskew /= 8;
  while (h-- > 0) {
    UNROLL8(w, bw = BWmap[*pp++], *cp++ = *bw++)
    cp += toskew;
    pp += fromskew;
  }
}

/*
 * 2-bit greyscale => colormap/RGB
 */
static void put2bitbwtile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  register byte *bw;

  fromskew /= 4;
  while (h-- > 0) {
    UNROLL4(w, bw = BWmap[*pp++], *cp++ = *bw++);
    cp += toskew;
    pp += fromskew;
  }
}

/*
 * 4-bit greyscale => colormap/RGB
 */
static void put4bitbwtile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  register byte *bw;

  fromskew /= 2;
  while (h-- > 0) {
    UNROLL2(w, bw = BWmap[*pp++], *cp++ = *bw++);
    cp += toskew;
    pp += fromskew;
  }
}

/*
 * 16-bit greyscale => colormap/RGB
 */
static void put16bitbwtile(cp, pp, Map, w, h, fromskew, toskew)
     byte  *cp;
     u_short *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  register uint32   x;

  while (h-- > 0) {
    for (x=w; x>0; x--) {
      *cp++ = Map[*pp++];
    }
    cp += toskew;
    pp += fromskew;
  }
}



/*
 * 8-bit packed samples => RGB
 */
static void putRGBcontig8bittile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  fromskew *= samplesperpixel;
  if (Map) {
    while (h-- > 0) {
      register uint32 x;
      for (x = w; x-- > 0;) {
	*cp++ = Map[pp[0]];
	*cp++ = Map[pp[1]];
	*cp++ = Map[pp[2]];
	pp += samplesperpixel;
      }
      pp += fromskew;
      cp += toskew;
    }
  } else {
    while (h-- > 0) {
      UNROLL8(w, ,
	      *cp++ = pp[0];
	      *cp++ = pp[1];
	      *cp++ = pp[2];
	      pp += samplesperpixel);
      cp += toskew;
      pp += fromskew;
    }
  }
}

/*
 * 16-bit packed samples => RGB
 */
static void putRGBcontig16bittile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_short *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  register u_int x;

  fromskew *= samplesperpixel;
  if (Map) {
    while (h-- > 0) {
      for (x = w; x-- > 0;) {
	*cp++ = Map[pp[0]];
	*cp++ = Map[pp[1]];
	*cp++ = Map[pp[2]];
	pp += samplesperpixel;
      }
      cp += toskew;
      pp += fromskew;
    }
  } else {
    while (h-- > 0) {
      for (x = w; x-- > 0;) {
	*cp++ = pp[0];
	*cp++ = pp[1];
	*cp++ = pp[2];
	pp += samplesperpixel;
      }
      cp += toskew;
      pp += fromskew;
    }
  }
}

/*
 * 8-bit unpacked samples => RGB
 */
static void putRGBseparate8bittile(cp, r, g, b, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *r, *g, *b;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;

{
  if (Map) {
    while (h-- > 0) {
      register uint32 x;
      for (x = w; x > 0; x--) {
	*cp++ = Map[*r++];
	*cp++ = Map[*g++];
	*cp++ = Map[*b++];
      }
      SKEW(r, g, b, fromskew);
      cp += toskew;
    }
  } else {
    while (h-- > 0) {
      UNROLL8(w, ,
	      *cp++ = *r++;
	      *cp++ = *g++;
	      *cp++ = *b++;
	      );
      SKEW(r, g, b, fromskew);
      cp += toskew;
    }
  }
}

/*
 * 16-bit unpacked samples => RGB
 */
static void putRGBseparate16bittile(cp, r, g, b, Map, w, h, fromskew, toskew)
     byte *cp;
     u_short *r, *g, *b;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  uint32 x;

  if (Map) {
    while (h-- > 0) {
      for (x = w; x > 0; x--) {
	*cp++ = Map[*r++];
	*cp++ = Map[*g++];
	*cp++ = Map[*b++];
      }
      SKEW(r, g, b, fromskew);
      cp += toskew;
    }
  } else {
    while (h-- > 0) {
      for (x = 0; x < w; x++) {
	*cp++ = *r++;
	*cp++ = *g++;
	*cp++ = *b++;
      }
      SKEW(r, g, b, fromskew);
      cp += toskew;
    }
  }
}

#define Code2V(c, RB, RW, CR)  (((((int)c)-(int)RB)*(float)CR)/(float)(RW-RB))

#define	CLAMP(f,min,max) \
    (int)((f)+.5 < (min) ? (min) : (f)+.5 > (max) ? (max) : (f)+.5)

#define	LumaRed		YCbCrCoeffs[0]
#define	LumaGreen	YCbCrCoeffs[1]
#define	LumaBlue	YCbCrCoeffs[2]

static	float D1, D2, D3, D4 /*, D5 */;


static void initYCbCrConversion()
{
  /*
   * Old, broken version (goes back at least to 19920426; made worse 19941222):
   *   YCbCrCoeffs[] = {0.299, 0.587, 0.114}
   *     D1 = 1.402
   *     D2 = 0.714136
   *     D3 = 1.772
   *     D4 = 0.138691  <-- bogus
   *     D5 = 1.70358   <-- unnecessary
   *
   * New, fixed version (GRR 20050319):
   *   YCbCrCoeffs[] = {0.299, 0.587, 0.114}
   *     D1 = 1.402
   *     D2 = 0.714136
   *     D3 = 1.772
   *     D4 = 0.344136
   */
  D1 = 2 - 2*LumaRed;
  D2 = D1*LumaRed / LumaGreen;
  D3 = 2 - 2*LumaBlue;
  D4 = D3*LumaBlue / LumaGreen;  /* ARGH, used to be D2*LumaBlue/LumaGreen ! */
/* D5 = 1.0 / LumaGreen; */      /* unnecessary */
}

static void putRGBContigYCbCrClump(cp, pp, cw, ch, w, n, fromskew, toskew)
     byte *cp;
     u_char *pp;
     int cw, ch;
     uint32 w;
     int n, fromskew, toskew;
{
  float Cb, Cr;
  int j, k;

  Cb = Code2V(pp[n],   refBlackWhite[2], refBlackWhite[3], 127);
  Cr = Code2V(pp[n+1], refBlackWhite[4], refBlackWhite[5], 127);
  for (j = 0; j < ch; j++) {
    for (k = 0; k < cw; k++) {
      float Y, R, G, B;
      Y = Code2V(*pp++,
		 refBlackWhite[0], refBlackWhite[1], 255);
      R = Y + Cr*D1;
/*    G = Y*D5 - Cb*D4 - Cr*D2;  highly bogus! */
      G = Y - Cb*D4 - Cr*D2;
      B = Y + Cb*D3;
      /*
       * These are what the JPEG/JFIF equations--which aren't _necessarily_
       * what JPEG/TIFF uses but which seem close enough--are supposed to be,
       * according to Avery Lee (e.g., see http://www.fourcc.org/fccyvrgb.php):
       *
       *     R = Y + 1.402 (Cr-128)
       *     G = Y - 0.34414 (Cb-128) - 0.71414 (Cr-128)
       *     B = Y + 1.772 (Cb-128)
       *
       * Translated into xvtiff.c notation:
       *
       *     R = Y + Cr*D1
       *     G = Y - Cb*D4' - Cr*D2   (i.e., omit D5 and fix D4)
       *     B = Y + Cb*D3
       */
      cp[3*k+0] = CLAMP(R,0,255);
      cp[3*k+1] = CLAMP(G,0,255);
      cp[3*k+2] = CLAMP(B,0,255);
    }
    cp += 3*w+toskew;
    pp += fromskew;
  }
}

static void putRGBSeparateYCbCrClump(cp, y, cb, cr, cw, ch, w, n, fromskew, toskew)
     byte *cp;
     u_char *y, *cb, *cr;
     int cw, ch;
     uint32 w;
     int n, fromskew, toskew;
{
  float Cb, Cr;
  int j, k;
  
  Cb = Code2V(cb[0], refBlackWhite[2], refBlackWhite[3], 127);
  Cr = Code2V(cr[0], refBlackWhite[4], refBlackWhite[5], 127);
  for (j = 0; j < ch; j++) {
    for (k = 0; k < cw; k++) {
      float Y, R, G, B;
      Y = Code2V(y[k], refBlackWhite[0], refBlackWhite[1], 255);
      R = Y + Cr*D1;
      G = Y - Cb*D4 - Cr*D2;
      B = Y + Cb*D3;
      cp[3*k+0] = CLAMP(R,0,255);
      cp[3*k+1] = CLAMP(G,0,255);
      cp[3*k+2] = CLAMP(B,0,255);
    }
    cp += w*3 + toskew;
    y  += w + ch*fromskew;
  }
}

static void putRGBSeparate16bitYCbCrClump(cp, y, cb, cr, cw, ch, w, n, fromskew, toskew)
     byte *cp;
     u_short *y, *cb, *cr;
     int cw, ch;
     uint32 w;
     int n, fromskew, toskew;
{
  float Cb, Cr;
  int j, k;
  
  Cb = Code2V(cb[0], refBlackWhite[2], refBlackWhite[3], 127);
  Cr = Code2V(cr[0], refBlackWhite[4], refBlackWhite[5], 127);
  for (j = 0; j < ch; j++) {
    for (k = 0; k < cw; k++) {
      float Y, R, G, B;
      Y = Code2V(y[k], refBlackWhite[0], refBlackWhite[1], 255);
      R = Y + Cr*D1;
      G = Y - Cb*D4 - Cr*D2;
      B = Y + Cb*D3;
      cp[3*k+0] = CLAMP(R,0,255);
      cp[3*k+1] = CLAMP(G,0,255);
      cp[3*k+2] = CLAMP(B,0,255);
    }
    cp += w*3 + toskew;
    y  += w + ch*fromskew;
  }
}

#undef LumaBlue
#undef LumaGreen
#undef LumaRed
#undef CLAMP
#undef Code2V


/*
 * 8-bit packed YCbCr samples => RGB
 */
static void putcontig8bitYCbCrtile(cp, pp, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *pp;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  u_int Coff = YCbCrVertSampling * YCbCrHorizSampling;
  byte *tp;
  uint32 x;

  /* XXX adjust fromskew */
  while (h >= YCbCrVertSampling) {
    tp = cp;
    for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
      putRGBContigYCbCrClump(tp, pp, YCbCrHorizSampling, YCbCrVertSampling,
			     w, (int) Coff, 0, toskew);
      tp += 3*YCbCrHorizSampling;
      pp += Coff+2;
    }
    if (x > 0) {
      putRGBContigYCbCrClump(tp, pp, (int) x, YCbCrVertSampling,
			     w, (int) Coff, (int)(YCbCrHorizSampling - x),
			     toskew);
      pp += Coff+2;
    }
    cp += YCbCrVertSampling*(3*w + toskew);
    pp += fromskew;
    h -= YCbCrVertSampling;
  }
  if (h > 0) {
    tp = cp;
    for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
      putRGBContigYCbCrClump(tp, pp, YCbCrHorizSampling, (int) h,
			     w, (int) Coff, 0, toskew);
      tp += 3*YCbCrHorizSampling;
      pp += Coff+2;
    }
    if (x > 0)
      putRGBContigYCbCrClump(tp, pp, (int) x, (int) h, w,
			     (int)Coff, (int)(YCbCrHorizSampling-x),toskew);
  }
}

/*
 * 8-bit unpacked YCbCr samples => RGB
 */
static void putYCbCrseparate8bittile(cp, y, cb, cr, Map, w, h, fromskew, toskew)
     byte *cp;
     u_char *y, *cb, *cr;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  uint32 x;
  int fromskew2 = fromskew/YCbCrHorizSampling;
  
  while (h >= YCbCrVertSampling) {
    for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
      putRGBSeparateYCbCrClump(cp, y, cb, cr, YCbCrHorizSampling,
			       YCbCrVertSampling, w, 0, 0, toskew);
      cp += 3*YCbCrHorizSampling;
      y += YCbCrHorizSampling;
      ++cb;
      ++cr;
    }
    if (x > 0) {
      putRGBSeparateYCbCrClump(cp, y, cb, cr, (int) x, YCbCrVertSampling,
			       w, 0, (int)(YCbCrHorizSampling - x), toskew);
      cp += x*3;
      y += YCbCrHorizSampling;
      ++cb;
      ++cr;
    }
    cp += (YCbCrVertSampling - 1)*w*3 + YCbCrVertSampling*toskew;
    y  += (YCbCrVertSampling - 1)*w + YCbCrVertSampling*fromskew;
    cb += fromskew2;
    cr += fromskew2;
    h -= YCbCrVertSampling;
  }
  if (h > 0) {
    for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
      putRGBSeparateYCbCrClump(cp, y, cb, cr, YCbCrHorizSampling, (int) h,
			       w, 0, 0, toskew);
      cp += 3*YCbCrHorizSampling;
      y += YCbCrHorizSampling;
      ++cb;
      ++cr;
    }
    if (x > 0)
      putRGBSeparateYCbCrClump(cp, y, cb, cr, (int) x, (int) h, w, 
			       0, (int)(YCbCrHorizSampling-x),toskew);
  }
}

/*
 * 16-bit unpacked YCbCr samples => RGB
 */
static void putYCbCrseparate16bittile(cp, y, cb, cr, Map, w, h, fromskew, toskew)
     byte *cp;
     u_short *y, *cb, *cr;
     RGBvalue *Map;
     uint32 w, h;
     int fromskew, toskew;
{
  uint32 x;
  int fromskew2 = fromskew/YCbCrHorizSampling;
  
  while (h >= YCbCrVertSampling) {
    for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
      putRGBSeparate16bitYCbCrClump(cp, y, cb, cr, YCbCrHorizSampling,
		 		    YCbCrVertSampling, w, 0, 0, toskew);
      cp += 3*YCbCrHorizSampling;
      y += YCbCrHorizSampling;
      ++cb;
      ++cr;
    }
    if (x > 0) {
      putRGBSeparate16bitYCbCrClump(cp, y, cb, cr, (int) x, YCbCrVertSampling,
				    w, 0, (int)(YCbCrHorizSampling - x),
				    toskew);
      cp += x*3;
      y += YCbCrHorizSampling;
      ++cb;
      ++cr;
    }
    cp += (YCbCrVertSampling - 1)*w*3 + YCbCrVertSampling*toskew;
    y  += (YCbCrVertSampling - 1)*w + YCbCrVertSampling*fromskew;
    cb += fromskew2;
    cr += fromskew2;
    h -= YCbCrVertSampling;
  }
  if (h > 0) {
    for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
      putRGBSeparate16bitYCbCrClump(cp, y, cb, cr, YCbCrHorizSampling, (int) h,
				    w, 0, 0, toskew);
      cp += 3*YCbCrHorizSampling;
      y += YCbCrHorizSampling;
      ++cb;
      ++cr;
    }
    if (x > 0)
      putRGBSeparate16bitYCbCrClump(cp, y, cb, cr, (int) x, (int) h, w, 
			 	    0, (int)(YCbCrHorizSampling-x),toskew);
  }
}

/*
 * Select the appropriate conversion routine for packed data.
 */
static tileContigRoutine pickTileContigCase(Map)
     RGBvalue* Map;
{
  tileContigRoutine put = 0;

  switch (photometric) {
  case PHOTOMETRIC_RGB:
    switch (bitspersample) {
    case 8:  put = (tileContigRoutine) putRGBcontig8bittile;   break;
    case 16: put = (tileContigRoutine) putRGBcontig16bittile;  break;
    }
    break;

  case PHOTOMETRIC_PALETTE:
    switch (bitspersample) {
    case 8: put = put8bitcmaptile; break;
    case 4: put = put4bitcmaptile; break;
    case 2: put = put2bitcmaptile; break;
    case 1: put = put1bitcmaptile; break;
    }
    break;

  case PHOTOMETRIC_MINISWHITE:
  case PHOTOMETRIC_MINISBLACK:
    switch (bitspersample) {
    case 16: put = (tileContigRoutine) put16bitbwtile; break;
    case 8:  put = putgreytile;    break;
    case 4:  put = put4bitbwtile;  break;
    case 2:  put = put2bitbwtile;  break;
    case 1:  put = put1bitbwtile;  break;
    }
    break;

  case PHOTOMETRIC_YCBCR:
    switch (bitspersample) {
    case 8: put = putcontig8bitYCbCrtile; break;
    }
    break;
  }

  if (put==0) TIFFError(filename, "Cannot handle format");
  return (put);
}


/*
 * Select the appropriate conversion routine for unpacked data.
 *
 * NB: we assume that unpacked single-channel data is directed
 *	 to the "packed" routines.
 */
static tileSeparateRoutine pickTileSeparateCase(Map)
     RGBvalue* Map;
{
  tileSeparateRoutine put = 0;

  switch (photometric) {
  case PHOTOMETRIC_RGB:
    switch (bitspersample) {
    case  8: put = (tileSeparateRoutine) putRGBseparate8bittile;  break;
    case 16: put = (tileSeparateRoutine) putRGBseparate16bittile; break;
    }
    break;

  case PHOTOMETRIC_YCBCR:
    switch (bitspersample) {
    case  8: put = (tileSeparateRoutine) putYCbCrseparate8bittile;  break;
    case 16: put = (tileSeparateRoutine) putYCbCrseparate16bittile; break;
    }
    break;
  }

  if (put==0) TIFFError(filename, "Cannot handle format");
  return (put);
}



/*******************************************/
void
VersionInfoTIFF()      /* GRR 19980605 */
{
  char temp[1024], *p, *q;

  strcpy(temp, TIFFGetVersion());
  p = temp;
  while (!isdigit(*p))
    ++p;
  if ((q = strchr(p, '\n')) != NULL)
    *q = '\0';

  fprintf(stderr, "   Compiled with libtiff %s", p);
#ifdef TIFFLIB_VERSION
  fprintf(stderr, " of %d", TIFFLIB_VERSION);    /* e.g., 19960307 */
#endif
  fprintf(stderr, ".\n");
}



#endif /* HAVE_TIFF */
