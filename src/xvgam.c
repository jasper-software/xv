/*
 * xvgam.c
 *
 * callable functions:
 *   <too many to list>
 */

#include "copyright.h"

#include "xv.h"

#include "bits/h_rotl"
#include "bits/h_rotr"
#include "bits/h_flip"
#include "bits/h_sinc"
#include "bits/h_sdec"
#include "bits/h_sat"
#include "bits/h_desat"


#define GAMW 664
#define GAMH 518

#define GAMbutF 386

#define CMAPX  10
#define CMAPY  17
#define CMAPCW 12
#define CMAPCH  9
#define CMAPW  (CMAPCW * 16)
#define CMAPH  (CMAPCH * 16)

#define MAXUNDO 32

#define BUTTH   23
#define N_HMAP   6     /* # of Hue modification remappings */

#define N_HDBUTT 5
#define HDB_ROTL  0
#define HDB_ROTR  1
#define HDB_EXPND 2
#define HDB_SHRNK 3
#define HDB_FLIP  4

#define N_HDBUTT2 4
#define HDB_DESAT 2
#define HDB_SAT   3


#define HD_CLEAR  0x01   /* clears inside of hue dial */
#define HD_FRAME  0x02
#define HD_HANDS  0x04
#define HD_DIR    0x08
#define HD_VALS   0x10
#define HD_TITLE  0x20
#define HD_CLHNDS 0x40
#define HD_BUTTS  0x80
#define HD_ALL   (HD_FRAME | HD_HANDS | HD_DIR | HD_VALS | HD_TITLE | HD_BUTTS)

#define HD_RADIUS 30   /* radius of bounding circle of HDIALs */

#define DEG2RAD (3.14159 / 180.0)
#define RAD2DEG (180.0 / 3.14159)



/* stuff for colormap Undo button */
struct cmapstate { byte  rm[256], gm[256], bm[256];
		   int cellgroup[256];
		   int curgroup;
		   int maxgroup;
		   int editColor;
		 } prevcmap, tmpcmap;

struct hmap {    int src_st;
		 int src_en;
		 int src_ccw;
		 int dst_st;
		 int dst_en;
		 int dst_ccw;
	       };

struct gamstate { struct hmap hmap[N_HMAP];
		  int hueRBnum;                          /* 1 - 6 */
		  int wht_stval, wht_satval, wht_enab;
		  int satval;
		  GRAF_STATE istate, rstate, gstate, bstate;
		};

static struct gamstate undo[MAXUNDO], preset[4], defstate;
static struct gamstate *defLoadState;

static int uptr, uhead, utail;

typedef struct huedial {
		 Window win;      /* window that dial exists in */
		 int    x,y;      /* coordinates of center of dial */
                 int    range;    /* 0 = single value, 1 = range */
		 int    stval;    /* start of range (ONLY val ifnot range) */
		 int    enval;    /* end of range */
		 int    ccwise;   /* 1 if range goes ccwise, 0 if cwise */
		 const char *str; /* title string */
		 u_long fg,bg;    /* colors */
		 int    satval;   /* saturation value on non-range dial */
		 BUTT   hdbutt[N_HDBUTT];
		 CBUTT  enabCB;
		 void (*drawobj)PARM((void));
	       } HDIAL;



static BUTT   hueclrB;
static CBUTT  enabCB, autoCB, resetCB, dragCB;
static HDIAL  srcHD, dstHD, whtHD;
static DIAL   satDial, rhDial, gsDial, bvDial;
static GRAF   intGraf, rGraf, gGraf, bGraf;
static RBUTT *hueRB;
static Window cmapF, hsvF, rgbF, modF, butF;
static struct hmap hmap[N_HMAP];
static int    hremap[360];

static int    defAutoApply;
static int    hsvnonlinear = 0;

static void printUTime       PARM((const char *));

static void computeHSVlinear PARM((void));
static void changedGam       PARM((void));
static void drawGam          PARM((int,int,int,int));
static void drawBut          PARM((int,int,int,int));
static void drawArrow        PARM((int, int));
static void drawCmap         PARM((void));
static void clickCmap        PARM((int,int,int));
static void clickGam         PARM((int,int));
static void selectCell       PARM((int,int));
static int  deladdCell       PARM((int, int));
static void doCmd            PARM((int));
static void SetHSVmode       PARM((void));
static void applyGamma       PARM((int));
static void calcHistEQ       PARM((int *, int *, int *));
static void saveGamState     PARM((void));
static void gamUndo          PARM((void));
static void gamRedo          PARM((void));
static void ctrls2gamstate   PARM((struct gamstate *));
static void gamstate2ctrls   PARM((struct gamstate *));
static void rndCols          PARM((void));
static void saveCMap         PARM((struct cmapstate *));
static void restoreCMap      PARM((struct cmapstate *));
static void parseResources   PARM((void));
static void makeResources    PARM((void));

static void dragGamma        PARM((void));
static void dragHueDial      PARM((void));
static void dragEditColor    PARM((void));

static void HDCreate         PARM((HDIAL *, Window, int, int, int, int,
				   int, int, const char *, u_long, u_long));

static void HDRedraw         PARM((HDIAL *, int));
static int  HDClick          PARM((HDIAL *, int, int));
static int  HDTrack          PARM((HDIAL *, int, int));
static int  hdg2xdg          PARM((int));
static void pol2xy           PARM((int, int, double, int, int *, int *));
static int  computeHDval     PARM((HDIAL *, int, int));
static void initHmap         PARM((void));
static void init1hmap        PARM((int));
static void dials2hmap       PARM((void));
static void hmap2dials       PARM((void));
static void build_hremap     PARM((void));



#define CMAPF_WIDE 212
#define CMAPF_HIGH 322
#define BUTF_WIDE  212
#define BUTF_HIGH   96
#define MODF_WIDE  212
#define MODF_HIGH   70
#define HSVF_WIDE  205
#define HSVF_HIGH  500
#define RGBF_WIDE  185
#define RGBF_HIGH  500


#undef TIMING_TEST

#ifdef TIMING_TEST
#include <sys/resource.h>
#endif


/***************************/
static void printUTime(str)
     const char *str;
{
#ifdef TIMING_TEST
  int i;
  struct rusage ru;

  i = getrusage(RUSAGE_SELF, &ru);
  fprintf(stderr,"%s: utime = %ld.%ld seconds\n",
	    str, ru.ru_utime.tv_sec, ru.ru_utime.tv_usec);
#endif
}



/***************************************************/
void CreateGam(geom, gam, rgam, ggam, bgam, defpreset)
     const char *geom;
     double      gam, rgam, ggam, bgam;
     int         defpreset;
{
  XSetWindowAttributes xswa;

  gamW = CreateWindow("xv color editor", "XVcedit", geom,
		      GAMW, GAMH, infofg,infobg, 0);
  if (!gamW) FatalError("can't create cedit window!");

  cmapF = XCreateSimpleWindow(theDisp,gamW, 10,   8,CMAPF_WIDE,CMAPF_HIGH,
			      1,infofg,infobg);
  butF  = XCreateSimpleWindow(theDisp,gamW, 10, 336,BUTF_WIDE,BUTF_HIGH,
			      1,infofg,infobg);
  modF  = XCreateSimpleWindow(theDisp,gamW, 10, 438,MODF_WIDE,MODF_HIGH,
			      1,infofg,infobg);
  hsvF  = XCreateSimpleWindow(theDisp,gamW, 242,  8,HSVF_WIDE,HSVF_HIGH,
			      1,infofg,infobg);
  rgbF  = XCreateSimpleWindow(theDisp,gamW, 467,  8,RGBF_WIDE,RGBF_HIGH,
			      1,infofg,infobg);

  if (!cmapF || !butF || !modF || !hsvF || !rgbF)
    FatalError("couldn't create frame windows");

#ifdef BACKING_STORE
  xswa.backing_store = WhenMapped;
  XChangeWindowAttributes(theDisp, cmapF, CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, butF,  CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, modF,  CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, hsvF,  CWBackingStore, &xswa);
#endif

  XSelectInput(theDisp, gamW,  ExposureMask);
  XSelectInput(theDisp, cmapF, ExposureMask | ButtonPressMask);
  XSelectInput(theDisp, butF,  ExposureMask | ButtonPressMask);
  XSelectInput(theDisp, modF,  ExposureMask | ButtonPressMask);
  XSelectInput(theDisp, hsvF,  ExposureMask | ButtonPressMask);
  XSelectInput(theDisp, rgbF,  ExposureMask);

  if (ctrlColor) XSetWindowBackground(theDisp, gamW, locol);
            else XSetWindowBackgroundPixmap(theDisp, gamW, grayTile);

  /********** COLORMAP editing doo-wahs ***********/


  BTCreate(&gbut[G_BCOLUNDO], cmapF, 5, 165, 66, BUTTH,
	   "ColUndo", infofg, infobg, hicol, locol);
  BTCreate(&gbut[G_BCOLREV], cmapF,  5 + 66 + 1, 165, 67, BUTTH,
	   "Revert", infofg, infobg, hicol, locol);
  BTCreate(&gbut[G_BHSVRGB], cmapF,  5+66+67+2,  165, 66, BUTTH,
	   "RGB/HSV", infofg, infobg, hicol, locol);

  BTCreate(&gbut[G_BMONO], cmapF,    5, 189, 66, BUTTH,
	   "Grey", infofg, infobg, hicol, locol);
  BTCreate(&gbut[G_BRV],   cmapF,    5 + 66 + 1, 189, 67, BUTTH,
	   "RevVid", infofg, infobg, hicol, locol);
  BTCreate(&gbut[G_BRNDCOL], cmapF,  5 + 66 + 67 + 2, 189, 66, BUTTH,
	   "Random", infofg, infobg, hicol, locol);

  DCreate(&rhDial, cmapF, 5, 215, 66, 100,   0.0, 360.0, 180.0, 1.0, 5.0,
	  infofg, infobg, hicol, locol, "Hue", NULL);
  DCreate(&gsDial, cmapF, 72, 215, 66, 100,  0.0, 360.0, 180.0, 1.0, 5.0,
	  infofg, infobg, hicol, locol, "Sat.", NULL);
  DCreate(&bvDial, cmapF, 139, 215, 66, 100, 0.0, 360.0, 180.0, 1.0, 5.0,
	  infofg, infobg, hicol, locol, "Value", NULL);

  rhDial.drawobj = gsDial.drawobj = bvDial.drawobj = dragEditColor;


  /*********** CONTROL BUTTONS ***********/

/* positioning constants for buttons.  (arranged as 4x4 grid...) */
#define BXSPACE 53
#define BYSPACE (BUTTH+1)

#define BX0 0
#define BX1 (BX0 + BXSPACE)
#define BX2 (BX0 + BXSPACE*2)
#define BX3 (BX0 + BXSPACE*3)

#define BY0 0
#define BY1 (BY0 + BYSPACE)
#define BY2 (BY0 + BYSPACE*2)
#define BY3 (BY0 + BYSPACE*3)

  BTCreate(&gbut[G_BAPPLY],  butF, BX0,BY0, 52,BUTTH,"Apply",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BNOGAM],  butF, BX0,BY1, 52,BUTTH,"NoMod",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BMAXCONT],butF, BX0,BY2, 52,BUTTH,"Norm",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BHISTEQ], butF, BX0,BY3, 52,BUTTH,"HistEq",
	   infofg,infobg,hicol,locol);

  BTCreate(&gbut[G_BUP_BR],butF, BX1,BY0, 52,BUTTH,"Brite",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BDN_BR],butF, BX1,BY1, 52,BUTTH,"Dim",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BUP_CN],butF, BX1,BY2, 52,BUTTH,"Sharp",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BDN_CN],butF, BX1,BY3, 52,BUTTH,"Dull",
	   infofg,infobg,hicol,locol);

  BTCreate(&gbut[G_BRESET],butF, BX2,   BY0, 52,BUTTH,"Reset",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_B1],    butF, BX2,   BY1, 25,BUTTH,"1",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_B2],    butF, BX2+26,BY1, 26,BUTTH,"2",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_B3],    butF, BX2,   BY2, 25,BUTTH,"3",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_B4],    butF, BX2+26,BY2, 26,BUTTH,"4",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BSET],  butF, BX2,   BY3, 52,BUTTH,"Set",
	   infofg,infobg,hicol,locol);

  BTCreate(&gbut[G_BUNDO], butF, BX3, BY0, 52,BUTTH,"Undo",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BREDO], butF, BX3, BY1, 52,BUTTH,"Redo",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BGETRES],butF,BX3, BY2, 52,BUTTH,"CutRes",
	   infofg,infobg,hicol,locol);
  BTCreate(&gbut[G_BCLOSE],butF, BX3, BY3, 52,BUTTH,"Close",
	   infofg,infobg,hicol,locol);


  gbut[G_BSET].toggle = 1;
  gbut[G_BUNDO].active = 0;
  gbut[G_BREDO].active = 0;

  CBCreate(&enabCB, modF,2,2,     "Display with HSV/RGB mods.",
	   infofg,infobg,hicol,locol);
  CBCreate(&autoCB, modF,2,2+17,  "Auto-apply HSV/RGB mods.",
	   infofg,infobg,hicol,locol);
  CBCreate(&dragCB, modF,2,2+17*2,"Auto-apply while dragging.",
	   infofg,infobg,hicol,locol);
  CBCreate(&resetCB,modF,2,2+17*3,"Auto-reset on new image.",
	   infofg,infobg,hicol,locol);

  enabCB.val = autoCB.val = resetCB.val = dragCB.val = 1;

  defAutoApply = autoCB.val;

  /************ HSV editing doo-wahs **************/


  HDCreate(&srcHD, hsvF,  52, 65, 1, 0, 30, 0, "From", infofg, infobg);
  HDCreate(&dstHD, hsvF, 154, 65, 1, 0, 30, 0, "To",   infofg, infobg);

  HDCreate(&whtHD, hsvF,  50,243, 0, 0,  0, 0, "White",infofg, infobg);

  srcHD.drawobj = dstHD.drawobj = whtHD.drawobj = dragHueDial;

  DCreate(&satDial, hsvF, 100, 199, 100, 121, -100.0, 100.0, 0.0, 1.0, 5.0,
	   infofg, infobg,hicol,locol, "Saturation", "%");

  hueRB = RBCreate(NULL, hsvF,  7, 153, "1",
		   infofg, infobg,hicol,locol);
  RBCreate        (hueRB,hsvF, 47, 153, "2",
		   infofg, infobg,hicol,locol);
  RBCreate        (hueRB,hsvF, 87, 153, "3",
		   infofg, infobg,hicol,locol);
  RBCreate        (hueRB,hsvF,  7, 170, "4",
		   infofg, infobg,hicol,locol);
  RBCreate        (hueRB,hsvF, 47, 170, "5",
		   infofg, infobg,hicol,locol);
  RBCreate        (hueRB,hsvF, 87, 170, "6",
		   infofg, infobg,hicol,locol);

  BTCreate(&hueclrB, hsvF, 127, 158, 70, BUTTH, "Reset",
	   infofg, infobg,hicol,locol);

  initHmap();
  hmap2dials();
  build_hremap();

  InitGraf(&intGraf);
  CreateGraf(&intGraf, hsvF, 20, 339, infofg, infobg, "Intensity");


  /********* RGB color correction doo-wahs ***********/


  InitGraf(&rGraf);
  CreateGraf(&rGraf, rgbF, 10, 20, infofg, infobg, "Red");

  InitGraf(&gGraf);
  CreateGraf(&gGraf, rgbF, 10, 179, infofg, infobg, "Green");

  InitGraf(&bGraf);
  CreateGraf(&bGraf, rgbF, 10, 338, infofg, infobg, "Blue");

  satDial.drawobj = dragGamma;
  intGraf.drawobj = rGraf.drawobj = gGraf.drawobj = bGraf.drawobj = dragGamma;

  SetHSVmode();

  ctrls2gamstate(&defstate);

  /* set up preset0 as a '2-color' preset */
  ctrls2gamstate(&preset[0]);
  Str2Graf(&preset[0].istate,"L 4 : 0,0 : 127,0 : 128,255 : 255,255");


  /* set up preset1 as a '8-color' preset */
  ctrls2gamstate(&preset[1]);
  Str2Graf(&preset[1].rstate,"L 4 : 0,0 : 127,0 : 128,255 : 255,255");
  Str2Graf(&preset[1].gstate,"L 4 : 0,0 : 127,0 : 128,255 : 255,255");
  Str2Graf(&preset[1].bstate,"L 4 : 0,0 : 127,0 : 128,255 : 255,255");


  /* set up preset2 as a 'temperature' pseudo-color preset */
  ctrls2gamstate(&preset[2]);
  Str2Graf(&preset[2].rstate,"S 4 : 0,0 : 105,0 : 155,140 : 255,255");
  Str2Graf(&preset[2].gstate,"S 5 : 0,0 : 57,135 : 127,255 : 198,135 : 255,0");
  Str2Graf(&preset[2].bstate,"S 4 : 0,255 : 100,140 : 150,0 : 255,0");


  /* set up preset3 as a 'map' pseudo-color preset */
  ctrls2gamstate(&preset[3]);
  Str2Graf(&preset[3].rstate,"L 4 : 0,0 : 66,0 : 155,255 : 255,146");
  Str2Graf(&preset[3].gstate,"L 5 : 0,0 : 28,0 : 75,255 : 162,210 : 255,46");
  Str2Graf(&preset[3].bstate,"L 5 : 0,105 : 19,255 : 66,232 : 108,62:255,21");


  parseResources();
  CBSetActive(&dragCB, (allocMode == AM_READWRITE));

  /* deal with passed in [r,g,b]gam values.  If <0.0, ignore them */
  if (gam>=0.0) {
    char str[64];
    sprintf(str, "G %f", gam);
    if (Str2Graf(&defstate.istate, str)) { /* unable to parse */ }
  }

  if (rgam>=0.0 && ggam>=0.0 && bgam>=0.0) {
    char str[64];
    sprintf(str, "G %f", rgam);
    Str2Graf(&defstate.rstate, str);
    sprintf(str, "G %f", ggam);
    Str2Graf(&defstate.gstate, str);
    sprintf(str, "G %f", bgam);
    Str2Graf(&defstate.bstate, str);
  }

  if (defpreset) defLoadState = &preset[defpreset-1];
            else defLoadState = &defstate;


  gamstate2ctrls(defLoadState);  /* defstate may have changed */

  uptr = utail = uhead = 0;
  ctrls2gamstate(&undo[0]);

#ifdef BACKING_STORE
  xswa.backing_store = WhenMapped;
  XChangeWindowAttributes(theDisp, intGraf.win, CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, rGraf.win,   CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, gGraf.win,   CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, bGraf.win,   CWBackingStore, &xswa);

  XChangeWindowAttributes(theDisp, intGraf.gwin, CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, rGraf.gwin,   CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, gGraf.gwin,   CWBackingStore, &xswa);
  XChangeWindowAttributes(theDisp, bGraf.gwin,   CWBackingStore, &xswa);
#endif

  XMapSubwindows(theDisp, cmapF);
  XMapSubwindows(theDisp, hsvF);
  XMapSubwindows(theDisp, rgbF);
  XMapSubwindows(theDisp, gamW);

  computeHSVlinear();
}


/***************************************************/
int GamCheckEvent(xev)
XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;

  rv = 1;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    /* throw away excess redraws for 'dumb' windows */
    if (e->count > 0 &&
	(e->window == satDial.win || e->window == rhDial.win ||
	 e->window == gsDial.win  || e->window == bvDial.win ||
	 e->window == cmapF       || e->window == modF       ||
	 e->window == intGraf.win || e->window == rGraf.win  ||
	 e->window == gGraf.win   || e->window == bGraf.win  ||
	 e->window == intGraf.gwin || e->window == rGraf.gwin  ||
	 e->window == gGraf.gwin   || e->window == bGraf.gwin  ||
	 e->window == rgbF)) {}

    else if (e->window == gamW)        drawGam(x, y, w, h);
    else if (e->window == butF)        drawBut(x, y, w, h);
    else if (e->window == satDial.win) DRedraw(&satDial);
    else if (e->window == rhDial.win)  DRedraw(&rhDial);
    else if (e->window == gsDial.win)  DRedraw(&gsDial);
    else if (e->window == bvDial.win)  DRedraw(&bvDial);

    else if (e->window == intGraf.win) RedrawGraf(&intGraf, 0);
    else if (e->window == rGraf.win)   RedrawGraf(&rGraf,   0);
    else if (e->window == gGraf.win)   RedrawGraf(&gGraf,   0);
    else if (e->window == bGraf.win)   RedrawGraf(&bGraf,   0);

    else if (e->window == intGraf.gwin) RedrawGraf(&intGraf, 1);
    else if (e->window == rGraf.gwin)   RedrawGraf(&rGraf,   1);
    else if (e->window == gGraf.gwin)   RedrawGraf(&gGraf,   1);
    else if (e->window == bGraf.gwin)   RedrawGraf(&bGraf,   1);

    else if (e->window == cmapF) drawCmap();

    else if (e->window == modF) {
      Draw3dRect(modF, 0,0, MODF_WIDE-1, MODF_HIGH-1, R3D_IN, 2,
		 hicol, locol, infobg);

      CBRedraw(&enabCB);
      CBRedraw(&autoCB);
      CBRedraw(&dragCB);
      CBRedraw(&resetCB);
    }

    else if (e->window == hsvF) {
      XRectangle xr;
      xr.x = x;  xr.y = y;  xr.width = e->width;  xr.height = e->height;
      XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

      Draw3dRect(hsvF, 0,0, HSVF_WIDE-1, HSVF_HIGH-1, R3D_IN, 2,
		 hicol, locol, infobg);

      XSetForeground(theDisp, theGC, infofg);
      ULineString(hsvF, 3, 2+ASCENT, "HSV Modification");

      HDRedraw(&srcHD, HD_ALL);
      HDRedraw(&dstHD, HD_ALL);
      HDRedraw(&whtHD, HD_ALL);
      RBRedraw(hueRB, -1);
      BTRedraw(&hueclrB);

      XSetClipMask(theDisp, theGC, None);
    }

    else if (e->window == rgbF) {
      Draw3dRect(rgbF, 0,0, RGBF_WIDE-1, RGBF_HIGH-1, R3D_IN, 2,
		 hicol, locol, infobg);
      XSetForeground(theDisp, theGC, infofg);
      ULineString(rgbF, 3, 2+ASCENT, "RGB Modification");
    }

    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int i,x,y;
    x = e->x;  y = e->y;

    if (e->button == Button1) {
      if      (e->window == butF)  clickGam(x,y);
      else if (e->window == cmapF) clickCmap(x,y,1);

      else if (e->window == modF) {
	if (CBClick(&enabCB,x,y)) {
	  if (CBTrack(&enabCB)) applyGamma(0);  /* enabCB changed, regen */
	}

	else if (CBClick(&autoCB,x,y)) {
	  if (CBTrack(&autoCB)) {
	    defAutoApply = autoCB.val;
	    if (autoCB.val) applyGamma(0); /* auto-apply turned on, apply */
	  }
	}

	else if (CBClick(&dragCB,x,y)) {
	  if (CBTrack(&dragCB)) {
	    if (dragCB.val) applyGamma(0);  /* drag turned on, apply now */
	  }
	}

	else if (CBClick(&resetCB,x,y)) CBTrack(&resetCB);
      }


      else if (e->window == hsvF) {
	if (HDClick(&srcHD, x,y) || HDClick(&dstHD, x,y)) {
	  dials2hmap();
	  build_hremap();
	  changedGam();
	}
	else if (HDClick(&whtHD, x,y)) changedGam();

	else if (PTINRECT(x,y,hueclrB.x, hueclrB.y, hueclrB.w, hueclrB.h)) {
	  if (BTTrack(&hueclrB)) {   /* RESET */
	    dstHD.stval  = srcHD.stval;
	    dstHD.enval  = srcHD.enval;
	    dstHD.ccwise = srcHD.ccwise;
	    HDRedraw(&dstHD, HD_ALL | HD_CLEAR);
	    dials2hmap();
	    build_hremap();
	    changedGam();
	  }
	}

	else if ((i=RBClick(hueRB,x,y)) >= 0) {
	  dials2hmap();
	  if (RBTrack(hueRB, i)) hmap2dials();
	}
      }

      else if (e->window == intGraf.win) {
	GRAF_STATE gs;
	if (ClickGraf(&intGraf, e->subwindow, x,y)) {
	  GetGrafState(&intGraf, &gs);
	  changedGam();
	}
      }

      else if (e->window == rGraf.win) {
	if (ClickGraf(&rGraf, e->subwindow, x,y)) changedGam();
      }

      else if (e->window == gGraf.win) {
	if (ClickGraf(&gGraf, e->subwindow, x,y)) changedGam();
      }

      else if (e->window == bGraf.win) {
	if (ClickGraf(&bGraf, e->subwindow, x,y)) changedGam();
      }


      else if (e->window == satDial.win) {
	if (DTrack(&satDial, x, y)) changedGam();
      }

      else if (e->window == rhDial.win ||
	       e->window == gsDial.win ||
	       e->window == bvDial.win) {

	if ((e->window == rhDial.win && DTrack(&rhDial, x,y)) ||
	    (e->window == gsDial.win && DTrack(&gsDial, x,y)) ||
	    (e->window == bvDial.win && DTrack(&bvDial, x,y))) {
	  saveCMap(&prevcmap);
	  BTSetActive(&gbut[G_BCOLUNDO],1);
	  ApplyEditColor(0);
	}
      }

      else rv = 0;
    }

    else if (e->button == Button2) {
      if (e->window == cmapF) clickCmap(x, y, 2);
      else rv = 0;
    }

    else if (e->button == Button3) {
      if (e->window == cmapF) clickCmap(x, y, 3);
      else rv = 0;
    }
    else rv = 0;
  }


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;
    int stlen;

    stlen = XLookupString(e,buf,128,&ks,(XComposeStatus *) NULL);
    buf[stlen] = '\0';

    RemapKeyCheck(ks, buf, &stlen);

    if (!stlen) return 0;

    else if (e->window == intGraf.win) rv = GrafKey(&intGraf,buf);
    else if (e->window == rGraf.win  ) rv = GrafKey(&rGraf,buf);
    else if (e->window == gGraf.win  ) rv = GrafKey(&gGraf,buf);
    else if (e->window == bGraf.win  ) rv = GrafKey(&bGraf,buf);
    else rv = 0;

    if (rv>1) changedGam();   /* hit 'enter' in one of Graf's */
  }

  else rv = 0;

  return rv;
}



/***************************************************/
static void computeHSVlinear()
{
  /* determine linearity of HSV controls to avoid semi-expensive (and
     error-inducing) HSV calculations */

  int i;

  hsvnonlinear = 0;

  for (i=0; i<360 && hremap[i] == i; i++);
  if (i!=360) hsvnonlinear++;

  if (whtHD.enabCB.val && whtHD.satval) hsvnonlinear++;

  if (satDial.val != 0.0) hsvnonlinear++;

  /* check intensity graf */
  for (i=0; i<256 && intGraf.func[i]==i; i++);
  if (i<256) hsvnonlinear++;
}


/***************************************************/
static void changedGam()
{
  /* called whenever an HSV/RGB gamma ctrl has changed
     applies change to image if autoCB.val is set */

  computeHSVlinear();
  saveGamState();
  if (autoCB.val) applyGamma(0);
}


/***************************************************/
void GamBox(vis)
int vis;
{
  if (vis) XMapRaised(theDisp, gamW);
  else     XUnmapWindow(theDisp, gamW);

  gamUp = vis;
}


/***************************************************/
static void drawGam(x,y,w,h)
int x,y,w,h;
{
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  drawArrow(232,178);
  drawArrow(457,178);

  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void drawBut(x,y,w,h)
int x,y,w,h;
{
  int i;
  XRectangle xr;

  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  for (i=0; i<G_NBUTTS; i++) {
    if (gbut[i].win == butF) BTRedraw(&gbut[i]);
  }

  XSetClipMask(theDisp, theGC, None);
}


/***************************************************/
static void drawArrow(x,y)
int x,y;
{
  XPoint pts[8];

  pts[0].x = x+10;     pts[0].y = y;
  pts[1].x = x-4;      pts[1].y = y-100;
  pts[2].x = x-4;      pts[2].y = y-40;
  pts[3].x = x-10;     pts[3].y = y-40;
  pts[4].x = x-10;     pts[4].y = y+40;
  pts[5].x = x-4;      pts[5].y = y+40;
  pts[6].x = x-4;      pts[6].y = y+100;
  pts[7].x = pts[0].x; pts[7].y = pts[0].y;

  XSetForeground(theDisp, theGC, infobg);
  XFillPolygon(theDisp, gamW, theGC, pts, 8, Convex, CoordModeOrigin);
  XSetForeground(theDisp, theGC, infofg);
  XDrawLines(theDisp, gamW, theGC, pts, 8, CoordModeOrigin);
}


/***************************************************/
static void drawCmap()
{
  int i;

  Draw3dRect(cmapF, 0,0, CMAPF_WIDE-1, CMAPF_HIGH-1, R3D_IN, 2,
	     hicol, locol, infobg);

  XSetForeground(theDisp, theGC, infofg);
  ULineString(cmapF, 3, 2+ASCENT, "Colormap Editing");

  for (i=0; i<G_NBUTTS; i++) {
    if (gbut[i].win == cmapF) BTRedraw(&gbut[i]);
  }

  RedrawCMap();
}



/***************************************************/
void NewCMap()
{
  /* called when we've loaded a new picture */

  int i;

  XClearArea(theDisp, cmapF, CMAPX, CMAPY, CMAPW+1, CMAPH+1, False);
  for (i=0; i<256; i++) cellgroup[i] = 0;
  curgroup = maxgroup = 0;

  BTSetActive(&gbut[G_BCOLUNDO],0);

  if (resetCB.val) {            /* auto-reset gamma controls */
    i = autoCB.val;
    if (i) autoCB.val = 0;      /* must NOT apply changes! */
    gamstate2ctrls(defLoadState);
    autoCB.val = i;
  }

  /* disable/enable things if we're in PIC24 or PIC8 mode */
  BTSetActive(&gbut[G_BCOLREV], (picType == PIC8) ? 1 : 0);
  BTSetActive(&gbut[G_BHSVRGB], (picType == PIC8) ? 1 : 0);
  BTSetActive(&gbut[G_BMONO],   (picType == PIC8) ? 1 : 0);
  BTSetActive(&gbut[G_BRV],     (picType == PIC8) ? 1 : 0);
  BTSetActive(&gbut[G_BRNDCOL], (picType == PIC8) ? 1 : 0);

  DSetActive(&rhDial, (picType == PIC8) ? 1 : 0);
  DSetActive(&gsDial, (picType == PIC8) ? 1 : 0);
  DSetActive(&bvDial, (picType == PIC8) ? 1 : 0);
}


/***************************************************/
void RedrawCMap()
{
  int i;

  CBSetActive(&dragCB, (allocMode == AM_READWRITE));

  XSetLineAttributes(theDisp, theGC, 0, LineSolid, CapButt, JoinMiter);
  XSetForeground(theDisp, theGC, infofg);

  if (picType != PIC8) {
    CenterString(cmapF, CMAPX + CMAPW/2, CMAPY + CMAPH/2,
		 "No colormap in 24-bit mode.");
    return;
  }



  for (i=0; i<numcols; i++) {
    int x,y;
    x = CMAPX + (i%16)*CMAPCW;
    y = CMAPY + (i/16)*CMAPCH;
    XDrawRectangle(theDisp, cmapF, theGC, x, y, CMAPCW,CMAPCH);
  }

  for (i=0; i<numcols; i++) {
    int x,y;
    x = CMAPX + (i%16)*CMAPCW;
    y = CMAPY + (i/16)*CMAPCH;

    XSetForeground(theDisp, theGC, cols[i]);
    XFillRectangle(theDisp, cmapF, theGC, x+1, y+1, CMAPCW-1,CMAPCH-1);

    if (i == editColor || (curgroup && (cellgroup[i]==curgroup))) {
      XSetForeground(theDisp, theGC, infobg);
      XDrawRectangle(theDisp, cmapF, theGC, x+1, y+1, CMAPCW-2,CMAPCH-2);
      XSetForeground(theDisp, theGC, infofg);
      XDrawRectangle(theDisp, cmapF, theGC, x+2, y+2, CMAPCW-4,CMAPCH-4);
    }
  }
}


/***************************************************/
static void selectCell(cellno, sel)
int cellno, sel;
{
  int x,y;

  if (cellno >= numcols) return;

  x = CMAPX + (cellno%16)*CMAPCW;
  y = CMAPY + (cellno/16)*CMAPCH;

  if (!sel) {   /* unhighlight a cell */
    XSetForeground(theDisp, theGC, cols[cellno]);
    XFillRectangle(theDisp, cmapF, theGC, x+1, y+1, CMAPCW-1,CMAPCH-1);
  }
  else {  /* highlight a cell */
    XSetForeground(theDisp, theGC, infobg);
    XDrawRectangle(theDisp, cmapF, theGC, x+1, y+1, CMAPCW-2,CMAPCH-2);
    XSetForeground(theDisp, theGC, infofg);
    XDrawRectangle(theDisp, cmapF, theGC, x+2, y+2, CMAPCW-4,CMAPCH-4);
  }
}


/***************************************************/
static void clickGam(x,y)
int x,y;
{
  int i;
  BUTT *bp;

  for (i=0; i<G_NBUTTS; i++) {
    bp = &gbut[i];
    if (bp->win == butF && PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  /* if 'Set' is lit, and we didn't click 'set' or 'Reset' or '1'..'4',
     turn it off */
  if (i!=G_BSET && i!=G_B1 && i!=G_B2 && i!=G_B3 && i!=G_B4 && i!=G_BRESET
      && gbut[G_BSET].lit) {
    gbut[G_BSET].lit = 0;
    BTRedraw(&gbut[G_BSET]);
  }


  if (i<G_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }
}


/***************************************************/
static void clickCmap(x,y,but)
int x,y,but;
{
  int i, recolor;
  BUTT *bp;

  if (but==1) {   /* if left click, check the cmap controls */
    for (i=0; i<G_NBUTTS; i++) {
      bp = &gbut[i];
      if (bp->win == cmapF && PTINRECT(x,y,bp->x, bp->y, bp->w, bp->h)) break;
    }

    if (i<G_NBUTTS) {  /* found one */
      if (BTTrack(bp)) doCmd(i);
      return;
    }
  }


  /* see if we're anywhere in the colormap area */

  if (picType == PIC8 && PTINRECT(x,y,CMAPX,CMAPY,CMAPW,CMAPH)) {
    if (but==1) {           /* select different cell/group for editing */
      /* compute colorcell # */
      i = ((x-CMAPX)/CMAPCW) + ((y-CMAPY)/CMAPCH)*16;
      if (i<numcols) {    /* clicked in colormap.  track til mouseup */
	if (i!=editColor) ChangeEC(i);

	while (1) {
	  Window       rW,cW;
	  int          rx,ry,x,y;
	  unsigned int mask;

	  if (XQueryPointer(theDisp,cmapF,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	    if (!(mask & Button1Mask)) break;    /* button released */

	    RANGE(x, CMAPX, CMAPX+CMAPW-1);
	    RANGE(y, CMAPY, CMAPY+CMAPH-1);

	    i = ((x-CMAPX)/CMAPCW) + ((y-CMAPY)/CMAPCH)*16;
	    if (i<numcols && i != editColor) ChangeEC(i);
	  }
	} /* while */
      } /* if i<numcols */
    } /* if but==1 */


    else if (but==2) {   /* color smooth */
      int cellnum, delc, col1, j, delr, delg, delb;

      /* compute colorcell # */
      cellnum = ((x-CMAPX)/CMAPCW) + ((y-CMAPY)/CMAPCH)*16;
      if (cellnum<numcols) {
	delc = abs(cellnum - editColor);
	if (delc) {  /* didn't click on same cell */
	  saveCMap(&tmpcmap);  /* save the current cmap state */

	  if (cellnum < editColor) col1 = cellnum;  else col1 = editColor;

	  delr = rcmap[col1 + delc] - rcmap[col1];
	  delg = gcmap[col1 + delc] - gcmap[col1];
	  delb = bcmap[col1 + delc] - bcmap[col1];

	  for (i=0; i<delc; i++) {
	    rcmap[col1 + i] = rcmap[col1] + (delr * i) / delc;
	    gcmap[col1 + i] = gcmap[col1] + (delg * i) / delc;
	    bcmap[col1 + i] = bcmap[col1] + (delb * i) / delc;

	    if (cellgroup[col1 + i]) {
	      /* propogate new color to all members of this group */
	      for (j=0; j<numcols; j++)
		if (cellgroup[j] == cellgroup[col1 + i]) {
		  rcmap[j] = rcmap[col1 + i];
		  gcmap[j] = gcmap[col1 + i];
		  bcmap[j] = bcmap[col1 + i];
		}
	    }
	  }

	  for (i=0; i<numcols; i++) {
	    if (rcmap[i] != tmpcmap.rm[i] ||
		gcmap[i] != tmpcmap.gm[i] ||
		bcmap[i] != tmpcmap.bm[i]) break;
	  }

	  if (i<numcols) {  /* something changed */
	    xvbcopy((char *) &tmpcmap, (char *) &prevcmap,
		    sizeof(struct cmapstate));
	    BTSetActive(&gbut[G_BCOLUNDO],1);
	    applyGamma(1);
	  }
	}
      }
    }


    else if (but==3) { /* add/delete cell(s) from current group */
      int lastcell,j,resetdel,curcell;

      /* better save the current cmap state, as it might change */
      saveCMap(&tmpcmap);

      recolor = resetdel = 0;
      /* compute colorcell # clicked in */
      curcell = ((x-CMAPX)/CMAPCW) + ((y-CMAPY)/CMAPCH)*16;
      if (curcell<numcols) {    /* clicked in colormap.  track til mouseup */
	if (deladdCell(curcell, 1)) recolor=1;

	lastcell = curcell;

	j = XGrabPointer(theDisp, cmapF, False, 0, GrabModeAsync,
			 GrabModeAsync, None, None, (Time) CurrentTime);
	while (1) {
	  Window       rW,cW;
	  int          rx,ry,x,y;
	  unsigned int mask;

	  if (XQueryPointer(theDisp,cmapF,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	    /* if button3 and shift released */
	    if (!(mask & (Button3Mask | ShiftMask))) break;

	    /* if user lets go of B3, reset addonly/delonly flag & lastcell */
	    if (!(mask & Button3Mask) && (mask & ShiftMask)) {
	      resetdel = 1;
	      lastcell = -1;
	    }

	    if (mask & Button3Mask) {  /* select while b3 down */
	      RANGE(x, CMAPX, CMAPX+CMAPW-1);
	      RANGE(y, CMAPY, CMAPY+CMAPH-1);

	      curcell = ((x-CMAPX)/CMAPCW) + ((y-CMAPY)/CMAPCH)*16;
	      if (curcell<numcols && curcell != lastcell) {
		lastcell = curcell;
		if (deladdCell(curcell, resetdel)) recolor=1;
		resetdel = 0;
	      }
	    }
	  }
	} /* while */

	XUngrabPointer(theDisp, (Time) CurrentTime);

	/* restore all cells that aren't in curgroup, unless curgroup=0,
	   in which case restore everything...  nasty */
	for (i=0; i<numcols; i++) {
	  if (!curgroup || cellgroup[i]!=curgroup) {
	    rcmap[i] = tmpcmap.rm[i];
	    gcmap[i] = tmpcmap.gm[i];
	    bcmap[i] = tmpcmap.bm[i];
	  }
	}

	if (recolor) {
	  /* colors changed.  save to color undo area */
	  xvbcopy((char *) &tmpcmap, (char *) &prevcmap,
		  sizeof(struct cmapstate));
	  BTSetActive(&gbut[G_BCOLUNDO],1);
	  applyGamma(1);   /* have to regen entire image when groupings chg */
	}

      } /* if i<numcols */
    } /* if but==3 */

  } /* if PTINRECT */
}




/***************************************************/
static int deladdCell(cnum, first)
int cnum, first;
{
  int i,j,rv;
  static int mode;

#define ADDORDEL 0
#define DELONLY  1
#define ADDONLY  2

  /* if 'first', we can add/delete cells as appropriate.  otherwise,
     the (static) value of 'mode' determines whether we can
     delete or add cells to groups.  The practical upshot is that it should
     behave like the 'fat-bits pencil' in MacPaint */

  /* cases:  curgroup>0, clicked on something in same group
                         remove target from group
	     curgroup>0, clicked on something in different group
	                 merge groups.  (target group gets
			 set equal to current values)
             curgroup>0, clicked on something in no group
	                 add target to curgroup
             curgroup=0, clicked on something in a group
	                 add editColor to target group,
			 set curgroup = target group
			 target group gets current values
	     curgroup=0, clicked on something in no group
	                 create a new group, add both cells to it
   */


  rv = 0;
  if (first) mode = ADDORDEL;

  if (curgroup) {
    if ((mode!=ADDONLY) && cellgroup[cnum] == curgroup) {
      /* remove target from curgroup.  If it's the last one, delete group */

      for (i=0,j=0; i<numcols; i++) {     /* count #cells in this group */
	if (cellgroup[i] == curgroup) j++;
      }

      if (j>1) {  /* remove target cell from group */
	cellgroup[cnum] = 0;
	selectCell(cnum,0);
	mode = DELONLY;
	if (cnum==editColor) { /* set editColor to first cell in group */
	  for (i=0; i<numcols && cellgroup[i]!=curgroup; i++);
	  if (i<numcols) editColor = i;
	}
      }
      else {  /* last cell in group.  set to group=0, but don't unhighlight */
	cellgroup[cnum] = 0;
	curgroup = 0;
      }
    }

    else if ((mode!=DELONLY) && cellgroup[cnum] != curgroup &&
	     cellgroup[cnum]>0) {
      /* merge clicked-on group into curgroup */
      mode = ADDONLY;
      rv = 1;  j = cellgroup[cnum];
      for (i=0; i<numcols; i++) {
	if (cellgroup[i] == j) {
	  cellgroup[i] = curgroup;
	  selectCell(i,1);
	  rcmap[i] = rcmap[editColor];
	  gcmap[i] = gcmap[editColor];
	  bcmap[i] = bcmap[editColor];
	}
      }
    }

    else if ((mode!=DELONLY) && cellgroup[cnum] == 0) {
      /* merge clicked-on cell into curgroup */
      mode = ADDONLY;
      rv = 1;
      cellgroup[cnum] = curgroup;
      selectCell(cnum,1);
      rcmap[cnum] = rcmap[editColor];
      gcmap[cnum] = gcmap[editColor];
      bcmap[cnum] = bcmap[editColor];
    }
  }

  else {  /* !curgroup */
    if ((mode!=DELONLY) && cellgroup[cnum] != 0) {
      /* merge editColor into clicked-on group */
      /* clicked on group, however, takes values of editCell */
      mode = ADDONLY;
      rv = 1;  j = cellgroup[cnum];
      for (i=0; i<numcols; i++) {
	if (cellgroup[i] == j) {
	  selectCell(i,1);
	  rcmap[i] = rcmap[editColor];
	  gcmap[i] = gcmap[editColor];
	  bcmap[i] = bcmap[editColor];
	}
      }
      curgroup = cellgroup[cnum];
      cellgroup[editColor] = curgroup;
    }

    else if ((mode!=DELONLY) && (cellgroup[cnum] == 0)
	     && (cnum != editColor)) {
      /* create new group for these two cells (cnum and editColor) */
      mode = ADDONLY;
      rv = 1;
      maxgroup++;
      cellgroup[cnum] = cellgroup[editColor] = maxgroup;
      selectCell(cnum,1);
      curgroup = maxgroup;
      rcmap[cnum] = rcmap[editColor];
      gcmap[cnum] = gcmap[editColor];
      bcmap[cnum] = bcmap[editColor];
    }
  }

  return rv;
}


/*********************/
void ChangeEC(num)
int num;
{
  /* given a color # that is to become the new editColor, do all
     highlighting/unhighlighting, copy editColor's rgb values to
     the rgb/hsv dials */

  int i,oldgroup;

  if (picType != PIC8) return;

  oldgroup = curgroup;

  if (curgroup && cellgroup[num] != curgroup) {
    /* a group is currently selected, and we're picking a new cell that
       isn't in the group, have to unhighlight entire group */

    for (i=0; i<numcols; i++) {
      if (cellgroup[i] == curgroup) selectCell(i,0);
    }
  }
  else if (!curgroup) selectCell(editColor,0);

  editColor = num;
  curgroup = cellgroup[editColor];

  if (curgroup && curgroup != oldgroup) {
    /* if new cell is in a group, highlight that group */
    for (i=0; i<numcols; i++) {
      if (cellgroup[i] == curgroup) selectCell(i,1);
    }
  }
  else if (!curgroup) selectCell(editColor,1);


  if (hsvmode) {
    double h, s, v;
    rgb2hsv(rcmap[editColor], gcmap[editColor], bcmap[editColor], &h, &s, &v);
    if (h<0) h = 0;

    DSetVal(&rhDial, h);
    DSetVal(&gsDial, s*100);
    DSetVal(&bvDial, v*100);
  }
  else {
    DSetVal(&rhDial, (double)rcmap[editColor]);
    DSetVal(&gsDial, (double)gcmap[editColor]);
    DSetVal(&bvDial, (double)bcmap[editColor]);
  }
}


/*********************/
void ApplyECctrls()
{
  /* sets values of {r,g,b}cmap[editColor] based on dial settings */

  if (hsvmode) {
    int rv, gv, bv;
    hsv2rgb(rhDial.val, gsDial.val / 100.0, bvDial.val / 100.0, &rv, &gv, &bv);
    rcmap[editColor] = rv;
    gcmap[editColor] = gv;
    bcmap[editColor] = bv;
  }
  else {
    rcmap[editColor] = (int)rhDial.val;
    gcmap[editColor] = (int)gsDial.val;
    bcmap[editColor] = (int)bvDial.val;
  }
}



/*********************/
void GenerateFSGamma()
{
  /* this function generates the Floyd-Steinberg gamma curve (fsgamcr)

     This function generates a 4 point spline curve to be used as a
     non-linear grey 'colormap'.  Two of the points are nailed down at 0,0
     and 255,255, and can't be changed.  You specify the other two.  If
     you specify points on the line (0,0 - 255,255), you'll get the normal
     linear reponse curve.  If you specify points of 50,0 and 200,255, you'll
     get grey values of 0-50 to map to black (0), and grey values of 200-255
     to map to white (255) (roughly).  Values between 50 and 200 will cover
     the output range 0-255.  The reponse curve will be slightly 's' shaped. */

  int i,j;
  static int x[4] = {0,16,240,255};
  static int y[4] = {0, 0,255,255};
  double yf[4];

  InitSpline(x, y, 4, yf);

  for (i=0; i<256; i++) {
    j = (int) EvalSpline(x, y, yf, 4, (double) i);
    if (j<0) j=0;
    else if (j>255) j=255;
    fsgamcr[i] = j;
  }
}


/*********************/
static void doCmd(cmd)
int cmd;
{
  int i;
  GRAF_STATE gs;

  switch (cmd) {

  case G_BAPPLY:
    if (enabCB.val != 1) { enabCB.val = 1;  CBRedraw(&enabCB); }
    applyGamma(0);
    break;

  case G_BNOGAM:
    if (enabCB.val != 0) { enabCB.val = 0;  CBRedraw(&enabCB); }
    applyGamma(0);
    break;

  case G_BUNDO:  gamUndo();  break;
  case G_BREDO:  gamRedo();  break;
  case G_BCLOSE: GamBox(0);  break;


  case G_BHISTEQ: DoHistEq();  changedGam(); break;



  case G_BDN_BR:
  case G_BUP_BR: GetGrafState(&intGraf, &gs);
                 for (i=0; i < gs.nhands; i++) {
		   if (cmd==G_BUP_BR) gs.hands[i].y += 10;
		                 else gs.hands[i].y -= 10;
		   RANGE(gs.hands[i].y, 0, 255);
		 }
                 SetGrafState(&intGraf, &gs);
                 changedGam();
                 break;


  case G_BUP_CN: GetGrafState(&intGraf, &gs);
                 for (i=0; i < gs.nhands; i++) {
		   if (gs.hands[i].x < 128) gs.hands[i].y -= 10;
		                       else gs.hands[i].y += 10;
		   RANGE(gs.hands[i].y, 0, 255);
		 }
                 SetGrafState(&intGraf, &gs);
                 changedGam();
                 break;

  case G_BDN_CN: GetGrafState(&intGraf, &gs);
                 for (i=0; i < gs.nhands; i++) {
		   if (gs.hands[i].y < 128) {
		     gs.hands[i].y += 10;
		     if (gs.hands[i].y > 128) gs.hands[i].y = 128;
		   }
		   else {
		     gs.hands[i].y -= 10;
		     if (gs.hands[i].y < 128) gs.hands[i].y = 128;
		   }
		 }
                 SetGrafState(&intGraf, &gs);
                 changedGam();
                 break;


  case G_BMAXCONT: DoNorm();  changedGam();  break;

  case G_BRESET:
  case G_B1:
  case G_B2:
  case G_B3:
  case G_B4: { struct gamstate *ptr = &defstate;

	       if      (cmd==G_B1)     ptr = &preset[0];
	       else if (cmd==G_B2)     ptr = &preset[1];
	       else if (cmd==G_B3)     ptr = &preset[2];
	       else if (cmd==G_B4)     ptr = &preset[3];
	       else if (cmd==G_BRESET) ptr = &defstate;

	       if (gbut[G_BSET].lit) {
		 ctrls2gamstate(ptr);
		 gbut[G_BSET].lit = 0;
		 BTRedraw(&gbut[G_BSET]);
	       }
	       else gamstate2ctrls(ptr);
	     }
             break;

  case G_BSET:  break;


  case G_BHSVRGB:
    hsvmode = !hsvmode;
    SetHSVmode();
    saveGamState();
    break;


  case G_BCOLREV:
    {
      struct cmapstate tmp1cmap;
      int gchg;

      for (i=0; i<numcols && prevcmap.cellgroup[i]==0; i++);
      gchg = (i!=numcols);

      saveCMap(&tmpcmap);         /* buffer current cmapstate */

      for (i=0; i<numcols; i++) { /* do reversion */
	rcmap[i] = rorg[i];
	gcmap[i] = gorg[i];
	bcmap[i] = borg[i];
	cellgroup[i] = 0;
      }
      curgroup = maxgroup = 0;

      saveCMap(&tmp1cmap);        /* buffer current cmapstate */

      /* prevent multiple 'Undo All's from filling Undo buffer */
      if (xvbcmp((char *) &tmpcmap, (char *) &tmp1cmap,
		 sizeof(struct cmapstate))) {
	/* the reversion changed the cmapstate */
	xvbcopy((char *) &tmpcmap, (char *) &prevcmap,
		sizeof(struct cmapstate));
	BTSetActive(&gbut[G_BCOLUNDO],1);

	RedrawCMap();
	ChangeEC(editColor);
	applyGamma(1);
	if (gchg) ApplyEditColor(1);
      }
    }
    break;


  case G_BRNDCOL:
    saveCMap(&prevcmap);
    BTSetActive(&gbut[G_BCOLUNDO],1);
    rndCols();
    break;

  case G_BRV:
    saveCMap(&prevcmap);
    BTSetActive(&gbut[G_BCOLUNDO],1);

    if (hsvmode) {              /* reverse video in HSV space (flip V only) */
      double h,s,v;   int rr, gr, br;
      for (i=0; i<numcols; i++) {
	rgb2hsv(rcmap[i], gcmap[i], bcmap[i], &h, &s, &v);

	v = 1.0 - v;
	if (v <= .05) v = .05;

	hsv2rgb(h, s, v, &rr, &gr, &br);
	rcmap[i] = rr;  gcmap[i] = gr;  bcmap[i] = br;
      }
    }
    else {
      for (i=0; i<numcols; i++) {
	rcmap[i] = 255-rcmap[i];
	gcmap[i] = 255-gcmap[i];
	bcmap[i] = 255-bcmap[i];
      }
    }
    ChangeEC(editColor);
    applyGamma(1);
    break;


  case G_BMONO:
    saveCMap(&prevcmap);
    BTSetActive(&gbut[G_BCOLUNDO],1);
    for (i=0; i<numcols; i++) {
      rcmap[i] = gcmap[i] = bcmap[i] = MONO(rcmap[i],gcmap[i],bcmap[i]);
    }
    ChangeEC(editColor);
    applyGamma(1);
    break;


  case G_BCOLUNDO:
    for (i=0; i<numcols && cellgroup[i]==prevcmap.cellgroup[i]; i++);

    saveCMap(&tmpcmap);
    restoreCMap(&prevcmap);
    xvbcopy((char *) &tmpcmap, (char *) &prevcmap, sizeof(struct cmapstate));
    RedrawCMap();
    ChangeEC(editColor);
    applyGamma(1);
    if (i!=numcols) ApplyEditColor(1);
    break;

  case G_BGETRES:   makeResources();  break;
  }
}


/*********************/
static void SetHSVmode()
{
  if (!hsvmode) {
    rhDial.title = "Red";
    gsDial.title = "Green";
    bvDial.title = "Blue";

    DSetRange(&rhDial, 0.0, 255.0, (double)rcmap[editColor], 1.0, 16.0);
    DSetRange(&gsDial, 0.0, 255.0, (double)gcmap[editColor], 1.0, 16.0);
    DSetRange(&bvDial, 0.0, 255.0, (double)bcmap[editColor], 1.0, 16.0);

    XClearWindow(theDisp, rhDial.win);    DRedraw(&rhDial);
    XClearWindow(theDisp, gsDial.win);    DRedraw(&gsDial);
    XClearWindow(theDisp, bvDial.win);    DRedraw(&bvDial);
  }

  else {
    double h,s,v;

    rhDial.title = "Hue";
    gsDial.title = "Sat.";
    bvDial.title = "Value";

    rgb2hsv(rcmap[editColor], gcmap[editColor], bcmap[editColor],
	    &h, &s, &v);

    if (h<0.0) h = 0.0;
    DSetRange(&rhDial, 0.0, 360.0,     h, 1.0, 5.0);
    DSetRange(&gsDial, 0.0, 100.0, s*100, 1.0, 5.0);
    DSetRange(&bvDial, 0.0, 100.0, v*100, 1.0, 5.0);

    XClearWindow(theDisp, rhDial.win);    DRedraw(&rhDial);
    XClearWindow(theDisp, gsDial.win);    DRedraw(&gsDial);
    XClearWindow(theDisp, bvDial.win);    DRedraw(&bvDial);
  }
}


/*********************/
static void applyGamma(cmapchange)
     int cmapchange;
{
  /* called to regenerate the image based on r/g/bcmap or output of rgb/hsv
     filters.  Doesn't check autoCB.  Note: if cmap change, and we have
     less colors than we need, and we're doing rw color, force a resort
     of the alloc order */

  int i,j;
  byte oldr[256], oldg[256], oldb[256];

  if (!pic || !epic) return;   /* called before image exists.  ignore */

  if (picType == PIC8) {
    /* save current 'desired' colormap */
    xvbcopy((char *) rMap, (char *) oldr, (size_t) numcols);
    xvbcopy((char *) gMap, (char *) oldg, (size_t) numcols);
    xvbcopy((char *) bMap, (char *) oldb, (size_t) numcols);

    GammifyColors();

    /* if current 'desired' colormap hasn't changed, don't DO anything */
    if (!xvbcmp((char *) rMap, (char *) oldr, (size_t) numcols) &&
	!xvbcmp((char *) gMap, (char *) oldg, (size_t) numcols) &&
	!xvbcmp((char *) bMap, (char *) oldb, (size_t) numcols)) return;

    /* special case: if using R/W color, just modify the colors and leave */
    if (allocMode==AM_READWRITE && rwthistime &&
	(!cmapchange || nfcols==numcols)) {
      XColor ctab[256];

      for (i=0; i<nfcols; i++) {
	j = fc2pcol[i];
	if (mono) {
	  int intens = MONO(rMap[j], gMap[j], bMap[j]);
	  ctab[i].red = ctab[i].green = ctab[i].blue = intens<<8;
	}
	else {
	  ctab[i].red   = rMap[j]<<8;
	  ctab[i].green = gMap[j]<<8;
	  ctab[i].blue  = bMap[j]<<8;
	}

	ctab[i].pixel = freecols[i];
	ctab[i].flags = DoRed | DoGreen | DoBlue;
	XStoreColor(theDisp, LocalCmap ? LocalCmap : theCmap, &ctab[i]);
      }
      XStoreColor(theDisp, LocalCmap ? LocalCmap : theCmap, &ctab[0]);

      for (i=0; i<numcols; i++) {
	rdisp[i] = rMap[rwpc2pc[i]];
	gdisp[i] = gMap[rwpc2pc[i]];
	bdisp[i] = bMap[rwpc2pc[i]];
      }

      return;
    }

    FreeColors();

    {
      byte trans[256];
      SortColormap(pic,pWIDE,pHIGH,&numcols,rMap,gMap,bMap,
		   colAllocOrder, trans);
      ColorCompress8(trans);
    }

    AllocColors();


    if (epicMode != EM_RAW) {
      /* regen image, as we'll probably want to dither differently, given
	 new colors and such */

      GenerateEpic(eWIDE, eHIGH);
    }
  }

  DrawEpic();
  SetCursors(-1);
}


/*********************/
static void calcHistEQ(histeq, rminv, rmaxv)
     int *histeq, *rminv, *rmaxv;
{
  int i, maxv, topbin, hist[256], rgb[256];
  byte *ep;
  unsigned long total;

  if (picType == PIC8) {
    for (i=0; i<256; i++) {
      hist[i] = 0;
      rgb[i] = MONO(rcmap[i],gcmap[i],bcmap[i]);
    }

    /* compute intensity histogram */
    if (epic) for (i=eWIDE*eHIGH,ep=epic; i>0; i--, ep++) hist[rgb[*ep]]++;
         else for (i=pWIDE*pHIGH,ep=pic;  i>0; i--, ep++) hist[rgb[*ep]]++;

    /* compute minv/maxv values */
    for (i=0; i<256 && !hist[i]; i++);
    *rminv = i;

    for (i=255; i>0 && !hist[i]; i--);
    *rmaxv = i;
  }

  else {  /* PIC24 */
    int v,minv,maxv;

    for (i=0; i<256; i++) hist[i] = 0;

    minv = 255;  maxv = 0;
    if (epic) {
      for (i=0,ep=epic; i<eWIDE*eHIGH; i++,ep+=3) {
	v = MONO(ep[0],ep[1],ep[2]);
	if (v<minv) minv = v;
	if (v>maxv) maxv = v;
	hist[v]++;
      }
    }
    else {
      for (i=0,ep=pic; i<pWIDE*pHIGH; i++,ep+=3) {
	v = MONO(ep[0],ep[1],ep[2]);
	if (v<minv) minv = v;
	if (v>maxv) maxv = v;
	hist[v]++;
      }
    }

    *rminv = minv;  *rmaxv = maxv;
  }

  if (DEBUG) {
    fprintf(stderr,"intensity histogram:  ");
    for (i=0; i<256; i++) fprintf(stderr,"%d ", hist[i]);
    fprintf(stderr,"\n\n");
  }

  /* compute histeq curve */
  total = topbin = 0;
  for (i=0; i<256; i++) {
    histeq[i] = (total * 255) / ((unsigned long) eWIDE * eHIGH);
    if (hist[i]) topbin = i;
    total += hist[i];
  }

  /* stretch range, as histeq[255] is probably *not* equal to 255 */
  maxv = (histeq[topbin]) ? histeq[topbin] : 255;   /* avoid div by 0 */
  for (i=0; i<256; i++)
    histeq[i] = (histeq[i] * 255) / maxv;

  if (DEBUG) {
    fprintf(stderr,"intensity eq curve:  ");
    for (i=0; i<256; i++) fprintf(stderr,"%d ", histeq[i]);
    fprintf(stderr,"\n\n");
  }

  /* play it safe:  do a range check on histeq */
  for (i=0; i<256; i++) RANGE(histeq[i],0,255);
}


/*********************/
void DoHistEq()
{
  int i, histeq[256], minv, maxv;

  calcHistEQ(histeq, &minv, &maxv);  /* ignore minv,maxv */

  for (i=0; i<256; i++)
    intGraf.func[i] = histeq[i];

  for (i=0; i< intGraf.nhands; i++)
    intGraf.hands[i].y = intGraf.func[intGraf.hands[i].x];

  intGraf.entergamma = 0;

  if (gamUp) {
    XClearWindow(theDisp, intGraf.gwin);
    RedrawGraf(&intGraf, 1);
  }

  computeHSVlinear();
}


/*********************/
void DoNorm()
{
  int i, minv, maxv, v;
  GRAF_STATE gs;

  minv = 255;  maxv = 0;

  if (picType == PIC8) {
    for (i=0; i<numcols; i++) {
      v = MONO(rcmap[i],gcmap[i],bcmap[i]);
      if (v<minv) minv = v;
      if (v>maxv) maxv = v;
    }
  }
  else {
    int histeq[256];
    calcHistEQ(histeq, &minv, &maxv);  /* ignore histeq */
  }

  GetGrafState(&intGraf, &gs);

  gs.spline = 0;
  gs.entergamma = 0;
  gs.gammamode = 0;
  gs.nhands = 4;
  gs.hands[0].x = 0;       gs.hands[0].y = 0;
  gs.hands[1].x = minv;    gs.hands[1].y = 0;
  gs.hands[2].x = maxv;    gs.hands[2].y = 255;
  gs.hands[3].x = 255;     gs.hands[3].y = 255;

  if (minv<1)   { gs.hands[1].x = gs.hands[1].y = 1; }
  if (maxv>254) { gs.hands[2].x = gs.hands[2].y = 254; }

  SetGrafState(&intGraf, &gs);

  computeHSVlinear();
}


/*********************/
void GammifyColors()
{
  int i;

  if (picType != PIC8) return;

  if (enabCB.val) {
    for (i=0; i<numcols; i++) Gammify1(i);
  }
  else {
    for (i=0; i<numcols; i++) {
      rMap[i] = rcmap[i];
      gMap[i] = gcmap[i];
      bMap[i] = bcmap[i];
      if (!ncols)
	cols[i] = (((int)rMap[i]) + ((int)gMap[i]) + ((int)bMap[i]) >= 128*3)
	  ? white : black;
    }
  }
}


#define NOHUE -1

/*********************/
void Gammify1(col)
int col;
{
  int rv, gv, bv, vi, hi;
  double h,s,v;

  rv = rcmap[col];  gv = gcmap[col];  bv = bcmap[col];
  if (DEBUG>1) fprintf(stderr,"Gammify:  %d,%d,%d",rv,gv,bv);

  if (!hsvnonlinear) {
    if (DEBUG>1) fprintf(stderr," HSV stage skipped (is linear)");
  }
  else {
    rgb2hsv(rv, gv, bv, &h, &s, &v);
    if (DEBUG>1) fprintf(stderr," -> %f,%f,%f",h,s,v);

    /* map near-black to black to avoid weird effects */
    if (v <= .0625) s = 0.0;

    /* apply intGraf.func[] function to 'v' (the intensity) */
    vi = (int) floor((v * 255.0) + 0.5);
    if (DEBUG>1) fprintf(stderr," (vi=%d)",vi);

    v = ((double) intGraf.func[vi]) / 255.0;
    if (DEBUG>1) fprintf(stderr," (v=%f)",v);

    if (h>=0) {
      hi = (int) h;
      if (hi<0)    hi += 360;
      if (hi>=360) hi -= 360;
      h = (double) hremap[hi];
    }
    else {
      if (whtHD.enabCB.val) {
	h = (double) whtHD.stval;
	s = (double) whtHD.satval / 100.0;

	/* special case:  if stval = satval = 0, set hue = -1 */
	if (whtHD.stval == 0 && whtHD.satval == 0) h = -1.0;
      }
    }

    /* apply satDial value to s */
    s = s + satDial.val / 100.0;
    if (s<0.0) s = 0.0;
    if (s>1.0) s = 1.0;

    hsv2rgb(h,s,v,&rv, &gv, &bv);
    if (DEBUG>1) fprintf(stderr," -> %d,%d,%d",rv,gv,bv);
  }

  rMap[col] = rGraf.func[rv];
  gMap[col] = gGraf.func[gv];
  bMap[col] = bGraf.func[bv];

  if (!ncols)
    cols[col] =
      (((int)rMap[col]) + ((int)gMap[col]) + ((int)bMap[col]) >= 128*3)
	? white : black;

  if (DEBUG>1) fprintf(stderr," -> %d,%d,%d\n",rMap[col],gMap[col],bMap[col]);
}


/*********************/
void rgb2hsv(r,g,b, hr, sr, vr)
int r, g, b;
double *hr, *sr, *vr;
{
  double rd, gd, bd, h, s, v, max, min, del, rc, gc, bc;

  /* convert RGB to HSV */
  rd = r / 255.0;            /* rd,gd,bd range 0-1 instead of 0-255 */
  gd = g / 255.0;
  bd = b / 255.0;

  /* compute maximum of rd,gd,bd */
  if (rd>=gd) { if (rd>=bd) max = rd;  else max = bd; }
         else { if (gd>=bd) max = gd;  else max = bd; }

  /* compute minimum of rd,gd,bd */
  if (rd<=gd) { if (rd<=bd) min = rd;  else min = bd; }
         else { if (gd<=bd) min = gd;  else min = bd; }

  del = max - min;
  v = max;
  if (max != 0.0) s = (del) / max;
             else s = 0.0;

  h = NOHUE;
  if (s != 0.0) {
    rc = (max - rd) / del;
    gc = (max - gd) / del;
    bc = (max - bd) / del;

    if      (rd==max) h = bc - gc;
    else if (gd==max) h = 2 + rc - bc;
    else if (bd==max) h = 4 + gc - rc;

    h = h * 60;
    if (h<0) h += 360;
  }

  *hr = h;  *sr = s;  *vr = v;
}



/*********************/
void hsv2rgb(h, s, v, rr, gr, br)
double h, s, v;
int *rr, *gr, *br;
{
  int    j;
  double rd, gd, bd;
  double f, p, q, t;

  /* convert HSV back to RGB */
  if (h==NOHUE || s==0.0) { rd = v;  gd = v;  bd = v; }
  else {
    if (h==360.0) h = 0.0;
    h = h / 60.0;
    j = (int) floor(h);
    if (j<0) j=0;          /* either h or floor seem to go neg on some sys */
    f = h - j;
    p = v * (1-s);
    q = v * (1 - (s*f));
    t = v * (1 - (s*(1 - f)));

    switch (j) {
    case 0:  rd = v;  gd = t;  bd = p;  break;
    case 1:  rd = q;  gd = v;  bd = p;  break;
    case 2:  rd = p;  gd = v;  bd = t;  break;
    case 3:  rd = p;  gd = q;  bd = v;  break;
    case 4:  rd = t;  gd = p;  bd = v;  break;
    case 5:  rd = v;  gd = p;  bd = q;  break;
    default: rd = v;  gd = t;  bd = p;  break;  /* never happen */
    }
  }

  *rr = (int) floor((rd * 255.0) + 0.5);
  *gr = (int) floor((gd * 255.0) + 0.5);
  *br = (int) floor((bd * 255.0) + 0.5);
}



/*********************/
static void ctrls2gamstate(gs)
     struct gamstate *gs;
{
  xvbcopy((char *) hmap, (char *) gs->hmap, sizeof(hmap));

  gs->wht_stval = whtHD.stval;
  gs->wht_satval = whtHD.satval;
  gs->wht_enab = whtHD.enabCB.val;

  gs->hueRBnum = RBWhich(hueRB);

  gs->satval = (int)satDial.val;
  GetGrafState(&intGraf,&gs->istate);
  GetGrafState(&rGraf,  &gs->rstate);
  GetGrafState(&gGraf,  &gs->gstate);
  GetGrafState(&bGraf,  &gs->bstate);
}


/*********************/
static void gamstate2ctrls(gs)
struct gamstate *gs;
{
  int changed = 0;
  struct hmap *hm;

  if (gs->hueRBnum != RBWhich(hueRB)) {
    RBSelect(hueRB, gs->hueRBnum);
    changed++;
  }

  if (xvbcmp((char *) hmap, (char *) gs->hmap, sizeof(hmap))) { /*hmap chngd*/
    xvbcopy((char *) gs->hmap, (char *) hmap, sizeof(hmap));
    build_hremap();
    changed++;

    hm = &(gs->hmap[gs->hueRBnum]);

    if (srcHD.stval  != hm->src_st ||
	srcHD.enval  != hm->src_en ||
	srcHD.ccwise != hm->src_ccw) {
      srcHD.stval  = hm->src_st;
      srcHD.enval  = hm->src_en;
      srcHD.ccwise = hm->src_ccw;
      HDRedraw(&srcHD, HD_ALL | HD_CLEAR);
    }

    if (dstHD.stval  != hm->dst_st ||
	dstHD.enval  != hm->dst_en ||
	dstHD.ccwise != hm->dst_ccw) {
      dstHD.stval  = hm->dst_st;
      dstHD.enval  = hm->dst_en;
      dstHD.ccwise = hm->dst_ccw;
      HDRedraw(&dstHD, HD_ALL | HD_CLEAR);
    }
  }


  if (whtHD.stval != gs->wht_stval || whtHD.satval != gs->wht_satval ||
      whtHD.enabCB.val != gs->wht_enab) {
    whtHD.stval  = gs->wht_stval;
    whtHD.satval  = gs->wht_satval;
    whtHD.enabCB.val = gs->wht_enab;
    CBRedraw(&whtHD.enabCB);
    HDRedraw(&whtHD, HD_ALL | HD_CLEAR);
    changed++;
  }

  if (gs->satval != (int)satDial.val) {
    DSetVal(&satDial,(double)gs->satval);
    changed++;
  }

  if (SetGrafState(&intGraf, &gs->istate)) changed++;
  if (SetGrafState(&rGraf,   &gs->rstate)) changed++;
  if (SetGrafState(&gGraf,   &gs->gstate)) changed++;
  if (SetGrafState(&bGraf,   &gs->bstate)) changed++;

  if (changed) changedGam();
}


static int nosave_kludge = 0;

/*********************/
static void saveGamState()
{
  /* increment uptr, sticks current state into uptr
     set utail = uptr
     If utail==uhead, increment uhead (buffer full, throw away earliest)
   */

  if (nosave_kludge) return;

  uptr = (uptr+1) % MAXUNDO;
  ctrls2gamstate(&undo[uptr]);
  utail = uptr;
  if (utail == uhead) uhead = (uhead + 1) % MAXUNDO;

  BTSetActive(&gbut[G_BUNDO],1);
  BTSetActive(&gbut[G_BREDO],0);
}



/*********************/
static void gamUndo()
{
  /* if uptr!=uhead  decements uptr, restores state pointed to by uptr
                     if uptr now == uhead, turn off 'Undo' button
   */

  if (uptr != uhead) {   /* this should always be true when gamUndo called */
    uptr = (uptr + MAXUNDO - 1) % MAXUNDO;
    nosave_kludge = 1;
    gamstate2ctrls(&undo[uptr]);
    nosave_kludge = 0;
    if (uptr == uhead) BTSetActive(&gbut[G_BUNDO],0);
    if (uptr != utail) BTSetActive(&gbut[G_BREDO],1);
  }
}



/*********************/
static void gamRedo()
{
  /* if uptr != utail   increments uptr, restores state pointed to by uptr
                        if uptr now == utail, turn off 'Redo' button */

  if (uptr != utail) {   /* this should always be true when gamRedo called */
    uptr = (uptr + 1) % MAXUNDO;
    nosave_kludge = 1;
    gamstate2ctrls(&undo[uptr]);
    nosave_kludge = 0;
    if (uptr != uhead) BTSetActive(&gbut[G_BUNDO],1);
    if (uptr == utail) BTSetActive(&gbut[G_BREDO],0);
  }
}




/*********************/
static void rndCols()
{
  int i,j;

  for (i=0; i<numcols; i++) {
    if (cellgroup[i]) {
      /* determine if this group's already been given a random color */
      for (j=0; j<i && cellgroup[i] != cellgroup[j]; j++);
      if (j<i) {  /* it has */
	rcmap[i] = rcmap[j];  gcmap[i] = gcmap[j];  bcmap[i] = bcmap[j];
	continue;
      }
    }

    rcmap[i] = random()&0xff;
    gcmap[i] = random()&0xff;
    bcmap[i] = random()&0xff;
  }

  ChangeEC(editColor);
  applyGamma(1);
}



/*********************/
static void saveCMap(cst)
struct cmapstate *cst;
{
  int i;

  for (i=0; i<256; i++) {
    cst->rm[i] = rcmap[i];
    cst->gm[i] = gcmap[i];
    cst->bm[i] = bcmap[i];
    cst->cellgroup[i] = cellgroup[i];
  }

  cst->curgroup  = curgroup;
  cst->maxgroup  = maxgroup;
  cst->editColor = editColor;
}


/*********************/
static void restoreCMap(cst)
struct cmapstate *cst;
{
  int i;

  for (i=0; i<256; i++) {
    rcmap[i] = cst->rm[i];
    gcmap[i] = cst->gm[i];
    bcmap[i] = cst->bm[i];
    cellgroup[i] = cst->cellgroup[i];
  }

  curgroup = cst->curgroup;
  maxgroup = cst->maxgroup;
  editColor = cst->editColor;
}




/*********************/
static void parseResources()
{
  char gname[80], tmp[80], tmp1[256];
  int  i,j;
  struct gamstate *gsp, gs;


  /* look for the simple coledit-related resources */
  if (rd_flag("autoApply"))   autoCB.val = def_int;
  if (rd_flag("dragApply"))   dragCB.val = def_int;
  if (rd_flag("displayMods")) enabCB.val = def_int;
  if (rd_flag("autoReset"))   resetCB.val = def_int;

  defAutoApply = autoCB.val;

  /* look for preset/default resources */
  for (i=0; i<5; i++) {
    if (i) { sprintf(gname,"preset%d",i);  gsp = &preset[i-1]; }
      else { sprintf(gname,"default");     gsp = &defstate; }

    xvbcopy((char *) gsp, (char *) &gs,
	    sizeof(struct gamstate));   /* load 'gs' with defaults */

    for (j=0; j<6; j++) {                       /* xv.*.huemap resources */
      sprintf(tmp, "%s.huemap%d", gname, j+1);
      if (rd_str_cl(tmp, "Setting.Huemap",0)) {   /* got one */
	int fst, fen, tst, ten;
	char fcw[32], tcw[32];

	if (DEBUG) fprintf(stderr,"parseResource 'xv.%s: %s'\n",tmp, def_str);
	lower_str(def_str);
	if (sscanf(def_str,"%d %d %s %d %d %s",
		   &fst, &fen, fcw, &tst, &ten, tcw) != 6) {
	  fprintf(stderr,"%s: unable to parse resource 'xv.%s: %s'\n",
		  cmd, tmp, def_str);
	}
	else {
	  RANGE(fst, 0, 359);   RANGE(fen, 0, 359);
	  RANGE(tst, 0, 359);   RANGE(ten, 0, 359);
	  gs.hmap[j].src_st  = fst;
	  gs.hmap[j].src_en  = fen;
	  gs.hmap[j].src_ccw = (strcmp(fcw,"ccw") == 0);
	  gs.hmap[j].dst_st  = tst;
	  gs.hmap[j].dst_en  = ten;
	  gs.hmap[j].dst_ccw = (strcmp(tcw,"ccw") == 0);
	}
      }
    }

    sprintf(tmp, "%s.whtmap", gname);           /* xv.*.whtmap resource */
    if (rd_str_cl(tmp, "Setting.Whtmap",0)) {        /* got one */
      int wst, wsat, enab;
      if (DEBUG) fprintf(stderr,"parseResource 'xv.%s: %s'\n",tmp, def_str);
      if (sscanf(def_str,"%d %d %d", &wst, &wsat, &enab) != 3) {
	fprintf(stderr,"%s: unable to parse resource 'xv.%s: %s'\n",
		cmd, tmp, def_str);
      }
      else {                                    /* successful parse */
	RANGE(wst, 0, 359);  RANGE(wsat, 0, 100);
	gs.wht_stval  = wst;
	gs.wht_satval = wsat;
	gs.wht_enab   = enab;
      }
    }

    sprintf(tmp, "%s.satval", gname);           /* xv.*.satval resource */
    if (rd_str_cl(tmp, "Setting.Satval",0)) {         /* got one */
      int sat;
      if (DEBUG) fprintf(stderr,"parseResource 'xv.%s: %s'\n",tmp, def_str);
      if (sscanf(def_str,"%d", &sat) != 1) {
	fprintf(stderr,"%s: unable to parse resource 'xv.%s: %s'\n",
		cmd, tmp, def_str);
      }
      else {                                    /* successful parse */
	RANGE(sat, -100, 100);
	gs.satval = sat;
      }
    }

    for (j=0; j<4; j++) {                       /* xv.*.*graf resources */
      GRAF_STATE gstat, *gsgst;
      switch (j) {
      case 0: sprintf(tmp, "%s.igraf", gname);  gsgst = &gs.istate; break;
      case 1: sprintf(tmp, "%s.rgraf", gname);  gsgst = &gs.rstate; break;
      case 2: sprintf(tmp, "%s.ggraf", gname);  gsgst = &gs.gstate; break;
      case 3: sprintf(tmp, "%s.bgraf", gname);  gsgst = &gs.bstate; break;
      default: sprintf(tmp, "%s.bgraf", gname);  gsgst = &gs.bstate; break;
      }

      if (rd_str_cl(tmp, "Setting.Graf",0)) {       /* got one */
	strcpy(tmp1, def_str);
	xvbcopy((char *) gsgst, (char *) &gstat, sizeof(GRAF_STATE));
	if (DEBUG) fprintf(stderr,"parseResource 'xv.%s: %s'\n",tmp, tmp1);
	if (!Str2Graf(&gstat, tmp1)) {            /* successful parse */
	  xvbcopy((char *) &gstat, (char *) gsgst, sizeof(GRAF_STATE));
	}
      }
    }

    /* copy (potentially) modified gs back to default/preset */
    xvbcopy((char *) &gs, (char *) gsp, sizeof(struct gamstate));
  }
}


/*********************/
static void makeResources()
{
  char rsrc[2000];     /* wild over-estimation */
  char gname[40], rname[64], tmp[256], tmp1[256];
  struct gamstate gstate;
  int i;

  rsrc[0] = '\0';

  /* write out current state */
  ctrls2gamstate(&gstate);
  strcpy(gname, "xv.default");

  /* write out huemap resources */
  for (i=0; i<6; i++) {
    if (1 || gstate.hmap[i].src_st  != gstate.hmap[i].dst_st ||
	gstate.hmap[i].src_en  != gstate.hmap[i].dst_en ||
	gstate.hmap[i].src_ccw != gstate.hmap[i].dst_ccw) {
      sprintf(tmp, "%s.huemap%d: %3d %3d %3s %3d %3d %3s\n", gname, i+1,
	      gstate.hmap[i].src_st, gstate.hmap[i].src_en,
	      gstate.hmap[i].src_ccw ? "CCW" : "CW",
	      gstate.hmap[i].dst_st, gstate.hmap[i].dst_en,
	      gstate.hmap[i].dst_ccw ? "CCW" : "CW");
      strcat(rsrc, tmp);
    }
  }

  /* write out whtmap resource */
  if (1 || gstate.wht_stval || gstate.wht_satval || gstate.wht_enab != 1) {
    sprintf(tmp, "%s.whtmap:  %d %d %d\n", gname, gstate.wht_stval,
	    gstate.wht_satval, gstate.wht_enab);
    strcat(rsrc, tmp);
  }

  /* write out satval resource */
  if (1 || gstate.satval) {
    sprintf(tmp, "%s.satval:  %d\n", gname, gstate.satval);
    strcat(rsrc, tmp);
  }


  /* write out graf resources */
  for (i=0; i<4; i++) {
    GRAF_STATE *gfstat;
    switch (i) {
    case 0: sprintf(rname, "%s.igraf", gname);  gfstat = &gstate.istate; break;
    case 1: sprintf(rname, "%s.rgraf", gname);  gfstat = &gstate.rstate; break;
    case 2: sprintf(rname, "%s.ggraf", gname);  gfstat = &gstate.gstate; break;
    case 3: sprintf(rname, "%s.bgraf", gname);  gfstat = &gstate.bstate; break;
    default:
      sprintf(rname, "%s.bgraf", gname);  gfstat = &gstate.bstate; break;
    }

    Graf2Str(gfstat, tmp1);
    sprintf(tmp, "%s: %s\n", rname, tmp1);
    strcat(rsrc, tmp);
  }

  NewCutBuffer(rsrc);
}


/*****************************/
static void dragGamma ()
{
  /* called through DIAL.drawobj and GRAF.drawobj
     while gamma ctrls are being dragged
     applies change to image if dragCB.val is set
     does NOT call saveGamState() (as changedGam does) */

  if (dragCB.val && dragCB.active) {
    hsvnonlinear = 1;   /* force HSV calculations during drag */
    applyGamma(0);
  }
}


/*****************************/
static void dragHueDial()
{
  /* called through HDIAL.drawobj
     while hue gamma ctrls are being dragged
     applies change to image if dragCB.val is set
     does NOT call saveGamState() (as changedGam does) */

  if (dragCB.val && dragCB.active) {
    dials2hmap();
    build_hremap();
    hsvnonlinear = 1;   /* force HSV calculations during drag */
    applyGamma(0);
  }
}


/*****************************/
static void dragEditColor()
{
  /* called through DIAL.drawobj
     while color editor ctrls are being dragged
     applies change to image if dragCB.val is set
     does NOT call saveCMap(&prevcmap); BTSetActive(&gbut[G_BCOLUNDO],1); */

  if (dragCB.val && dragCB.active) ApplyEditColor(0);
}






/**********************************************/
/*************  HUE wheel functions ***********/
/**********************************************/


static int hdb_pixmaps_built = 0;
static Pixmap hdbpix1[N_HDBUTT];
static Pixmap hdbpix2[N_HDBUTT2];
#define PW 15
#define PH 15

/**************************************************/
static void HDCreate(hd, win, x, y, r, st, en, ccwise, str, fg, bg)
     HDIAL      *hd;
     Window      win;
     int         x, y, r, st, en, ccwise;
     const char *str;
     u_long      fg, bg;
{
  int i;

  hd->win    = win;
  hd->x      = x;
  hd->y      = y;
  hd->range  = r;
  hd->stval  = st;
  hd->enval  = en;
  hd->satval = 0;
  hd->ccwise = ccwise;
  hd->str    = str;
  hd->fg     = fg;
  hd->bg     = bg;

  hd->drawobj = NULL;

  if (!hdb_pixmaps_built) {
    hdbpix1[HDB_ROTL]   = MakePix1(win, h_rotl_bits, PW,PH);
    hdbpix1[HDB_ROTR]   = MakePix1(win, h_rotr_bits, PW,PH);
    hdbpix1[HDB_FLIP]   = MakePix1(win, h_flip_bits, PW,PH);
    hdbpix1[HDB_EXPND]  = MakePix1(win, h_sinc_bits, PW,PH);
    hdbpix1[HDB_SHRNK]  = MakePix1(win, h_sdec_bits, PW,PH);
    hdbpix2[HDB_SAT]    = MakePix1(win, h_sat_bits,  PW,PH);
    hdbpix2[HDB_DESAT]  = MakePix1(win, h_desat_bits,PW,PH);

    hdbpix2[HDB_ROTL]  = hdbpix1[HDB_ROTL];
    hdbpix2[HDB_ROTR]  = hdbpix1[HDB_ROTR];
  }


#define BCOLS fg,bg,hicol,locol

  if (hd->range) {
    BTCreate(&hd->hdbutt[HDB_ROTL], win, x-50,y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_ROTR], win, x-30,y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_FLIP], win, x-10,y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_EXPND],win, x+10,y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_SHRNK],win, x+30,y+60,18,18,NULL, BCOLS);

    for (i=0; i<N_HDBUTT; i++) {
      hd->hdbutt[i].pix = hdbpix1[i];
      hd->hdbutt[i].pw = PW;
      hd->hdbutt[i].ph = PH;
    }
  }

  else {
    BTCreate(&hd->hdbutt[HDB_ROTL], win, x-39,y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_ROTR], win, x-19,y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_DESAT],win, x+1, y+60,18,18,NULL, BCOLS);
    BTCreate(&hd->hdbutt[HDB_SAT],  win, x+21,y+60,18,18,NULL, BCOLS);
    CBCreate(&hd->enabCB, win, x+23, y-44, "", BCOLS);
    hd->enabCB.val = 1;

    for (i=0; i<N_HDBUTT2; i++) {
      hd->hdbutt[i].pix = hdbpix2[i];
      hd->hdbutt[i].pw = PW;
      hd->hdbutt[i].ph = PH;
    }
  }
#undef BCOLS
}


/**************************************************/
static void HDRedraw(hd, flags)
HDIAL *hd;
int flags;
{
  int    i, x, y, x1, y1;
  double a;

  if (flags & HD_CLEAR) {
    XSetForeground(theDisp, theGC, hd->bg);
    XFillArc(theDisp, hd->win, theGC, hd->x-(HD_RADIUS-1),hd->y-(HD_RADIUS-1),
	     (HD_RADIUS-1)*2, (HD_RADIUS-1)*2, 0, 360*64);
  }

  if (flags & HD_FRAME) {
    static const char *colstr = "RYGCBM";
    char tstr[2];

    XSetForeground(theDisp, theGC, hd->fg);
    XDrawArc(theDisp, hd->win, theGC, hd->x - HD_RADIUS, hd->y - HD_RADIUS,
	     HD_RADIUS*2, HD_RADIUS*2, 0, 360*64);

    for (i=0; i<6; i++) {
      int kldg;

      if (i==2 || i==5) kldg = -1;  else kldg = 0;
      a = hdg2xdg(i*60) * DEG2RAD;
      pol2xy(hd->x, hd->y, a, HD_RADIUS+1,      &x,  &y);
      pol2xy(hd->x, hd->y, a, HD_RADIUS+4+kldg, &x1, &y1);
      XDrawLine(theDisp, hd->win, theGC, x,y,x1,y1);

      tstr[0] = colstr[i];  tstr[1] = '\0';
      pol2xy(hd->x, hd->y, a, HD_RADIUS+10, &x, &y);
      CenterString(hd->win, x, y, tstr);
    }
  }

  if (flags & HD_HANDS || flags & HD_CLHNDS) {
    if (flags & HD_CLHNDS) XSetForeground(theDisp, theGC, hd->bg);
                      else XSetForeground(theDisp, theGC, hd->fg);

    if (hd->range ) {
      if (flags & HD_HANDS)   /* draw center dot */
	XFillRectangle(theDisp, hd->win, theGC, hd->x-1, hd->y-1, 3,3);

      a = hdg2xdg(hd->stval) * DEG2RAD;
      pol2xy(hd->x, hd->y, a, HD_RADIUS - 4, &x, &y);
      XDrawLine(theDisp, hd->win, theGC, hd->x, hd->y, x,y);

      if (flags & HD_CLHNDS)
	XFillRectangle(theDisp, hd->win, theGC, x-2,y-2, 5,5);
      else {
	XSetForeground(theDisp, theGC, hd->bg);
	XFillRectangle(theDisp, hd->win, theGC, x-1, y-1, 3,3);
	XSetForeground(theDisp, theGC, hd->fg);
	XDrawPoint(theDisp, hd->win, theGC, x, y);
	XDrawRectangle(theDisp, hd->win, theGC, x-2, y-2, 4,4);
      }

      a = hdg2xdg(hd->enval) * DEG2RAD;
      pol2xy(hd->x, hd->y, a, HD_RADIUS - 4, &x, &y);
      XDrawLine(theDisp, hd->win, theGC, hd->x, hd->y, x,y);

      if (flags & HD_CLHNDS)
	XFillRectangle(theDisp, hd->win, theGC, x-2,y-2, 5,5);
      else {
	XSetForeground(theDisp, theGC, hd->bg);
	XFillRectangle(theDisp, hd->win, theGC, x-1, y-1, 3,3);
	XSetForeground(theDisp, theGC, hd->fg);
	XDrawPoint(theDisp, hd->win, theGC, x, y);
	XDrawRectangle(theDisp, hd->win, theGC, x-2, y-2, 4,4);
      }
    }

    else {  /* not a range;  hue/sat dial */
      int r;

      /* compute x,y position from stval/satval */
      a = hdg2xdg(hd->stval) * DEG2RAD;
      r = ((HD_RADIUS - 4) * hd->satval) / 100;
      pol2xy(hd->x, hd->y, a, r, &x, &y);

      if (flags & HD_CLHNDS)
	XFillRectangle(theDisp, hd->win, theGC, x-2,y-2, 5,5);
      else {
	XFillRectangle(theDisp, hd->win, theGC, hd->x-1, hd->y-1, 3,3);

	XSetForeground(theDisp, theGC, hd->bg);
	XFillRectangle(theDisp, hd->win, theGC, x-1, y-1, 3,3);
	XSetForeground(theDisp, theGC, hd->fg);
	XDrawPoint(theDisp, hd->win, theGC, x, y);
	XDrawRectangle(theDisp, hd->win, theGC, x-2, y-2, 4,4);
      }
    }
  }




  if ((flags & HD_DIR || flags & HD_CLHNDS) && hd->range) {
    int xdg1, xdg2, xdlen;

    if (flags & HD_CLHNDS) XSetForeground(theDisp, theGC, hd->bg);
                      else XSetForeground(theDisp, theGC, hd->fg);

    if (hd->ccwise) {
      xdg1 = hdg2xdg(hd->stval);
      xdg2 = hdg2xdg(hd->enval);
    }
    else {
      xdg1 = hdg2xdg(hd->enval);
      xdg2 = hdg2xdg(hd->stval);
    }

    xdlen = xdg2 - xdg1;
    if (xdlen<0) xdlen += 360;   /* note: 0 len means no range */

    if (xdlen>1) {
      XDrawArc(theDisp, hd->win, theGC, hd->x - ((HD_RADIUS*3)/5),
	       hd->y - ((HD_RADIUS*3)/5), (HD_RADIUS*6)/5, (HD_RADIUS*6)/5,
	       xdg1 * 64, xdlen * 64);
    }

    if (xdlen > 16) {
      xdg1 = hdg2xdg(hd->enval);
      if (hd->ccwise) xdg2 = xdg1-10;
                 else xdg2 = xdg1+10;

      pol2xy(hd->x, hd->y, xdg2 * DEG2RAD, ((HD_RADIUS*3)/5) + 3, &x, &y);
      pol2xy(hd->x, hd->y, xdg1 * DEG2RAD, ((HD_RADIUS*3)/5),     &x1,&y1);
      XDrawLine(theDisp, hd->win, theGC, x, y, x1, y1);

      if (hd->ccwise) xdg2 = xdg1-16;
                 else xdg2 = xdg1+16;
      pol2xy(hd->x, hd->y, xdg2 * DEG2RAD, ((HD_RADIUS*3)/5) - 2, &x, &y);
      XDrawLine(theDisp, hd->win, theGC, x, y, x1, y1);
    }
  }


  if (flags & HD_VALS) {
    char vstr[32];

    XSetFont(theDisp, theGC, monofont);
    XSetForeground(theDisp, theGC, hd->fg);
    XSetBackground(theDisp, theGC, hd->bg);

    if (hd->range) {
      sprintf(vstr,"%3d\007,%3d\007 %s", hd->stval, hd->enval,
	      hd->ccwise ? "CCW" : " CW");
    }
    else {
      sprintf(vstr,"%3d\007 %3d%%", hd->stval, hd->satval);
    }

    XDrawImageString(theDisp, hd->win, theGC,
		     hd->x - XTextWidth(monofinfo, vstr, (int) strlen(vstr))/2,
		     hd->y + HD_RADIUS + 24, vstr, (int) strlen(vstr));
    XSetFont(theDisp, theGC, mfont);
  }


  if (flags & HD_TITLE) {
    XSetForeground(theDisp, theGC, hd->fg);
    ULineString(hd->win, hd->x - HD_RADIUS - 15, hd->y - HD_RADIUS - 4,
		hd->str);
  }


  if (flags & HD_BUTTS) {
    if (hd->range) {
      for (i=0; i<N_HDBUTT; i++) BTRedraw(&hd->hdbutt[i]);
    }
    else {
      for (i=0; i<N_HDBUTT2; i++) BTRedraw(&hd->hdbutt[i]);
      CBRedraw(&hd->enabCB);
    }
  }

  if (!hd->range && !hd->enabCB.val) {  /* draw dimmed */
    XSync(theDisp, False);
    DimRect(hd->win, hd->x-HD_RADIUS-15, hd->y-HD_RADIUS-4-ASCENT,
	    (u_int) 2*HD_RADIUS+30, (u_int) (2*HD_RADIUS+4+ASCENT+80), hd->bg);
    XSync(theDisp, False);
    CBRedraw(&hd->enabCB);
  }
}



/**************************************************/
static int HDClick(hd,mx,my)
HDIAL *hd;
int mx, my;
{
  /* called when a click received.  checks whether HDTrack needs to be
     called.  Returns '1' if click is in HD dial area, 0 otherwise */

  int bnum,maxb;
  BUTT *bp = (BUTT *) NULL;

  if (CBClick(&hd->enabCB, mx, my)) {
    if (CBTrack(&hd->enabCB)) {
      HDRedraw(hd, HD_ALL);
      return 1;
    }
  }

  if (!hd->range && !hd->enabCB.val) return 0;    /* disabled */


  if ( ((mx - hd->x) * (mx - hd->x)  +  (my - hd->y) * (my - hd->y))
      < (HD_RADIUS * HD_RADIUS)) {
    return HDTrack(hd,mx,my);
  }

  if (hd->range) maxb = N_HDBUTT;  else maxb = N_HDBUTT2;

  for (bnum=0; bnum<maxb; bnum++) {
    bp = &hd->hdbutt[bnum];
    if (PTINRECT(mx,my, bp->x, bp->y, bp->w, bp->h)) break;
  }
  if (bnum==maxb) return 0;

  if (bnum==HDB_FLIP && hd->range) {
    if (BTTrack(bp)) {
      int t;
      HDRedraw(hd, HD_CLHNDS);
      t = hd->stval;  hd->stval = hd->enval;  hd->enval = t;
      hd->ccwise = !hd->ccwise;
	  HDRedraw(hd, HD_HANDS | HD_DIR | HD_VALS);

      if (hd->drawobj) (hd->drawobj)();
      return 1;
    }
    return 0;
  }

  else {    /* track buttons til mouse up */
    Window rW, cW;
    int    rx,ry,x,y;
    unsigned int mask;

    bp->lit = 1;  BTRedraw(bp);  /* light it up */

    /* loop until mouse is released */
    while (XQueryPointer(theDisp,hd->win,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
      if (!(mask & Button1Mask)) break;    /* button released */

      /* check to see if state needs toggling */
      if (( bp->lit && !PTINRECT(x,y,bp->x,bp->y,bp->w,bp->h)) ||
	  (!bp->lit &&  PTINRECT(x,y,bp->x,bp->y,bp->w,bp->h))) {
	bp->lit = !bp->lit;
	BTRedraw(bp);
      }

      if (bp->lit) {
	switch (bnum) {
	case HDB_ROTL:
	  HDRedraw(hd, HD_CLHNDS);
	  hd->stval--;  if (hd->stval<0) hd->stval += 360;
	  hd->enval--;  if (hd->enval<0) hd->enval += 360;
	  HDRedraw(hd, HD_HANDS | HD_DIR | HD_VALS);
	  break;

	case HDB_ROTR:
	  HDRedraw(hd, HD_CLHNDS);
	  hd->stval++;  if (hd->stval>=360) hd->stval -= 360;
	  hd->enval++;  if (hd->enval>=360) hd->enval -= 360;
	  HDRedraw(hd, HD_HANDS | HD_DIR | HD_VALS);
	  break;

	/* case HDB_DESAT: */
	/* case HDB_SAT:   */
	case HDB_EXPND:
	case HDB_SHRNK:
	  if (hd->range) {
	    if ((bnum == HDB_EXPND) &&
		(( hd->ccwise && hd->enval == (hd->stval+1))  ||
		 (!hd->ccwise && hd->enval == (hd->stval-1))))
		{ /* max size:  can't grow */ }

	    else if (bnum == HDB_SHRNK && hd->stval == hd->enval)
	      { /* min size, can't shrink */ }

	    else {   /* can shrink or grow */
	      HDRedraw(hd, HD_CLHNDS);
	      if ((bnum==HDB_EXPND &&  hd->ccwise) ||
		  (bnum==HDB_SHRNK && !hd->ccwise)) {
		if ((hd->stval & 1) == (hd->enval & 1))
		  hd->stval = (hd->stval + 1 + 360) % 360;
		else
		  hd->enval = (hd->enval - 1 + 360) % 360;
	      }

	      else {
		if ((hd->stval & 1) == (hd->enval & 1))
		  hd->stval = (hd->stval - 1 + 360) % 360;
		else
		  hd->enval = (hd->enval + 1 + 360) % 360;
	      }
	      HDRedraw(hd, HD_HANDS | HD_DIR | HD_VALS);
	    }
	  }

	  else {   /* hue/sat dial:  SAT/DESAT */
	    if (bnum == HDB_DESAT && hd->satval>0) {
	      HDRedraw(hd, HD_CLHNDS);
	      hd->satval--;  if (hd->satval<0) hd->satval = 0;
	      HDRedraw(hd, HD_HANDS | HD_VALS);
	    }

	    else if (bnum == HDB_SAT && hd->satval<100) {
	      HDRedraw(hd, HD_CLHNDS);
	      hd->satval++;  if (hd->satval>100) hd->satval = 100;
	      HDRedraw(hd, HD_HANDS | HD_VALS);
	    }
	  }

	  break;
	}

	if (hd->drawobj) (hd->drawobj)();
	Timer(150);
      }
    }

    XFlush(theDisp);
  }

  if (bp->lit) {  bp->lit = 0;  BTRedraw(bp); }

  return 1;
}



/**************************************************/
static int HDTrack(hd,mx,my)
HDIAL *hd;
int mx,my;
{
  /* called when clicked in dial area.  tracks dragging handles around...
     returns '1' if anything changed */

  Window       rW,cW;
  int          rx,ry, x,y, ival,j, rv;
  unsigned int mask;

  rv = 0;

  if (!hd->range) {     /* hue/sat dial */
    int ihue, isat, newhue, newsat;
    double dx,dy,dist;

    ihue = hd->stval;  isat = hd->satval;

    /* loop until mouse is released */
    while (XQueryPointer(theDisp,hd->win,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
      if (!(mask & Button1Mask)) break;    /* button released */

      /* compute new value based on mouse pos */
      newhue = computeHDval(hd, x,y);
      if (newhue<0) newhue = hd->stval;  /* at 'spaz' point, keep hue const */

      dx = x - hd->x;  dy = y - hd->y;
      dist = sqrt(dx*dx + dy*dy);

      newsat = (int) (dist / ((double) (HD_RADIUS - 4)) * 100);
      RANGE(newsat,0,100);

      if (newhue != hd->stval || newsat != hd->satval) {
	HDRedraw(hd, HD_CLHNDS);
	hd->stval = newhue;  hd->satval = newsat;
	HDRedraw(hd, HD_HANDS | HD_VALS);
	if (hd->drawobj) (hd->drawobj)();
      }
    }

    rv = (hd->stval != ihue || hd->satval != isat);
  }

  else {   /* the hard case */
    double a;
    int    handle = 0, *valp;

    /* determine if we're in either of the handles */
    a = hdg2xdg(hd->stval) * DEG2RAD;
    pol2xy(hd->x, hd->y, a, HD_RADIUS-4, &x,&y);
    if (PTINRECT(mx,my,x-3,y-3,7,7)) handle = 1;

    a = hdg2xdg(hd->enval) * DEG2RAD;
    pol2xy(hd->x, hd->y, a, HD_RADIUS-4, &x,&y);
    if (PTINRECT(mx,my,x-3,y-3,7,7)) handle = 2;



    if (!handle) {  /* not in either, rotate both */
      int oldj, len, origj;

      if (!hd->ccwise) {
	len = ((hd->enval - hd->stval + 360) % 360);
	oldj = (hd->stval + len/2 + 360) % 360;
      }
      else {
	len = ((hd->stval - hd->enval + 360) % 360);
	oldj = (hd->enval + len/2 + 360) % 360;
      }

      origj = j = oldj;

      while (XQueryPointer(theDisp,hd->win,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	if (!(mask & Button1Mask)) break;    /* button released */

	/* compute new value based on mouse pos */
	j = computeHDval(hd, x,y);
	if (j>=0 && j != oldj) {
	  oldj = j;
	  HDRedraw(hd, HD_CLHNDS);
	  if (!hd->ccwise) {
	    hd->stval = (j - len/2 + 360) % 360;
	    hd->enval = (j + len/2 + 360) % 360;
	  }
	  else {
	    hd->stval = (j + len/2 + 360) % 360;
	    hd->enval = (j - len/2 + 360) % 360;
	  }
	  HDRedraw(hd, HD_HANDS | HD_DIR | HD_VALS);

	  if (hd->drawobj) (hd->drawobj)();
	}
      }
      rv = (origj != j);
    }


    else {  /* in one of the handles */
      if (handle==1) valp = &(hd->stval);  else valp = &(hd->enval);
      ival = *valp;

      /* loop until mouse is released */
      while (XQueryPointer(theDisp,hd->win,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
	if (!(mask & Button1Mask)) break;    /* button released */

	/* compute new value based on mouse pos */
	j = computeHDval(hd, x,y);
	if (j>=0 && j != *valp) {
	  int ndist, ddist;

	  HDRedraw(hd, HD_CLHNDS);

	  if (!hd->ccwise) {
	    ddist = (hd->enval - hd->stval + 360) % 360;
	    if (handle==1)
	      ndist = (hd->enval - j + 360) % 360;
	    else
	      ndist = (j - hd->stval + 360) % 360;
	  }
	  else {
	    ddist = (hd->stval - hd->enval + 360) % 360;
	    if (handle==1)
	      ndist = (j - hd->enval + 360) % 360;
	    else
	      ndist = (hd->stval - j + 360) % 360;
	  }

	  if (abs(ddist - ndist) >= 180 && ddist<180)
	    hd->ccwise = !hd->ccwise;

	  *valp = j;
	  HDRedraw(hd, HD_HANDS | HD_DIR | HD_VALS);

	  if (hd->drawobj) (hd->drawobj)();
	}
      }
      rv = (*valp != ival);
    }
  }

  return rv;
}



/**************************************************/
static int hdg2xdg(hdg)
int hdg;
{
  int xdg;

  xdg = 270 - hdg;
  if (xdg < 0)   xdg += 360;
  if (xdg > 360) xdg -= 360;

  return xdg;
}


/**************************************************/
static void pol2xy(cx, cy, ang, rad, xp, yp)
int cx, cy, rad, *xp, *yp;
double ang;
{
  *xp = cx + (int) (cos(ang) * (double) rad);
  *yp = cy - (int) (sin(ang) * (double) rad);
}


/***************************************************/
static int computeHDval(hd, x, y)
HDIAL *hd;
int x, y;
{
  int dx, dy;
  double angle;

  /* compute dx, dy (distance from center).  Note: +dy is *up* */
  dx = x - hd->x;  dy = hd->y - y;

  /* if too close to center, return -1 (invalid) avoid 'spazzing' */
  if (abs(dx) < 3 && abs(dy) < 3) return -1;

  /* figure out angle of vector dx,dy */
  if (dx==0) {     /* special case */
    if (dy>0) angle =  90.0;
         else angle = -90.0;
  }
  else if (dx>0) angle = atan((double)  dy / (double)  dx) * RAD2DEG;
  else           angle = atan((double) -dy / (double) -dx) * RAD2DEG + 180.0;

  angle = 270 - angle;   /* map into h-degrees */
  if (angle >= 360.0) angle -= 360.0;
  if (angle <    0.0) angle += 360.0;

  return (int) angle;
}




/****************************************************/
static void initHmap()
{
  int i;
  for (i=0; i<N_HMAP; i++) init1hmap(i);
}


/****************************************************/
static void init1hmap(i)
int i;
{
  int cang, width;

  width = 360 / N_HMAP;

  cang = i * width;
  hmap[i].src_st  = hmap[i].dst_st = (cang - width/2 + 360) % 360;
  hmap[i].src_en  = hmap[i].dst_en = (cang - width/2 + width +360) % 360;
  hmap[i].src_ccw = hmap[i].dst_ccw = 0;
}


/****************************************************/
static void dials2hmap()
{
  int i;
  i = RBWhich(hueRB);

  hmap[i].src_st  = srcHD.stval;
  hmap[i].src_en  = srcHD.enval;
  hmap[i].src_ccw = srcHD.ccwise;

  hmap[i].dst_st  = dstHD.stval;
  hmap[i].dst_en  = dstHD.enval;
  hmap[i].dst_ccw = dstHD.ccwise;
}


/****************************************************/
static void hmap2dials()
{
  int i;
  i = RBWhich(hueRB);

  srcHD.stval  = hmap[i].src_st;
  srcHD.enval  = hmap[i].src_en;
  srcHD.ccwise = hmap[i].src_ccw;

  dstHD.stval  = hmap[i].dst_st;
  dstHD.enval  = hmap[i].dst_en;
  dstHD.ccwise = hmap[i].dst_ccw;

  HDRedraw(&srcHD, HD_ALL | HD_CLEAR);
  HDRedraw(&dstHD, HD_ALL | HD_CLEAR);
}


/****************************************************/
static void build_hremap()
{
  int i,j, st1, en1, inc1, len1, st2, en2, inc2, len2;
  int a1, a2;

  /* start with a 1:1 mapping */
  for (i=0; i<360; i++) hremap[i] = i;

  for (i=0; i<N_HMAP; i++) {
    if ((hmap[i].src_st  != hmap[i].dst_st) ||
	(hmap[i].src_en  != hmap[i].dst_en) ||
	(hmap[i].src_ccw != hmap[i].dst_ccw)) {   /* not a 1:1 mapping */

      st1  = hmap[i].src_st;
      en1  = hmap[i].src_en;
      if (hmap[i].src_ccw) {
	inc1 = -1;
	len1 = (st1 - en1 + 360) % 360;
      }
      else {
	inc1 = 1;
	len1 = (en1 - st1 + 360) % 360;
      }

      st2 = hmap[i].dst_st;
      en2 = hmap[i].dst_en;
      if (hmap[i].dst_ccw) {
	inc2 = -1;
	len2 = (st2 - en2 + 360) % 360;
      }
      else {
	inc2 = 1;
	len2 = (en2 - st2 + 360) % 360;
      }

      if (len1==0) {
	a1 = st1;
	a2 = st2;
	hremap[a1] = a2;
      }
      else {
	if (DEBUG) fprintf(stderr,"mapping %d: %d,%d %s  %d,%d %s\n",
			   i, hmap[i].src_st, hmap[i].src_en,
			   hmap[i].src_ccw ? "ccw" : "cw",
			   hmap[i].dst_st, hmap[i].dst_en,
			   hmap[i].dst_ccw ? "ccw" : "cw");

	for (j=0, a1=st1; j<=len1; a1 = (a1 + inc1 + 360) % 360, j++) {
	  a2 = (((inc2 * len2 * j) / len1 + st2) + 360) % 360;
	  hremap[a1] = a2;
	  if (DEBUG) fprintf(stderr,"(%d->%d)  ", a1, a2);
	}
	if (DEBUG) fprintf(stderr,"\n");
      }
    }
  }
}






/*********************/
byte *GammifyPic24(pic24, wide, high)
     byte *pic24;
     int   wide,high;
{
  /* applies HSV/RGB modifications to each pixel in given 24-bit image.
     creates and returns a new picture, or NULL on failure.
     Also, checks to see if the result will be the same as the input, and
     if so, also returns NULL, as a time-saving maneuver */

  byte *pp, *op;
  int   i,j;
  int   rv, gv, bv;
  byte *outpic;
  int   min, max, del, h, s, v;
  int   f, p, q, t, vs100, vsf10000;
  int   hsvmod, rgbmod;

  outpic = (byte *) NULL;
  if (!enabCB.val) return outpic;              /* mods turned off */

  printUTime("start of GammifyPic24");

  /* check for HSV/RGB control linearity */

  hsvmod = rgbmod = 0;

  /* check HUE remapping */
  for (i=0; i<360 && hremap[i] == i; i++);
  if (i!=360) hsvmod++;

  if (whtHD.enabCB.val && whtHD.satval) hsvmod++;

  if (satDial.val != 0.0) hsvmod++;

  /* check intensity graf */
  for (i=0; i<256; i++) {
    if (intGraf.func[i] != i) break;
  }
  if (i<256) hsvmod++;


  /* check R,G,B grafs simultaneously */
  for (i=0; i<256; i++) {
    if (rGraf.func[i] != i ||
	gGraf.func[i] != i ||
	bGraf.func[i] != i) break;
  }
  if (i<256) rgbmod++;


  if (!hsvmod && !rgbmod) return outpic;  /* apparently, it's linear */



  WaitCursor();

  printUTime("NonLINEAR");

  outpic = (byte *) malloc((size_t) wide * high * 3);
  if (!outpic) return outpic;

  pp = pic24;  op = outpic;
  for (i=wide * high; i; i--) {

    if ((i&0x7fff)==0) WaitCursor();

    rv = *pp++;  gv = *pp++;  bv = *pp++;

    if (hsvmod) {
      /* convert RGB to HSV */
      /* the HSV computed will be int's ranging -1..359, 0..100, 0..255 */

      max = (rv>gv) ? rv : gv;      /* compute maximum of rv,gv,bv */
      if (max<bv) max = bv;

      min = (rv<gv) ? rv : gv;      /* compute minimum of rd,gd,bd */
      if (min>bv) min=bv;

      del = max - min;
      v = max;
      if (max != 0) s = (del * 100) / max;
               else s = 0;

      h = NOHUE;
      if (s) {
	if      (rv==max) h =       ((gv - bv) * 100) / del;
	else if (gv==max) h = 200 + ((bv - rv) * 100) / del;
	else if (bv==max) h = 400 + ((rv - gv) * 100) / del;


	/* h is in range -100..500  (= -1.0 .. 5.0) */
	if (h<0) h += 600;          /* h is in range 000..600  (0.0 .. 6.0) */
	h = (h * 60) / 100;         /* h is in range 0..360 */
	if (h>=360) h -= 360;
      }


      /* apply HSV mods */


      /* map near-black to black to avoid weird effects */
      if (v <= 16) s = 0;

      /* apply intGraf.func[] function to 'v' (the intensity) */
      v = intGraf.func[v];

      /* do Hue remapping */
      if (h>=0) h = hremap[h];
      else {  /* NOHUE */
	if (whtHD.enabCB.val && (whtHD.stval || whtHD.satval)) {
	  h = whtHD.stval;
	  s = whtHD.satval;
	}
      }

      /* apply satDial value to s */
      s = s + (int)satDial.val;
      if (s<  0) s =   0;
      if (s>100) s = 100;


      /* convert HSV back to RGB */


      if (h==NOHUE || !s) { rv = gv = bv = v; }
      else {
	if (h==360) h = 0;

	h        = (h*100) / 60;    /* h is in range 000..599 (0.0 - 5.99) */
	j        = h - (h%100);     /* j = 000, 100, 200, 300, 400, 500 */
	f        = h - j;           /* 'fractional' part of h (00..99) */
	vs100    = (v*s)/100;
	vsf10000 = (v*s*f)/10000;

	p = v - vs100;
	q = v - vsf10000;
	t = v - vs100 + vsf10000;

	switch (j) {
	case 000:  rv = v;  gv = t;  bv = p;  break;
	case 100:  rv = q;  gv = v;  bv = p;  break;
	case 200:  rv = p;  gv = v;  bv = t;  break;
	case 300:  rv = p;  gv = q;  bv = v;  break;
	case 400:  rv = t;  gv = p;  bv = v;  break;
	case 500:  rv = v;  gv = p;  bv = q;  break;
	default:   rv = gv = bv = 0;  /* never happens */
	}
      }
    }   /* if hsvmod */


    *op++ = rGraf.func[rv];
    *op++ = gGraf.func[gv];
    *op++ = bGraf.func[bv];
  }

  printUTime("end of GammifyPic24");

  return outpic;
}


/*********************/
void GamSetAutoApply(val)
     int val;
{
  /* turns auto apply checkbox on/off.  If val == -1, sets to 'default' val */

  if (!gamW) return;  /* not created yet.  shouldn't happen */

  if (val < 0) autoCB.val = defAutoApply;
  else autoCB.val = val;

  CBRedraw(&autoCB);
}
