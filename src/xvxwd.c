/* 
 ** Based on xwdtopnm.c - read and write an X11 or X10 window dump file
 **
 ** Modified heavily by Markus Baur (mbaur@ira.uka.de) for use as a part
 ** of xv-2.21, 12/30/92
 **
 ** Hacked up again to support xv-3.00 and XWDs from 64bit machines 
 ** (e.g. DEC Alphas), 04/10/94
 **
 ** Copyright (C) 1989, 1991 by Jef Poskanzer.
 **
 ** Permission to use, copy, modify, and distribute this software and its
 ** documentation for any purpose and without fee is hereby granted, provided
 ** that the above copyright notice appear in all copies and that both that
 ** copyright notice and this permission notice appear in supporting
 ** documentation.  This software is provided "as is" without express or
 ** implied warranty.
 **
 */

#include "xv.h"


/***************************** x11wd.h *****************************/
#define X11WD_FILE_VERSION 7
typedef struct {
    CARD32 header_size;         /* Size of the entire file header (bytes). */
    CARD32 file_version;        /* X11WD_FILE_VERSION */
    CARD32 pixmap_format;       /* Pixmap format */
    CARD32 pixmap_depth;        /* Pixmap depth */
    CARD32 pixmap_width;        /* Pixmap width */
    CARD32 pixmap_height;       /* Pixmap height */
    CARD32 xoffset;             /* Bitmap x offset */
    CARD32 byte_order;          /* MSBFirst, LSBFirst */
    CARD32 bitmap_unit;         /* Bitmap unit */
    CARD32 bitmap_bit_order;    /* MSBFirst, LSBFirst */
    CARD32 bitmap_pad;          /* Bitmap scanline pad */
    CARD32 bits_per_pixel;      /* Bits per pixel */
    CARD32 bytes_per_line;      /* Bytes per scanline */
    CARD32 visual_class;        /* Class of colormap */
    CARD32 red_mask;            /* Z red mask */
    CARD32 green_mask;          /* Z green mask */
    CARD32 blue_mask;           /* Z blue mask */
    CARD32 bits_per_rgb;        /* Log base 2 of distinct color values */
    CARD32 colormap_entries;    /* Number of entries in colormap */
    CARD32 ncolors;             /* Number of Color structures */
    CARD32 window_width;        /* Window width */
    CARD32 window_height;       /* Window height */
    CARD32 window_x;            /* Window upper left X coordinate */
    CARD32 window_y;            /* Window upper left Y coordinate */
    CARD32 window_bdrwidth;     /* Window border width */
#ifdef WORD64
    CARD32 header_pad;
#endif
  } X11WDFileHeader;

typedef struct {
  CARD32 num;
  CARD16 red, green, blue;
  CARD8  flags;                 /* do_red, do_green, do_blue */
  CARD8  pad;
  } X11XColor;


/*-------------------------------------------------------------------------*/

typedef byte pixel;

/* local functions */
static int    getinit         PARM((FILE *, int*, int*, int*, CARD32 *, 
			                          CARD32, PICINFO *));
static CARD32 getpixnum       PARM((FILE *));
static int    xwdError        PARM((char *));
static void   xwdWarning      PARM((char *));
static int    bs_short        PARM((int));
static CARD32 bs_long         PARM((CARD32));
static int    readbigshort    PARM((FILE *, CARD16 *));
static int    readbiglong     PARM((FILE *, CARD32 *));
static int    readlittleshort PARM((FILE *, CARD16 *));
static int    readlittlelong  PARM((FILE *, CARD32 *));
static int    writebigshort   PARM((FILE *, int));
static int    writebiglong    PARM((FILE *, CARD32));

static byte  *pic8, *pic24;
static CARD32 red_mask, green_mask, blue_mask;
static int    bits_per_item, bits_used, bit_shift, bits_per_pixel;
static char   buf[4];
static char   *byteP;
static CARD16 *shortP;
static CARD32 *longP;
static CARD32 pixel_mask;
static int    byte_swap, byte_order, bit_order, filesize;
static byte   bw[2] = {0, 0xff};

static char  *bname;



/*************************************/
int LoadXWD(fname, pinfo)
     char *fname;
     PICINFO *pinfo;
{
  /* returns '1' on success */

  pixel *xP;
  int    col;
  int    rows, cols, padright, row;
  CARD32 maxval, visualclass;
  FILE  *ifp;

  bname          = BaseName(fname);
  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;
  maxval         = 0;

  ifp = xv_fopen(fname, "r");
  if (!ifp) return (xwdError("can't open file"));
  
  /* figure out the file size (used to check colormap size) */
  fseek(ifp, 0L, 2);
  filesize = ftell(ifp);
  fseek(ifp, 0L, 0);
  

  if (getinit(ifp, &cols, &rows, &padright, &visualclass, maxval, pinfo))
    return 0;


  switch (visualclass) {
  case StaticGray:
  case GrayScale:
    pinfo->colType = F_GREYSCALE;
    pic8 = (byte *) calloc((size_t) cols*rows, (size_t) 1);
    if (!pic8) {
      xwdError("couldn't malloc 'pic'");
      return 0;
    }

    for (row=0; row<rows; row++) {
      for (col=0, xP=pic8+(row*cols); col<cols; col++, xP++)
	*xP = getpixnum(ifp);
      
      for (col=0; col<padright; col++) getpixnum(ifp);
    }

    pinfo->type = PIC8;
    pinfo->pic  = pic8;
    break;      

  case StaticColor:
  case PseudoColor:
    pinfo->colType = F_FULLCOLOR;
    pic8 = (byte *) calloc((size_t) cols*rows, (size_t) 1);
    if (!pic8) {
      xwdError("couldn't malloc 'pic'");
      return 0;
    }

    for (row=0; row<rows; row++) {
      for (col=0, xP=pic8+(row*cols); col<cols; col++, xP++)
	*xP = getpixnum(ifp);
      for (col=0; col<padright; col++) getpixnum(ifp);
    }
    
    pinfo->type = PIC8;
    pinfo->pic  = pic8;
    break;      

  case TrueColor:
  case DirectColor:
    pinfo->colType = F_FULLCOLOR;
    pic24 = (byte *) calloc((size_t) cols*rows*3, (size_t) 1);
    if (!pic24) {
      xwdError("couldn't malloc 'pic24'");
      return 0;
    }

    for (row=0; row<rows; row++) {
      for (col=0, xP=pic24+(row*cols*3); col<cols; col++) {
	CARD32 ul;
	
	ul = getpixnum(ifp);
	switch (bits_per_pixel) {
	case 16:
	  *xP++ = ((ul & red_mask)   >> 0);
	  *xP++ = ((ul & green_mask) >> 5);
	  *xP++ = ((ul & blue_mask)  >> 10);
	  break;
	  
	case 24:
	case 32:
	  *xP++ = (ul    ) & 0xff;
	  *xP++ = (ul>> 8) & 0xff;
	  *xP++ = (ul>>16) & 0xff;
	  break;
	  
	default:
	  xwdError("True/Direct only supports 16, 24, and 32 bits");
	  return 0;
	}
      }

      for (col=0; col<padright; col++) getpixnum(ifp);
    }
    
    pinfo->type = PIC24;
    pinfo->pic  = pic24;
    break;
    
  default:
    xwdError("unknown visual class");
    return 0;
  }

  sprintf(pinfo->fullInfo, "XWD, %d-bit %s.  (%d bytes)",
	  bits_per_pixel, 
	  ((visualclass == StaticGray ) ? "StaticGray"  :
	   (visualclass == GrayScale  ) ? "GrayScale"   :
	   (visualclass == StaticColor) ? "StaticColor" :
	   (visualclass == PseudoColor) ? "PseudoColor" :
	   (visualclass == TrueColor  ) ? "TrueColor"   :
	   (visualclass == DirectColor) ? "DirectColor" : "<unknown>"),
	  filesize);

  sprintf(pinfo->shrtInfo, "%dx%d XWD.", cols, rows);

  pinfo->normw = pinfo->w = cols;
  pinfo->normh = pinfo->h = rows;

  fclose(ifp);
  return 1;
}


/*********************/
static int getinit(file, colsP, rowsP, padrightP, visualclassP, maxv, pinfo)
     FILE* file;
     int* colsP;
     int* rowsP;
     int* padrightP;
     CARD32 * visualclassP;
     CARD32   maxv;
     PICINFO *pinfo;
{
  int              i;
  int              grayscale;
  char             errstr[200];
  byte             header[sizeof(X11WDFileHeader)];
  X11WDFileHeader *h11P;
  X11XColor       *x11colors;
  CARD32           pad;
  int              word64 = 0;

  x11colors = (X11XColor *) NULL;

  byte_swap = 0;
  maxv = 255L;

  h11P = (X11WDFileHeader*) header;
  
  if (fread(&header[0], sizeof(*h11P), (size_t) 1, file) != 1)
    return(xwdError("couldn't read X11 XWD file header"));
  
  if (h11P->file_version != X11WD_FILE_VERSION) {
    byte_swap = 1;
    h11P->header_size      = bs_long(h11P->header_size);
    h11P->file_version     = bs_long(h11P->file_version);
    h11P->pixmap_format    = bs_long(h11P->pixmap_format);
    h11P->pixmap_depth     = bs_long(h11P->pixmap_depth);
    h11P->pixmap_width     = bs_long(h11P->pixmap_width);
    h11P->pixmap_height    = bs_long(h11P->pixmap_height);
    h11P->xoffset          = bs_long(h11P->xoffset);
    h11P->byte_order       = bs_long(h11P->byte_order);
    h11P->bitmap_unit      = bs_long(h11P->bitmap_unit);
    h11P->bitmap_bit_order = bs_long(h11P->bitmap_bit_order);
    h11P->bitmap_pad       = bs_long(h11P->bitmap_pad);
    h11P->bits_per_pixel   = bs_long(h11P->bits_per_pixel);
    h11P->bytes_per_line   = bs_long(h11P->bytes_per_line);
    h11P->visual_class     = bs_long(h11P->visual_class);
    h11P->red_mask         = bs_long(h11P->red_mask);
    h11P->green_mask       = bs_long(h11P->green_mask);
    h11P->blue_mask        = bs_long(h11P->blue_mask);
    h11P->bits_per_rgb     = bs_long(h11P->bits_per_rgb);
    h11P->colormap_entries = bs_long(h11P->colormap_entries);
    h11P->ncolors          = bs_long(h11P->ncolors);
    h11P->window_width     = bs_long(h11P->window_width);
    h11P->window_height    = bs_long(h11P->window_height);
    h11P->window_x         = bs_long(h11P->window_x);
    h11P->window_y         = bs_long(h11P->window_y);
    h11P->window_bdrwidth  = bs_long(h11P->window_bdrwidth);
  }

  for (i=0; i<h11P->header_size - sizeof(*h11P); i++)
    if (getc(file) == EOF)
      return(xwdError("couldn't read rest of X11 XWD file header"));
      
  /* Check whether we can handle this dump. */
  if (h11P->pixmap_depth > 24)
    return(xwdError("can't handle X11 pixmap_depth > 24"));

  if (h11P->bits_per_rgb > 24)
    return(xwdError("can't handle X11 bits_per_rgb > 24"));

  if (h11P->pixmap_format != ZPixmap && h11P->pixmap_depth != 1)  {
    sprintf(errstr, "can't handle X11 pixmap_format %d with depth != 1",
	    h11P->pixmap_format);
    return(xwdError(errstr));
  }

  if (h11P->bitmap_unit != 8 && h11P->bitmap_unit != 16 &&
      h11P->bitmap_unit != 32)  {
    sprintf(errstr, "X11 bitmap_unit (%d) is non-standard - can't handle",
	    h11P->bitmap_unit);
    return(xwdError(errstr));
  }
  
  grayscale = 1;
  if (h11P->ncolors > 0) {      /* Read X11 colormap. */
    x11colors = (X11XColor*) malloc(h11P->ncolors * sizeof(X11XColor));
    if (!x11colors) return(xwdError("out of memory"));
    
    if (h11P->header_size + h11P->ncolors * sizeof(X11XColor) 
	+ h11P->pixmap_height * h11P->bytes_per_line + h11P->ncolors * 4
	== filesize ) word64 = 1;
    
    if (word64) {
      for (i = 0; i < h11P->ncolors; ++i) {
	if (fread(&pad, sizeof(pad), (size_t) 1, file ) != 1)
	  return(xwdError("couldn't read X11 XWD colormap"));

	if (fread( &x11colors[i], sizeof(X11XColor), (size_t) 1, file) != 1)
	  return(xwdError("couldn't read X11 XWD colormap"));
      }
    }
    else {
      if (fread(x11colors, sizeof(X11XColor), (size_t) h11P->ncolors, file) 
	  != h11P->ncolors)
	return(xwdError("couldn't read X11 XWD colormap"));
    }
    
    for (i = 0; i < h11P->ncolors; ++i) {
      if (byte_swap) {
	x11colors[i].red   = (CARD16) bs_short(x11colors[i].red);
	x11colors[i].green = (CARD16) bs_short(x11colors[i].green);
	x11colors[i].blue  = (CARD16) bs_short(x11colors[i].blue);
      }

      if (maxv != 65535L) {
	x11colors[i].red   = (u_short) ((x11colors[i].red   * maxv) / 65535);
	x11colors[i].green = (u_short) ((x11colors[i].green * maxv) / 65535);
	x11colors[i].blue  = (u_short) ((x11colors[i].blue  * maxv) / 65535);
      }
      if (x11colors[i].red != x11colors[i].green ||
	  x11colors[i].green != x11colors[i].blue)
	grayscale = 0;
    }
  }
  
  *visualclassP = h11P->visual_class;
  if (*visualclassP == TrueColor || *visualclassP == DirectColor) {
    if (h11P->bits_per_pixel == 16) maxv = 31;
    else maxv = 255;
  }

  else if (*visualclassP == StaticGray && h11P->bits_per_pixel == 1) {
    maxv = 1;
    /* B/W bitmaps have a two entry colormap */
    pinfo->r[0] = pinfo->g[0] = pinfo->b[0] = 255;   /* 0 = white */
    pinfo->r[1] = pinfo->g[1] = pinfo->b[1] = 0;     /* 1 = black */
  }

  else if (*visualclassP == StaticGray) {
    maxv = (1 << h11P->bits_per_pixel) - 1;
    for (i=0; i<=maxv; i++)
      pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = x11colors[i].red;
  }

  else {
    if (grayscale) {
      for (i=0; i<=h11P->ncolors; i++)
	pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = x11colors[i].red;
    }
    else {
      for (i = 0; i < h11P->ncolors; ++i) {
	pinfo->r[i] = x11colors[i].red;
	pinfo->g[i] = x11colors[i].green;
	pinfo->b[i] = x11colors[i].blue;
      }
    }
  }
      
  *colsP = h11P->pixmap_width;
  *rowsP = h11P->pixmap_height;
  *padrightP = h11P->bytes_per_line * 8 / h11P->bits_per_pixel -
    h11P->pixmap_width;
  
  bits_per_item  = h11P->bitmap_unit;
  bits_used      = bits_per_item;
  bits_per_pixel = h11P->bits_per_pixel;
  byte_order     = h11P->byte_order;
  bit_order      = h11P->bitmap_bit_order;
  
  if (bits_per_pixel == sizeof(pixel_mask) * 8)  pixel_mask = (CARD32) -1;
  else pixel_mask = (1 << bits_per_pixel) - 1;
  
  red_mask   = h11P->red_mask;
  green_mask = h11P->green_mask;
  blue_mask  = h11P->blue_mask;

  byteP  = (char   *) buf;
  shortP = (CARD16 *) buf;
  longP  = (CARD32 *) buf;

  return 0;
}


/******************************/
static CARD32 getpixnum(file)
     FILE* file;
{
  int n;
  
  if (bits_used == bits_per_item) {
    switch (bits_per_item) {
    case 8:
      *byteP = getc(file);
      break;
      
    case 16:
      if (byte_order == MSBFirst) {
	if (readbigshort(file, shortP) == -1)
	  xwdWarning("unexpected EOF");
      }
      else {
	if (readlittleshort(file, shortP) == -1)
	  xwdWarning("unexpected EOF");
      }
      break;
      
    case 32:
      if (byte_order == MSBFirst) {
	if (readbiglong(file, longP) == -1)
	  xwdWarning("unexpected EOF");
      }
      else {
	if (readlittlelong(file, longP) == -1)
	  xwdWarning("unexpected EOF");
      }
      break;
      
    default:
      xwdWarning("can't happen");
    }
    bits_used = 0;
    
    if (bit_order == MSBFirst)
      bit_shift = bits_per_item - bits_per_pixel;
    else
      bit_shift = 0;
  }
  
  switch (bits_per_item) {
  case 8:
    n = (*byteP >> bit_shift) & pixel_mask;
    break;
    
  case 16:
    n = (*shortP >> bit_shift) & pixel_mask;
    break;
    
  case 32:
    n = (*longP >> bit_shift) & pixel_mask;
    break;
    
  default:
    n = 0;
    xwdWarning("can't happen");
  }
  
  if (bit_order == MSBFirst) bit_shift -= bits_per_pixel;
                        else bit_shift += bits_per_pixel;

  bits_used += bits_per_pixel;
  
  return n;
}


/***************************/
static int xwdError(st)
     char *st;
{
  if (pic8  != NULL) free(pic8);
  if (pic24 != NULL) free(pic24);

  SetISTR(ISTR_WARNING,"%s:  %s", bname, st);
  return 0;
}


/***************************/
static void xwdWarning(st)
     char *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", bname, st);
}





/* 
 * Byte-swapping junk.
 */

union cheat {
  CARD32 l;
  CARD16 s;
  CARD8  c[sizeof(CARD32)];
};

static int bs_short(s)
     int s;
{
  union cheat u;
  unsigned char t;

  u.s = (CARD16) s;
  t = u.c[0];  u.c[0] = u.c[1];  u.c[1] = t;
  return (int) u.s;
}

static CARD32 bs_long(l)
     CARD32 l;
{
  union cheat u;
  unsigned char t;
  
  u.l = l;
  t = u.c[0];  u.c[0] = u.c[3];  u.c[3] = t;
  t = u.c[1];  u.c[1] = u.c[2];  u.c[2] = t;
  return u.l;
}






/* 
 * Endian I/O.
 */

static int readbigshort(in, sP)
     FILE  *in;
     CARD16 *sP;
{
  *sP = (getc(in) & 0xff) << 8;
  *sP |= getc(in) & 0xff;

  if (ferror(in)) return -1;
  return 0;
}

static int readbiglong(in, lP)
     FILE *in;
     CARD32 *lP;
{
  *lP  = (getc(in) & 0xff) << 24;
  *lP |= (getc(in) & 0xff) << 16;
  *lP |= (getc(in) & 0xff) << 8;
  *lP |=  getc(in) & 0xff;

  if (ferror(in)) return -1;
  return 0;
}


static int readlittleshort(in, sP)
     FILE  *in;
     CARD16 *sP;
{
  *sP  =  getc(in) & 0xff;
  *sP |= (getc(in) & 0xff) << 8;
  
  if (ferror(in)) return -1;
  return 0;
}


static int readlittlelong(in, lP)
     FILE *in;
     CARD32 *lP;
{
  *lP  =  getc(in) & 0xff;
  *lP |= (getc(in) & 0xff) << 8;
  *lP |= (getc(in) & 0xff) << 16;
  *lP |= (getc(in) & 0xff) << 24;

  if (ferror(in)) return -1;
  return 0;
}


static int writebiglong(out, l)
     FILE* out;
     CARD32 l;
{
  putc((l>>24) & 0xff, out);
  putc((l>>16) & 0xff, out);
  putc((l>> 8) & 0xff, out);
  putc( l      & 0xff, out);
  return 0;
}


static int writebigshort(out, s)
     FILE* out;
     int   s;
{
  putc((s>>8)&0xff, out);
  putc(s&0xff, out);
  return 0;
}
