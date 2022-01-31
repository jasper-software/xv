/*
 * bggen.c  -  a program that generates backgrounds for use with XV
 *
 * by John Bradley
 *
 *      Rev: 8/31/90   -  initial version
 *      Rev: 10/17/90  -  added '-w' option
 *      Rev: 7/15/93   -  added Xlib code (by PeterVG (pvgunm@polaris.unm.edu))
 *      Rev: 7/7/94    -  added '-G' option
 */



#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <math.h>

#ifdef __STDC__
#  include <stdlib.h>	/* atoi() */
#  include <ctype.h>	/* isdigit() */
#endif

#ifndef M_PI
#  define M_PI       3.1415926535897932385
#endif
#ifndef M_PI_2
#  define M_PI_2     1.5707963267948966192
#endif

#define DEFSIZE 1024
#define MAXCOLS  128

/* some VMS thing... */
#if defined(vax11c) || (defined(__sony_news) && (defined(bsd43) || defined(__bsd43) || defined(SYSTYPE_BSD) || defined(__SYSTYPE_BSD)))
#include <ctype.h>
#endif


#undef PARM
#ifdef __STDC__
#  define PARM(a) a
#else
#  define PARM(a) ()
#endif

#define RANGE(a,b,c) { if (a < b) a = b;  if (a > c) a = c; }


typedef unsigned char byte;

struct   color { int   r,g,b;
		 int   y;
	       } colors[MAXCOLS], *cur, *nex;

int      numcols;
int      bmask[8] = { 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

Display *theDisp;


int    main        PARM((int, char **));
void   usage       PARM((void));
void   dorot       PARM((byte *, int, int, int));
double computeDist PARM((int, int, int, int, int));
void   writePPM    PARM((byte *, int, int, int));


/*************************************/
int main(argc,argv)
     int    argc;
     char **argv;
{
  byte *pic24, *rpic24, *pp, *sp;
  int   i,j,cnt,x,y;
  int   high = DEFSIZE;
  int   wide = 1;
  int   bits = 8;
  int   rptwide, rpthigh;
  int   r, g, b, rot = 0;
  int   wset = 0, hset = 0, doascii=0;
  char *dname   = NULL;
  char *geom    = NULL;
  char *rptgeom = NULL;


#ifdef VMS
  getredirection(&argc, &argv);
#endif


  for (i=1; i<argc; i++) {
    if (!strncmp(argv[i],"-d",(size_t) 2)) {         /* -d disp */
      i++;  if (i<argc) dname = argv[i];
    }
  }

  if ((theDisp = XOpenDisplay(dname)) == NULL) {
    fprintf(stderr,"bggen:  Warning - can't open display, screen %s",
	    "size unknown, color names not accepted.\n");
  }


  cnt = 0;  numcols = 0;

  /* parse cmd-line args */
  for (i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-h")) {                          /* -h high */
      i++;  if (i<argc) high = atoi(argv[i]);
      hset++;
    }

    else if (!strcmp(argv[i],"-w")) {                     /* -w wide */
      i++;  if (i<argc) wide = atoi(argv[i]);
      wset++;
    }

    else if (!strcmp(argv[i],"-b")) {                     /* -b bits */
      i++;  if (i<argc) bits = atoi(argv[i]);
    }

    else if (!strncmp(argv[i],"-g",(size_t) 2)) {         /* -g geom */
      i++;  if (i<argc) geom = argv[i];
    }

    else if (!strncmp(argv[i],"-d",(size_t) 2)) {         /* -d disp */
      i++;  if (i<argc) dname = argv[i];
    }

    else if (!strcmp(argv[i],"-G")) {                      /* -G rptgeom */
      i++;  if (i<argc) rptgeom = argv[i];
    }

    else if (!strncmp(argv[i],"-a",(size_t) 2)) doascii++;  /* -a */

    else if (!strcmp(argv[i],"-r")) {                     /* -r rot */
      i++;  if (i<argc) rot = atoi(argv[i]);
    }

    else if (argv[i][0]=='-') usage();    /* any other '-' option is unknown */

    else if (isdigit(argv[i][0])) {
      switch (cnt) {
      case 0:  colors[numcols].r = atoi(argv[i]);  break;
      case 1:  colors[numcols].g = atoi(argv[i]);  break;
      case 2:  colors[numcols].b = atoi(argv[i]);  break;
      }
      cnt++;

      if (cnt==3) {
	if (numcols<MAXCOLS) numcols++;
	cnt = 0;
      }
    }

    else {                               /* color name */
      if (!theDisp) {
	fprintf(stderr,"%s: '%s' - no color names accepted\n",argv[0],argv[i]);
      }
      else {
	int      theScreen;
	Colormap theCmap;
	XColor   ecdef, sdef;

	theScreen = DefaultScreen(theDisp);
	theCmap   = DefaultColormap(theDisp, theScreen);

	if (XLookupColor(theDisp, theCmap, argv[i], &ecdef, &sdef)) {
	  colors[numcols].r = (ecdef.red   >> 8) & 0xff;
	  colors[numcols].g = (ecdef.green >> 8) & 0xff;
	  colors[numcols].b = (ecdef.blue  >> 8) & 0xff;
	  if (numcols<MAXCOLS) numcols++;
	  cnt = 0;
	}
	else {
	  fprintf(stderr,"%s: color '%s' unknown.  skipped.\n",
		  argv[0], argv[i]);
	}
      }
    }
  }



  /* print error/usage message, if appropriate */
  if (cnt || numcols==0 || high<1 || wide<1 || bits<1 || bits>8) usage();


  if (geom) {
    int x,y;  unsigned int w,h;
    i = XParseGeometry(geom, &x, &y, &w, &h);
    if (i&WidthValue)  { wset++;  wide = (int) w; }
    if (i&HeightValue) { hset++;  high = (int) h; }
  }


  /* attempt to connect to X server and get screen dimensions */
  if (theDisp) {
    i = DefaultScreen(theDisp);
    if (!wset) wide = DisplayWidth(theDisp, i);
    if (!hset) high = DisplayHeight(theDisp, i);
  }


  /* normalize 'rot' */
  while (rot<   0) rot += 360;
  while (rot>=360) rot -= 360;


  rptwide = wide;  rpthigh = high;
  if (rptgeom) {
    int x,y;  unsigned int w,h;
    i = XParseGeometry(rptgeom, &x, &y, &w, &h);
    if (i&WidthValue)  rptwide = (int) w;
    if (i&HeightValue) rpthigh = (int) h;

    RANGE(rptwide, 1, wide);
    RANGE(rpthigh, 1, high);
  }




  rpic24 = (byte *) malloc(rptwide * rpthigh * 3 * sizeof(byte));
  if (rptwide != wide || rpthigh != high)
    pic24  = (byte *) malloc(wide * high * 3 * sizeof(byte));
  else pic24 = rpic24;

  if (!pic24 || !rpic24) {
    fprintf(stderr,"%s:  couldn't alloc space for %dx%d 24-bit image!\n",
	    argv[0], wide, high);
    exit(1);
  }
  for (i=0, pp=pic24; i<wide*high*3; i++) *pp++ = 0;



  /*** generate image ***/

  if (numcols==1) {                /* solid color:  trivial case */
    r = colors[0].r;  g=colors[0].g;  b=colors[0].b;

    pp = pic24;
    for (i=0; i<high; i++) {
      for (j=0; j<wide; j++) {
	*pp++ = (byte) r;  *pp++ = (byte) g;  *pp++ = (byte) b;
      }
    }
  }


  else if (rot==0) {   /* un-rotated linear (vertical) gradient */
    for (i=0; i<numcols; i++)
      colors[i].y = ((rpthigh-1) * i) / (numcols-1);

    cur = &colors[0];  nex = cur+1;

    for (i=0; i<rpthigh; i++) {
      pp = rpic24 + (i * rptwide * 3);

      /* advance to next pair of colors if we're outside region */
      while (nex->y < i) { cur++; nex++; }

      r = cur->r + ((nex->r - cur->r) * (i - cur->y)) / (nex->y - cur->y);
      g = cur->g + ((nex->g - cur->g) * (i - cur->y)) / (nex->y - cur->y);
      b = cur->b + ((nex->b - cur->b) * (i - cur->y)) / (nex->y - cur->y);

      r = r & bmask[bits-1];
      g = g & bmask[bits-1];
      b = b & bmask[bits-1];

      for (j=0; j<rptwide; j++) {
	*pp++ = (byte) r;  *pp++ = (byte) g;  *pp++ = (byte) b;
      }
    }
  }

  else dorot(rpic24, rptwide, rpthigh, rot);



  /* do 'center-tiling' ala root window, if necessary */

  if (rptwide != wide || rpthigh != high) {
    int ax, ay;

    ax = (wide - rptwide)/2;  ay = (high - rpthigh)/2;
    while (ax>0) ax = ax - rptwide;
    while (ay>0) ay = ay - rpthigh;

    pp = pic24;
    for (i=0; i<high; i++) {
      for (j=0; j<wide; j++, pp+=3) {

	x = ((j-ax) % rptwide);
	y = ((i-ay) % rpthigh);

	sp = rpic24 + (y * rptwide + x) * 3;

	pp[0] = *sp++;  pp[1] = *sp++;  pp[2] = *sp++;
      }
    }
  }


  writePPM(pic24, wide, high, doascii);
  return 0;
}



static double theta, tant1, cost1, cost, sint;
static int distdebug;


/*************************************/
void usage()
{
  fprintf(stderr,"usage:  bggen [-h high] [-w wide] [-b bits] [-g geom]\n");
  fprintf(stderr,"\t\t[-d disp] [-a] [-r rot] [-G rptgeom] \n");
  fprintf(stderr,"\t\tr1 g1 b1 [r2 g2 b2 ...]\n\n");
  exit(1);
}


/*************************************/
void dorot(pic, w, h, rot)
     byte *pic;
     int   w, h, rot;
{
  byte   *pp;
  double maxd, mind, del, d, rat, crat, cval;
  int    x, y, cx, cy, r,g,b, bc, nc1;

  distdebug = 0;
  cx = w/2;  cy = h/2;
  theta = (double) rot * M_PI / 180.0;
  tant1 = tan(M_PI_2 - theta);
  cost1 = cos(M_PI_2 - theta);
  cost  = cos(theta);
  sint  = sin(theta);
  nc1 = numcols - 1;

  /* compute max/min distances */
  if (rot>0 && rot<90) {
    mind = computeDist(0,    0,    cx, cy, rot);
    maxd = computeDist(w-1,  h-1,  cx, cy, rot);
  }
  else if (rot>=90 && rot<180) {
    mind = computeDist(0,    h-1,  cx, cy, rot);
    maxd = computeDist(w-1,  0,    cx, cy, rot);
  }
  else if (rot>=180 && rot<270) {
    mind = computeDist(w-1,  h-1,  cx, cy, rot);
    maxd = computeDist(0,    0,    cx, cy, rot);
  }
  else {
    mind = computeDist(w-1,  0,    cx, cy, rot);
    maxd = computeDist(0,    h-1,  cx, cy, rot);
  }

  del = maxd - mind;         /* maximum distance */

  distdebug = 0;


  for (y=0; y<h; y++) {
    pp = pic + (y * w * 3);
    for (x=0; x<w; x++) {
      d = computeDist(x,y, cx,cy, rot);
      rat = (d - mind) / del;
      if (rat<0.0) rat = 0.0;
      if (rat>1.0) rat = 1.0;

      cval = rat * nc1;
      bc   = floor(cval);
      crat = cval - bc;

      if (bc < nc1) {
	r = colors[bc].r + crat * (colors[bc+1].r - colors[bc].r);
	g = colors[bc].g + crat * (colors[bc+1].g - colors[bc].g);
	b = colors[bc].b + crat * (colors[bc+1].b - colors[bc].b);
      }
      else {
	r = colors[nc1].r;
	g = colors[nc1].g;
	b = colors[nc1].b;
      }

      *pp++ = (byte) r;  *pp++ = (byte) g;  *pp++ = (byte) b;
    }
  }
}



double computeDist(x, y, cx, cy, rot)
     int x,y,cx,cy,rot;
{
  /* rot has to be in range 0-359 */

  double x1, y1, x2, y2, x3, y3, d, d1, b;

  if (rot == 0)   return (double) (y - cy);
  if (rot == 180) return (double) (cy - y);

  /* x1,y1 = original point */
  x1 = (double) x;  y1 = (double) y;

  /* x2,y2 = vertical projection onto a || line that runs through cx,cy */
  x2 = x1;
  y2 = cy - (cx-x2)*tant1;

  d1 = y2 - y1;  /* vertical distance between lines */
  b  = d1 * cost1;

  /* x3,y3 = cx,cy projected perpendicularly onto line running throuh x1,y1 */
  x3 = cx + b * cost;
  y3 = cy - b * sint;

  d = sqrt((x1-x3) * (x1-x3) + (y1-y3) * (y1-y3));
  if ((rot<180 && x1<x3) || (rot>180 && x1>x3)) d = -d;

  if (distdebug) {
    fprintf(stderr,"Input: %d,%d (center=%d,%d), rot=%d\n", x,y,cx,cy,rot);
    fprintf(stderr,"point2 = %f,%f  point3 = %f,%f\nd1=%f  b=%f  d=%f\n\n",
	    x2,y2,x3,y3,d1,b,d);
  }

  return d;
}



/******************************/
void writePPM(pic, w, h, doascii)
     byte *pic;
     int   w,h,doascii;
{
  /* dumps a pic24 in PPM format to stdout */

  int x,y;

  printf("P%s %d %d 255\n", (doascii) ? "3" : "6", w, h);

  for (y=0; y<h; y++) {
    if (doascii) {
      for (x=0; x<w; x++, pic+=3)
	printf("%d %d %d\n", pic[0],pic[1],pic[2]);
    }
    else {
      for (x=0; x<w; x++, pic+=3) {
	putchar(pic[0]);  putchar(pic[1]);  putchar(pic[2]);
      }
    }
  }
}


