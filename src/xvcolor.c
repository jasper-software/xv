/*
 * xvcolor.c - color allocation/sorting/freeing code
 *
 *  Author:    John Bradley, University of Pennsylvania
 *                (bradley@cis.upenn.edu)
 *
 *  Contains:
 *     void   SortColormap()
 *     void   ColorCompress8(trans)
 *     void   AllocColors()
 *     void   FreeColors()
 *     Status xvAllocColor()
 *     void   xvFreeColors()
 *     void   ApplyEditColor();
 *     int    MakeStdCmaps();
 *     void   MakeBrowCmap();
 *     void   ChangeCmapMode();
 */

#include "copyright.h"

#include "xv.h"


static void allocROColors  PARM((void));
static void allocRWColors  PARM((void));
static void putECfirst     PARM((void));
static void diverseOrder   PARM((byte *, byte *, byte *, int, byte *));
static void freeStdCmaps   PARM((void));
static int  highbit        PARM((unsigned long));


static char stdCmapSuccess[80];


/************************************************/
/* structure and routine used in SortColormap() */
/************************************************/

typedef struct thing {
  byte r,g,b, n;          /* actual value of color + alignment */
  int oldindex;           /* its index in the old colormap */
  int use;                /* # of pixels of this color */
  int mindist;            /* min distance to a selected color */
} CMAPENT;



/***********************************/
void SortColormap(pic, pwide, phigh, pnumcols, rmap,gmap,bmap, order, trans)
     byte *pic, *rmap, *gmap, *bmap, *order, *trans;
     int   pwide, phigh, *pnumcols;
{
  /* operates on 8-bit images.  sorts the colormap into 'best' order
   * 'order' is the 'best' order to allocate the colors.  'trans' is a 
   * transformation to be done to pic, cpic, and epic (in PIC8 mode) to
   * compress the colormap
   */

  byte *p;
  int   i, j, mdist, entry, d, hist[256], ncols;
  static CMAPENT c[256], c1[256], *cp, *cj, *ck;

  /* init some stuff */
  for (i=0; i<256; i++) order[i]=i;

  /* initialize histogram and compute it */
  for (i=0; i<256; i++) hist[i]=0;
  for (i=pwide*phigh, p=pic; i; i--, p++) hist[*p]++;
  
  if (DEBUG>1) {
    fprintf(stderr,"%s: Desired colormap\n",cmd);
    for (i=0; i<256; i++) 
      if (hist[i]) fprintf(stderr,"(%3d  %02x,%02x,%02x %d)\n",
			   i,rmap[i],gmap[i],bmap[i], hist[i]);
    fprintf(stderr,"\n\n");
  }
  
  
  /* put the actually-used colors into the 'c' array in the order they occur
     also, while we're at it, calculate ncols, and close up gaps in
     colortable */
  
  for (i=ncols=0; i<256; i++) {
    if (hist[i]) { 
      rmap[ncols] = rmap[i];
      gmap[ncols] = gmap[i];
      bmap[ncols] = bmap[i];
      trans[i]    = (byte) ncols;

      cp = &c[ncols];
      cp->r = rmap[i];    cp->g = gmap[i];    cp->b = bmap[i];
      cp->use = hist[i];  cp->oldindex = ncols;
      cp->mindist = 1000000; /* 255^2 * 3 = 195075 */
      ncols++;
    }
  }


  /* find most-used color, put that in c1[0] */
  entry = -1;  mdist = -1;
  for (i=0; i<ncols; i++) {
    if (c[i].use > mdist) { mdist = c[i].use;  entry=i; }
  }
  xvbcopy((char *) &c[entry], (char *) &c1[0], sizeof(CMAPENT));
  c[entry].use = 0;   /* dealt with */
  
  
  /* sort rest of colormap.  Half of the entries are allocated on the
     basis of distance from already allocated colors, and half on the
     basis of usage.  (NB: 'taxicab' distance is used throughout this file.)

     Mod:  pick first 10 colors based on maximum distance.  pick remaining
     colors half by distance and half by usage   -- JHB

     To obtain O(n^2) performance, we keep each unselected color
     (in c[], with use>0) marked with the minimum distance to any of
     the selected colors (in c1[]).  Each time we select a color, we
     can update the minimum distances in O(n) time. 

     mod by Tom Lane   Tom.Lane@g.gp.cs.cmu.edu */

  for (i=1; i<ncols; i++) {
    int ckR, ckG, ckB;
    /* Get RGB of color last selected  and choose selection method */
    ck = &c1[i-1];            /* point to just-selected color */
    ckR = ck->r; ckG = ck->g; ckB = ck->b;

    /* Now find the i'th most different color */

    if (i&1 || i<10) {
      /* select the unused color that has the greatest mindist */
      entry = -1;  mdist = -1;
      for (j=0, cj=c; j<ncols; j++,cj++) {
	if (cj->use) {      /* this color has not been marked already */
	  /* update mindist */
          d = (cj->r - ckR)*(cj->r - ckR) + 
	      (cj->g - ckG)*(cj->g - ckG) + 
	      (cj->b - ckB)*(cj->b - ckB);
          if (cj->mindist > d) cj->mindist = d;
	  if (cj->mindist > mdist) { mdist = cj->mindist;  entry = j; }
	}
      }
    }
    else {
      /* select the unused color that has the greatest usage */
      entry = -1;  mdist = -1;
      for (j=0, cj=c; j<ncols; j++,cj++) {
	if (cj->use) {  /* this color has not been marked already */
	  /* update mindist */
          d = (cj->r - ckR)*(cj->r - ckR) + 
	      (cj->g - ckG)*(cj->g - ckG) + 
    	      (cj->b - ckB)*(cj->b - ckB);
          if (cj->mindist > d) cj->mindist = d;
	  if (cj->use > mdist) { mdist = cj->use;  entry = j; }
	}
      }
    }


    /* c[entry] is the next color to put in the map.  do so */
    xvbcopy((char *) &c[entry], (char *) &c1[i], sizeof(CMAPENT));
    c[entry].use = 0;
  }
  

  for (i=0; i<ncols; i++) order[i] = (byte) c1[i].oldindex;

  if (DEBUG>1) {
    fprintf(stderr,"%s: result of sorting colormap\n",cmd);
    for (i=0; i<ncols; i++) 
      fprintf(stderr,"(%3d  %02x,%02x,%02x)     ",i,rmap[i],gmap[i],bmap[i]);
    fprintf(stderr,"\n\n");
    
    fprintf(stderr,"%s: allocation order table\n",cmd);
    for (i=0; i<ncols; i++) 
      fprintf(stderr,"order[%d] = -> %d\n", i, order[i]);
    fprintf(stderr,"\n");
  }

  *pnumcols = ncols;
}


/***********************************/
void ColorCompress8(trans)
     byte *trans;
{
  /* modify pic,cpic,epic to reflect new (compressed) colormap */

  int i;
  byte *p, j;

  for (i=pWIDE*pHIGH, p=pic; i; i--, p++) { j = trans[*p];  *p = j; }

  if (cpic && cpic != pic) {
    for (i=cWIDE*cHIGH, p=cpic; i; i--, p++) { j = trans[*p];  *p = j; }
  }

  if (epic && epic != cpic) {
    for (i=eWIDE*eHIGH, p=epic; i; i--, p++) { j = trans[*p];  *p = j; }
  }
}



/***********************************/
void AllocColors()
{
  int i;

  nfcols = 0;

  if (ncols == 0) {
    for (i=0; i<numcols; i++) {
      rdisp[i] = rMap[i];
      gdisp[i] = gMap[i];
      bdisp[i] = bMap[i];
    }

    SetISTR(ISTR_COLOR,"Dithering with 'black' & 'white'.");
    SetISTR(ISTR_COLOR2,"");
    rwthistime = 0;

    RedrawCMap();
    return;
  }


  if (colorMapMode == CM_STDCMAP) {
    /* map desired image colors to closest standard colors */

    if (picType == PIC24 &&
	(theVisual->class == TrueColor || theVisual->class == DirectColor)) {
      SetISTR(ISTR_COLOR,"Using %s visual.",
	      (theVisual->class == TrueColor) ? "TrueColor" : "DirectColor");
      SetISTR(ISTR_COLOR2,"");
    }
    else {
      SetISTR(ISTR_COLOR,"Using %s colormap.",
	      (haveStdCmap == STD_111 ? "2x2x2" :
	       haveStdCmap == STD_222 ? "4x4x4" :
	       haveStdCmap == STD_232 ? "4x8x4" : 
               haveStdCmap == STD_666 ? "6x6x6" : "8x8x4"));

      if (ncols>0) SetISTR(ISTR_COLOR2,stdCmapSuccess);
              else SetISTR(ISTR_COLOR2,"Dithering with 'black' & 'white'.");
    }

    rwthistime = 0;

    for (i=0; i<numcols; i++) {
      int i332;
      i332 = ((int)rMap[i]&0xe0) | (((int)gMap[i]&0xe0)>>3) | 
	     (((int)bMap[i]&0xc0)>>6);

      cols[i]  = stdcols[i332];
      rdisp[i] = stdrdisp[i332];
      gdisp[i] = stdgdisp[i332];
      bdisp[i] = stdbdisp[i332];
    }
  }


  else if (allocMode == AM_READWRITE) allocRWColors();
  else allocROColors();

  RedrawCMap();
}


/********************************/
void FreeColors()
{
  int i;

  /* frees all colors allocated by 'AllocColors()'.  Doesn't touch stdcmap */
  /* Note:  might be called multiple times.  Must not free colors once it
     has done so */

  if (LocalCmap) {
    XSetWindowAttributes xswa;

    xswa.colormap = None;
    XChangeWindowAttributes(theDisp,mainW,CWColormap,&xswa);
    if (cmapInGam) XChangeWindowAttributes(theDisp,gamW,CWColormap,&xswa);

    XFreeColormap(theDisp,LocalCmap);
    LocalCmap = 0;
    nfcols = 0;
  }

  else {
    for (i=0; i<nfcols; i++) 
      xvFreeColors(theDisp, theCmap, &freecols[i], 1, 0L);

    nfcols = 0;
    XFlush(theDisp);
  }
}


/***********************************/
static void allocROColors()
{
  int      i, j, c, unique, p2alloc;
  Colormap cmap;
  XColor   defs[256];
  XColor   ctab[256];
  int      failed[256];
  int      dc;


  unique = p2alloc = 0;
  rwthistime = 0;

  /* FIRST PASS COLOR ALLOCATION:  
     for each color in the 'desired colormap', try to get it via
     xvAllocColor().  If for any reason it fails, mark that pixel
     'unallocated' and worry about it later.  Repeat. */

  /* attempt to allocate first ncols entries in colormap 
     note: On displays with less than 8 bits per RGB gun, it's quite
     possible that different colors in the original picture will be
     mapped to the same color on the screen.  X does this for you
     silently.  However, this is not-desirable for this application, 
     because when I say 'allocate me 32 colors' I want it to allocate
     32 different colors, not 32 instances of the same 4 shades... */
  

  for (i=0; i<256; i++) failed[i] = 1;

  cmap = (LocalCmap) ? LocalCmap : theCmap;

  for (i=0; i<numcols && unique<ncols; i++) {
    c = colAllocOrder[i];
    if (mono) { 
      int intens = MONO(rMap[c], gMap[c], bMap[c]);
      defs[c].red = defs[c].green = defs[c].blue = intens<<8;
    }
    else {
      defs[c].red   = rMap[c]<<8;
      defs[c].green = gMap[c]<<8;
      defs[c].blue  = bMap[c]<<8;
    }

    defs[c].flags = DoRed | DoGreen | DoBlue;

    if (!(colorMapMode==CM_OWNCMAP && cmap==theCmap && CMAPVIS(theVisual)) 
	&& xvAllocColor(theDisp,cmap,&defs[c])) { 
      unsigned long pixel, *fcptr;

      pixel = cols[c] = defs[c].pixel;
      rdisp[c] = defs[c].red   >> 8;
      gdisp[c] = defs[c].green >> 8;
      bdisp[c] = defs[c].blue  >> 8;
      failed[c]= 0;
      
      /* see if the newly allocated color is new and different */
      for (j=0, fcptr=freecols; j<nfcols && *fcptr!=pixel; j++,fcptr++);
      if (j==nfcols) unique++;

      fc2pcol[nfcols] = c;
      freecols[nfcols++] = pixel;
    }

    else {
      /* the allocation failed.  If we want 'perfect' color, and we haven't 
	 already created our own colormap, we'll want to do so */
      if ((colorMapMode == CM_PERFECT || colorMapMode == CM_OWNCMAP)
	  && !LocalCmap && CMAPVIS(theVisual)) {
	LocalCmap = XCreateColormap(theDisp, vrootW, theVisual, AllocNone);
	
	if (LocalCmap) {  /* succeeded, presumably */
	  /* free all colors that were allocated, and try again with the
	     new colormap.  This is necessary because 'XCopyColormapAndFree()'
	     has the unpleasant side effect of freeing up the various
	     colors I need for the control panel, etc. */

	  for (i=0; i<nfcols; i++) 
	    xvFreeColors(theDisp, theCmap, &freecols[i], 1, 0L);
	  
	  if (mainW && !useroot) XSetWindowColormap(theDisp,mainW, LocalCmap);

	  if (mainW && !useroot && cmapInGam) 
	    XSetWindowColormap(theDisp,gamW, LocalCmap);
	  cmap = LocalCmap;

	  /* redo ALL allocation requests */
	  for (i=0; i<256; i++) failed[i] = 1;
	  nfcols = unique = 0;
	  i = -1;
	}
      }

      else {
	/* either we don't care about perfect color, or we do care, have
	   allocated our own colormap, and have STILL run out of colors
	   (possible, even on an 8 bit display), just mark pixel as
	   unallocated.  We'll deal with it later */
	failed[c] = 1;
      }
    }
  }  /* FIRST PASS */
  
  
  
  if (nfcols==numcols) {
    if (numcols != unique)
      SetISTR(ISTR_COLOR,"Got all %d colors.  (%d unique)", numcols,
	      unique);
    else
      SetISTR(ISTR_COLOR,"Got all %d colors.", numcols);

    SetISTR(ISTR_COLOR2,"");
    return;
  }
  


  /* SECOND PASS COLOR ALLOCATION:
     Allocating 'exact' colors failed.  Now try to allocate 'closest'
     colors.

     Read entire X colormap (or first 256 entries) in from display.
     for each unallocated pixel, find the closest color that actually
     is in the X colormap.  Try to allocate that color (read only).
     If that fails, the THIRD PASS will deal with it */

  SetISTR(ISTR_COLOR,"Got %d of %d colors.  (%d unique)", 
	  nfcols,numcols,unique);

  /* read entire colormap (or first 256 entries) into 'ctab' */
  dc = (ncells<256) ? ncells : 256;

  if (dc>0) {  /* only do SECOND PASS if there IS a colormap to read */
    for (i=0; i<dc; i++) ctab[i].pixel = (unsigned long) i;
    XQueryColors(theDisp, cmap, ctab, dc);
    
    for (i=0; i<numcols && unique<ncols; i++) {
      c = colAllocOrder[i];
      
      if (failed[c]) {  /* an unallocated pixel */
	int d, mdist, close;
	int rd, gd, bd, ri, gi, bi;
	
	mdist = 1000000;   close = -1;
	ri = rMap[c];  gi = gMap[c];  bi = bMap[c];
	
	for (j=0; j<dc; j++) {
	  rd = ri - (ctab[j].red  >>8);
	  gd = gi - (ctab[j].green>>8);
	  bd = bi - (ctab[j].blue >>8);
	  
	  d = rd*rd + gd*gd + bd*bd;
	  if (d<mdist) { mdist=d; close=j; }
	}
	
	if (close<0) FatalError("This Can't Happen! (How reassuring.)");
	if (xvAllocColor(theDisp, cmap, &ctab[close])) { 
	  xvbcopy((char *) &ctab[close], (char *) &defs[c], sizeof(XColor));
	  failed[c]= 0;
	  cols[c]  = ctab[close].pixel;
	  rdisp[c] = ctab[close].red   >> 8;
	  gdisp[c] = ctab[close].green >> 8;
	  bdisp[c] = ctab[close].blue  >> 8;
	  fc2pcol[nfcols] = c;
	  freecols[nfcols++] = cols[c];
	  p2alloc++;
	  unique++;
	}
      }
    }
  }


  /* THIRD PASS COLOR ALLOCATION:
     We've alloc'ed all the colors we can.  Now, we have to map any
     remaining unalloced pixels into either the colors that we DID get */

  for (i=0; i<numcols; i++) {
    c = colAllocOrder[i];

    if (failed[c]) {  /* an unallocated pixel */
      int d, k, mdist, close;
      int rd,gd,bd, ri,gi,bi;

      mdist = 1000000;   close = -1;
      ri = rMap[c];  gi = gMap[c];  bi = bMap[c];
      
      /* search the alloc'd colors */
      for (j=0; j<nfcols; j++) {
	k = fc2pcol[j];
	rd = ri - (defs[k].red  >>8);
	gd = gi - (defs[k].green>>8);
	bd = bi - (defs[k].blue >>8);

	d = rd*rd + gd*gd + bd*bd;

	if (d<mdist) { mdist=d;  close=k; }
      }

      if (close<0) FatalError("Pass3: Failed to alloc ANY colors!\n");
      xvbcopy((char *) &defs[close], (char *) &defs[c], sizeof(XColor));
      failed[c]= 0;
      cols[c]  = defs[c].pixel;
      rdisp[c] = defs[c].red   >> 8;
      gdisp[c] = defs[c].green >> 8;
      bdisp[c] = defs[c].blue  >> 8;
    }
  }  /* THIRD PASS */



  if (p2alloc) SetISTR(ISTR_COLOR2,"Got %d 'close' color%s.",
		       p2alloc, (p2alloc>1) ? "s" : "");
}



/***********************************/
static void allocRWColors()
{
  int      i,j,c, failed[256];
  Colormap cmap;
  XColor   defs[256];

  rwthistime = 1;

  cmap = (LocalCmap) ? LocalCmap : theCmap;

  for (i=0; i<numcols; i++) failed[colAllocOrder[i]] = 1;

  for (i=0; i<numcols && i<ncols; i++) {
    unsigned long pmr[1], pix[1];
    c = colAllocOrder[i];

    if (cellgroup[c]) {  
      int n;
      /* this color is part of a group.  see if its group's
	 been seen already, and if so, skip this */
      for (n=0; n<i && cellgroup[c] != cellgroup[colAllocOrder[n]]; n++);
      if (n<i) {  /* found one */
	cols[c] = cols[colAllocOrder[n]];
	rwpc2pc[c] = colAllocOrder[n];
	failed[c] = 0;
	continue;
      }
    }

    if (!(colorMapMode==CM_OWNCMAP && cmap==theCmap && CMAPVIS(theVisual)) && 
	XAllocColorCells(theDisp, cmap, False, pmr, 0, pix, 1)) {
      defs[c].pixel = cols[c] = pix[0];
      failed[c] = 0;
      if (mono) { 
	int intens = MONO(rMap[c], gMap[c], bMap[c]);
	defs[c].red = defs[c].green = defs[c].blue = intens<<8;
      }
      else {
	defs[c].red   = rMap[c]<<8;
	defs[c].green = gMap[c]<<8;
	defs[c].blue  = bMap[c]<<8;
      }

      defs[c].flags = DoRed | DoGreen | DoBlue;
      rdisp[c] = rMap[c];
      gdisp[c] = gMap[c];
      bdisp[c] = bMap[c];

      fc2pcol[nfcols]    = c;
      rwpc2pc[c]         = c;
      freecols[nfcols++] = pix[0];
    }

    else {
      if ((colorMapMode == CM_PERFECT || colorMapMode == CM_OWNCMAP) 
	  && !LocalCmap && CMAPVIS(theVisual)) {
	LocalCmap = XCreateColormap(theDisp, vrootW, theVisual, AllocNone);
	
	/* free all colors that were allocated, and try again with the
	   new colormap.  This is necessary because 'XCopyColormapAndFree()'
	   has the unpleasant side effect of freeing up the various
	   colors I need for the control panel, etc. */

	for (i=0; i<nfcols; i++) 
	  xvFreeColors(theDisp, theCmap, &freecols[i], 1, 0L);
	
	if (mainW && !useroot) XSetWindowColormap(theDisp,mainW, LocalCmap);
	if (mainW && !useroot && cmapInGam) 
	  XSetWindowColormap(theDisp,gamW, LocalCmap);
	cmap = LocalCmap;

	/* redo ALL allocation requests */
	for (i=0; i<numcols; i++) failed[colAllocOrder[i]] = 1;
	nfcols = 0;
	i = -1;
      }

      else failed[c] = 1;
    }
  }  /* for (i=0; ... */



  if (nfcols==numcols) {
    SetISTR(ISTR_COLOR,"Got all %d colors.", numcols);
    SetISTR(ISTR_COLOR2,"");
  }

  else {
    /* Failed to allocate all colors in picture.  Map remaining desired 
       colors into closest allocated desired colors */

      if (nfcols==0 && !LocalCmap) {
	char tstr[128], *tmp,
	    *foo = "No r/w cells available.  Using r/o color.";

	tmp = GetISTR(ISTR_WARNING);
	if (strlen(tmp) > (size_t) 0) sprintf(tstr, "%s  %s", tmp, foo);
	else sprintf(tstr, "%s", foo);
	SetISTR(ISTR_WARNING,tstr);

	allocROColors();
	return;
      }
	
      SetISTR(ISTR_COLOR,"Got %d of %d colors.",  nfcols,numcols);

      for (i=0; i<numcols; i++) {
	c = colAllocOrder[i];
	if (failed[c]) {  /* an unallocated pixel */
	  int k, d, mdist, close;
	  int rd, gd, bd, ri, gi, bi;

	  mdist = 1000000;   close = -1;
	  ri = rMap[c];  gi = gMap[c];  bi = bMap[c];

	  for (j=0; j<nfcols; j++) {
	    k = fc2pcol[j];
	    rd = ri - (defs[k].red  >>8);
	    gd = gi - (defs[k].green>>8);
	    bd = bi - (defs[k].blue >>8);

	    d = rd*rd + gd*gd + bd*bd;
	    if (d<mdist) { mdist=d; close=k; }
	  }

	  if (close<0) FatalError("This Can't Happen! (How reassuring.)");
	  xvbcopy((char *) &defs[close], (char *) &defs[c], sizeof(XColor));
	  failed[c]= 0;
	  cols[c]  = defs[c].pixel;
	  rdisp[c] = defs[c].red   >> 8;
	  gdisp[c] = defs[c].green >> 8;
	  bdisp[c] = defs[c].blue  >> 8;
	  rwpc2pc[c] = close;
	}
      }
    }

  /* load up the allocated colorcells */
  for (i=0; i<nfcols; i++) {
    j = fc2pcol[i];
    defs[j].pixel = freecols[i];

    if (mono) { 
      int intens = MONO(rMap[j], gMap[j], bMap[j]);
      defs[j].red = defs[j].green = defs[j].blue = intens<<8;
    }
    else {
      defs[j].red   = rMap[j]<<8;
      defs[j].green = gMap[j]<<8;
      defs[j].blue  = bMap[j]<<8;
    }

    defs[j].flags = DoRed | DoGreen | DoBlue;
    XStoreColor(theDisp, cmap, &defs[j]);
  }
}





/*******************************************************/
/* 24/32-bit TrueColor display color 'allocation' code */
/*******************************************************/

static int highbit(ul)
     unsigned long ul;
{
  /* returns position of highest set bit in 'ul' as an integer (0-31),
   or -1 if none */

  int i;  unsigned long hb;
  hb = 0x8000;  hb = (hb<<16);  /* hb = 0x80000000UL */
  for (i=31; ((ul & hb) == 0) && i>=0;  i--, ul<<=1);
  return i;
}


Status xvAllocColor(dp, cm, cdef)
     Display *dp;
     Colormap cm;
     XColor *cdef;
{
  if (theVisual->class == TrueColor || theVisual->class == DirectColor) {
    unsigned long r, g, b, rmask, gmask, bmask, origr, origg, origb;
    int rshift, gshift, bshift;
    
    /* shift r,g,b so that high bit of 16-bit color specification is 
     * aligned with high bit of r,g,b-mask in visual, 
     * AND each component with its mask,
     * and OR the three components together
     */

    origr = r = cdef->red;  origg = g = cdef->green;  origb = b = cdef->blue;

    if (theVisual->class == DirectColor) {
      /* r,g,b are 16-bit values  */
      int maplen = theVisual->map_entries;
      if (maplen>256) maplen = 256;

      rshift = 15 - highbit((unsigned long) (maplen-1));
      r = (u_long) directConv[(r>>rshift) & 0xff];
      g = (u_long) directConv[(g>>rshift) & 0xff];
      b = (u_long) directConv[(b>>rshift) & 0xff];

      /* shift the bits back up */
      r = r << rshift;
      g = g << rshift;
      b = b << rshift;
    }


    rmask = theVisual->red_mask;
    gmask = theVisual->green_mask;
    bmask = theVisual->blue_mask;

    rshift = 15 - highbit(rmask);
    gshift = 15 - highbit(gmask);
    bshift = 15 - highbit(bmask);

    /* shift the bits around */
    if (rshift<0) r = r << (-rshift);
             else r = r >> rshift;

    if (gshift<0) g = g << (-gshift);
             else g = g >> gshift;

    if (bshift<0) b = b << (-bshift);
             else b = b >> bshift;


    r = r & rmask;
    g = g & gmask;
    b = b & bmask;


    cdef->pixel = r | g | b;


    /* put 'exact' colors into red,green,blue fields */
    /* shift the bits BACK to where they were, now that they've been masked */
    if (rshift<0) r = r >> (-rshift);
             else r = r << rshift;

    if (gshift<0) g = g >> (-gshift);
             else g = g << gshift;

    if (bshift<0) b = b >> (-bshift);
             else b = b << bshift;

    cdef->red = r;  cdef->green = g;  cdef->blue = b;


    if (DEBUG > 1) {
      fprintf(stderr,
	      "xvAlloc: col=%04lx,%04lx,%04lx -> exact=%04x,%04x,%04x\n",
	      origr, origg, origb, cdef->red, cdef->green, cdef->blue);
      fprintf(stderr,
	      "         mask=%04lx,%04lx,%04lx  pix=%08lx\n",
	      rmask, gmask, bmask, cdef->pixel);
    }
    
    return 1;
  }
  else {
    return (XAllocColor(dp,cm,cdef));
  }
}


void xvFreeColors(dp, cm, pixels, npixels, planes)
     Display *dp;
     Colormap cm;
     unsigned long *pixels;
     int npixels;
     unsigned long planes;
{
  if (theVisual->class != TrueColor && theVisual->class != DirectColor)
    XFreeColors(dp, cm, pixels, npixels, planes);
}





/********************************/
void ApplyEditColor(regroup)
int regroup;
{
  int i, j;

  /* if regroup is set, we *must* do a full realloc, as the cols[] array 
     isn't correct anymore.  (cell groupings changed) */

  ApplyECctrls();  /* set {r,g,b}cmap[editColor] based on dial settings */
  Gammify1(editColor);

  if (curgroup) {  /* do the same to all its friends */
    for (i=0; i<numcols; i++) {
      if (cellgroup[i] == curgroup) {
	rcmap[i] = rcmap[editColor];
	gcmap[i] = gcmap[editColor];
	bcmap[i] = bcmap[editColor];
	rMap[i]  = rMap[editColor];
	gMap[i]  = gMap[editColor];
	bMap[i]  = bMap[editColor];
      }
    }
  }

    
  /* do something clever if we're using R/W color and this colorcell isn't
     shared */

  if (!regroup && allocMode==AM_READWRITE && rwthistime) {
    /* let's try to be clever */
    /* determine if the editColor cell is unique, or shared (among 
       non-group members, that is) */

    for (i=j=0; i<numcols; i++) 
      if (rwpc2pc[i] == rwpc2pc[editColor]) j++;

    /* if this is a group, subtract off the non-this-one pixels from group */
    if (curgroup) {
      for (i=0; i<numcols; i++) {
	if (cellgroup[i] == curgroup && i!=editColor) j--;
      }
    }

    if (j==1) {  /* we can be way cool about this one */
      XColor ctab;
      ctab.pixel = cols[editColor];
      if (mono) {
	int intens = MONO(rMap[editColor], gMap[editColor], bMap[editColor]);
	ctab.red = ctab.green = ctab.blue = intens<<8;
      }
      else {
	ctab.red   = rMap[editColor]<<8;
	ctab.green = gMap[editColor]<<8;
	ctab.blue  = bMap[editColor]<<8;
      }
      ctab.flags = DoRed | DoGreen | DoBlue;
      XStoreColor(theDisp, LocalCmap ? LocalCmap : theCmap, &ctab);
      rdisp[editColor] = rMap[editColor];
      gdisp[editColor] = gMap[editColor];
      bdisp[editColor] = bMap[editColor];
      return;
    }
  }

  /* either we aren't using R/W color, or we are, but this particular color
     cell isn't mapped a unique X colorcell.  Either way... */

  FreeColors();
  putECfirst();     /* make certain this one gets alloc'd */
  AllocColors();

  DrawEpic();
  SetCursors(-1);
}


/**************************************/
static void putECfirst()
{
  /* called after all colors have been freed up, but before reallocating.
     moves color #editColor to first in 'colAllocOrder' list, so that it
     is most-likely to get its desired color */

  int i;

  /* find it in the list */
  for (i=0; i<numcols; i++) {
    if (editColor == colAllocOrder[i]) break;
  }

  if (i==numcols || i==0) { /* didn't find it, or it's first already */
    return;
  }

  /* shift 0..i-1 down one position */
  xvbcopy((char *) colAllocOrder, (char *) colAllocOrder+1, 
	  i * sizeof(colAllocOrder[0]));
  colAllocOrder[0] = editColor;
}








#define CDIST(x,y,z)  ((x)*(x) + (y)*(y) + (z)*(z))

/***************************************************************/
int MakeStdCmaps()
{
  /* produces many things:
   *   stdr,stdg,stdb[256] - a 256-entry, desired 3/3/2 colormap
   *   stdcols[256]        - 256-entry, maps 3/3/2 colors into X pixel values
   *   stdrdisp,stdgdisp, stdbdisp[256]
   *                       - for a given 3/3/2 color, gives the rgb values
   *			     actually displayed.  Since we're not going to
   *			     successfully allocate all 256 colors (generally),
   *			     several 3/3/2 colors may be mapped into the
   *			     same X pixel (and hence, same rgb value)
   *   stdfreecols[256]    - list of colors to free on exit
   *   stdnfcols           - # of colors to free
   *
   * possibly modifies browR, browG, browB, and browcols arrays 
   *     (if !browPerfect)
   */       

  /* returns '1' if the colors were reallocated, '0' otherwise */

  /*
   * if we're on a TrueColor/DirectColor visual, it will attempt to alloc
   *    256 colors
   * otherwise, if colorMapMode == CM_STDCMAP, it will attempt to alloc
   *    6*6*6 = 216 colors, or 4*4*4 = 64, or 2*2*2 = 8 colors, depending
   *    on 'ncols' variable
   */

  /* note:
   *   if (ncols==0) (ie, we're either on, or emulating a b/w display),
   *   build std*[], std*disp[], colormaps, but don't actually 
   *   allocate any colors.
   */

  int i,j,r,g,b, desMode, screwed;
  XColor def;
  byte rmap[256],gmap[256],bmap[256],order[256];
  unsigned long descols[256];
  int des2got[256], failed[256];
  int maplen, exactCnt, nearCnt;
  
  
  /* generate stdr,stdg,stdb cmap.  Same in all cases */
  for (r=0, i=0; r<8; r++)
    for (g=0; g<8; g++)
      for (b=0; b<4; b++,i++) {
	stdr[i] = (r*255)/7;
	stdg[i] = (g*255)/7;
	stdb[i] = (b*255)/3;
      }
  
  
  /* determine what size cmap we should build */
  if (theVisual->class == TrueColor || 
      theVisual->class == DirectColor) desMode = STD_332;
  else if (colorMapMode == CM_STDCMAP) desMode = STD_232;
  else desMode = STD_222;


  /* make sure that we're not exceeding 'ncols' (ignore ncols==0) */
  if (ncols > 0) {
    if      (ncols < 64)  desMode = STD_111;
    else if (ncols < 128) desMode = STD_222;
    else if (ncols < 256 && desMode == STD_332) desMode = STD_232;
  }


  if (DEBUG) fprintf(stderr,"MakeStdCmaps: have=%d, des=%d, ncols=%d\n", 
		     haveStdCmap, desMode, ncols);
  
  if (haveStdCmap != STD_NONE && haveStdCmap == desMode) return 0;
  freeStdCmaps();


  /* init some stuff */
  screwed = 0;
  stdnfcols = 0;
  for (i=0; i<256; i++) stdcols[i] = stdfreecols[i] = 0;
  for (i=0; i<256; i++) des2got[i] = i;
  exactCnt = nearCnt = 0;

  
  if (desMode == STD_111) {   /* try to alloc 8 colors */
    /* generate a 1/1/1 desired colormap */
    maplen = 8;
    for (r=0, i=0; r<2; r++)
      for (g=0; g<2; g++)
	for (b=0; b<2; b++,i++) {
	  rmap[i] = (r*255);
	  gmap[i] = (g*255);
	  bmap[i] = (b*255);
	}
  }
  
  else if (desMode == STD_222) {   /* try to alloc 64 colors */
    /* generate a 2/2/2 desired colormap */
    maplen = 64;
    for (r=0, i=0; r<4; r++)
      for (g=0; g<4; g++)
	for (b=0; b<4; b++,i++) {
	  rmap[i] = (r*255)/3;
	  gmap[i] = (g*255)/3;
	  bmap[i] = (b*255)/3;
	}
  }
  
  else if (desMode == STD_232) {   /* try to alloc 128 colors */
    /* generate a 2/3/2 desired colormap */
    maplen = 128;
    for (r=0, i=0; r<4; r++)
      for (g=0; g<8; g++)
	for (b=0; b<4; b++,i++) {
	  rmap[i] = (r*255)/3;
	  gmap[i] = (g*255)/7;
	  bmap[i] = (b*255)/3;
	}
  }
  
  else if (desMode == STD_666) {   /* try to alloc 216 colors */
    /* generate a 6*6*6 desired colormap */
    maplen = 216;
    for (r=0, i=0; r<6; r++)
      for (g=0; g<6; g++)
	for (b=0; b<6; b++,i++) {
	  rmap[i] = (r*255)/5;
	  gmap[i] = (g*255)/5;
	  bmap[i] = (b*255)/5;
	}
  }
  
  else {   /* desMode == STD_332 */
    maplen = 256;
    for (i=0; i<maplen; i++) {
      rmap[i] = stdr[i];  gmap[i] = stdg[i];  bmap[i] = stdb[i];
    }
  }
  

  /* sort the colors according to the diversity algorithm... */
  diverseOrder(rmap,gmap,bmap,maplen,order);


  for (i=0; i<maplen; i++) failed[i] = 0;

  if (ncols!=0) {
    XColor ctab[256];
    long d, mind;
    int  dc,num,j,rd,gd,bd, numgot;

    numgot = 0;

    /* try to allocate the desired (rmap,gmap,bmap) colormap */
    for (i=0; i<maplen; i++) {
      def.red   = rmap[order[i]] << 8;
      def.green = gmap[order[i]] << 8;
      def.blue  = bmap[order[i]] << 8;
      
      def.flags = DoRed | DoGreen | DoBlue;

      if (xvAllocColor(theDisp, theCmap, &def)) {  /* success */
	numgot++;
	des2got[order[i]] = order[i];
	descols[order[i]] = def.pixel;
	for (j=0; j<stdnfcols && stdfreecols[j]!=def.pixel; j++);
	if (j==stdnfcols) exactCnt++;     /* new and different */

	stdfreecols[stdnfcols++] = def.pixel;
      }
      else failed[order[i]] = 1;
    }


    if (numgot != maplen) {
      /* PHASE 2:  find 'close' colors in colormap, try to alloc those */
      
      /* read entire colormap (or first 256 entries) into 'ctab' */
      dc = (ncells<256) ? ncells : 256;
      if (dc>0) {
	for (i=0; i<dc; i++) ctab[i].pixel = (unsigned long) i;
	XQueryColors(theDisp, theCmap, ctab, dc);
	
	for (i=0; i<maplen; i++) {
	  if (failed[i]) {
	    
	    /* find closest color in colormap, and try to alloc it */
	    mind = 1000000;   /* greater than 3 * (256^2) */
	    for (j=0,num = -1; j<dc; j++) {
	      rd = rmap[i] - (ctab[j].red  >>8);
	      gd = gmap[i] - (ctab[j].green>>8);
	      bd = bmap[i] - (ctab[j].blue >>8);
	      
	      d = CDIST(rd, gd, bd);
	      if (d<mind) { mind = d;  num = j; }
	    }
	    
	    if (num < 0) screwed = 1;
	    else if (xvAllocColor(theDisp, theCmap, &ctab[num])) {  /*success*/
	      des2got[i] = i;
	      descols[i] = ctab[num].pixel;
	      failed[i]  = 0;
	      nearCnt++; 
	      /* for (j=0; j<stdnfcols && stdfreecols[j]!=ctab[num].pixel; 
		 j++); */
	      stdfreecols[stdnfcols++] = ctab[num].pixel;
	    }
	  }
	}
      }
    }
      
    /* PHASE 3:  map remaining unallocated colors into closest we got */  
    
    for (i=0; i<maplen; i++) {
      if (failed[i]) {
	
	/* find closest alloc'd color */
	mind = 1000000;   /* greater than 3 * (256^2) */
	for (j=0,num=0; j<maplen; j++) {
	  if (!failed[j]) {
	    d = CDIST(rmap[i]-rmap[j], gmap[i]-gmap[j], bmap[i]-bmap[j]);
	    if (d<mind) { mind = d;  num = j; }
	  }
	}
	
	if (failed[num]) screwed = 1;
	else {
	  descols[i] = descols[num];
	  des2got[i] = num;
	  failed[i] = 0;
	}
      }
    }
  }


  /* at this point, we have 'descols', a maplen long array of 
     X pixel values that maps 1/1/1, 2/2/2, 6*6*6, or 3/3/2 values 
     into an X pixel value */

  /* build stdcols and stdrdisp,stdgdisp,stdbdisp colormap */
  if (desMode == STD_111) {
    for (r=0; r<8; r++)
      for (g=0; g<8; g++)
	for (b=0; b<4; b++) {
	  int i332, i111;
	  i332 = (r<<5) | (g<<2) | b;
	  i111 = (r&0x04) | ((g&0x04)>>1) | (b>>1);

	  stdrdisp[i332] = rmap[des2got[i111]];
	  stdgdisp[i332] = gmap[des2got[i111]];
	  stdbdisp[i332] = bmap[des2got[i111]];

	  stdcols[i332] = descols[des2got[i111]];
	}
  } 

  else if (desMode == STD_222) {
    for (r=0; r<8; r++)
      for (g=0; g<8; g++)
	for (b=0; b<4; b++) {
	  int i332, i222;
	  i332 = (r<<5) | (g<<2) | b;
	  i222 = ((r&0x06)<<3) | ((g&0x06)<<1) | b;

	  stdrdisp[i332] = rmap[des2got[i222]];
	  stdgdisp[i332] = gmap[des2got[i222]];
	  stdbdisp[i332] = bmap[des2got[i222]];

	  stdcols[i332] = descols[des2got[i222]];
	}
  } 

  else if (desMode == STD_232) {
    for (r=0; r<8; r++)
      for (g=0; g<8; g++)
	for (b=0; b<4; b++) {
	  int i332, i232;
	  i332 = (r<<5) | (g<<2) | b;
	  i232 = ((r&0x06)<<4) | ((g&0x07)<<2) | b;

	  stdrdisp[i332] = rmap[des2got[i232]];
	  stdgdisp[i332] = gmap[des2got[i232]];
	  stdbdisp[i332] = bmap[des2got[i232]];
	  stdcols[i332]  = descols[des2got[i232]];
	}
  } 

  else if (desMode == STD_666) {
    for (r=0,i=0; r<8; r++)
      for (g=0; g<8; g++)
	for (b=0; b<4; b++,i++) {
	  int r6,g6,b6,i666;

	  r6 = (((r*10) + 7) / 14);   /* r6 = round(r*5 / 7) */
	  g6 = (((g*10) + 7) / 14);   /* g6 = round(g*5 / 7) */
	  b6 = (((b*10) + 3) / 6);    /* b6 = round(b*5 / 3) */

	  i666 = (36 * r6) + (6 * g6) + b6;

	  stdrdisp[i] = rmap[des2got[i666]];
	  stdgdisp[i] = gmap[des2got[i666]];
	  stdbdisp[i] = bmap[des2got[i666]];

	  stdcols[i]  = descols[des2got[i666]];
	}
  } 

  else {  /* desMode == STD_332 */
    for (i=0; i<256; i++) {
      stdrdisp[i] = rmap[des2got[i]];
      stdgdisp[i] = gmap[des2got[i]];
      stdbdisp[i] = bmap[des2got[i]];

      stdcols[i] = descols[des2got[i]];
    }
  }



  if (!browPerfect) {  /* we've changed the colors the browser icons used */
    for (i=0; i<256; i++) {
      browR[i]    = stdr[i];
      browG[i]    = stdg[i];
      browB[i]    = stdb[i];
      browcols[i] = stdcols[i];
    }
  }

  haveStdCmap = desMode;

  if (DEBUG > 1) {
    fprintf(stderr,"MakeStdCmaps:  ncols=%d  maplen=%d\n", ncols, maplen);
    fprintf(stderr,"  std*[]= ");
    for (i=0; i<256; i++) 
      fprintf(stderr,"%02x,%02x,%02x  ",stdr[i],stdg[i],stdb[i]);
    fprintf(stderr,"\n\n");

    fprintf(stderr,"  disp[]= ");
    for (i=0; i<256; i++) 
      fprintf(stderr,"%02x,%02x,%02x  ",stdrdisp[i],stdgdisp[i],stdbdisp[i]);
    fprintf(stderr,"\n\n");

    fprintf(stderr,"  stdcols[]= ");
    for (i=0; i<256; i++) 
      fprintf(stderr,"%02lx ",stdcols[i]);
    fprintf(stderr,"\n\n");

    fprintf(stderr,"  stdfreecols[%d] = ", stdnfcols);
    for (i=0; i<stdnfcols; i++) 
      fprintf(stderr,"%02lx ",stdfreecols[i]);
    fprintf(stderr,"\n\n");
  }

  if (exactCnt == maplen)
    sprintf(stdCmapSuccess, "Got all %d colors.", exactCnt);
  else {
    if (nearCnt>0) 
      sprintf(stdCmapSuccess, "Got %d out of %d colors.  (%d close color%s)", 
	      exactCnt, maplen, nearCnt, (nearCnt>1) ? "s" : "");
    else
      sprintf(stdCmapSuccess, "Got %d out of %d colors.", exactCnt, maplen);
  }

  if (screwed) FatalError("something nasty happened in makeStdCmap()\n");

  if (ncols==0) return 0;   /* as no colors were actually alloc'd */
  return 1;
}


/***************************************************************/
void MakeBrowCmap()
{
  /* This function should only be called once, at the start of the program.
   *
   * produces many things:
   *   browR,browG,browB[256] 
   *                       - a 3/3/2 colormap used by genIcon
   *   browcols[256]       - maps 3/3/2 values into X colors
   *   browCmap            - local cmap used in browse window, if browPerfect
   */       

  int    i,j,r,g,b, screwed, num, exactCnt, nearCnt;
  XColor def;
  byte   rmap[256],gmap[256],bmap[256],order[256];
  u_long descols[256];
  int    des2got[256], failed[256];
  long   d, mind;


  if (DEBUG) 
    fprintf(stderr,"MakeBrowCmap:  perfect = %d, ncols = %d\n", 
	    browPerfect, ncols);

  if (ncols == 0 || !CMAPVIS(theVisual)) browPerfect = 0;

  if (!browPerfect) {  /* sharing the 'std' cmaps */
    MakeStdCmaps();
    return;
  }


  for (r=0, i=0; r<8; r++)
    for (g=0; g<8; g++)
      for (b=0; b<4; b++,i++) {
	rmap[i] = browR[i] = (r*255)/7;
	gmap[i] = browG[i] = (g*255)/7;
	bmap[i] = browB[i] = (b*255)/3;
	browcols[i] = 0;
      }


  screwed = exactCnt = nearCnt = 0;
  for (i=0; i<256; i++) des2got[i] = i;


  diverseOrder(rmap,gmap,bmap,256,order);


  browCmap = XCreateColormap(theDisp, rootW, theVisual, AllocNone);
  if (!browCmap) {
    fprintf(stderr,"Couldn't create private colormap for browser!\n");
    browPerfect = 0;
    MakeStdCmaps();
    return;
  }

  for (i=0; i<256; i++) failed[i] = 0;

  /* try to allocate the desired (rmap,gmap,bmap) colormap */
  for (i=0; i<256; i++) {
    def.red   = rmap[order[i]] << 8;
    def.green = gmap[order[i]] << 8;
    def.blue  = bmap[order[i]] << 8;
      
    def.flags = DoRed | DoGreen | DoBlue;

    if (xvAllocColor(theDisp, browCmap, &def)) {  /* success */
      des2got[order[i]] = order[i];
      descols[order[i]] = def.pixel;

      if (DEBUG>1)
	fprintf(stderr,"makebrowcmap: Phase 1: Alloc %x,%x,%x succeeded!\n", 
		rmap[order[i]], gmap[order[i]], bmap[order[i]]);
    }
    else failed[order[i]] = 1;
  }

    
  /* PHASE 2:  map remaining unallocated colors into closest we got */  

  for (i=0; i<256; i++) {
    if (failed[i]) {
      /* find closest alloc'd color */
      mind = 1000000;   /* greater than 3 * (256^2) */
      for (j=0,num=0; j<256; j++) {
	if (!failed[j]) {
	  d = CDIST(rmap[i]-rmap[j], gmap[i]-gmap[j], bmap[i]-bmap[j]);
	  if (d<mind) { mind = d;  num = j; }
	}
      }
	  
      if (DEBUG>1)
	fprintf(stderr,"makebrowcmap: closest to %x,%x,%x = %x,%x,%x\n", 
		rmap[i],gmap[i],bmap[i], rmap[num], gmap[num], bmap[num]);

      if (failed[num]) screwed = 1;
      else {
	descols[i] = descols[num];
	des2got[i] = num;
	failed[i]  = 0;
      }
    }
  }


  for (i=0; i<256; i++) {
    browcols[i] = descols[des2got[i]];
  }


  if (screwed) FatalError("something nasty happened in makeStdCmap()\n");
}


/************************************/
static void diverseOrder(rmap,gmap,bmap,maplen,order)
     byte *rmap, *gmap, *bmap, *order;
     int   maplen;
{
  /* takes a colormap (maxlen 256) and produces an order array that 
     contains the most-diverse order for allocating these colors */

  int dist[256], i, pick, maxv, ocnt, d;

  /* arbitrarily pick the brightest color first */
  pick = 0;  maxv = 0;
  for (i=0; i<maplen; i++) {
    if (CDIST((int)rmap[i], (int)gmap[i], (int)bmap[i]) > maxv) {
      maxv = CDIST((int) rmap[i], (int) gmap[i], (int)bmap[i]);
      pick = i;
    }
  }

  ocnt = 0;
  order[ocnt++] = pick;
  
  /* init dist[] array */
  for (i=0; i<maplen; i++) dist[i] = 1000000;

  while (ocnt < maplen) {
    /* update distances */
    for (i=0; i<maplen; i++) {
      d = CDIST(rmap[pick]-rmap[i], gmap[pick]-gmap[i], bmap[pick]-bmap[i]);
      if (dist[i] > d) dist[i] = d;
    }

    /* pick greatest distance */
    for (i=0, maxv=0, pick=0;  i<maplen; i++) {
      if (dist[i] > maxv) { maxv = dist[i];  pick = i; }
    }

    order[ocnt++] = pick;
  }
}


/***************************************************************/
static void freeStdCmaps()
{
  int i;

  if (DEBUG) fprintf(stderr,"freeStdCmaps:  haveStdCmap = %d\n", haveStdCmap);

  if (haveStdCmap == STD_NONE) return;

  for (i=0; i<stdnfcols; i++)
    xvFreeColors(theDisp, theCmap, &stdfreecols[i], 1, 0L);
  stdnfcols = 0;
  haveStdCmap = STD_NONE;
}


/***************************************************************/
void ChangeCmapMode(cmode, genepic, freeKludge)
     int cmode, genepic, freeKludge;
{
  /* note:  MAY BE CALLED before there is an image or anything */

  /* called whenever colormap allocation methods change (by selecting a
     colormap mode from the dispMB button, or by going into/outof
     a root display mode when the cmapmode is 'CM_PERFECT' or 'CM_OWNCMAP'

     Also called whenever a new pic is loaded and color realloc may need
     to be done.

     if !genepic, just do the color alloc/realloc.  Don't generate or
     draw epic.

     As a general rule, frees, and reallocates colors using new strategy

     if cmode==CM_STDCMAP, frees 2/2/2 colormap that the icons may be
        using (if it exists), and allocs a bigger stdcmap (6x6x6 or 3/3/2)

     if cmode==CM_NORMAL, CM_PERFECT, or CM_OWNCMAP, frees all regular
        allocated colors (doesn't touch stdcmap), and reallocates using
	new method
   */

  int i, iconCmapSize, oldmode;


  if (useroot && (cmode==CM_PERFECT || cmode==CM_OWNCMAP)) cmode=CM_NORMAL;

  /* free all normal allocated colors, if any */
  if ((pic && freeKludge==1 && noFreeCols==0) ||
      (pic && freeKludge==0)) FreeColors();

  oldmode = colorMapMode;
  colorMapMode = cmode;

  iconCmapSize = STD_222;
  if (ncols > 0 && ncols < 64) iconCmapSize = STD_111;

  if (cmode == CM_STDCMAP) {
    if (MakeStdCmaps() && anyBrowUp && !browPerfect && CMAPVIS(theVisual))
      RegenBrowseIcons();   /* redraw icons */
  }


  else if (cmode == CM_NORMAL) {
    if (novbrowse || browPerfect || haveStdCmap != iconCmapSize)
      freeStdCmaps();
    
    /* if using browser, and killed stdcmap, make icon stdcmap */
    if (!novbrowse && !browPerfect && haveStdCmap == STD_NONE) {
      if (MakeStdCmaps() && anyBrowUp && CMAPVIS(theVisual))
	RegenBrowseIcons();
    }
  }
  
  else if (cmode == CM_PERFECT) { }
  else if (cmode == CM_OWNCMAP) { }

  /* disable rwcolor if STDCMAP or non-colormapped display mode */
  dispMB.dim[DMB_COLRW] = (cmode==CM_STDCMAP || !CMAPVIS(theVisual));

  allocMode = (!dispMB.dim[DMB_COLRW] && dispMB.flags[DMB_COLRW]) ?
    AM_READWRITE : AM_READONLY;


  /* move checkmark to current selection */
  for (i=DMB_COLNORM; i<=DMB_COLSTDC; i++) dispMB.flags[i] = 0;
  dispMB.flags[ cmode + (DMB_COLNORM - CM_NORMAL)] = 1;

  if (!pic) return;   /* no pic, so don't alloc any colors or anything */

  AllocColors();

  if (cmode == CM_STDCMAP && epicMode == EM_RAW) {  /* turn on dithering */
    epicMode = EM_DITH;
    SetEpicMode();
    if (genepic) GenerateEpic(eWIDE, eHIGH);
  }
  else { 
    if (oldmode == CM_STDCMAP && cmode != CM_STDCMAP && epicMode != EM_RAW) {
      /* just left STDCMAP mode.  Switch to using 'RAW' */
      epicMode = EM_RAW;
      SetEpicMode();
      if (genepic) GenerateEpic(eWIDE, eHIGH);
    }
  }

  if (genepic) DrawEpic();
  SetCursors(-1);
}


