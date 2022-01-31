/*
 * xvpm.c - load routine for 'pm' format pictures
 *
 * LoadPM();
 * WritePM(fp, pic, ptype, w, h, r,g,b, numcols, style, comment)
 */

#include "copyright.h"

#include "xv.h"


/**** PM.H ******/
#define	PM_MAGICNO	0x56494557		/* Hex for VIEW */
#define	PM_A		0x8000
#define	PM_C		0x8001
#define	PM_S		0x8002
#define	PM_I		0x8004
#define PM_F		0xc004

#define PM_IOHDR_SIZE	(sizeof(pmpic)-(2*sizeof(char*)))

typedef struct {
	int	pm_id;		/* Magic number for pm format files.	*/
	int	pm_np;		/* Number of planes. Normally 1.	*/
	int	pm_nrow;	/* Number of rows. 1 - MAXNELM.		*/
	int	pm_ncol;	/* Number of columns. 1 - MAXNELM.	*/
	int	pm_nband;	/* Number of bands.			*/
	int	pm_form;	/* Pixel format.			*/
	int	pm_cmtsize;	/* Number comment bytes. Includes NULL. */
	char	*pm_image;	/* The image itself.			*/
	char	*pm_cmt;	/* Transforms performed.		*/
} pmpic;


#define pm_nelm(p)	((p)->pm_ncol * (p)->pm_nrow)
#define pm_nbelm(p)	(pm_nelm(p) * (p)->pm_nband)
#define pm_psize(p)	(pm_nbelm(p) * (((p)->pm_form)&0xff))
#define pm_isize(p)	((p)->pm_np * pm_psize(p))
#define pm_npix(p)      (pm_nbelm(p) * (p)->pm_np)

/***** end PM.H *****/


static pmpic thePic;

static int  pmError  PARM((const char *, const char *));
static int  flip4    PARM((int));
static int  getint32 PARM((FILE *));
static void putint32 PARM((int, FILE *));


/*******************************************/
int LoadPM(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  FILE  *fp;
  byte  *pic8;
  int    isize,i,flipit,w,h,npixels,nRGBbytes;
  const char  *bname;

  bname = BaseName(fname);
  thePic.pm_image = (char *) NULL;
  thePic.pm_cmt   = (char *) NULL;

  pinfo->pic = (byte *) NULL;
  pinfo->comment = (char *) NULL;


  fp = xv_fopen(fname,"r");
  if (!fp) return( pmError(bname, "unable to open file") );

  /* read in the pmpic struct, one byte at a time */
  thePic.pm_id      = getint32(fp);
  thePic.pm_np      = getint32(fp);
  thePic.pm_nrow    = getint32(fp);
  thePic.pm_ncol    = getint32(fp);
  thePic.pm_nband   = getint32(fp);
  thePic.pm_form    = getint32(fp);
  thePic.pm_cmtsize = getint32(fp);

  if (ferror(fp) || feof(fp)) return(pmError(bname, "error reading header"));

  flipit = 0;

  if (thePic.pm_id != PM_MAGICNO) {
    thePic.pm_id = flip4(thePic.pm_id);
    if (thePic.pm_id == PM_MAGICNO) flipit = 1;
    else thePic.pm_id = flip4(thePic.pm_id);
  }
  if (thePic.pm_id != PM_MAGICNO) return( pmError(bname, "not a PM file") );

  if (flipit) {
    thePic.pm_np      = flip4(thePic.pm_np);
    thePic.pm_nrow    = flip4(thePic.pm_nrow);
    thePic.pm_ncol    = flip4(thePic.pm_ncol);
    thePic.pm_nband   = flip4(thePic.pm_nband);
    thePic.pm_form    = flip4(thePic.pm_form);
    thePic.pm_cmtsize = flip4(thePic.pm_cmtsize);
    }

  w = thePic.pm_ncol;
  h = thePic.pm_nrow;

  /* make sure that the input picture can be dealt with */
  if ( thePic.pm_nband!=1 ||
      (thePic.pm_form!=PM_I && thePic.pm_form!=PM_C) ||
      (thePic.pm_form==PM_I && thePic.pm_np>1) ||
      (thePic.pm_form==PM_C && (thePic.pm_np==2 || thePic.pm_np>4)) ) {
    fprintf(stderr,"PM picture not in a displayable format.\n");
    fprintf(stderr,"(ie, 1-plane PM_I, or 1-, 3-, or 4-plane PM_C)\n");

    return pmError(bname, "PM file in unsupported format");
  }


  isize = pm_isize(&thePic);
  npixels = w*h;
  nRGBbytes = 3*npixels;

  /* make sure image is more-or-less valid (and no overflows) */
  if (isize <= 0 || w <= 0 || h <= 0 || npixels/w < h ||
      nRGBbytes/3 < npixels || thePic.pm_cmtsize < 0)
    return pmError(bname, "Bogus PM file!!");

  if (DEBUG)
    fprintf(stderr,"%s: LoadPM() - loading a %dx%d %s pic, %d planes\n",
	    cmd, w, h, (thePic.pm_form==PM_I) ? "PM_I" : "PM_C",
	    thePic.pm_np);


  /* allocate memory for picture and read it in */
  thePic.pm_image = (char *) malloc((size_t) isize);
  if (thePic.pm_image == NULL)
    return( pmError(bname, "unable to malloc PM picture") );

  if (fread(thePic.pm_image, (size_t) isize, (size_t) 1, fp) != 1)   {
    free(thePic.pm_image);
    return( pmError(bname, "file read error") );
  }


  /* alloc and read in comment, if any */
  if (thePic.pm_cmtsize>0) {
    thePic.pm_cmt = (char *) malloc((size_t) thePic.pm_cmtsize+1);
    if (thePic.pm_cmt) {
      thePic.pm_cmt[thePic.pm_cmtsize] = '\0';  /* to be safe */
      if (fread(thePic.pm_cmt,(size_t) thePic.pm_cmtsize,(size_t) 1,fp) != 1) {
	free(thePic.pm_cmt);
	thePic.pm_cmt = (char *) NULL;
      }
    }
  }

  fclose(fp);


  if (thePic.pm_form == PM_I) {
    int  *intptr;
    byte *pic24, *picptr;

    if ((pic24 = (byte *) malloc((size_t) nRGBbytes))==NULL) {
      if (thePic.pm_cmt) free(thePic.pm_cmt);
      return( pmError(bname, "unable to malloc 24-bit picture") );
    }

    intptr = (int *) thePic.pm_image;
    picptr = pic24;

    if (flipit) {    /* if flipit, integer is RRGGBBAA instead of AABBGGRR */
      for (i=w*h; i>0; i--, intptr++) {
	if ((i & 0x3fff) == 0) WaitCursor();
	*picptr++ = (*intptr>>24) & 0xff;
	*picptr++ = (*intptr>>16) & 0xff;
	*picptr++ = (*intptr>>8)  & 0xff;
      }
    }
    else {
      for (i=w*h; i>0; i--, intptr++) {
	if ((i & 0x3fff) == 0) WaitCursor();
	*picptr++ = (*intptr)     & 0xff;
	*picptr++ = (*intptr>>8)  & 0xff;
	*picptr++ = (*intptr>>16) & 0xff;
      }
    }

    free(thePic.pm_image);

    pinfo->pic  = pic24;
    pinfo->type = PIC24;
  }


  else if (thePic.pm_form == PM_C && thePic.pm_np>1) {
    byte *pic24, *picptr, *rptr, *gptr, *bptr;

    if ((pic24 = (byte *) malloc((size_t) nRGBbytes))==NULL) {
      if (thePic.pm_cmt) free(thePic.pm_cmt);
      return( pmError(bname, "unable to malloc 24-bit picture") );
    }

    rptr = (byte *) thePic.pm_image;
    gptr = rptr + w*h;
    bptr = rptr + w*h*2;
    picptr = pic24;
    for (i=w*h; i>0; i--) {
      if ((i & 0x3fff) == 0) WaitCursor();
      *picptr++ = *rptr++;
      *picptr++ = *gptr++;
      *picptr++ = *bptr++;
    }
    free(thePic.pm_image);

    pinfo->pic  = pic24;
    pinfo->type = PIC24;
  }


  else if (thePic.pm_form == PM_C && thePic.pm_np==1) {
    /* don't have to convert, just point pic at thePic.pm_image */
    pic8 = (byte *) thePic.pm_image;
    for (i=0; i<256; i++)
      pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;  /* build mono cmap */

    pinfo->pic  = pic8;
    pinfo->type = PIC8;
  }


  /* load up remaining pinfo fields */
  pinfo->w = thePic.pm_ncol;  pinfo->h = thePic.pm_nrow;
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;

  pinfo->frmType = F_PM;
  pinfo->colType = (thePic.pm_form==PM_I || thePic.pm_np>1)
                         ? F_FULLCOLOR : F_GREYSCALE;
  sprintf(pinfo->fullInfo,"PM, %s.  (%d plane %s)  (%d bytes)",
	  (thePic.pm_form==PM_I || thePic.pm_np>1)
	        ? "24-bit color" : "8-bit greyscale",
	  thePic.pm_np, (thePic.pm_form==PM_I) ? "PM_I" : "PM_C",
	  isize + (int)PM_IOHDR_SIZE + thePic.pm_cmtsize);

  sprintf(pinfo->shrtInfo, "%dx%d PM.", w,h);
  pinfo->comment = thePic.pm_cmt;

  return 1;
}


/*******************************************/
int WritePM(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle,
	    comment)
     FILE *fp;
     byte *pic;
     int   ptype,w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
     char *comment;
{
  /* writes a PM file to the already open stream
     'colorstyle' single-handedly determines the type of PM pic written
     if colorstyle==0, (Full Color) a 3-plane PM_C pic is written
     if colorstyle==1, (Greyscal) a 1-plane PM_C pic is written
     if colorstyle==0, (B/W stipple) a 1-plane PM_C pic is written */

  char  foo[256];
  int   i;
  byte *p;

  /* create 'comment' field */
  sprintf(foo,"CREATOR: XV %s\n", REVDATE);

  /* fill in fields of a pmheader */
  thePic.pm_id = PM_MAGICNO;
  thePic.pm_np = (colorstyle==0) ? 3 : 1;
  thePic.pm_ncol = w;
  thePic.pm_nrow = h;
  thePic.pm_nband = 1;
  thePic.pm_form  = PM_C;
  thePic.pm_cmtsize = (strlen(foo) + 1);   /* +1 to write trailing '\0' */
  if (comment) thePic.pm_cmtsize += (strlen(comment) + 1);

  putint32(thePic.pm_id, fp);
  putint32(thePic.pm_np, fp);
  putint32(thePic.pm_nrow, fp);
  putint32(thePic.pm_ncol, fp);
  putint32(thePic.pm_nband, fp);
  putint32(thePic.pm_form, fp);
  putint32(thePic.pm_cmtsize, fp);

  /* write the picture data */
  if (colorstyle == 0) {         /* 24bit RGB, organized as 3 8bit planes */

    if (ptype == PIC8) {
      WaitCursor();
      for (i=0,p=pic; i<w*h; i++, p++) putc(rmap[*p], fp);

      WaitCursor();
      for (i=0,p=pic; i<w*h; i++, p++) putc(gmap[*p], fp);

      WaitCursor();
      for (i=0,p=pic; i<w*h; i++, p++) putc(bmap[*p], fp);
    }

    else {  /* PIC24 */
      WaitCursor();
      for (i=0,p=pic; i<w*h; i++, p+=3) putc(*p, fp);

      WaitCursor();
      for (i=0,p=pic+1; i<w*h; i++, p+=3) putc(*p, fp);

      WaitCursor();
      for (i=0,p=pic+2; i<w*h; i++, p+=3) putc(*p, fp);
    }
  }


  else if (colorstyle == 1) {    /* GreyScale: 8 bits per pixel */
    byte rgb[256];

    if (ptype == PIC8) {
      for (i=0; i<numcols; i++) rgb[i] = MONO(rmap[i],gmap[i],bmap[i]);
      for (i=0, p=pic; i<w*h; i++, p++) {
	if ((i & 0x3fff) == 0) WaitCursor();
	putc(rgb[*p],fp);
      }
    }
    else {  /* PIC24 */
      for (i=0, p=pic; i<w*h; i++, p+=3) {
	if ((i & 0x3fff) == 0) WaitCursor();
	putc( MONO(p[0],p[1],p[2]), fp);
      }
    }
  }

  else /* (colorstyle == 2) */ { /* B/W stipple.  pic is 1's and 0's */
    /* note: pic has already been dithered into 8-bit image */
    for (i=0, p=pic; i<w*h; i++, p++) {
      if ((i & 0x3fff) == 0) WaitCursor();
      putc(*p ? 255 : 0,fp);
    }
  }

  if (comment) {
    fwrite(comment, (size_t) strlen(comment), (size_t) 1, fp);
    fwrite("\n",    (size_t) 1,               (size_t) 1, fp);
  }
  fwrite(foo, strlen(foo) + 1, (size_t) 1, fp);  /* +1 for trailing '\0' */

  if (ferror(fp)) return -1;

  return 0;
}


/*****************************/
static int pmError(fname, st)
     const char *fname, *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  if (thePic.pm_image != NULL) free(thePic.pm_image);
  return 0;
}


/*****************************/
static int flip4(i)
     int i;
{
  /* flips low-order 4 bytes around in integer */

  byte b0, b1, b2, b3;
  int  rv;

  b0 = (((u_long) i)) & 0xff;
  b1 = (((u_long) i) >>  8) & 0xff;
  b2 = (((u_long) i) >> 16) & 0xff;
  b3 = (((u_long) i) >> 24) & 0xff;

  rv = (((u_long) b0) << 24) |
       (((u_long) b1) << 16) |
       (((u_long) b2) <<  8) |
       (((u_long) b3));

  return rv;
}



static int getint32(fp)
     FILE *fp;
{
  int i;

  i  = (getc(fp) & 0xff) << 24;
  i |= (getc(fp) & 0xff) << 16;
  i |= (getc(fp) & 0xff) << 8;
  i |= (getc(fp) & 0xff);

  return i;
}



static void putint32(i, fp)
     int   i;
     FILE *fp;
{
  putc( ((i>>24) & 0xff), fp);
  putc( ((i>>16) & 0xff), fp);
  putc( ((i>> 8) & 0xff), fp);
  putc( ((i) & 0xff), fp);
}



