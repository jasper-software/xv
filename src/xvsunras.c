/*
 * xvsunras.c - load routine for 'sun rasterfile' format pictures
 *
 * LoadSunRas(fname, numcols)  -  loads a PM pic, does 24to8 code if nec.
 * WriteSunRas(fp, pic, w, h, r,g,b, numcols, style)
 * WriteRaw(fp, pic, w, h, r,g,b, numcols, style)
 *
 * This file written by Dave Heath (heath@cs.jhu.edu)
 * fixBGR() added by Ken Rossman (ken@shibuya.cc.columbia.edu)
 */


#include "xv.h"

/* info on sun rasterfiles taken from rasterfile(5) man page */

#define   RAS_MAGIC 0x59a66a95

struct rasterfile {
     int  ras_magic;
     int  ras_width;
     int  ras_height;
     int  ras_depth;
     int  ras_length;
     int  ras_type;
     int  ras_maptype;
     int  ras_maplength;
};

#define RT_OLD          0       /* Raw pixrect image in 68000 byte order */
#define RT_STANDARD     1       /* Raw pixrect image in 68000 byte order */
#define RT_BYTE_ENCODED 2       /* Run-length compression of bytes */
#define RT_FORMAT_RGB   3       /* XRGB or RGB instead of XBGR or BGR */

#define RMT_RAW		2
#define RMT_NONE	0
#define RMT_EQUAL_RGB	1

#define RAS_RLE 0x80


static int  sunRasError    PARM((char *, char *));
static int  rle_read       PARM((byte *, int, int, FILE *, int));
static void sunRas1to8     PARM((byte *, byte *, int));
static void sunRas8to1     PARM((byte *, byte *, int, int));
static int  read_sun_long  PARM((int *, FILE *));
static int  write_sun_long PARM((int, FILE *));
static void fixBGR         PARM((unsigned char *, int, int));


/*******************************************/
int LoadSunRas(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
{
  FILE	*fp;
  int	 linesize,lsize,csize,isize,i,w,h,d;
  byte	 *image, *line, *pic8;
  struct rasterfile sunheader;
  char   *bname;

  bname = BaseName(fname);

  /* read in the Sun Rasterfile picture */
  fp = xv_fopen(fname,"r");
  if (!fp) return( sunRasError(bname, "unable to open file") );

  read_sun_long (&sunheader.ras_magic	 , fp);
  read_sun_long (&sunheader.ras_width	 , fp);
  read_sun_long (&sunheader.ras_height	 , fp);
  read_sun_long (&sunheader.ras_depth	 , fp);
  read_sun_long (&sunheader.ras_length	 , fp);
  read_sun_long (&sunheader.ras_type	 , fp);
  read_sun_long (&sunheader.ras_maptype  , fp);
  read_sun_long (&sunheader.ras_maplength, fp);

  if (sunheader.ras_magic != RAS_MAGIC) {
    fclose(fp);
    return( sunRasError(bname, "not a Sun rasterfile") );
  }


  /* make sure that the input picture can be dealt with */
  if (sunheader.ras_depth != 1 &&
      sunheader.ras_depth != 8 &&
      sunheader.ras_depth != 24 &&
      sunheader.ras_depth != 32) {
    fprintf (stderr, "Sun rasterfile image has depth %d\n", 
	     sunheader.ras_depth);
    fprintf (stderr, "Depths supported are 1, 8, 24, and 32\n");
    fclose(fp);
    return 0;
  }


  if (sunheader.ras_type != RT_OLD &&
      sunheader.ras_type != RT_STANDARD &&
      sunheader.ras_type != RT_BYTE_ENCODED &&
      sunheader.ras_type != RT_FORMAT_RGB) {
    fprintf (stderr, "Sun rasterfile of unsupported type %d\n",
	     sunheader.ras_type);
    fclose(fp);
    return 0;
  }


  if (sunheader.ras_maptype != RMT_RAW &&
      sunheader.ras_maptype != RMT_NONE &&
      sunheader.ras_maptype != RMT_EQUAL_RGB) {
    fprintf (stderr, "Sun rasterfile colormap of unsupported type %d\n",
	     sunheader.ras_maptype);
    fclose(fp);
    return 0;
  }

  w = sunheader.ras_width;
  h = sunheader.ras_height;
  d = sunheader.ras_depth;
  isize = sunheader.ras_length ?
	  sunheader.ras_length :
	  (w * h * d) / 8;
  csize = (sunheader.ras_maptype == RMT_NONE) ? 0 : sunheader.ras_maplength;


  /* compute length of the output (xv-format) image */
  lsize = w * h;     
  if (d == 24 || d == 32) lsize = lsize * 3;


  linesize = w * d;
  if (linesize % 16) linesize += (16 - (linesize % 16));
  linesize /= 8;

  if (DEBUG) {
    fprintf(stderr,"%s: LoadSunRas() - loading a %dx%d pic, %d planes\n",
	    cmd, w, h, d);
    fprintf (stderr, 
	  "type %d, maptype %d, isize %d, csize %d, lsize %d, linesize %d\n",
	     sunheader.ras_type, sunheader.ras_maptype,
	     isize, csize, lsize, linesize);
  }


  /* read in the colormap, if any */
  if (sunheader.ras_maptype == RMT_EQUAL_RGB && csize) {
    fread (pinfo->r, (size_t) 1, (size_t) sunheader.ras_maplength/3, fp);
    fread (pinfo->g, (size_t) 1, (size_t) sunheader.ras_maplength/3, fp);
    fread (pinfo->b, (size_t) 1, (size_t) sunheader.ras_maplength/3, fp);
  }

  else if (sunheader.ras_maptype == RMT_RAW && csize) {
    /* we don't know how to handle raw colormap, ignore */
    fseek (fp, (long) csize, 1);
  }

  else {  /* no colormap, make one up */
    if (sunheader.ras_depth == 1) {
      pinfo->r[0] = pinfo->g[0] = pinfo->b[0] = 0;
      pinfo->r[1] = pinfo->g[1] = pinfo->b[1] = 255;
    }

    else if (sunheader.ras_depth == 8) {
      for (i = 0; i < 256; i++)
	pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
    }
  }


  /* allocate memory for picture and read it in */
  /* note we may slightly overallocate here (if image is padded) */
  image = (byte *) malloc ((size_t) lsize);
  line  = (byte *) malloc ((size_t) linesize);
  if (image == NULL || line == NULL)
    FatalError("Can't allocate memory for image\n");


  for (i = 0; i < h; i++) {
    if ((i&0x1f) == 0) WaitCursor();
    if (sunheader.ras_type == RT_BYTE_ENCODED) {
      if (rle_read (line, 1, linesize, fp, (i==0)) != linesize) break;
    }

    else {
      if (fread (line, (size_t) 1, (size_t) linesize, fp) != linesize) {
	free(image);  free(line);  fclose(fp);
	return (sunRasError (bname, "file read error"));
      }
    }

    switch (d) {
    case 1:  sunRas1to8 (image + w * i, line, w);	                
             break;
    case 8:  xvbcopy((char *) line, (char *) image + w * i, (size_t) w);
             break;
    case 24: xvbcopy((char *) line, (char *) image + w * i * 3, (size_t) w*3);
             break;
      
    case 32:
      {
	int k;
	byte *ip, *op;
	ip = line;
	op = (byte *) (image + w * i * 3);
	for (k = 0; k<w; k++) {
	  *ip++;           /* skip 'alpha' */
	  *op++ = *ip++;   /* red   */
	  *op++ = *ip++;   /* green */
	  *op++ = *ip++;   /* blue  */
	}
      }
    }
  }
  
  free(line);
  
  if (DEBUG) fprintf(stderr,"Sun ras: image loaded!\n");



  if (d == 24 || d == 32) {
    if (sunheader.ras_type != RT_FORMAT_RGB) fixBGR(image,w,h);
    pinfo->type = PIC24;
  }
  else pinfo->type = PIC8;

  pinfo->pic = image;
  pinfo->w = w;  
  pinfo->h = h;
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
  pinfo->frmType = F_SUNRAS;
  pinfo->colType = (d==1) ? F_BWDITHER : F_FULLCOLOR;
  sprintf(pinfo->fullInfo, "Sun %s rasterfile.  (%d plane%s)  (%ld bytes)",
	  sunheader.ras_type == RT_BYTE_ENCODED ? "rle" : "standard",
	  d, d == 1 ? "" : "s",
	  (long) (sizeof(struct rasterfile) + csize + isize));

  sprintf(pinfo->shrtInfo, "%dx%d Sun Rasterfile.",w,h);
  pinfo->comment = (char *) NULL;

  fclose(fp);
  return 1;
}


/*****************************/
static int rle_read (ptr, size, nitems, fp, init)
byte *ptr;
int size, nitems,init;
FILE *fp;
{
  static int count, ch;
  int readbytes, c, read;

  if (init) { count = ch = 0; }

  readbytes = size * nitems;
  for (read = 0; read < readbytes; read++) {
    if (count) {
      *ptr++ = (byte) ch;
      count--;
    }

    else {
      c = getc(fp);
      if (c == EOF) break;

      if (c == RAS_RLE) {   /* 0x80 */
	count = getc(fp);
	if (count == EOF) break;

	if (count < 0) count &= 0xff;
	if (count == 0) *ptr++ = c;
        else {
          if ((ch = getc(fp)) == EOF) break;
          *ptr++ = ch;
        }
      }
      else *ptr++ = c;
    }
  }

  return (read/size);
}


/*****************************/
static int sunRasError(fname, st)
     char *fname, *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  return 0;
}


/************************************************/
static void sunRas1to8 (dest, src, len)
byte *dest, *src;
int len;
{
  int i, b;
  int c = 0;

  for (i = 0, b = -1; i < len; i++) {
    if (b < 0) {
      b = 7;
      c = ~*src++;
    }
    *dest++ = ((c >> (b--)) & 1);
  }
}



static void sunRas8to1 (dest, src, len, flip)
byte *dest, *src;
int len, flip;
{
  int i, b;
  int c;

  for (c = b = i = 0; i < len; i++) {
    c <<= 1;
    c |= (*src++ ? 1 : 0);
    if (b++ == 7) {
      if (flip) c = ~c;
      *dest++ = (byte) (c & 0xff);
      b = c = 0;
    }
  }
  if (b) {
    if (flip) c = ~c;
    *dest = (byte) ((c<<(8-b)) & 0xff);
  }
}




/*******************************************/
int WriteSunRas(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,userle)
     FILE *fp;
     byte *pic;
     int   ptype,w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle, userle;
{
  /* writes a sun rasterfile to the already open stream
     writes either 24-bit, 8-bit or 1-bit
     currently will not write rle files

     if PIC24 and F_GREYSCALE, writes an 8-bit grayscale image

     biggest problem w/ rle file: should we compute
     image size first (nicer) or go back and write it
     in when we are done (kludgy)?
   */

  struct rasterfile sunheader;
  int   linesize, i, color, d, y, flipbw;
  byte *line, *graypic, graymap[256], *sp, *dp;

  graypic = NULL;
  flipbw  = 0;

  /* special case: if PIC24 and writing GREYSCALE, write 8-bit file */
  if (ptype == PIC24  && colorstyle == F_GREYSCALE) {
    graypic = (byte *) malloc((size_t) w*h);
    if (!graypic) FatalError("unable to malloc in WriteSunRas()");
    
    for (i=0,sp=pic,dp=graypic; i<w*h; i++,sp+=3,dp++) {
      *dp = MONO(sp[0],sp[1],sp[2]);
    }

    for (i=0; i<256; i++) graymap[i] = i;
    rmap = gmap = bmap = graymap;
    numcols = 256;
    ptype = PIC8;
    pic = graypic;
  }


  if      (ptype==PIC24)    { d = 24;  linesize = w * 3; }
  else if (colorstyle != F_BWDITHER) { d = 8;   linesize = w;     }
  else { 
    d = 1;
    linesize = w;
    if (linesize % 8) linesize += (8 - linesize % 8);
    linesize /= 8;
  }



  if (linesize % 2) linesize++;
  line = (byte *) malloc((size_t) linesize);
  if (!line) {
    SetISTR(ISTR_WARNING, "Can't allocate memory for save!\n");
    if (graypic) free(graypic);
    return (1);
  }

  if (DEBUG)
    fprintf (stderr,
	     "WriteSunRas: d %d, linesize %d numcols %d\n",
	     d, linesize, numcols);

  if (d==1) {
    /* set flipbw if color#0 is black */
    flipbw = (MONO(rmap[0],gmap[0],bmap[0]) < MONO(rmap[1],gmap[1],bmap[1]));
  }

  /* set up the header */
  sunheader.ras_magic	  = RAS_MAGIC;
  sunheader.ras_width	  = w;
  sunheader.ras_height	  = h;
  sunheader.ras_depth	  = d;
  sunheader.ras_length	  = linesize * h;
  sunheader.ras_type	  = RT_STANDARD;
  sunheader.ras_maptype   = (d==1 || d==24) ? RMT_NONE : RMT_EQUAL_RGB;
  sunheader.ras_maplength = (d==1 || d==24) ? 0 : 3 * numcols;

  write_sun_long (sunheader.ras_magic	 , fp);
  write_sun_long (sunheader.ras_width	 , fp);
  write_sun_long (sunheader.ras_height	 , fp);
  write_sun_long (sunheader.ras_depth	 , fp);
  write_sun_long (sunheader.ras_length	 , fp);
  write_sun_long (sunheader.ras_type	 , fp);
  write_sun_long (sunheader.ras_maptype  , fp);
  write_sun_long (sunheader.ras_maplength, fp);

  /* write the colormap */
  if (d == 8)
    if (colorstyle == 1)  /* grayscale */
      for (color=0; color<3; color++)
	for (i=0; i<numcols; i++)
	  putc (MONO(rmap[i],gmap[i],bmap[i]), fp);
    else {
      fwrite (rmap, sizeof(byte), (size_t) numcols, fp);
      fwrite (gmap, sizeof(byte), (size_t) numcols, fp);
      fwrite (bmap, sizeof(byte), (size_t) numcols, fp);
    }


  /* write the image */
  line[linesize-1] = 0;
  for (y = 0; y < h; y++) {
    if ((y&0x1f) == 0) WaitCursor();

    if (d == 24) {
      byte *lptr, *pix;

      for (i=0,lptr=line,pix=pic+y*w*3; i<w; i++,pix+=3) {
	*lptr++ = pix[2];          /* write date out in BGR order */
	*lptr++ = pix[1];
	*lptr++ = pix[0];
      }
    }

    else if (d == 8)
      xvbcopy((char *) pic + y * w, (char *) line, (size_t) w);

    else /* d == 1 */
      sunRas8to1 (line, pic + y * w, w, flipbw);

    if (fwrite (line, (size_t) 1, (size_t) linesize, fp) != linesize) {
      SetISTR(ISTR_WARNING, "Write failed during save!\n");
      if (graypic) free(graypic);
      free(line);
      return (2);
    }
  }

  free (line);
  if (graypic) free(graypic);
  return (0);
}


/* reads a 4-byte int in Sun byteorder
   returns 0 for success, EOF for failure */
static int read_sun_long (l, fp)
     int *l;
     FILE *fp;
{
  int c0, c1, c2, c3;

  c0 = fgetc(fp);
  c1 = fgetc(fp);
  c2 = fgetc(fp);
  c3 = fgetc(fp);

  *l = (((u_long) c0 & 0xff) << 24) |
       (((u_long) c1 & 0xff) << 16) |
       (((u_long) c2 & 0xff) <<  8) |
       (((u_long) c3 & 0xff));

  if (ferror(fp) || feof(fp)) return EOF;

  return 0;
}


/* write a long word in sun byte-order
   returns 0 for success, EOF for failure
 */
static int write_sun_long (l, fp)
int l;
FILE *fp;
{
    char c;

    c = ((l >> 24) & 0xff);
    if (putc (c, fp) == EOF) return (EOF);
    c = ((l >> 16) & 0xff);
    if (putc (c, fp) == EOF) return (EOF);
    c = ((l >> 8) & 0xff);
    if (putc (c, fp) == EOF) return (EOF);
    c = (l & 0xff);
    if (putc (c, fp) == EOF) return (EOF);
    return (0);
}




/* kr3 - fix up BGR order SUN 24-bit rasters to be RGB order */
static void fixBGR(img,w,h)
unsigned char *img;
int w,h;
{
  int i,npixels;
  unsigned char tmp;

  npixels = w*h;
  for (i=0; i<npixels; i++) {
    tmp = img[0];                   /* swap red and blue channels */
    img[0] = img[2];
    img[2] = tmp;
    img += 3;                       /* bump to next pixel */
  }
}

