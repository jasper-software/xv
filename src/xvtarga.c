/*
 * xvtarga.c - load routine for 'targa' format pictures
 *
 * written and submitted by:
 *     Derek Dongray    (dongray@genrad.com)
 *
 * The format read/written is actually Targa type 2 uncompressed as
 * produced by POVray 1.0
 *
 * LoadTarga(fname, pinfo)
 * WriteTarga(fp, pic, ptype, w,h, rmap,gmap,bmap,numcols, cstyle)
 */


/*
 * Targa Format (near as I can tell)
 *   0:
 *   1: colormap type
 *   2: image type  (1=colmap RGB, 2=uncomp RGB, 3=uncomp gray)
 *   3:
 *   4:
 *   5: colormap_length, low byte
 *   6: colormap_length, high byte
 *   7: bits per cmap entry     (8, 24, 32)
 *
 *  12: width, low byte
 *  13: width, high byte
 *  14: height, low byte
 *  15: height, high byte
 *  16: bits per pixel (8, 24)
 *  17: flags
 */



#include "xv.h"

static long filesize;
static const char *bname;


/*******************************************/
int LoadTarga(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  FILE  *fp;
  int    i, row, c, c1, w, h, npixels, bufsize, flags, intlace, topleft, trunc;
  byte *pic24, *pp;

  bname = BaseName(fname);

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  fp=xv_fopen(fname,"r");
  if (!fp) {
     SetISTR(ISTR_WARNING,"%s:  %s", bname, "can't open file");
     return 0;
   }

  /* compute file length */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);

  if (filesize < 18) {
     fclose(fp);
     SetISTR(ISTR_WARNING,"%s:  %s", bname, "file is too short");
     return 0;
   }

  /* Discard the first few bytes of the file.
     The format check has already been done or we wouldn't be here. */

  for (i=0; i<12; i++) {
    c=getc(fp);
  }


  /* read in header information */
  c=getc(fp); c1=getc(fp);
  w = c1*256 + c;

  c=getc(fp); c1=getc(fp);
  h = c1*256 + c;

  npixels = w * h;
  bufsize = 3 * npixels;
  if (w <= 0 || h <= 0 || npixels/w != h || bufsize/3 != npixels) {
    fclose(fp);
    SetISTR(ISTR_WARNING,"%s:  error in Targa header (bad image size)", bname);
    return 0;
  }

  c=getc(fp);
  if (c!=24)  {
    fclose(fp);
    SetISTR(ISTR_WARNING,"%s:  unsupported type (not 24-bit)", bname);
    return 0;
  }

  flags   = getc(fp);
  topleft = (flags & 0x20) >> 5;
  intlace = (flags & 0xc0) >> 6;

#ifdef FOO
  if (c!=0x20)  {
    fclose(fp);
    SetISTR(ISTR_WARNING,"%s:  unsupported type (not top-down, noninterlaced)",
	    bname);
    return 0;
  }
#endif


  pic24 = (byte *) calloc((size_t) bufsize, (size_t) 1);
  if (!pic24) FatalError("couldn't malloc 'pic24'");


  trunc = 0;

  /* read the data */
  for (i=0; i<h; i++) {
    if (intlace == 2) {        /* four pass interlace */
      if      (i < (1*h) / 4) row = 4 * i;
      else if (i < (2*h) / 4) row = 4 * (i - ((1*h)/4)) + 1;
      else if (i < (3*h) / 4) row = 4 * (i - ((2*h)/4)) + 2;
      else                    row = 4 * (i - ((3*h)/4)) + 3;
    }

    else if (intlace == 1) {   /* two pass interlace */
      if      (i < h / 2) row = 2 * i;
      else                row = 2 * (i - h/2) + 1;
    }

    else row = i;              /* no interlace */



    if (!topleft) row = (h - row - 1);     /* bottom-left origin: invert y */


    c = fread(pic24 + (row*w*3), (size_t) 1, (size_t) w*3, fp);
    if (c != w*3) trunc=1;
  }

  if (trunc) SetISTR(ISTR_WARNING,"%s:  File appears to be truncated.",bname);


  /* swap R,B values (file is in BGR, pic24 should be in RGB) */
  for (i=0, pp=pic24; i<npixels; i++, pp+=3) {
    c = pp[0];  pp[0] = pp[2];  pp[2] = c;
  }


  pinfo->pic     = pic24;
  pinfo->type    = PIC24;
  pinfo->w       = w;
  pinfo->h       = h;
  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
  pinfo->frmType = F_TARGA;
  sprintf(pinfo->fullInfo,"Targa, uncompressed RGB.  (%ld bytes)", filesize);
  sprintf(pinfo->shrtInfo,"%dx%d Targa.", w,h);
  pinfo->colType = F_FULLCOLOR;

  fclose(fp);

  return 1;
}


/*******************************************/
int WriteTarga(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle)
     FILE *fp;
     byte *pic;
     int   ptype, w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
/*******************************************/
{
  int i, j;
  byte *xpic;

  /* write the header */
  for (i=0; i<12; i++) putc( (i==2) ? 2 : 0, fp);

  putc(w&0xff,     fp);
  putc((w>>8)&0xff,fp);
  putc(h&0xff,     fp);
  putc((h>>8)&0xff,fp);

  putc(24,fp);
  putc(0x20,fp);

  xpic = pic;

  for (i=0; i<h; i++) {
    if ((i&63)==0) WaitCursor();

    for (j=0; j<w; j++) {
      if (ptype==PIC8) {
	putc(bmap[*xpic],fp);  putc(gmap[*xpic],fp);  putc(rmap[*xpic],fp);
	xpic++;
      }
      else {  /* PIC24 */
	putc(xpic[2], fp);  /* b */
	putc(xpic[1], fp);  /* g */
	putc(xpic[0], fp);  /* r */
	xpic+=3;
      }
    }
  }

  if (ferror(fp)) return -1;

  return 0;
}
