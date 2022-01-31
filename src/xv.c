/*
 * xv.c - main section of xv.  X setup, window creation, etc.
 */

#include "copyright.h"

#define MAIN
#define NEEDSTIME
#define NEEDSDIR     /* for value of MAXPATHLEN */

#include "xv.h"

#include "bits/icon"
#include "bits/iconmask"
#include "bits/runicon"
#include "bits/runiconm"
#include "bits/cboard50"
#include "bits/gray25"

#include <X11/Xatom.h>

#ifdef VMS
  extern Window pseudo_root();
#endif


/* program needs one of the following fonts.  Trys them in ascending order */
#define FONT1 "-*-lucida-medium-r-*-*-12-*-*-*-*-*-*-*"
#define FONT2 "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*"
#define FONT3 "-*-helvetica-medium-r-*-*-11-*-*-*-*-*-*-*"
#define FONT4 "6x13"
#define FONT5 "fixed"

/* a mono-spaced font needed for the 'pixel value tracking' feature */
#define MFONT1 "-misc-fixed-medium-r-normal-*-13-*"
#define MFONT2 "6x13"   
#define MFONT3 "-*-courier-medium-r-*-*-12-*"
#define MFONT4 "fixed"   


/* default positions for various windows */
#define DEFINFOGEOM "-10+10"       /* default position of info window */
#define DEFCTRLGEOM "+170+380"     /* default position of ctrl window */
#define DEFGAMGEOM  "+10-10"       /* default position of gamma window */
#define DEFBROWGEOM "-10-10"       /* default position of browse window */
#define DEFTEXTGEOM "-10+320"      /* default position of text window */
#define DEFCMTGEOM  "-10+300"      /* default position of comment window */



static int    revvideo   = 0;   /* true if we should reverse video */
static int    dfltkludge = 0;   /* true if we want dfltpic dithered */
static int    clearonload;      /* clear window/root (on colormap visuals) */
static int    randomShow = 0;   /* do a 'random' slideshow */
static int    startIconic = 0;  /* '-iconic' option */
static int    defaultVis  = 0;  /* true if using DefaultVisual */
static double hexpand = 1.0;    /* '-expand' argument */
static double vexpand = 1.0;    /* '-expand' argument */
static char  *maingeom = NULL;
static char  *icongeom = NULL;
static Atom   __SWM_VROOT = None;

static char   basefname[128];   /* just the current fname, no path */

/* things to do upon successfully loading an image */
static int    autoraw    = 0;   /* force raw if using stdcmap */
static int    autodither = 0;   /* dither */
static int    autosmooth = 0;   /* smooth */
static int    auto4x3    = 0;   /* do a 4x3 */
static int    autorotate = 0;   /* rotate 0, +-90, +-180, +-270 */
static int    autohflip  = 0;   /* Flip Horizontally */
static int    autovflip  = 0;   /* Flip Vertically */
static int    autocrop   = 0;   /* do an 'AutoCrop' command */
static int    acrop      = 0;   /* automatically do a 'Crop' */
static int    acropX, acropY, acropW, acropH;
static int    autonorm   = 0;   /* normalize */
static int    autohisteq = 0;   /* Histogram equalization */

static int    force8     = 0;   /* force 8-bit mode */
static int    force24    = 0;   /* force 24-bit mode */

/* used in DeleteCmd() and Quit() */
static char  **mainargv;
static int     mainargc;


/* local function pre-definitions */
       int  main                     PARM((int, char **));
static int  highbit                  PARM((unsigned long));
static void makeDirectCmap           PARM((void));
static void useOtherVisual           PARM((XVisualInfo *, int));
static void parseResources           PARM((int, char **));
static void parseCmdLine             PARM((int, char **));
static void verifyArgs               PARM((void));
static void printoption              PARM((char *));
static void cmdSyntax                PARM((void));
static void rmodeSyntax              PARM((void));
static int  openPic                  PARM((int));
static int  readpipe                 PARM((char *, char *));
static void openFirstPic             PARM((void));
static void openNextPic              PARM((void));
static void openNextQuit             PARM((void));
static void openNextLoop             PARM((void));
static void openPrevPic              PARM((void));
static void openNamedPic             PARM((void));
static int  findRandomPic            PARM((void));
static void mainLoop                 PARM((void));
static void createMainWindow         PARM((char *, char *));
static void setWinIconNames          PARM((char *));
static void makeDispNames            PARM((void));
static void fixDispNames             PARM((void));
static void deleteFromList           PARM((int));
static int  argcmp                   PARM((char *, char *, int, int, int *));
static void add_filelist_to_namelist PARM((char *, char **, int *, int));


/* formerly local vars in main, made local to this module when
   parseResources() and parseCmdLine() were split out of main() */
   
int   imap, ctrlmap, gmap, browmap, cmtmap, clrroot, nopos, limit2x;
char *display, *whitestr, *blackstr, *histr, *lostr,
     *infogeom, *fgstr, *bgstr, *ctrlgeom, *gamgeom, *browgeom, *tmpstr;
char *rootfgstr, *rootbgstr, *visualstr, *textgeom, *cmtgeom;
char *monofontname, *flistName;
int  curstype, stdinflag, browseMode, savenorm, preview, pscomp, preset, 
     rmodeset, gamset, cgamset, perfect, owncmap, rwcolor, stdcmap;
int  nodecor;
double gamval, rgamval, ggamval, bgamval;




/*******************************************/
int main(argc, argv)
     int    argc;
     char **argv;
/*******************************************/
{
  int    i;
  XColor ecdef;
  Window rootReturn, parentReturn, *children;
  unsigned int numChildren, rootDEEP;


#ifdef VMS
  /* convert VMS-style arguments to unix names and glob */
  do_vms_wildcard(&argc, &argv);
  getredirection(&argc, &argv);
#endif


  /*****************************************************/
  /*** variable Initialization                       ***/
  /*****************************************************/

  xv_getwd(initdir, sizeof(initdir));
  searchdir[0] = '\0';
  fullfname[0] = '\0';

  mainargv = argv;
  mainargc = argc;

  /* init internal variables */
  display = NULL;
  fgstr = bgstr = rootfgstr = rootbgstr = NULL;
  histr = lostr = whitestr = blackstr = NULL;
  visualstr = monofontname = flistName = NULL;
  winTitle = NULL;

  pic = egampic = epic = cpic = NULL;
  theImage = NULL;

  picComments = (char *) NULL;

  numPages = 1;  curPage = 0;
  pageBaseName[0] = '\0';

  LocalCmap = browCmap = 0;
  stdinflag = 0;
  autoclose = autoDelete = 0; 
  cmapInGam = 0;
  grabDelay = 0;
  showzoomcursor = 0;
  perfect = owncmap = stdcmap = rwcolor = 0;

  ignoreConfigs = 0;
  browPerfect = 1;  
  gamval = rgamval = ggamval = bgamval = 1.0;
  
  picType = -1;              /* gets set once file is loaded */
  colorMapMode = CM_NORMAL;
  haveStdCmap  = STD_NONE;
  allocMode    = AM_READONLY;
  novbrowse    = 0;

#ifndef VMS
  strcpy(printCmd, "lpr");
#else
  strcpy(printCmd, "Print /Queue = XV_Queue");
#endif

  forceClipFile = 0;
  clearR = clearG = clearB = 0;
  InitSelection();


  /* default Ghostscript parameters */
  gsDev = "ppmraw";
#ifdef GS_DEV
  gsDev = GS_DEV;
#endif

  gsRes = 72;
  gsGeomStr = NULL;


  /* init default colors */
  fgstr = "#000000";  bgstr = "#B2C0DC";
  histr = "#C6D5E2";  lostr = "#8B99B5";

  cmd = (char *) rindex(argv[0],'/');
  if (!cmd) cmd = argv[0]; else cmd++;

  tmpstr = (char *) getenv("TMPDIR");
  if (!tmpstr) tmpdir = "/tmp";
  else {
    tmpdir = (char *) malloc(strlen(tmpstr) + 1);
    if (!tmpdir) FatalError("can't malloc 'tmpdir'\n");
    strcpy(tmpdir, tmpstr);
  }

  /* init command-line options flags */
  infogeom = DEFINFOGEOM;  ctrlgeom = DEFCTRLGEOM;  
  gamgeom  = DEFGAMGEOM;   browgeom = DEFBROWGEOM;
  textgeom = DEFTEXTGEOM;  cmtgeom  = DEFCMTGEOM;

  ncols = -1;  mono = 0;  
  ninstall = 0;  fixedaspect = 0;  noFreeCols = nodecor = 0;
  DEBUG = 0;  bwidth = 2;
  nolimits = useroot = clrroot = noqcheck = 0;
  waitsec = -1;  waitloop = 0;  automax = 0;
  rootMode = 0;  hsvmode = 0;
  rmodeset = gamset = cgamset = 0;
  nopos = limit2x = 0;
  resetroot = 1;
  clearonload = 0;
  curstype = XC_top_left_arrow;
  browseMode = savenorm = nostat = 0;
  preview = 0;
  pscomp = 0;
  preset = 0;
  viewonly = 0;

  /* init 'xormasks' array */
  xorMasks[0] = 0x01010101;
  xorMasks[1] = 0x02020203;
  xorMasks[2] = 0x84848485;
  xorMasks[3] = 0x88888889;
  xorMasks[4] = 0x10101011;
  xorMasks[5] = 0x20202023;
  xorMasks[6] = 0xc4c4c4c5;
  xorMasks[7] = 0xffffffff;

  kludge_offx = kludge_offy = winCtrPosKludge = 0;

  conv24 = CONV24_SLOW;  /* use 'slow' algorithm by default */

  defaspect = normaspect = 1.0;
  mainW = dirW = infoW = ctrlW = gamW = psW = (Window) NULL;
  anyBrowUp = 0;

#ifdef HAVE_JPEG
  jpegW = (Window) NULL;  jpegUp = 0;
#endif

#ifdef HAVE_TIFF
  tiffW = (Window) NULL;  tiffUp = 0;
#endif

  imap = ctrlmap = gmap = browmap = cmtmap = 0;

  ch_offx = ch_offy = p_offx = p_offy = 0;

  /* init info box variables */
  infoUp = 0;
  infoMode = INF_STR;
  for (i=0; i<NISTR; i++) SetISTR(i,"");

  /* init ctrl box variables */
  ctrlUp = 0;
  curname = -1;
  formatStr[0] ='\0';

  gamUp = 0;
  psUp  = 0;

  Init24to8();


  /* handle user-specified resources and cmd-line arguments */
  parseResources(argc,argv);
  parseCmdLine(argc, argv);
  verifyArgs();


  /*****************************************************/
  /*** X Setup                                       ***/
  /*****************************************************/
  
  theScreen = DefaultScreen(theDisp);
  theCmap   = DefaultColormap(theDisp, theScreen);
  rootW     = RootWindow(theDisp,theScreen);
  theGC     = DefaultGC(theDisp,theScreen);
  theVisual = DefaultVisual(theDisp,theScreen);
  ncells    = DisplayCells(theDisp, theScreen);
  dispDEEP  = DisplayPlanes(theDisp,theScreen);
  maxWIDE   = vrWIDE  = dispWIDE  = DisplayWidth(theDisp,theScreen);
  maxHIGH   = vrHIGH  = dispHIGH  = DisplayHeight(theDisp,theScreen);


  rootDEEP = dispDEEP;

  /* things dependant on theVisual:
   *    dispDEEP, theScreen, rootW, ncells, theCmap, theGC, 
   *    vrWIDE, dispWIDE, vrHIGH, dispHIGH, maxWIDE, maxHIGH
   */



  /* if we *haven't* had a non-default visual specified,
     see if we have a TrueColor or DirectColor visual of 24 or 32 bits, 
     and if so, use that as the default visual (prefer TrueColor) */

  if (!visualstr && !useroot) {
    XVisualInfo *vinfo, rvinfo;
    int          best,  numvis;
    long         flags;

    best = -1;
    rvinfo.class  = TrueColor;
    rvinfo.screen = theScreen;
    flags = VisualClassMask | VisualScreenMask;
    
    vinfo = XGetVisualInfo(theDisp, flags, &rvinfo, &numvis);
    if (vinfo) {     /* look for a TrueColor, 24-bit or more (pref 24) */
      for (i=0, best = -1; i<numvis; i++) {
	if (vinfo[i].depth == 24) best = i;
	else if (vinfo[i].depth>24 && best<0) best = i;
      }
    }

    if (best == -1) {   /* look for a DirectColor, 24-bit or more (pref 24) */
      rvinfo.class = DirectColor;
      if (vinfo) XFree((char *) vinfo);
      vinfo = XGetVisualInfo(theDisp, flags, &rvinfo, &numvis);
      if (vinfo) {
	for (i=0, best = -1; i<numvis; i++) {
	  if (vinfo[i].depth == 24) best = i;
	  else if (vinfo[i].depth>24 && best<0) best = i;
	}
      }
    }
    
    if (best>=0 && best<numvis) useOtherVisual(vinfo, best);
    if (vinfo) XFree((char *) vinfo);
  }


			   
  if (visualstr && useroot) {
    fprintf(stderr, "%s: %sUsing default visual.\n",
	    cmd, "Warning:  Can't use specified visual on root.  ");
    visualstr = NULL;
  }

  if (visualstr && !useroot) {     /* handle non-default visual */
    int vclass = -1;
    int vid = -1;

    lower_str(visualstr);
    if      (!strcmp(visualstr,"staticgray"))  vclass = StaticGray;
    else if (!strcmp(visualstr,"staticcolor")) vclass = StaticColor;
    else if (!strcmp(visualstr,"truecolor"))   vclass = TrueColor;
    else if (!strcmp(visualstr,"grayscale"))   vclass = GrayScale;
    else if (!strcmp(visualstr,"pseudocolor")) vclass = PseudoColor;
    else if (!strcmp(visualstr,"directcolor")) vclass = DirectColor;
    else if (!strcmp(visualstr,"default"))     {}   /* recognize it as valid */
    else if (!strncmp(visualstr,"0x",(size_t) 2)) {  /* specified visual id */
      if (sscanf(visualstr, "0x%x", &vid) != 1) vid = -1;
    }
    else {
      fprintf(stderr,"%s: Unrecognized visual type '%s'.  %s\n",
	      cmd, visualstr, "Using server default.");
    }


    /* if 'default', vclass and vid will both be '-1' */

    if (vclass >= 0 || vid >= 0) {   /* try to find asked-for visual type */
      XVisualInfo *vinfo, rvinfo;
      long vinfomask;
      int numvis, best;

      if (vclass >= 0) { 
	rvinfo.class = vclass;   vinfomask = VisualClassMask;
      }
      else { rvinfo.visualid = vid;  vinfomask = VisualIDMask; }
      
      rvinfo.screen = theScreen;
      vinfomask |= VisualScreenMask;

      vinfo = XGetVisualInfo(theDisp, vinfomask, &rvinfo, &numvis);

      if (vinfo) {       /* choose the 'best' one, if multiple */
	for (i=0, best = 0; i<numvis; i++) {
	  if (vinfo[i].depth > vinfo[best].depth) best = i;
	}

	useOtherVisual(vinfo, best);
	XFree((char *) vinfo);
      }
      else fprintf(stderr,"%s: Visual type '%s' not available.  %s\n",
		   cmd, visualstr, "Using server default.");
    }
  }



  /* make linear colormap for DirectColor visual */
  if (theVisual->class == DirectColor) makeDirectCmap();

  defaultVis = (XVisualIDFromVisual(theVisual) == 
       XVisualIDFromVisual(DefaultVisual(theDisp, DefaultScreen(theDisp))));
    

  /* turn GraphicsExposures OFF in the default GC */
  {
    XGCValues xgcv;
    xgcv.graphics_exposures = False;
    XChangeGC(theDisp, theGC, GCGraphicsExposures, &xgcv);
  }


  if (!useroot && limit2x) { maxWIDE *= 2;  maxHIGH *= 2; }
  if (nolimits) { maxWIDE = 65000; maxHIGH = 65000; }

  XSetErrorHandler(xvErrorHandler);

  /* always search for virtual root window */
  vrootW = rootW;
#ifndef VMS
  __SWM_VROOT = XInternAtom(theDisp, "__SWM_VROOT", False);
  XQueryTree(theDisp, rootW, &rootReturn, &parentReturn, &children,
	     &numChildren);
  for (i = 0; i < numChildren; i++) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytesafter;
    Window *newRoot = NULL;
    XWindowAttributes xwa;
    if (XGetWindowProperty (theDisp, children[i], __SWM_VROOT, 0L, 1L,
	  False, XA_WINDOW, &actual_type, &actual_format, &nitems,
	  &bytesafter, (unsigned char **) &newRoot) == Success && newRoot) {
      vrootW = *newRoot;
      XGetWindowAttributes(theDisp, vrootW, &xwa);
      vrWIDE = xwa.width;  vrHIGH = xwa.height;
      dispDEEP = xwa.depth;
      break;
    }
  }
#else  /* VMS */
  vrootW = pseudo_root(theDisp, theScreen);
#endif




  if (clrroot || useroot) {
    /* have enough info to do a '-clear' now */
    KillOldRootInfo();   /* if any */
    if (resetroot || clrroot) ClearRoot();  /* don't clear on '-noresetroot' */
    if (clrroot) Quit(0);
  }


  arrow     = XCreateFontCursor(theDisp,(u_int) curstype);
  cross     = XCreateFontCursor(theDisp,XC_crosshair);
  tcross    = XCreateFontCursor(theDisp,XC_tcross);
  zoom      = XCreateFontCursor(theDisp,XC_sizing);

  {
    XColor fc, bc;
    fc.red = fc.green = fc.blue = 0xffff;
    bc.red = bc.green = bc.blue = 0x0000;
    
    XRecolorCursor(theDisp, zoom, &fc, &bc);
  }

  { /* create inviso cursor */
    Pixmap      pix;
    static char bits[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    XColor      cfg;

    cfg.red = cfg.green = cfg.blue = 0;
    pix = XCreateBitmapFromData(theDisp, rootW, bits, 8, 8);
    inviso = XCreatePixmapCursor(theDisp, pix, pix, &cfg, &cfg, 0,0);
    XFreePixmap(theDisp, pix);
  }



  /* set up white,black colors */
  whtRGB = 0xffffff;  blkRGB = 0x000000;

  if (defaultVis) {
    white = WhitePixel(theDisp,theScreen);
    black = BlackPixel(theDisp,theScreen);
  }
  else {
    ecdef.flags = DoRed | DoGreen | DoBlue;
    ecdef.red = ecdef.green = ecdef.blue = 0xffff;
    if (xvAllocColor(theDisp, theCmap, &ecdef)) white = ecdef.pixel;
    else white = 0xffffffff;    /* probably evil... */

    ecdef.red = ecdef.green = ecdef.blue = 0x0000;
    if (xvAllocColor(theDisp, theCmap, &ecdef)) black = ecdef.pixel;
    else black = 0x00000000;    /* probably evil... */
  }

  if (whitestr && XParseColor(theDisp, theCmap, whitestr, &ecdef) &&
      xvAllocColor(theDisp, theCmap, &ecdef))  {
    white = ecdef.pixel;
    whtRGB = ((ecdef.red>>8)<<16) | (ecdef.green&0xff00) | (ecdef.blue>>8);
  }

  if (blackstr && XParseColor(theDisp, theCmap, blackstr, &ecdef) &&
      xvAllocColor(theDisp, theCmap, &ecdef))  {
    black = ecdef.pixel;
    blkRGB = ((ecdef.red>>8)<<16) | (ecdef.green&0xff00) | (ecdef.blue>>8);
  }


  /* set up fg,bg colors */
  fg = black;   bg = white;  
  if (fgstr && XParseColor(theDisp, theCmap, fgstr, &ecdef) &&
      xvAllocColor(theDisp, theCmap, &ecdef)) {
    fg = ecdef.pixel;
  }

  if (bgstr && XParseColor(theDisp, theCmap, bgstr, &ecdef) &&
      xvAllocColor(theDisp, theCmap, &ecdef))  {
    bg = ecdef.pixel;
  }


  /* set up root fg,bg colors */
  rootfg = white;   rootbg = black;
  if (rootfgstr && XParseColor(theDisp, theCmap, rootfgstr, &ecdef) &&
      xvAllocColor(theDisp, theCmap, &ecdef))  rootfg = ecdef.pixel;
  if (rootbgstr && XParseColor(theDisp, theCmap, rootbgstr, &ecdef) &&
      xvAllocColor(theDisp, theCmap, &ecdef))  rootbg = ecdef.pixel;


  /* set up hi/lo colors */
  i=0;
  if (dispDEEP > 1) {   /* only if we're on a reasonable display */
    if (histr && XParseColor(theDisp, theCmap, histr, &ecdef) &&
	xvAllocColor(theDisp, theCmap, &ecdef))  { hicol = ecdef.pixel; i|=1; }
    if (lostr && XParseColor(theDisp, theCmap, lostr, &ecdef) &&
	xvAllocColor(theDisp, theCmap, &ecdef))  { locol = ecdef.pixel; i|=2; }
  }

  if      (i==0) ctrlColor = 0;
  else if (i==3) ctrlColor = 1;
  else {  /* only got some of them */
    if (i&1) xvFreeColors(theDisp, theCmap, &hicol, 1, 0L);
    if (i&2) xvFreeColors(theDisp, theCmap, &locol, 1, 0L);
    ctrlColor = 0;
  }

  if (!ctrlColor) { hicol = bg;  locol = fg; }

  XSetForeground(theDisp,theGC,fg);
  XSetBackground(theDisp,theGC,bg);

  infofg = fg;  infobg = bg;

  /* if '-mono' not forced, determine if we're on a grey or color monitor */
  if (!mono) {
    if (theVisual->class == StaticGray || theVisual->class == GrayScale)
      mono = 1;
  }
  


  iconPix  = MakePix1(rootW, icon_bits,     icon_width,    icon_height);
  iconmask = MakePix1(rootW, iconmask_bits, icon_width,    icon_height);
  riconPix = MakePix1(rootW, runicon_bits,  runicon_width, runicon_height);
  riconmask= MakePix1(rootW, runiconm_bits, runiconm_width,runiconm_height);

  if (!iconPix || !iconmask || !riconPix || !riconmask) 
    FatalError("Unable to create icon pixmaps\n");

  gray50Tile = XCreatePixmapFromBitmapData(theDisp, rootW, 
				(char *) cboard50_bits,
				cboard50_width, cboard50_height, 
				infofg, infobg, dispDEEP);
  if (!gray50Tile) FatalError("Unable to create gray50Tile bitmap\n");

  gray25Tile = XCreatePixmapFromBitmapData(theDisp, rootW, 
				(char *) gray25_bits,
				gray25_width, gray25_height, 
				infofg, infobg, dispDEEP);
  if (!gray25Tile) FatalError("Unable to create gray25Tile bitmap\n");


  /* try to load fonts */
  if ( (mfinfo = XLoadQueryFont(theDisp,FONT1))==NULL && 
       (mfinfo = XLoadQueryFont(theDisp,FONT2))==NULL && 
       (mfinfo = XLoadQueryFont(theDisp,FONT3))==NULL && 
       (mfinfo = XLoadQueryFont(theDisp,FONT4))==NULL && 
       (mfinfo = XLoadQueryFont(theDisp,FONT5))==NULL) {
    sprintf(str,
	    "couldn't open the following fonts:\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s",
	    FONT1, FONT2, FONT3, FONT4, FONT5);
    FatalError(str);
  }
  mfont=mfinfo->fid;
  XSetFont(theDisp,theGC,mfont);

  monofinfo = (XFontStruct *) NULL;

  if (monofontname) {
    monofinfo = XLoadQueryFont(theDisp, monofontname);
    if (!monofinfo) fprintf(stderr,"xv: unable to load font '%s'\n", 
			    monofontname);
  }    

  if (!monofinfo) {
    if ((monofinfo = XLoadQueryFont(theDisp,MFONT1))==NULL && 
	(monofinfo = XLoadQueryFont(theDisp,MFONT2))==NULL && 
	(monofinfo = XLoadQueryFont(theDisp,MFONT3))==NULL && 
	(monofinfo = XLoadQueryFont(theDisp,MFONT4))==NULL) {
      sprintf(str,"couldn't open %s fonts:\n\t%s\n\t%s\n\t%s\n\t%s",
	      "any of the following",
	      MFONT1, MFONT2, MFONT3, MFONT4);
      FatalError(str);
    }
  }

  monofont=monofinfo->fid;
  

  
  
  /* if ncols wasn't set, set it to 2^dispDEEP, unless dispDEEP=1, in which
     case ncols = 0;  (ncols = max number of colors allocated.  on 1-bit
     displays, no colors are allocated */
  
  if (ncols == -1) {
    if (dispDEEP>1) ncols = 1 << ((dispDEEP>8) ? 8 : dispDEEP);
    else ncols = 0;
  }
  else if (ncols>256) ncols = 256;       /* so program doesn't blow up */
  
  
  GenerateFSGamma();  /* has to be done before 'OpenBrowse()' is called */
  
  
  
  /* no filenames.  build one-name (stdio) list (if stdinflag) */
  if (numnames==0) {
    if (stdinflag) {  
      /* have to malloc namelist[0] so we can free it in deleteFromList() */
      namelist[0] = (char *) malloc(strlen(STDINSTR) + 1);
      if (!namelist[0]) FatalError("unable to to build namelist[0]");
      strcpy(namelist[0], STDINSTR);
      numnames = 1;
    }
    else namelist[0] = NULL;
  }
  
  if (numnames) makeDispNames();
  
  
  if (viewonly || autoquit) { 
    imap = ctrlmap = gmap = browmap = cmtmap = 0; 
    novbrowse = 1;
  }
  
  
  /* create the info box window */
  CreateInfo(infogeom);
  XSelectInput(theDisp, infoW, ExposureMask | ButtonPressMask | KeyPressMask
	       | StructureNotifyMask);
  InfoBox(imap);     /* map it (or not) */
  if (imap) {
    RedrawInfo(0,0,1000,1000);  /* explicit draw if mapped */
    XFlush(theDisp);
  }
  
  
  /* create the control box window */
  CreateCtrl(ctrlgeom);
  epicMode = EM_RAW;   SetEpicMode();
  
  XSelectInput(theDisp, ctrlW, ExposureMask | ButtonPressMask | KeyPressMask
	       | StructureNotifyMask);
  if (ctrlmap < 0) {    /* map iconified */
    XWMHints xwmh;
    xwmh.input         = True;
    xwmh.initial_state = IconicState;
    xwmh.flags         = (InputHint | StateHint);
    XSetWMHints(theDisp, ctrlW, &xwmh);
    ctrlmap = 1;
  }
  CtrlBox(ctrlmap);     /* map it (or not) */
  if (ctrlmap) {
    RedrawCtrl(0,0,1000,1000);   /* explicit draw if mapped */
    XFlush(theDisp);
  }
  
  fixDispNames();
  ChangedCtrlList();
  
  /* disable root modes if using non-default visual */
  if (!defaultVis) {
    for (i=RMB_ROOT; i<RMB_MAX; i++) rootMB.dim[i] = 1;
  }
  
  
  /* create the directory window */
  CreateDirW(NULL);
  XSelectInput(theDisp, dirW, ExposureMask | ButtonPressMask | KeyPressMask);
  browseCB.val = browseMode;
  savenormCB.val = savenorm;
  
  /* create the gamma window */
  CreateGam(gamgeom, (gamset) ? gamval : -1.0,
	    (cgamset) ? rgamval : -1.0,
	    (cgamset) ? ggamval : -1.0,
	    (cgamset) ? bgamval : -1.0,
	    preset);
  XSelectInput(theDisp, gamW, ExposureMask | ButtonPressMask | KeyPressMask
	       | StructureNotifyMask
	       | (cmapInGam ? ColormapChangeMask : 0));
  
  GamBox(gmap);     /* map it (or not) */
  
  
  
  stdnfcols = 0;   /* so we don't try to free any if we don't create any */
  
  if (!novbrowse) {
    MakeBrowCmap();
    /* create the visual browser window */
    CreateBrowse(browgeom, fgstr, bgstr, histr, lostr);
    
    if (browmap) OpenBrowse();
  }
  else windowMB.dim[WMB_BROWSE] = 1;    /* disable visual schnauzer */
  
  
  CreateTextWins(textgeom, cmtgeom);
  if (cmtmap) OpenCommentText();
  
  
  /* create the ps window */
  CreatePSD(NULL);
  XSetTransientForHint(theDisp, psW, dirW);
  encapsCB.val = preview;
  pscompCB.val = pscomp;
  
  
#ifdef HAVE_JPEG
  CreateJPEGW();
  XSetTransientForHint(theDisp, jpegW, dirW);
#endif
  
#ifdef HAVE_TIFF
  CreateTIFFW();
  XSetTransientForHint(theDisp, tiffW, dirW);
#endif
  
  
  LoadFishCursors();
  SetCursors(-1);
  
  
  /* if we're not on a colormapped display, turn off rwcolor */
  if (!CMAPVIS(theVisual)) {
    if (rwcolor) fprintf(stderr, "xv: not a colormapped display.  %s\n",
			 "'rwcolor' turned off.");
    
    allocMode = AM_READONLY;
    dispMB.flags[DMB_COLRW] = 0;  /* de-'check' */
    dispMB.dim[DMB_COLRW] = 1;    /* and dim it */
  }
  
  
  if (force24) {
    Set824Menus(PIC24);
    conv24MB.flags[CONV24_LOCK]  = 1;
    picType = PIC24;
  }
  else if (force8) {
    Set824Menus(PIC8);
    conv24MB.flags[CONV24_LOCK]  = 1;
    picType = PIC8;
  }
  else {
    Set824Menus(PIC8);     /* default mode */
    picType = PIC8;
  }
  
  
  
  /* make std colormap, maybe */
  ChangeCmapMode(colorMapMode, 0, 0);


  
  
  /* Do The Thing... */
  mainLoop();
  Quit(0);
  return(0);
}



/*****************************************************/
static void makeDirectCmap()
{
  int    i, j, cmaplen, numgot;
  byte   origgot[256];
  XColor c;
  u_long rmask, gmask, bmask;
  int    rshift, gshift, bshift;
  

  rmask = theVisual->red_mask;
  gmask = theVisual->green_mask;
  bmask = theVisual->blue_mask;

  rshift = highbit(rmask) - 15;
  gshift = highbit(gmask) - 15;
  bshift = highbit(bmask) - 15;

  if (rshift<0) rmask = rmask << (-rshift);
           else rmask = rmask >> rshift;
  
  if (gshift<0) gmask = gmask << (-gshift);
           else gmask = gmask >> gshift;
  
  if (bshift<0) bmask = bmask << (-bshift);
           else bmask = bmask >> bshift;


  cmaplen = theVisual->map_entries;
  if (cmaplen>256) cmaplen=256;
  

  /* try to alloc a 'cmaplen' long grayscale colormap.  May not get all
     entries for whatever reason.  Build table 'directConv[]' that
     maps range [0..(cmaplen-1)] into set of colors we did get */
  
  for (i=0; i<256; i++) {  origgot[i] = 0;  directConv[i] = 0; }

  for (i=numgot=0; i<cmaplen; i++) {
    c.red = c.green = c.blue = (i * 0xffff) / (cmaplen - 1);
    c.red   = c.red   & rmask;
    c.green = c.green & gmask;
    c.blue  = c.blue  & bmask;
    c.flags = DoRed | DoGreen | DoBlue;

    if (XAllocColor(theDisp, theCmap, &c)) {
      /* fprintf(stderr,"%3d: %4x,%4x,%4x\n", i, c.red,c.green,c.blue); */
      directConv[i] = i;
      origgot[i] = 1;
      numgot++;
    }
  }

  
  if (numgot == 0) FatalError("Got no entries in DirectColor cmap!\n");
  
  /* directConv may or may not have holes in it. */
  for (i=0; i<cmaplen; i++) {
    if (!origgot[i]) {
      int numbak, numfwd;
      numbak = numfwd = 0;
      while ((i - numbak) >= 0       && !origgot[i-numbak]) numbak++;
      while ((i + numfwd) <  cmaplen && !origgot[i+numfwd]) numfwd++;
      
      if (i-numbak<0        || !origgot[i-numbak]) numbak = 999;
      if (i+numfwd>=cmaplen || !origgot[i+numfwd]) numfwd = 999;
      
      if      (numbak<numfwd) directConv[i] = directConv[i-numbak];
      else if (numfwd<999)    directConv[i] = directConv[i+numfwd];
      else FatalError("DirectColor cmap:  can't happen!");
    }
  }
}


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




/*****************************************************/
static void useOtherVisual(vinfo, best)
     XVisualInfo *vinfo;
     int best;
{
  if (!vinfo || best<0) return;

  if (vinfo[best].visualid == 
      XVisualIDFromVisual(DefaultVisual(theDisp, theScreen))) return;

  theVisual = vinfo[best].visual;

  if (DEBUG) {
    fprintf(stderr,"%s: using %s visual (0x%0x), depth = %d, screen = %d\n",
	    cmd, 
	    (vinfo[best].class==StaticGray)  ? "StaticGray" :
	    (vinfo[best].class==StaticColor) ? "StaticColor" :
	    (vinfo[best].class==TrueColor)   ? "TrueColor" :
	    (vinfo[best].class==GrayScale)   ? "GrayScale" :
	    (vinfo[best].class==PseudoColor) ? "PseudoColor" :
	    (vinfo[best].class==DirectColor) ? "DirectColor" : "<unknown>",
	    (int) vinfo[best].visualid,
	    vinfo[best].depth, vinfo[best].screen);

    fprintf(stderr,"\tmasks: (0x%x,0x%x,0x%x), bits_per_rgb=%d\n",
	    (int) vinfo[best].red_mask, (int) vinfo[best].green_mask,
	    (int) vinfo[best].blue_mask, vinfo[best].bits_per_rgb);
  }
  
  dispDEEP  = vinfo[best].depth;
  theScreen = vinfo[best].screen;
  rootW     = RootWindow(theDisp, theScreen);
  ncells    = vinfo[best].colormap_size;
  theCmap   = XCreateColormap(theDisp, rootW, theVisual, AllocNone);
  
  {
    /* create a temporary window using this visual so we can
       create a GC for this visual */
    
    Window win;  
    XSetWindowAttributes xswa;
    XGCValues xgcv;
    unsigned long xswamask;
    
    XFlush(theDisp);
    XSync(theDisp, False);
    
    xswa.background_pixel = 0;
    xswa.border_pixel     = 1;
    xswa.colormap         = theCmap;
    xswamask = CWBackPixel | CWBorderPixel | CWColormap;
    
    win = XCreateWindow(theDisp, rootW, 0, 0, 100, 100, 2, (int) dispDEEP,
			InputOutput, theVisual, xswamask, &xswa);
    
    XFlush(theDisp);
    XSync(theDisp, False);
    
    theGC = XCreateGC(theDisp, win, 0L, &xgcv);
    
    XDestroyWindow(theDisp, win);
  }
  
  vrWIDE = dispWIDE  = DisplayWidth(theDisp,theScreen);
  vrHIGH = dispHIGH  = DisplayHeight(theDisp,theScreen);
  maxWIDE = dispWIDE;  maxHIGH = dispHIGH;
}




/*****************************************************/
static void parseResources(argc, argv)
     int argc;
     char **argv;
{
  int i, pm;

  /* once through the argument list to find the display name
     and DEBUG level, if any */

  for (i=1; i<argc; i++) {
    if (!strncmp(argv[i],"-help", (size_t) 5)) {  /* help */
      cmdSyntax();
      exit(0);
    }

    else if (!argcmp(argv[i],"-display",4,0,&pm)) {
      i++;
      if (i<argc) display = argv[i];
      break;
    }

#ifdef VMS    /* in VMS, cmd-line-opts are in lower case */
    else if (!argcmp(argv[i],"-debug",3,0,&pm)) {
      { if (++i<argc) DEBUG = atoi(argv[i]); }
    }
#else
    else if (!argcmp(argv[i],"-DEBUG",2,0,&pm)) {
      { if (++i<argc) DEBUG = atoi(argv[i]); }
    }
#endif
  }

  /* open the display */
  if ( (theDisp=XOpenDisplay(display)) == NULL) {
    fprintf(stderr, "%s: Can't open display\n",argv[0]);
    exit(1);
  }



  if (rd_str ("aspect")) {
    int n,d;
    if (sscanf(def_str,"%d:%d",&n,&d)!=2 || n<1 || d<1)
      fprintf(stderr,"%s: unable to parse 'aspect' resource\n",cmd);
    else defaspect = (float) n / (float) d;
  }
      
  if (rd_flag("2xlimit"))        limit2x     = def_int;      
  if (rd_flag("auto4x3"))        auto4x3     = def_int;
  if (rd_flag("autoClose"))      autoclose   = def_int;
  if (rd_flag("autoCrop"))       autocrop    = def_int;
  if (rd_flag("autoDither"))     autodither  = def_int;
  if (rd_flag("autoHFlip"))      autohflip   = def_int;
  if (rd_flag("autoHistEq"))     autohisteq  = def_int;
  if (rd_flag("autoNorm"))       autonorm    = def_int;
  if (rd_flag("autoRaw"))        autoraw     = def_int;
  if (rd_int ("autoRotate"))     autorotate  = def_int;
  if (rd_flag("autoSmooth"))     autosmooth  = def_int;
  if (rd_flag("autoVFlip"))      autovflip   = def_int;
  if (rd_str ("background"))     bgstr       = def_str;
  if (rd_flag("best24") && def_int)  conv24  = CONV24_BEST;
  if (rd_str ("black"))          blackstr    = def_str;
  if (rd_int ("borderWidth"))    bwidth      = def_int;
  if (rd_str ("ceditGeometry"))  gamgeom     = def_str;
  if (rd_flag("ceditMap"))       gmap        = def_int;
  if (rd_flag("ceditColorMap"))  cmapInGam   = def_int;
  if (rd_flag("clearOnLoad"))    clearonload = def_int;
  if (rd_str ("commentGeometry")) cmtgeom    = def_str;
  if (rd_flag("commentMap"))     cmtmap      = def_int;
  if (rd_str ("ctrlGeometry"))   ctrlgeom    = def_str;
  if (rd_flag("ctrlMap"))        ctrlmap     = def_int;
  if (rd_int ("cursor"))         curstype    = def_int;
  if (rd_int ("defaultPreset"))  preset      = def_int;

  if (rd_str ("driftKludge")) {
    if (sscanf(def_str,"%d %d", &kludge_offx, &kludge_offy) != 2) {
      kludge_offx = kludge_offy = 0;
    }
  }

  if (rd_str ("expand")) {
    if (index(def_str, ':')) {
      if (sscanf(def_str, "%lf:%lf", &hexpand, &vexpand)!=2) 
	{ hexpand = vexpand = 1.0; }
    }
    else hexpand = vexpand = atof(def_str);
  }

  if (rd_str ("fileList"))       flistName   = def_str;
  if (rd_flag("fixed"))          fixedaspect = def_int;
  if (rd_flag("force8"))         force8      = def_int;
  if (rd_flag("force24"))        force24     = def_int;
  if (rd_str ("foreground"))     fgstr       = def_str;
  if (rd_str ("geometry"))       maingeom    = def_str;
  if (rd_str ("gsDevice"))       gsDev       = def_str;
  if (rd_str ("gsGeometry"))     gsGeomStr   = def_str;
  if (rd_int ("gsResolution"))   gsRes       = def_int;
  if (rd_flag("hsvMode"))        hsvmode     = def_int;
  if (rd_str ("highlight"))      histr       = def_str;
  if (rd_str ("iconGeometry"))   icongeom    = def_str;
  if (rd_flag("iconic"))         startIconic = def_int;
  if (rd_str ("infoGeometry"))   infogeom    = def_str;
  if (rd_flag("infoMap"))        imap        = def_int;
  if (rd_flag("loadBrowse"))     browseMode  = def_int;
  if (rd_str ("lowlight"))       lostr       = def_str;
  if (rd_flag("mono"))           mono        = def_int;
  if (rd_str ("monofont"))       monofontname = def_str;
  if (rd_int ("ncols"))          ncols       = def_int;
  if (rd_flag("ninstall"))       ninstall    = def_int;
  if (rd_flag("nodecor"))        nodecor     = def_int;
  if (rd_flag("nolimits"))       nolimits    = def_int;
  if (rd_flag("nopos"))          nopos       = def_int;
  if (rd_flag("noqcheck"))       noqcheck    = def_int;
  if (rd_flag("nostat"))         nostat      = def_int;
  if (rd_flag("ownCmap"))        owncmap     = def_int;
  if (rd_flag("perfect"))        perfect     = def_int;
  if (rd_flag("popupKludge"))    winCtrPosKludge = def_int;
  if (rd_str ("print"))          strncpy(printCmd, def_str, 
					 (size_t) PRINTCMDLEN);
  if (rd_flag("pscompress"))     pscomp      = def_int;
  if (rd_flag("pspreview"))      preview     = def_int;
  if (rd_flag("quick24") && def_int)  conv24 = CONV24_FAST;
  if (rd_flag("resetroot"))      resetroot   = def_int;
  if (rd_flag("reverse"))        revvideo    = def_int;
  if (rd_str ("rootBackground")) rootbgstr   = def_str;
  if (rd_str ("rootForeground")) rootfgstr   = def_str;
  if (rd_int ("rootMode"))       { rootMode    = def_int;  rmodeset++; }
  if (rd_flag("rwColor"))        rwcolor     = def_int;
  if (rd_flag("saveNormal"))     savenorm    = def_int;
  if (rd_str ("searchDirectory"))  strcpy(searchdir, def_str);
  if (rd_str ("textviewGeometry")) textgeom  = def_str;
  if (rd_flag("useStdCmap"))     stdcmap     = def_int;
  if (rd_str ("visual"))         visualstr   = def_str;
  if (rd_flag("vsDisable"))      novbrowse   = def_int;
  if (rd_str ("vsGeometry"))     browgeom    = def_str;
  if (rd_flag("vsMap"))          browmap     = def_int;
  if (rd_flag("vsPerfect"))      browPerfect = def_int;
  if (rd_str ("white"))          whitestr    = def_str;
}



/*****************************************************/
static void parseCmdLine(argc, argv)
     int argc;  char **argv;
{
  int i, oldi, not_in_first_half, pm;

  orignumnames = 0;

  for (i=1, numnames=0; i<argc; i++) {
    oldi = i;

    not_in_first_half = 0;

    if (argv[i][0] != '-' && argv[i][0] != '+') { 
      /* a file name.  put it in list */

      if (!nostat) {
	struct stat st;  int ftype;
	if (stat(argv[i], &st) == 0) {
	  ftype = st.st_mode;
	  if (!S_ISREG(ftype)) continue;   /* stat'd & isn't file. skip it */
	}
      }

      if (numnames<MAXNAMES) {
	namelist[numnames++] = argv[i];
	if (numnames==MAXNAMES) {
	  fprintf(stderr,"%s: too many filenames.  Only using first %d.\n",
		  cmd, MAXNAMES);
	}
      }
    }

    else if (!strcmp(argv[i],  "-"))                    /* stdin flag */
      stdinflag++;

    else if (!argcmp(argv[i],"-24",     3,1,&force24 ));  /* force24 */
    else if (!argcmp(argv[i],"-2xlimit",3,1,&limit2x ));  /* 2xlimit */
    else if (!argcmp(argv[i],"-4x3",    2,1,&auto4x3 ));  /* 4x3 */
    else if (!argcmp(argv[i],"-8",      2,1,&force8  ));  /* force8 */
    else if (!argcmp(argv[i],"-acrop",  3,1,&autocrop));  /* autocrop */

    else if (!argcmp(argv[i],"-aspect",3,0,&pm)) {        /* def. aspect */
      int n,d;
      if (++i<argc) {
	if (sscanf(argv[i],"%d:%d",&n,&d)!=2 || n<1 || d<1)
	  fprintf(stderr,"%s: bad aspect ratio '%s'\n",cmd,argv[i]);
	else defaspect = (float) n / (float) d;
      }
    }

    else if (!argcmp(argv[i],"-best24",3,0,&pm))          /* -best */
      conv24 = CONV24_BEST;
    
    else if (!argcmp(argv[i],"-bg",3,0,&pm))              /* bg color */
      { if (++i<argc) bgstr = argv[i]; }
    
    else if (!argcmp(argv[i],"-black",3,0,&pm))           /* black color */
      { if (++i<argc) blackstr = argv[i]; }
    
    else if (!argcmp(argv[i],"-bw",3,0,&pm))              /* border width */
      { if (++i<argc) bwidth=atoi(argv[i]); }
    
    else if (!argcmp(argv[i],"-cecmap",4,1,&cmapInGam));  /* cmapInGam */
    
    else if (!argcmp(argv[i],"-cegeometry",4,0,&pm))      /* gammageom */
      { if (++i<argc) gamgeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-cemap",4,1,&gmap));        /* gmap */
    
    else if (!argcmp(argv[i],"-cgamma",4,0,&pm)) {        /* color gamma */
      if (i+3<argc) {
	rgamval = atof(argv[++i]); 
	ggamval = atof(argv[++i]); 
	bgamval = atof(argv[++i]); 
      }
      cgamset++;
    }
    
    else if (!argcmp(argv[i],"-cgeometry",4,0,&pm))	  /* ctrlgeom */
      { if (++i<argc) ctrlgeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-clear",4,1,&clrroot));	  /* clear */
    else if (!argcmp(argv[i],"-close",4,1,&autoclose));	  /* close */
    else if (!argcmp(argv[i],"-cmap", 3,1,&ctrlmap));	  /* ctrlmap */

    else if (!argcmp(argv[i],"-cmtgeometry",5,0,&pm))	  /* comment geom */
      { if (++i<argc) cmtgeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-cmtmap",5,1,&cmtmap));	  /* map cmt window */
    
    else if (!argcmp(argv[i],"-crop",3,0,&pm)) {          /* crop */
      if (i+4<argc) {
	acropX = atoi(argv[++i]); 
	acropY = atoi(argv[++i]); 
	acropW = atoi(argv[++i]); 
	acropH = atoi(argv[++i]); 
      }
      acrop++;
    }
    
    else if (!argcmp(argv[i],"-cursor",3,0,&pm))	  /* cursor */
      { if (++i<argc) curstype = atoi(argv[i]); }

#ifdef VMS    /* in VMS, cmd-line-opts are in lower case */
    else if (!argcmp(argv[i],"-debug",3,0,&pm)) {
      { if (++i<argc) DEBUG = atoi(argv[i]); }
    }
#else
    else if (!argcmp(argv[i],"-DEBUG",2,0,&pm)) {
      { if (++i<argc) DEBUG = atoi(argv[i]); }
    }
#endif

    else if (!argcmp(argv[i],"-dir",4,0,&pm))             /* search dir */
      { if (++i<argc) strcpy(searchdir, argv[i]); }

    else if (!argcmp(argv[i],"-display",4,0,&pm))         /* display */
      { if (++i<argc) display = argv[i]; }

    else if (!argcmp(argv[i],"-dither",4,1,&autodither)); /* autodither */

    else if (!argcmp(argv[i],"-drift",3,0,&pm)) {         /* drift kludge */
      if (i<argc-2) {
	kludge_offx = atoi(argv[++i]);
	kludge_offy = atoi(argv[++i]);
      }
    }

    else if (!argcmp(argv[i],"-expand",2,0,&pm)) {	  /* expand factor */
      if (++i<argc) {
	if (index(argv[i], ':')) {
	  if (sscanf(argv[i], "%lf:%lf", &hexpand, &vexpand)!=2) 
	    { hexpand = vexpand = 1.0; }
	}
	else hexpand = vexpand = atof(argv[i]);
      }
    }

    else if (!argcmp(argv[i],"-fg",3,0,&pm))              /* fg color */
      { if (++i<argc) fgstr = argv[i]; }
    
    else if (!argcmp(argv[i],"-fixed",3,1,&fixedaspect)); /* fix asp. ratio */
    
    else if (!argcmp(argv[i],"-flist",3,0,&pm))           /* file list */
      { if (++i<argc) flistName = argv[i]; }

    else if (!argcmp(argv[i],"-gamma",3,0,&pm))	          /* gamma */
      { if (++i<argc) gamval = atof(argv[i]);  gamset++; }
    
    else if (!argcmp(argv[i],"-geometry",3,0,&pm))	  /* geometry */
      { if (++i<argc) maingeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-grabdelay",3,0,&pm))	  /* grabDelay */
      { if (++i<argc) grabDelay = atoi(argv[i]); }
    
    else if (!argcmp(argv[i],"-gsdev",4,0,&pm))	          /* gsDevice */
      { if (++i<argc) gsDev = argv[i]; }
    
    else if (!argcmp(argv[i],"-gsgeom",4,0,&pm))          /* gsGeometry */
      { if (++i<argc) gsGeomStr = argv[i]; }
    
    else if (!argcmp(argv[i],"-gsres",4,0,&pm))           /* gsResolution */
      { if (++i<argc) gsRes=abs(atoi(argv[i])); }
    
    else if (!argcmp(argv[i],"-hflip",3,1,&autohflip));   /* hflip */

    else if (!argcmp(argv[i],"-hi",3,0,&pm))	          /* highlight */
      { if (++i<argc) histr = argv[i]; }
    
    else if (!argcmp(argv[i],"-hist", 4,1,&autohisteq));  /* hist eq */

    else if (!argcmp(argv[i],"-hsv",   3,1,&hsvmode));     /* hsvmode */

    else if (!argcmp(argv[i],"-icgeometry",4,0,&pm))       /* icon geometry */
      { if (++i<argc) icongeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-iconic",4,1,&startIconic)); /* iconic */
    
    else if (!argcmp(argv[i],"-igeometry",3,0,&pm))        /* infogeom */
      { if (++i<argc) infogeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-imap",     3,1,&imap));        /* imap */
    else if (!argcmp(argv[i],"-lbrowse",  3,1,&browseMode));  /* browse mode */

    else if (!argcmp(argv[i],"-lo",3,0,&pm))	        /* lowlight */
      { if (++i<argc) lostr = argv[i]; }

    else if (!argcmp(argv[i],"-loadclear",4,1,&clearonload)); /* clearonload */

    
    else not_in_first_half = 1;     



    if (i != oldi) continue;   /* parsed something... */



    /* split huge else-if group into two halves, as it breaks some compilers */



    if (!argcmp(argv[i],"-max",4,1,&automax));	        /* auto maximize */
    else if (!argcmp(argv[i],"-maxpect",5,1,&pm))       /* auto maximize */
      { automax=pm; fixedaspect=pm; }
    
    else if (!argcmp(argv[i],"-mfn",3,0,&pm))           /* mono font name */
      { if (++i<argc) monofontname = argv[i]; }

    else if (!argcmp(argv[i],"-mono",3,1,&mono));	/* mono */
    
    else if (!argcmp(argv[i],"-name",3,0,&pm))          /* name */
      { if (++i<argc) winTitle = argv[i]; }
    
    else if (!argcmp(argv[i],"-ncols",3,0,&pm))        /* ncols */
      { if (++i<argc) ncols=abs(atoi(argv[i])); }
    
    else if (!argcmp(argv[i],"-ninstall",  3,1,&ninstall));   /* inst cmaps?*/
    else if (!argcmp(argv[i],"-nodecor",   4,1,&nodecor));
    else if (!argcmp(argv[i],"-nofreecols",4,1,&noFreeCols));
    else if (!argcmp(argv[i],"-nolimits",  4,1,&nolimits));   /* nolimits */
    else if (!argcmp(argv[i],"-nopos",     4,1,&nopos));      /* nopos */
    else if (!argcmp(argv[i],"-noqcheck",  4,1,&noqcheck));   /* noqcheck */
    else if (!argcmp(argv[i],"-noresetroot",5,1,&resetroot)); /* reset root*/
    else if (!argcmp(argv[i],"-norm",      5,1,&autonorm));   /* norm */
    else if (!argcmp(argv[i],"-nostat",    4,1,&nostat));     /* nostat */
    else if (!argcmp(argv[i],"-owncmap",   2,1,&owncmap));    /* own cmap */
    else if (!argcmp(argv[i],"-perfect",   3,1,&perfect));    /* -perfect */
    else if (!argcmp(argv[i],"-pkludge",   3,1,&winCtrPosKludge));
    else if (!argcmp(argv[i],"-poll",      3,1,&polling));    /* chk mod? */

    else if (!argcmp(argv[i],"-preset",3,0,&pm))      /* preset */
      { if (++i<argc) preset=abs(atoi(argv[i])); }
    
    else if (!argcmp(argv[i],"-quick24",5,0,&pm))     /* quick 24-to-8 conv */
      conv24 = CONV24_FAST;
    
    else if (!argcmp(argv[i],"-quit",   2,1,&autoquit));   /* auto-quit */
    else if (!argcmp(argv[i],"-random", 4,1,&randomShow)); /* random */
    else if (!argcmp(argv[i],"-raw",    4,1,&autoraw));    /* force raw */

    else if (!argcmp(argv[i],"-rbg",3,0,&pm))      /* root background color */
      { if (++i<argc) rootbgstr = argv[i]; }
    
    else if (!argcmp(argv[i],"-rfg",3,0,&pm))      /* root foreground color */
      { if (++i<argc) rootfgstr = argv[i]; }
    
    else if (!argcmp(argv[i],"-rgb",4,1,&pm))      /* rgb mode */
      hsvmode = !pm;
    
    else if (!argcmp(argv[i],"-RM",3,0,&pm))	   /* auto-delete */
      autoDelete = 1;
    
    else if (!argcmp(argv[i],"-rmode",3,0,&pm))	   /* root pattern */
      { if (++i<argc) rootMode = atoi(argv[i]); 
	useroot++;  rmodeset++;
      }
    
    else if (!argcmp(argv[i],"-root",4,1,&useroot));  /* use root window */
    
    else if (!argcmp(argv[i],"-rotate",4,0,&pm))      /* rotate */
      { if (++i<argc) autorotate = atoi(argv[i]); }
    
    else if (!argcmp(argv[i],"-rv",3,1,&revvideo));   /* reverse video */
    else if (!argcmp(argv[i],"-rw",3,1,&rwcolor));    /* use r/w color */

    else if (!argcmp(argv[i],"-slow24",3,0,&pm))      /* slow 24->-8 conv.*/
      conv24 = CONV24_SLOW;
    
    else if (!argcmp(argv[i],"-smooth",3,1,&autosmooth));  /* autosmooth */
    else if (!argcmp(argv[i],"-stdcmap",3,1,&stdcmap));    /* use stdcmap */

    else if (!argcmp(argv[i],"-tgeometry",2,0,&pm))	   /* textview geom */
      { if (++i<argc) textgeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-vflip",3,1,&autovflip));	   /* vflip */
    else if (!argcmp(argv[i],"-viewonly",4,1,&viewonly));  /* viewonly */

    else if (!argcmp(argv[i],"-visual",4,0,&pm))           /* visual */
      { if (++i<argc) visualstr = argv[i]; }
    
    else if (!argcmp(argv[i],"-vsdisable",4,1,&novbrowse)); /* disable sch? */
    
    else if (!argcmp(argv[i],"-vsgeometry",4,0,&pm))	/* visSchnauzer geom */
      { if (++i<argc) browgeom = argv[i]; }
    
    else if (!argcmp(argv[i],"-vsmap",4,1,&browmap));	/* visSchnauzer map */
    
    else if (!argcmp(argv[i],"-vsperfect",3,1,&browPerfect));	/* vs perf. */

    else if (!argcmp(argv[i],"-wait",3,0,&pm)) {        /* secs betwn pics */
      if (++i<argc) {
	waitsec = abs(atoi(argv[i]));
	if (waitsec<0) waitsec = 0;
      }
    }
    
    else if (!argcmp(argv[i],"-white",3,0,&pm))	        /* white color */
      { if (++i<argc) whitestr = argv[i]; }
    
    else if (!argcmp(argv[i],"-wloop",3,1,&waitloop));	/* waitloop */
    
    else if (not_in_first_half) cmdSyntax();
  }


  /* build origlist[], a copy of namelist that remains unmodified, for
     use with the 'autoDelete' option */
  orignumnames = numnames;
  xvbcopy( (char *) namelist, (char *) origlist, sizeof(origlist));
}


/*****************************************************************/
static void verifyArgs()
{
  /* check options for validity */

  if (strlen(searchdir)) {  /* got a search directory */
    if (chdir(searchdir)) {
      fprintf(stderr,"xv: unable to cd to directory '%s'.\n",searchdir);
      fprintf(stderr,
       "    Ignoring '-dir' option and/or 'xv.searchDirectory' resource\n");
      searchdir[0] = '\0';
    }
  }


  if (flistName) 
    add_filelist_to_namelist(flistName, namelist, &numnames, MAXNAMES);

  RANGE(curstype,0,254);
  curstype = curstype & 0xfe;   /* clear low bit to make curstype even */

  if (hexpand == 0.0 || vexpand == 0.0) cmdSyntax();
  if (rootMode < 0 || rootMode > RM_MAX) rmodeSyntax();

  if (DEBUG) XSynchronize(theDisp, True);

  /* if using root, generally gotta map ctrl window, 'cause there won't be
     any way to ask for it.  (no kbd or mouse events from rootW) */
  if (useroot && !autoquit) ctrlmap = -1;    

  
  if (abs(autorotate) !=   0 && abs(autorotate) != 90 &&
      abs(autorotate) != 180 && abs(autorotate) != 270) {
    fprintf(stderr,"Invalid auto rotation value (%d) ignored.\n", autorotate);
    fprintf(stderr,"  (Valid values:  0, +-90, +-180, +-270)\n");

    autorotate = 0;
  } 


  if (grabDelay < 0 || grabDelay > 15) {
    fprintf(stderr,
        "Invalid '-grabdelay' value ignored.  Valid range is 0-15 seconds.\n");
    grabDelay = 0;
  }

  if (preset<0 || preset>4) {
    fprintf(stderr,"Invalid default preset value (%d) ignored.\n", preset);
    fprintf(stderr,"  (Valid values:  1, 2, 3, 4)\n");

    preset = 0;
  } 

  if (waitsec < 0) noFreeCols = 0;   /* disallow nfc if not doing slideshow */
  if (noFreeCols && perfect) { perfect = 0;  owncmap = 1; }

  /* decide what default color allocation stuff we've settled on */
  if (rwcolor) allocMode = AM_READWRITE;

  if (perfect) colorMapMode = CM_PERFECT;
  if (owncmap) colorMapMode = CM_OWNCMAP;
  if (stdcmap) colorMapMode = CM_STDCMAP;

  defaultCmapMode = colorMapMode;  /* default mode for 8-bit images */

  if (nopos) { 
    maingeom = infogeom = ctrlgeom = gamgeom = browgeom = textgeom = NULL;
    cmtgeom = NULL;
  }

  /* if -root and -maxp, disallow 'integer' tiling modes */
  if (useroot && fixedaspect && automax && !rmodeset && 
      (rootMode == RM_TILE || rootMode == RM_IMIRROR))
    rootMode = RM_CSOLID;
}




/***********************************/
static int cpos = 0;
static void printoption(st)
     char *st;
{
  if (strlen(st) + cpos > 78) {
    fprintf(stderr,"\n   ");
    cpos = 3;
  }

  fprintf(stderr,"%s ",st);
  cpos = cpos + strlen(st) + 1;
}

static void cmdSyntax()
{
  fprintf(stderr, "Usage:\n");
  printoption(cmd);
  printoption("[-]");
  printoption("[-/+24]");
  printoption("[-/+2xlimit]");
  printoption("[-/+4x3]");
  printoption("[-/+8]");
  printoption("[-/+acrop]");
  printoption("[-aspect w:h]");
  printoption("[-best24]");
  printoption("[-bg color]");
  printoption("[-black color]");
  printoption("[-bw width]");
  printoption("[-/+cecmap]");
  printoption("[-cegeometry geom]");
  printoption("[-/+cemap]");
  printoption("[-cgamma rval gval bval]");
  printoption("[-cgeometry geom]");
  printoption("[-/+clear]");
  printoption("[-/+close]");
  printoption("[-/+cmap]");
  printoption("[-cmtgeometry geom]");
  printoption("[-/+cmtmap]");
  printoption("[-crop x y w h]");
  printoption("[-cursor char#]");

#ifndef VMS
  printoption("[-DEBUG level]");
#else
  printoption("[-debug level]");
#endif

  printoption("[-dir directory]");
  printoption("[-display disp]");
  printoption("[-/+dither]");
  printoption("[-drift dx dy]");
  printoption("[-expand exp | hexp:vexp]");
  printoption("[-fg color]");
  printoption("[-/+fixed]");
  printoption("[-flist fname]");
  printoption("[-gamma val]");
  printoption("[-geometry geom]");
  printoption("[-grabdelay seconds]");
  printoption("[-gsdev str]");
  printoption("[-gsgeom geom]");
  printoption("[-gsres int]");
  printoption("[-help]");
  printoption("[-/+hflip]");
  printoption("[-hi color]");
  printoption("[-/+hist]");
  printoption("[-/+hsv]");
  printoption("[-icgeometry geom]");
  printoption("[-/+iconic]");
  printoption("[-igeometry geom]");
  printoption("[-/+imap]");
  printoption("[-/+lbrowse]");
  printoption("[-lo color]");
  printoption("[-/+loadclear]");
  printoption("[-/+max]");
  printoption("[-/+maxpect]");
  printoption("[-mfn font]");
  printoption("[-/+mono]");
  printoption("[-name str]");
  printoption("[-ncols #]");
  printoption("[-/+ninstall]");
  printoption("[-/+nodecor]");
  printoption("[-/+nofreecols]");
  printoption("[-/+nolimits]");
  printoption("[-/+nopos]");
  printoption("[-/+noqcheck]");
  printoption("[-/+noresetroot]");
  printoption("[-/+norm]");
  printoption("[-/+nostat]");
  printoption("[-/+owncmap]");
  printoption("[-/+perfect]");
  printoption("[-/+pkludge]");
  printoption("[-/+poll]");
  printoption("[-preset #]");
  printoption("[-quick24]");
  printoption("[-/+quit]");
  printoption("[-/+random]");
  printoption("[-/+raw]");
  printoption("[-rbg color]");
  printoption("[-rfg color]");
  printoption("[-/+rgb]");
  printoption("[-RM]");
  printoption("[-rmode #]");
  printoption("[-/+root]");
  printoption("[-rotate deg]");
  printoption("[-/+rv]");
  printoption("[-/+rw]");
  printoption("[-slow24]");
  printoption("[-/+smooth]");
  printoption("[-/+stdcmap]");
  printoption("[-tgeometry geom]");
  printoption("[-/+vflip]");
  printoption("[-/+viewonly]");
  printoption("[-visual type]");
  printoption("[-/+vsdisable]");
  printoption("[-vsgeometry geom]");
  printoption("[-/+vsmap]");
  printoption("[-/+vsperfect]");
  printoption("[-wait seconds]");
  printoption("[-white color]");
  printoption("[-/+wloop]");
  printoption("[filename ...]");
  fprintf(stderr,"\n\n");
  Quit(1);
}


/***********************************/
static void rmodeSyntax()
{
  fprintf(stderr,"%s: unknown root mode '%d'.  Valid modes are:\n", 
	  cmd, rootMode);
  fprintf(stderr,"\t0: tiling\n");
  fprintf(stderr,"\t1: integer tiling\n");
  fprintf(stderr,"\t2: mirrored tiling\n");
  fprintf(stderr,"\t3: integer mirrored tiling\n");
  fprintf(stderr,"\t4: centered tiling\n");
  fprintf(stderr,"\t5: centered on a solid background\n");
  fprintf(stderr,"\t6: centered on a 'warp' background\n");
  fprintf(stderr,"\t7: centered on a 'brick' background\n");
  fprintf(stderr,"\t8: symmetrical tiling\n");
  fprintf(stderr,"\t9: symmetrical mirrored tiling\n");
  fprintf(stderr,"\n");
  Quit(1);
}


/***********************************/
static int argcmp(a1, a2, minlen, plusallowed, plusminus)
     char *a1, *a2;
     int  minlen, plusallowed;
     int *plusminus;
{
  /* does a string compare between a1 and a2.  To return '0', a1 and a2 
     must match to the length of a2, and that length has to
     be at least 'minlen'.  Otherwise, return non-zero.  plusminus set to '1'
     if '-option', '0' if '+option' */

  int i;

  if ((strlen(a1) < (size_t) minlen) || (strlen(a2) < (size_t) minlen))
    return 1;
  if (strlen(a1) > strlen(a2)) return 1;

  if (strncmp(a1+1, a2+1, strlen(a1)-1)) return 1;

  if (a1[0]=='-' || (plusallowed && a1[0]=='+')) {
    /* only set if we match */
    *plusminus = (a1[0] == '-');    
    return 0;
  }

  return 1;
}


/***********************************/
static int openPic(filenum)
     int filenum;
{
  /* tries to load file #filenum (from 'namelist' list)
   * returns 0 on failure (cleans up after itself)
   * returns '-1' if caller should display DFLTPIC  (shown as text)
   * if successful, returns 1, creates mainW
   *
   * By the way, I'd just like to point out that this procedure has gotten
   * *way* out of hand...
   */

  PICINFO pinfo;
  int   i,filetype,freename, frompipe, frompoll, fromint, killpage;
  int   oldeWIDE, oldeHIGH, oldpWIDE, oldpHIGH;
  int   oldCXOFF, oldCYOFF, oldCWIDE, oldCHIGH, wascropped;
  char *tmp;
  char *fullname,       /* full name of the original file */
        filename[512],  /* full name of file to load (could be /tmp/xxx)*/
        globnm[512];    /* globbed version of fullname of orig file */

  xvbzero((char *) &pinfo, sizeof(PICINFO));

  /* init important fields of pinfo */
  pinfo.pic = (byte *) NULL;
  pinfo.comment = (char *) NULL;
  pinfo.numpages = 1;
  pinfo.pagebname[0] = '\0';


  normaspect = defaspect;
  freename = dfltkludge = frompipe = frompoll = fromint = wascropped = 0;
  oldpWIDE = oldpHIGH = oldCXOFF = oldCYOFF = oldCWIDE = oldCHIGH = 0;
  oldeWIDE = eWIDE;  oldeHIGH = eHIGH;
  fullname = NULL;
  killpage = 0;

  WaitCursor();

  SetISTR(ISTR_INFO,"");
  SetISTR(ISTR_WARNING,"");


  /* if we're not loading next or prev page in a multi-page doc, kill off
     page files */
  if (strlen(pageBaseName) && filenum!=OP_PAGEDN && filenum!=OP_PAGEUP) 
    killpage = 1;


  if (strlen(pageBaseName) && (filenum==OP_PAGEDN || filenum==OP_PAGEUP)) {
    if      (filenum==OP_PAGEUP && curPage>0)          curPage--;
    else if (filenum==OP_PAGEDN && curPage<numPages-1) curPage++;
    else    {
      XBell(theDisp, 0);     /* hit either end */
      SetCursors(-1);
      return 0;
    }

    sprintf(filename, "%s%d", pageBaseName, curPage+1);
    fullname = filename;
    goto HAVE_FILENAME;
  }


  if (filenum == DFLTPIC) {
    filename[0] = '\0';  basefname[0] = '\0';  fullfname[0] = '\0';
    fullname = "";
    curname = -1;         /* ??? */
    LoadDfltPic(&pinfo);

    if (killpage) {      /* kill old page files, if any */
      KillPageFiles(pageBaseName, numPages);
      pageBaseName[0] = '\0';
      numPages = 1;
      curPage = 0;
    }

    goto GOTIMAGE;
  }
  else if (filenum == GRABBED) {
    filename[0] = '\0';  basefname[0] = '\0';  fullfname[0] = '\0';
    fullname = "";
    i = LoadGrab(&pinfo);
    if (!i) goto FAILED;   /* shouldn't happen */

    if (killpage) {      /* kill old page files, if any */
      KillPageFiles(pageBaseName, numPages);
      pageBaseName[0] = '\0';
      numPages = 1;
      curPage = 0;
    }

    goto GOTIMAGE;
  }

  else if (filenum == PADDED) {
    /* need fullfname (used for window/icon name), 
       basefname(compute from fullfname) */

    i = LoadPad(&pinfo, fullfname);
    fullname = fullfname;
    strcpy(filename, fullfname);
    tmp = BaseName(fullfname);
    strcpy(basefname, tmp);

    if (!i) goto FAILED;   /* shouldn't happen */

    if (killpage) {      /* kill old page files, if any */
      KillPageFiles(pageBaseName, numPages);
      pageBaseName[0] = '\0';
      numPages = 1;
      curPage = 0;
    }

    goto GOTIMAGE;
  }


  if (filenum == POLLED) {
    frompoll = 1;
    oldpWIDE = pWIDE;  oldpHIGH = pHIGH;
    wascropped = (cWIDE!=pWIDE || cHIGH!=pHIGH);
    oldCXOFF = cXOFF;  oldCYOFF = cYOFF;  oldCWIDE = cWIDE;  oldCHIGH = cHIGH;
    filenum = curname;
  }

  if (filenum == RELOAD) {
    fromint = 1;
    filenum = nList.selected;
  }

  if (filenum != LOADPIC) {
    if (filenum >= nList.nstr || filenum < 0) goto FAILED;
    curname = filenum;
    nList.selected = curname;
    ScrollToCurrent(&nList);  /* have scrl/list show current */
    XFlush(theDisp);    /* update NOW */
  }



  /* set up fullname and basefname */

  if (filenum == LOADPIC) {
    fullname = GetDirFullName();

    if (ISPIPE(fullname[0])) {    /* read from a pipe. */
      strcpy(filename, fullname);
      if (readpipe(fullname, filename)) goto FAILED;
      frompipe = 1;
    }
  }
  else fullname = namelist[filenum];


  strcpy(fullfname, fullname);
  tmp = BaseName(fullname);
  strcpy(basefname, tmp);


  /* chop off trailing ".Z", ".z", or ".gz" from displayed basefname, if any */
  if (strlen(basefname) > (size_t) 2     && 
      strcmp(basefname+strlen(basefname)-2,".Z")==0)
    basefname[strlen(basefname)-2]='\0';
  else {
#ifdef GUNZIP
    if (strlen(basefname)>2 && strcmp(basefname+strlen(basefname)-2,".Z")==0)
      basefname[strlen(basefname)-2]='\0';

    else if (strlen(basefname)>3 && 
	     strcmp(basefname+strlen(basefname)-3,".gz")==0)
      basefname[strlen(basefname)-3]='\0';
#endif /* GUNZIP */
  }


  if (filenum == LOADPIC && ISPIPE(fullname[0])) {
    /* if we're reading from a pipe, 'filename' will have the /tmp/xvXXXXXX
       filename, and we can skip a lot of stuff:  (such as prepending 
       'initdir' to relative paths, dealing with reading from stdin, etc. */

    /* at this point, fullname = "! do some commands",
                      filename = "/tmp/xv123456",
		  and basefname= "xv123456" */
  }

  else {  /* NOT reading from a PIPE */

    /* if fullname doesn't start with a '/' (ie, it's a relative path), 
       (and it's not LOADPIC and it's not the special case '<stdin>') 
       then we need to prepend a directory name to it:
       
       prepend 'initdir', 
       if we have a searchdir (ie, we have multiple places to look),
             see if such a file exists (via fopen()),
       if it does, we're done.
       if it doesn't, and we have a searchdir, try prepending searchdir
             and see if file exists.
       if it does, we're done.
       if it doesn't, remove all prepended directories, and fall through
             to error code below.  */
    
    if (filenum!=LOADPIC && fullname[0]!='/' && strcmp(fullname,STDINSTR)!=0) {
      char *tmp1;

      /* stick 'initdir ' onto front of filename */

#ifndef VMS
      tmp1 = (char *) malloc(strlen(fullname) + strlen(initdir) + 2);
      if (!tmp1) FatalError("malloc 'filename' failed");
      sprintf(tmp1,"%s/%s", initdir, fullname);
#else  /* it is VMS */
      tmp1 = (char *) malloc(strlen(fullname) + 2);
      if (!tmp1) FatalError("malloc 'filename' failed");
      sprintf(tmp1,"%s", fullname);
#endif

      if (!strlen(searchdir)) {            /* no searchdir, don't check */
	fullname = tmp1;
	freename = 1;
      }
      else {                     	   /* see if said file exists */
	FILE *fp;
	fp = fopen(tmp1, "r");
	if (fp) {                          /* initpath/fullname exists */
	  fclose(fp);
	  fullname = tmp1;
	  freename = 1;
	}
	else {                             /* doesn't:  try searchdir */
	  free(tmp1);
#ifndef VMS
	  tmp1 = (char *) malloc(strlen(fullname) + strlen(searchdir) + 2);
	  if (!tmp1) FatalError("malloc 'filename' failed");
	  sprintf(tmp1,"%s/%s", searchdir, fullname);
#else  /* it is VMS */
	  tmp1 = (char *) malloc(strlen(fullname) + 2);
	  if (!tmp1) FatalError("malloc 'filename' failed");
	  sprintf(tmp1,"%s", fullname);
#endif

	  fp = fopen(tmp1, "r");
	  if (fp) {                        /* searchpath/fullname exists */
	    fclose(fp);
	    fullname = tmp1;
	    freename = 1;
	  }
	  else free(tmp1);                  /* doesn't exist either... */
	}
      }
    }
    
    strcpy(filename, fullname);
    
    
    /* if the file is STDIN, write it out to a temp file */

    if (strcmp(filename,STDINSTR)==0) {
      FILE *fp;

#ifndef VMS      
      sprintf(filename,"%s/xvXXXXXX",tmpdir);
#else /* it is VMS */
      sprintf(filename, "[]xvXXXXXX");
#endif
      mktemp(filename);

      clearerr(stdin);
      fp = fopen(filename,"w");
      if (!fp) FatalError("openPic(): can't write temporary file");
    
      while ( (i=getchar()) != EOF) putc(i,fp);
      fclose(fp);

      /* and remove it from list, since we can never reload from stdin */
      if (strcmp(namelist[0], STDINSTR)==0) deleteFromList(0);
    }
  }



 HAVE_FILENAME:

  /******* AT THIS POINT 'filename' is the name of an actual data file
    (no pipes or stdin, though it could be compressed) to be loaded */
  filetype = ReadFileType(filename);


  if (filetype == RFT_COMPRESS) {   /* a compressed file.  uncompress it */
    char tmpname[128];

    if (
#ifndef VMS
	UncompressFile(filename, tmpname)
#else
	UncompressFile(basefname, tmpname)
#endif
	) {

      filetype = ReadFileType(tmpname);    /* and try again */
      
      /* if we made a /tmp file (from stdin, etc.) won't need it any more */
      if (strcmp(fullname,filename)!=0) unlink(filename);

      strcpy(filename, tmpname);
    }
    else filetype = RFT_ERROR;

    WaitCursor();
  }


  if (filetype == RFT_ERROR) {
    char  foostr[512];
    sprintf(foostr,"Can't open file '%s'\n\n  %s.",filename, ERRSTR(errno));

    if (!polling) ErrPopUp(foostr, "\nBummer!");

    goto FAILED;  /* couldn't get magic#, screw it! */
  }


  if (filetype == RFT_UNKNOWN) {
    /* view as a text/hex file */
    TextView(filename);
    SetISTR(ISTR_INFO,"'%s' not in a recognized format.", basefname);
    /* Warning();  */
    goto SHOWN_AS_TEXT;
  }

  if (filetype < RFT_ERROR) {
    SetISTR(ISTR_INFO,"'%s' not in a readable format.", basefname);
    Warning();
    goto FAILED;
  }


  /****** AT THIS POINT: the filetype is a known, readable format */

  /* kill old page files, if any */
  if (killpage) {
    KillPageFiles(pageBaseName, numPages);
    pageBaseName[0] = '\0';
    numPages = 1;
    curPage = 0;
  }


  SetISTR(ISTR_INFO,"Loading...");

  i = ReadPicFile(filename, filetype, &pinfo, 0);

  if (filetype == RFT_XBM && (!i || pinfo.w==0 || pinfo.h==0)) {
    /* probably just a '.h' file or something... */
    SetISTR(ISTR_INFO," ");
    TextView(filename);
    goto SHOWN_AS_TEXT;
  }

  if (!i) {
    SetISTR(ISTR_INFO,"Couldn't load file '%s'.",filename);
    Warning();
    goto FAILED;
  }



  WaitCursor();

  if (pinfo.w==0 || pinfo.h==0) {  /* shouldn't happen, but let's be safe */
    SetISTR(ISTR_INFO,"Image size '0x0' not allowed.");
    Warning();
    if (pinfo.pic)     free(pinfo.pic);      pinfo.pic     = (byte *) NULL;
    if (pinfo.comment) free(pinfo.comment);  pinfo.comment = (char *) NULL;
    goto FAILED;
  }


  /**************/
  /* SUCCESS!!! */
  /**************/
    

 GOTIMAGE:
  /* successfully read this picture.  No failures from here on out
     (well, once the pic has been converted if we're locked in a mode) */


  state824 = 0;

  /* if we're locked into a mode, do appropriate conversion */
  if (conv24MB.flags[CONV24_LOCK]) {  /* locked */
    if (pinfo.type==PIC24 && picType==PIC8) {           /* 24 -> 8 bit */
      byte *pic8;
      pic8 = Conv24to8(pinfo.pic, pinfo.w, pinfo.h, ncols, 
		       pinfo.r, pinfo.g, pinfo.b);
      free(pinfo.pic);
      pinfo.pic = pic8;
      pinfo.type = PIC8;

      state824 = 1;
    }

    else if (pinfo.type!=PIC24 && picType==PIC24) {    /* 8 -> 24 bit */
      byte *pic24;
      pic24 = Conv8to24(pinfo.pic, pinfo.w, pinfo.h, 
			pinfo.r, pinfo.g, pinfo.b);
      free(pinfo.pic);
      pinfo.pic  = pic24;
      pinfo.type = PIC24;
    }
  }
  else {    /* not locked.  switch 8/24 mode */
    picType = pinfo.type;
    Set824Menus(picType);
  }


  if (!pinfo.pic) {  /* must've failed in the 8-24 or 24-8 conversion */
    SetISTR(ISTR_INFO,"Couldn't do %s conversion.",
	    (picType==PIC24) ? "8->24" : "24->8");
    if (pinfo.comment) free(pinfo.comment);  pinfo.comment = (char *) NULL;
    Warning();
    goto FAILED;
  }



  /* ABSOLUTELY no failures from here on out... */


  if (strlen(pinfo.pagebname)) {
    strcpy(pageBaseName, pinfo.pagebname);
    numPages = pinfo.numpages;
    curPage = 0;
  }

  ignoreConfigs = 1;

  if (mainW && !useroot) {
    /* avoid generating excess configure events while we resize the window */
    XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask 
		 | StructureNotifyMask
                 | ButtonPressMask | KeyReleaseMask
                 | EnterWindowMask | LeaveWindowMask);
    XFlush(theDisp);
  }

  /* kill off OLD picture, now that we've succesfully loaded a new one */
  KillOldPics();
  SetInfoMode(INF_STR);


  /* get info out of the PICINFO struct */
  pic   = pinfo.pic;
  pWIDE = pinfo.w;
  pHIGH = pinfo.h;
  if (pinfo.frmType >=0) SetDirSaveMode(F_FORMAT, pinfo.frmType);
  if (pinfo.colType >=0) SetDirSaveMode(F_COLORS, pinfo.colType);
  
  SetISTR(ISTR_FORMAT, pinfo.fullInfo);
  strcpy(formatStr, pinfo.shrtInfo);
  picComments = pinfo.comment;
  ChangeCommentText();

  for (i=0; i<256; i++) {
    rMap[i] = pinfo.r[i];
    gMap[i] = pinfo.g[i];
    bMap[i] = pinfo.b[i];
  }



  AlgInit();

  /* stick this file in the 'ctrlW' name list */
  if (filenum == LOADPIC && !frompipe) StickInCtrlList(1);

  if (polling && !frompoll) InitPoll();

  /* turn off 'frompoll' if the pic has changed size */
  if (frompoll && (pWIDE != oldpWIDE || pHIGH != oldpHIGH)) frompoll = 0;


  if (!browseCB.val && filenum == LOADPIC) DirBox(0);   /* close the DirBox */


  /* if we read a /tmp file, delete it.  won't be needing it any more */
  if (fullname && strcmp(fullname,filename)!=0) unlink(filename);


  SetISTR(ISTR_INFO,formatStr);
	
  SetInfoMode(INF_PART);
  SetISTR(ISTR_FILENAME, 
	  (filenum==DFLTPIC || filenum==GRABBED || frompipe)
	  ? "<none>" : basefname);

  SetISTR(ISTR_RES,"%d x %d",pWIDE,pHIGH);
  SetISTR(ISTR_COLOR, "");
  SetISTR(ISTR_COLOR2,"");

  /* adjust button in/activity */
  if (HaveSelection()) EnableSelection(0);
  SetSelectionString();
  BTSetActive(&but[BCROP],   0);
  BTSetActive(&but[BUNCROP], 0);
  BTSetActive(&but[BCUT],    0);
  BTSetActive(&but[BCOPY],   0);
  BTSetActive(&but[BCLEAR],  0);

  ActivePrevNext();



  /* handle various 'auto-whatever' command line options
     Note that if 'frompoll' is set, things that have to do with 
     setting the expansion factor are skipped, as we'll want it to
     display in the (already-existing) window at the same size */


  if (frompoll) {
    cpic = pic;  cWIDE = pWIDE;  cHIGH = pHIGH;  cXOFF = cYOFF = 0;
    FreeEpic();
    if (wascropped) DoCrop(oldCXOFF, oldCYOFF, oldCWIDE, oldCHIGH);
    FreeEpic();  eWIDE = oldeWIDE;  eHIGH = oldeHIGH;
    SetCropString();
  }
  else {
    int w,h,aspWIDE,aspHIGH,oldemode;

    oldemode = epicMode;
    epicMode = EM_RAW;   /* be in raw mode for all intermediate conversions */
    cpic = pic;  cWIDE = pWIDE;  cHIGH = pHIGH;  cXOFF = cYOFF = 0;
    epic = cpic; eWIDE = cWIDE;  eHIGH = cHIGH;

    SetCropString();

    /*****************************************/
    /* handle aspect options:  -aspect, -4x3 */
    /*****************************************/

    if (normaspect != 1.0) {  /* -aspect */
      FixAspect(1, &w, &h);
      eWIDE = w;  eHIGH = h;
    }

    if (auto4x3) {
      w = eWIDE;  h = (w*3) / 4;
      eWIDE = w;  eHIGH = h;
    }
    

    if (eWIDE != cWIDE || eHIGH != cHIGH) epic = (byte *) NULL;

    /**************************************/
    /* handle cropping (-acrop and -crop) */
    /**************************************/

    if (autocrop) DoAutoCrop();
    if (acrop)    DoCrop(acropX, acropY, acropW, acropH);

    if (eWIDE != cWIDE || eHIGH != cHIGH) epic = (byte *) NULL;

    /********************************/
    /* handle rotation and flipping */
    /********************************/

    if (autorotate) {
      /* figure out optimal rotation.  (ie, instead of +270, do -90) */
      if      (autorotate ==  270) autorotate = -90;
      else if (autorotate == -270) autorotate =  90;

      i = autorotate;
      while (i) {
	if (i < 0) {   /* rotate CW */
	  DoRotate(0);
	  i += 90;
	}
	else {  /* rotate CCW */
	  DoRotate(1);
	  i -= 90;
	}
      }
    }

    if (autohflip) Flip(0);  /* horizontal flip */
    if (autovflip) Flip(1);  /* vertical flip */


    /********************************************/
    /* handle expansion options:                */
    /*   -expand, -max, -maxpect, -fixed, -geom */
    /********************************************/

    /* at this point, treat whatever eWIDE,eHIGH is as 1x1 expansion,
       (due to earlier aspect-ratio option handling).  Note that
       all that goes on here is that eWIDE/eHIGH are modified.  No
       images are generated. */

    aspWIDE = eWIDE;  aspHIGH = eHIGH;   /* aspect-corrected eWIDE,eHIGH */

    if (hexpand < 0.0) eWIDE=(int)(aspWIDE / -hexpand);  /* neg:  reciprocal */
                  else eWIDE=(int)(aspWIDE *  hexpand);  
    if (vexpand < 0.0) eHIGH=(int)(aspHIGH / -vexpand);  /* neg:  reciprocal */
                  else eHIGH=(int)(aspHIGH *  vexpand);  

    if (maingeom) {
      /* deal with geometry spec.  Note, they shouldn't have given us
       *both* an expansion factor and a geomsize.  The geomsize wins out */
    
      int i,x,y,gewide,gehigh;  u_int w,h;

      gewide = eWIDE;  gehigh = eHIGH;
      i = XParseGeometry(maingeom,&x,&y,&w,&h);

      if (i&WidthValue)  gewide = (int) w;
      if (i&HeightValue) gehigh = (int) h;
      
      /* handle case where the pinheads only specified width *or * height */
      if (( i&WidthValue && ~i&HeightValue) ||
	  (~i&WidthValue &&  i&HeightValue)) {
    
	if (i&WidthValue) { gehigh = (aspHIGH * gewide) / pWIDE; }
	             else { gewide = (aspWIDE * gehigh) / pHIGH; }
      }

      /* specified a 'maximum size, but please keep your aspect ratio */
      if (fixedaspect && i&WidthValue && i&HeightValue) {
	if (aspWIDE > gewide || aspHIGH > gehigh) {
	  /* shrink aspWIDE,HIGH accordingly... */
	  double r,wr,hr;

	  wr = ((double) aspWIDE) / gewide;
	  hr = ((double) aspHIGH) / gehigh;

	  r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
	  aspWIDE = (int) ((aspWIDE / r) + 0.5);
	  aspHIGH = (int) ((aspHIGH / r) + 0.5);
	}

	/* image is now definitely no larger than gewide,gehigh */
	/* should now expand it so that one axis is of specified size */

	if (aspWIDE != gewide && aspHIGH != gehigh) {  /* is smaller */
	  /* grow aspWIDE,HIGH accordingly... */
	  double r,wr,hr;

	  wr = ((double) aspWIDE) / gewide;
	  hr = ((double) aspHIGH) / gehigh;

	  r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
	  aspWIDE = (int) ((aspWIDE / r) + 0.5);
	  aspHIGH = (int) ((aspHIGH / r) + 0.5);

	}

	eWIDE = aspWIDE;  eHIGH = aspHIGH;
      }
      else { eWIDE = gewide;  eHIGH = gehigh; }
    }


    if (automax) {   /* -max and -maxpect */
      eWIDE = dispWIDE;  eHIGH = dispHIGH;
      if (fixedaspect) FixAspect(0,&eWIDE,&eHIGH);
    }


    /* now, just make sure that eWIDE/eHIGH aren't too big... */
    /* shrink eWIDE,eHIGH preserving aspect ratio, if so... */
    if (eWIDE>maxWIDE || eHIGH>maxHIGH) {
      /* the numbers here can get big.  use floats */
      double r,wr,hr;

      wr = ((double) eWIDE) / maxWIDE;
      hr = ((double) eHIGH) / maxHIGH;

      r = (wr>hr) ? wr : hr;   /* r is the max(wr,hr) */
      eWIDE = (int) ((eWIDE / r) + 0.5);
      eHIGH = (int) ((eHIGH / r) + 0.5);
    }

    if (eWIDE < 1) eWIDE = 1;    /* just to be safe... */
    if (eHIGH < 1) eHIGH = 1;

    /* if we're using an integer tiled root mode, truncate eWIDE/eHIGH to
       be an integer divisor of the display size */
      
    if (useroot && (rootMode == RM_TILE || rootMode == RM_IMIRROR)) {
      /* make picture size a divisor of the rootW size.  round down */
      i = (dispWIDE + eWIDE-1) / eWIDE;   eWIDE = (dispWIDE + i-1) / i;
      i = (dispHIGH + eHIGH-1) / eHIGH;   eHIGH = (dispHIGH + i-1) / i;
    }


    if (eWIDE != cWIDE || eHIGH != cHIGH) epic = (byte *) NULL;

    /********************************************/
    /* handle epic generation options:          */
    /*   -raw, -dith, -smooth                   */
    /********************************************/

    if (autodither && ncols>0) epicMode = EM_DITH;

    /* if in CM_STDCMAP mode, and *not* in '-wait 0', then autodither */
    if (colorMapMode == CM_STDCMAP && waitsec != 0) epicMode = EM_DITH;

    /* if -smooth or image has been shrunk to fit screen */
    if (autosmooth || (pWIDE >maxWIDE || pHIGH>maxHIGH)
	           || (cWIDE != eWIDE || cHIGH != eHIGH)) epicMode = EM_SMOOTH;

    if (autoraw) epicMode = EM_RAW;

    /* 'dithering' makes no sense in 24-bit mode */
    if (picType == PIC24 && epicMode == EM_DITH) epicMode = EM_RAW;
    
    SetEpicMode();
  } /* end of !frompoll */



  /* at this point eWIDE,eHIGH are correct, but a proper epic (particularly
     if in DITH or SMOOTH mode) doesn't exist yet.  Will be made once the
     colors have been picked. */



  /* clear old image (window/root) before we start changing colors... */
  if (CMAPVIS(theVisual) && clearonload && picType == PIC8 &&
      colorMapMode != CM_STDCMAP) {

    if (mainW && !useroot) {
      XClearArea(theDisp, mainW, 0,0, (u_int)oldeWIDE, (u_int)oldeHIGH, True);
      XFlush(theDisp);
    }

    if (useroot) {
      mainW = vrootW;
      ClearRoot();
    }
  }


  if (useroot) mainW = vrootW;
  if (eWIDE != cWIDE || eHIGH != cHIGH) epic = (byte *) NULL;

  NewPicGetColors(autonorm, autohisteq); 

  GenerateEpic(eWIDE, eHIGH);     /* want to dither *after* color allocs */
  CreateXImage();

  WaitCursor();
  HandleDispMode();   /* create root pic, or mainW, depending... */


  if (LocalCmap) {
    XSetWindowAttributes xswa;
    xswa.colormap = LocalCmap;

    if (!ninstall) XInstallColormap(theDisp,LocalCmap);
    XChangeWindowAttributes(theDisp,mainW,CWColormap,&xswa);
    if (cmapInGam) XChangeWindowAttributes(theDisp,gamW,CWColormap,&xswa);
  }



  tmp = GetISTR(ISTR_COLOR);
  SetISTR(ISTR_INFO,"%s  %s  %s", formatStr,
	  (picType==PIC8) ? "8-bit mode." : "24-bit mode.",
	  tmp);
	
  SetInfoMode(INF_FULL);
  if (freename) free(fullname);


  SetCursors(-1);


  if (dirUp!=BLOAD) {
    /* put current filename into the 'save-as' filename */
    if      (strcmp(filename,STDINSTR)==0)   SetDirFName("stdin");
    else if (frompipe || filenum == LOADPIC || filenum == GRABBED ||
	     filenum == DFLTPIC || filenum == PADDED) {} /* leave it alone */
    else SetDirFName(basefname);
  }


  /* force a redraw of the whole window, as I can't quite trust Config's
     to generate the correct exposes (particularly with 'BitGravity' turned
     on */

  if (mainW && !useroot) GenExpose(mainW, 0, 0, (u_int) eWIDE, (u_int) eHIGH);

  return 1;

  
 FAILED:
  SetCursors(-1);
  KillPageFiles(pinfo.pagebname, pinfo.numpages);

  if (fullname && strcmp(fullname,filename)!=0) 
    unlink(filename);   /* kill /tmp file */
  if (freename) free(fullname);

  if (!fromint && !polling && filenum>=0 && filenum<nList.nstr) 
    deleteFromList(filenum);

  if (polling) sleep(1);

  return 0;


 SHOWN_AS_TEXT:    /* file wasn't in recognized format... */
  SetCursors(-1);

  if (strcmp(fullname,filename)!=0) unlink(filename);   /* kill /tmp file */
  if (freename) free(fullname);

  ActivePrevNext();
  return 1;       /* we've displayed the file 'ok' */
}



/********************************/
int ReadFileType(fname)
     char *fname;
{
  /* opens fname (which *better* be an actual file by this point!) and
     reads the first couple o' bytes.  Figures out what the file's likely
     to be, and returns the appropriate RFT_*** code */


  FILE *fp;
  byte  magicno[30];    /* first 30 bytes of file */
  int   rv, n;

  if (!fname) return RFT_ERROR;   /* shouldn't happen */

  fp = xv_fopen(fname, "r");
  if (!fp) return RFT_ERROR;

  n = fread(magicno, (size_t) 1, (size_t) 30, fp);  
  fclose(fp);

  if (n<30) return RFT_UNKNOWN;    /* files less than 30 bytes long... */

  rv = RFT_UNKNOWN;

  if (strncmp((char *) magicno,"GIF87a", (size_t) 6)==0 ||
      strncmp((char *) magicno,"GIF89a", (size_t) 6)==0)        rv = RFT_GIF;

  else if (strncmp((char *) magicno,"VIEW", (size_t) 4)==0 ||
	   strncmp((char *) magicno,"WEIV", (size_t) 4)==0)     rv = RFT_PM;

  else if (magicno[0] == 'P' && magicno[1]>='1' && 
	   magicno[1]<='6')                             rv = RFT_PBM;

  /* note: have to check XPM before XBM, as first 2 chars are the same */
  else if (strncmp((char *) magicno, "/* XPM */", (size_t) 9)==0) rv = RFT_XPM;

  else if (strncmp((char *) magicno,"#define", (size_t) 7)==0 ||
	   (magicno[0] == '/' && magicno[1] == '*'))    rv = RFT_XBM;

  else if (magicno[0]==0x59 && (magicno[1]&0x7f)==0x26 &&
	   magicno[2]==0x6a && (magicno[3]&0x7f)==0x15) rv = RFT_SUNRAS;

  else if (magicno[0] == 'B' && magicno[1] == 'M')      rv = RFT_BMP;

  else if (magicno[0]==0x52 && magicno[1]==0xcc)        rv = RFT_UTAHRLE;

  else if ((magicno[0]==0x01 && magicno[1]==0xda) ||
	   (magicno[0]==0xda && magicno[1]==0x01))      rv = RFT_IRIS;

  else if (magicno[0]==0x1f && magicno[1]==0x9d)        rv = RFT_COMPRESS;

#ifdef GUNZIP
  else if (magicno[0]==0x1f && magicno[1]==0x8b)        rv = RFT_COMPRESS;
#endif

  else if (magicno[0]==0x0a && magicno[1] <= 5)         rv = RFT_PCX;

  else if (strncmp((char *) magicno,   "FORM", (size_t) 4)==0 && 
	   strncmp((char *) magicno+8, "ILBM", (size_t) 4)==0)   rv = RFT_IFF;

  else if (magicno[0]==0 && magicno[1]==0 &&
	   magicno[2]==2 && magicno[3]==0 &&
	   magicno[4]==0 && magicno[5]==0 &&
	   magicno[6]==0 && magicno[7]==0)              rv = RFT_TARGA;

  else if (magicno[4]==0x00 && magicno[5]==0x00 &&
	   magicno[6]==0x00 && magicno[7]==0x07)        rv = RFT_XWD;

  else if (strncmp((char *) magicno,"SIMPLE  ", (size_t) 8)==0 && 
	   magicno[29] == 'T')                          rv = RFT_FITS;
  

#ifdef HAVE_JPEG
  else if (magicno[0]==0xff && magicno[1]==0xd8 && 
	   magicno[2]==0xff)                            rv = RFT_JFIF;
#endif

#ifdef HAVE_TIFF
  else if ((magicno[0]=='M' && magicno[1]=='M') ||
	   (magicno[0]=='I' && magicno[1]=='I'))        rv = RFT_TIFF;
#endif

#ifdef HAVE_PDS
  else if (strncmp((char *) magicno,  "NJPL1I00", (size_t) 8)==0 ||
	   strncmp((char *) magicno+2,"NJPL1I",   (size_t) 6)==0 ||
           strncmp((char *) magicno,  "CCSD3ZF",  (size_t) 7)==0 ||
	   strncmp((char *) magicno+2,"CCSD3Z",   (size_t) 6)==0 ||
	   strncmp((char *) magicno,  "LBLSIZE=", (size_t) 8)==0)
      rv = RFT_PDSVICAR;
#endif

#ifdef GS_PATH
  else if (strncmp((char *) magicno, "%!",     (size_t) 2)==0 ||
	   strncmp((char *) magicno, "\004%!", (size_t) 3)==0)   rv = RFT_PS;
#endif

  return rv;
}



/********************************/
int ReadPicFile(fname, ftype, pinfo, quick)
     char    *fname;
     int      ftype, quick;
     PICINFO *pinfo;
{
  /* if quick is set, we're being called to generate icons, or something
     like that.  We should load the image as quickly as possible.  Currently,
     this only affects the LoadPS routine, which, if quick is set, only
     generates the page file for the first page of the document */

  int rv = 0;

  /* by default, most formats aren't multi-page */
  pinfo->numpages = 1;
  pinfo->pagebname[0] = '\0';

  switch (ftype) {
  case RFT_GIF:     rv = LoadGIF   (fname, pinfo);         break;
  case RFT_PM:      rv = LoadPM    (fname, pinfo);         break;
  case RFT_PBM:     rv = LoadPBM   (fname, pinfo);         break;
  case RFT_XBM:     rv = LoadXBM   (fname, pinfo);         break;
  case RFT_SUNRAS:  rv = LoadSunRas(fname, pinfo);         break;
  case RFT_BMP:     rv = LoadBMP   (fname, pinfo);         break;
  case RFT_UTAHRLE: rv = LoadRLE   (fname, pinfo);         break;
  case RFT_IRIS:    rv = LoadIRIS  (fname, pinfo);         break;
  case RFT_PCX:     rv = LoadPCX   (fname, pinfo);         break;
  case RFT_IFF:     rv = LoadIFF   (fname, pinfo);         break;
  case RFT_TARGA:   rv = LoadTarga (fname, pinfo);         break;
  case RFT_XPM:     rv = LoadXPM   (fname, pinfo);         break;
  case RFT_XWD:     rv = LoadXWD   (fname, pinfo);         break;
  case RFT_FITS:    rv = LoadFITS  (fname, pinfo, quick);  break;

#ifdef HAVE_JPEG
  case RFT_JFIF:    rv = LoadJFIF  (fname, pinfo, quick);    break;
#endif

#ifdef HAVE_TIFF
  case RFT_TIFF:    rv = LoadTIFF  (fname, pinfo);           break;
#endif

#ifdef HAVE_PDS
  case RFT_PDSVICAR: rv = LoadPDS  (fname, pinfo);           break;
#endif

#ifdef GS_PATH
  case RFT_PS:      rv = LoadPS    (fname, pinfo, quick);    break;
#endif

  }
  return rv;
}


/********************************/
int UncompressFile(name, uncompname)
     char *name, *uncompname;
{
  /* returns '1' on success, with name of uncompressed file in uncompname
     returns '0' on failure */

  char namez[128], *fname, buf[512];

  fname = name;
  namez[0] = '\0';


#if !defined(VMS) && !defined(GUNZIP)
  /* see if compressed file name ends with '.Z'.  If it *doesn't* we need
     temporarily rename it so it *does*, uncompress it, and rename *back*
     to what it was.  necessary because uncompress doesn't handle files
     that don't end with '.Z' */

  if (strlen(name) >= (size_t) 2            && 
      strcmp(name + strlen(name)-2,".Z")!=0 &&
      strcmp(name + strlen(name)-2,".z")!=0) {
    strcpy(namez, name);
    strcat(namez,".Z");

    if (rename(name, namez) < 0) {
      sprintf(buf, "Error renaming '%s' to '%s':  %s",
	      name, namez, ERRSTR(errno));
      ErrPopUp(buf, "\nBummer!");
      return 0;
    }

    fname = namez;
  }
#endif   /* not VMS and not GUNZIP */


  
#ifndef VMS
  sprintf(uncompname, "%s/xvuXXXXXX", tmpdir);
  mktemp(uncompname);
  sprintf(buf,"%s -c %s >%s", UNCOMPRESS, fname, uncompname);
#else /* it IS VMS */
  strcpy(uncompname, "[]xvuXXXXXX");
  mktemp(uncompname);
#  ifdef GUNZIP
  sprintf(buf,"%s %s %s", UNCOMPRESS, fname, uncompname);
#  else
  sprintf(buf,"%s %s", UNCOMPRESS, fname);
#  endif
#endif

  SetISTR(ISTR_INFO, "Uncompressing '%s'...", BaseName(fname));
#ifndef VMS
  if (system(buf)) {
#else
  if (!system(buf)) {
#endif
    SetISTR(ISTR_INFO, "Unable to uncompress '%s'.", BaseName(fname));
    Warning();
    return 0;
  }

#ifndef VMS  
  /* if we renamed the file to end with a .Z for the sake of 'uncompress', 
     rename it back to what it once was... */

  if (strlen(namez)) {
    if (rename(namez, name) < 0) {
      sprintf(buf, "Error renaming '%s' to '%s':  %s",
	      namez, name, ERRSTR(errno));
      ErrPopUp(buf, "\nBummer!");
    }
  }
#else
  /*
    sprintf(buf,"Rename %s %s", fname, uncompname);
    SetISTR(ISTR_INFO,"Renaming '%s'...", fname);
    if (!system(buf)) {
    SetISTR(ISTR_INFO,"Unable to rename '%s'.", fname);
    Warning();
    return 0;
    }
   */
#endif /* not VMS */
  
  return 1;
}


/********************************/
void KillPageFiles(bname, numpages)
  char *bname;
  int   numpages;
{
  /* deletes any page files (numbered 1 through numpages) that might exist */
  char tmp[128];
  int  i;

  if (strlen(bname) == 0) return;   /* no page files */

  for (i=1; i<=numpages; i++) {
    sprintf(tmp, "%s%d", bname, i);
    unlink(tmp);
  }
}


/********************************/
void NewPicGetColors(donorm, dohist)
     int donorm, dohist;
{
  int i;

  /* some stuff that necessary whenever running an algorithm or 
     installing a new 'pic' (or switching 824 modes) */

  numcols = 0;   /* gets set by SortColormap:  set to 0 for PIC24 images */
  for (i=0; i<256; i++) cols[i]=infobg;   

  if (picType == PIC8) {
    byte trans[256];
    SortColormap(pic,pWIDE,pHIGH,&numcols,rMap,gMap,bMap,colAllocOrder,trans);
    ColorCompress8(trans);
  }

  if (picType == PIC8) {
    /* see if image is a b/w bitmap.  
       If so, use '-black' and '-white' colors */
    if (numcols == 2) {
      if ((rMap[0] == gMap[0] && rMap[0] == bMap[0] && rMap[0] == 255) &&
	  (rMap[1] == gMap[1] && rMap[1] == bMap[1] && rMap[1] ==   0)) {
	/* 0=wht, 1=blk */
	rMap[0] = (whtRGB>>16)&0xff;  
	gMap[0] = (whtRGB>>8)&0xff;  
	bMap[0] = whtRGB&0xff;

	rMap[1] = (blkRGB>>16)&0xff;
	gMap[1] = (blkRGB>>8)&0xff;  
	bMap[1] = blkRGB&0xff;
      }

      else if ((rMap[0] == gMap[0] && rMap[0] == bMap[0] && rMap[0] ==   0) &&
	       (rMap[1] == gMap[1] && rMap[1] == bMap[1] && rMap[1] == 255)) {
	/*0=blk,1=wht*/
	rMap[0] = (blkRGB>>16)&0xff;
	gMap[0] = (blkRGB>>8)&0xff;
	bMap[0] = blkRGB&0xff;

	rMap[1] = (whtRGB>>16)&0xff;
	gMap[1] = (whtRGB>>8)&0xff;
	bMap[1]=whtRGB&0xff;
      }
    }
  }


  if (picType == PIC8) {
    /* reverse image, if desired */
    if (revvideo) {
      for (i=0; i<numcols; i++) {
	rMap[i] = 255 - rMap[i];
	gMap[i] = 255 - gMap[i];
	bMap[i] = 255 - bMap[i];
      }
    }

    /* save the desired RGB colormap (before dicking with it) */
    for (i=0; i<numcols; i++) { 
      rorg[i] = rcmap[i] = rMap[i];  
      gorg[i] = gcmap[i] = gMap[i];  
      borg[i] = bcmap[i] = bMap[i];  
    }
  }

  else if (picType == PIC24 && revvideo) {
    if (pic)                        InvertPic24(pic,     pWIDE, pHIGH);
    if (cpic && cpic!=pic)          InvertPic24(cpic,    cWIDE, cHIGH);
    if (epic && epic!=cpic)         InvertPic24(epic,    eWIDE, eHIGH);
    if (egampic && egampic != epic) InvertPic24(egampic, eWIDE, eHIGH);
  }


  NewCMap();

  if (donorm) DoNorm();
  if (dohist) DoHistEq();

  GammifyColors();

  if (picType == PIC24) ChangeCmapMode(CM_STDCMAP, 0, 1);
                   else ChangeCmapMode(defaultCmapMode, 0, 1);

  ChangeEC(0);
}



/***********************************/
static int readpipe(cmd, fname)
     char *cmd, *fname;
{
  /* cmd is something like: "! bggen 100 0 0"
   *
   * runs command (with "> /tmp/xv******" appended).  
   * returns "/tmp/xv******" in fname
   * returns '0' if everything's cool, '1' on error
   */

  char fullcmd[512], tmpname[64], str[512];
  int i;

  if (!cmd || (strlen(cmd) < (size_t) 2)) return 1;

  sprintf(tmpname,"%s/xvXXXXXX", tmpdir);
  mktemp(tmpname);
  if (tmpname[0] == '\0') {   /* mktemp() blew up */
    sprintf(str,"Unable to create temporary filename.");
    ErrPopUp(str, "\nHow unlikely!");
    return 1;
  }

  /* build command */
  strcpy(fullcmd, cmd+1);  /* skip the leading '!' character in cmd */
  strcat(fullcmd, " > ");
  strcat(fullcmd, tmpname);

  /* execute the command */
  sprintf(str, "Doing command: '%s'", fullcmd);
  OpenAlert(str);
  i = system(fullcmd);
  if (i) {
    sprintf(str, "Unable to complete command:\n  %s\n\n  exit status: %d",
	    fullcmd, i);
    CloseAlert();
    ErrPopUp(str, "\nThat Sucks!");
    unlink(tmpname);      /* just in case it was created */
    return 1;
  }

  CloseAlert();
  strcpy(fname, tmpname);
  return 0;
}






/****************/
static void openFirstPic()
{
  int i;

  if (!numnames) {  openPic(DFLTPIC);  return; }

  i = 0;
  if (!randomShow) {
    while (numnames>0) {
      if (openPic(0)) return;  /* success */
      else {
	if (polling && !i) 
	  fprintf(stderr,"%s: POLLING: Waiting for file '%s' \n\tto %s\n",
		  cmd, namelist[0], "be created, or whatever...");
	i = 1;
      }
    }
  }

  else {    /* pick random first picture */
    for (i=findRandomPic();  i>=0;  i=findRandomPic())
      if (openPic(i)) return;    /* success */
  }

  if (numnames>1) FatalError("couldn't open any pictures");
  else Quit(-1);
}


/****************/
static void openNextPic()
{
  int i;

  if (curname>=0) i = curname+1;
  else if (nList.selected >= 0 && nList.selected < numnames) 
       i = nList.selected;
  else i = 0;

 
  while (i<numnames && !openPic(i));
  if (i<numnames) return;    /* success */

  openPic(DFLTPIC);
}


/****************/
static void openNextQuit()
{
  int i;

  if (!randomShow) {
    if (curname>=0) i = curname+1;
    else if (nList.selected >= 0 && nList.selected < numnames) 
      i = nList.selected;
    else i = 0;

    while (i<numnames && !openPic(i));
    if (i<numnames) return;    /* success */
  }
  else {
    for (i=findRandomPic(); i>=0; i=findRandomPic())
      if (openPic(i)) return;
  }

  Quit(0);
}


/****************/
static void openNextLoop()
{
  int i,j,loop;

  j = loop = 0;
  while (1) {
    if (!randomShow) {

      if (curname>=0) i = curname+1;
      else if (nList.selected >= 0 && nList.selected < numnames) 
	i = nList.selected;
      else i = 0;

      if (loop) {  i = 0;   loop = 0; }

      while (i<numnames && !openPic(i));
      if (i<numnames) return;
    }
    else {
      for (i=findRandomPic(); i>=0; i=findRandomPic())
	if (openPic(i)) return;
    }

    loop = 1;        /* back to top of list */
    if (j) break;                         /* we're in a 'failure loop' */
    j++;
  }

  openPic(DFLTPIC);
}


/****************/
static void openPrevPic()
{
  int i;

  if (curname>0) i = curname-1;
  else if (nList.selected>0 && nList.selected < numnames) 
    i = nList.selected - 1;
  else i = numnames-1;

  for ( ; i>=0; i--) {
    if (openPic(i)) return;    /* success */
  }

  openPic(DFLTPIC);
}


/****************/
static void openNamedPic()
{
  /* if (!openPic(LOADPIC)) openPic(DFLTPIC); */
  openPic(LOADPIC);
}




/****************/
static int findRandomPic()
/****************/
{
  static byte *loadList;
  static int   left_to_load, listLen = -1;
  int          k;
  time_t       t;

  /* picks a random name out of the list, and returns it's index.  If there
     are no more names to pick, it returns '-1' and resets itself */

  if (!loadList || numnames!=listLen) {
    if (loadList) free(loadList);
    else {
      time(&t);
      srandom((unsigned int) t); /* seed the random */
    }

    left_to_load = listLen = numnames;
    loadList = (byte *) malloc((size_t) listLen);
    for (k=0; k<listLen; k++) loadList[k] = 0;
  }
  
  if (left_to_load <= 0) {   /* we've loaded all the pics */
    for (k=0; k<listLen; k++) loadList[k] = 0;   /* clear flags */
    left_to_load = listLen;
    return -1;   /* 'done' return */
  }

  for (k=abs(random()) % listLen;  loadList[k];  k = (k+1) % listLen);
  
  left_to_load--;
  loadList[k] = TRUE;

  return k;
}

/****************/
static void mainLoop()
{
  /* search forward until we manage to display a picture, 
     then call EventLoop.  EventLoop will eventually return 
     NEXTPIC, PREVPIC, NEXTQUIT, QUIT, or, if >= 0, a filenum to GOTO */

  int i;

  /* if curname<0 (there is no 'current' file), 'Next' means view the 
     selected file (or the 0th file, if no selection either), and 'Prev' means
     view the one right before the selected file */

  openFirstPic();   /* find first displayable picture, exit if none */

  if (!pic)  {  /* must've opened a text file...  display dflt pic */
    openPic(DFLTPIC);
    if (mainW && !useroot) RaiseTextWindows();
  }

  if (useroot && autoquit) Quit(0);

  while ((i=EventLoop()) != QUIT) {
    if      (i==NEXTPIC) {
      if ((curname<0 && numnames>0) ||
	  (curname<numnames-1))  openNextPic();
    }

    else if (i==PREVPIC) {
      if (curname>0 || (curname<0 && nList.selected>0)) 
	openPrevPic();
    }

    else if (i==NEXTQUIT) openNextQuit();
    else if (i==NEXTLOOP) openNextLoop();
    else if (i==LOADPIC)  openNamedPic();

    else if (i==DELETE)   {  /* deleted currently-viewed image */
      curname = -1;
      ActivePrevNext();
      if (but[BNEXT].active)
	FakeButtonPress(&but[BNEXT]);
      else openPic(DFLTPIC);
    }

    else if (i==THISNEXT) {  /* open current sel, 'next' until success */
      int j;
      j = nList.selected;  
      if (j<0) j = 0;
      while (j<numnames && !openPic(j));
      if (!pic) openPic(DFLTPIC);
    }

    else if (i>=0 || i==GRABBED || i==POLLED || i==RELOAD ||
	     i==OP_PAGEUP || i==OP_PAGEDN || i==DFLTPIC || i==PADDED) {
      openPic(i);
      /* if (!openPic(i)) openPic(DFLTPIC); */
    }
  }
}



/***********************************/
static void createMainWindow(geom, name)
     char *geom, *name;
{
  XSetWindowAttributes xswa;
  unsigned long        xswamask;
  XWindowAttributes    xwa;
  XWMHints             xwmh;
  XSizeHints           hints;
  XClassHint           classh;
  int                  i,x,y;
  unsigned int         w,h;
  static int           firstTime = 1;

  /*
   * this function mainly deals with parsing the geometry spec correctly.
   * More trouble than it should be, and probably more trouble than
   * it has to be, but who can tell these days, what with all those
   * Widget-usin' Weenies out there...
   *
   * Note:  eWIDE,eHIGH have the correct, desired window size.  Ignore the
   *        w,h fields in the geometry spec, as they've already been dealt with
   */

  x = y = w = h = 1;
  i = XParseGeometry(geom,&x,&y,&w,&h);

  hints.flags = 0;
  if ((i&XValue || i&YValue)) hints.flags = USPosition;

  if (i&XValue && i&XNegative) x = vrWIDE - eWIDE - abs(x);
  if (i&YValue && i&YNegative) y = vrHIGH - eHIGH - abs(y);

  if (x+eWIDE > vrWIDE) x = vrWIDE - eWIDE;   /* keep on screen */
  if (y+eHIGH > vrHIGH) y = vrHIGH - eHIGH;


#define VROOT_TRANS
#ifdef VROOT_TRANS
  if (vrootW != rootW) { /* virtual window manager running */
    int x1,y1;
    Window child;
    XTranslateCoordinates(theDisp, rootW, vrootW, x, y, &x1, &y1, &child);
    if (DEBUG) fprintf(stderr,"translate:  %d,%d -> %d,%d\n",x,y,x1,y1);
    x = x1;  y = y1;
  }
#endif

  hints.x = x;                  hints.y = y;
  hints.width = eWIDE;          hints.height = eHIGH;
  hints.max_width  = maxWIDE;   hints.max_height = maxHIGH;
  hints.flags |= USSize | PMaxSize;
    
  xswa.bit_gravity = StaticGravity;
  xswa.background_pixel = bg;
  xswa.border_pixel     = fg;
  xswa.colormap         = theCmap;
  
  xswa.backing_store    = WhenMapped;

  /* NOTE: I've turned 'backing-store' off on the image window, as some
     servers (HP 9000/300 series running X11R4) don't properly deal with
     things when the image window changes size.  It isn't a big performance
     improvement anyway (for the image window), unless you're on a slow
     network.  In any event, I'm not *turning off* backing store, I'm
     just not explicitly turning it *on*.  If your X server is set up
     that windows, by default, have backing-store turned on, then the 
     image window will, too */
  
  xswamask = CWBackPixel | CWBorderPixel | CWColormap /* | CWBackingStore */;
  if (!clearonload) xswamask |= CWBitGravity;

  if (mainW) {
    GetWindowPos(&xwa);
    xwa.width = eWIDE;  xwa.height = eHIGH;

    /* try to keep the damned thing on-screen, if possible */
    if (xwa.x + xwa.width  > dispWIDE) xwa.x = dispWIDE - xwa.width;
    if (xwa.y + xwa.height > dispHIGH) xwa.y = dispHIGH - xwa.height;
    if (xwa.x < 0) xwa.x = 0;
    if (xwa.y < 0) xwa.y = 0;

    SetWindowPos(&xwa);
    hints.flags = PSize | PMaxSize;
  } 

  else {
    mainW = XCreateWindow(theDisp,rootW,x,y, (u_int) eWIDE, (u_int) eHIGH,
			  (u_int) bwidth, (int) dispDEEP, InputOutput, 
			  theVisual, xswamask, &xswa);
    if (!mainW) FatalError("can't create window!");

    XSetCommand(theDisp, mainW, mainargv, mainargc);

    if (LocalCmap) {
      xswa.colormap = LocalCmap;
      XChangeWindowAttributes(theDisp,mainW,CWColormap,&xswa);
    }
  }


  XSetStandardProperties(theDisp,mainW,"","",None,NULL,0,&hints);
  setWinIconNames(name);

  xwmh.input = True;
  xwmh.flags = InputHint;

  xwmh.icon_pixmap = iconPix;  
  xwmh.icon_mask   = iconmask;  
  xwmh.flags |= (IconPixmapHint | IconMaskHint);


  if (startIconic && firstTime) {
    xwmh.initial_state = IconicState;
    xwmh.flags |= StateHint;

    if (icongeom) {
      int i,x,y;  unsigned int w,h;
      i = XParseGeometry(icongeom, &x, &y, &w, &h);   /* ignore w,h */
      if (i&XValue && i&YValue) {
	if (i&XValue && i&XNegative) x = vrWIDE - icon_width - abs(x);
	if (i&YValue && i&YNegative) y = vrHIGH - icon_height - abs(y);

	xwmh.icon_x = x;  xwmh.icon_y = y;
	xwmh.flags |= (IconPositionHint);
      }
    }
  }
  XSetWMHints(theDisp, mainW, &xwmh);

  classh.res_name = "xv";
  classh.res_class = "XVroot";
  XSetClassHint(theDisp, mainW, &classh);


  if (nodecor) {   /* turn of image window decorations (in MWM) */ 
    Atom mwm_wm_hints;
    struct s_mwmhints {
      long   flags;
      long   functions;
      long   decorations;
      u_long input_mode;
      long   status;
    } mwmhints;
    
    mwm_wm_hints = XInternAtom(theDisp, "_MOTIF_WM_HINTS", False);
    if (mwm_wm_hints != None) {
      xvbzero((char *) &mwmhints, sizeof(mwmhints));
      mwmhints.flags = 2;
      mwmhints.decorations = 4;

      XChangeProperty(theDisp, mainW, mwm_wm_hints, mwm_wm_hints, 32,
		      PropModeReplace, (byte *) &mwmhints, 
		      (int) (sizeof(mwmhints))/4); 
      XSync(theDisp, False);
    }
  }

    
  firstTime = 0;
}


/***********************************/
static void setWinIconNames(name)
     char *name;
{
  char winname[256], iconname[256];

  if (winTitle) {
    strcpy(winname, winTitle);
    strcpy(iconname, winTitle);
  }
  else if (name[0] == '\0') {
    sprintf(winname, "xv %s",VERSTR);
    sprintf(iconname,"xv");
  }
  else {
    sprintf(winname,"xv %s: %s", VERSTR, name);
    sprintf(iconname,"%s",name);
  }

#ifndef REGSTR
  strcat(winname, " <unregistered>");
#endif

  if (mainW) {
    XStoreName(theDisp, mainW, winname);
    XSetIconName(theDisp, mainW, iconname);
  }
}


/***********************************/
void FixAspect(grow,w,h)
int   grow;
int   *w, *h;
{
  /* computes new values of eWIDE and eHIGH which will have aspect ratio
     'normaspect'.  If 'grow' it will preserve aspect by enlarging, 
     otherwise, it will shrink to preserve aspect ratio.  
     Returns these values in 'w' and 'h' */

  float xr,yr,curaspect,a,exp;

  *w = eWIDE;  *h = eHIGH;

  /* xr,yr are expansion factors */
  xr = ((float) eWIDE) / cWIDE;
  yr = ((float) eHIGH) / cHIGH;
  curaspect  = xr / yr;

  /* if too narrow & shrink, shrink height.  too wide and grow, grow height */
  if ((curaspect < normaspect && !grow) || 
      (curaspect > normaspect &&  grow)) {    /* modify height */
    exp = curaspect / normaspect;
    *h = (int) (eHIGH * exp + .5);
  }

  /* if too narrow & grow, grow width.  too wide and shrink, shrink width */
  if ((curaspect < normaspect &&  grow) || 
      (curaspect > normaspect && !grow)) {    /* modify width */
    exp = normaspect / curaspect;
    *w = (int) (eWIDE * exp + .5);
  }


  /* shrink to fit screen without changing aspect ratio */
  if (*w>maxWIDE) {
    int i;
    a = (float) *w / maxWIDE;
    *w = maxWIDE;
    i = (int) (*h / a + .5);        /* avoid freaking some optimizers */
    *h = i;
  }

  if (*h>maxHIGH) {
    a = (float) *h / maxHIGH;
    *h = maxHIGH;
    *w = (int) (*w / a + .5);
  }

  if (*w < 1) *w = 1;
  if (*h < 1) *h = 1;
}


/***********************************/
static void makeDispNames()
{
  int   prelen, n, i, done;
  char *suffix;

  suffix = namelist[0];
  prelen = 0;   /* length of prefix to be removed */
  n = i = 0;    /* shut up pesky compiler warnings */
  
  done = 0;
  while (!done) {
    suffix = (char *) index(suffix,'/');    /* find next '/' in file name */
    if (!suffix) break;
    
    suffix++;                       /* go past it */
    n = suffix - namelist[0];
    for (i=1; i<numnames; i++) {
      if (strncmp(namelist[0], namelist[i], (size_t) n)!=0) { done=1; break; }
    }
    
    if (!done) prelen = n;
  }
  
  for (i=0; i<numnames; i++) 
    dispnames[i] = namelist[i] + prelen;
}


/***********************************/
static void fixDispNames()
{
  /* fix dispnames array so that names don't go off right edge */
  
  int   i,j;
  char *tmp;
  
  for (i=j=0; i<numnames; i++) {
    char *dname;
    
    dname = dispnames[i];
    if (StringWidth(dname) > (nList.w-10-16)) {  /* have to trunc. */
      tmp = dname;
      while (1) {
	tmp = (char *) index(tmp,'/'); /* find next '/' in filename */
	if (!tmp) { tmp = dname;  break; }
	
	tmp++;                   /* move to char following the '/' */
	if (StringWidth(tmp) <= (nList.w-10-16)) { /* is cool now */
	  j++;  break;
	}
      }
      dispnames[i] = tmp;
    }
  }
}


/***********************************/
void StickInCtrlList(select)
     int select;
{
  /* stick current name (from 'load/save' box) and current working directory
     into 'namelist'.  Does redraw list.  */

  char *name;
  char  cwd[MAXPATHLEN];

  name = GetDirFName();
  GetDirPath(cwd);
  
  AddFNameToCtrlList(cwd, name);
  
  if (select) {
    nList.selected = numnames-1;
    curname = numnames - 1;
  }

  ChangedCtrlList();
}


/***********************************/
void AddFNameToCtrlList(fpath,fname)
     char *fpath, *fname;
{
  /* stick given path/name into 'namelist'.  Doesn't redraw list */
  
  char *fullname, *dname;
  char cwd[MAXPATHLEN], globnm[MAXPATHLEN+100];
  int i;
  
  if (!fpath) fpath = "";  /* bulletproofing... */
  if (!fname) fname = "";  
  
  if (numnames == MAXNAMES) return;  /* full up */
  
  /* handle globbing */
  if (fname[0] == '~') {
    strcpy(globnm, fname);
    Globify(globnm);
    fname = globnm;
  }
  
  if (fname[0] != '/') {  /* prepend path */
    strcpy(cwd, fpath);   /* copy it to a modifiable place */
    
    /* make sure fpath has a trailing '/' char */
    if (strlen(cwd)==0 || cwd[strlen(cwd)-1]!='/') strcat(cwd, "/");
    
    fullname = (char *) malloc(strlen(cwd) + strlen(fname) + 2);
    if (!fullname) FatalError("couldn't alloc name in AddFNameToCtrlList()\n");
    
    sprintf(fullname, "%s%s", cwd, fname);
  }
  else {                 /* copy name to fullname */
    fullname = (char *) malloc(strlen(fname) + 1);
    if (!fullname) FatalError("couldn't alloc name in AddFNameToCtrlList()\n");
    strcpy(fullname, fname);
  }
  
  
  /* see if this name is a duplicate.  Don't add it if it is. */
  for (i=0; i<numnames; i++)
    if (strcmp(fullname, namelist[i]) == 0) {
      free(fullname);
      return;
    }
  
  namelist[numnames] = fullname;
  numnames++;
  makeDispNames();
  fixDispNames();
}


/***********************************/
void ChangedCtrlList()
{
  /* called when the namelist/dispnames arrays have changed, and list needs
     to be re-displayed */

  int cname, lsel;

  if (numnames>0) BTSetActive(&but[BDELETE],1);
  windowMB.dim[WMB_TEXTVIEW] = (numnames==0);

  cname = curname;  lsel = nList.selected;  /* get blown away in LSNewData */
  LSChangeData(&nList, dispnames, numnames);
  curname = cname;  nList.selected = lsel;  /* restore prev values */

  ActivePrevNext();

  ScrollToCurrent(&nList);
  DrawCtrlNumFiles();
}


/***********************************/
void ActivePrevNext()
{
  /* called to enable/disable the Prev/Next buttons whenever curname and/or
     numnames and/or nList.selected change */

  /* if curname<0 (there is no 'current' file), 'Next' means view the 
     selected file (or the 0th file, if no selection either), and 'Prev' means
     view the one right before the selected file */

  if (curname<0) {  /* light things based on nList.selected, instead */
    BTSetActive(&but[BNEXT], (numnames>0));
    BTSetActive(&but[BPREV], (nList.selected>0));
  }
  else {
    BTSetActive(&but[BNEXT], (curname<numnames-1));
    BTSetActive(&but[BPREV], (curname>0));
  }
}
  

/***********************************/
int DeleteCmd()
{
  /* 'delete' button was pressed.  Pop up a dialog box to determine
     what should be deleted, then do it.
     returns '1' if THE CURRENTLY VIEWED entry was deleted from the list, 
     in which case the 'selected' filename on the ctrl list is now 
     different, and should be auto-loaded, or something */

  static char *bnames[] = { "\004Disk File", "\nList Entry", "\033Cancel" };
  char str[512];
  int  del, i, delnum, rv;

  /* failsafe */
  delnum = nList.selected;
  if (delnum < 0 || delnum >= numnames) return 0;

  sprintf(str,"Delete '%s'?\n\n%s%s",
	  namelist[delnum],
	  "'List Entry' deletes selection from list.\n",
	  "'Disk File' deletes file associated with selection.");

  del = PopUp(str, bnames, 3);
  
  if (del == 2) return 0;   /* cancel */
  
  if (del == 0) {           /* 'Disk File' */
    char *name;
    if (namelist[delnum][0] != '/') {    /* prepend 'initdir' */
      name = (char *) malloc(strlen(namelist[delnum]) + strlen(initdir) + 2);
      if (!name) FatalError("malloc in DeleteCmd failed\n");
      sprintf(name,"%s/%s", initdir, namelist[delnum]);
    }
    else name = namelist[delnum];

    i = unlink(name);
    if (i) {
      sprintf(str,"Can't delete file '%s'\n\n  %s.", name, ERRSTR(errno));
      ErrPopUp(str, "\nPity");
      if (name != namelist[delnum]) free(name);
      return 0;
    }

    XVDeletedFile(name);
    if (name != namelist[delnum]) free(name);
  }

  deleteFromList(delnum);

  rv = 0;
  if (delnum == curname) {      /* deleted the viewed file */
    curname = nList.selected;
    rv = 1;                     /* auto-load currently 'selected' filename */
  }
  else if (delnum < curname) curname = (curname > 0) ? curname-1 : 0;

  return rv;
}


/********************************************/
static void deleteFromList(delnum)
     int delnum;
{
  int i;

  /* remove from list on either 'List Entry' or (successful) 'Disk File' */

  /* determine if namelist[delnum] needs to be freed or not */
  for (i=0; i<mainargc && mainargv[i] != namelist[delnum]; i++) ;
  if (i == mainargc) {  /* not found.  free it */
    free(namelist[delnum]);
  }

  if (delnum != numnames-1) {
    /* snip out of namelist and dispnames lists */
    xvbcopy((char *) &namelist[delnum+1], (char *) &namelist[delnum], 
	  (numnames - delnum - 1) * sizeof(namelist[0]));

    xvbcopy((char *) &dispnames[delnum+1], (char *) &dispnames[delnum], 
	  (numnames - delnum - 1) * sizeof(dispnames[0]));
  }
  
  numnames--;
  if (numnames==0) BTSetActive(&but[BDELETE],0);
  windowMB.dim[WMB_TEXTVIEW] = (numnames==0);

  nList.nstr = numnames;
  nList.selected = delnum;

  if (nList.selected >= numnames) nList.selected = numnames-1;
  if (nList.selected < 0) nList.selected = 0;

  SCSetRange(&nList.scrl, 0, numnames - nList.nlines, 
	     nList.scrl.val, nList.nlines-1);
  ScrollToCurrent(&nList);
  DrawCtrlNumFiles();

  ActivePrevNext();
}


/***********************************/
void HandleDispMode()
{
  /* handles a change in the display mode (windowed/root).
     Also, called to do the 'right' thing when opening a picture
     displays epic, in current size, UNLESS we've selected an 'integer'
     root tiling thingy, in which case we resize epic appropriately */

  static int haveoldinfo = 0;
  static Window            oldMainW;
  static int               oldCmapMode;
  static XSizeHints        oldHints;
  static XWindowAttributes oldXwa;
  int i;


  WaitCursor();

  /****************************************************************/
  /*** RMB_WINDOW windowed mode                                   */
  /****************************************************************/


  if (dispMode == RMB_WINDOW) {        /* windowed */
    char fnam[256];

    BTSetActive(&but[BANNOT], 1);

    if (fullfname[0] == '\0') fnam[0] = '\0';
    else {
      char *tmp;
      int   i, state;

      /* find beginning of next-to-last pathname component, ie,
	 if fullfname is "/foo/bar/snausage", we'd like "bar/snausage" */

      state = 0;
      for (i=strlen(fullfname); i>0 && state!=2; i--) {
	if (fullfname[i] == '/') state++;
      }

      if (state==2) tmp = fullfname + i + 2;
      else tmp = fullfname;

      strcpy(fnam, tmp);

      /* if we're viewing a multi-page doc, add page # to title */
      if (strlen(pageBaseName) && numPages>1) {
	char foo[64];
	sprintf(foo, "  Page %d of %d", curPage+1, numPages);
	strcat(fnam, foo);
      }

    }

    if (useroot && resetroot) ClearRoot();

    if (mainW == (Window) NULL || useroot) {  /* window not visible */
      useroot = 0;  

      if (haveoldinfo) {             /* just remap mainW and resize it */
	XWMHints xwmh;

	mainW = oldMainW;

	/* enable 'perfect' and 'owncmap' options */
	dispMB.dim[DMB_COLPERF] = (picType == PIC8) ? 0 : 1;
	dispMB.dim[DMB_COLOWNC] = (picType == PIC8) ? 0 : 1;

	XSetStandardProperties(theDisp,mainW,"","",None,NULL,0,&oldHints);
	setWinIconNames(fnam);

	xwmh.initial_state = NormalState;
	xwmh.input = True;
	xwmh.flags = InputHint;

	xwmh.icon_pixmap = iconPix;  
	xwmh.icon_mask   = iconmask;  
	xwmh.flags |= ( IconPixmapHint | IconMaskHint) ;

	xwmh.flags |= StateHint;
	XSetWMHints(theDisp, mainW, &xwmh);

	oldXwa.width = eWIDE;  oldXwa.height = eHIGH;
	SetWindowPos(&oldXwa);
	XResizeWindow(theDisp, mainW, (u_int) eWIDE, (u_int) eHIGH);
	XMapWindow(theDisp, mainW);
      }

      else {                         /* first time.  need to create mainW */
	mainW = (Window) NULL;
	createMainWindow(maingeom, fnam);
	XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask 
		     | StructureNotifyMask | ButtonPressMask
		     | KeyReleaseMask | ColormapChangeMask
		     | EnterWindowMask | LeaveWindowMask );

	StoreDeleteWindowProp(mainW);
	XFlush(theDisp);
	XMapWindow(theDisp,mainW);
	XFlush(theDisp);
	if (startIconic) sleep(2);   /* give it time to get the window up...*/
      }
    }

    else {                            /* mainW already visible */
      createMainWindow(maingeom, fnam);
      XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask 
		   | StructureNotifyMask | ButtonPressMask
		   | KeyReleaseMask | ColormapChangeMask
		   | EnterWindowMask | LeaveWindowMask );

      if (LocalCmap) {                /* AllocColors created local colormap */
	XSetWindowColormap(theDisp, mainW, LocalCmap);
      }
    }



    useroot = 0;
  }


  /****************************************************************/
  /*** ROOT mode                                                  */
  /****************************************************************/


  else if (dispMode > RMB_WINDOW && dispMode < RMB_MAX) {
    int ew, eh, regen;

    BTSetActive(&but[BANNOT], 0);

    regen = 0;
    if (!useroot) {                  /* have to hide mainW, etc. */
      dispMB.dim[DMB_COLPERF] = 1;   /* no perfect or owncmap in root mode */
      dispMB.dim[DMB_COLOWNC] = 1;

      /* save current window stuff */
      haveoldinfo = 1;
      oldMainW = mainW;
      oldCmapMode = colorMapMode;

      GetWindowPos(&oldXwa);
      if (!XGetNormalHints(theDisp, mainW, &oldHints)) oldHints.flags = 0;
      oldHints.x=oldXwa.x;  oldHints.y=oldXwa.y;  oldHints.flags|=USPosition;

      if (LocalCmap) regen=1;

      /* this reallocs the colors */
      if (colorMapMode==CM_PERFECT || colorMapMode==CM_OWNCMAP) 
	ChangeCmapMode(CM_NORMAL, 0, 0);
      
      
      XUnmapWindow(theDisp, mainW);
      mainW = vrootW;
      
      if (!ctrlUp) {    /* make sure ctrl is up when going to 'root' mode */
	XWMHints xwmh;
	xwmh.input         = True;
	xwmh.initial_state = IconicState;
	xwmh.flags         = (InputHint | StateHint);
	XSetWMHints(theDisp, ctrlW, &xwmh);
	CtrlBox(1);
      }
    }
      
    useroot = 1;
    rootMode = dispMode - RMB_ROOT;
    ew = eWIDE;  eh = eHIGH;

    RANGE(ew,1,maxWIDE);  RANGE(eh,1,maxHIGH);

    if (rootMode == RM_TILE || rootMode == RM_IMIRROR) {
      i = (dispWIDE + ew-1) / ew;   ew = (dispWIDE + i-1) / i;
      i = (dispHIGH + eh-1) / eh;   eh = (dispHIGH + i-1) / i;
    }

    if (ew != eWIDE || eh != eHIGH) {  /* changed size... */
      GenerateEpic(ew, eh);
      CreateXImage();
    }
    else if (regen) CreateXImage();                    

    KillOldRootInfo();
    MakeRootPic();
    SetCursors(-1);
  }

  else {
    fprintf(stderr,"unknown dispMB value '%d' in HandleDispMode()\n",
	    dispMode);
  }

  SetCursors(-1);
}


/*******************************************************/
static void add_filelist_to_namelist(flist, nlist, numn, maxn)
     char  *flist;
     char **nlist;
     int   *numn, maxn;
{
  /* written by Brian Gregory  (bgregory@megatest.com) */

  FILE *fp;

  fp = fopen(flist,"r");
  if (!fp) {
    fprintf(stderr,"Can't open filelist '%s': %s\n", flist, ERRSTR(errno));
    return;
  }

  while (*numn < maxn) {
    char *s, *nlp, fbuf[MAXPATHLEN];
    if (!fgets(fbuf, MAXPATHLEN, fp) ||
	!(s = (char *) malloc(strlen(fbuf)))) break;

    nlp = (char *) rindex(fbuf, '\n');
    if (nlp) *nlp = '\0';
    strcpy(s, fbuf);

    namelist[*numn] = s;  (*numn)++;
  }


  if (*numn == maxn) {
    fprintf(stderr, "%s: too many filenames.  Only using first %d.\n",
	    flist, maxn);
  }

  fclose(fp);
}




/************************************************************************/

/***********************************/
char *lower_str(str)
     char *str;
{
  char *p;
  for (p=str; *p; p++) if (isupper(*p)) *p = tolower(*p);
  return str;
}


/***********************************/
int rd_int(name)
     char *name;
{
  /* returns '1' if successful.  result in def_int */

  if (rd_str_cl(name, "", 0)) {     /* sets def_str */
    if (sscanf(def_str, "%d", &def_int) == 1) return 1;
    else {
      fprintf(stderr, "%s: couldn't read integer value for %s resource\n", 
	      cmd, name);
      return 0;
    }
  }
  else return 0;
}


/***********************************/
int rd_str(name)
     char *name;
{
  return rd_str_cl(name, "", 0);
}


/***********************************/
int rd_flag(name)
char *name;
{
  /* returns '1' if successful.  result in def_int */
  
  char buf[256];

  if (rd_str_cl(name, "", 0)) {  /* sets def_str */
    strcpy(buf, def_str);
    lower_str(buf);

    def_int = (strcmp(buf, "on")==0) || 
              (strcmp(buf, "1")==0) ||
	      (strcmp(buf, "true")==0) ||
	      (strcmp(buf, "yes")==0);
    return 1;
    }

  else return 0;
}
    



static int xrm_initted = 0;
 
/***********************************/
int rd_str_cl (name_str, class_str, reinit)
     char *name_str;
     char *class_str;
     int  reinit;
{
  /* note: *all* X resource reading goes through this routine... */

  /* returns '1' if successful, result in def_str */

  char     q_name[BUFSIZ], q_class[BUFSIZ];
  char    *type;
  XrmValue result;
  int      gotit;
  static XrmDatabase def_resource;


  if (reinit) {
#ifndef vax11c
    if (xrm_initted && def_resource) XrmDestroyDatabase(def_resource);
#endif
    xrm_initted = 0;
  }

  if (!xrm_initted) {
    Atom  resAtom;
    char *xrm_str;

    XrmInitialize();
    xrm_initted = 1;
    def_resource = (XrmDatabase) 0;

    gotit = 0;


    /* don't use XResourceManagerString, as it is a snapshot of the
       string when theDisp was opened, and doesn't change */

    resAtom = XInternAtom(theDisp, "RESOURCE_MANAGER", True);
    if (resAtom != None) {
      Atom actType;
      int  i, actFormat;
      unsigned long nitems, nleft;
      byte *data;

      i = XGetWindowProperty(theDisp, RootWindow(theDisp, 0),
			     resAtom, 0L, 1L, False, 
			     XA_STRING, &actType, &actFormat, &nitems, &nleft, 
			     (unsigned char **) &data);
      if (i==Success && actType==XA_STRING && actFormat==8) {
	if (nitems>0 && data) XFree(data);
	i = XGetWindowProperty(theDisp, RootWindow(theDisp, 0), resAtom, 0L, 
			       (long) ((nleft+4+3)/4),
			       False, XA_STRING, &actType, &actFormat, 
			       &nitems, &nleft, (unsigned char **) &data);
	if (i==Success && actType==XA_STRING && actFormat==8 && data) {
	  def_resource = XrmGetStringDatabase((char *) data);
	  XFree(data);
	  gotit = 1;
	}
      }
    }



    if (!gotit) {
      xrm_str = XResourceManagerString(theDisp); 
      
      if (xrm_str) {
	def_resource = XrmGetStringDatabase(xrm_str);
	if (DEBUG) fprintf(stderr,"rd_str_cl: Using RESOURCE_MANAGER prop.\n");
      }
      else {    /* no RESOURCE_MANAGER prop.  read from 'likely' file */
	char foo[256], *homedir, *xenviron;
	XrmDatabase res1;
	
#ifdef VMS
	strcpy(foo, "SYS$LOGIN:DECW$XDEFAULTS.DAT");
#else
	homedir = (char *) getenv("HOME");
	if (!homedir) homedir = ".";
	sprintf(foo,"%s/.Xdefaults", homedir);
#endif
	
	def_resource = XrmGetFileDatabase(foo);
	
	if (DEBUG) {
	  fprintf(stderr,"rd_str_cl: No RESOURCE_MANAGER prop.\n");
	  fprintf(stderr,"rd_str_cl: Using file '%s' (%s)  ",
		  foo, (def_resource) ? "success" : "failure");
	}
	
	
	/* merge file pointed to by XENVIRONMENT */
	xenviron = (char *) getenv("XENVIRONMENT");
	if (xenviron) {
	  res1 = XrmGetFileDatabase(xenviron);
	  
	  if (DEBUG) {
	    fprintf(stderr,"merging XENVIRONMENT='%s' (%s)  ",
		    xenviron, (res1) ? "success" : "failure");
	  }
	  
	  if (res1) {    /* merge databases */
	    if (!def_resource) def_resource = res1;
	    else XrmMergeDatabases(res1, &def_resource);
	  }
	}
	
	
	if (DEBUG) fprintf(stderr,"\n\n");
      }
    }
  }


  if (!def_resource) return 0;     /* no resource database to search! */


  strcpy (q_name, PROGNAME);
  strcat (q_name, ".");
  strcat (q_name, name_str);
  
  strcpy (q_class, "Program");
  strcat (q_class, ".");
  strcat (q_class, class_str);

  (void) XrmGetResource(def_resource, q_name, q_class, &type, &result);
  
  def_str = result.addr;
  if (def_str) return (1);
  else return (0);
}


