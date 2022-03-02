/*
 * xvmask.c - image mask algorithms (FLmask, etc.)
 *
 *  Contains:
 *         void DoMask(int algnum);
 */

#include "copyright.h"
#include "xv.h"

/* Flmask */
void cpcode 		PARM ((char *, unsigned char *, int));
void MaskCr 		PARM ((void));
static void FLmask 	PARM ((void));
static void Q0mask 	PARM ((void));
static void WINmask 	PARM ((void));
static void MEKOmask 	PARM ((void));
static void CPmask 	PARM ((void));
static void RGBchange 	PARM ((void));
static void BitReverse 	PARM ((void));
static void ColReverse 	PARM ((void));

/***** Flmask *****/
static void doMaskCr 		PARM ((byte *, int, int, int, int, int, int));
static void doFLmask 		PARM ((byte *, int, int, byte *, int, int, int, int));
static void doCPmask 		PARM ((byte *, int, int, byte *, int, int, int, int, char *));
static void doMEKOmask 		PARM ((byte *, int, int, byte *, int, int, int, int, int));
static void doColReverse 	PARM ((byte *, int, int, byte *, int, int, int, int, int));
static void doRGBchange 	PARM ((byte *, int, int, byte *, int, int, int, int));
static void doQ0mask 		PARM ((byte *, int, int, byte *, int, int, int, int, int, int));
static int *calcFLmask 		PARM ((int, int));
static void move8bit 		PARM ((byte *, byte *, int));
static void move16bit 		PARM ((byte *, byte *, int));
static void moveCP 		PARM ((byte *, byte *, int, int));
static void doWINmask 		PARM ((byte *, int, int, byte *, int, int, int, int));
static void wincp 		PARM ((int, int, byte *, byte *));

/******************/

int start24bitAlg 		PARM ((byte **, byte **));
void end24bitAlg 		PARM ((byte *, byte *));
void saveOrigPic 		PARM ((void));
void printUTime 		PARM ((char *));

#undef TIMING_TEST

#ifdef TIMING_TEST
#include <sys/time.h>
#include <sys/resource.h>
#endif

/************************/
void
DoMask (anum)
     int anum;
{
    switch (anum)
    {
    case MSK_NONE: 	DoAlg (ALG_NONE); 	break;
    case MSK_FLMASK: 	FLmask (); 		break;
    case MSK_Q0MASK: 	Q0mask (); 		break;
    case MSK_WIN: 	WINmask (); 		break;
    case MSK_MEKO: 	MEKOmask (); 		break;
    case MSK_CPMASK: 	CPmask (); 		break;
    case MSK_RGB: 	RGBchange (); 		break;
    case MSK_BITREV: 	BitReverse (); 		break;
    case MSK_COLREV: 	ColReverse (); 		break;
    }

    algMB.dim[ALG_NONE] = (origPic == (byte *) NULL);
    flmaskMB.dim[ALG_NONE] = (origPic == (byte *) NULL);
}


/******************************
      Flmask:  FLMASK.
******************************/
static void
FLmask ()
{
    byte *pic24, *tmpPic;
    char *str;
    int sx, sy, sw, sh;

    WaitCursor ();

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    str = "Doing FLMASK...";
    SetISTR (ISTR_INFO, str);

    if (start24bitAlg (&pic24, &tmpPic))
	return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doFLmask (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh);

    end24bitAlg (pic24, tmpPic);
}

/******************************
      Flmask:  BitReverse.
******************************/
static void
BitReverse ()
{
    byte *pic24, *tmpPic;
    char *str;
    int sx, sy, sw, sh;

    WaitCursor ();

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    str = "Doing reverse bit...";
    SetISTR (ISTR_INFO, str);

    if (start24bitAlg (&pic24, &tmpPic))
	return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doColReverse (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh, 1);

    end24bitAlg (pic24, tmpPic);
}

/******************************
      Flmask:  ColReverse.
******************************/
static void
ColReverse ()
{
    byte *pic24, *tmpPic;
    char *str;
    int sx, sy, sw, sh;

    WaitCursor ();

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    str = "Doing reverse colormap...";
    SetISTR (ISTR_INFO, str);

    if (start24bitAlg (&pic24, &tmpPic))
	return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doColReverse (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh, 0);
    end24bitAlg (pic24, tmpPic);
}


/******************************
      Flmask:  Q0 MASK.
******************************/
static void
Q0mask ()
{
    int pixX, pixY, err;
    static const char *labels[] = {"\nOk", "\033Cancel"};
    char txt[256];
    static char buf[64] = {'8', '\0'};
    byte *pic24, *tmpPic;
    char *str;
    int i, sx, sy, sw, sh;

    sprintf (txt, "FLmask: Q0mask        \n\nEnter mask pixels size: %s\n", "(ex. '8', '8x16')");

    i = GetStrPopUp (txt, labels, 2, buf, 64, "0123456789x", 1);
    if (i == 1 || strlen (buf) == 0) return;

    pixX = pixY = err = 0;

    if (index (buf, 'x'))
    {
	if (sscanf (buf, "%d x %d", &pixX, &pixY) != 2) err++;
    }
    else
    {
	if (sscanf (buf, "%d", &pixX) != 1) err++;
	pixY = pixX;
    }

    if (pixX < 1 || pixY < 1 || err)
    {
	ErrPopUp ("Error:  The entered string is not valid.",
		  "\nWho Cares!");
	return;
    }
    WaitCursor ();

    str = "Doing Q0 MASK...";
    SetISTR (ISTR_INFO, str);

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    if (start24bitAlg (&pic24, &tmpPic)) return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doQ0mask (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh, pixX, pixY);
    end24bitAlg (pic24, tmpPic);
}

/******************************
      Flmask:  RGBchange.
******************************/
static void
RGBchange ()
{
    byte *pic24, *tmpPic;
    char *str;
    int sx, sy, sw, sh;

    WaitCursor ();

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    str = "Doing RGB change...";
    SetISTR (ISTR_INFO, str);

    if (start24bitAlg (&pic24, &tmpPic)) return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doRGBchange (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh);

    end24bitAlg (pic24, tmpPic);
}

/******************************
      Flmask:  WINmask.
******************************/
static void
WINmask ()
{
    byte *pic24, *tmpPic;
    char *str;
    int sx, sy, sw, sh;

    WaitCursor ();

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    str = "Doing WIN mask...";
    SetISTR (ISTR_INFO, str);

    if (start24bitAlg (&pic24, &tmpPic)) return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doWINmask (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh);

    end24bitAlg (pic24, tmpPic);
}

/******************************
      Flmask:  MEKOmask.
******************************/
static
void
MEKOmask ()
{
    static const char *labels[] = {"\nForward", "\nBackward"};
    char txt[256];
    byte *pic24, *tmpPic;
    char *str;
    int i, sx, sy, sw, sh;

    sprintf (txt, "FLmask: MEKO mask  \n\n    Select MEKOmask type ");
    i = PopUp (txt, labels, 2);

    WaitCursor ();
    str = "Doing MEKOmask...";
    SetISTR (ISTR_INFO, str);

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    if (start24bitAlg (&pic24, &tmpPic)) return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));
    doMEKOmask (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh, i);
    end24bitAlg (pic24, tmpPic);
}

/************************/
static void
doMEKOmask (pic24, w, h, results, selx, sely, selw, selh, flag)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh, flag;
{
    register byte *dst, *src;
    register int i;
    register int xmax, ymax;
    MKT *mt;

    printUTime ("start of MEKOmask.");

    xmax = selw / 16;
    ymax = selh / 16;

    mt = (MKT *) calcMEKO (xmax * ymax);
    if (mt == NULL)
	return;

    for (i = 0; i < xmax * ymax; i++)
    {
	ProgressMeter (1, (xmax * ymax) - 1, i, "MEKOmask");
	if ((i & 63) == 0)
	    WaitCursor ();

	if (flag == 0)
	{
	    dst = results + ((sely + ((mt[i].n - 1) / xmax) * 16) * w
			     + selx + ((mt[i].n - 1) % xmax) * 16) * 3;
	    src = pic24 + ((sely + (i / xmax) * 16) * w
			   + selx + (i % xmax) * 16) * 3;
	}
	else
	{
	    src = pic24 + ((sely + ((mt[i].n - 1) / xmax) * 16) * w
			   + selx + ((mt[i].n - 1) % xmax) * 16) * 3;
	    dst = results + ((sely + (i / xmax) * 16) * w
			     + selx + (i % xmax) * 16) * 3;
	}
	move16bit (dst, src, w);
    }

    free (mt);
    printUTime ("end of MEKOmask.");
}

static void
move16bit (byte * dest, byte * src, int w)
{
    int y, x;
    byte *tmp1, *tmp2;

    for (y = 0; y < 16; y++)
    {
	tmp1 = dest + y * w * 3;
	tmp2 = src + y * w * 3;
	for (x = 0; x < 16 * 3; x++)
	{
	    *tmp1 = *tmp2;
	    tmp1++;
	    tmp2++;
	}
    }
}

/******************************
      Flmask:  CP MASK.
******************************/
static void
CPmask ()
{
    static const char *labels[] = {"\nOk", "\033Cancel"};
    char buf[64];
    char txt[256];
    unsigned char key[48];
    byte *pic24, *tmpPic;
    char *str;
    int i, sx, sy, sw, sh;

    memset (key, 0, 48);
    cpcode (fullfname, key, 0);
    if (key[0] == 0)
    {
	memset (key, 0, 48);
	cpcode (fullfname, key, 1);
	if (key[0] == 0)
	{
	    memset (key, 0, 48);
	    cpcode (fullfname, key, 2);
	}
    }

    strcpy(buf,(char *)key);
    if (strlen(buf)==0)strcpy(buf,"SAMPLE");
    sprintf (txt, "FLmask: CPmask          \n\nEnter CPmask code: %s \n",
    	"(ex. 'SAMPLE')");

    i = GetStrPopUp (txt, labels, 2, buf, 64, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 1);
    if (i == 1 || strlen (buf) == 0) return; 
    strcpy((char *)key,buf);

    /*  Old version.
    sprintf (txt, "FLmask: CP mask  \n\n   CP code = %s ", key);
    i = PopUp (txt, labels, 2);
    if (i == 1) return;
    */

    WaitCursor ();
    str = "Doing CP mask...";
    SetISTR (ISTR_INFO, str);

    if (HaveSelection ()) GetSelRCoords (&sx, &sy, &sw, &sh);
    else { sx = 0; sy = 0; sw = pWIDE; sh = pHIGH; }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    if (start24bitAlg (&pic24, &tmpPic)) return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));
    doCPmask (pic24, pWIDE, pHIGH, tmpPic, sx, sy, sw, sh, (char *) key);
    end24bitAlg (pic24, tmpPic);
}

/************************/
static void
doCPmask (pic24, w, h, results, selx, sely, selw, selh, key)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh;
     char *key;
{
    register byte *dst, *src;
    register int i;
    register int xmax, ymax;
    CPS *cps;

    printUTime ("start of CPmask.");

    xmax = selw / 8;
    ymax = selh / 8;

    cps = (CPS *) calcCPmask (key, xmax * ymax);

    for (i = 0; i < xmax * ymax; i++)
    {
	ProgressMeter (1, (xmax * ymax) - 1, i, "CPmask");
	if ((i & 63) == 0) WaitCursor ();

	dst = results + ((sely + (cps[i].n / xmax) * 8) * w
			 + selx + (cps[i].n % xmax) * 8) * 3;
	src = pic24 + ((sely + (i / xmax) * 8) * w
		       + selx + (i % xmax) * 8) * 3;
	moveCP (dst, src, w, cps[i].flg);
    }

    free (cps);
    printUTime ("end of CPmask.");
}

static void
moveCP (byte * dest, byte * src, int w, int cp_flag)
{
    int y, x;
    byte *tmp1, *tmp2;

    if (cp_flag == 0)
    {
	for (y = 0; y < 8; y++)
	{
	    tmp1 = dest + y * w * 3;
	    tmp2 = src + y * w * 3;
	    for (x = 0; x < 8 * 3; x += 3)
	    {
		tmp1[0] = (byte) ~ tmp2[1];
		tmp1[1] = (byte) ~ tmp2[0];
		tmp1[2] = (byte) ~ tmp2[2];
		tmp1 += 3;
		tmp2 += 3;
	    }
	}
    }
    else
    {
	for (y = 0; y < 8; y++)
	{
	    tmp1 = dest + y * 3;
	    tmp2 = src + y * w * 3;
	    for (x = 0; x < 8 * 3; x += 3)
	    {
		tmp1[0] = (byte) ~ tmp2[1];
		tmp1[1] = (byte) ~ tmp2[0];
		tmp1[2] = (byte) ~ tmp2[2];
		tmp1 += w * 3;
		tmp2 += 3;
	    }
	}
    }
}

/************************/
static void
doFLmask (pic24, w, h, results, selx, sely, selw, selh)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh;
{
    register byte *dst, *src;
    register int i;
    register int *ar, xmax, ymax;

    printUTime ("start of FLMASK.");

    xmax = selw / 8;
    ymax = selh / 8;
    ar = (int *) calcFLmask (xmax, ymax);

    for (i = 0; i < xmax * ymax; i++)
    {
	ProgressMeter (1, (xmax * ymax * 2) - 1,
		i + xmax * ymax, "FLMASK");
	if ((i & 63) == 0) WaitCursor ();

	dst=results+((sely+(ar[xmax*ymax-i-1]/xmax)*8)*w
		+selx+(ar[xmax*ymax-i-1]%xmax)*8)*3;
	src=pic24+((sely+(ar[i]/xmax)*8)*w
		+selx+(ar[i]%xmax)*8)*3;
	if (ar[i] != ar[xmax * ymax - i - 1])
	    move8bit (dst, src, w);
    }

    free (ar);
    printUTime ("end of FLMASK.");
}

static int *
calcFLmask (int xmax, int ymax)
{
    int *spc, *tmp_ar;
    int i, l, x, y, c;
    struct direction { int x; int y; } dir[4];

    dir[0].x = 0;  dir[0].y = -1;
    dir[1].x = -1; dir[1].y = 0;
    dir[2].x = 0;  dir[2].y = 1;
    dir[3].x = 1;  dir[3].y = 0;

    spc = (int *) malloc (sizeof (int) * xmax * ymax);
    tmp_ar = (int *) malloc (sizeof (int) * xmax * ymax);

    c = 0;
    for (l = 0; l < ymax; l++)
    {
	for (i = 0; i < xmax; i++)
	{
	    spc[c] = c;
	    c++;
	}
    }

    c = 3;
    x = 0;
    y = ymax - 1;
    for (i = 0; i < xmax * ymax; i++)
    {
	ProgressMeter (1, (xmax * ymax * 2) - 1, i, "FLmask");
	if ((i & 63) == 0) WaitCursor ();

	tmp_ar[i] = spc[x + y * xmax];
	if ((x + dir[c].x) == xmax || (x + dir[c].x) < 0)
	{
	    c++;
	    if (c == 4) c = 0;
	}
	if ((y + dir[c].y) == ymax || (y + dir[c].y) < 0)
	{
	    c++;
	    if (c == 4) c = 0;
	}
	for (l = 0; l < i; l++)
	{
		if(spc[x+(dir[c].x)+(y+(dir[c].y))*xmax]==tmp_ar[l])
	    {
		c++;
		if (c == 4) c = 0;
		break;
	    }
	}
	x = x + dir[c].x;
	y = y + dir[c].y;
    }
    free (spc);
    return (tmp_ar);
}

static void
move8bit (byte * dest, byte * src, int w)
{
    int y, x;
    byte *tmp1, *tmp2;

    for (y = 0; y < 8; y++)
    {
	tmp1 = dest + y * w * 3;
	tmp2 = src + y * w * 3;
	for (x = 0; x < 8 * 3; x++)
	{
	    *tmp1 = (byte) ~ (*tmp2);
	    tmp1++;
	    tmp2++;
	}
    }
}

/************************/
static void
doColReverse (pic24, w, h, results, selx, sely, selw, selh, bit_flag)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh, bit_flag;
{
    register byte *p24;
    register byte *rp;
    register int x, y;

    printUTime ("start of Reverse.");

    for (y = sely; y < sely + selh; y++)
    {

	ProgressMeter (sely + 1, (sely + selh - 1) - 1, y, "Reverse");
	if ((y & 63) == 0) WaitCursor ();

	rp = results + (y * w + selx) * 3;
	p24 = pic24 + (y * w + selx) * 3;

	if (bit_flag == 1)
	{
	    /* BitReverse. */
	    for (x = selx; x < selx + selw * 3; x++, p24++, rp++)
		*rp = (byte) ~ (*p24);

	}
	else
	{
	    /* ColReverse. */
	    for (x = selx; x < selx + selw * 3; x++, p24++, rp++)
		*rp = *p24 ^ 0x80;
	}
    }
    printUTime ("end of Reverse.");
}

/************************/
static void
doQ0mask (pic24, w, h, results, selx, sely, selw, selh, pixX, pixY)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh, pixX, pixY;
{
    register byte *p24;
    register int bperlin;
    byte *rp;
    int x, y;
    int skip, y0, x0;

    printUTime ("start of Q0mask.");

    bperlin = w * 3;

    for (y = sely; y < sely + ((selh / pixY) * pixY); y++)
    {
	ProgressMeter(sely+1,(sely+((selh/pixY)*pixY))-1,y,"Q0mask");
	if ((y & 63) == 0) WaitCursor ();

	rp = results + (y * w + selx) * 3;
	y0 = y - sely;
	p24=pic24+((((y0/pixY+1)*pixY)-1-(y0%pixY)+sely)*w+selx)*3;

	for(x=selx;x<selx+((selw/pixX)*pixX);x++,p24+=3,rp+=3)
	{
	    x0 = x - selx;
	    skip = (pixX - 1 - 2 * (x0 % pixX)) * 3;

	    rp[0] = (byte) ~ p24[skip];
	    rp[1] = (byte) ~ p24[skip + 1];
	    rp[2] = (byte) ~ p24[skip + 2];
	}
    }
    printUTime ("end of Q0 MASK.");
}

/************************/
static void
doWINmask (pic24, w, h, results, selx, sely, selw, selh)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh;
{
    register byte *p24;
    register byte *rp;
    register int x, y;

    printUTime ("start of WIN mask.");

    for (y = sely; y < sely + selh; y++)
    {

	ProgressMeter (sely + 1, (sely + selh - 1) - 1, y, "WINmask");
	if ((y & 63) == 0) WaitCursor ();

	rp = results + (y * w + selx) * 3;
	p24 = pic24 + (y * w + selx) * 3;

	for(x=selx;x<selx+(selw/16)*16*3;x+=48,p24+=48,rp+=48)
	{
	    wincp ( 0,12, p24, rp);
	    wincp ( 1, 8, p24, rp);
	    wincp ( 2, 6, p24, rp);
	    wincp ( 3,15, p24, rp);
	    wincp ( 4, 9, p24, rp);
	    wincp ( 5,13, p24, rp);
	    wincp ( 6, 2, p24, rp);
	    wincp ( 7,11, p24, rp);
	    wincp ( 8, 1, p24, rp);
	    wincp ( 9, 4, p24, rp);
	    wincp (10,14, p24, rp);
	    wincp (11, 7, p24, rp);
	    wincp (12, 0, p24, rp);
	    wincp (13, 5, p24, rp);
	    wincp (14,10, p24, rp);
	    wincp (15, 3, p24, rp);
	}
    }
    printUTime ("end of WIN mask.");
}

static void
wincp (int src, int dst, byte * p24, byte * rp)
{
    *(rp + dst * 3) = *(p24 + src * 3);
    *(rp + dst * 3 + 1) = *(p24 + src * 3 + 1);
    *(rp + dst * 3 + 2) = *(p24 + src * 3 + 2);
}

/************************/
static void
doRGBchange (pic24, w, h, results, selx, sely, selw, selh)
     byte *pic24, *results;
     int w, h, selx, sely, selw, selh;
{
    register byte *p24;
    register byte *rp;
    register int x, y;

    printUTime ("start of RGB change.");

    for (y = sely; y < sely + selh; y++)
    {

	ProgressMeter (sely + 1, (sely + selh - 1) - 1, y, "Change");
	if ((y & 63) == 0) WaitCursor ();

	rp = results + (y * w + selx) * 3;
	p24 = pic24 + (y * w + selx) * 3;

	for (x = selx; x < selx + selw * 3; x += 3, p24 += 3, rp += 3)
	{
	    *rp = *(p24 + 1);
	    *(rp + 1) = *(p24 + 2);
	    *(rp + 2) = *(p24);
	}
    }
    printUTime ("end of Reverse.");
}


/******************************
      Flmask:  MaskCrop.
******************************/
void
MaskCr ()
{
    byte *pic24, *tmpPic;
    char str[] = "Doing AutoMaskCrop...";
    int sx, sy, sw, sh;

    WaitCursor ();

    if (HaveSelection ())
	GetSelRCoords (&sx, &sy, &sw, &sh);
    else
    {
	sx = 0;
	sy = 0;
	sw = pWIDE - 1;
	sh = pHIGH - 1;
    }

    CropRect2Rect (&sx, &sy, &sw, &sh, 0, 0, pWIDE, pHIGH);

    SetISTR (ISTR_INFO, str);

    if (start24bitAlg (&pic24, &tmpPic)) return;
    xvbcopy ((char *) pic24, (char *) tmpPic,
	     (size_t) (pWIDE * pHIGH * 3));

    doMaskCr (pic24, pWIDE, pHIGH, sx, sy, sw, sh);

    end24bitAlg (pic24, tmpPic);
}

/************************/
static void
doMaskCr (pic24, w, h, selx, sely, selw, selh)
     byte *pic24;
     int w, h, selx, sely, selw, selh;
{
    register byte *p24, *p24u;
    register int x, y;
    double edge, *edgeX, *edgeY, maxX, maxY, tmp;
    int x1, x2, y1, y2;
    int xp1, xp2, yp1, yp2;

    printUTime ("start of MaskCrop.");

    edgeX = (double *) malloc (sizeof (edge) * selw);
    edgeY = (double *) malloc (sizeof (edge) * selh);
    memset (edgeX, 0, sizeof (edge) * selw);
    memset (edgeY, 0, sizeof (edge) * selh);

    for (y = sely + 1; y < sely + selh - 1; y++)
    {
	ProgressMeter (sely + 1, (sely + selh - 1) - 1, y, "Search");
	if ((y & 63) == 0) WaitCursor ();

	p24u = pic24 + ((y - 1) * w + selx) * 3;
	p24 = pic24 + (y * w + selx) * 3;

#define DBL(x)	((x)*(x))
	for (x = selx; x < selx + selw * 3; x += 3)
	{
	    edgeX[(x - selx) / 3] += (double) DBL (p24[3] - p24[0]) +
		DBL (p24[4] - p24[1]) + DBL (p24[5] - p24[2]);
	    edgeY[y - sely] += (double) DBL (p24[0] - p24u[0]) +
		DBL (p24[1] - p24u[1]) + DBL (p24[2] - p24u[2]);
	    p24u += 3;
	    p24 += 3;
	}
    }

    maxX = 0;
    for (x = 0; x < selw; x++)
    {
	if (maxX < edgeX[x])
	    maxX = edgeX[x];
    }
    maxY = 0;
    for (y = 0; y < selh; y++)
    {
	if (maxY < edgeY[y])
	    maxY = edgeY[y];
    }

    tmp = 0.3;	/* ←↓この辺の数値は適当に決定されました(?)。 */
    for (;;)
    {
	xp1 = 0; xp2 = 0;
	yp1 = 0; yp2 = 0;
	for (x = 1; x < selw - 1; x++) {
	    if (edgeX[x] > maxX * tmp) { xp1 = x; break; }
	}
	for (x = selw - 1; x >= 1; x--) {
	    if (edgeX[x] > maxX * tmp) { xp2 = x; break; }
	}
	for (y = 1; y < selh - 1; y++) {
	    if (edgeY[y] >= maxY * tmp) { yp1 = y; break; }
	}
	for (y = selh - 1; y >= 1; y--) {
	    if (edgeY[y] >= maxY * tmp) { yp2 = y; break; }
	}
	if(xp2-xp1 > 4 && yp2-yp1 > 4 && xp1*xp2*yp1*yp2 != 0) break;

	if (tmp <= 0.01) return;
	else tmp = tmp / 2.0;
    }

    free (edgeY);
    free (edgeX);

    x1 = xp1 + selx + 1;
    x2 = xp2 - xp1;
    y1 = yp1 + sely;
    y2 = yp2 - yp1;

    /* crop1(x1,y1,x2,y2,0);  <-- DO_CROP  */
    MaskSelect (x1, x2, y1, y2);

    printUTime ("end of MaskSearch.");
}
