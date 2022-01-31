/*
 * xviris.c - load routine for IRIS 'rgb' format pictures
 *
 * LoadIRIS()
 * WriteIRIS()
 */

#include "copyright.h"

/*  based on:
 *
 *   	fastimg -
 *		Faster reading and writing of image files.
 *
 *      This code should work on machines with any byte order.
 *
 *	Could someone make this run real fast using multiple processors 
 *	or how about using memory mapped files to speed it up?
 *
 *				Paul Haeberli - 1991
 */

#include "xv.h"

#define IMAGIC 	0732

#define BPPMASK			0x00ff
#define ITYPE_VERBATIM		0x0000
#define ITYPE_RLE		0x0100
#define ISRLE(type)		(((type) & 0xff00) == ITYPE_RLE)
#define ISVERBATIM(type)	(((type) & 0xff00) == ITYPE_VERBATIM)
#define BPP(type)		((type) & BPPMASK)
#define RLE(bpp)		(ITYPE_RLE | (bpp))
#define VERBATIM(bpp)		(ITYPE_VERBATIM | (bpp))

#define FERROR(fp) (ferror(fp) || feof(fp))

typedef struct {
    u_short	imagic;		/* stuff saved on disk . . */
    u_short 	type;
    u_short 	dim;
    u_short 	xsize;
    u_short 	ysize;
    u_short 	zsize;
    u_long 	min;
    u_long 	max;
    u_long	wastebytes;	
    char 	name[80];
    u_long	colormap;

    long 	file;		/* stuff used in core only */
    u_short 	flags;
    short	dorev;
    short	x;
    short	y;
    short	z;
    short	cnt;
    u_short	*ptr;
    u_short	*base;
    u_short	*tmpbuf;
    u_long	offset;
    u_long	rleend;		/* for rle images */
    u_long	*rowstart;	/* for rle images */
    long	*rowsize;	/* for rle images */
} IMAGE;


#define TAGLEN	(5)

#define RINTLUM (79)
#define GINTLUM (156)
#define BINTLUM (21)

#define OFFSET_R	3	/* this is byte order dependent */
#define OFFSET_G	2
#define OFFSET_B	1
#define OFFSET_A	0

#define ILUM(r,g,b)     ((int)(RINTLUM*(r)+GINTLUM*(g)+BINTLUM*(b))>>8)
#define CHANOFFSET(z)	(3-(z))	/* this is byte order dependent */


static int      irisError     PARM((char *, char *));
static byte    *getimagedata  PARM((FILE *, IMAGE *));
static void     interleaverow PARM((byte *, byte *, int, int));
static void     expandrow     PARM((byte *, byte *, int));
static void     readtab       PARM((FILE *, u_long *, int));
static void     addimgtag     PARM((byte *, int, int));

static void     lumrow        PARM((byte *, byte *, int));
static int      compressrow   PARM((byte *, byte *, int, int));
static void     writetab      PARM((FILE *, u_long *, int));

static u_short  getshort      PARM((FILE *));
static u_long   getlong       PARM((FILE *));
static void     putshort      PARM((FILE *, int));
static void     putlong       PARM((FILE *, u_long));


static char *loaderr;
static char *bname;

/*****************************************************/
int LoadIRIS(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*****************************************************/
{
  /* returns '1' on success, '0' on failure */

  FILE   *fp;
  IMAGE   img;
  byte   *rawdata, *rptr;
  byte   *pic824,  *bptr;
  int     trunc, i, j;
  long    filesize;

  trunc = 0;
  xvbzero((char *) &img, sizeof(IMAGE));

  bname = BaseName(fname);

  /* open the file */
  fp = xv_fopen(fname, "r");
  if (!fp) return(irisError(bname, "can't open file"));

  /* figure out the file size */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);

  /* read header information from file */
  img.imagic = getshort(fp);
  img.type   = getshort(fp);
  img.dim    = getshort(fp);
  img.xsize  = getshort(fp);
  img.ysize  = getshort(fp);
  img.zsize  = getshort(fp);

  if (FERROR(fp)) {
    fclose(fp);
    return irisError(bname, "error in header info");
  }

  if (img.imagic != IMAGIC) {
    fclose(fp);
    return irisError(bname, "bad magic number");
  }

  rawdata = getimagedata(fp, &img);
  if (!rawdata) {   
    fclose(fp);
    if (loaderr) irisError(bname, loaderr);
    return 0;
  }

  if (FERROR(fp)) trunc = 1;   /* probably truncated file */

  fclose(fp);


  /* got the raw image data.  Convert to an XV image (1,3 bytes / pix) */


  if (img.zsize < 3) {  /* grayscale */
    pic824 = (byte *) malloc((size_t) img.xsize * img.ysize);
    if (!pic824) FatalError("couldn't malloc pic824 in LoadIRIS()");

    /* copy plane 3 from rawdata into pic824, inverting pic vertically */
    for (i=0, bptr=pic824; i<(int) img.ysize; i++) {
      rptr = rawdata + 3 + ((img.ysize - 1) - i) * (img.xsize * 4);
      for (j=0; j<(int) img.xsize; j++, bptr++, rptr+=4) *bptr = *rptr;
    }


    for (i=0; i<256; i++) 
      pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;

    pinfo->pic  = pic824;
    pinfo->type = PIC8;

    pinfo->frmType = F_IRIS;
    pinfo->colType = F_GREYSCALE;

    sprintf(pinfo->fullInfo, "IRIS Greyscale format%s  (%ld bytes)",
	    (ISRLE(img.type)) ? ", RLE compressed." : ".", filesize);
    sprintf(pinfo->shrtInfo, "%dx%d IRIS Greyscale.", img.xsize, img.ysize);
  }

  else {  /* truecolor */
    pic824 = (byte *) malloc((size_t) img.xsize * img.ysize * 3);
    if (!pic824) FatalError("couldn't malloc pic824 in LoadIRIS()");
    
    /* copy plane 3 from rawdata into pic824, inverting pic vertically */
    for (i=0, bptr=pic824; i<(int) img.ysize; i++) {
      rptr = rawdata + ((img.ysize - 1) - i) * (img.xsize * 4);
      for (j=0; j<(int) img.xsize; j++, rptr+=4) {
	*bptr++ = rptr[3];
	*bptr++ = rptr[2];
	*bptr++ = rptr[1];
      }
    }

    pinfo->pic = pic824;
    pinfo->type = PIC24;

    pinfo->frmType = F_IRIS;
    pinfo->colType = F_FULLCOLOR;
    sprintf(pinfo->fullInfo, "IRIS RGB format%s  (%ld bytes)",
	    (ISRLE(img.type)) ? ", RLE compressed." : ".", filesize);
    sprintf(pinfo->shrtInfo, "%dx%d IRIS RGB.", img.xsize, img.ysize);
  }

  free(rawdata);

  if (trunc) irisError(bname, "File appears to be truncated.");

  pinfo->w = img.xsize;  pinfo->h = img.ysize;
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
  pinfo->comment = (char *) NULL;

  return 1;
}     


/*******************************************/
static int irisError(name, st)
  char *name, *st;
{
  SetISTR(ISTR_WARNING,"%s: %s", name, st);
  return 0;
}


/****************************************************/
static byte *getimagedata(fp, img)
     FILE  *fp;
     IMAGE *img;
{
  /* read in a B/W RGB or RGBA iris image file and return a 
     pointer to an array of 4-byte pixels, arranged ABGR, NULL on error */

  byte   *base, *lptr;
  byte   *verdat;
  int     y, z, pos, len, tablen;
  int     xsize, ysize, zsize;
  int     bpp, rle, cur, badorder;
  int     rlebuflen;


  rle     = ISRLE(img->type);
  bpp     = BPP(img->type);
  loaderr = (char *) NULL;

  if (bpp != 1) {
    loaderr = "image must have 1 byte per pix chan";
    return (byte *) NULL;
  }

  xsize = img->xsize;
  ysize = img->ysize;
  zsize = img->zsize;

  if (rle) {
    byte *rledat;
    u_long *starttab, *lengthtab;

    rlebuflen = 2 * xsize + 10;
    tablen    = ysize * zsize;
    starttab  = (u_long *) malloc((size_t) tablen * sizeof(long));
    lengthtab = (u_long *) malloc((size_t) tablen * sizeof(long));
    rledat    = (byte *)   malloc((size_t) rlebuflen);

    if (!starttab || !lengthtab || !rledat) 
      FatalError("out of memory in LoadIRIS()");

    fseek(fp, 512L, 0);
    readtab(fp, starttab,  tablen);
    readtab(fp, lengthtab, tablen);

    if (FERROR(fp)) {
      loaderr = "error reading scanline tables";
      free(starttab);  free(lengthtab);  free(rledat);
      return (byte *) NULL;
    }


    /* check data order */
    cur = 0;
    badorder = 0;
    for (y=0; y<ysize && !badorder; y++) {
      for (z=0; z<zsize && !badorder; z++) {
	if (starttab[y+z*ysize] < cur) badorder = 1;
	else cur = starttab[y+z*ysize];
      }
    }

    fseek(fp, (long) (512 + 2*tablen*4), 0);
    cur = 512 + 2*tablen*4;

    base = (byte *) malloc((size_t) (xsize*ysize+TAGLEN) * 4);
    if (!base) FatalError("out of memory in LoadIRIS()");

    addimgtag(base,xsize,ysize);

    if (badorder) {
      for (z=0; z<zsize; z++) {
	lptr = base;
	for (y=0; y<ysize; y++) {
	  if (cur != starttab[y+z*ysize]) {
	    fseek(fp, (long) starttab[y+z*ysize], 0);
	    cur = starttab[y+z*ysize];
	  }

	  if (lengthtab[y+z*ysize]>rlebuflen) {
	    free(starttab); free(lengthtab); free(rledat); free(base);
	    loaderr = "rlebuf too small (corrupt image file?)";
	    return (byte *) NULL;
	  }

	  fread(rledat, (size_t) lengthtab[y+z*ysize], (size_t) 1, fp);
	  cur += lengthtab[y+z*ysize];
	  expandrow(lptr,rledat,3-z);
	  lptr += (xsize * 4);
	}
      }
    }
    else {
      lptr = base;
      for (y=0; y<ysize; y++) {
	for (z=0; z<zsize; z++) {
	  if (cur != starttab[y+z*ysize]) {
	    fseek(fp, (long) starttab[y+z*ysize], 0);
	    cur = starttab[y+z*ysize];
	  }

	  fread(rledat, (size_t) lengthtab[y+z*ysize], (size_t) 1, fp);
	  cur += lengthtab[y+z*ysize];
	  expandrow(lptr,rledat,3-z);
	}
	lptr += (xsize * 4);
      }
    }

    free(starttab);
    free(lengthtab);
    free(rledat);
    return base;
  }      /* end of RLE case */

  else {  /* not RLE */
    verdat = (byte *) malloc((size_t) xsize);
    base   = (byte *) malloc((size_t) (xsize*ysize+TAGLEN) * 4);
    if (!base || !verdat) FatalError("out of memory in LoadIRIS()");

    addimgtag(base,xsize,ysize);
    
    fseek(fp,512L,0);

    for (z=0; z<zsize; z++) {
      lptr = base;
      for (y=0; y<ysize; y++) {
	fread(verdat, (size_t) xsize, (size_t) 1,fp);
	interleaverow(lptr,verdat,3-z,xsize);
	lptr += (xsize * 4);
      }
    }

    free(verdat);
    return base;
  }
}


/******************************************/
static void interleaverow(lptr,cptr,z,n)
     byte *lptr, *cptr;
     int z, n;
{
  lptr += z;
  while(n--) {
    *lptr = *cptr++;
    lptr += 4;
  }
}


/******************************************/
static void expandrow(optr,iptr,z)
     byte *optr, *iptr;
     int z;
{
  byte pixel, count;

  optr += z;
  while (1) {
    pixel = *iptr++;
    if ( !(count = (pixel & 0x7f)) ) return;
    if (pixel & 0x80) {
      while (count>=8) {
	optr[0*4] = iptr[0];
	optr[1*4] = iptr[1];
	optr[2*4] = iptr[2];
	optr[3*4] = iptr[3];
	optr[4*4] = iptr[4];
	optr[5*4] = iptr[5];
	optr[6*4] = iptr[6];
	optr[7*4] = iptr[7];
	optr += 8*4;
	iptr += 8;
	count -= 8;
      }
      while(count--) {
	*optr = *iptr++;
	optr+=4;
      }
    }
    else {
      pixel = *iptr++;
      while(count>=8) {
	optr[0*4] = pixel;
	optr[1*4] = pixel;
	optr[2*4] = pixel;
	optr[3*4] = pixel;
	optr[4*4] = pixel;
	optr[5*4] = pixel;
	optr[6*4] = pixel;
	optr[7*4] = pixel;
	optr += 8*4;
	count -= 8;
      }
      while(count--) {
	*optr = pixel;
	optr+=4;
      }
    }
  }
}



/****************************************************/
static void readtab(fp, tab, n)
     FILE   *fp;
     u_long *tab;
     int     n;
{
  while (n) {
    *tab++ = getlong(fp);
    n--;
  }
}


/*****************************************************/
static void addimgtag(dptr,xsize,ysize)
     byte *dptr;
     int   xsize, ysize;
{
  /* this is used to extract image data from core dumps. 
     I doubt this is necessary...  --jhb */

  dptr    = dptr + (xsize * ysize * 4);
  dptr[0] = 0x12;  dptr[1] = 0x34;  dptr[2] = 0x56;  dptr[3] = 0x78;
  dptr += 4;

  dptr[0] = 0x59;  dptr[1] = 0x49;  dptr[2] = 0x33;  dptr[3] = 0x33;
  dptr += 4;

  dptr[0] = 0x69;  dptr[1] = 0x43;  dptr[2] = 0x42;  dptr[3] = 0x22;
  dptr += 4;

  dptr[0] = (xsize>>24)&0xff;
  dptr[1] = (xsize>>16)&0xff;
  dptr[2] = (xsize>> 8)&0xff;
  dptr[3] = (xsize    )&0xff;
  dptr += 4;

  dptr[0] = (ysize>>24)&0xff;
  dptr[1] = (ysize>>16)&0xff;
  dptr[2] = (ysize>> 8)&0xff;
  dptr[3] = (ysize    )&0xff;
}




/*************************************************/
/* IRIS image-writing routines                   */
/*************************************************/




/*************************************************/
int WriteIRIS(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
     FILE *fp;
     byte *pic, *rmap, *gmap, *bmap;
     int   ptype, w, h, numcols, colorstyle;
{
  /* writes a greyscale or 24-bit RGB IRIS file to the already open
     stream, rle compressed */

  IMAGE img;
  int     i, j, pos, len, tablen, rlebuflen, zsize;
  u_long *starttab, *lengthtab;
  byte   *rlebuf, *pptr;
  byte   *lumbuf, *lptr, *longpic;

  xvbzero((char *) &img, sizeof(IMAGE));
  
  /* write header information */
  fwrite(&img, sizeof(IMAGE), (size_t) 1, fp);
  fseek(fp, 0L, 0);

  /* load up header */
  img.imagic = IMAGIC;
  img.type   = ITYPE_RLE | (1 & BPPMASK);   /* RLE, 1 byteperpix */
  img.dim    = (colorstyle == F_FULLCOLOR) ? 3 : 2;
  img.xsize  = w;
  img.ysize  = h;
  img.zsize  = zsize = (colorstyle == F_FULLCOLOR) ? 3 : 1;
  img.min    = 0;
  img.max    = 255;

  putshort(fp, img.imagic);
  putshort(fp, img.type);
  putshort(fp, img.dim);
  putshort(fp, img.xsize);
  putshort(fp, img.ysize);
  putshort(fp, img.zsize);
  putlong (fp, img.min);
  putlong (fp, img.max);
  putlong (fp, 0L);
  fwrite  ("no name", (size_t) 8, (size_t) 1,fp);

  if (ferror(fp)) { fclose(fp);  return -1; }

  /* allocate RLE compression tables & stuff */
  rlebuflen = 2*w + 10;
  tablen    = h * zsize;

  starttab  = (u_long *) malloc((size_t) tablen * sizeof(long));
  lengthtab = (u_long *) malloc((size_t) tablen * sizeof(long));
  rlebuf    = (byte *)   malloc((size_t) rlebuflen);
  lumbuf    = (byte *)   malloc((size_t) w * 4);

  if (!starttab || !lengthtab || !rlebuf || !lumbuf) 
    FatalError("out of memory in WriteIRIS()");

  pos = 512 + 2 * (tablen * 4);
  fseek(fp, (long) pos, 0);

  /* convert image into 4-byte per pix image that the compress routines want */
  longpic = (byte *) malloc((size_t) w * h * 4);
  if (!longpic) FatalError("couldn't malloc longpic in WriteIRIS()");

  for (i=0, pptr=pic; i<h; i++) {
    lptr = longpic + ((h-1) - i) * (w * 4);    /* vertical flip */
    if (ptype == PIC8) {    /* colormapped */
      for (j=0; j<w; j++, pptr++) {
	*lptr++ = 0xff;
	*lptr++ = bmap[*pptr];
	*lptr++ = gmap[*pptr];
	*lptr++ = rmap[*pptr];
      }
    }
    else {     /* PIC24 */
      for (j=0; j<w; j++, pptr+=3) {
	*lptr++ = 0xff;
	*lptr++ = pptr[2];
	*lptr++ = pptr[1];
	*lptr++ = pptr[0];
      }
    }
  }
      


  /* compress and write the data */
  lptr = longpic;
  for (i=0; i<h; i++) {
    for (j=0; j<zsize; j++) {
      if (zsize == 1) {
	lumrow(lptr, lumbuf, w);
	len = compressrow(lumbuf, rlebuf, CHANOFFSET(j), w);
      } 
      else {
	len = compressrow(lptr, rlebuf, CHANOFFSET(j), w);
      }

      if (len > rlebuflen) {
	FatalError("WriteIRIS: rlebuf is too small");
	exit(1);
      }

      fwrite(rlebuf, (size_t) len, (size_t) 1, fp);
      starttab [i + j*h] = pos;
      lengthtab[i + j*h] = len;
      pos += len;
    }
    lptr += (w*4);
  }


  /* write out line start and length tables */
  fseek   (fp, 512L, 0);
  writetab(fp, starttab, tablen);
  writetab(fp, lengthtab,tablen);

  free(starttab);
  free(lengthtab);
  free(rlebuf);
  free(lumbuf);
  free(longpic);

  if (ferror(fp)) return -1;

  return 0;
}
  

/*************************************/
static void lumrow(rgbptr, lumptr, n) 
     byte *rgbptr, *lumptr;
     int n;
{
  lumptr += CHANOFFSET(0);
  while (n--) {
    *lumptr = ILUM(rgbptr[OFFSET_R],rgbptr[OFFSET_G],rgbptr[OFFSET_B]);
    lumptr += 4;
    rgbptr += 4;
  }
}


/*************************************/
static int compressrow(lbuf, rlebuf, z, cnt)
     byte *lbuf, *rlebuf;
     int   z, cnt;
{
  byte *iptr, *ibufend, *sptr, *optr;
  short todo, cc;							
  long  count;

  lbuf    += z;
  iptr    = lbuf;
  ibufend = iptr+cnt*4;
  optr    = rlebuf;

  while (iptr<ibufend) {
    sptr = iptr;
    iptr += 8;
    while ((iptr<ibufend) && ((iptr[-8]!=iptr[-4])||(iptr[-4]!=iptr[0])))
      iptr += 4;
    iptr -= 8;

    count = (iptr-sptr)/4;

    while (count) {
      todo = count>126 ? 126 : count;
      count -= todo;
      *optr++ = 0x80|todo;
      while (todo>8) {
	optr[0] = sptr[0*4];
	optr[1] = sptr[1*4];
	optr[2] = sptr[2*4];
	optr[3] = sptr[3*4];
	optr[4] = sptr[4*4];
	optr[5] = sptr[5*4];
	optr[6] = sptr[6*4];
	optr[7] = sptr[7*4];
	optr += 8;
	sptr += 8*4;
	todo -= 8;
      }

      while (todo--) {
	*optr++ = *sptr;
	sptr += 4;
      }
    }

    sptr = iptr;
    cc = *iptr;
    iptr += 4;
    while ((iptr<ibufend) && (*iptr == cc))  iptr += 4;
    
    count = (iptr-sptr)/4;
    while (count) {
      todo = count>126 ? 126:count;
      count -= todo;
      *optr++ = todo;
      *optr++ = cc;
    }
  }
  
  *optr++ = 0;
  return (optr - rlebuf);
}


/****************************************************/
static void writetab(outf,tab,n)
     FILE *outf;
     u_long *tab;
     int n;
{
  while (n) {
    putlong(outf,*tab++);
    n--;
  }
}




/* byte order independent read/write of shorts and longs. */

static u_short getshort(inf)
     FILE *inf;
{
  byte buf[2];
  fread(buf, (size_t) 2, (size_t) 1,inf);
  return (buf[0]<<8)+(buf[1]<<0);
}


static u_long getlong(inf)
     FILE *inf;
{
  byte buf[4];
  fread(buf, (size_t) 4, (size_t) 1,inf);
  return (((u_long) buf[0])<<24) + (((u_long) buf[1])<<16)
       + (((u_long) buf[2])<<8) + buf[3];
}


static void putshort(outf,val)
     FILE *outf;
     int val;
{
  byte buf[2];
  buf[0] = (val>>8);
  buf[1] = (val>>0);
  fwrite(buf,(size_t) 2,(size_t) 1,outf);
}


static void putlong(outf,val)
     FILE *outf;
     u_long val;
{
  byte buf[4];
  buf[0] = (val>>24);
  buf[1] = (val>>16);
  buf[2] = (val>>8);
  buf[3] = (val>>0);
  fwrite(buf,(size_t) 4,(size_t) 1,outf);
}


