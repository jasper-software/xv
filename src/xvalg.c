/*
 * xvalg.c - image manipulation algorithms (Blur, etc.)
 *
 *  Contains:
 *         void AlgInit();
 *         void DoAlg(int algnum);
 */

#include "copyright.h"

#include "xv.h"

#ifndef M_PI
#  define M_PI (3.1415926535897932385)
#endif


static void NoAlg          PARM((void));
static void Blur           PARM((void));
static void Sharpen        PARM((void));
static void EdgeDetect     PARM((void));
static void TinFoil        PARM((void));
static void OilPaint       PARM((void));
static void Blend          PARM((void));
static void FineRotate     PARM((int));
static void Pixelize       PARM((void));
static void Spread         PARM((void));
static void MedianFilter   PARM((void));
static void saveOrigPic    PARM((void));

static void doBlurConvolv  PARM((byte *,int,int,byte *, int,int,int,int, int));
static void doSharpConvolv PARM((byte *,int,int,byte *, int,int,int,int, int));
static void doEdgeConvolv  PARM((byte *,int,int,byte *, int,int,int,int));
static void doAngleConvolv PARM((byte *,int,int,byte *, int,int,int,int));
static void doOilPaint     PARM((byte *,int,int,byte *, int,int,int,int, int));
static void doBlend        PARM((byte *,int,int,byte *, int,int,int,int));
static void doRotate       PARM((byte *,int,int,byte *, int,int,int,int,
				 double, int));
static void doPixel        PARM((byte *,int,int,byte *, int,int,int,int,
				 int, int));
static void doSpread       PARM((byte *,int,int,byte *, int,int,int,int, 
				 int, int));
static void doMedianFilter PARM((byte *,int,int,byte *, int,int,int,int, int));

static void add2bb         PARM((int *, int *, int *, int *, int, int));
static void rotXfer        PARM((int, int, double *,double *,
				 double,double, double));

#ifdef FOO
static void intsort        PARM((int *, int));
#endif

static int  start24bitAlg  PARM((byte **, byte **));
static void end24bitAlg    PARM((byte *, byte *));

static void printUTime     PARM((char *));

static byte *origPic = (byte *) NULL;
static int  origPicType;
static byte origrmap[256], origgmap[256], origbmap[256];


#undef TIMING_TEST

#ifdef TIMING_TEST
#include <sys/time.h>
#include <sys/resource.h>
#endif


/***************************/
static void printUTime(str)
     char *str;
{
#ifdef TIMING_TEST
  int i;  struct rusage ru;

  i = getrusage(RUSAGE_SELF, &ru);
  fprintf(stderr,"%s: utime = %d.%d seconds\n",
	    str, ru.ru_utime.tv_sec, ru.ru_utime.tv_usec);
#endif
}






/************************************************************/
void AlgInit()
{
  /* called whenver an image file is loaded.  disposes of origPic 
     if neccessary, and points it to null */

  if (origPic) free(origPic);
  origPic = (byte *) NULL;

  algMB.dim[ALG_NONE] = 1;    /* can't undo when init'ed already */
}


/************************/
void DoAlg(anum)
     int anum;
{
  /* called with a value from the algMB button.  Executes the specified
     algorithm */

  switch (anum) {
  case ALG_NONE:      NoAlg();        break;
  case ALG_BLUR:      Blur();         break;
  case ALG_SHARPEN:   Sharpen();      break;
  case ALG_EDGE:      EdgeDetect();   break;
  case ALG_TINF:      TinFoil();      break;
  case ALG_OIL:       OilPaint();     break;
  case ALG_BLEND:     Blend();        break;
  case ALG_ROTATE:    FineRotate(0);  break;
  case ALG_ROTATECLR: FineRotate(1);  break;
  case ALG_PIXEL:     Pixelize();     break;
  case ALG_SPREAD:    Spread();       break;
  case ALG_MEDIAN:    MedianFilter(); break;
  }

  algMB.dim[ALG_NONE] = (origPic == (byte *) NULL);
}



/************************/
static void NoAlg()
{
  int i;

  /* restore original picture */
  if (!origPic) return;  /* none to restore */

  WaitCursor();

  KillOldPics();   /* toss the old pic/cpic/epic/theImage away */

  picType = origPicType;
  Set824Menus(picType);

  pic = origPic;  origPic = NULL;

  if (picType == PIC8) {
    for (i=0; i<256; i++) {
      rMap[i] = origrmap[i];
      gMap[i] = origgmap[i];
      bMap[i] = origbmap[i];
    }
  }

  InstallNewPic();
}


/************************/
static void Blur()
{
  /* runs a n*n convolution mask (all 1's) over 'pic',
     producing a 24-bit version.  Then calls 24to8 to generate a new 8-bit
     image, and installs it. 

     Note that 'n' must be odd for things to work properly */

  byte        *pic24, *tmpPic;
  int          i, sx,sy,sw,sh, n;
  static char *labels[] = { "\nOk", "\033Cancel" };
  char         txt[256];
  static char  buf[64] = { '3', '\0' };
  
  sprintf(txt, "Blur:                                   \n\n%s",
	  "Enter mask size (ex. 3, 5, 7, ...)");

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789", 1);
  if (i==1 || strlen(buf)==0) return;
  n = atoi(buf);

  if (n < 1 || (n&1)!=1) {
    ErrPopUp("Error:  The value entered must be odd and greater than zero.", 
	     "\nOh!");
    return;
  }

  WaitCursor();

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);

  SetISTR(ISTR_INFO, "Blurring %s with %dx%d convolution mask...",
	  (HaveSelection() ? "selection" : "image"), n,n);

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));
    
  doBlurConvolv(pic24, pWIDE,pHIGH, tmpPic, sx,sy,sw,sh, n);

  end24bitAlg(pic24, tmpPic);
}



/************************/
static void Sharpen()
{
  /* runs an edge-enhancment algorithm */

  byte        *pic24, *tmpPic;
  int          i, sx,sy,sw,sh, n;
  static char *labels[] = { "\nOk", "\033Cancel" };
  char         txt[256];
  static char  buf[64] = { '7', '5', '\0' };
  
  sprintf(txt, "Sharpen:                                   \n\n%s",
	  "Enter enhancement factor (0-99%)");

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789", 1);
  if (i==1 || strlen(buf)==0) return;
  n = atoi(buf);

  if (n>=100) {
    ErrPopUp("Error:  The value entered must be less than 100%.", "\nOh!");
    return;
  }

  WaitCursor();

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);

  SetISTR(ISTR_INFO, "Sharpening %s by factor of %d%%...",
	  (HaveSelection() ? "selection" : "image"), n);

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));
    
  doSharpConvolv(pic24, pWIDE,pHIGH, tmpPic, sx,sy,sw,sh, n);

  end24bitAlg(pic24, tmpPic);
}



/************************/
static void EdgeDetect()
{
  byte *pic24, *p24, *tmpPic, *tlp;
  char *str;
  int  i, j, v, maxv, sx,sy,sw,sh;

  WaitCursor();

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);

  str = "Doing edge detection...";
  SetISTR(ISTR_INFO, str);

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  doEdgeConvolv(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh);
  
  SetISTR(ISTR_INFO, "%snormalizing...", str);

  /* Normalize results */
  for (i=sy, maxv=0; i<sy+sh; i++) {              /* compute max value */
    p24 = tmpPic + (i*pWIDE + sx) * 3;
    for (j=sx; j<sx+sw; j++, p24+=3) {
      v = MONO(p24[0], p24[1], p24[2]);
      if (v>maxv) maxv = v;
    }
  }

  for (i=sy; maxv>0 && i<sy+sh; i++) {            /* normalize */
    p24 = tlp = tmpPic + (i*pWIDE + sx) * 3;
    for (j=0; j<sw*3; j++) {
      v = (((int) *p24) * 255) / maxv;
      RANGE(v,0,255);
      *p24++ = (byte) v;
    }
  }

  end24bitAlg(pic24, tmpPic);
}


/************************/
static void TinFoil()
{
  byte *pic24, *p24, *tmpPic, *tp, *tlp;
  char *str;
  int  i, j, v, maxv,sx,sy,sw,sh;
  
  WaitCursor();
  
  str = "Doing cheesy embossing effect...";
  SetISTR(ISTR_INFO, str);
  
  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);
  
  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  /* fill selected area of tmpPic with gray128 */
  for (i=sy; i<sy+sh; i++) {
    tp = tlp = tmpPic + (i*pWIDE + sx) * 3;
    for (j=sx; j<sx+sw; j++) {
      *tp++ = 128;  *tp++ = 128;  *tp++ = 128;
    }
  }
  
  doAngleConvolv(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh);
  
  /* mono-ify selected area of tmpPic */
  for (i=sy; i<sy+sh; i++) {
    tp = tlp = tmpPic + (i*pWIDE + sx) * 3;
    for (j=sx; j<sx+sw; j++, tp+=3) {
      v = MONO(tp[0], tp[1], tp[2]);
      RANGE(v,0,255);
      tp[0] = tp[1] = tp[2] = (byte) v;
    }
  }
    
  end24bitAlg(pic24, tmpPic);
}  


/************************/
static void OilPaint()
{
  byte *pic24, *tmpPic;
  int   sx,sy,sw,sh;

  WaitCursor();

  SetISTR(ISTR_INFO, "Doing oilpaint effect...");

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);
  
  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  doOilPaint(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh, 3);

  end24bitAlg(pic24, tmpPic);
}



/************************/
static void Blend()
{
  byte *pic24, *tmpPic;
  int   sx,sy,sw,sh;

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);
  
  WaitCursor();

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  doBlend(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh);

  end24bitAlg(pic24, tmpPic);
}


/************************/
static void FineRotate(clr)
     int clr;
{
  byte        *pic24, *tmpPic;
  int          i,sx,sy,sw,sh;
  double       rotval;
  static char *labels[] = { "\nOk", "\033Cancel" };
  char         txt[256];
  static char  buf[64] = { '\0' };

  sprintf(txt, "Rotate (%s):\n\nEnter rotation angle, in degrees:  (>0 = CCW)",
	  (clr ? "Clear" : "Copy"));

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789.-", 1);
  if (i==1 || strlen(buf)==0) return;
  rotval = atof(buf);

  if (rotval == 0.0) return;
  

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);
  
  WaitCursor();

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  doRotate(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh, rotval, clr);

  end24bitAlg(pic24, tmpPic);
}


/************************/
static void Pixelize()
{
  byte        *pic24, *tmpPic;
  int          i,sx,sy,sw,sh, pixX,pixY,err;
  static char *labels[] = { "\nOk", "\033Cancel" };
  char         txt[256];
  static char  buf[64] = { '4', '\0' };

  sprintf(txt, "Pixelize:\n\nEnter new pixel size, in image pixels:  %s",
	  "(ex. '3', '5x8')");

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789x", 1);
  if (i==1 || strlen(buf)==0) return;

  pixX = pixY = err = 0;

  if (index(buf, 'x')) {
    if (sscanf(buf, "%d x %d", &pixX, &pixY) != 2) err++;
  }
  else {
    if (sscanf(buf, "%d", &pixX) != 1) err++;
    pixY = pixX;
  }

  if (pixX<1 || pixY<1 || err) {
    ErrPopUp("Error:  The entered string is not valid.", "\nWho Cares!");
    return;
  }

  
  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);
  
  WaitCursor();

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  doPixel(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh, pixX, pixY);

  end24bitAlg(pic24, tmpPic);
}



/************************/
static void Spread()
{
  byte        *pic24, *tmpPic;
  int          i,sx,sy,sw,sh, pixX,pixY,err;
  static char *labels[] = { "\nOk", "\033Cancel" };
  char         txt[256];
  static char  buf[64] = { '5', '\0' };

  sprintf(txt, "Spread:\n\nEnter spread factor (or x,y factors):  %s",
	  "(ex. '10', '1x5')");

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789x", 1);
  if (i==1 || strlen(buf)==0) return;

  pixX = pixY = err = 0;

  if (index(buf, 'x')) {
    if (sscanf(buf, "%d x %d", &pixX, &pixY) != 2) err++;
    if (pixX<0 || pixY<0) err++;
  }
  else {
    if (sscanf(buf, "%d", &pixX) != 1) err++;
    pixX = -pixX;
  }

  if (!pixX && !pixY) return;     /* 0x0 has no effect */

  if (err) {
    ErrPopUp("Error:  The entered string is not valid.", "\nBite Me!");
    return;
  }

  
  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);
  
  WaitCursor();

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));

  doSpread(pic24, pWIDE, pHIGH, tmpPic, sx,sy,sw,sh, pixX, pixY);

  end24bitAlg(pic24, tmpPic);
}



/************************/
static void MedianFilter()
{
  /* runs median filter algorithm (for n*n rect centered around each pixel,
     replace with median value */

  byte        *pic24, *tmpPic;
  int          i, sx,sy,sw,sh, n;
  static char *labels[] = { "\nOk", "\033Cancel" };
  char         txt[256];
  static char  buf[64] = { '3', '\0' };
  
  sprintf(txt, "DeSpeckle (median filter):                          \n\n%s",
	  "Enter mask size (ex. 3, 5, 7, ...)");

  i = GetStrPopUp(txt, labels, 2, buf, 64, "0123456789", 1);
  if (i==1 || strlen(buf)==0) return;
  n = atoi(buf);

  if (n < 1 || (n&1)!=1) {
    ErrPopUp("Error:  The value entered must be odd and greater than zero.", 
	     "\nOh!");
    return;
  }

  WaitCursor();

  if (HaveSelection()) GetSelRCoords(&sx,&sy,&sw,&sh);
  else { sx = 0;  sy = 0;  sw = pWIDE;  sh = pHIGH; }
  CropRect2Rect(&sx,&sy,&sw,&sh, 0,0,pWIDE,pHIGH);

  SetISTR(ISTR_INFO, "DeSpeckling %s with %dx%d convolution mask...",
	  (HaveSelection() ? "selection" : "image"), n,n);

  if (start24bitAlg(&pic24, &tmpPic)) return;
  xvbcopy((char *) pic24, (char *) tmpPic, (size_t) (pWIDE*pHIGH*3));
    
  doMedianFilter(pic24, pWIDE,pHIGH, tmpPic, sx,sy,sw,sh, n);

  end24bitAlg(pic24, tmpPic);
}



/************************/
static void doBlurConvolv(pic24, w, h, results, selx,sely,selw,selh, n)
     byte *pic24, *results;
     int   w,h, selx,sely,selw,selh, n;
{

  /* convolves with an n*n array, consisting of only 1's.  
     Operates on rectangular region 'selx,sely,selw,selh' (in pic coords)
     Region is guaranteed to be completely within pic boundaries
     'n' must be odd */

  register byte *p24;
  register int   rsum,gsum,bsum;
  byte          *rp;
  int            i,j,k,x,y,x1,y1,count,n2;


  printUTime("start of blurConvolv");

  n2 = n/2;

  for (y=sely; y<sely+selh; y++) {
    ProgressMeter(sely, (sely+selh)-1, y, "Blur");
    if ((y & 15) == 0) WaitCursor();

    p24 = pic24   + y*w*3 + selx*3;
    rp  = results + y*w*3 + selx*3;

    for (x=selx; x<selx+selw; x++) {

      rsum = gsum = bsum = 0;  count = 0;

      for (y1=y-n2; y1<=y+n2; y1++) {

	if (y1>=sely && y1<sely+selh) {
	  p24 = pic24 + y1*w*3 +(x-n2)*3; 

	  for (x1=x-n2; x1<=x+n2; x1++) {
	    if (x1>=selx && x1<selx+selw) {
	      rsum += *p24++;
	      gsum += *p24++;
	      bsum += *p24++;
	      count++;
	    }
	    else p24 += 3;
	  }
	}
      }

      rsum = rsum / count;
      gsum = gsum / count;
      bsum = bsum / count;

      RANGE(rsum,0,255);
      RANGE(gsum,0,255);
      RANGE(bsum,0,255);

      *rp++ = (byte) rsum;
      *rp++ = (byte) gsum;
      *rp++ = (byte) bsum;
    }
  }


  printUTime("end of blurConvolv");
}



/************************/
static void doSharpConvolv(pic24, w, h, results, selx,sely,selw,selh, n)
     byte *pic24, *results;
     int   w,h, selx,sely,selw,selh, n;
{
  byte  *p24;
  int    rv, gv, bv;
  byte  *rp;
  int    i,j,k,x,y,x1,y1;
  double fact, ifact, hue,sat,val, vsum;
  double *linem1, *line0, *linep1, *tmpptr;

  printUTime("start of sharpConvolv");

  fact  = n / 100.0;
  ifact = 1.0 - fact;

  if (selw<3 || selh<3) return;  /* too small */


  linem1 = (double *) malloc(selw * sizeof(double));
  line0  = (double *) malloc(selw * sizeof(double));
  linep1 = (double *) malloc(selw * sizeof(double));

  if (!linem1 || !line0 || !linep1) {
    ErrPopUp("Malloc() error in doSharpConvov().", "\nDoh!");
    if (linem1) free(linem1);
    if (line0)  free(line0);
    if (linep1) free(linep1);
    return;
  }


  /* load up line arrays */
  p24 = pic24 + (((sely+1)-1)*w + selx) * 3;
  for (x=selx, tmpptr=line0; x<selx+selw; x++, p24+=3, tmpptr++) {
    rgb2hsv((int) p24[0], (int) p24[1], (int) p24[2], &hue,&sat,&val);
    *tmpptr = val;
  }

  p24 = pic24 + (((sely+1)+0)*w + selx) * 3;
  for (x=selx, tmpptr=linep1; x<selx+selw; x++, p24+=3, tmpptr++) {
    rgb2hsv((int) p24[0], (int) p24[1], (int) p24[2], &hue,&sat,&val);
    *tmpptr = val;
  }


  for (y=sely+1; y<(sely+selh)-1; y++) {
    ProgressMeter(sely+1, (sely+selh)-2, y, "Sharpen");
    if ((y & 15) == 0) WaitCursor();
    
    tmpptr = linem1;   linem1 = line0;   line0 = linep1;   linep1 = tmpptr;

    /* get next line */
    p24 = pic24 + ((y+1)*w + selx) * 3;
    for (x=selx, tmpptr=linep1; x<selx+selw; x++, p24+=3, tmpptr++) {
      rgb2hsv((int) p24[0], (int) p24[1], (int) p24[2], &hue,&sat,&val);
      *tmpptr = val;
    }

    p24 = pic24   + (y*w + selx+1) * 3;
    rp  = results + (y*w + selx+1) * 3;

    for (x=selx+1; x<selx+selw-1; x++, p24+=3) {
      vsum = 0.0;  i = x-selx;
      vsum = linem1[i-1] + linem1[i] + linem1[i+1] +
	     line0 [i-1] + line0 [i] + line0 [i+1] +
	     linep1[i-1] + linep1[i] + linep1[i+1];
      
      rgb2hsv((int) p24[0], (int) p24[1], (int) p24[2], &hue, &sat, &val);

      val = ((val - (fact * vsum) / 9) / ifact);
      RANGE(val, 0.0, 1.0);
      hsv2rgb(hue,sat,val, &rv, &gv, &bv);

      RANGE(rv,0,255);
      RANGE(gv,0,255);
      RANGE(bv,0,255);

      *rp++ = (byte) rv;
      *rp++ = (byte) gv;
      *rp++ = (byte) bv;
    }
  }


  free(linem1);  free(line0);  free(linep1);

  printUTime("end of sharpConvolv");
}



/************************/
static void doEdgeConvolv(pic24, w, h, results, selx,sely,selw,selh)
     byte *pic24, *results;
     int   w, h, selx,sely,selw,selh;
{

  /* convolves with two edge detection masks (vertical and horizontal)
     simultaneously, taking Max(abs(results)) 
     
     The two masks are (hard coded):

          -1 0 1             -1 -1 -1
      H = -1 0 1     and V =  0  0  0
          -1 0 1              1  1  1

     divided into 
           -1 0 0         0 0 0         0 0 1        0  1 0
       a =  0 0 0 ,  b = -1 0 1 ,  c =  0 0 0 ,  d = 0  0 0 .
            0 0 1         0 0 0        -1 0 0        0 -1 0

     So H = a + b + c,  V = a - c - d.
     gradient = max(abs(H),abs(V)).
          
     Also, only does pixels in which the masks fit fully onto the picture
     (no pesky boundary conditionals)  */


  register byte *p24;
  register int   bperlin, a, b, c, d, rsum, gsum, bsum;
  byte          *rp;
  int            i, x, y;


  printUTime("start of edgeConvolv");

  bperlin = w * 3;

  for (y=sely+1; y<sely+selh-1; y++) {
    ProgressMeter(sely+1, (sely+selh-1)-1, y, "Edge Detect");
    if ((y & 63) == 0) WaitCursor();

    rp  = results + (y*w + selx+1)*3;
    p24 = pic24   + (y*w + selx+1)*3;

    for (x=selx+1; x<selx+selw-1; x++, p24+=3) {
      /* RED PLANE */
      a = p24[bperlin+3]  - p24[-bperlin-3]; /* bottom right - top left */
      b = p24[3]   -        p24[-3];         /* mid    right - mid left */
      c = p24[-bperlin+3] - p24[bperlin-3];  /* top    right - bottom left */
      d = p24[-bperlin]   - p24[bperlin];    /* top    mid   - bottom mid */

      rsum = a + b + c;      /* horizontal gradient */
      if (rsum < 0) rsum = -rsum;
      a = a - c - d;         /* vertical gradient */
      if (a < 0) a = -a;
      if (a > rsum) rsum = a;
      rsum /= 3;

      /* GREEN PLANE */
      a = p24[bperlin+4]  - p24[-bperlin-2]; /* bottom right - top left */
      b = p24[4]   -        p24[-2];         /* mid    right - mid left */
      c = p24[-bperlin+4] - p24[bperlin-2];  /* top    right - bottom left */
      d = p24[-bperlin+1] - p24[bperlin+1];  /* top    mid   - bottom mid */

      gsum = a + b + c;      /* horizontal gradient */
      if (gsum < 0) gsum = -gsum;
      a = a - c - d;         /* vertical gradient */
      if (a < 0) a = -a;
      if (a > gsum) gsum = a;
      gsum /= 3;

      /* BLUE PLANE */
      a = p24[bperlin+5]  - p24[-bperlin-1]; /* bottom right - top left */
      b = p24[5]   -        p24[-1];         /* mid    right - mid left */
      c = p24[-bperlin+5] - p24[bperlin-1];  /* top    right - bottom left */
      d = p24[-bperlin+2] - p24[bperlin+2];  /* top    mid   - bottom mid */

      bsum = a + b + c;      /* horizontal gradient */
      if (bsum < 0) bsum = -bsum;
      a = a - c - d;         /* vertical gradient */
      if (a < 0) a = -a;
      if (a > bsum) bsum = a;
      bsum /= 3;

      *rp++ = (byte) rsum;
      *rp++ = (byte) gsum;
      *rp++ = (byte) bsum;
    }
  }

  printUTime("end of edgeConvolv");
}



/************************/
static void doAngleConvolv(pic24, w, h, results, selx,sely,selw,selh)
     byte *pic24, *results;
     int   w, h, selx,sely,selw,selh;
{

  /* convolves with edge detection mask, at 45 degrees to horizontal.
     
     The mask is (hard coded):

             -2 -1 0
             -1  0 1
              0  1 2
          
     Also, only does pixels in which the masks fit fully onto the picture
     (no pesky boundary conditionals)

     Adds value of rsum,gsum,bsum to results pic */

  register byte *p24;
  register int   bperlin,rsum,gsum,bsum;
  byte          *rp;
  int            i, x,y;


  printUTime("start of doAngleConvolv");

  bperlin = w * 3;

  for (y=sely+1; y<sely+selh-1; y++) {
    ProgressMeter(sely+1, (sely+selh-1)-1, y, "Convolve");
    if ((y & 63) == 0) WaitCursor();

    rp  = results + (y*w + selx+1)*3;
    p24 = pic24   + (y*w + selx+1)*3;

    for (x=selx+1; x<selx+selw-1; x++, p24+=3) {

      /* compute weighted average for *p24 (pic24[x,y]) */
      rsum = gsum = bsum = 0;

      rsum -= (p24[-bperlin-3] * 2);   /* top left */
      gsum -= (p24[-bperlin-2] * 2);
      bsum -= (p24[-bperlin-1] * 2);

      rsum -= p24[-bperlin];           /* top mid */
      gsum -= p24[-bperlin+1];
      bsum -= p24[-bperlin+2];

      rsum -= p24[-3];                 /* mid left */
      gsum -= p24[-2];
      bsum -= p24[-1];

      rsum += p24[3];                  /* mid right */
      gsum += p24[4];
      bsum += p24[5];

      rsum += p24[bperlin];            /* bottom mid */
      gsum += p24[bperlin+1];
      bsum += p24[bperlin+2];

      rsum += (p24[bperlin+3] * 2);    /* bottom right */
      gsum += (p24[bperlin+4] * 2);
      bsum += (p24[bperlin+5] * 2);

      rsum = rsum / 8;
      gsum = gsum / 8;
      bsum = bsum / 8;

      rsum += rp[0];  RANGE(rsum,0,255);
      gsum += rp[1];  RANGE(gsum,0,255);
      bsum += rp[2];  RANGE(bsum,0,255);

      *rp++ = (byte) rsum;
      *rp++ = (byte) gsum;
      *rp++ = (byte) bsum;
    }
  }

  printUTime("end of edgeConvolv");
}




/************************/
static void doOilPaint(pic24, w, h, results, selx,sely,selw,selh, n)
     byte *pic24, *results;
     int   w, h, selx,sely,selw,selh, n;
{

  /* does an 'oil transfer', as described in the book "Beyond Photography",
     by Holzmann, chapter 4, photo 7.  It is a sort of localized smearing.

     The following algorithm (but no actual code) was borrowed from
     'pgmoil.c', written by Wilson Bent (whb@hoh-2.att.com),
     and distributed as part of the PBMPLUS package.

     for each pixel in the image (assume, for a second, a grayscale image),
     compute a histogram of the n*n rectangle centered on the pixel.
     replace the pixel with the color that had the greatest # of hits in
     the histogram.  Note that 'n' should be odd. 

     I've modified the algorithm to do the *right* thing for RGB images.
     (jhb, 6/94)  */


  register byte *pp;
  register int   bperlin, rsum,gsum,bsum;
  byte          *rp, *p24, *plin;
  int            i,j,k,x,y,n2,col,cnt,maxcnt;
  int           *nnrect;

  printUTime("start of doOilPaint");

  if (n & 1) n++;   /* n must be odd */

  bperlin = w * 3;
  n2 = n/2;

  /* nnrect[] is an n*n array of ints, with '-1' meaning 'outside the region'
     otherwise they'll have 24-bit RGB values */
  
  nnrect = (int *) malloc(n * n * sizeof(int));
  if (!nnrect) FatalError("can't malloc nnrect[] in doOilPaint()\n");

  for (y=sely; y<sely+selh; y++) {
    if ((y & 15) == 0) WaitCursor();
    ProgressMeter(sely, (sely+selh)-1, y, "Oil Paint");

    p24 = pic24 + ((y-n2)*w + selx-n2)*3;   /* pts to top-left of mask */
    rp  = results + (y*w + selx)*3;
    
    for (x=selx; x<selx+selw; x++) {
      /* fill 'nnrect' with valid pixels from n*n region centered round x,y */
      pp = plin = p24;
      for (i=y-n2, k=0; i<y+n2; i++) {
	for (j=x-n2; j<x+n2; j++, k++, pp+=3) {
	  if (i>=sely && i<sely+selh && j>=selx && j<selx+selw) { 
	    nnrect[k] = (((int) pp[0])<<16) | (((int) pp[1])<<8) | pp[2];
	  }
	  else nnrect[k] = -1;
	}
	plin += bperlin;  pp = plin;
      }

      
      /* find 'most popular color' in nnrect, not counting '-1' */
      maxcnt = cnt = col = 0;
      for (i=0; i<n*n; i++) {
	if (nnrect[i] != -1) {
	  cnt = 1;
	  for (j=i+1; j<n*n; j++) { /* count it */
	    if (nnrect[i] == nnrect[j]) cnt++;
	  }
	  if (cnt>maxcnt) { col = nnrect[i];  maxcnt = cnt; }
	}
      }

      *rp++ = (byte) ((col>>16) & 0xff);
      *rp++ = (byte) ((col>>8)  & 0xff);
      *rp++ = (byte) ((col)     & 0xff);

      p24 += 3;
    }
  }

  free(nnrect);
  printUTime("end of doOilPaint");
}


/************************/
static void doBlend(pic24, w, h, results, selx,sely,selw,selh)
     byte *pic24, *results;
     int   w, h, selx,sely,selw,selh;
{
  /* 'blends' a rectangular region out of existence.  computes the average
     color of all the pixels on the edge of the rect, stores this in the
     center, and for each pixel in the rect, replaces it with a weighted
     average of the center color, and the color on the edge that intersects
     a line drawn from the center to the point */

  byte  *p24;
  int    i,x,y;
  int    cx,cy,cR,cG,cB;
  int    ex,ey,eR,eG,eB;
  int    dx,dy,r,g,b;
  double rf,gf,bf, slope,dslope, d,d1, ratio;

  if (selw<3 || selh<3) return;        /* too small to blend */

  printUTime("start of blend");

  /*** COMPUTE COLOR OF CENTER POINT ***/

  i = 0;  rf = gf = bf = 0.0;
  for (x=selx; x<selx+selw; x++) {
    p24 = pic24 + (sely*w + x) * 3;
    rf += (double) p24[0];  gf += (double) p24[1];  bf += (double) p24[2];
    i++;

    p24 = pic24 + ((sely+selh-1)*w + x) * 3;
    rf += (double) p24[0];  gf += (double) p24[1];  bf += (double) p24[2];
    i++;
  }
  for (y=sely; y<sely+selh; y++) {
    p24 = pic24 + (y*w + selx) * 3;
    rf += (double) p24[0];  gf += (double) p24[1];  bf += (double) p24[2];
    i++;
    
    p24 = pic24 + (y*w + (selx+selw-1)) * 3;
    rf += (double) p24[0];  gf += (double) p24[1];  bf += (double) p24[2];
    i++;
  }

  cR = (int) (rf / i);
  cG = (int) (gf / i);
  cB = (int) (bf / i);

  cx = selx + selw/2;  cy = sely + selh/2;

  dslope = ((double) selh) / selw;

  /* for each point on INTERIOR of region, do the thing */
  for (y=sely+1; y<sely+selh-1; y++) {
    if ((y & 15) == 0) WaitCursor();
    ProgressMeter(sely+1, (sely+selh-1)-1, y, "Blend");

    for (x=selx+1; x<selx+selw-1; x++) {

      /* compute edge intercept point ex,ey */
      dx = x - cx;  dy = y - cy;
      if (dx==0 && dy==0) { ex = selx;  ey = sely; }  /* don't care */
      else if (dx==0) {	ex = cx;  ey = (dy<0) ? sely : sely+selh-1; }
      else if (dy==0) {	ey = cy;  ex = (dx<0) ? selx : selx+selw-1; }
      else { 
	slope = ((double) dy) / dx;
	if (fabs(slope) > fabs(dslope)) {   /* y axis is major */
	  ey = (dy<0) ? sely : sely+selh-1;
	  ex = cx + ((ey-cy) * dx) / dy;
	}
	else {                              /* x axis is major */
	  ex = (dx<0) ? selx : selx+selw-1;
	  ey = cy + ((ex-cx) * dy) / dx;
	}
      }

      /* let's play it safe... */
      RANGE(ex, selx, selx+selw-1);
      RANGE(ey, sely, sely+selh-1);

      /* given (cx,cy), (x,y), and (ex,ey), compute d, d1 */
      d  = sqrt((double) (dx * dx) + (double) (dy * dy));
      d1 = sqrt((double) ((ex-cx) * (ex-cx)) + (double) ((ey-cy) * (ey-cy)));
      ratio = pow(d/d1, 2.0);

      /* fetch color of ex,ey */
      p24 = pic24 + (ey*w + ex) * 3;
      eR = p24[0];  eG=p24[1];  eB=p24[2];

      /* compute new color for x,y */
      if (dx==0 && dy==0) { r=cR;  g=cG;  b=cB; }
      else {
	r = cR + (int) ((eR-cR) * ratio);
	g = cG + (int) ((eG-cG) * ratio);
	b = cB + (int) ((eB-cB) * ratio);
	RANGE(r,0,255);
	RANGE(g,0,255);
	RANGE(b,0,255);
      }

      /* and stuff it... */
      p24 = results + (y*w + x) * 3;
      p24[0] = (byte) r;  p24[1] = (byte) g;  p24[2] = (byte) b;
    }
  }

  printUTime("end of blend");
}

  

/************************/
static void doRotate(pic24, w, h, results, selx,sely,selw,selh, rotval, clear)
     byte  *pic24, *results;
     int    w, h, selx,sely,selw,selh,clear;
     double rotval;
{
  /* rotates a rectangular region (selx,sely,selw,selh) of an image (pic24,w,h)
     by the amount specified in degrees (rotval), and stores the result in
     'results', which is also a w*h 24-bit image.  The rotated bits are
     clipped to fit in 'results'.  If 'clear', the (unrotated) rectangular
     region is cleared (in results) first.  
     sel[x,y,w,h] is guaranteed to be within image bounds */

  byte  *pp, *dp;
  int    i, j, x, y, ox,oy, ox1,oy1;
  int    rx1,ry1, rx2,ry2, rx3,ry3, rx4,ry4;
  int    rbx, rby, rbx1, rby1, rbw, rbh;
  double rotrad, xf,yf, xfrac,yfrac, px,py, apx, apy, cfx,cfy;

  if (selw<1 || selh<1) return;

  printUTime("start of rotate");

  /*
   * cfx,cfy  -  center point of sel rectangle (double) 
   * rx1,ry1  -  top-left  of sel, rotated
   * rx2,ry2  -  bot-left  of sel, rotated
   * rx3,ry3  -  top-right of sel, rotated
   * rx4,ry4  -  bot-right of sel, rotated
   * rbx, rby, rbw, rbh  -  bounding box for rotated sel, clipped to pic
   */

  rotrad = rotval * M_PI / 180.0;

  /* compute corner points of 'sel' after rotation */
  cfx = selx + (selw-1)/2.0;  cfy = sely + (selh-1)/2.0;

  rotXfer(selx,        sely,      &xf, &yf, cfx, cfy, rotrad);
  rx1 = (int) (xf + ((xf<0) ? -0.5 : 0.5));
  ry1 = (int) (yf + ((yf<0) ? -0.5 : 0.5));

  rotXfer(selx,        sely+selh, &xf, &yf, cfx, cfy, rotrad);
  rx2 = (int) (xf + ((xf<0) ? -0.5 : 0.5));
  ry2 = (int) (yf + ((yf<0) ? -0.5 : 0.5));

  rotXfer(selx+selw,   sely,      &xf, &yf, cfx, cfy, rotrad);
  rx3 = (int) (xf + ((xf<0) ? -0.5 : 0.5));
  ry3 = (int) (yf + ((yf<0) ? -0.5 : 0.5));

  rotXfer(selx+selw,   sely+selh, &xf, &yf, cfx, cfy, rotrad);
  rx4 = (int) (xf + ((xf<0) ? -0.5 : 0.5));
  ry4 = (int) (yf + ((yf<0) ? -0.5 : 0.5));

  /* compute bounding box for rotated rect */

  rbx = rbx1 = rx1;  rby = rby1 = ry1;
  add2bb(&rbx, &rby, &rbx1, &rby1, rx2,ry2);
  add2bb(&rbx, &rby, &rbx1, &rby1, rx3,ry3);
  add2bb(&rbx, &rby, &rbx1, &rby1, rx4,ry4);
  rbw = rbx1 - rbx;  rbh = rby1 - rby;

  /* make it a *little* bigger, just to be safe... */
  rbx--;  rby--;  rbw+=2;  rbh+=2;
  CropRect2Rect(&rbx, &rby, &rbw, &rbh, 0,0,w,h);

  if (clear) {           /* clear original selection in results pic */
    for (i=sely; i<sely+selh; i++) {
      dp = results + (i*w + selx)*3;
      for (j=selx; j<selx+selw; j++) {
	*dp++ = clearR;
	*dp++ = clearG;
	*dp++ = clearB;
      }
    }
  }


  /* now, for each pixel in rb[x,y,w,h], do the inverse rotation to see if
     it would be in the original unrotated selection rectangle.  if it *is*,
     compute and store an appropriate color, otherwise skip it */
 
  for (y=rby; y<rby+rbh; y++) {
    dp = results + (y * w + rbx) * 3;

    if ((y & 15) == 0) WaitCursor();
    ProgressMeter(rby, rby+rbh-1, y, "Rotate");

    for (x=rbx; x<rbx+rbw; x++, dp+=3) {
      rotXfer(x,y, &xf, &yf, cfx, cfy, -rotrad);

      /* cheat a little... */
      if (xf < 0.0  &&  xf > -0.5) xf = 0.0;
      if (yf < 0.0  &&  yf > -0.5) yf = 0.0;

      ox = (int) floor(xf);   oy = (int) floor(yf);

      if (PTINRECT(ox,oy, selx,sely,selw,selh)) {
	int    p0r,p0g,p0b, p1r,p1g,p1b, p2r,p2g,p2b, p3r,p3g,p3b;
	int    rv,gv,bv;
	double rd,gd,bd, p0wgt, p1wgt, p2wgt, p3wgt;
	
	/* compute the color, same idea as in Smooth**().  The color
	   will be a linear combination of the colors of the center pixel,
	   its left-or-right neighbor, its top-or-bottom neighbor, and
	   its corner neighbor.  *which* neighbors are used are determined by
	   the position of the fractional part of xf,yf within the 1-unit
	   square of the pixel.  */

	/* compute px,py: fractional offset from center of pixel (x.5,y.5) */
	xfrac = xf - ox;        /* 0 - .9999 */
	yfrac = yf - oy;
	px = ((xfrac >= .5) ? (xfrac - .5) : (-.5 + xfrac));
	py = ((yfrac >= .5) ? (yfrac - .5) : (-.5 + yfrac));
	apx = fabs(px);  apy = fabs(py);

	/* get neighbor colors:  p0col, p1col, p2col, p3col */
	ox1 = ox + ((px < 0.0) ? -1 : 1);
	oy1 = oy + ((py < 0.0) ? -1 : 1);

	pp = pic24 + (oy * w + ox) * 3;
	p0r = pp[0];  p0g = pp[1];  p0b = pp[2];                   /* ctr */

	if (ox1 >= selx && ox1 < selx+selw) {
	  pp = pic24 + (oy * w + ox1) * 3;
	  p1r = pp[0];  p1g = pp[1];  p1b = pp[2];                 /* l/r */
	  p1wgt = apx * (1.0 - apy);
	}
	else { p1r=p1g=p1b=0;  p1wgt = 0.0; }

	if (oy1 >= sely && oy1 < sely+selh) {
	  pp = pic24 + (oy1 * w + ox) * 3;
	  p2r = pp[0];  p2g = pp[1];  p2b = pp[2];                 /* t/b */
	  p2wgt = apy * (1.0 - apx);
	}
	else { p2r=p2g=p2b=0;  p2wgt = 0.0; }

	if (ox1>=selx && ox1<selx+selw && oy1>=sely && oy1<sely+selh){
	  pp = pic24 + (oy1 * w + ox1) * 3;
	  p3r = pp[0];  p3g = pp[1];  p3b = pp[2];                 /* diag */
	  p3wgt = apx * apy;
	}
	else { p3r=p3g=p3b=0;  p3wgt = 0.0; }

	p1wgt = p1wgt * .7;        /* black art */
	p2wgt = p2wgt * .7;
	p3wgt = p3wgt * .7;

	p0wgt = 1.0 - (p1wgt + p2wgt + p3wgt);

	/* okay, compute and store resulting color */
	rd = p0r * p0wgt + p1r * p1wgt + p2r * p2wgt + p3r * p3wgt;
	gd = p0g * p0wgt + p1g * p1wgt + p2g * p2wgt + p3g * p3wgt;
	bd = p0b * p0wgt + p1b * p1wgt + p2b * p2wgt + p3b * p3wgt;

	rv = (int) (rd + 0.5);
	gv = (int) (gd + 0.5);
	bv = (int) (bd + 0.5);

	RANGE(rv,0,255);  RANGE(gv,0,255);  RANGE(bv,0,255);

#ifdef ROTATE_FOO
if (0)	{
  fprintf(stderr,"--------\n%3d,%3d in results maps to %6.2f,%6.2f in pic24\n",
	  x,y,xf,yf);
  fprintf(stderr,"ox,oy = %3d,%3d (%3d,%3d)  frac=%4.2f,%4.2f  px,py=%4.2f,%4.2f\n", ox,oy, ox1,oy1, xfrac,yfrac, px,py);
  fprintf(stderr,"Colors: 0:%02x%02x%02x 1:%02x%02x%02x 2:%02x%02x%02x 3:%02x%02x%02x\n", p0r,p0g,p0b, p1r,p1g,p1b, p2r,p2g,p2b, p3r,p3g,p3b);
  fprintf(stderr,"Weights: 0[%f]  1[%f]  2[%f]  3[%f] -> %3d,%3d,%3d\n",
	  p0wgt, p1wgt, p2wgt, p3wgt, rv, gv, bv);
}
#endif /* ROTATE_FOO */

	dp[0] = (byte) (rv&0xff);  
	dp[1] = (byte) (gv&0xff);  
	dp[2] = (byte) (bv&0xff);  
      }
    }
  }
  printUTime("end of rotate");
}


/***********************************************/
static void add2bb(x1, y1, x2, y2, x,y)
     int *x1, *y1, *x2, *y2, x,y;
{
  if (x < *x1) *x1 = x;
  if (x > *x2) *x2 = x;
  if (y < *y1) *y1 = y;
  if (y > *y2) *y2 = y;
}


/***********************************************/
static void rotXfer(x,y, rx,ry, cx,cy, rad)
     int x,y;
     double *rx, *ry, cx, cy, rad;
{
  /* take point x,y, rotate it 'rad' radians around cx,cy, return rx,ry */
  double d, xf, yf, ang;

  xf = x;  yf = y;

  d  = sqrt((xf-cx) * (xf-cx) + (yf-cy) * (yf-cy));
  if      ((xf-cx) != 0.0) {
    ang = atan((cy-yf) / (xf-cx));                 /* y-axis flip */
    if ((xf-cx) < 0) ang += M_PI;
  }

  else if ((yf-cy) > 0.0) ang = M_PI * 3.0 / 2.0;
  else                    ang = M_PI * 1.0 / 2.0;

  *rx = (double) cx + (d * cos(ang + rad));
  *ry = (double) cy - (d * sin(ang + rad));

#ifdef FOO
  fprintf(stderr,"rotXfer:  rotating (%4d,%4d) %7.2f degrees around",
	  x,y, rad*180.0 / M_PI);
  fprintf(stderr,"(%4d,%4d) -> %7.2f %7.2f  (d=%f ang=%f)\n", 
	  cx,cy, *rx,*ry, d, ang);
#endif
}
  


/************************/
static void doPixel(pic24, w, h, results, selx,sely,selw,selh, pixX, pixY)
     byte *pic24, *results;
     int   w, h, selx,sely,selw,selh, pixX,pixY;
{
  /* runs 'pixelization' algorithm.  replaces each pixX-by-pixY region 
     (smaller on edges) with the average color within that region */
  
  byte  *pp;
  int    nwide, nhigh, i,j, x,y, x1,y1, stx,sty;
  int    nsum, rsum, gsum, bsum;
  
  printUTime("start of pixelize");
  
  /* center grid on selection */
  nwide = (selw + pixX-1) / pixX;
  nhigh = (selh + pixY-1) / pixY;
  
  stx = selx - (nwide*pixX - selw)/2;
  sty = sely - (nhigh*pixY - selh)/2;
  
  y = sty;
  for (i=0; i<nhigh; i++, y+=pixY) {
    ProgressMeter(0, nhigh-1, i, "Pixelize");
    
    x = stx;
    for (j=0; j<nwide; j++, x+=pixX) {
      
      /* COMPUTE AVERAGE COLOR FOR RECT:[x,y,pixX,pixY] */
      nsum = rsum = gsum = bsum = 0;
      for (y1=y; y1<y+pixY; y1++) {
	pp = pic24 + (y1 * w + x) * 3;
	for (x1=x; x1<x+pixX; x1++) {
	  if (PTINRECT(x1,y1, selx,sely,selw,selh)) {
	    nsum++;
	    rsum += *pp++;  gsum += *pp++;  bsum += *pp++;
	  }
	}
      }
      
      if (nsum>0) {   /* just to be safe... */
	rsum /= nsum;  gsum /= nsum;  bsum /= nsum;
	RANGE(rsum,0,255);  RANGE(gsum,0,255);  RANGE(bsum,0,255);
      }
      
      
      /* STORE color in rect:[x,y,pixX,pixY] */
      for (y1=y; y1<y+pixY; y1++) {
	if (!j && (y1 & 255)==0) WaitCursor();
	
	pp = results + (y1 * w + x) * 3;
	for (x1=x; x1<x+pixX; x1++, pp+=3) {
	  if (PTINRECT(x1,y1, selx,sely,selw,selh)) {
	    pp[0] = (byte) rsum;  pp[1] = (byte) gsum;  pp[2] = (byte) bsum;
	  }
	}
      }
    }
  }

  printUTime("end of pixelize");
}

  

/************************/
static void doSpread(pic24, w, h, results, selx,sely,selw,selh, pixX, pixY)
     byte *pic24, *results;
     int   w, h, selx,sely,selw,selh, pixX,pixY;
{
  /* runs 'spread' algorithm.  For each pixel in the image, swaps it with
     a random pixel near it.  Distances of random pixels are controlled
     by pixX,pixY.  If pixX<0, it is treated as a single 'distance' value
     (after being abs()'d). */

  /* assumes that initially 'results' is a copy of pic24.  Doesn't 
     even look at pic24 */
  
  byte  *pp, *dp, r,g,b;
  int    x,y, dx,dy, x1,y1, d, xrng, xoff, yrng, yoff, i,j;
  int    minx, maxx, miny, maxy, rdist;
  time_t nowT;

  time(&nowT);
  srandom((unsigned int) nowT);
  
  printUTime("start of spread");

  for (y=sely; y<sely+selh; y++) {
    ProgressMeter(sely, sely+selh-1, y, "Spread");
    pp = results + (y * w + selx) * 3;
    for (x=selx; x<selx+selw; x++, pp+=3) {

      if (pixX < 0) {
	/* compute a random neighbor within radius 'd', cropped to 'sel' */

	d = abs(pixX);

	minx = x - d;	if (minx < selx)       minx = selx;
	maxx = x + d;	if (maxx >= selx+selw) maxx = selx+selw-1;
	x1 = minx + abs(random()) % ((maxx - minx) + 1);

	miny = y - d;	if (miny < sely)       miny = sely;
	maxy = y + d;	if (maxy >= sely+selh) maxy = sely+selh-1;

	rdist = d - abs(x1 - x);
	if (y - miny > rdist) miny = (y-rdist);
	if (maxy - y > rdist) maxy = (y+rdist);

	y1 = miny + abs(random()) % ((maxy - miny) + 1);
      }

      else {
	/* compute a neighbor within +/-pixX by +/-pixY, cropped to sel */

	minx = x - pixX;  if (minx < selx)       minx = selx;
	maxx = x + pixX;  if (maxx >= selx+selw) maxx = selx+selw-1;
	x1 = minx + abs(random()) % ((maxx - minx) + 1);

	miny = y - pixY;  if (miny < sely)       miny = sely;
	maxy = y + pixY;  if (maxy >= sely+selh) maxy = sely+selh-1;
	y1 = miny + abs(random()) % ((maxy - miny) + 1);
      }

      if (PTINRECT(x1,y1, selx,sely,selw,selh)) {  /* should always be true */
	dp = results + (y1 * w + x1) * 3;
	r     = pp[0];  g     = pp[1];  b     = pp[2];
	pp[0] = dp[0];  pp[1] = dp[1];  pp[2] = dp[2];
	dp[0] = r;      dp[1] = g;      dp[2] = b;
      }
    }
  }
  printUTime("end of spread");
}

  

/************************/
static void doMedianFilter(pic24, w, h, results, selx,sely,selw,selh, n)
     byte *pic24, *results;
     int   w,h, selx,sely,selw,selh, n;
{
  /* runs the median filter algorithm
     Operates on rectangular region 'selx,sely,selw,selh' (in pic coords)
     Region is guaranteed to be completely within pic boundaries
     'n' must be odd */

  register byte *p24;
  register int   rsum,gsum,bsum;
  byte          *rp;
  int            i,j,k,x,y,x1,y1,count,n2,nsq,c2;
  int           *rtab, *gtab, *btab;

  printUTime("start of doMedianFilter");

  n2 = n/2;  nsq = n * n;

  rtab = (int *) malloc(nsq * sizeof(int));
  gtab = (int *) malloc(nsq * sizeof(int));
  btab = (int *) malloc(nsq * sizeof(int));
  if (!rtab || !gtab || !btab) FatalError("can't malloc in doMedianFilter!");

  for (y=sely; y<sely+selh; y++) {
    ProgressMeter(sely, (sely+selh)-1, y, "DeSpeckle");
    if ((y & 15) == 0) WaitCursor();

    p24 = pic24   + y*w*3 + selx*3;
    rp  = results + y*w*3 + selx*3;

    for (x=selx; x<selx+selw; x++) {

      rsum = gsum = bsum = 0;  count = 0;

      for (y1=y-n2; y1<=y+n2; y1++) {

	if (y1>=sely && y1<sely+selh) {
	  p24 = pic24 + y1*w*3 +(x-n2)*3; 

	  for (x1=x-n2; x1<=x+n2; x1++) {
	    if (x1>=selx && x1<selx+selw) {
	      rtab[count] = *p24++;
	      gtab[count] = *p24++;
	      btab[count] = *p24++;
	      count++;
	    }
	    else p24 += 3;
	  }
	}
      }


      /* now sort the rtab,gtab,btab arrays, (using shell sort) 
	 and pick the middle value.  doing it in-line, rather than 
	 as a function call (ie, 'qsort()') , for speed */
      {  
	int i,j,t,d;
	
	for (d=count/2;  d>0;  d=d/2) {
	  for (i=d; i<count; i++) {
	    for (j=i-d;  j>=0 && rtab[j]>rtab[j+d];  j-=d) {
	      t = rtab[j];  rtab[j] = rtab[j+d];  rtab[j+d] = t;
	    }

	    for (j=i-d;  j>=0 && gtab[j]>gtab[j+d];  j-=d) {
	      t = gtab[j];  gtab[j] = gtab[j+d];  gtab[j+d] = t;
	    }

	    for (j=i-d;  j>=0 && btab[j]>btab[j+d];  j-=d) {
	      t = btab[j];  btab[j] = btab[j+d];  btab[j+d] = t;
	    }
	  }
	}
      }
      
      c2 = count/2;
      *rp++ = (byte) ( (count&1) ? rtab[c2] : (rtab[c2] + rtab[c2-1])/2);
      *rp++ = (byte) ( (count&1) ? gtab[c2] : (gtab[c2] + gtab[c2-1])/2);
      *rp++ = (byte) ( (count&1) ? btab[c2] : (btab[c2] + btab[c2-1])/2);
    }
  }
  
  free(rtab);  free(gtab);  free(btab);
  printUTime("end of doMedianFilter");
}


#ifdef FOO
/***********************************************/
static void intsort(a, n)
     int *a, n;
{
  /* uses the shell-sort algorithm.  for the relatively small data sets 
     we'll be using, should be quicker than qsort() because of all the
     function calling overhead associated with qsort(). */

  int i,j,t,d;

  for (d=n/2;  d>0;  d=d/2) {
    for (i=d; i<n; i++) {
      for (j=i-d;  j>=0 && a[j]>a[j+d];  j-=d) {
	t = a[j];  a[j] = a[j+d];  a[j+d] = t;
      }
    }
  }
}
#endif /* FOO */


/***********************************************/
static int start24bitAlg(pic24, tmpPic)
     byte **pic24, **tmpPic;
{
  /* generates a 24-bit version of 'pic', if neccessary, and also mallocs
   * a pWIDE*pHIGH*3 24-bit output pic.  
   *
   * Returns '1' if there's some sort of screwup, '0' if cool
   */


  if (picType == PIC8) {
    *pic24 = Conv8to24(pic, pWIDE, pHIGH, rMap, gMap, bMap);
    if (!*pic24) { SetCursors(-1);  return 1; }
  }
  else *pic24 = pic;


  /* need to create another w*h*3 pic to hold results */
  *tmpPic = (byte *) calloc((size_t) (pWIDE * pHIGH * 3), (size_t) 1);
  if (!(*tmpPic)) {
    SetCursors(-1);
    ErrPopUp("Unable to malloc() tmp 24-bit image in start24bitAlg()", 
	     "\nTough!");
    if (picType == PIC8) free(*pic24);
    return 1;
  }

  return 0;
}



/***********************************************/
static void end24bitAlg(pic24, outPic)
     byte *pic24, *outPic;
{
  /* given pic24, and outPic, which has the new 24-bit image, installs it */


  saveOrigPic();  /* also kills pic/cpic/epic/egampic/theImage, NOT pic24 */

  /* copy results to pic24 */
  xvbcopy((char *) outPic, (char *) pic24, (size_t) (pWIDE*pHIGH*3)); 
  free(outPic);

  if (picType == PIC8) {
    pic = Conv24to8(pic24, pWIDE, pHIGH, ncols, rMap,gMap,bMap);
    free(pic24);
    if (!pic) { 
      SetCursors(-1);
      ErrPopUp("Some sort of failure occured in 24to8 conversion\n","\nDamn!");
      NoAlg(); 
      return;
    }
  }
  else pic = pic24;

  InstallNewPic();
}


/************************/
static void saveOrigPic()
{
  /* saves original picture into origPic, if it hasn't already been done.
     This allows us to undo algorithms...  

     Also, frees all pics, (except 'pic', if we're in PIC24 mode) */


  int i;

  FreeEpic();
  if (cpic && cpic != pic) free(cpic);
  xvDestroyImage(theImage);
  theImage = NULL;
  cpic = NULL;

  if (!origPic) {
    /* make a backup copy of 'pic' */
    origPic = (byte *) malloc((size_t)(pWIDE*pHIGH*((picType==PIC8) ? 1 : 3)));
    if (!origPic) FatalError("out of memory in 'saveOrigPic()'");
    xvbcopy((char *) pic, (char *) origPic, 
	    (size_t) (pWIDE * pHIGH * ((picType==PIC8) ? 1 : 3)));

    origPicType = picType;

    if (picType == PIC8) {
      for (i=0; i<256; i++) {   /* save old colormap */
	origrmap[i] = rorg[i];
	origgmap[i] = gorg[i];
	origbmap[i] = borg[i];
      }
    }
  }


  if (picType != PIC24) {  /* kill pic, as well */
    if (pic) free(pic);
    pic = NULL;
  }
}



