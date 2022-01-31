/*
 * xvpcd.c - load routine for 'PhotoCD' format pictures
 *
 * LoadPCD(fname, pinfo, size)  -  loads a PhotoCD file
 *
 * This routine will popup a choice of which of the 5 available resolutions
 * the user wants to choose, then load it as a 24 bit image.
 *
 * Copyright 1993 David Clunie, Melbourne, Australia.
 *
 * The outline of this is shamelessly derived from xvpbm.c to read the
 * file, and xvtiffwr.c to handle the popup window and X stuff (X never
 * has been my forte !), and the PhotoCD format information (though not
 * the code) was found in Hadmut Danisch's (danisch@ira.uka.de) hpcdtoppm
 * program in which he has reverse engineered the format by studying
 * hex dumps of PhotoCDs ! After all who can afford the Kodak developer's
 * kit, which none of us have seen yet ? Am I even allowed to mention these
 * words (Kodak, PhotoCD) ? I presume they are registered trade marks.
 *
 * PS. I have no idea how Halmut worked out the YCC <-> RGB conversion
 * factors, but I have calculated them from his tables and the results
 * look good enough to me.
 *
 * Added size parameter to allow the schnautzer to create thumnails
 * without requesting the size every time.
 */

#include "xv.h"

#ifdef HAVE_PCD

#include <memory.h>
#ifndef alloca
#  include <alloca.h> /* "not in POSIX or SUSv3" according to Linux man page */
#endif                /* ...but required for Sun C compiler (alloca = macro) */

#define  TRACE  0
#if TRACE
#  define trace(x) fprintf x
#else
#  define trace(x)
#endif

/* Comments on error-handling:
   A truncated file is not considered a Major Error.  The file is loaded,
   and the rest of the pic is filled with 0's.

   Not being able to malloc is a Fatal Error.  The program is aborted. */


#ifdef __STDC__
static void magnify(int, int, int, int, int, byte *);
static int pcdError(const char *, const char *);
static int gethuffdata(byte *, byte *, byte *, int, int);
#else
static void magnify();
static int pcdError();
static int gethuffdata();
#endif

#define wcurfactor 16  /* Call WaitCursor() every n rows */

static int  size;    /* Set by window routines */
static int  leaveitup;/* Cleared by docmd() when OK or CANCEL pressed */
static int  goforit;  /* Set to 1 if OK or 0 if CANCEL */
static FILE  *fp;
static CBUTT  lutCB;

/*
 * This "beyond 100%" table is taken from ImageMagick (gamma 2.2).
 * Why there are 351 entries and not 346 as per Kodak documentation
 * is a mystery.
 */
static  double  rscale = 1.00,
    gscale = 1.00,
    bscale = 1.00;

static  byte  Y[351] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
   20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
   30,  32,  33,  34,  35,  36,  37,  38,  39,  40,
   41,  42,  43,  45,  46,  47,  48,  49,  50,  51,
   52,  53,  54,  56,  57,  58,  59,  60,  61,  62,
   63,  64,  66,  67,  68,  69,  70,  71,  72,  73,
   74,  76,  77,  78,  79,  80,  81,  82,  83,  84,
   86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
   97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
  107, 108, 110, 111, 112, 113, 114, 115, 116, 117,
  118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
  129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
  139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
  149, 150, 151, 152, 153, 154, 155, 156, 157, 158,
  159, 160, 161, 162, 163, 164, 165, 166, 167, 168,
  169, 170, 171, 172, 173, 174, 175, 176, 176, 177,
  178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
  188, 189, 190, 191, 192, 193, 193, 194, 195, 196,
  197, 198, 199, 200, 201, 201, 202, 203, 204, 205,
  206, 207, 207, 208, 209, 210, 211, 211, 212, 213,
  214, 215, 215, 216, 217, 218, 218, 219, 220, 221,
  221, 222, 223, 224, 224, 225, 226, 226, 227, 228,
  228, 229, 230, 230, 231, 232, 232, 233, 234, 234,
  235, 236, 236, 237, 237, 238, 238, 239, 240, 240,
  241, 241, 242, 242, 243, 243, 244, 244, 245, 245,
  245, 246, 246, 247, 247, 247, 248, 248, 248, 249,
  249, 249, 249, 250, 250, 250, 250, 251, 251, 251,
  251, 251, 252, 252, 252, 252, 252, 253, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255
};

/*******************************************/
/* The size should be -1 for the popup to ask otherwise fast is assumed */
/* returns '1' on success */
/*******************************************/
int
LoadPCD(char *fname, PICINFO *pinfo, int theSize)
{
  long   offset;
  int    mag;
  int   rotate;
  byte   header[3*0x800];
  byte  *pic24, *luma, *chroma1, *chroma2, *ptr, *lptr, *c1ptr, *c2ptr;
  int   w, h, npixels, bufsize;
  int   row, col;
  int  huffplanes;
  const char  *bname;

  bname    = BaseName(fname);
  pinfo->pic  = NULL;
  pinfo->comment  = NULL;


  /*
   *  open the file
   */
  if((fp=fopen(fname,"r")) == NULL)
    return pcdError(bname, "can't open file");

  /*
   * inspect the header
   */
  if(fread(&header[0], 1, sizeof(header), fp) != sizeof(header))
    return pcdError(bname, "could not load PCD header");
  if(strncmp((char *)&header[0x800], "PCD_", 4) != 0)
    return pcdError(bname, "not a PCD file");
  rotate = header[0x0E02] & 0x03;

/* base/16
  - plain data starts at sector 1+2+1=4
    (numbered from 0, ie. the 5th sector)
  - luma 192*128 = 24576 bytes (12 sectors)
    + chroma1 96*64 = 6144 bytes (3 sectors)
    + chroma2 96*64 = 6144 bytes (3 sectors)
    = total 18 sectors

  - NB. "Plain" data is interleaved - 2 luma rows 192 wide,
    then 1 of each of the chroma rows 96 wide !

   base/4
  - plain data starts at sector 1+2+1+18+1=23
  - luma 384*256 = 98304 bytes (48 sectors)
    + chroma1 192*128 = 24576 bytes (12 sectors)
    + chroma2 192*128 = 24576 bytes (12 sectors)
    = total 72 sectors

  - NB. "Plain" data is interleaved - 2 luma rows 384 wide,
    then 1 of each of the chroma rows 192 wide !

   base
  - plain data starts at sector 1+2+1+18+1+72+1=96

  - luma 768*512 = 393216 bytes (192 sectors)
    + chroma1 384*256 = 98304 bytes (48 sectors)
    + chroma2 384*256 = 98304 bytes (48 sectors)
    = total 288 sectors

  - NB. "Plain" data is interleaved - 2 luma rows 768 wide,
    then 1 of each of the chroma rows 384 wide !

   4base
  - plain data for base is read
  - luma data interpolated *2
  - chroma data interpolated *4

  - cd_offset is 1+2+1+18+1+72+1+288=384
  - at cd_offset+4 (388) is huffman table
  - at cd_offset+5 (389) is 4base luma plane

  (the sector at cd_offset+3 seems to contain 256 words each of
  which is an offset presumably to the sector containing certain
  rows ? rows/4 given 1024 possible rows. The rest of this sector
  is filled with zeroes)


   16base
  - plain data for base is read
  - luma data interpolated *2
  - chroma data interpolated *4

  - cd_offset is 1+2+1+18+1+72+1+288=384
  - at cd_offset+4 (388) is huffman table for 4 base
  - at cd_offset+5 (389) is 4base luma plane
  - luma plane interpolated *2

  - cd_offset is set to current position (should be start of sector)
  - at cd_offset+12 is huffman table for 16 base
  - at cd_offset+14 is 16 base luma & 2 chroma planes which are read
          (note that the luma plane comes first, with a sync pattern
           announcing each row from 0 to 2047, then the two chroma planes
           are interleaved by row, the row # being even from 0 to 2046, with
           each row containing 1536 values, the chroma1 row coming first,
           finally followed by a sync pattern with a row of 2048 announcing
           the end (its plane seems to be set to 3, ie. chroma2)
  - chroma planes interpolated *2

  (the sector at cd_offset+10 & 11 seem to contain 1024 pairs of words
        the first for luma and the second for chroma, each of
  which is an offset presumably to the sector containing certain
  rows ? rows/2 given 2048 possible rows)

Not yet implemented:

In order to do overskip for base and 4base, one has to reach the chroma
data for 16 base:

  - for 4base, after reading the 4base luma plane (and presumably
    skipping the chroma planes) one sets cd_offset to the start of
    the "current" sector

  - for base, one has to skip the 4base data first:
  - cd_offset is set to 384
  - at (cd_offset+3 sectors)[510] is a 16 bit word high byte 1st
    containing an offset to the beginning of the 16base stuff
    though there is then a loop until >30 0xff's start a sector !

  - being now positioned after the end of the 4base stuff,
  - at (cd_offset+10 sectors)[2] is a 16 bit word high byte 1st
    containing an offset to the chroma planes.
  - at cd_offset+12 is the set of huffman tables

  - for base, the 16base chroma planes are then halved
*/

  PCDSetParamOptions(bname);
  if (theSize == -1)
  {
    PCDDialog(1);                   /* Open PCD Dialog box */
    SetCursors(-1);                 /* Somebody has already set it to wait :( */
    leaveitup=1;
    goforit=0;
  size = 1;
    /* block until the popup window gets closed */
    while (leaveitup) {
      int i;
      XEvent event;
      XNextEvent(theDisp, &event);
      HandleEvent(&event, &i);
    }
    /* At this point goforit and size will have been set */
    if (!goforit) {
      /* nothing allocated so nothing needs freeing */
      return 0;
    }
    WaitCursor();
  }
  else
  {
    size = theSize;
    goforit = 1;
  }

  if(lutCB.val)
    rscale = gscale = bscale = 255.0/346.0;
  else
    rscale = gscale = bscale = 1.0;

  switch (size) {
  case 0:
    pinfo->w = 192;
    pinfo->h = 128;
    offset=4*0x800;
    mag=1;
    huffplanes=0;
    sprintf(pinfo->fullInfo, "PhotoCD, base/16 resolution");
    break;

  case 1:
    pinfo->w = 384;
    pinfo->h = 256;
    offset=23*0x800;
    mag=1;
    huffplanes=0;
    sprintf(pinfo->fullInfo, "PhotoCD, base/4 resolution");
    break;

  case 2:
  default:
    pinfo->w = 768;
    pinfo->h = 512;
    offset=96*0x800;
    mag=1;
    huffplanes=0;
    sprintf(pinfo->fullInfo, "PhotoCD, base resolution");
    break;

  case 3:
    pinfo->w = 1536;
    pinfo->h = 1024;
    offset=96*0x800;
    mag=2;
    huffplanes=1;
    sprintf(pinfo->fullInfo, "PhotoCD, 4base resolution");
    break;

  case 4:
    pinfo->w=3072;
    pinfo->h=2048;
    offset=96*0x800;
    mag=4;
    huffplanes=2;
    sprintf(pinfo->fullInfo, "PhotoCD, 16base resolution");
    break;
  }

  /*
   * rotate?
   */
  w = pinfo->w;
  h = pinfo->h;
  switch(rotate) {
  case  0:
    break;

  case  1:
  case  3:
    pinfo->w = h;
    pinfo->h = w;
    break;

  default:
    fprintf(stderr, "unknown image rotate %d; assuming none\n",
      rotate);
    rotate = 0;
  }

  /*
   * allocate 24-bit image
   */
  npixels = pinfo->w * pinfo->h;
  bufsize = 3 * npixels;
  if (pinfo->w <= 0 || pinfo->h <= 0 || npixels/pinfo->w != pinfo->h ||
      bufsize/3 != npixels)
    FatalError("image dimensions out of range");

  pinfo->pic = (byte *)malloc((size_t) bufsize);
  if(!pinfo->pic)
    FatalError("couldn't malloc '24-bit RGB plane'");

  pinfo->type = PIC24;
  sprintf(pinfo->shrtInfo, "%dx%d PhotoCD.", pinfo->w, pinfo->h);
  pinfo->colType = F_FULLCOLOR;
  pinfo->frmType = -1;

  if(fseek(fp, offset, SEEK_SET) == -1) {
    free(pinfo->pic);
    return pcdError(bname,"Can't find start of data.");
  }

  pic24 = pinfo->pic;

  luma=(byte *)calloc(npixels,1);
  if(!luma) {
    free(pinfo->pic);
    FatalError("couldn't malloc 'luma plane'");
  }

  chroma1=(byte *)calloc(npixels/4,1);
  if(!chroma1) {
    free(pinfo->pic);
    free(luma);
    FatalError("couldn't malloc 'chroma1 plane'");
  }

  chroma2=(byte *)calloc(npixels/4,1);
  if(!chroma2) {
    free(pinfo->pic);
    free(luma);
    free(chroma1);
    FatalError("couldn't malloc 'chroma2 plane'");
  }

  /* Read 2 luma rows length w, then one of each chroma rows w/2 */
  /* If a mag factor is active, the small image is read into the */
  /* top right hand corner of the larger allocated image */

  trace((stderr, "base image: start @ 0x%08lx (sector %ld.%ld)\n",
        ftell(fp), ftell(fp)/0x800, ftell(fp) % 0x800));
  for(row=0,lptr=luma,c1ptr=chroma1,c2ptr=chroma2; row <h/mag;
        row+=2,lptr+=w*2,c1ptr+=w/2,c2ptr+=w/2) {
    if(fread(lptr, 1, w/mag, fp) != w/mag) {
      pcdError(bname, "Luma plane too short.");
      break;
    }
    if(fread(lptr+w, 1, w/mag, fp) != w/mag) {
      pcdError(bname, "Luma plane too short.");
      break;
    }
    if(fread(c1ptr, 1, w/2/mag, fp) != w/2/mag) {
      pcdError(bname, "Chroma1 plane too short.");
      break;
    }
    if(fread(c2ptr, 1, w/2/mag, fp) != w/2/mag) {
      pcdError(bname, "Chroma2 plane too short.");
      break;
    }
    if(row%wcurfactor == 0)
      WaitCursor();
  }
  trace((stderr, "base image: done @ 0x%08lx (sector %ld.%ld)\n",
        ftell(fp), ftell(fp)/0x800, ftell(fp) % 0x800));

  if(huffplanes) {
    if(fseek(fp, 388*0x800, SEEK_SET) == -1)
      return pcdError(bname,
          "Can't find start of huffman tables.");

    magnify(2, h/mag, w/mag, h, w, luma);
    magnify(2, h/2/mag, w/2/mag, h/2, w/2, chroma1);
    magnify(2, h/2/mag, w/2/mag, h/2, w/2, chroma2);

    /*
     * doesn't really touch the chroma planes which aren't
     * present in 4base
     */
    gethuffdata(luma, chroma1, chroma2, w, h/mag*2);

    /*
     * if only doing 4base should probably fetch 16bases
     * chroma planes here
     */
    if(huffplanes == 2) {
      /*
       * This depends on gethuffdata() having grabbed
       * things in 0x800 sectors AND still being
       * positioned in the "last" sector of the data
       * (cf. Hadmut's code which is positioned at start
       * of the next sector)
       */
      long  offset = ftell(fp)/0x800+12;

      if(fseek(fp, offset*0x800, SEEK_SET) == 0) {
        magnify(2,h/2,w/2,h,w,luma);
        magnify(2,h/4,w/4,h/2,w/2,chroma1);
        magnify(2,h/4,w/4,h/2,w/2,chroma2);
        gethuffdata(luma,chroma1,chroma2,w,h);
      } else
        fprintf(stderr, "can't seek to 2nd huffman tables\n");
    }
  }
  fclose(fp);

  /*
   * YCC -> R'G'B' and image rotate
   */
  ptr=pic24;
  lptr=luma; c1ptr=chroma1; c2ptr=chroma2;
  for(row = 0; row < h; ++row) {
    byte  *rowc1ptr = c1ptr,
      *rowc2ptr = c2ptr;
    int  k = 0;

    switch(rotate) {
    case  1:
      ptr = &pic24[row*3 + (w - 1)*h*3];
        k = -3*(h + 1);
      break;

    case  3:
      ptr = &pic24[(h - 1 - row)*3];
        k =  3*(h - 1);
      break;

    default:
      ptr = &pic24[row*w*3];
        k = 0;
      break;
    }
    for(col = 0; col < w; ++col) {
      double  L  = 1.3584*(double) *lptr++,
        C1  = 2.2179*(double) (*c1ptr - 156),
        C2  = 1.8215*(double) (*c2ptr - 137);
      int  r  = rscale*(L + C2),
        g  = gscale*(L - 0.194*C1 - 0.509*C2),
        b  = bscale*(L + C1);

      if(lutCB.val) {
        if(r < 0) r = 0; else if(r >= 255) r = 255;
        if(g < 0) g = 0; else if(g >= 255) g = 255;
        if(b < 0) b = 0; else if(b >= 255) b = 255;
      } else {
        if(r < 0) r = 0; else if(r >= 351) r = 350;
        if(g < 0) g = 0; else if(g >= 351) g = 350;
        if(b < 0) b = 0; else if(b >= 351) b = 350;
        r = Y[r]; g = Y[g]; b = Y[b];
      }
      *ptr++ = r;
      *ptr++ = g;
      *ptr++ = b;
      ptr   += k;
      if(col & 1) {
        ++c1ptr;
        ++c2ptr;
      }
    }
    if((row & 1) == 0) {
      c1ptr = rowc1ptr;
      c2ptr = rowc2ptr;
    }
    if(row%wcurfactor == 0)
      WaitCursor();
  }
  free(luma); free(chroma1); free(chroma2);
  return 1;
}

/*
 * derived from Hadmut Danisch's interpolate()
 */
static void
magnify(int mag,  /* power of 2 by which to magnify in place */
  int h, int w,  /* the "start" unmag'd dimensions of the array */
  int mh, int mw,  /* the real (maximum) dimensions of the array */
  byte *p)  /* pointer to the data */
{
  int x,y,yi;
  byte *optr,*nptr,*uptr;  /* MUST be unsigned, else averaging fails */

  while (mag > 1) {

    /* create every 2nd new row from 0 */
    /*  even pixels being equal to the old, odd ones averaged with successor */
    /*  special case being the last column which is just set equal to the */
    /*  second last) ... */

    for(y=0;y<h;y++) {
      yi=h-1-y;
      optr=p+  yi*mw + (w-1);            /* last pixel of an old row */
      nptr=p+2*yi*mw + (2*w - 2);         /* last pixel of a new row */

      nptr[0]=nptr[1]=optr[0];            /* special cases */

      for(x=1;x<w;x++) {
        optr--; nptr-=2;                  /* next lower pixel(s) */
        nptr[0]=optr[0];                  /* even pixels duped */
        nptr[1]=(((int)optr[0])+
                 ((int)optr[1])+1)>>1;    /* odd averaged */
      }
    }

    /* Fill in odd rows, as average of prior & succeeding rows, with */
    /* even pixels average of one column, odd pixels average of two */

    for(y=0;y<h-1;y++) {                  /* all but the last old row */
      optr=p + 2*y*mw;                    /* start of the new "even" rows */
      nptr=optr+mw;                       /* start of the next empty row */
      uptr=nptr+mw;                       /* start of the next again (even) */

      for(x=0;x<w-1;x++) {                /* for all cols except the last */
        nptr[0]=(((int)optr[0])+
                 ((int)uptr[0])+1)>>1;    /* even pixels */
        nptr[1]=(((int)optr[0])+
                 ((int)optr[2])+
                 ((int)uptr[0])+
                 ((int)uptr[2])+2)>>2;    /* odd pixels */
        nptr+=2; optr+=2; uptr+=2;
      }
      *(nptr++)=(((int)*(optr++))+
                 ((int)*(uptr++))+1)>>1;  /* 2nd last pixel */
      *(nptr++)=(((int)*(optr++))+
                 ((int)*(uptr++))+1)>>1;  /* last pixel */
    }

    xvbcopy((char *)(p + (2*h-2)*mw),     /* 2nd last row */
            (char *)(p + (2*h-1)*mw),     /* the last row */
            2*w);                         /* length of a new row */

    h*=2; w*=2;
    mag>>=1;  /* Obviously mag must be a power of 2 ! */
  }
}

/*******************************************/
static int
pcdError(const char *fname, const char *st)
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  return 0;
}


/**** Stuff for PCDDialog box ****/

#define TWIDE 380
#define THIGH 160
#define T_NBUTTS 2
#define T_BOK    0
#define T_BCANC  1
#define BUTTH    24

static void drawTD    PARM((int, int, int, int));
static void clickTD   PARM((int, int));
static void doCmd     PARM((int));
static void PCDSetParams PARM((void));

/* local variables */
static BUTT  tbut[T_NBUTTS];
static RBUTT *resnRB;



/***************************************************/
void CreatePCDW()
{
  int       y;

  pcdW = CreateWindow("xv pcd", "XVpcd", NULL,
           TWIDE, THIGH, infofg, infobg, 0);
  if (!pcdW) FatalError("can't create pcd window!");

  XSelectInput(theDisp, pcdW, ExposureMask | ButtonPressMask | KeyPressMask);

  BTCreate(&tbut[T_BOK], pcdW, TWIDE-140-1, THIGH-10-BUTTH-1, 60, BUTTH,
     "Ok", infofg, infobg, hicol, locol);

  BTCreate(&tbut[T_BCANC], pcdW, TWIDE-70-1, THIGH-10-BUTTH-1, 60, BUTTH,
     "Cancel", infofg, infobg, hicol, locol);

  y = 55;
  resnRB = RBCreate(NULL, pcdW, 36, y,   "192*128   Base/16",
           infofg, infobg,hicol,locol);
  RBCreate(resnRB, pcdW, 36, y+18,       "384*256   Base/4",
           infofg, infobg,hicol,locol);
  RBCreate(resnRB, pcdW, 36, y+36,       "768*512   Base",
           infofg, infobg, hicol, locol);
  RBCreate(resnRB, pcdW, TWIDE/2, y,     "1536*1024 4Base",
           infofg, infobg, hicol, locol);
  RBCreate(resnRB, pcdW, TWIDE/2, y+18,  "3072*2048 16Base",
           infofg, infobg, hicol, locol);

  CBCreate(&lutCB, pcdW, TWIDE/2, y+36,  "Linear LUT",
           infofg, infobg, hicol, locol);

  RBSelect(resnRB, 2);

  XMapSubwindows(theDisp, pcdW);
}


/***************************************************/
void PCDDialog(vis)
int vis;
{
  if (vis) {
    CenterMapWindow(pcdW, tbut[T_BOK].x + tbut[T_BOK].w/2,
        tbut[T_BOK].y + tbut[T_BOK].h/2, TWIDE, THIGH);
  }
  else     XUnmapWindow(theDisp, pcdW);
  pcdUp = vis;
}


/***************************************************/
int PCDCheckEvent(xev)
XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!pcdUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    if (e->window == pcdW)       drawTD(x, y, w, h);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == pcdW)     clickTD(x,y);
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;  XComposeStatus status;
    int stlen;

    stlen = XLookupString(e,buf,128,&ks,&status);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    if (e->window == pcdW) {
      if (stlen) {
  if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
    FakeButtonPress(&tbut[T_BOK]);
  }
  else if (buf[0] == '\033') {            /* ESC */
    FakeButtonPress(&tbut[T_BCANC]);
  }
      }
    }
    else rv = 0;
  }
  else rv = 0;

  if (rv==0 && (xev->type == ButtonPress || xev->type == KeyPress)) {
    XBell(theDisp, 50);
    rv = 1;   /* eat it */
  }

  return rv;
}


/***************************************************/
void
PCDSetParamOptions(const char *fname)
{
  int cur;
  cur = RBWhich(resnRB);

  RBSetActive(resnRB,0,1);
  RBSetActive(resnRB,1,1);
  RBSetActive(resnRB,2,1);
  RBSetActive(resnRB,3,1);
  RBSetActive(resnRB,4,1);
  CBSetActive(&lutCB,1);
}


/***************************************************/
static void
drawTD(int x, int y, int w, int h)
{
  const char *title = "Load PhotoCD file...";
  int         i;
  XRectangle  xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<T_NBUTTS; i++) BTRedraw(&tbut[i]);

  ULineString(pcdW, resnRB->x-16, resnRB->y-10-DESCENT, "Resolution");
  RBRedraw(resnRB, -1);
  CBRedraw(&lutCB);

  XDrawString(theDisp, pcdW, theGC, 20, 19, title, strlen(title));

  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void clickTD(x,y)
int x,y;
{
  int i;
  BUTT *bp;

  /* check BUTTs */

  /* check the RBUTTS first, since they don't DO anything */
  if ( (i=RBClick(resnRB, x,y)) >= 0) {
    (void) RBTrack(resnRB, i);
    return;
  }

  if(CBClick(&lutCB, x, y)) {
    (void) CBTrack(&lutCB);
    return;
  }

  for (i=0; i<T_NBUTTS; i++) {
    bp = &tbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<T_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }
}



/***************************************************/
static void doCmd(cmd)
int cmd;
{
  leaveitup=0;
  goforit=0;
  switch (cmd) {
  case T_BOK:    PCDSetParams();
                goforit=1;
  case T_BCANC:  PCDDialog(0);
                break;

  default:  break;
  }
}


/*******************************************/
static void PCDSetParams()
{
  switch (RBWhich(resnRB)) {
  case 0: size = 0;      break;
  case 1: size = 1;      break;
  case 2: size = 2;      break;
  case 3: size = 3;      break;
  case 4: size = 4;      break;
  case 5: size = 0;      break;
  default: size = 0;     break;
  }
}

/*
 * Read the Huffman tables which consist of an unsigned byte # of entries
 * (less 1) followed by up to 256 entries, each of which is a series of 4
 * unsigned bytes - length, highseq, lowseq, and key.
 *
 * Store the huffman table into tree type structure:
 *
 *   int int[n of entries*2]
 *
 * Each entry consists of two words (the 1st for zero and the 2nd for one).
 *
 * If the word is negative, then subtract it from the current pointer to
 * get the next entry (ie. it is the negative offset from the current
 * position*2 in order to skip entries not words) with which to
 * make a decision.
 *
 * If the word is not negative, then the low 8 bits contain the value (which
 * is supposed to be a signed char) and the rest of the word is zero.
 */
static void
dumphufftab(int n, const byte *h, int m, const int *t)
{
  int  j;

  for(j = 0; j < n || j < m; ++j) {
    if(j < m)
      fprintf(stderr, "%04x %04x ::", 0xffff & t[2*j + 0],
               0xffff & t[2*j + 1]);
    else
      fprintf(stderr, "%s %s ::", "    ", "    ");
    if(j < n) {
      int   k;
      unsigned l = (h[4*j + 1] << 8) | h[4*j + 2];

      fprintf(stderr, " %02x %2d ", h[4*j + 3], h[4*j + 0]);
      for(k = 0; k <= h[4*j + 0]; ++k, l *= 2)
        fprintf(stderr, "%c", '0'+((l & 0x8000) != 0));
    }
    fprintf(stderr, "\n");
  }
}

static int *
gethufftable(void)
{
  int  *hufftab, *h, i, j, N, num, bufsize, huffptr, hufftop;
  byte  *huf;

  /*
   * absorb the entirety of the table in one chunk (for better
   * dumps in case of error)
   */
  trace((stderr, "hufftab 0x%08lx ", ftell(fp)));
  num = 1 + fgetc(fp);   /* 256 max */
  huf = (byte *)alloca(4*num*sizeof(byte));
  if((i = fread(huf, 1, 4*num, fp)) != 4*num) {
    fprintf(stderr, "unexpected EOF: got %d bytes, wanted %d\n",
                i, 4*num);
    return NULL;
  }

  /*
   * guess an initial size and prepare the initial entry
   */
  trace((stderr, "length %u\n", num));
  N = 2*num;   /* 512 max */
  bufsize = N * sizeof(int);
/*  this case can't happen, but added for symmetry with loop below
  if (N/2 != num || bufsize/N != sizeof(int)) {
    SetISTR(ISTR_WARNING, "Huffman table size out of range");
    return NULL;
  }
 */
  if((hufftab = (int *)malloc(bufsize)) == NULL)
    FatalError("couldn't malloc initial Huffman table");
  hufftab[0] = hufftab[1] = 0;

  /*
   * we check the table for reasonableness;  there is a lack of detailed
   * documentation on this format.  in particular, for the base16,
   * the position of the huffman tables is uncertain to within one
   * "sector", and we have to detect his before trying to read
   * bogusness.
   */
  hufftop = 0;
  for(i = 0; i < num; ++i) {
    unsigned length   =  huf[4*i + 0],
       codeword = (huf[4*i + 1] << 8) | huf[4*i + 2];

    /*
     * some sanity checks
     */
    if(length >= 16) {
      fprintf(stderr,
        "gethufftable: improbable length @ %d/%d\n",
          i, num);
      dumphufftab(num, huf, hufftop/2, hufftab);
      free(hufftab);
      return NULL;
    }

    /*
     * walk the whole set of codes
     */
    huffptr = 0;
    for(j = 0; j < 16; ++j, codeword *= 2) {
      /*
       * choose the child node
       */
      if(codeword & 0x8000)
        ++huffptr;

      /*
       * store value at end-of-code
       */
      if(j == length) {
        /*
         * more sanity
         */
        if((codeword *= 2) & 0xffff) {
          fprintf(stderr,
            "gethufftable: "
            ":probable invalid code @ %d\n",
              i);
          dumphufftab(num, huf,
              hufftop/2, hufftab);
          free(hufftab);
          return NULL;
        }
        hufftab[huffptr] = 1 + (int) huf[4*i + 3];
        break;
      }

      /*
       * otherwise, follow the tree to date
       */
      if(hufftab[huffptr] < 0) {
        huffptr -= hufftab[huffptr];
        continue;
      } else if(hufftab[huffptr] > 0) {
        fprintf(stderr, "duplicate code %d %d/%d\n",
          huffptr, i, num);
        dumphufftab(num, huf, hufftop/2, hufftab);
        free(hufftab);
        return NULL;
      }

      /*
       * and if necessary, make the tree bigger
       */
      if((hufftop += 2) >= N) {
        int oldN = N;
#if TRACE
        dumphufftab(num, huf, hufftop/2, hufftab);
#endif
        N *= 2;
        bufsize = N*sizeof(int);
        if (N/2 != oldN || bufsize/N != sizeof(int)) {
          SetISTR(ISTR_WARNING,
            "new Huffman table is too large");
          free(hufftab);
          return NULL;
        }
        h = (int *)realloc(hufftab, bufsize);
        if(h == NULL) {
          fprintf(stderr,
            "Table overflow %d/%d\n",
                 i, num);
          dumphufftab(num, huf,
              hufftop/2, hufftab);
          free(hufftab);
          FatalError(
            "couldn't realloc Huffman table");
        }
        hufftab = h;
      }

      /*
       * then add new ptr
       */
      hufftab[huffptr] = huffptr - hufftop;
      huffptr = hufftop;
      hufftab[huffptr + 0] =
      hufftab[huffptr + 1] = 0;
    }
  }
  return hufftab;
}

/* WORDTYPE & char buffer must be unsigned else */
/* fills with sign bit not 0 on right shifts */
typedef unsigned int WORDTYPE;
typedef int SWORDTYPE;
#define WORDSIZE sizeof(WORDTYPE)
#define NBYTESINBUF 0x800

static byte buffer[NBYTESINBUF];
static int bitsleft=0;
static int bytesleft=0;
static byte *bufptr;
static WORDTYPE word;

#if 0
static void
dumpbuffer(void)
{
  int i,left;
  byte *ptr=buffer;

  fprintf(stderr,"dumpbuffer: bytesleft=%d bitsleft= %d word=0x%08lx\n",
    bytesleft,bitsleft,(unsigned long)word);
  for (left=NBYTESINBUF; left>0; left-=16) {
    fprintf(stderr,"%05d  ",left);
    for (i=0; i<8; i++) {
      fprintf(stderr,"%02x",*ptr++);
      fprintf(stderr,"%02x ",*ptr++);
    }
    fprintf(stderr,"\n");
  }
}
#endif /* 0 */

static void
loadbuffer(void)
{
  if ((bytesleft=fread(buffer,1,NBYTESINBUF,fp)) == 0) {
    fprintf(stderr,"Truncation error\n");
    exit(1);
  }
  bufptr=buffer;
  /* dumpbuffer(); */
}

static void
loadbyte(void)
{
  if (bytesleft <= 0) loadbuffer();
  --bytesleft;
  word|=(WORDTYPE)(*bufptr++)<<(sizeof(WORDTYPE)*8-8-bitsleft);
  bitsleft+=8;
}

static int
getbit(void)
{
  int bit;

  while (bitsleft <= 0) loadbyte();
  --bitsleft;
  bit=(SWORDTYPE)(word)<0;  /* assumes word is signed */
  /* bit=word>>(sizeof(WORDTYPE)*8-1); */
  word<<=1;
  return bit;
}

static WORDTYPE
getnn(int nn)
{
  WORDTYPE value;

  while (bitsleft <= nn) loadbyte();
  bitsleft-=nn;
  value=word>>(sizeof(WORDTYPE)*8-nn);
  word<<=nn;
  return value;
}

static WORDTYPE
isnn(int nn)
{
  WORDTYPE value;

  while (bitsleft <= nn) loadbyte();
  value=word>>(sizeof(WORDTYPE)*8-nn);
  return value;
}

static void
skipnn(int nn)
{
  while (bitsleft <= nn) loadbyte();
  bitsleft-=nn;
  word<<=nn;
}

#define get1()    (getbit())
#define get2()    (getnn(2))
#define get8()    (getnn(8))
#define get13()    (getnn(13))
#define get16()    (getnn(16))
#define get24()    (getnn(24))

#define is24()    (isnn(24))

#define skip1()    (skipnn(1))
#define skip24()  (skipnn(24))

static int
gethuffdata(  byte *luma,
    byte *chroma1,
    byte *chroma2,
    int realrowwidth,
    int maxrownumber)
{
static  byte  clip[3*256];
  int  *hufftable[3], *huffstart = NULL, *huffptr = NULL;
  int  row, col, plane, i, result = 1;
#if TRACE
  int  uflow = 0, oflow = 0;
#endif
  byte  *pixelptr = NULL;

  trace((stderr,"gethuffdata: start @ 0x%08lx (sector %ld.%ld)\n",
      ftell(fp), ftell(fp)/0x800, ftell(fp) % 0x800));

  /*
   * correction clipping
   */
  if(clip[256+255] == 0) {
    for(i = 0; i < 256; ++i)
      clip[i +   0] = 0x00,
      clip[i + 256] = (byte) i,
      clip[i + 512] = 0xff;
  }

  /*
   * should really only look for luma plane for 4base, but the
   * there are zeroes in the rest of the sector that give both
   * chroma tables 0 length
   */
  for(i = 0; i < 3; ++i)
    hufftable[i] = NULL;
  for(i = 0; i < 3; ++i) {
    if((hufftable[i] = gethufftable()) == NULL) {
      result = 0;
      break;
    }
  }
  if(result == 0)
    goto oops;

  /*
   * skip remainder of current sector
   */
  i = (ftell(fp) | 0x7ff) + 1;
  if(fseek(fp, i, SEEK_SET) < 0) {
    fprintf(stderr, "gethuffdata: sector skip failed\n");
    return 0;
  }

  /*
   * skip remainder of "sector"
   */
  i = 0;
  while (is24() != 0xfffffe) {
    (void)get24();
    if(++i == 1)
      trace((stderr,"gethuffdata: skipping for sync ..."));
  }
  if(i != 0)
    trace((stderr, " %d times\n", i));

  while(result) {
    if(is24() == 0xfffffe) {
      skip24();
      plane = get2();
      row = get13(); col = 0;
      skip1();
      if(row >= maxrownumber) {
        trace((stderr,
          "gethuffdata: stopping at row %d\n",
                row));
        break;
      }
      switch (plane) {
      case  0:
        huffstart = hufftable[0];
        pixelptr  = luma + row*realrowwidth;
        break;

      case  2:
        huffstart = hufftable[1];
        pixelptr  = chroma1 + row/2*realrowwidth/2;
        break;

      case  3:
        huffstart = hufftable[2];
        pixelptr  = chroma2 + row/2*realrowwidth/2;
        break;

      default:
        fprintf(stderr, "gethuffdata: bad plane %d\n",
                  plane);
        result = 0;
        break;
      }
      WaitCursor();
      continue;
    }

    /*
     * locate correction in huffman tree
     */
    for(huffptr = huffstart;;) {
      huffptr += get1();
      if(*huffptr < 0) {
        huffptr -= *huffptr;
      } else if(*huffptr == 0) {
        fprintf(stderr,
          "gethuffdata: invalid code: "
            "image quality reduced\n");
        result = 0;
        break;
      } else
        break;
    }
    if(!result)
      break;

    /*
     * apply correction to the pixel
     *
     * eeeek!!  the corrections can sometimes over or underflow!
     * this strongly suggested that the 'magnify' method was in
     * some way wrong.  however, experiments showed that the
     * over/under flows even occured for the pixels that are
     * copied through magnify without change (ie, the even
     * row/even column case).  curiously, though, the odd
     * column and odd row cases were about 3x more likely to have
     * the over/underflow, and the odd row/odd column case was
     * about 5x higher, so maybe the use of a bi-linear
     * interpolation is not correct -- just *close*?
     *
     * the other clue in this area is that the overflows are
     * by far most frequenct along edges of very bright
     * areas -- rarely in the interior of such regions.
     */
    i = (int) *pixelptr + (signed char) (*huffptr - 1);
#if TRACE
    if(i > 255)
      ++oflow;
/*      trace((stderr,
        "gethuffdata: oflow %d %d %d\n", row, col, i));*/
    else if(i < 0)
      ++uflow;
/*      trace((stderr,
        "gethuffdata: uflow %d %d %d\n", row, col, i));*/
    ++col;
#endif
    *pixelptr++ = clip[i + 256];
  }

oops:
  for(i = 0; i < 3; ++i)
    free(hufftable[i]);
  trace((stderr, "gethuffdata: uflow=%d oflow=%d\n", uflow, oflow));
  trace((stderr, "gethuffdata: done @ 0x%08lx (sector %ld.%d)\n",
        ftell(fp), ftell(fp)/0x800, 0x800 - bytesleft));
  return result;
}

#endif /* HAVE_PCD */
