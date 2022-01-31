/*
 * xvbmp.c - I/O routines for .BMP files (MS Windows 3.x and later; OS/2)
 *
 * LoadBMP(fname, numcols)
 * WriteBMP(fp, pic, ptype, w, h, r, g, b, numcols, style);
 */

#include "copyright.h"

#include "xv.h"

/* Comments on error-handling:
   A truncated file is not considered a Major Error.  The file is loaded,
   and the rest of the pic is filled with 0's.

   A file with garbage characters in it is an unloadable file.  All allocated
   stuff is tossed, and LoadBMP returns non-zero.

   Not being able to malloc is a Fatal Error.  The program is aborted. */


#define BI_RGB       0   /* a.k.a. uncompressed */
#define BI_RLE8      1
#define BI_RLE4      2
#define BI_BITFIELDS 3   /* BMP version 4 */
#define BI_JPEG      4   /* BMP version 5 (not yet supported) */
#define BI_PNG       5   /* BMP version 5 (not yet supported) */

#define WIN_OS2_OLD 12
#define WIN_NEW     40
#define OS2_NEW     64

#if (defined(UINT_MAX) && UINT_MAX != 0xffffffffU)
#  error XV's BMP code requires 32-bit unsigned integer type, but u_int isn't
#endif

static long filesize;

static int   loadBMP1   PARM((FILE *, byte *, u_int, u_int));
static int   loadBMP4   PARM((FILE *, byte *, u_int, u_int, u_int));
static int   loadBMP8   PARM((FILE *, byte *, u_int, u_int, u_int));
static int   loadBMP16  PARM((FILE *, byte *, u_int, u_int, u_int *));
static int   loadBMP24  PARM((FILE *, byte *, u_int, u_int, u_int));
static int   loadBMP32  PARM((FILE *, byte *, u_int, u_int, u_int *));
static u_int getshort   PARM((FILE *));
static u_int getint     PARM((FILE *));
static void  putshort   PARM((FILE *, int));
static void  putint     PARM((FILE *, int));
static void  writeBMP1  PARM((FILE *, byte *, int, int));
static void  writeBMP4  PARM((FILE *, byte *, int, int));
static void  writeBMP8  PARM((FILE *, byte *, int, int));
static void  writeBMP24 PARM((FILE *, byte *, int, int));
static int   bmpError   PARM((const char *, const char *));


#define FERROR(fp) (ferror(fp) || feof(fp))

/*******************************************/
int LoadBMP(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  FILE       *fp;
  int        i, c, c1, rv, bPad;
  u_int      bfSize, bfOffBits, biSize, biWidth, biHeight, biPlanes;
  u_int      biBitCount, biCompression, biSizeImage, biXPelsPerMeter;
  u_int      biYPelsPerMeter, biClrUsed, biClrImportant;
  u_int      colormask[3];
  char       buf[512], rgb_bits[16];
  const char *cmpstr, *bname;
  byte       *pic24, *pic8;

  /* returns '1' on success */

  pic8 = pic24 = (byte *) NULL;
  bname = BaseName(fname);

  fp = xv_fopen(fname,"r");
  if (!fp) return (bmpError(bname, "couldn't open file"));

  fseek(fp, 0L, 2);      /* figure out the file size */
  filesize = ftell(fp);
  fseek(fp, 0L, 0);


  /* read the file type (first two bytes) */
  c = getc(fp);  c1 = getc(fp);
  if (c!='B' || c1!='M') { bmpError(bname,"file type != 'BM'"); goto ERROR; }

  bfSize = getint(fp);
  getshort(fp);         /* reserved and ignored */
  getshort(fp);
  bfOffBits = getint(fp);

  biSize          = getint(fp);

  if (biSize == WIN_NEW || biSize == OS2_NEW) {
    biWidth         = getint(fp);
    biHeight        = getint(fp);
    biPlanes        = getshort(fp);
    biBitCount      = getshort(fp);
    biCompression   = getint(fp);
    biSizeImage     = getint(fp);
    biXPelsPerMeter = getint(fp);
    biYPelsPerMeter = getint(fp);
    biClrUsed       = getint(fp);
    biClrImportant  = getint(fp);
  }
  else {    /* old bitmap format */
    biWidth         = getshort(fp);          /* Types have changed ! */
    biHeight        = getshort(fp);
    biPlanes        = getshort(fp);
    biBitCount      = getshort(fp);

    /* not in old versions, so have to compute them */
    biSizeImage = (((biPlanes * biBitCount*biWidth)+31)/32)*4*biHeight;

    biCompression   = BI_RGB;
    biXPelsPerMeter = biYPelsPerMeter = 0;
    biClrUsed       = biClrImportant  = 0;
  }

  if (DEBUG>1) {
    fprintf(stderr,"\nLoadBMP:\tbfSize=%d, bfOffBits=%d\n",bfSize,bfOffBits);
    fprintf(stderr,"\t\tbiSize=%d, biWidth=%d, biHeight=%d, biPlanes=%d\n",
	    biSize, biWidth, biHeight, biPlanes);
    fprintf(stderr,"\t\tbiBitCount=%d, biCompression=%d, biSizeImage=%d\n",
	    biBitCount, biCompression, biSizeImage);
    fprintf(stderr,"\t\tbiX,YPelsPerMeter=%d,%d  biClrUsed=%d, biClrImp=%d\n",
	    biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant);
  }

  if (FERROR(fp)) { bmpError(bname,"EOF reached in file header"); goto ERROR; }


  /* error-checking */
  if ((biBitCount!=1 && biBitCount!=4 && biBitCount!=8 &&
       biBitCount!=16 && biBitCount!=24 && biBitCount!=32) ||
      biPlanes!=1 || biCompression>BI_PNG ||
      biWidth<=0 || biHeight<=0 ||
      (biClrUsed && biClrUsed > (1 << biBitCount))) {

    sprintf(buf,
	    "Unsupported BMP type (%dx%d, Bits=%d, Colors=%d, Planes=%d, "
	    "Compr=%d)",
	    biWidth, biHeight, biBitCount, biClrUsed, biPlanes, biCompression);

    bmpError(bname, buf);
    goto ERROR;
  }

  if (biCompression>BI_BITFIELDS) {
    sprintf(buf, "Unsupported BMP compression method (%s)",
	    biCompression == BI_JPEG? "JPEG" :
	    biCompression == BI_PNG? "PNG" :
	    "unknown/newer than v5");

    bmpError(bname, buf);
    goto ERROR;
  }

  if (((biBitCount==1 || biBitCount==24) && biCompression != BI_RGB) ||
      (biBitCount==4 && biCompression!=BI_RGB && biCompression!=BI_RLE4) ||
      (biBitCount==8 && biCompression!=BI_RGB && biCompression!=BI_RLE8) ||
      ((biBitCount==16 || biBitCount==32) &&
       biCompression!=BI_RGB && biCompression!=BI_BITFIELDS)) {

    sprintf(buf,"Unsupported BMP type (bitCount=%d, Compression=%d)",
	    biBitCount, biCompression);

    bmpError(bname, buf);
    goto ERROR;
  }


  bPad = 0;
  if (biSize != WIN_OS2_OLD) {
    /* skip ahead to colormap, using biSize */
    c = biSize - 40;    /* 40 bytes read from biSize to biClrImportant */
    for (i=0; i<c; i++)
      getc(fp);
    bPad = bfOffBits - (biSize + 14);
  }

  /* 16-bit or 32-bit color mask */
  if (biCompression==BI_BITFIELDS) {
    colormask[0] = getint(fp);
    colormask[1] = getint(fp);
    colormask[2] = getint(fp);
    bPad -= 12;
  }

  /* load up colormap, if any */
  if (biBitCount == 1 || biBitCount == 4 || biBitCount == 8) {
    int i, cmaplen;

    cmaplen = (biClrUsed) ? biClrUsed : 1 << biBitCount;
    for (i=0; i<cmaplen; i++) {
      pinfo->b[i] = getc(fp);
      pinfo->g[i] = getc(fp);
      pinfo->r[i] = getc(fp);
      if (biSize != WIN_OS2_OLD) {
	getc(fp);
	bPad -= 4;
      }
    }

    if (FERROR(fp))
      { bmpError(bname,"EOF reached in BMP colormap"); goto ERROR; }

    if (DEBUG>1) {
      fprintf(stderr,"LoadBMP:  BMP colormap:  (RGB order)\n");
      for (i=0; i<cmaplen; i++) {
	fprintf(stderr,"%02x%02x%02x  ", pinfo->r[i],pinfo->g[i],pinfo->b[i]);
      }
      fprintf(stderr,"\n\n");
    }
  }

  if (biSize != WIN_OS2_OLD) {
    /* Waste any unused bytes between the colour map (if present)
       and the start of the actual bitmap data. */

    while (bPad > 0) {
      (void) getc(fp);
      bPad--;
    }
  }

  /* create pic8 or pic24 */

  if (biBitCount==16 || biBitCount==24 || biBitCount==32) {
    u_int npixels = biWidth * biHeight;
    u_int count = 3 * npixels;

    if (biWidth == 0 || biHeight == 0 || npixels/biWidth != biHeight ||
        count/3 != npixels)
      return (bmpError(bname, "image dimensions too large"));
    pic24 = (byte *) calloc((size_t) count, (size_t) 1);
    if (!pic24) return (bmpError(bname, "couldn't malloc 'pic24'"));
  }
  else {
    u_int npixels = biWidth * biHeight;

    if (biWidth == 0 || biHeight == 0 || npixels/biWidth != biHeight)
      return (bmpError(bname, "image dimensions too large"));
    pic8 = (byte *) calloc((size_t) npixels, (size_t) 1);
    if (!pic8) return(bmpError(bname, "couldn't malloc 'pic8'"));
  }

  WaitCursor();

  /* load up the image */
  switch (biBitCount) {
  case 1:
    rv = loadBMP1(fp, pic8, biWidth, biHeight);
    break;
  case 4:
    rv = loadBMP4(fp, pic8, biWidth, biHeight, biCompression);
    break;
  case 8:
    rv = loadBMP8(fp, pic8, biWidth, biHeight, biCompression);
    break;
  case 16:
    rv = loadBMP16(fp, pic24, biWidth, biHeight,           /*  v-- BI_RGB */
                   biCompression == BI_BITFIELDS? colormask : NULL);
    break;
  default:
    if (biBitCount == 32 && biCompression == BI_BITFIELDS)
      rv = loadBMP32(fp, pic24, biWidth, biHeight, colormask);
    else /* 24 or (32 and BI_RGB) */
      rv = loadBMP24(fp, pic24, biWidth, biHeight, biBitCount);
    break;
  }

  if (rv) bmpError(bname, "File appears truncated.  Winging it.");


  fclose(fp);


  if (biBitCount > 8) {
    pinfo->pic  = pic24;
    pinfo->type = PIC24;
  }
  else {
    pinfo->pic  = pic8;
    pinfo->type = PIC8;
  }

  cmpstr = "";
  if      (biCompression == BI_RLE4) cmpstr = ", RLE4 compressed";
  else if (biCompression == BI_RLE8) cmpstr = ", RLE8 compressed";
  else if (biCompression == BI_BITFIELDS) {
    int    bit, c[3], i;
    u_int  mask;

    for (i = 0; i < 3; ++i) {
      mask = colormask[i];
      c[i] = 0;
      for (bit = 0; bit < 32; ++bit) {
        if (mask & 1)
          ++c[i];
        mask >>= 1;
      }
    }
    sprintf(rgb_bits, ", RGB%d%d%d", c[0], c[1], c[2]);
    cmpstr = rgb_bits;
  }

  pinfo->w = biWidth;  pinfo->h = biHeight;
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
  pinfo->frmType = F_BMP;
  pinfo->colType = F_FULLCOLOR;

  sprintf(pinfo->fullInfo, "%sBMP, %d bit%s per pixel%s.  (%ld bytes)",
	  ((biSize==WIN_OS2_OLD) ? "Old OS/2 " :
	   (biSize==WIN_NEW)     ? "Windows "  : ""),
	  biBitCount,  (biBitCount == 1) ? "" : "s",
	  cmpstr, filesize);

  sprintf(pinfo->shrtInfo, "%dx%d BMP.", biWidth, biHeight);
  pinfo->comment = (char *) NULL;

  return 1;


 ERROR:
  fclose(fp);
  return 0;
}


/*******************************************/
static int loadBMP1(fp, pic8, w, h)
     FILE *fp;
     byte *pic8;
     u_int  w,h;
{
  int   i,j,c,bitnum,padw;
  byte *pp = pic8 + ((h - 1) * w);
  size_t l = w*h;

  c = 0;
  padw = ((w + 31)/32) * 32;  /* 'w', padded to be a multiple of 32 */

  for (i=h-1; i>=0 && (pp - pic8 <= l); i--) {
    pp = pic8 + (i * w);
    if ((i&0x3f)==0) WaitCursor();
    for (j=bitnum=0; j<padw; j++,bitnum++) {
      if ((bitnum&7) == 0) { /* read the next byte */
	c = getc(fp);
	bitnum = 0;
      }

      if (j<w) {
	*pp++ = (c & 0x80) ? 1 : 0;
	c <<= 1;
      }
    }
    if (FERROR(fp)) break;
  }

  return (FERROR(fp));
}



/*******************************************/
static int loadBMP4(fp, pic8, w, h, comp)
     FILE *fp;
     byte *pic8;
     u_int  w,h,comp;
{
  int   i,j,c,c1,x,y,nybnum,padw,rv;
  byte *pp = pic8 + ((h - 1) * w);
  size_t l = w*h;

  rv = 0;
  c = c1 = 0;

  if (comp == BI_RGB) {   /* read uncompressed data */
    padw = ((w + 7)/8) * 8; /* 'w' padded to a multiple of 8pix (32 bits) */

    for (i=h-1; i>=0 && (pp - pic8 <= l); i--) {
      pp = pic8 + (i * w);
      if ((i&0x3f)==0) WaitCursor();

      for (j=nybnum=0; j<padw; j++,nybnum++) {
	if ((nybnum & 1) == 0) { /* read next byte */
	  c = getc(fp);
	  nybnum = 0;
	}

	if (j<w) {
	  *pp++ = (c & 0xf0) >> 4;
	  c <<= 4;
	}
      }
      if (FERROR(fp)) break;
    }
  }

  else if (comp == BI_RLE4) {  /* read RLE4 compressed data */
    x = y = 0;
    pp = pic8 + x + (h-y-1)*w;

    while (y<h) {
      c = getc(fp);  if (c == EOF) { rv = 1;  break; }

      if (c) {                                   /* encoded mode */
	c1 = getc(fp);
	for (i=0; i<c && (pp - pic8 <= l); i++,x++,pp++)
	  *pp = (i&1) ? (c1 & 0x0f) : ((c1>>4)&0x0f);
      }

      else {    /* c==0x00  :  escape codes */
	c = getc(fp);  if (c == EOF) { rv = 1;  break; }

	if      (c == 0x00) {                    /* end of line */
	  x=0;  y++;  pp = pic8 + x + (h-y-1)*w;
	}

	else if (c == 0x01) break;               /* end of pic8 */

	else if (c == 0x02) {                    /* delta */
	  c = getc(fp);  x += c;
	  c = getc(fp);  y += c;
	  pp = pic8 + x + (h-y-1)*w;
	}

	else {                                   /* absolute mode */
	  for (i=0; i<c && (pp - pic8 <= l); i++, x++, pp++) {
	    if ((i&1) == 0) c1 = getc(fp);
	    *pp = (i&1) ? (c1 & 0x0f) : ((c1>>4)&0x0f);
	  }

	  if (((c&3)==1) || ((c&3)==2)) getc(fp);  /* read pad byte */
	}
      }  /* escape processing */
      if (FERROR(fp)) break;
    }  /* while */
  }

  else {
    fprintf(stderr,"unknown BMP compression type 0x%0x\n", comp);
  }

  if (FERROR(fp)) rv = 1;
  return rv;
}



/*******************************************/
static int loadBMP8(fp, pic8, w, h, comp)
     FILE *fp;
     byte *pic8;
     u_int  w,h,comp;
{
  int   i,j,c,c1,padw,x,y,rv;
  byte *pp = pic8 + ((h - 1) * w);
  size_t l = w*h;
  byte *pend;

  rv = 0;

  pend = pic8 + w * h;

  if (comp == BI_RGB) {   /* read uncompressed data */
    padw = ((w + 3)/4) * 4; /* 'w' padded to a multiple of 4pix (32 bits) */

    for (i=h-1; i>=0 && (pp - pic8 <= l); i--) {
      pp = pic8 + (i * w);
      if ((i&0x3f)==0) WaitCursor();

      for (j=0; j<padw; j++) {
	c = getc(fp);  if (c==EOF) rv = 1;
	if (j<w) *pp++ = c;
      }
      if (FERROR(fp)) break;
    }
  }

  else if (comp == BI_RLE8) {  /* read RLE8 compressed data */
    x = y = 0;
    pp = pic8 + x + (h-y-1)*w;

    while (y<h && pp<=pend) {
      c = getc(fp);  if (c == EOF) { rv = 1;  break; }

      if (c) {                                   /* encoded mode */
	c1 = getc(fp);
	for (i=0; i<c && pp<=pend; i++,x++,pp++) *pp = c1;
      }

      else {    /* c==0x00  :  escape codes */
	c = getc(fp);  if (c == EOF) { rv = 1;  break; }

	if      (c == 0x00) {                    /* end of line */
	  x=0;  y++;  pp = pic8 + x + (h-y-1)*w;
	}

	else if (c == 0x01) break;               /* end of pic8 */

	else if (c == 0x02) {                    /* delta */
	  c = getc(fp);  x += c;
	  c = getc(fp);  y += c;
	  pp = pic8 + x + (h-y-1)*w;
	}

	else {                                   /* absolute mode */
	  for (i=0; i<c && pp<=pend; i++, x++, pp++) {
	    c1 = getc(fp);
	    *pp = c1;
	  }

	  if (c & 1) getc(fp);  /* odd length run: read an extra pad byte */
	}
      }  /* escape processing */
      if (FERROR(fp)) break;
    }  /* while */
  }

  else {
    fprintf(stderr,"unknown BMP compression type 0x%0x\n", comp);
  }

  if (FERROR(fp)) rv = 1;
  return rv;
}



/*******************************************/
static int loadBMP16(fp, pic24, w, h, mask)
     FILE  *fp;
     byte  *pic24;
     u_int w, h, *mask;
{
  int	 x, y;
  byte	*pp = pic24 + ((h - 1) * w * 3);
  size_t l = w*h*3;
  u_int	 buf, colormask[6];
  int	 i, bit, bitshift[6], colorbits[6], bitshift2[6];

  if (mask == NULL) {  /* RGB555 */
    colormask[0] = 0x00007c00;
    colormask[1] = 0x000003e0;
    colormask[2] = 0x0000001f;
    colormask[3] = 0x7c000000;
    colormask[4] = 0x03e00000;
    colormask[5] = 0x001f0000;
    bitshift[0] =  7;	bitshift2[0] = 0;
    bitshift[1] =  2;	bitshift2[1] = 0;
    bitshift[2] =  0;	bitshift2[2] = 3;
    bitshift[3] = 23;	bitshift2[3] = 0;
    bitshift[4] = 18;	bitshift2[4] = 0;
    bitshift[5] = 13;	bitshift2[5] = 0;
  } else {
    colormask[0] = mask[0];
    colormask[1] = mask[1];
    colormask[2] = mask[2];
    colormask[3] = (mask[0] & 0xffff) << 16;
    colormask[4] = (mask[1] & 0xffff) << 16;
    colormask[5] = (mask[2] & 0xffff) << 16;

    for (i = 0; i < 3; ++i) {
      buf = colormask[i];

      bitshift[i] = 0;
      for (bit = 0; bit < 32; ++bit) {
	if (buf & 1)
	  break;
	else
	  ++bitshift[i];
	buf >>= 1;
      }
      bitshift[i+3] = bitshift[i] + 16;

      colorbits[i] = 0;
      for (; bit < 32; ++bit) {
	if (buf & 1)
	  ++colorbits[i];
	else
	  break;
	buf >>= 1;
      }
      if (colorbits[i] > 8) { /* over 8-bit depth */
	bitshift[i] += (colorbits[i] - 8);
	bitshift[i+3] = bitshift[i] + 16;
	bitshift2[i] = bitshift2[i+3] = 0;
      } else
	bitshift2[i] = bitshift2[i+3] = 8 - colorbits[i];
    }
  }

  if (DEBUG > 1)
    fprintf(stderr, "loadBMP16: bitfields\n"
	    "\tR: bits = %2d, mask = %08x, shift >>%2d, <<%2d\n"
	    "\t             (mask = %08x, shift >>%2d, <<%2d)\n"
	    "\tG: bits = %2d, mask = %08x, shift >>%2d, <<%2d\n"
	    "\t             (mask = %08x, shift >>%2d, <<%2d)\n"
	    "\tB: bits = %2d, mask = %08x, shift >>%2d, <<%2d\n"
	    "\t             (mask = %08x, shift >>%2d, <<%2d)\n",
	    colorbits[0], colormask[0], bitshift[0], bitshift2[0],
	    colormask[3], bitshift[3], bitshift2[3],
	    colorbits[1], colormask[1], bitshift[1], bitshift2[1],
	    colormask[4], bitshift[4], bitshift2[4],
	    colorbits[2], colormask[2], bitshift[2], bitshift2[2],
	    colormask[5], bitshift[5], bitshift2[5]);

  for (y = h-1; y >= 0 && (pp - pic24 <= l); y--) {
    pp = pic24 + (3 * w * y);
    if ((y&0x3f)==0) WaitCursor();

    for (x = w; x > 1; x -= 2) {
      buf = getint(fp);
      *(pp++) = (buf & colormask[0]) >> bitshift[0] << bitshift2[0];
      *(pp++) = (buf & colormask[1]) >> bitshift[1] << bitshift2[1];
      *(pp++) = (buf & colormask[2]) >> bitshift[2] << bitshift2[2];
      *(pp++) = (buf & colormask[3]) >> bitshift[3] << bitshift2[3];
      *(pp++) = (buf & colormask[4]) >> bitshift[4] << bitshift2[4];
      *(pp++) = (buf & colormask[5]) >> bitshift[5] << bitshift2[5];
    }
    if (w & 1) { /* padded to 2 pix */
      buf = getint(fp);
      *(pp++) = (buf & colormask[0]) >> bitshift[0];
      *(pp++) = (buf & colormask[1]) >> bitshift[1];
      *(pp++) = (buf & colormask[2]) >> bitshift[2];
    }
  }

  return FERROR(fp)? 1 : 0;
}



/*******************************************/
static int loadBMP24(fp, pic24, w, h, bits)   /* also handles 32-bit BI_RGB */
     FILE *fp;
     byte *pic24;
     u_int  w,h, bits;
{
  int   i,j,padb,rv;
  byte *pp = pic24 + ((h - 1) * w * 3);
  size_t l = w*h*3;

  rv = 0;

  padb = (4 - ((w*3) % 4)) & 0x03;  /* # of pad bytes to read at EOscanline */
  if (bits==32) padb = 0;

  for (i=h-1; i>=0; i--) {
    pp = pic24 + (i * w * 3);
    if ((i&0x3f)==0) WaitCursor();

    for (j=0; j<w && (pp - pic24 <= l); j++) {
      pp[2] = getc(fp);   /* blue */
      pp[1] = getc(fp);   /* green */
      pp[0] = getc(fp);   /* red */
      if (bits==32) getc(fp);
      pp += 3;
    }

    for (j=0; j<padb; j++) getc(fp);

    rv = (FERROR(fp));
    if (rv) break;
  }

  return rv;
}



/*******************************************/
static int loadBMP32(fp, pic24, w, h, colormask) /* 32-bit BI_BITFIELDS only */
     FILE  *fp;
     byte  *pic24;
     u_int w, h, *colormask;
{
  int	 x, y;
  byte	*pp;
  u_int	 buf;
  int	 i, bit, bitshift[3], colorbits[3], bitshift2[3];

  for (i = 0; i < 3; ++i) {
    buf = colormask[i];

    bitshift[i] = 0;
    for (bit = 0; bit < 32; ++bit) {
      if (buf & 1)
	break;
      else
	++bitshift[i];
      buf >>= 1;
    }

    colorbits[i] = 0;
    for (; bit < 32; ++bit) {
      if (buf & 1)
	++colorbits[i];
      else
	break;
      buf >>= 1;
    }
    if (colorbits[i] > 8) {  /* over 8-bit depth */
      bitshift[i] += (colorbits[i] - 8);
      bitshift2[i] = 0;
    } else
      bitshift2[i] = 8 - colorbits[i];
  }

  if (DEBUG > 1)
    fprintf(stderr, "loadBMP32: bitfields\n"
	    "\tR: bits = %2d, mask = %08x, shift >>%2d, <<%2d\n"
	    "\tG: bits = %2d, mask = %08x, shift >>%2d, <<%2d\n"
	    "\tB: bits = %2d, mask = %08x, shift >>%2d, <<%2d\n",
	    colorbits[0], colormask[0], bitshift[0], bitshift2[0],
	    colorbits[1], colormask[1], bitshift[1], bitshift2[1],
	    colorbits[2], colormask[2], bitshift[2], bitshift2[2]);

  for (y = h-1; y >= 0; y--) {
    pp = pic24 + (3 * w * y);
    if ((y&0x3f)==0) WaitCursor();

    for(x = w; x > 0; x --) {
      buf = getint(fp);
      *(pp++) = (buf & colormask[0]) >> bitshift[0] << bitshift2[0];
      *(pp++) = (buf & colormask[1]) >> bitshift[1] << bitshift2[1];
      *(pp++) = (buf & colormask[2]) >> bitshift[2] << bitshift2[2];
    }
  }

  return FERROR(fp)? 1 : 0;
}



/*******************************************/
static u_int getshort(fp)
     FILE *fp;
{
  int c, c1;
  c = getc(fp);  c1 = getc(fp);
  return ((u_int) c) + (((u_int) c1) << 8);
}


/*******************************************/
static u_int getint(fp)
     FILE *fp;
{
  int c, c1, c2, c3;
  c = getc(fp);  c1 = getc(fp);  c2 = getc(fp);  c3 = getc(fp);
  return  ((u_int) c) +
	 (((u_int) c1) << 8) +
	 (((u_int) c2) << 16) +
	 (((u_int) c3) << 24);
}


/*******************************************/
static void putshort(fp, i)
     FILE *fp;
     int i;
{
  int c, c1;

  c = ((u_int) i) & 0xff;  c1 = (((u_int) i)>>8) & 0xff;
  putc(c, fp);   putc(c1,fp);
}


/*******************************************/
static void putint(fp, i)
     FILE *fp;
     int i;
{
  int c, c1, c2, c3;
  c  =  ((u_int) i)      & 0xff;
  c1 = (((u_int) i)>>8)  & 0xff;
  c2 = (((u_int) i)>>16) & 0xff;
  c3 = (((u_int) i)>>24) & 0xff;

  putc(c, fp);   putc(c1,fp);  putc(c2,fp);  putc(c3,fp);
}




static byte pc2nc[256],r1[256],g1[256],b1[256];


/*******************************************/
int WriteBMP(fp,pic824,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle)
     FILE *fp;
     byte *pic824;
     int   ptype,w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
{
  /*
   * if PIC8, and colorstyle == F_FULLCOLOR, F_GREYSCALE, or F_REDUCED,
   * the program writes an uncompressed 4- or 8-bit image (depending on
   * the value of numcols)
   *
   * if PIC24, and colorstyle == F_FULLCOLOR, program writes an uncompressed
   *    24-bit image
   * if PIC24 and colorstyle = F_GREYSCALE, program writes an uncompressed
   *    8-bit image
   * note that PIC24 and F_BWDITHER/F_REDUCED won't happen
   *
   * if colorstyle == F_BWDITHER, it writes a 1-bit image
   *
   */

  int i,j, nc, nbits, bperlin, cmaplen, npixels;
  byte *graypic, *sp, *dp, graymap[256];

  nc = nbits = cmaplen = 0;
  graypic = NULL;

  if (ptype == PIC24 && colorstyle == F_GREYSCALE) {
    /* generate a faked 8-bit per pixel image with a grayscale cmap,
       so that it can just fall through existing 8-bit code */

    npixels = w * h;
    if (w <= 0 || h <= 0 || npixels/w != h) {
      SetISTR(ISTR_WARNING, "image dimensions too large");
      return -1;
    }

    graypic = (byte *) malloc((size_t) npixels);
    if (!graypic) FatalError("unable to malloc in WriteBMP()");

    for (i=0,sp=pic824,dp=graypic; i<npixels; i++,sp+=3, dp++) {
      *dp = MONO(sp[0],sp[1],sp[2]);
    }

    for (i=0; i<256; i++) graymap[i] = i;
    rmap = gmap = bmap = graymap;
    numcols = 256;
    ptype = PIC8;

    pic824 = graypic;
  }


  if (ptype == PIC24) {  /* is F_FULLCOLOR */
    nbits = 24;
    cmaplen = 0;
    nc = 0;
  }

  else if (ptype == PIC8) {
    /* we may have duplicate colors in the colormap, and we'd prefer not to.
     * build r1,g1,b1 (a contiguous, minimum set colormap), and pc2nc[], a
     * array that maps 'pic8' values (0-numcols) into corresponding values
     * in the r1,g1,b1 colormaps (0-nc)
     */

    for (i=0; i<256; i++) { pc2nc[i] = r1[i] = g1[i] = b1[i] = 0; }

    nc = 0;
    for (i=0; i<numcols; i++) {
      /* see if color #i is a duplicate */
      for (j=0; j<i; j++) {
	if (rmap[i] == rmap[j] && gmap[i] == gmap[j] &&
	    bmap[i] == bmap[j]) break;
      }

      if (j==i) {  /* wasn't found */
	pc2nc[i] = nc;
	r1[nc] = rmap[i];
	g1[nc] = gmap[i];
	b1[nc] = bmap[i];
	nc++;
      }
      else pc2nc[i] = pc2nc[j];
    }

    /* determine how many bits per pixel we'll be writing */
    if (colorstyle == F_BWDITHER || nc <= 2) nbits = 1;
    else if (nc<=16) nbits = 4;
    else nbits = 8;

    cmaplen = 1<<nbits;                      /* # of entries in cmap */
  }


  bperlin = ((w * nbits + 31) / 32) * 4;   /* # bytes written per line */

  putc('B', fp);  putc('M', fp);           /* BMP file magic number */

  /* compute filesize and write it */
  i = 14 +                /* size of bitmap file header */
      40 +                /* size of bitmap info header */
      (nc * 4) +          /* size of colormap */
      bperlin * h;        /* size of image data */

  putint(fp, i);
  putshort(fp, 0);        /* reserved1 */
  putshort(fp, 0);        /* reserved2 */
  putint(fp, 14 + 40 + (nc * 4));  /* offset from BOfile to BObitmap */

  putint(fp, 40);         /* biSize: size of bitmap info header */
  putint(fp, w);          /* biWidth */
  putint(fp, h);          /* biHeight */
  putshort(fp, 1);        /* biPlanes:  must be '1' */
  putshort(fp, nbits);    /* biBitCount: 1,4,8, or 24 */
  putint(fp, BI_RGB);     /* biCompression:  BI_RGB, BI_RLE8 or BI_RLE4 */
  putint(fp, bperlin*h);  /* biSizeImage:  size of raw image data */
  putint(fp, 75 * 39);    /* biXPelsPerMeter: (75dpi * 39" per meter) */
  putint(fp, 75 * 39);    /* biYPelsPerMeter: (75dpi * 39" per meter) */
  putint(fp, nc);         /* biClrUsed: # of colors used in cmap */
  putint(fp, nc);         /* biClrImportant: same as above */


  /* write out the colormap */
  for (i=0; i<nc; i++) {
    if (colorstyle == F_GREYSCALE) {
      j = MONO(r1[i],g1[i],b1[i]);
      putc(j,fp);  putc(j,fp);  putc(j,fp);  putc(0,fp);
    }
    else {
      putc(b1[i],fp);
      putc(g1[i],fp);
      putc(r1[i],fp);
      putc(0,fp);
    }
  }

  /* write out the image */
  if      (nbits ==  1) writeBMP1 (fp, pic824, w, h);
  else if (nbits ==  4) writeBMP4 (fp, pic824, w, h);
  else if (nbits ==  8) writeBMP8 (fp, pic824, w, h);
  else if (nbits == 24) writeBMP24(fp, pic824, w, h);

  if (graypic) free(graypic);

#ifndef VMS
  if (FERROR(fp)) return -1;
#else
  if (!FERROR(fp)) return -1;
#endif

  return 0;
}




/*******************************************/
static void writeBMP1(fp, pic8, w, h)
     FILE *fp;
     byte *pic8;
     int  w,h;
{
  int   i,j,c,bitnum,padw;
  byte *pp;

  padw = ((w + 31)/32) * 32;  /* 'w', padded to be a multiple of 32 */

  for (i=h-1; i>=0; i--) {
    pp = pic8 + (i * w);
    if ((i&0x3f)==0) WaitCursor();

    for (j=bitnum=c=0; j<=padw; j++,bitnum++) {
      if (bitnum == 8) { /* write the next byte */
	putc(c,fp);
	bitnum = c = 0;
      }

      c <<= 1;

      if (j<w) {
	c |= (pc2nc[*pp++] & 0x01);
      }
    }
  }
}



/*******************************************/
static void writeBMP4(fp, pic8, w, h)
     FILE *fp;
     byte *pic8;
     int  w,h;
{
  int   i,j,c,nybnum,padw;
  byte *pp;


  padw = ((w + 7)/8) * 8; /* 'w' padded to a multiple of 8pix (32 bits) */

  for (i=h-1; i>=0; i--) {
    pp = pic8 + (i * w);
    if ((i&0x3f)==0) WaitCursor();

    for (j=nybnum=c=0; j<=padw; j++,nybnum++) {
      if (nybnum == 2) { /* write next byte */
	putc((c&0xff), fp);
	nybnum = c = 0;
      }

      c <<= 4;

      if (j<w) {
	c |= (pc2nc[*pp] & 0x0f);
	pp++;
      }
    }
  }
}



/*******************************************/
static void writeBMP8(fp, pic8, w, h)
     FILE *fp;
     byte *pic8;
     int  w,h;
{
  int   i,j,padw;
  byte *pp;

  padw = ((w + 3)/4) * 4; /* 'w' padded to a multiple of 4pix (32 bits) */

  for (i=h-1; i>=0; i--) {
    pp = pic8 + (i * w);
    if ((i&0x3f)==0) WaitCursor();

    for (j=0; j<w; j++) putc(pc2nc[*pp++], fp);
    for ( ; j<padw; j++) putc(0, fp);
  }
}


/*******************************************/
static void writeBMP24(fp, pic24, w, h)
     FILE *fp;
     byte *pic24;
     int  w,h;
{
  int   i,j,padb;
  byte *pp;

  padb = (4 - ((w*3) % 4)) & 0x03;  /* # of pad bytes to write at EOscanline */

  for (i=h-1; i>=0; i--) {
    pp = pic24 + (i * w * 3);
    if ((i&0x3f)==0) WaitCursor();

    for (j=0; j<w; j++) {
      putc(pp[2], fp);
      putc(pp[1], fp);
      putc(pp[0], fp);
      pp += 3;
    }

    for (j=0; j<padb; j++) putc(0, fp);
  }
}






/*******************************************/
static int bmpError(fname, st)
     const char *fname, *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  return 0;
}



