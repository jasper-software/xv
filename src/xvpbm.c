/*
 * xvpbm.c - load routine for 'pm' format pictures
 *
 * LoadPBM(fname, pinfo)  -  loads a PBM, PGM, or PPM file
 * WritePBM(fp,pic,ptype,w,h,r,g,b,numcols,style,raw,cmt,comment)
 */

#include "copyright.h"

#include "xv.h"



/*
 * comments on error handling:
 * a truncated file is not considered a Major Error.  The file is loaded, the
 * rest of the pic is filled with 0's.
 *
 * a file with garbage characters in it is an unloadable file.  All allocated
 * stuff is tossed, and LoadPBM returns non-zero
 *
 * not being able to malloc is a Fatal Error.  The program is aborted.
 */


#define TRUNCSTR "File appears to be truncated."

static int garbage;
static long numgot, filesize;

static int loadpbm  PARM((FILE *, PICINFO *, int));
static int loadpgm  PARM((FILE *, PICINFO *, int, int));
static int loadppm  PARM((FILE *, PICINFO *, int, int));
static int getint   PARM((FILE *, PICINFO *));
static int getbit   PARM((FILE *, PICINFO *));
static int getshort PARM((FILE *));
static int pbmError PARM((char *, char *));

static char *bname;

/*******************************************/
int LoadPBM(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  FILE  *fp;
  int    c, c1;
  int    maxv, rv;

  garbage = maxv = rv = 0;
  bname = BaseName(fname);

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;


  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp) return (pbmError(bname, "can't open file"));

  /* compute file length */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);


  /* read the first two bytes of the file to determine which format
     this file is.  "P1" = ascii bitmap, "P2" = ascii greymap,
     "P3" = ascii pixmap, "P4" = raw bitmap, "P5" = raw greymap,
     "P6" = raw pixmap */

  c = getc(fp);  c1 = getc(fp);
  if (c!='P' || c1<'1' || c1>'6') return(pbmError(bname, "unknown format"));

  /* read in header information */
  pinfo->w = getint(fp, pinfo);  pinfo->h = getint(fp, pinfo);
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;

  /* if we're not reading a bitmap, read the 'max value' */
  if ( !(c1=='1' || c1=='4')) {
    maxv = getint(fp, pinfo);
    if (maxv < 1) garbage=1;    /* to avoid 'div by zero' probs */
  }


  if (garbage) {
    fclose(fp);
    if (pinfo->comment) free(pinfo->comment);
    pinfo->comment = (char *) NULL;
    return (pbmError(bname, "Garbage characters in header."));
  }


  if (c1=='1' || c1=='2' || c1=='3') pinfo->frmType = F_PBMASCII;
                                else pinfo->frmType = F_PBMRAW;

  /* note:  pic, type, r,g,b, frmInfo, shortFrm, and colorType fields of
     picinfo struct are filled in in the format-specific loaders */

  /* call the appropriate subroutine to handle format-specific stuff */
  if      (c1=='1' || c1=='4') rv = loadpbm(fp, pinfo, c1=='4' ? 1 : 0);
  else if (c1=='2' || c1=='5') rv = loadpgm(fp, pinfo, c1=='5' ? 1 : 0, maxv);
  else if (c1=='3' || c1=='6') rv = loadppm(fp, pinfo, c1=='6' ? 1 : 0, maxv);

  fclose(fp);

  if (!rv) {
    if (pinfo->pic) free(pinfo->pic);
    if (pinfo->comment) free(pinfo->comment);
    pinfo->pic     = (byte *) NULL;
    pinfo->comment = (char *) NULL;
  }

  return rv;
}  



/*******************************************/
static int loadpbm(fp, pinfo, raw)
     FILE    *fp;
     PICINFO *pinfo;
     int      raw;
{
  byte *pic8;
  byte *pix;
  int   i,j,bit,w,h;

  w = pinfo->w;  h = pinfo->h;
  pic8 = (byte *) calloc((size_t) w * h, (size_t) 1);
  if (!pic8) return pbmError(bname, "couldn't malloc 'pic8'");

  pinfo->pic  = pic8;
  pinfo->type = PIC8;
  sprintf(pinfo->fullInfo, "PBM, %s format.  (%ld bytes)", 
	  (raw) ? "raw" : "ascii", filesize);
  sprintf(pinfo->shrtInfo, "%dx%d PBM.", w, h);
  pinfo->colType = F_BWDITHER;


  /* B/W bitmaps have a two entry colormap */
  pinfo->r[0] = pinfo->g[0] = pinfo->b[0] = 255;   /* entry #0 = white */
  pinfo->r[1] = pinfo->g[1] = pinfo->b[1] = 0;     /* entry #1 = black */


  if (!raw) {
    numgot = 0;
    for (i=0, pix=pic8; i<h; i++) {
      if ((i&0x3f)==0) WaitCursor();
      for (j=0; j<w; j++, pix++) *pix = getbit(fp, pinfo);
    }

    if (numgot != w*h) pbmError(bname, TRUNCSTR);
    if (garbage) {
      return(pbmError(bname, "Garbage characters in image data."));
    }
  }


  else {   /* read raw bits */
    int trunc = 0, k = 0;

    for (i=0, pix=pic8; i<h; i++) {
      if ((i&15)==0) WaitCursor();
      for (j=0,bit=0; j<w; j++, pix++, bit++) {

	bit &= 7;
	if (!bit) {
	  k = getc(fp);
	  if (k==EOF) { trunc=1; k=0; }
	}

	*pix = (k&0x80) ? 1 : 0;
	k = k << 1;
      }
    }

    if (trunc) pbmError(bname, TRUNCSTR);
  }

  return 1;
}


/*******************************************/
static int loadpgm(fp, pinfo, raw, maxv)
     FILE    *fp;
     PICINFO *pinfo;
     int      raw, maxv;
{
  byte *pix, *pic8;
  int   i,j,bitshift,w,h, holdmaxv;


  w = pinfo->w;  h = pinfo->h;
  pic8 = (byte *) calloc((size_t) w*h, (size_t) 1);
  if (!pic8) return(pbmError(bname, "couldn't malloc 'pic8'"));


  pinfo->pic  = pic8;
  pinfo->type = PIC8;
  sprintf(pinfo->fullInfo, "PGM, %s format.  (%ld bytes)", 
	  (raw) ? "raw" : "ascii", filesize);
  sprintf(pinfo->shrtInfo, "%dx%d PGM.", pinfo->w, pinfo->h);
  pinfo->colType = F_GREYSCALE;


  /* if maxv>255, keep dropping bits until it's reasonable */
  holdmaxv = maxv;
  bitshift = 0;
  while (maxv>255) { maxv = maxv>>1;  bitshift++; }

  /* fill in a greyscale colormap where maxv maps to 255 */
  for (i=0; i<=maxv; i++)
    pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = (i*255)/maxv;


  numgot = 0;

  if (!raw) {
    for (i=0, pix=pic8; i<h; i++) {
      if ((i&0x3f)==0) WaitCursor();
      for (j=0; j<w; j++, pix++)
	*pix = (byte) (getint(fp, pinfo) >> bitshift);
    }
  }
  else { /* raw */
    if (holdmaxv>255) {
      for (i=0, pix=pic8; i<h; i++) {
	if ((i&0x3f)==0) WaitCursor();
	for (j=0; j<w; j++, pix++)
	  *pix = (byte) (getshort(fp) >> bitshift);
      }
    }
    else {
      numgot = fread(pic8, (size_t) 1, (size_t) w*h, fp);  /* read raw data */
    }
  }

  if (numgot != w*h) pbmError(bname, TRUNCSTR);   /* warning only */

  if (garbage) {
    return (pbmError(bname, "Garbage characters in image data."));
  }

  return 1;
}


/*******************************************/
static int loadppm(fp, pinfo, raw, maxv)
     FILE    *fp;
     PICINFO *pinfo;
     int      raw, maxv;
{
  byte *pix, *pic24, scale[256], *pic8;
  int   i,j,bitshift, w, h, holdmaxv;

  w = pinfo->w;  h = pinfo->h;

  /* allocate 24-bit image */
  pic24 = (byte *) calloc((size_t) w*h*3, (size_t) 1);
  if (!pic24) FatalError("couldn't malloc 'pic24'");

  pinfo->pic  = pic24;
  pinfo->type = PIC24;
  sprintf(pinfo->fullInfo, "PPM, %s format.  (%ld bytes)", 
	  (raw) ? "raw" : "ascii", filesize);
  sprintf(pinfo->shrtInfo, "%dx%d PPM.", w, h);
  pinfo->colType = F_FULLCOLOR;


  /* if maxv>255, keep dropping bits until it's reasonable */
  holdmaxv = maxv;
  bitshift = 0;
  while (maxv>255) { maxv = maxv>>1;  bitshift++; }


  numgot = 0;

  if (!raw) {
    for (i=0, pix=pic24; i<h; i++) {
      if ((i&0x3f)==0) WaitCursor();
      for (j=0; j<w*3; j++, pix++)
	*pix = (byte) (getint(fp, pinfo) >> bitshift);
    }
  }
  else { /* raw */
    if (holdmaxv>255) {
      for (i=0, pix=pic24; i<h; i++) {
	if ((i&0x3f)==0) WaitCursor();
	for (j=0; j<w*3; j++,pix++)
	  *pix = (byte) (getshort(fp) >> bitshift);
      }
    }
    else {
      numgot = fread(pic24, (size_t) 1, (size_t) w*h*3, fp);  /* read data */
    }
  }
  
  if (numgot != w*h*3) pbmError(bname, TRUNCSTR);

  if (garbage)
    return(pbmError(bname, "Garbage characters in image data."));


  /* have to scale all RGB values up (Conv24to8 expects RGB values to
     range from 0-255 */

  if (maxv<255) { 
    for (i=0; i<=maxv; i++) scale[i] = (i * 255) / maxv;

    for (i=0, pix=pic24; i<h; i++) {
      if ((i&0x3f)==0) WaitCursor();
      for (j=0; j<w*3; j++, pix++) *pix = scale[*pix];
    }
  }

  return 1;
}



/*******************************************/
static int getint(fp, pinfo)
     FILE *fp;
     PICINFO *pinfo;
{
  int c, i, firstchar;

  /* note:  if it sees a '#' character, all characters from there to end of
     line are appended to the comment string */

  /* skip forward to start of next number */
  c = getc(fp);
  while (1) {
    /* eat comments */
    if (c=='#') {   /* if we're at a comment, read to end of line */
      char cmt[256], *sp, *tmpptr;

      sp = cmt;  firstchar = 1;
      while (1) {
	c=getc(fp);
	if (firstchar && c == ' ') firstchar = 0;  /* lop off 1 sp after # */
	else {
	  if (c == '\n' || c == EOF) break;
	  if ((sp-cmt)<250) *sp++ = c;
	}
      }
      *sp++ = '\n';
      *sp   = '\0';

      if (strlen(cmt) > (size_t) 0) {    /* add to pinfo->comment */
	if (!pinfo->comment) {
	  pinfo->comment = (char *) malloc(strlen(cmt)+1);
	  if (!pinfo->comment) FatalError("malloc failure in xvpbm.c getint");
	  pinfo->comment[0] = '\0';
	}
	else {
	  tmpptr = (char *) realloc(pinfo->comment, 
		      strlen(pinfo->comment) + strlen(cmt) + 1); 
	  if (!tmpptr) FatalError("realloc failure in xvpbm.c getint");
	  pinfo->comment = tmpptr;
	}
	strcat(pinfo->comment, cmt);
      }
    }

    if (c==EOF) return 0;
    if (c>='0' && c<='9') break;   /* we've found what we were looking for */

    /* see if we are getting garbage (non-whitespace) */
    if (c!=' ' && c!='\t' && c!='\r' && c!='\n' && c!=',') garbage=1;

    c = getc(fp);
  }


  /* we're at the start of a number, continue until we hit a non-number */
  i = 0;
  while (1) {
    i = (i*10) + (c - '0');
    c = getc(fp);
    if (c==EOF) return i;
    if (c<'0' || c>'9') break;
  }

  numgot++;
  return i;
}



/*******************************************/
static int getshort(fp)
     FILE    *fp;
{
  /* used in RAW mode to read 16-bit values */

  int c1, c2;

  c1 = getc(fp);
  if (c1 == EOF) return 0;
  c2 = getc(fp);
  if (c2 == EOF) return 0;

  numgot++;

  return (c2 << 8) | c1;
}



/*******************************************/
static int getbit(fp, pinfo)
     FILE *fp;
     PICINFO *pinfo;
{
  int c;

  /* skip forward to start of next number */
  c = getc(fp);
  while (1) {
    /* eat comments */
    if (c=='#') {   /* if we're at a comment, read to end of line */
      char cmt[256], *sp, *tmpptr;

      sp = cmt;
      while (1) {
	c=getc(fp);
	if (c == '\n' || c == EOF) break;

	if ((sp-cmt)<250) *sp++ = c;
      }
      *sp++ = '\n';
      *sp = '\0';

      if (strlen(cmt) > (size_t) 0) {    /* add to pinfo->comment */
	if (!pinfo->comment) {
	  pinfo->comment = (char *) malloc(strlen(cmt)+1);
	  if (!pinfo->comment) FatalError("malloc failure in xvpbm.c getint");
	  pinfo->comment[0] = '\0';
	}
	else {
	  tmpptr = (char *) realloc(pinfo->comment, 
		      strlen(pinfo->comment) + strlen(cmt) + 1); 
	  if (!tmpptr) FatalError("realloc failure in xvpbm.c getint");
	  pinfo->comment = tmpptr;
	}
	strcat(pinfo->comment, cmt);
      }
    }
    if (c==EOF) return 0;
    if (c=='0' || c=='1') break;   /* we've found what we were looking for */

    /* see if we are getting garbage (non-whitespace) */
    if (c!=' ' && c!='\t' && c!='\r' && c!='\n' && c!=',') garbage=1;

    c = getc(fp);
  }


  numgot++;
  return(c-'0');
}


/*******************************************/
static int pbmError(fname, st)
     char *fname, *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  return 0;
}





/*******************************************/
int WritePBM(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,raw,comment)
     FILE *fp;
     byte *pic;
     int   ptype, w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle, raw;
     char *comment;
{
  /* writes a PBM/PGM/PPM file to the already open stream
     if (raw), writes as RAW bytes, otherwise writes as ASCII 
     'colorstyle' single-handedly determines the type of file written
     if colorstyle==0, (Full Color) a PPM file is written
     if colorstyle==1, (Greyscale)  a PGM file is written
     if colorstyle==2, (B/W stipple) a PBM file is written */

  int   magic;
  byte *pix;
  int   i,j,len;

  /* calc the appropriate magic number for this file type */
  magic = 0;
  if      (colorstyle==0) magic = 3;
  else if (colorstyle==1) magic = 2;
  else if (colorstyle==2) magic = 1;

  if (raw && magic) magic+=3;


  /* write the header info */
  fprintf(fp,"P%d\n",magic);
  fprintf(fp,"# CREATOR: XV %s\n", REVDATE);

  if (comment) {      /* write comment lines */
    char *sp;

    sp = comment;
    while (*sp) {
      fprintf(fp, "# ");
      while (*sp && *sp != '\n') fputc(*sp++, fp);
      if (*sp == '\n') sp++;
      fputc('\n', fp);
    }
  }


  fprintf(fp,"%d %d\n",w,h);
  if (colorstyle!=2) fprintf(fp,"255\n");

  if (ferror(fp)) return -1;

  /* write the image data */

  if (colorstyle==0) {                  /* 24bit RGB, 3 bytes per pixel */
    for (i=0, pix=pic, len=0; i<h; i++) {
      if ((i&63)==0) WaitCursor();
      for (j=0; j<w; j++) {
	if (raw) {
	  if (ptype==PIC8) {
	    putc(rmap[*pix],fp);  putc(gmap[*pix],fp);  putc(bmap[*pix],fp);
	  }
	  else {  /* PIC24 */
	    putc(pix[0],fp);  putc(pix[1],fp);  putc(pix[2],fp);
	  }
	}
	else {
	  if (ptype==PIC8) 
	    fprintf(fp,"%3d %3d %3d ",rmap[*pix], gmap[*pix], bmap[*pix]);
	  else
	    fprintf(fp,"%3d %3d %3d ",pix[0], pix[1], pix[2]);

	  len+=12;
	  if (len>58) { fprintf(fp,"\n");  len=0; }
	}
	
	pix += (ptype==PIC24) ? 3 : 1;
      }
    }
  }


  else if (colorstyle==1) {             /* 8-bit greyscale */
    byte rgb[256];
    if (ptype==PIC8)
      for (i=0; i<numcols; i++) rgb[i] = MONO(rmap[i],gmap[i],bmap[i]);

    for (i=0, pix=pic, len=0; i<w*h; i++) {
      if ((i&0x7fff)==0) WaitCursor();

      if (raw) putc((ptype==PIC8) ? rgb[*pix] : MONO(pix[0],pix[1],pix[2]),fp);

      else {
	if (ptype==PIC8) fprintf(fp,"%3d ",rgb[*pix]);
	            else fprintf(fp,"%3d ",MONO(pix[0],pix[1],pix[2]));
	len += 4;
	if (len>66) { fprintf(fp,"\n");  len=0; }
      }

      pix += (ptype==PIC24) ? 3 : 1;
    }
  }

  else if (colorstyle==2) {             /* 1-bit B/W stipple */
    int bit,k,flipbw;
    char *str0, *str1;

    /* shouldn't happen */
    if (ptype == PIC24) FatalError("PIC24 and B/W Stipple in WritePBM()\n");

    /* if '0' is black, set flipbw */
    flipbw = (MONO(rmap[0],gmap[0],bmap[0]) < MONO(rmap[1],gmap[1],bmap[1]));

    str0 = (flipbw) ? "1 " : "0 ";
    str1 = (flipbw) ? "0 " : "1 ";

    for (i=0, pix=pic, len=0; i<h; i++) {
      if ((i&15)==0) WaitCursor();
      for (j=0, bit=0, k=0; j<w; j++, pix++) {
	if (raw) {
	  k = (k << 1) | *pix;
	  bit++;
	  if (bit==8) {
	    if (flipbw) k = ~k;
	    fputc(k,fp);
	    bit = k = 0;
	  }
	}
	else {
	  if (*pix) fprintf(fp,str1);
	       else fprintf(fp,str0);
	  len+=2;
	  if (len>68) { fprintf(fp,"\n"); len=0; }
	}
      } /* j */
      if (raw && bit) {
	k = k << (8-bit);
	if (flipbw) k = ~k;
	fputc(k,fp);
      }
    }
  }

  if (ferror(fp)) return -1;

  return 0;
}


	  
	  



