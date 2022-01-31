/*
 * xvsmooth.c - smoothing/color dither routines for XV
 *
 *  Contains:
 *            byte *SmoothResize(src8, swide, shigh, dwide, dhigh,
 *                               rmap, gmap, bmap, rdmap, gdmap, bdmap, maplen)
 *            byte *Smooth24(pic824, is24, swide, shigh, dwide, dhigh,
 *                               rmap, gmap, bmap)
 *            byte *DoColorDither(pic24, pic8, w, h, rmap,gmap,bmap,
 *                                rdisp, gdisp, bdisp, maplen)
 *            byte *Do332ColorDither(pic24, pic8, w, h, rmap,gmap,bmap,
 *                                rdisp, gdisp, bdisp, maplen)
 */

#include "copyright.h"

#include "xv.h"

static int smoothX  PARM((byte *, byte *, int, int, int, int, int,
			  byte *, byte *, byte *));
static int smoothY  PARM((byte *, byte *, int, int, int, int, int,
			  byte *, byte *, byte *));
static int smoothXY PARM((byte *, byte *, int, int, int, int, int,
			  byte *, byte *, byte *));


/***************************************************/
byte *SmoothResize(srcpic8, swide, shigh, dwide, dhigh,
		   rmap, gmap, bmap, rdmap, gdmap, bdmap, maplen)
     byte *srcpic8, *rmap, *gmap, *bmap, *rdmap, *gdmap, *bdmap;
     int   swide, shigh, dwide, dhigh, maplen;
{
  /* generic interface to Smooth and ColorDither code.
     given an 8-bit-per, swide * shigh image with colormap rmap,gmap,bmap,
     will generate a new 8-bit-per, dwide * dhigh image, which is dithered
     using colors found in rdmap, gdmap, bdmap arrays */

  /* returns ptr to a dwide*dhigh array of bytes, or NULL on failure */

  byte *pic24, *pic8;

  pic24 = Smooth24(srcpic8, 0, swide, shigh, dwide, dhigh, rmap, gmap, bmap);

  if (pic24) {
    pic8 = DoColorDither(pic24, NULL, dwide, dhigh, rmap, gmap, bmap,
			 rdmap, gdmap, bdmap, maplen);
    free(pic24);
    return pic8;
  }

  return (byte *) NULL;
}



/***************************************************/
byte *Smooth24(pic824, is24, swide, shigh, dwide, dhigh, rmap, gmap, bmap)
     byte *pic824, *rmap, *gmap, *bmap;
     int   is24, swide, shigh, dwide, dhigh;
{
  /* does a SMOOTH resize from pic824 (which is either a swide*shigh, 8-bit
     pic, with colormap rmap,gmap,bmap OR a swide*shigh, 24-bit image, based
     on whether 'is24' is set) into a dwide * dhigh 24-bit image

     returns a dwide*dhigh 24bit image, or NULL on failure (malloc) */
  /* rmap,gmap,bmap should be 'desired' colors */

  byte *pic24, *pp;
  int  *cxtab, *pxtab;
  int   y1Off, cyOff;
  int   ex, ey, cx, cy, px, py, apx, apy, x1, y1;
  int   cA, cB, cC, cD;
  int   pA, pB, pC, pD;
  int   retval, bperpix;

  cA = cB = cC = cD = 0;
  pp = pic24 = (byte *) malloc((size_t) (dwide * dhigh * 3));
  if (!pic24) {
    fprintf(stderr,"unable to malloc pic24 in 'Smooth24()'\n");
    return pic24;
  }

  bperpix = (is24) ? 3 : 1;

  /* decide which smoothing routine to use based on type of expansion */
  if      (dwide <  swide && dhigh <  shigh)
    retval = smoothXY(pic24, pic824, is24, swide, shigh, dwide, dhigh,
		      rmap, gmap, bmap);

  else if (dwide <  swide && dhigh >= shigh)
    retval = smoothX (pic24, pic824, is24, swide, shigh, dwide, dhigh,
		      rmap, gmap, bmap);

  else if (dwide >= swide && dhigh <  shigh)
    retval = smoothY (pic24, pic824, is24, swide, shigh, dwide, dhigh,
		      rmap, gmap, bmap);

  else {
    /* dwide >= swide && dhigh >= shigh */

    /* cx,cy = original pixel in pic824.  px,py = relative position
       of pixel ex,ey inside of cx,cy as percentages +-50%, +-50%.
       0,0 = middle of pixel */

    /* we can save a lot of time by precomputing cxtab[] and pxtab[], both
       dwide arrays of ints that contain values for the equations:
         cx = (ex * swide) / dwide;
         px = ((ex * swide * 128) / dwide) - (cx * 128) - 64; */

    cxtab = (int *) malloc(dwide * sizeof(int));
    if (!cxtab) { free(pic24);  return NULL; }

    pxtab = (int *) malloc(dwide * sizeof(int));
    if (!pxtab) { free(pic24);  free(cxtab);  return NULL; }

    for (ex=0; ex<dwide; ex++) {
      cxtab[ex] = (ex * swide) / dwide;
      pxtab[ex] = (((ex * swide)* 128) / dwide)
	           - (cxtab[ex] * 128) - 64;
    }

    for (ey=0; ey<dhigh; ey++) {
      byte *pptr, rA, gA, bA, rB, gB, bB, rC, gC, bC, rD, gD, bD;

      ProgressMeter(0, (dhigh)-1, ey, "Smooth");

      cy = (ey * shigh) / dhigh;
      py = (((ey * shigh) * 128) / dhigh) - (cy * 128) - 64;
      if (py<0) { y1 = cy-1;  if (y1<0) y1=0; }
           else { y1 = cy+1;  if (y1>shigh-1) y1=shigh-1; }

      cyOff = cy * swide * bperpix;    /* current line */
      y1Off = y1 * swide * bperpix;    /* up or down one line, depending */

      if ((ey&15) == 0) WaitCursor();

      for (ex=0; ex<dwide; ex++) {
	rA = rB = rC = rD = gA = gB = gC = gD = bA = bB = bC = bD = 0;

	cx = cxtab[ex];
	px = pxtab[ex];

	if (px<0) { x1 = cx-1;  if (x1<0) x1=0; }
	     else { x1 = cx+1;  if (x1>swide-1) x1=swide-1; }

	if (is24) {
	  pptr = pic824 + y1Off + x1*bperpix;   /* corner pixel */
	  rA = *pptr++;  gA = *pptr++;  bA = *pptr++;

	  pptr = pic824 + y1Off + cx*bperpix;   /* up/down center pixel */
	  rB = *pptr++;  gB = *pptr++;  bB = *pptr++;

	  pptr = pic824 + cyOff + x1*bperpix;   /* left/right center pixel */
	  rC = *pptr++;  gC = *pptr++;  bC = *pptr++;

	  pptr = pic824 + cyOff + cx*bperpix;   /* center pixel */
	  rD = *pptr++;  gD = *pptr++;  bD = *pptr++;
	}
	else {  /* 8-bit picture */
	  cA = pic824[y1Off + x1];   /* corner pixel */
	  cB = pic824[y1Off + cx];   /* up/down center pixel */
	  cC = pic824[cyOff + x1];   /* left/right center pixel */
	  cD = pic824[cyOff + cx];   /* center pixel */
	}

	/* quick check */
	if (!is24 && cA == cB && cB == cC && cC == cD) {
	  /* set this pixel to the same color as in pic8 */
	  *pp++ = rmap[cD];  *pp++ = gmap[cD];  *pp++ = bmap[cD];
	}

	else {
	  /* compute weighting factors */
	  apx = abs(px);  apy = abs(py);
	  pA = (apx * apy) >> 7; /* div 128 */
	  pB = (apy * (128 - apx)) >> 7; /* div 128 */
	  pC = (apx * (128 - apy)) >> 7; /* div 128 */
	  pD = 128 - (pA + pB + pC);

	  if (is24) {
	    *pp++ = (((int) (pA * rA))>>7) + (((int) (pB * rB))>>7) +
	            (((int) (pC * rC))>>7) + (((int) (pD * rD))>>7);

	    *pp++ = (((int) (pA * gA))>>7) + (((int) (pB * gB))>>7) +
	            (((int) (pC * gC))>>7) + (((int) (pD * gD))>>7);

	    *pp++ = (((int) (pA * bA))>>7) + (((int) (pB * bB))>>7) +
	            (((int) (pC * bC))>>7) + (((int) (pD * bD))>>7);
	  }
	  else {  /* 8-bit pic */
	    *pp++ = (((int)(pA * rmap[cA]))>>7) + (((int)(pB * rmap[cB]))>>7) +
	            (((int)(pC * rmap[cC]))>>7) + (((int)(pD * rmap[cD]))>>7);

	    *pp++ = (((int)(pA * gmap[cA]))>>7) + (((int)(pB * gmap[cB]))>>7) +
	            (((int)(pC * gmap[cC]))>>7) + (((int)(pD * gmap[cD]))>>7);

	    *pp++ = (((int)(pA * bmap[cA]))>>7) + (((int)(pB * bmap[cB]))>>7) +
  	            (((int)(pC * bmap[cC]))>>7) + (((int)(pD * bmap[cD]))>>7);
	  }
	}
      }
    }

    free(cxtab);
    free(pxtab);
    retval = 0;    /* okay */
  }

  if (retval) {    /* one of the Smooth**() methods failed */
    free(pic24);
    pic24 = (byte *) NULL;
  }

  return pic24;
}




/***************************************************/
static int smoothX(pic24, pic824, is24, swide, shigh, dwide, dhigh,
		   rmap, gmap, bmap)
byte *pic24, *pic824, *rmap, *gmap, *bmap;
int   is24, swide, shigh, dwide, dhigh;
{
  byte *cptr, *cptr1;
  int  i, j;
  int  *lbufR, *lbufG, *lbufB;
  int  pixR, pixG, pixB, bperpix;
  int  pcnt0, pcnt1, lastpix, pixcnt, thisline, ypcnt;
  int  *pixarr, *paptr;

  /* returns '0' if okay, '1' if failed (malloc) */

  /* for case where pic8 is shrunk horizontally and stretched vertically
     maps pic8 into an dwide * dhigh 24-bit picture.  Only works correctly
     when swide>=dwide and shigh<=dhigh */


  /* malloc some arrays */
  lbufR  = (int *) calloc((size_t) swide,   sizeof(int));
  lbufG  = (int *) calloc((size_t) swide,   sizeof(int));
  lbufB  = (int *) calloc((size_t) swide,   sizeof(int));
  pixarr = (int *) calloc((size_t) swide+1, sizeof(int));

  if (!lbufR || !lbufG || !lbufB || !pixarr) {
    if (lbufR)  free(lbufR);
    if (lbufG)  free(lbufG);
    if (lbufB)  free(lbufB);
    if (pixarr) free(pixarr);
    return 1;
  }

  bperpix = (is24) ? 3 : 1;

  for (j=0; j<=swide; j++)
    pixarr[j] = (j*dwide + (15*swide)/16) / swide;

  cptr = pic824;  cptr1 = cptr + swide * bperpix;

  for (i=0; i<dhigh; i++) {
    ProgressMeter(0, (dhigh)-1, i, "Smooth");
    if ((i&15) == 0) WaitCursor();

    ypcnt = (((i*shigh)<<6) / dhigh) - 32;
    if (ypcnt<0) ypcnt = 0;

    pcnt1 = ypcnt & 0x3f;                     /* 64ths of NEXT line to use */
    pcnt0 = 64 - pcnt1;                       /* 64ths of THIS line to use */

    thisline = ypcnt>>6;

    cptr  = pic824 + thisline * swide * bperpix;
    if (thisline+1 < shigh) cptr1 = cptr + swide * bperpix;
    else cptr1 = cptr;

    if (is24) {
      for (j=0; j<swide; j++) {
	lbufR[j] = ((int) ((*cptr++ * pcnt0) + (*cptr1++ * pcnt1))) >> 6;
	lbufG[j] = ((int) ((*cptr++ * pcnt0) + (*cptr1++ * pcnt1))) >> 6;
	lbufB[j] = ((int) ((*cptr++ * pcnt0) + (*cptr1++ * pcnt1))) >> 6;
      }
    }
    else {  /* 8-bit input pic */
      for (j=0; j<swide; j++, cptr++, cptr1++) {
	lbufR[j] = ((int)((rmap[*cptr]* pcnt0) + (rmap[*cptr1]* pcnt1))) >> 6;
	lbufG[j] = ((int)((gmap[*cptr]* pcnt0) + (gmap[*cptr1]* pcnt1))) >> 6;
	lbufB[j] = ((int)((bmap[*cptr]* pcnt0) + (bmap[*cptr1]* pcnt1))) >> 6;
      }
    }

    pixR = pixG = pixB = pixcnt = lastpix = 0;

    for (j=0, paptr=pixarr; j<=swide; j++,paptr++) {
      if (*paptr != lastpix) {   /* write a pixel to pic24 */
	if (!pixcnt) pixcnt = 1;    /* this NEVER happens:  quiets compilers */
	*pic24++ = pixR / pixcnt;
	*pic24++ = pixG / pixcnt;
	*pic24++ = pixB / pixcnt;
	lastpix = *paptr;
	pixR = pixG = pixB = pixcnt = 0;
      }

      if (j<swide) {
	pixR += lbufR[j];
	pixG += lbufG[j];
	pixB += lbufB[j];
	pixcnt++;
      }
    }
  }

  free(lbufR);  free(lbufG);  free(lbufB);  free(pixarr);
  return 0;
}






/***************************************************/
static int smoothY(pic24, pic824, is24, swide, shigh, dwide, dhigh,
		   rmap, gmap, bmap)
byte *pic24, *pic824, *rmap, *gmap, *bmap;
int   is24, swide, shigh, dwide, dhigh;
{
  byte *clptr, *cptr, *cptr1;
  int  i, j, bperpix;
  int  *lbufR, *lbufG, *lbufB, *pct0, *pct1, *cxarr, *cxptr;
  int  lastline, thisline, linecnt;
  int  retval;


  /* returns '0' if okay, '1' if failed (malloc) */

  /* for case where pic8 is shrunk vertically and stretched horizontally
     maps pic8 into a dwide * dhigh 24-bit picture.  Only works correctly
     when swide<=dwide and shigh>=dhigh */

  retval = 0;   /* no probs, yet... */

  bperpix = (is24) ? 3 : 1;

  lbufR = lbufG = lbufB = pct0 = pct1 = cxarr = NULL;
  lbufR = (int *) calloc((size_t) dwide, sizeof(int));
  lbufG = (int *) calloc((size_t) dwide, sizeof(int));
  lbufB = (int *) calloc((size_t) dwide, sizeof(int));
  pct0  = (int *) calloc((size_t) dwide, sizeof(int));
  pct1  = (int *) calloc((size_t) dwide, sizeof(int));
  cxarr = (int *) calloc((size_t) dwide, sizeof(int));

  if (!lbufR || !lbufG || !lbufB || !pct0 || ! pct1 || !cxarr) {
    retval = 1;
    goto smyexit;
  }



  for (i=0; i<dwide; i++) {                /* precompute some handy tables */
    int cx64;
    cx64 = (((i * swide) << 6) / dwide) - 32;
    if (cx64<0) cx64 = 0;
    pct1[i] = cx64 & 0x3f;
    pct0[i] = 64 - pct1[i];
    cxarr[i] = cx64 >> 6;
  }


  lastline = linecnt = 0;

  for (i=0, clptr=pic824; i<=shigh; i++, clptr+=swide*bperpix) {
    ProgressMeter(0, shigh, i, "Smooth");
    if ((i&15) == 0) WaitCursor();

    thisline = (i * dhigh + (15*shigh)/16) / shigh;

    if (thisline != lastline) {  /* copy a line to pic24 */
      for (j=0; j<dwide; j++) {
	*pic24++ = lbufR[j] / linecnt;
	*pic24++ = lbufG[j] / linecnt;
	*pic24++ = lbufB[j] / linecnt;
      }

      xvbzero( (char *) lbufR, dwide * sizeof(int));  /* clear out line bufs */
      xvbzero( (char *) lbufG, dwide * sizeof(int));
      xvbzero( (char *) lbufB, dwide * sizeof(int));
      linecnt = 0;  lastline = thisline;
    }


    for (j=0, cxptr=cxarr; j<dwide; j++, cxptr++) {
      cptr  = clptr + *cxptr * bperpix;
      if (*cxptr < swide-1) cptr1 = cptr + 1*bperpix;
                       else cptr1 = cptr;

      if (is24) {
	lbufR[j] += ((int)((*cptr++ * pct0[j]) + (*cptr1++ * pct1[j]))) >> 6;
	lbufG[j] += ((int)((*cptr++ * pct0[j]) + (*cptr1++ * pct1[j]))) >> 6;
	lbufB[j] += ((int)((*cptr++ * pct0[j]) + (*cptr1++ * pct1[j]))) >> 6;
      }
      else {  /* 8-bit input pic */
	lbufR[j] += ((int)((rmap[*cptr]*pct0[j])+(rmap[*cptr1]*pct1[j]))) >> 6;
	lbufG[j] += ((int)((gmap[*cptr]*pct0[j])+(gmap[*cptr1]*pct1[j]))) >> 6;
	lbufB[j] += ((int)((bmap[*cptr]*pct0[j])+(bmap[*cptr1]*pct1[j]))) >> 6;
      }
    }

    linecnt++;
  }


 smyexit:
  if (lbufR) free(lbufR);
  if (lbufG) free(lbufG);
  if (lbufB) free(lbufB);
  if (pct0)  free(pct0);
  if (pct1)  free(pct1);
  if (cxarr) free(cxarr);

  return retval;
}






/***************************************************/
static int smoothXY(pic24, pic824, is24, swide, shigh, dwide, dhigh,
		    rmap, gmap, bmap)
byte *pic24, *pic824, *rmap, *gmap, *bmap;
int   is24, swide, shigh, dwide, dhigh;
{
  byte *cptr;
  int  i,j;
  int  *lbufR, *lbufG, *lbufB;
  int  pixR, pixG, pixB, bperpix;
  int  lastline, thisline, lastpix, linecnt, pixcnt;
  int  *pixarr, *paptr;


  /* returns '0' if okay, '1' if failed (malloc) */

  /* shrinks pic8 into a dwide * dhigh 24-bit picture.  Only works correctly
     when swide>=dwide and shigh>=dhigh (ie, the picture is shrunk on both
     axes) */


  /* malloc some arrays */
  lbufR  = (int *) calloc((size_t) swide,   sizeof(int));
  lbufG  = (int *) calloc((size_t) swide,   sizeof(int));
  lbufB  = (int *) calloc((size_t) swide,   sizeof(int));
  pixarr = (int *) calloc((size_t) swide+1, sizeof(int));
  if (!lbufR || !lbufG || !lbufB || !pixarr) {
    if (lbufR)  free(lbufR);
    if (lbufG)  free(lbufG);
    if (lbufB)  free(lbufB);
    if (pixarr) free(pixarr);
    return 1;
  }

  bperpix = (is24) ? 3 : 1;

  for (j=0; j<=swide; j++)
    pixarr[j] = (j*dwide + (15*swide)/16) / swide;

  lastline = linecnt = pixR = pixG = pixB = 0;
  cptr = pic824;

  for (i=0; i<=shigh; i++) {
    ProgressMeter(0, shigh, i, "Smooth");
    if ((i&15) == 0) WaitCursor();

    thisline = (i * dhigh + (15*shigh)/16 ) / shigh;

    if ((thisline != lastline)) {      /* copy a line to pic24 */
      pixR = pixG = pixB = pixcnt = lastpix = 0;

      for (j=0, paptr=pixarr; j<=swide; j++,paptr++) {
	if (*paptr != lastpix) {                 /* write a pixel to pic24 */
	  if (!pixcnt) pixcnt = 1;    /* NEVER happens: quiets compilers */
	  *pic24++ = (pixR/linecnt) / pixcnt;
	  *pic24++ = (pixG/linecnt) / pixcnt;
	  *pic24++ = (pixB/linecnt) / pixcnt;
	  lastpix = *paptr;
	  pixR = pixG = pixB = pixcnt = 0;
	}

	if (j<swide) {
	  pixR += lbufR[j];
	  pixG += lbufG[j];
	  pixB += lbufB[j];
	  pixcnt++;
	}
      }

      lastline = thisline;
      xvbzero( (char *) lbufR, swide * sizeof(int));  /* clear out line bufs */
      xvbzero( (char *) lbufG, swide * sizeof(int));
      xvbzero( (char *) lbufB, swide * sizeof(int));
      linecnt = 0;
    }

    if (i<shigh) {
      if (is24) {
	for (j=0; j<swide; j++) {
	  lbufR[j] += *cptr++;
	  lbufG[j] += *cptr++;
	  lbufB[j] += *cptr++;
	}
      }
      else {
	for (j=0; j<swide; j++, cptr++) {
	  lbufR[j] += rmap[*cptr];
	  lbufG[j] += gmap[*cptr];
	  lbufB[j] += bmap[*cptr];
	}
      }

      linecnt++;
    }
  }

  free(lbufR);  free(lbufG);  free(lbufB);  free(pixarr);
  return 0;
}




/********************************************/
byte *DoColorDither(pic24, pic8, w, h, rmap, gmap, bmap,
		    rdisp, gdisp, bdisp, maplen)
     byte *pic24, *pic8, *rmap, *gmap, *bmap, *rdisp, *gdisp, *bdisp;
     int   w, h, maplen;
{
  /* takes a 24 bit picture, of size w*h, dithers with the colors in
     rdisp, gdisp, bdisp (which have already been allocated),
     and generates an 8-bit w*h image, which it returns.
     ignores input value 'pic8'
     returns NULL on error

     note: the rdisp,gdisp,bdisp arrays should be the 'displayed' colors,
     not the 'desired' colors

     if pic24 is NULL, uses the passed-in pic8 (an 8-bit image) as
     the source, and the rmap,gmap,bmap arrays as the desired colors */

  byte *np, *ep, *newpic;
  short *cache;
  int r2, g2, b2;
  int *thisline, *nextline, *thisptr, *nextptr, *tmpptr;
  int  i, j, rerr, gerr, berr, pwide3;
  int  imax, jmax;
  int key;
  long cnt1, cnt2;
  int fserrmap[512];   /* -255 .. 0 .. +255 */

  /* compute somewhat non-linear floyd-steinberg error mapping table */
  for (i=j=0; i<=0x40; i++,j++)
    { fserrmap[256+i] = j;  fserrmap[256-i] = -j; }
  for (     ; i<0x80; i++, j += !(i&1) ? 1 : 0)
    { fserrmap[256+i] = j;  fserrmap[256-i] = -j; }
  for (     ; i<=0xff; i++)
    { fserrmap[256+i] = j;  fserrmap[256-i] = -j; }


  cnt1 = cnt2 = 0;
  pwide3 = w*3;  imax = h-1;  jmax = w-1;
  ep = (pic24) ? pic24 : pic8;

  /* attempt to malloc things */
  newpic = (byte *)  malloc((size_t) (w * h));
  cache  = (short *) calloc((size_t) (2<<14), sizeof(short));
  thisline = (int *) malloc(pwide3 * sizeof(int));
  nextline = (int *) malloc(pwide3 * sizeof(int));
  if (!cache || !newpic || !thisline || !nextline) {
    if (newpic)   free(newpic);
    if (cache)    free(cache);
    if (thisline) free(thisline);
    if (nextline) free(nextline);

    return (byte *) NULL;
  }

  np = newpic;

  /* get first line of picture */

  if (pic24) {
    for (j=pwide3, tmpptr=nextline; j; j--, ep++)
      *tmpptr++ = (int) *ep;
  }
  else {
    for (j=w, tmpptr=nextline; j; j--, ep++) {
      *tmpptr++ = (int) rmap[*ep];
      *tmpptr++ = (int) gmap[*ep];
      *tmpptr++ = (int) bmap[*ep];
    }
  }


  for (i=0; i<h; i++) {
    ProgressMeter(0, h-1, i, "Dither");
    if ((i&15) == 0) WaitCursor();

    tmpptr = thisline;  thisline = nextline;  nextline = tmpptr;   /* swap */

    if (i!=imax) {  /* get next line */
      if (!pic24)
	for (j=w, tmpptr=nextline; j; j--, ep++) {
	  *tmpptr++ = (int) rmap[*ep];
	  *tmpptr++ = (int) gmap[*ep];
	  *tmpptr++ = (int) bmap[*ep];
	}
      else
	for (j=pwide3, tmpptr=nextline; j; j--, ep++) *tmpptr++ = (int) *ep;
    }

    /* dither a line */
    for (j=0, thisptr=thisline, nextptr=nextline; j<w; j++,np++) {
      int k, d, mind, closest;

      r2 = *thisptr++;  g2 = *thisptr++;  b2 = *thisptr++;

      /* map r2,g2,b2 components (could be outside 0..255 range)
	 into 0..255 range */

      if (r2<0 || g2<0 || b2<0) {   /* are there any negatives in RGB? */
	if (r2<g2) { if (r2<b2) k = 0; else k = 2; }
	else { if (g2<b2) k = 1; else k = 2; }

	switch (k) {
	case 0:  g2 -= r2;  b2 -= r2;  d = (abs(r2) * 3) / 2;    /* RED */
	         r2 = 0;
	         g2 = (g2>d) ? g2 - d : 0;
	         b2 = (b2>d) ? b2 - d : 0;
	         break;

	case 1:  r2 -= g2;  b2 -= g2;  d = (abs(g2) * 3) / 2;    /* GREEN */
	         r2 = (r2>d) ? r2 - d : 0;
	         g2 = 0;
	         b2 = (b2>d) ? b2 - d : 0;
	         break;

	case 2:  r2 -= b2;  g2 -= b2;  d = (abs(b2) * 3) / 2;    /* BLUE */
	         r2 = (r2>d) ? r2 - d : 0;
	         g2 = (g2>d) ? g2 - d : 0;
	         b2 = 0;
	         break;
	}
      }

      if (r2>255 || g2>255 || b2>255) {   /* any overflows in RGB? */
	if (r2>g2) { if (r2>b2) k = 0; else k = 2; }
              else { if (g2>b2) k = 1; else k = 2; }

	switch (k) {
	case 0:   g2 = (g2*255)/r2;  b2 = (b2*255)/r2;  r2=255;  break;
	case 1:   r2 = (r2*255)/g2;  b2 = (b2*255)/g2;  g2=255;  break;
	case 2:   r2 = (r2*255)/b2;  g2 = (g2*255)/b2;  b2=255;  break;
	}
      }

      key = ((r2&0xf8)<<6) | ((g2&0xf8)<<1) | (b2>>4);
      if (key >= (2<<14)) FatalError("'key' overflow in DoColorDither()");

      if (cache[key]) { *np = (byte) (cache[key] - 1);  cnt1++;	}
      else {
	/* not in cache, have to search the colortable */
	cnt2++;

        mind = 10000;
	for (k=closest=0; k<maplen && mind>7; k++) {
	  d = abs(r2 - rdisp[k])
	    + abs(g2 - gdisp[k])
	    + abs(b2 - bdisp[k]);
	  if (d<mind) { mind = d;  closest = k; }
	}
	cache[key] = closest + 1;
	*np = closest;
      }


      /* propogate the error */
      rerr = r2 - rdisp[*np];
      gerr = g2 - gdisp[*np];
      berr = b2 - bdisp[*np];


      RANGE(rerr, -255, 255);
      RANGE(gerr, -255, 255);
      RANGE(berr, -255, 255);
      rerr = fserrmap[256+rerr];
      gerr = fserrmap[256+gerr];
      berr = fserrmap[256+berr];



      if (j!=jmax) {  /* adjust RIGHT pixel */
	thisptr[0] += (rerr*7)/16;
	thisptr[1] += (gerr*7)/16;
	thisptr[2] += (berr*7)/16;
      }

      if (i!=imax) {	/* do BOTTOM pixel */
	nextptr[0] += (rerr*5)/16;
	nextptr[1] += (gerr*5)/16;
	nextptr[2] += (berr*5)/16;

	if (j>0) {  /* do BOTTOM LEFT pixel */
	  nextptr[-3] += (rerr*3)/16;
	  nextptr[-2] += (gerr*3)/16;
	  nextptr[-1] += (berr*3)/16;
	}

	if (j!=jmax) {  /* do BOTTOM RIGHT pixel */
	  nextptr[3] += rerr/16;
	  nextptr[4] += gerr/16;
	  nextptr[5] += berr/16;
	}
	nextptr += 3;
      }
    }
  }


  free(thisline);  free(nextline);
  free(cache);

  return newpic;
}



/********************************************/
byte *Do332ColorDither(pic24, pic8, w, h, rmap, gmap, bmap,
		    rdisp, gdisp, bdisp, maplen)
     byte *pic24, *pic8, *rmap, *gmap, *bmap, *rdisp, *gdisp, *bdisp;
     int   w, h, maplen;
{
  /* some sort of color dither optimized for the 332 std cmap */

  /* takes a 24 bit picture, of size w*h, dithers with the colors in
     rdisp, gdisp, bdisp (which have already been allocated),
     and generates an 8-bit w*h image, which it returns.
     ignores input value 'pic8'
     returns NULL on error

     note: the rdisp,gdisp,bdisp arrays should be the 'displayed' colors,
     not the 'desired' colors

     if pic24 is NULL, uses the passed-in pic8 (an 8-bit image) as
     the source, and the rmap,gmap,bmap arrays as the desired colors */

  byte *np, *ep, *newpic;
  int r2, g2, b2;
  int *thisline, *nextline, *thisptr, *nextptr, *tmpptr;
  int  i, j, rerr, gerr, berr, pwide3;
  int  imax, jmax;
  long cnt1, cnt2;
  int  fserrmap[512];   /* -255 .. 0 .. +255 */

  /* compute somewhat non-linear floyd-steinberg error mapping table */
  for (i=j=0; i<=0x40; i++,j++)
    { fserrmap[256+i] = j;  fserrmap[256-i] = -j; }
  for (     ; i<0x80; i++, j += !(i&1) ? 1 : 0)
    { fserrmap[256+i] = j;  fserrmap[256-i] = -j; }
  for (     ; i<=0xff; i++)
    { fserrmap[256+i] = j;  fserrmap[256-i] = -j; }


  cnt1 = cnt2 = 0;
  pwide3 = w*3;  imax = h-1;  jmax = w-1;

  /* attempt to malloc things */
  newpic   = (byte *) malloc((size_t) (w * h));
  thisline = (int *)  malloc(pwide3 * sizeof(int));
  nextline = (int *)  malloc(pwide3 * sizeof(int));
  if (!newpic || !thisline || !nextline) {
    if (newpic)   free(newpic);
    if (thisline) free(thisline);
    if (nextline) free(nextline);

    return (byte *) NULL;
  }

  np = newpic;
  ep = (pic24) ? pic24 : pic8;


  /* get first line of picture */

  if (pic24) {
    for (j=pwide3, tmpptr=nextline; j; j--, ep++) *tmpptr++ = (int) *ep;
  }
  else {
    for (j=w, tmpptr=nextline; j; j--, ep++) {
      *tmpptr++ = (int) rmap[*ep];
      *tmpptr++ = (int) gmap[*ep];
      *tmpptr++ = (int) bmap[*ep];
    }
  }


  for (i=0; i<h; i++) {
    np = newpic + i*w;
    ProgressMeter(0, h-1, i, "Dither");
    if ((i&127) == 0) WaitCursor();

    tmpptr = thisline;  thisline = nextline;  nextline = tmpptr;   /* swap */

    if (i!=imax) {  /* get next line */
      if (!pic24)
	for (j=w, tmpptr=nextline; j; j--, ep++) {
	  *tmpptr++ = (int) rmap[*ep];
	  *tmpptr++ = (int) gmap[*ep];
	  *tmpptr++ = (int) bmap[*ep];
	}
      else
	for (j=pwide3, tmpptr=nextline; j; j--, ep++) *tmpptr++ = (int) *ep;
    }


    /* dither a line, doing odd-lines right-to-left (serpentine) */
    thisptr = (i&1) ? thisline + w*3 - 3 : thisline;
    nextptr = (i&1) ? nextline + w*3 - 3 : nextline;
    if (i&1) np += w-1;


    for (j=0; j<w; j++) {
      int rb,gb,bb;

      r2 = *thisptr++;  g2 = *thisptr++;  b2 = *thisptr++;
      if (i&1) thisptr -= 6;  /* move left */

      rb = (r2 + 0x10);    /* round top 3 bits */
      RANGE(rb,0,255);
      rb = rb & 0xe0;

      gb = (g2 + 0x10);    /* round 3 bits */
      RANGE(gb,0,255);
      gb = gb & 0xe0;

      bb = (b2 + 0x20);    /* round 2 bits */
      RANGE(bb,0,255);
      bb = bb & 0xc0;


      *np = rb | (gb>>3) | (bb>>6);

      /* propogate the error */
      rerr = r2 - rdisp[*np];
      gerr = g2 - gdisp[*np];
      berr = b2 - bdisp[*np];


      RANGE(rerr, -255, 255);
      RANGE(gerr, -255, 255);
      RANGE(berr, -255, 255);
      rerr = fserrmap[256+rerr];
      gerr = fserrmap[256+gerr];
      berr = fserrmap[256+berr];


      if (j!=jmax) {  /* adjust LEFT/RIGHT pixel */
	thisptr[0] += (rerr/2);  rerr -= (rerr/2);
	thisptr[1] += (gerr/2);  gerr -= (gerr/2);
	thisptr[2] += (berr/2);  berr -= (berr/2);
      }

      if (i!=imax) { /* adjust BOTTOM pixel */
	nextptr[0] += rerr;    /* possibly all err if we're at l/r edge */
	nextptr[1] += gerr;
	nextptr[2] += berr;
      }

      if (i&1) { nextptr -= 3;  np--; }
          else { nextptr += 3;  np++; }
    }
  }


  free(thisline);  free(nextline);

  return newpic;
}



