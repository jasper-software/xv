/*
 *  xvbrowse.c  -  visual schnauzer routines
 *
 *  includes:
 *      void CreateBrowse(char *, char *, char *, char *, char *);
 *      void OpenBrowse();
 *      void HideBrowseWindows();
 *      void UnHideBrowseWindows();
 *      void SetBrowseCursor(Cursor);
 *      void KillBrowseWindows();
 *      int  BrowseCheckEvent(evt, int *retval, int *done);
 *      int  BrowseDelWin(Window);
 *      void SetBrowStr(char *);
 *      void RegenBrowseIcons();
 *
 */

#include "copyright.h"

#define NEEDSDIR
#include "xv.h"
#include <unistd.h>   /* access() */

#if defined(VMS) || defined(isc)
typedef unsigned int mode_t;  /* file mode bits */
#endif

#ifndef MAX
#  define MAX(a,b) (((a)>(b))?(a):(b))   /* used only for wheelmouse support */
#endif


/* load up built-in icons */
#include "bits/br_file"
#include "bits/br_dir"
#include "bits/br_exe"
#include "bits/br_chr"
#include "bits/br_blk"
#include "bits/br_sock"
#include "bits/br_fifo"
#include "bits/br_error"
/* #include "bits/br_unknown"	commented out (near line 492) */

#include "bits/br_cmpres"
#include "bits/br_bzip2"

#include "bits/br_bmp"
#include "bits/br_fits"
#include "bits/br_gif"
#include "bits/br_iff"
#include "bits/br_iris"
#include "bits/br_jfif"
#include "bits/br_jp2"
#include "bits/br_jpc"
#include "bits/br_mag"
#include "bits/br_maki"
#include "bits/br_mgcsfx"
#include "bits/br_pbm"
#include "bits/br_pcd"
#include "bits/br_pcx"
#include "bits/br_pds"
#include "bits/br_pi"
#include "bits/br_pic"
#include "bits/br_pic2"
#include "bits/br_pm"
#include "bits/br_png"
#include "bits/br_ps"
#include "bits/br_sunras"
#include "bits/br_targa"
#include "bits/br_tiff"
#include "bits/br_utah"
#include "bits/br_xbm"
#include "bits/br_xpm"
#include "bits/br_xwd"
#include "bits/br_zx"	/* [JCE] The Spectrum+3 icon */

#include "bits/br_trash"
#include "bits/fcurs"
#include "bits/fccurs"
#include "bits/fdcurs"
#include "bits/fcursm"



/* FileTypes */
#define BF_HAVEIMG  -1   /* should use 'pimage' and 'ximage' fields */
#define BF_FILE      0
#define BF_DIR       1
#define BF_EXE       2
#define BF_CHR       3
#define BF_BLK       4
#define BF_SOCK      5
#define BF_FIFO      6
#define BF_ERROR     7
#define BF_UNKNOWN   8
#define BF_GIF       9
#define BF_PM       10
#define BF_PBM      11
#define BF_XBM      12
#define BF_SUNRAS   13
#define BF_BMP      14
#define BF_UTAHRLE  15
#define BF_IRIS     16
#define BF_PCX      17
#define BF_JFIF     18
#define BF_TIFF     19
#define BF_PDS      20
#define BF_COMPRESS 21
#define BF_PS       22
#define BF_IFF      23
#define BF_TGA      24
#define BF_XPM      25
#define BF_XWD      26
#define BF_FITS     27
#define BF_PNG      28
#define BF_ZX       29    /* [JCE] Spectrum SCREEN$ */
#define BF_PCD      30
#define BF_BZIP2    31
#define BF_JP2      32
#define BF_JPC      33
#define JP_EXT_BF   (BF_JPC)
#define BF_MAG      (JP_EXT_BF + 1)
#define BF_MAKI     (JP_EXT_BF + 2)
#define BF_PIC      (JP_EXT_BF + 3)
#define BF_PI       (JP_EXT_BF + 4)
#define BF_PIC2     (JP_EXT_BF + 5)
#define BF_MGCSFX   (JP_EXT_BF + 6)
#define JP_EXT_BF_END  (BF_MGCSFX)
#define BF_MAX      (JP_EXT_BF_END + 1)    /* # of built-in icons */

#define ISLOADABLE(ftyp) (ftyp!=BF_DIR  && ftyp!=BF_CHR && ftyp!=BF_BLK && \
			  ftyp!=BF_SOCK && ftyp!=BF_FIFO)

#define SCROLLVERT  8      /* height of scroll region at top/bottom of iconw */
#define PAGEVERT    40     /* during rect drag, if further than this, page */

#define ICON_ONLY 2        /* if 'lit' field ==, invert icon only, not title */
#define TEMP_LIT  3        /* temporarily selected, normally off */
#define TEMP_LIT1 4        /* temporarily selected, normally on */

#define TOPMARGIN 30       /* from top of window to top of iconwindow */
#define BOTMARGIN 58       /* room for a row of buttons and a line of text */
#define LRMARGINS 5        /* left and right margins */

/* some people like bigger icons; 4:3 aspect ratio is recommended
 * (NOTE:  standard XV binaries will not be able to read larger icons!) */
#ifndef ISIZE_WIDE
#  define ISIZE_WIDE 80    /* maximum size of an icon */
#endif
#ifndef ISIZE_HIGH
#  define ISIZE_HIGH 60
#endif

#ifndef ISIZE_WPAD
#  define ISIZE_WPAD 16    /* extra horizontal padding between icons */
#endif

#ifndef INUM_WIDE
#  define INUM_WIDE 6      /* size initial window to hold this many icons */
#endif
#ifndef INUM_HIGH
#  define INUM_HIGH 3
#endif

#define ISPACE_WIDE (ISIZE_WIDE+ISIZE_WPAD)   /* icon spacing */
#define ISPACE_TOP  4                 /* dist btwn top of ISPACE and ISIZE */
#define ISPACE_TTOP 4                 /* dist btwn bot of icon and title */
#define ISPACE_HIGH (ISIZE_HIGH+ISPACE_TOP+ISPACE_TTOP+16+4)

#define DBLCLICKTIME 300  /* milliseconds */

#define COUNT(x) (sizeof (x) / sizeof (x)[0])

/* button/menu indices */
#define BR_CHDIR      0
#define BR_DELETE     1
#define BR_MKDIR      2
#define BR_RENAME     3
#define BR_RESCAN     4
#define BR_UPDATE     5
#define BR_NEWWIN     6
#define BR_GENICON    7
#define BR_SELALL     8
#define BR_TEXTVIEW   9
#define BR_RECURSUP   10
#define BR_QUIT       11
#define BR_CLOSE      12
#define BR_NBUTTS     13   /* # of command buttons */
#define BR_SEP1       13   /* separator */
#define BR_HIDDEN     14
#define BR_SELFILES   15
#define BR_CLIPBRD    16
#ifdef AUTO_EXPAND
#  define BR_CLEARVD  17
#  define BR_NCMDS    18   /* # of menu commands */
#else
#  define BR_NCMDS    17   /* # of menu commands */
#endif

#define BUTTW 80
#define BUTTH 24

/* original size of window was 615 x 356 (for 80x60 thumbnails in 6x3 array) */
#define DEF_BROWWIDE  (ISPACE_WIDE * INUM_WIDE + LRMARGINS * 2 + 29)
#define DEF_BROWHIGH  (ISPACE_HIGH * INUM_HIGH + BUTTH * 2 + 16 + 28)
/* last number is a fudge--e.g., extra spaces, borders, etc. -----^  */

static const char *showHstr = "Show hidden files";
static const char *hideHstr = "Hide 'hidden' files";

static const char *cmdMList[] = { "Change directory...\t^c",
				  "Delete file(s)\t^d",
				  "New directory...\t^n",
				  "Rename file...\t^r",
				  "Rescan directory\t^s",
				  "Update icons\t^u",
				  "Open new window\t^w",
				  "Generate icon(s)\t^g",
				  "Select all files\t^a",
				  "Text view\t^t",
				  "Recursive Update\t^e",
				  "Quit xv\t^q",
				  "Close window\t^c",
				  MBSEP,
				  "Show hidden files",     /* no equiv */
				  "Select files...\t^f",
				  "Clipboard\t^x"
#ifdef AUTO_EXPAND
				  , "Clear virtual directory"
#endif
				  };


#define MAXDEEP 30     /* maximum directory depth */

#define BOGUSPATH "NO SUCH PATH"


typedef struct { char   *name;    /* name of file */
		 char   *imginfo; /* info on the real image */
                 int     ftype;   /* BF_EXE, BF_DIR, BF_FILE, etc... */
		 byte   *pimage;  /* normal, 8-bit-per image */
		 XImage *ximage;  /* X version of pimage */
		 int     w,h;     /* size of icon */
		 int     lit;     /* true if 'selected' */
	       } BFIL;

/* data needed per schnauzer window */
typedef struct {  Window        win, iconW;
		  int           vis, wasvis;

		  int           wide, high;
		  int           iwWide, iwHigh;
		  int           numWide, numHigh, visHigh;

		  SCRL          scrl;
		  BUTT          but[BR_NBUTTS];
		  MBUTT         dirMB, cmdMB;
		  char          dispstr[256];
		  int           numbutshown;
		  int           showhidden;

		  int           numlit;
		  BFIL         *bfList;
		  int           bfLen;
		  int           lastIconClicked;
		  unsigned long lastClickTime;

		  int           ndirs;
		  const char   *mblist[MAXDEEP];
		  char          path[MAXPATHLEN+2];   /* '/' terminated */

		  char         *str;
		  int           siz, len;
		  time_t        lst;
		} BROWINFO;


/* keep track of last icon visible in each path */
typedef struct IVIS IVIS;
    struct IVIS { IVIS   *next;
                  char   *name;
                  int    icon;
                };

static Cursor   movecurs, copycurs, delcurs;
static BROWINFO binfo[MAXBRWIN];
static Pixmap   bfIcons[BF_MAX], trashPix;
static int      hasBeenSized = 0;
static int      haveWindows  = 0;

static unsigned long browfg, browbg, browhi, browlo;

static void closeBrowse      PARM((BROWINFO *));
static int  brChkEvent       PARM((BROWINFO *, XEvent *));
static void resizeBrowse     PARM((BROWINFO *, int, int));
static void setBrowStr       PARM((BROWINFO *, const char *));
static void doCmd            PARM((BROWINFO *, int));
static void drawBrow         PARM((BROWINFO *));
static void drawNumfiles     PARM((BROWINFO *));
static void eraseNumfiles    PARM((BROWINFO *, int));
static void drawTrash        PARM((BROWINFO *));
static int  inTrash          PARM((BROWINFO *, int, int));
static void drawBrowStr      PARM((BROWINFO *));
static void changedNumLit    PARM((BROWINFO *, int, int));
static void setSelInfoStr    PARM((BROWINFO *, int));
static void exposeIconWin    PARM((BROWINFO *, int, int, int, int));
static void drawIconWin      PARM((int, SCRL *));
static void eraseIcon        PARM((BROWINFO *, int));
static void eraseIconTitle   PARM((BROWINFO *, int));
static void drawIcon         PARM((BROWINFO *, int));
static void makeIconVisible  PARM((BROWINFO *, int));
static void clickBrow        PARM((BROWINFO *, int, int));
static int  clickIconWin     PARM((BROWINFO *, int, int, unsigned long, int));
static void doubleClick      PARM((BROWINFO *, int));
static int  mouseInWhichIcon PARM((BROWINFO *, int, int));
static void invertSelRect    PARM((BROWINFO *, int, int, int, int));
static void keyIconWin       PARM((BROWINFO *, XKeyEvent *));
static void browKey          PARM((BROWINFO *, int));
static void browAlpha        PARM((BROWINFO *, int));
static void changedBrDirMB   PARM((BROWINFO *, int));
static int  cdBrow           PARM((BROWINFO *));
static void scanDir          PARM((BROWINFO *));
static void copyDirInfo      PARM((BROWINFO *, BROWINFO *));
static void endScan          PARM((BROWINFO *, int));
static void scanFile         PARM((BROWINFO *, BFIL *, char *));
static void sortBFList       PARM((BROWINFO *));
static int  bfnamCmp         PARM((const void *, const void *));
static void rescanDir        PARM((BROWINFO *));
static int  namcmp           PARM((const void *, const void *));
static void freeBfList       PARM((BROWINFO *br));
static char **getDirEntries  PARM((const char *, int *, int));
static void computeScrlVals  PARM((BROWINFO *, int *, int *));
static void genSelectedIcons PARM((BROWINFO *));
static void genIcon          PARM((BROWINFO *, BFIL *));
static void loadThumbFile    PARM((BROWINFO *, BFIL *));
static void writeThumbFile   PARM((BROWINFO *, BFIL *, byte *, int,
				      int, char *));

static void makeThumbDir     PARM((BROWINFO *));
static void updateIcons      PARM((BROWINFO *));

static void drawTemp         PARM((BROWINFO *, int, int));
static void clearTemp        PARM((BROWINFO *));

static void doTextCmd        PARM((BROWINFO *));
static void doRenameCmd      PARM((BROWINFO *));
static void doMkdirCmd       PARM((BROWINFO *));

static void doChdirCmd       PARM((BROWINFO *));
static void doDeleteCmd      PARM((BROWINFO *));
static void doSelFilesCmd    PARM((BROWINFO *));

static void doRecurseCmd     PARM((BROWINFO *));
static void recurseUpdate    PARM((BROWINFO *, const char *));

static void rm_file          PARM((BROWINFO *, char *));
static void rm_dir           PARM((BROWINFO *, char *));
static void rm_dir1          PARM((BROWINFO *));

static void dragFiles        PARM((BROWINFO *, BROWINFO *, char *, char *,
				   const char *, char **, int, int));
static int  moveFile         PARM((char *, char *));
static int  copyFile         PARM((char *, char *));
static void cp               PARM((void));
static void cp_dir           PARM((void));
static void cp_file          PARM((struct stat *, int));
static void cp_special       PARM((struct stat *, int));
static void cp_fifo          PARM((struct stat *, int));

#ifdef AUTO_EXPAND
static int  stat2bf          PARM((u_int, char *));
#else
static int  stat2bf          PARM((u_int));
#endif

static int  selmatch         PARM((char *, char *));
static int  selmatch1        PARM((char *, char *));
static void recIconVisible   PARM((char *, int));
static void restIconVisible  PARM((BROWINFO *));

static void clipChanges      PARM((BROWINFO *));



/***************************************************************/
void CreateBrowse(geom, fgstr, bgstr, histr, lostr)
     const char *geom;
     const char *fgstr, *bgstr, *histr, *lostr;
{
  int                   i;
  XSizeHints            hints;
  XSetWindowAttributes  xswa;
  BROWINFO             *br;
  XColor                ecdef, cursfg, cursbg;
  Pixmap                mcpix, ccpix, dcpix, fcmpix;
  int                   gx, gy, gw, gh, gset, gx1, gy1;
  unsigned int          uw, uh;
  char                  wgeom[64];

  if (!geom) geom = "";

  /* map color spec strings into browCmap, if we're in browPerfect mode */
  if (browPerfect && browCmap) {
    browfg = 0;  browbg = 255;     /* black text on white bg, by default */
    if (fgstr && XParseColor(theDisp, browCmap, fgstr, &ecdef)) {
      browfg = browcols[((ecdef.red   >>  8) & 0xe0) |
			((ecdef.green >> 11) & 0x1c) |
			((ecdef.blue  >> 14) & 0x03)];
    }

    if (bgstr && XParseColor(theDisp, browCmap, bgstr, &ecdef)) {
      browbg = browcols[((ecdef.red   >>  8) & 0xe0) |
			((ecdef.green >> 11) & 0x1c) |
			((ecdef.blue  >> 14) & 0x03)];
    }

    browhi = browbg;  browlo = browfg;
    if (histr && XParseColor(theDisp, browCmap, histr, &ecdef)) {
      browhi = browcols[((ecdef.red   >>  8) & 0xe0) |
			((ecdef.green >> 11) & 0x1c) |
			((ecdef.blue  >> 14) & 0x03)];
    }

    if (lostr && XParseColor(theDisp, browCmap, lostr, &ecdef)) {
      browlo = browcols[((ecdef.red   >>  8) & 0xe0) |
			((ecdef.green >> 11) & 0x1c) |
			((ecdef.blue  >> 14) & 0x03)];
    }
  }
  else {
    browfg = infofg;  browbg = infobg;  browhi = hicol;  browlo = locol;
  }



  gset = XParseGeometry(geom, &gx, &gy, &uw, &uh);
  gw = (int) uw;  gh = (int) uh;

  /* creates *all* schnauzer windows at once */

  for (i=0; i<MAXBRWIN; i++) binfo[i].win = (Window) NULL;

  for (i=0; i<MAXBRWIN; i++) {
    char wname[64];

    /* create a slightly offset geometry, so the windows stack nicely */
    if ((gset & XValue) && (gset & YValue)) {
      if (gset & XNegative) gx1 = gx - i * 20;
                       else gx1 = gx + i * 20;

      if (gset & YNegative) gy1 = gy - i * 20;
	               else gy1 = gy + i * 20;

      if ((gset & WidthValue) && (gset & HeightValue))
	sprintf(wgeom, "%dx%d%s%d%s%d", gw, gh,
		(gset & XNegative) ? "-" : "+", abs(gx1),
		(gset & YNegative) ? "-" : "+", abs(gy1));
      else
	sprintf(wgeom, "%s%d%s%d",
		(gset & XNegative) ? "-" : "+", abs(gx1),
		(gset & YNegative) ? "-" : "+", abs(gy1));
    }
    else strcpy(wgeom, geom);

    br = &binfo[i];

    if (i) sprintf(wname, "xv visual schnauzer (%d)", i);
      else sprintf(wname, "xv visual schnauzer");

    br->win = CreateWindow(wname, "XVschnauze", wgeom,
			   DEF_BROWWIDE, DEF_BROWHIGH, browfg, browbg, 1);
    if (!br->win) FatalError("can't create schnauzer window!");

    haveWindows = 1;
    br->vis = br->wasvis = 0;

    if (browPerfect && browCmap) {
      xswa.colormap = browCmap;
      XChangeWindowAttributes(theDisp, br->win, CWColormap, &xswa);
    }

    if (ctrlColor) XSetWindowBackground(theDisp, br->win, browlo);
              else XSetWindowBackgroundPixmap(theDisp, br->win, grayTile);

    /* note: everything is sized and positioned in ResizeBrowse() */

    br->iconW = XCreateSimpleWindow(theDisp, br->win, 1,1, 100,100,
				     1,browfg,browbg);
    if (!br->iconW) FatalError("can't create schnauzer icon window!");

    SCCreate(&(br->scrl), br->win, 0,0, 1,100, 0,0,0,0,
	     browfg, browbg, browhi, browlo, drawIconWin);


    if (XGetNormalHints(theDisp, br->win, &hints)) {
      hints.min_width  = 325 + 96;
       hints.min_height = 180;
      hints.flags |= PMinSize;
      XSetNormalHints(theDisp, br->win, &hints);
    }

#ifdef BACKING_STORE
    xswa.backing_store = WhenMapped;
    XChangeWindowAttributes(theDisp, br->iconW, CWBackingStore, &xswa);
#endif

    XSelectInput(theDisp, br->iconW, ExposureMask | ButtonPressMask);



    BTCreate(&(br->but[BR_CHDIR]), br->win, 0,0,BUTTW,BUTTH,
	     "Change Dir",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_DELETE]), br->win, 0,0,BUTTW,BUTTH,
	     "Delete",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_MKDIR]), br->win, 0,0,BUTTW,BUTTH,
	     "New Dir",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_RENAME]), br->win, 0,0,BUTTW,BUTTH,
	     "Rename",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_RESCAN]), br->win, 0,0,BUTTW,BUTTH,
	     "ReScan",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_UPDATE]), br->win, 0,0,BUTTW,BUTTH,
	     "Update",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_NEWWIN]), br->win, 0,0,BUTTW,BUTTH,
	     "Open Win",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_GENICON]),br->win, 0,0,BUTTW,BUTTH,
	     "GenIcon",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_SELALL]), br->win, 0,0,BUTTW,BUTTH,
	     "Select All",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_TEXTVIEW]), br->win, 0,0,BUTTW,BUTTH,
	     "Text view",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_RECURSUP]), br->win, 0,0,BUTTW,BUTTH,
	     "RecursUpd",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_QUIT]), br->win, 0,0,BUTTW,BUTTH,
	     "Quit xv",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_CLOSE]), br->win, 0,0,BUTTW,BUTTH,
	     "Close",browfg,browbg,browhi,browlo);
    BTCreate(&(br->but[BR_CLIPBRD]), br->win, 0,0,BUTTW,BUTTH,
	     "Clipboard",browfg,browbg,browhi,browlo);

    XMapSubwindows(theDisp, br->win);

    MBCreate(&(br->dirMB), br->win, 0,0,100,19, NULL,NULL,0,
	     browfg,browbg,browhi,browlo);

    MBCreate(&(br->cmdMB), br->win, 0,0,160,19, "Misc. Commands",
	     cmdMList, BR_NCMDS, browfg,browbg,browhi,browlo);

    br->showhidden   = 0;
    br->cmdMB.list[BR_HIDDEN]   = showHstr;

    br->numbutshown  = 0;
    br->numlit       = 0;
    br->bfList       = NULL;
    br->bfLen        = 0;
    br->dispstr[0]   = '\0';
    br->ndirs        = 0;
    sprintf(br->path, BOGUSPATH);

    br->lastIconClicked = -1;
    br->lastClickTime = 0;
  }


  /* create built-in icon pixmaps */
  bfIcons[BF_FILE]=MakePix1(br->win,br_file_bits,br_file_width,br_file_height);
  bfIcons[BF_DIR] =MakePix1(br->win,br_dir_bits, br_dir_width, br_dir_height);
  bfIcons[BF_EXE] =MakePix1(br->win,br_exe_bits, br_exe_width, br_exe_height);
  bfIcons[BF_CHR] =MakePix1(br->win,br_chr_bits, br_chr_width, br_chr_height);
  bfIcons[BF_BLK] =MakePix1(br->win,br_blk_bits, br_blk_width, br_blk_height);
  bfIcons[BF_SOCK]=MakePix1(br->win,br_sock_bits,br_sock_width,br_sock_height);
  bfIcons[BF_FIFO]=MakePix1(br->win,br_fifo_bits,br_fifo_width,br_fifo_height);

  bfIcons[BF_ERROR]   = MakePix1(br->win, br_error_bits,
			       br_error_width,     br_error_height);

/* bfIcons[BF_UNKNOWN] = MakePix1(br->win, br_unknown_bits,
                                br_unknown_width, br_unknown_height); */
  bfIcons[BF_UNKNOWN] = bfIcons[BF_FILE];

  bfIcons[BF_COMPRESS] = MakePix1(br->win, br_cmpres_bits,
				  br_cmpres_width, br_cmpres_height);
  bfIcons[BF_BZIP2]    = MakePix1(br->win, br_bzip2_bits,
				  br_bzip2_width, br_bzip2_height);

  bfIcons[BF_BMP] =MakePix1(br->win,br_bmp_bits, br_bmp_width, br_bmp_height);
  bfIcons[BF_FITS]=MakePix1(br->win,br_fits_bits,br_fits_width,br_fits_height);
  bfIcons[BF_GIF] =MakePix1(br->win,br_gif_bits, br_gif_width, br_gif_height);
  bfIcons[BF_IFF] =MakePix1(br->win,br_iff_bits, br_iff_width, br_iff_height);
  bfIcons[BF_IRIS]=MakePix1(br->win,br_iris_bits,br_iris_width,br_iris_height);
  bfIcons[BF_JFIF]=MakePix1(br->win,br_jfif_bits,br_jfif_width,br_jfif_height);
  bfIcons[BF_JP2] =MakePix1(br->win,br_jp2_bits, br_jp2_width, br_jp2_height);
  bfIcons[BF_JPC] =MakePix1(br->win,br_jpc_bits, br_jpc_width, br_jpc_height);
  bfIcons[BF_MAG] =MakePix1(br->win,br_mag_bits, br_mag_width, br_mag_height);
  bfIcons[BF_MAKI]=MakePix1(br->win,br_maki_bits,br_maki_width,br_maki_height);
  bfIcons[BF_PBM] =MakePix1(br->win,br_pbm_bits, br_pbm_width, br_pbm_height);
  bfIcons[BF_PCD] =MakePix1(br->win,br_pcd_bits, br_pcd_width, br_pcd_height);
  bfIcons[BF_PCX] =MakePix1(br->win,br_pcx_bits, br_pcx_width, br_pcx_height);
  bfIcons[BF_PDS] =MakePix1(br->win,br_pds_bits, br_pds_width, br_pds_height);
  bfIcons[BF_PIC2]=MakePix1(br->win,br_pic2_bits,br_pic2_width,br_pic2_height);
  bfIcons[BF_PIC] =MakePix1(br->win,br_pic_bits, br_pic_width, br_pic_height);
  bfIcons[BF_PI]  =MakePix1(br->win,br_pi_bits,  br_pi_width,  br_pi_height);
  bfIcons[BF_PM]  =MakePix1(br->win,br_pm_bits,  br_pm_width,  br_pm_height);
  bfIcons[BF_PNG] =MakePix1(br->win,br_png_bits, br_png_width, br_png_height);
  bfIcons[BF_PS]  =MakePix1(br->win,br_ps_bits,  br_ps_width,  br_ps_height);
  bfIcons[BF_TGA] =MakePix1(br->win,br_tga_bits, br_tga_width, br_tga_height);
  bfIcons[BF_TIFF]=MakePix1(br->win,br_tiff_bits,br_tiff_width,br_tiff_height);
  bfIcons[BF_XBM] =MakePix1(br->win,br_xbm_bits, br_xbm_width, br_xbm_height);
  bfIcons[BF_XPM] =MakePix1(br->win,br_xpm_bits, br_xpm_width, br_xpm_height);
  bfIcons[BF_XWD] =MakePix1(br->win,br_xwd_bits, br_xwd_width, br_xwd_height);
  bfIcons[BF_ZX]  =MakePix1(br->win,br_zx_bits,  br_zx_width,  br_zx_height);

  bfIcons[BF_SUNRAS]  = MakePix1(br->win, br_sunras_bits,
				 br_sunras_width, br_sunras_height);
  bfIcons[BF_UTAHRLE] = MakePix1(br->win, br_utahrle_bits,
				 br_utahrle_width, br_utahrle_height);
  bfIcons[BF_MGCSFX]  = MakePix1(br->win, br_mgcsfx_bits,
				 br_mgcsfx_width, br_mgcsfx_height);


  /* check that they all got built */
  for (i=0; i<BF_MAX && bfIcons[i]; i++);
  if (i<BF_MAX)
    FatalError("unable to create all built-in icons for schnauzer");

  for (i=0; i<MAXBRWIN; i++) {
    resizeBrowse(&binfo[i], DEF_BROWWIDE, DEF_BROWHIGH);

    XSelectInput(theDisp, binfo[i].win, ExposureMask | ButtonPressMask |
		 KeyPressMask | StructureNotifyMask);
  }


  trashPix = MakePix1(br->win, br_trash_bits, br_trash_width, br_trash_height);
  if (!trashPix)
    FatalError("unable to create all built-in icons for schnauzer");


  /* create movecurs and copycurs cursors */
  mcpix = MakePix1(rootW, filecurs_bits,  filecurs_width,  filecurs_height);
  ccpix = MakePix1(rootW, fileccurs_bits, fileccurs_width, fileccurs_height);
  dcpix = MakePix1(rootW, fdcurs_bits,    fdcurs_width,    fdcurs_height);
  fcmpix= MakePix1(rootW, filecursm_bits, filecursm_width, filecursm_height);

  if (mcpix && ccpix && fcmpix && dcpix) {
    cursfg.red = cursfg.green = cursfg.blue = 0;
    cursbg.red = cursbg.green = cursbg.blue = 0xffff;

    movecurs = XCreatePixmapCursor(theDisp,mcpix,fcmpix,&cursfg,&cursbg,13,13);
    copycurs = XCreatePixmapCursor(theDisp,ccpix,fcmpix,&cursfg,&cursbg,13,13);
    delcurs  = XCreatePixmapCursor(theDisp,dcpix,fcmpix,&cursbg,&cursfg,13,13);
    if (!movecurs || !copycurs || !delcurs)
      FatalError("unable to create schnauzer cursors...");
  }
  else FatalError("unable to create schnauzer cursors...");

  XFreePixmap(theDisp, mcpix);
  XFreePixmap(theDisp, ccpix);
  XFreePixmap(theDisp, dcpix);
  XFreePixmap(theDisp, fcmpix);


  hasBeenSized = 1;  /* we can now start looking at browse events */
}


/***************************************************************/
void OpenBrowse()
{
  /* opens up a single browser window */

  int       i;
  BROWINFO *br;
  char      path[MAXPATHLEN+1];

  /* find next browser to be opened */
  for (i=0; i<MAXBRWIN; i++) {
    br = &binfo[i];
    if (!br->vis) break;
  }
  if (i==MAXBRWIN) return;  /* full up: shouldn't happen */

  anyBrowUp = 1;
  XMapRaised(theDisp, br->win);
  br->vis = 1;

  freeBfList(br);

  /* see if some browser is pointing to the same path as CWD.  If so,
     copy the info, rather than reading the directory */
  xv_getwd(path, sizeof(path));
  if (path[strlen(path)-1] != '/') strcat(path,"/");   /* add trailing '/' */

  for (i=0; i<MAXBRWIN; i++) {
    if (strcmp(binfo[i].path,path)==0) break;
  }

  if (i<MAXBRWIN && &binfo[i] != br) copyDirInfo(&binfo[i], br);
  else scanDir(br);

  SCSetVal(&(br->scrl), 0);


  /* see if that was the last one */
  for (i=0; i<MAXBRWIN; i++) {
    if (!binfo[i].vis) break;
  }
  if (i==MAXBRWIN) {        /* can't open any more */
    windowMB.dim[WMB_BROWSE] = 1;
    for (i=0; i<MAXBRWIN; i++) {
      BTSetActive(&(binfo[i].but[BR_NEWWIN]), 0);
      binfo[i].cmdMB.dim[BR_NEWWIN] = 1;
    }
  }


  changedNumLit(br, -1, 0);
}


/***************************************************************/
static void closeBrowse(br)
     BROWINFO *br;
{
  int i;

  WaitCursor();

  /* closes a specified browse window */
  XUnmapWindow(theDisp, br->win);
  br->vis = 0;

  for (i=0; i<MAXBRWIN; i++) {
    if (binfo[i].vis) break;
  }
  if (i==MAXBRWIN) anyBrowUp = 0;

  /* free all info for this browse window */
  freeBfList(br);
  sprintf(br->path, BOGUSPATH);

  /* turn on 'open new window' command doodads */
  windowMB.dim[WMB_BROWSE] = 0;
  for (i=0; i<MAXBRWIN; i++) {
    BTSetActive(&(binfo[i].but[BR_NEWWIN]), 1);
    binfo[i].cmdMB.dim[BR_NEWWIN] = 0;
  }

  SetCursors(-1);
}


/***************************************************************/
void HideBrowseWindows()
{
  int i;

  for (i=0; i<MAXBRWIN; i++) {
    if (binfo[i].vis) {
      XUnmapWindow(theDisp, binfo[i].win);
      binfo[i].wasvis = 1;
      binfo[i].vis = 0;
    }
  }
}


/***************************************************************/
void UnHideBrowseWindows()
{
  int i;

  for (i=0; i<MAXBRWIN; i++) {
    if (binfo[i].wasvis) {
      XMapRaised(theDisp, binfo[i].win);
      binfo[i].wasvis = 0;
      binfo[i].vis = 1;
    }
  }
}


/***************************************************************/
void SetBrowseCursor(c)
     Cursor c;
{
  int i;

  for (i=0; i<MAXBRWIN; i++) {
    if (haveWindows && binfo[i].win) XDefineCursor(theDisp, binfo[i].win, c);
  }
}

#ifdef VS_RESCMAP
static int _IfTempOut=0;
#endif

/***************************************************************/
void KillBrowseWindows()
{
  int i;

  for (i=0; i<MAXBRWIN; i++) {
    if (haveWindows && binfo[i].win) XDestroyWindow(theDisp, binfo[i].win);
  }
}


static int *event_retP, *event_doneP;

/***************************************************************/
int BrowseCheckEvent(xev, retP, doneP)
     XEvent *xev;
     int *retP, *doneP;
{
  int i;

  event_retP  = retP;     /* so don't have to pass these all over the place */
  event_doneP = doneP;

  for (i=0; i<MAXBRWIN; i++) {
    if (brChkEvent(&binfo[i], xev)) break;
  }

  if (i<MAXBRWIN) return 1;
  return 0;
}

/***************************************************************/
static int brChkEvent(br, xev)
     BROWINFO *br;
     XEvent *xev;
{
  /* checks event to see if it's a browse-window related thing.  If it
     is, it eats the event and returns '1', otherwise '0'. */

  int rv = 1;

  if (!hasBeenSized) return 0;  /* ignore evrythng until we get 1st Resize */


#ifdef VS_RESCMAP
  /* force change color map if have LocalCmap */
  if (browPerfect && browCmap && (_IfTempOut==2)) {
    int i;
    XSetWindowAttributes xswa;

    xswa.colormap = LocalCmap? LocalCmap : theCmap;
    for (i=0; i<MAXBRWIN; ++i)
      XChangeWindowAttributes(theDisp, binfo[i].win, CWColormap, &xswa);
    XFlush(theDisp);
    _IfTempOut=1;
  }
#endif

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x;  y = e->y;  w = e->width;  h = e->height;

    /* throw away excess redraws for 'dumb' windows */
    if (e->count > 0 && (e->window == br->scrl.win))
      ;

    else if (e->window == br->scrl.win)
      SCRedraw(&(br->scrl));

    else if (e->window == br->win || e->window == br->iconW) { /* smart wins */
      /* group individual expose rects into a single expose region */
      int           count;
      Region        reg;
      XRectangle    rect;
      XEvent        evt;

      XSync(theDisp, False);

      xvbcopy((char *) e, (char *) &evt, sizeof(XEvent));
      reg = XCreateRegion();
      count = 0;
      do {
	rect.x      = evt.xexpose.x;
	rect.y      = evt.xexpose.y;
	rect.width  = evt.xexpose.width;
	rect.height = evt.xexpose.height;

	XUnionRectWithRegion(&rect, reg, reg);
	count++;
      } while (XCheckWindowEvent(theDisp, evt.xexpose.window,
				 ExposureMask, &evt));

      XClipBox(reg, &rect);  /* bounding box of region */
      XSetRegion(theDisp, theGC, reg);

      if (DEBUG) {
	fprintf(stderr,"win = %lx, br->win = %lx, iconW = %lx\n",
		e->window, br->win, br->iconW);
	fprintf(stderr,"grouped %d expose events into %d,%d %dx%d rect\n",
		count, rect.x, rect.y, rect.width, rect.height);
      }

      if      (e->window == br->win)   drawBrow(br);

      else if (e->window == br->iconW)
	exposeIconWin(br, rect.x, rect.y, rect.width, rect.height);

      XSetClipMask(theDisp, theGC, None);
      XDestroyRegion(reg);
    }

    else rv = 0;
  }


  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int i,x,y;
    x = e->x;  y = e->y;

#ifdef VS_RESCMAP
    if (browCmap && browPerfect && (_IfTempOut!=0)) {
      XSetWindowAttributes  xswa;
      _IfTempOut--;
      xswa.colormap = browCmap;
      for(i=0;i<MAXBRWIN;i++)
        XChangeWindowAttributes(theDisp, binfo[i].win, CWColormap, &xswa);
      XFlush(theDisp);
    }
#endif

    if (e->button == Button1) {
      if      (e->window == br->win)      clickBrow(br,x,y);
      else if (e->window == br->scrl.win) SCTrack(&(br->scrl),x,y);
      else if (e->window == br->iconW) {
	i = clickIconWin(br, x,y,(unsigned long) e->time,
			 (e->state&ControlMask) || (e->state&ShiftMask));
      }
      else rv = 0;
    }
    else if (e->button == Button4) {   /* note min vs. max, + vs. - */
      /* scroll regardless of where we are in the browser window */
      if (e->window == br->win ||
	  e->window == br->scrl.win ||
	  e->window == br->iconW)
      {
	SCRL *sp=&(br->scrl);
	int  halfpage=MAX(1,sp->page/2); /* user resize to 1 line? */

	if (sp->val > sp->min+halfpage)
	  SCSetVal(sp,sp->val-halfpage);
	else
	  SCSetVal(sp,sp->min);
      }
      else rv = 0;
    }
    else if (e->button == Button5) {   /* note max vs. min, - vs. + */
      /* scroll regardless of where we are in the browser window */
      if (e->window == br->win ||
	  e->window == br->scrl.win ||
	  e->window == br->iconW)
      {
	SCRL *sp=&(br->scrl);
	int  halfpage=MAX(1,sp->page/2); /* user resize to 1 line? */

	if (sp->val < sp->max-halfpage)
	  SCSetVal(sp,sp->val+halfpage);
	else
	  SCSetVal(sp,sp->max);
      }
      else rv = 0;
    }
    else rv = 0;
  }


  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    if (e->window == br->win) keyIconWin(br, e);
    else rv = 0;
  }


  else if (xev->type == ConfigureNotify) {
    XConfigureEvent *e = (XConfigureEvent *) xev;

    if (e->window == br->win) {
      if (DEBUG) fprintf(stderr,"browW got a configure event (%dx%d)\n",
			 e->width, e->height);

      if (br->wide != e->width || br->high != e->height) {
	if (DEBUG) fprintf(stderr,"Forcing a redraw!  (from configure)\n");
	XClearArea(theDisp, br->win, 0, 0,
		   (u_int) e->width, (u_int) e->height, True);
	resizeBrowse(br, e->width, e->height);
      }
    }
    else rv = 0;
  }

  else rv = 0;

  return rv;
}


/***************************************************************/
int BrowseDelWin(win)
     Window win;
{
  /* got a delete window request.  see if the window is a browser window,
     and close accordingly.  Return 1 if event was eaten */

  int i;

  for (i=0; i<MAXBRWIN; i++) {
    if (binfo[i].win == win) {
      closeBrowse(&binfo[i]);
      return 1;
    }
  }

  return 0;
}


/***************************************************************/
static void resizeBrowse(br,w,h)
     BROWINFO *br;
     int w,h;
{
  XSizeHints hints;
  int        i, maxv, page, maxh;

  if (br->wide == w && br->high == h) return;  /* no change in size */

  if (XGetNormalHints(theDisp, br->win, &hints)) {
    hints.width  = w;
    hints.height = h;
    hints.flags |= USSize;
    XSetNormalHints(theDisp, br->win, &hints);
  }

  br->wide = w;  br->high = h;
  br->iwWide = br->wide - (2*LRMARGINS) - (br->scrl.tsize+1) - 2;
  maxh = br->high - (TOPMARGIN + BOTMARGIN);

  br->iwHigh = (maxh / ISPACE_HIGH) * ISPACE_HIGH;
  if (br->iwHigh < ISPACE_HIGH) br->iwHigh = ISPACE_HIGH;

  XMoveResizeWindow(theDisp, br->iconW, LRMARGINS, TOPMARGIN,
		    (u_int) br->iwWide, (u_int) br->iwHigh);


  /* move the buttons around */
  br->numbutshown = (br->wide / (BUTTW + 5));
  RANGE(br->numbutshown, 1, BR_NBUTTS);
  br->numbutshown--;

  for (i=0; i<BR_NBUTTS; i++) {
    /* 'close' always goes on right-most edge */

    if (i<br->numbutshown)
      br->but[i].x = br->wide - (1+br->numbutshown-i) * (BUTTW+5);
    else if (i==BR_CLOSE)
      br->but[i].x = br->wide - (BUTTW+5);
    else
      br->but[i].x = br->wide + 10;    /* offscreen */

    br->but[i].y = br->high - BUTTH - 5;
  }

  br->dirMB.x = br->wide/2 - br->dirMB.w/2;
  br->dirMB.x = br->dirMB.x - br->cmdMB.w/2 + (StringWidth("123 files") + 6)/2;

  br->dirMB.y = 5;

  br->cmdMB.x = br->wide - LRMARGINS - br->cmdMB.w - 2;
  br->cmdMB.y = 5;

  br->numWide = br->iwWide / ISPACE_WIDE;
  br->visHigh = br->iwHigh / ISPACE_HIGH;

  /* compute maxv,page values based on new current size */
  computeScrlVals(br, &maxv, &page);
  if (br->scrl.val>maxv) br->scrl.val = maxv;

  SCChange(&br->scrl, LRMARGINS+br->iwWide+1, TOPMARGIN,
	   1, br->iwHigh, 0, maxv, br->scrl.val, page);
}



/***************************************************************/
void SetBrowStr(str)
     const char *str;
{
  /* put string in *all* browse windows */
  int i;

  for (i=0; i<MAXBRWIN; i++)
    setBrowStr(&binfo[i], str);
}


/***************************************************************/
static void setBrowStr(br, str)
     BROWINFO *br;
     const char *str;
{
  strncpy(br->dispstr, str, (size_t) 256);
  br->dispstr[255] = '\0';
  drawBrowStr(br);
  XFlush(theDisp);
}


/***************************************************************/
void RegenBrowseIcons()
{
  /* called whenever colormaps have changed, and icons need to be rebuilt */

  int i, j, iconcount;
  BFIL     *bf;
  BROWINFO *br;

  for (j=0; j<MAXBRWIN; j++) {
    br = &binfo[j];

    iconcount = 0;

    for (i=0; i<br->bfLen; i++) {
      bf = &(br->bfList[i]);

      drawTemp(br, i, br->bfLen);

      if (bf->pimage && bf->ftype == BF_HAVEIMG) {
	xvDestroyImage(bf->ximage);
	bf->ximage = Pic8ToXImage(bf->pimage, (u_int) bf->w, (u_int) bf->h,
				  browcols, browR, browG, browB);
	iconcount++;
      }

      if ((i         && (i        %100)==0) ||
	  (iconcount && (iconcount% 20)==0)) {  /* mention progress */

	char tmp[64];

	sprintf(tmp, "Re-coloring icons:  processed %d out of %d...",
		i+1, br->bfLen);
	setBrowStr(br, tmp);
      }
    }

    clearTemp(br);
    setBrowStr(br, "");
    drawIconWin(0, &(br->scrl));  /* redraw new icons */
  }
}


/***************************************************************/
void BRDeletedFile(name)
     char *name;
{
  /* called when file 'name' has been deleted.  If any of the browsers
     were showing the directory that the file was in, does a rescan() */

  int  i;
  char buf[MAXPATHLEN + 2], *tmp;

  strcpy(buf, name);
  tmp = (char *) BaseName(buf);  /* intentionally losing constness */
  *tmp = '\0';     /* truncate after last '/' */

  for (i=0; i<MAXBRWIN; i++) {
    if (strcmp(binfo[i].path, buf)==0) rescanDir(&binfo[i]);
  }
}


/***************************************************************/
void BRCreatedFile(name)
     char *name;
{
  BRDeletedFile(name);
}


/**************************************************************/
/***                    INTERNAL FUNCTIONS                  ***/
/**************************************************************/


/***************************************************************/
static void doCmd(br, cmd)
     BROWINFO *br;
     int cmd;
{
  br->lst = 0;

  switch (cmd) {
  case BR_CHDIR:   doChdirCmd(br);
                   break;

  case BR_DELETE:  doDeleteCmd(br);
                   break;

  case BR_MKDIR:   doMkdirCmd(br);
                   break;

  case BR_RENAME:  doRenameCmd(br);
                   break;

  case BR_RESCAN:  rescanDir(br);
                   break;

  case BR_UPDATE:  rescanDir(br);
                   updateIcons(br);
                   break;

  case BR_NEWWIN:  cdBrow(br);     /* try to open current dir */
                   OpenBrowse();
                   break;

  case BR_GENICON: genSelectedIcons(br);  break;

  case BR_SELALL:  {
                     int i;

		     for (i=0; i<br->bfLen; i++)
		       br->bfList[i].lit = 1;
		     br->numlit = br->bfLen;

		     /* EXCEPT for parent directory */
		     if (br->bfLen>0 && strcmp(br->bfList[0].name,"..")==0) {
		       br->numlit--;  br->bfList[0].lit = 0;
		     }

		     changedNumLit(br, -1, 0);
		     drawIconWin(0, &(br->scrl));
		   }
                   break;

  case BR_TEXTVIEW: doTextCmd(br);       break;

  case BR_QUIT:     Quit(0);             break;

  case BR_CLOSE:    closeBrowse(br);     break;

  case BR_HIDDEN:   br->showhidden = !br->showhidden;
                    br->cmdMB.list[cmd] = br->showhidden ? hideHstr : showHstr;
                    rescanDir(br);
                    break;

  case BR_SELFILES: doSelFilesCmd(br);   break;

  case BR_RECURSUP: doRecurseCmd(br);    break;

  case BR_CLIPBRD:  clipChanges(br);     break;

#ifdef AUTO_EXPAND
  case BR_CLEARVD:  Vdsettle();          break;
#endif
  }
}



/***************************************************************/
static void drawBrow(br)
     BROWINFO *br;
{
  int i;

  drawNumfiles(br);
  drawTrash(br);
  drawBrowStr(br);

  for (i=0; i<BR_NBUTTS; i++) {
    if (i<br->numbutshown || i == BR_CLOSE) BTRedraw(&(br->but[i]));
  }

  MBRedraw(&(br->dirMB));
  MBRedraw(&(br->cmdMB));
}


/***************************************************************/
static void drawNumfiles(br)
     BROWINFO *br;
{
  char foo[40];
  int  x, y;

  x = 30;
  y = br->dirMB.y;

  if (br->bfLen != 1) sprintf(foo, "%d files", br->bfLen);
  else strcpy(foo, "1 file");

  XSetForeground(theDisp, theGC, browbg);
  XFillRectangle(theDisp,br->win, theGC, x+1,y+1,
		 (u_int) StringWidth(foo)+6, (u_int) br->dirMB.h-1);

  XSetForeground(theDisp,theGC,browfg);
  XDrawRectangle(theDisp,br->win, theGC, x,y,
		 (u_int) StringWidth(foo)+7, (u_int) br->dirMB.h);

  Draw3dRect(br->win, x+1, y+1, (u_int) StringWidth(foo)+5,
	     (u_int) br->dirMB.h-2, R3D_IN, 2,  browhi, browlo, browbg);

  XSetForeground(theDisp,theGC,browfg);
  DrawString(br->win, x+3, (int) CENTERY(mfinfo, y + br->dirMB.h/2), foo);
}


/***************************************************************/
static void eraseNumfiles(br,nf)
     BROWINFO *br;
     int nf;
{
  char foo[40];

  if (nf != 1) sprintf(foo,"%d files",nf);
  else strcpy(foo,"1 file");

  XClearArea(theDisp,br->win, 30, br->dirMB.y,
	     (u_int) StringWidth(foo)+8, (u_int) br->dirMB.h+1, False);
}


/***************************************************************/
static void drawTrash(br)
     BROWINFO *br;
{
  int  x, y, w, h;

  x = 5;
  y = br->dirMB.y;
  w = 20;
  h = br->dirMB.h;

  XSetForeground(theDisp, theGC, browbg);
  XFillRectangle(theDisp,br->win, theGC, x+1,y+1, (u_int) w-1, (u_int) h-1);

  XSetForeground(theDisp,theGC,browfg);
  XDrawRectangle(theDisp,br->win, theGC, x,y, (u_int) w, (u_int) h);
  Draw3dRect(br->win, x+1, y+1, (u_int) w-2, (u_int) h-2,
	     R3D_IN, 2,  browhi, browlo, browbg);

  XSetForeground(theDisp,theGC,browfg);
  XSetBackground(theDisp,theGC,browbg);
  XCopyPlane(theDisp, trashPix, br->win, theGC,
	     0,0,(u_int) br_trash_width, (u_int) br_trash_height,
	     x+(w-br_trash_width)/2, y+(h-br_trash_height)/2,
	     1L);
}


/***************************************************************/
static int inTrash(br, mx,my)
     BROWINFO *br;
     int       mx,my;
{
  int  x, y, w, h;

  x = 5;
  y = br->dirMB.y;
  w = 20;
  h = br->dirMB.h;

  return PTINRECT(mx, my, x,y,w,h);
}


/***************************************************************/
static void drawBrowStr(br)
     BROWINFO *br;
{
  int y;

  y = br->high - (BUTTH+10) - (CHIGH + 6);

  XSetForeground(theDisp, theGC, browbg);
  XFillRectangle(theDisp, br->win, theGC, 0, y+3,
		 (u_int) br->wide, (u_int) CHIGH+1);

  XSetForeground(theDisp, theGC, browfg);
  XDrawLine(theDisp, br->win, theGC, 0, y,  br->wide, y);
  XDrawLine(theDisp, br->win, theGC, 0, y+CHIGH+4, br->wide, y+CHIGH+4);

  XSetForeground(theDisp, theGC, (ctrlColor) ? browlo : browbg);
  XDrawLine(theDisp, br->win, theGC, 0, y+1,       br->wide, y+1);
  XDrawLine(theDisp, br->win, theGC, 0, y+CHIGH+5, br->wide, y+CHIGH+5);

  XSetForeground(theDisp, theGC, (ctrlColor) ? browhi : browfg);
  XDrawLine(theDisp, br->win, theGC, 0, y+2, br->wide, y+2);
  XDrawLine(theDisp, br->win, theGC, 0, y+CHIGH+6, br->wide, y+CHIGH+6);

  XSetForeground(theDisp, theGC, browfg);
  DrawString(br->win, 5, y+ASCENT+3,  br->dispstr);
}


/***************************************************************/
static void changedNumLit(br, sel, nostr)
     BROWINFO *br;
     int       sel, nostr;
{
  int i, allowtext;

  if (!nostr) setSelInfoStr(br, sel);
#ifdef AUTO_EXPAND
  if (Isvdir(br->path)) {
    BTSetActive(&br->but[BR_DELETE],  0);
    br->cmdMB.dim[BR_DELETE] = 1;

    BTSetActive(&br->but[BR_RENAME],  0);
    br->cmdMB.dim[BR_RENAME] = 1;

    BTSetActive(&br->but[BR_MKDIR],  0);
    br->cmdMB.dim[BR_MKDIR] = 1;
  }
  else {
#endif
  BTSetActive(&br->but[BR_DELETE],  br->numlit>0);
  br->cmdMB.dim[BR_DELETE] = !(br->numlit>0);

  BTSetActive(&br->but[BR_RENAME],  br->numlit==1);
  br->cmdMB.dim[BR_RENAME] = !(br->numlit==1);

  BTSetActive(&br->but[BR_GENICON], br->numlit>0);
  br->cmdMB.dim[BR_GENICON] = !(br->numlit>0);
#ifdef AUTO_EXPAND
  BTSetActive(&br->but[BR_MKDIR],  1);
  br->cmdMB.dim[BR_MKDIR] = 0;
  }
#endif

  /* turn on 'text view' cmd if exactly one non-dir is lit */
  allowtext = 0;
  if (br->numlit == 1) {
    for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);
    if (i<br->bfLen && br->bfList[i].ftype != BF_DIR) allowtext = 1;
  }
  BTSetActive(&br->but[BR_TEXTVIEW], allowtext);
  br->cmdMB.dim[BR_TEXTVIEW] = !allowtext;
}


/***************************************************************/
static void setSelInfoStr(br, sel)
     BROWINFO *br;
     int       sel;
{
  /* sets the '# files selected' string in the brow window appropriately */

  /* criteria:
   *    if no files are lit, display ''
   *    if 1 file is lit, pretend it was selected, fall through...
   *    if 1 or more files are lit
   *       if >1 files are lit, and clicked on nothing, disp '# files selected'
   *       otherwise, display info on selected file
   */

  int  i;
  char buf[256], buf1[256];
  BFIL *bf;


  /* default case if no special info */
  if (br->numlit>0) sprintf(buf, "%d file%s selected", br->numlit,
			    (br->numlit>1) ? "s" : "");
  else buf[0] = '\0';


  if (br->numlit > 0) {                      /* one or more files lit */
    if (br->numlit == 1) {                   /* pretend it was selected */
      for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);
      sel = i;
    }

    if (sel >= 0 && sel < br->bfLen) {  /* display info on selected file */
      bf = &(br->bfList[sel]);

      if (bf->lit) {
	if (br->numlit>1) sprintf(buf1,"  (%d files selected)", br->numlit);
	else buf1[0] = '\0';

	if (bf->ftype==BF_HAVEIMG && bf->imginfo) {
	  sprintf(buf,"%s:  %s", bf->name, bf->imginfo);
	  strcat(buf, buf1);
	}

	else if (bf->ftype != BF_DIR) {     /* no info.  display file size */
	  struct stat st;

	  sprintf(buf, "%s%s", br->path, bf->name);  /* build filename */
#ifdef AUTO_EXPAND
	  Dirtovd(buf);
#endif
	  if (stat(buf, &st) == 0) {
	    sprintf(buf, "%s:  %ld bytes", bf->name, (long)st.st_size);
	    strcat(buf, buf1);
	  }
	}
      }
    }
  }

  setBrowStr(br, buf);
}


/***************************************************************/
static void exposeIconWin(br, x,y,w,h)
     BROWINFO *br;
     int x,y,w,h;
{
  int        i, j, cnt;

  if (!hasBeenSized || !br->bfList || !br->bfLen) return;

  /* figure out what icons intersect the clip rect, and only redraw those,
     as drawing entirely clipped-out images *still* requires transmitting
     the image */

  cnt = (1 + br->visHigh) * br->numWide;  /* # of icons to check */

  for (i=0, j=br->scrl.val * br->numWide;  i<cnt;  i++,j++) {
    int ix,iy,iw,ih;

    /* figure out if this icon region is in the clip region */

    ix = (i%br->numWide) * ISPACE_WIDE;   /* x,y = top-left of icon region */
    iy = (i/br->numWide) * ISPACE_HIGH;
    iw = ISPACE_WIDE;
    ih = ISPACE_HIGH;

    if ((ix+iw >= x) && (ix < x+w) && (iy+ih >= y) && (iy < y+h)) {
      if (j>=0 && j < br->bfLen) drawIcon(br,j);
    }
  }

  Draw3dRect(br->iconW, 0, 0, (u_int) br->iwWide-1, (u_int) br->iwHigh-1,
	     R3D_IN, 2, browhi, browlo, browbg);
}


/***************************************************************/
static void drawIconWin(delta, sptr)
     int delta;
     SCRL *sptr;
{
  int   i,indx, num;
  BROWINFO *br;

  /* figure out BROWINFO pointer from SCRL pointer */
  for (i=0; i<MAXBRWIN; i++) {
    if (&(binfo[i].scrl) == sptr) break;
  }
  if (i==MAXBRWIN) return;   /* didn't find one */

  br = &binfo[i];

  /* make sure we've been sized.  Necessary, as creating/modifying the
     scrollbar calls this routine directly, rather than through
     BrowseCheckEvent() */

  if (!hasBeenSized) return;

  XSetForeground(theDisp, theGC, browfg);
  XSetBackground(theDisp, theGC, browbg);

  if (br->bfList && br->bfLen) {
    num = br->visHigh * br->numWide;

    for (i=0, indx=br->scrl.val * br->numWide;  i<num;  i++,indx++) {
      int x,y,w,h;

      /* erase old icon + string */
      x = (i%br->numWide) * ISPACE_WIDE;   /* x,y = top-left of icon region */
      y = (i/br->numWide) * ISPACE_HIGH;
      w = ISPACE_WIDE;
      h = ISPACE_HIGH;

      XSetForeground(theDisp, theGC, browbg);
      if (ctrlColor) {  /* keep from erasing borders */
	if (x<2) x = 2;
	if (y<2) y = 2;
	if (x+w > br->iwWide-4) w = (br->iwWide-4)-x + 2;
	if (y+h > br->iwHigh-4) h = (br->iwHigh-4)-y + 2;
      }
      XFillRectangle(theDisp, br->iconW, theGC, x, y, ISPACE_WIDE, (u_int) h);

      if (indx>=0 && indx < br->bfLen) drawIcon(br, indx);
    }
  }

  Draw3dRect(br->iconW, 0, 0, (u_int) br->iwWide-1, (u_int) br->iwHigh-1,
	     R3D_IN, 2, browhi, browlo, browbg);
}



/***************************************/
static void drawIcon(br, num)
     BROWINFO *br;
     int       num;
{
  int i,x,y,ix,iy,sw,sh,sx,sy;
  BFIL *bf;
  const char  *nstr, *cstr;
  char  tmpstr[64];
#ifdef VMS
  char  fixedname[64];
#endif


  if (num<0 || num >= br->bfLen) return;
  bf = &(br->bfList[num]);

  i = num - (br->scrl.val * br->numWide);
  if (i<0 || i>(br->visHigh+1)*br->numWide) return; /* not visible */

  if (bf->lit) {
    XSetForeground(theDisp, theGC, browbg);
    XSetBackground(theDisp, theGC, browfg);
  }
  else {
    XSetForeground(theDisp, theGC, browfg);
    XSetBackground(theDisp, theGC, browbg);
  }

  x = (i%br->numWide) * ISPACE_WIDE;   /* x,y = top-left of icon region */
  y = (i/br->numWide) * ISPACE_HIGH;

  /* draw the icon */
  ix = x + ISPACE_WIDE/2 - bf->w/2;          /* center align */
  iy = y + ISPACE_TOP + ISIZE_HIGH - bf->h;  /* bottom align */


  if (bf->ftype >= 0 && bf->ftype <BF_MAX) {  /* built-in icon */
    XCopyPlane(theDisp, bfIcons[bf->ftype], br->iconW, theGC,
	       0, 0, (u_int) bf->w, (u_int) bf->h, ix, iy, 1L);
  }

  else if (bf->ftype == BF_HAVEIMG && bf->ximage) {
    XPutImage(theDisp, br->iconW, theGC, bf->ximage, 0,0, ix,iy,
	      (u_int) bf->w, (u_int) bf->h);
  }

  else {  /* shouldn't happen */
    XDrawRectangle(theDisp, br->iconW, theGC, ix, iy,
		   (u_int) bf->w, (u_int) bf->h);
  }


  cstr = bf->name;
#ifdef VMS
  if (bf->ftype == BF_DIR) {
    char  *vstr;
    strcpy(fixedname, bf->name);
    vstr = rindex(fixedname, '.');   /* lop off '.DIR' suffix, if any */
    if (vstr) *vstr = '\0';
    cstr = fixedname;
  }
#endif /* VMS */

  if (!strcmp(bf->name,"..")) cstr = "<parent>";


  /* decide if the title is too big, and shorten if neccesary */
  if (StringWidth(cstr) > ISPACE_WIDE-6) {
    int dotpos;
    strncpy(tmpstr, cstr, (size_t) 56);
    tmpstr[56] = '\0'; /* MR: otherwise it dies on long file names */
    dotpos = strlen(tmpstr);
    strcat(tmpstr,"...");

    while(StringWidth(tmpstr) > ISPACE_WIDE-6 && dotpos>0) {
      /* change last non-dot char in tmpstr to a dot, and lop off
	 last dot */

      dotpos--;
      tmpstr[dotpos] = '.';
      tmpstr[dotpos+3] = '\0';
    }

    nstr = tmpstr;
  }
  else nstr = cstr;


  /* draw the title */
  sw = StringWidth(nstr);
  sh = CHIGH;

  sx = x + ISPACE_WIDE/2 - sw/2 - 2;
  sy = y + ISPACE_TOP + ISIZE_HIGH + ISPACE_TTOP - 1;

  XSetForeground(theDisp, theGC,
		 (bf->lit && bf->lit!=ICON_ONLY) ? browfg : browbg);
  XFillRectangle(theDisp, br->iconW, theGC, sx, sy,
		 (u_int) sw + 4, (u_int) sh + 2);

  XSetForeground(theDisp, theGC,
		 (bf->lit && bf->lit!=ICON_ONLY) ? browbg : browfg);
  CenterString(br->iconW, x + ISPACE_WIDE/2,
	       y + ISPACE_TOP + ISIZE_HIGH + ISPACE_TTOP + CHIGH/2, nstr);
}


/***************************************/
static void eraseIcon(br, num)
     BROWINFO *br;
     int       num;
{
  /* note: doesn't erase the icon's title, just the icon itself */

  int i,x,y,ix,iy,w,h;
  BFIL *bf;

  if (num<0 || num >= br->bfLen) return;
  bf = &(br->bfList[num]);

  i = num - (br->scrl.val * br->numWide);

  XSetForeground(theDisp, theGC, browbg);

  x = (i % br->numWide) * ISPACE_WIDE;   /* x,y = top-left of icon region */
  y = (i / br->numWide) * ISPACE_HIGH;
  w = bf->w;
  h = bf->h;

  ix = x + ISPACE_WIDE/2 - w/2;          /* center align */
  iy = y + ISPACE_TOP + ISIZE_HIGH - h;  /* bottom align */

  if (ctrlColor) { /* keep from erasing borders */
    if (ix<2) ix = 2;
    if (iy<2) iy = 2;
    if (ix+w > br->iwWide-2) w = (br->iwWide-2) - ix;
    if (iy+h > br->iwHigh-2) h = (br->iwHigh-2) - iy;
  }

  XFillRectangle(theDisp, br->iconW, theGC, ix, iy, (u_int) w+1, (u_int) h+1);
}


/***************************************/
static void eraseIconTitle(br, num)
     BROWINFO *br;
     int       num;
{
  /* note: doesn't erase the icon, just the icon's title */

  int i,x,y;

  if (num<0 || num >= br->bfLen) return;
  i = num - (br->scrl.val * br->numWide);

  x = (i % br->numWide) * ISPACE_WIDE;   /* x,y = top-left of icon region */
  y = (i / br->numWide) * ISPACE_HIGH;

  XSetForeground(theDisp, theGC, browbg);
  XFillRectangle(theDisp, br->iconW, theGC,
		 x, y + ISPACE_TOP + ISIZE_HIGH + ISPACE_TTOP - 1,
		 (u_int) ISPACE_WIDE, (u_int) LINEHIGH);

  if (ctrlColor)
    Draw3dRect(br->iconW, 0, 0, (u_int) br->iwWide-1, (u_int) br->iwHigh-1,
	       R3D_IN, 2, browhi, browlo, browbg);
}



/***************************************/
static void makeIconVisible(br, num)
     BROWINFO *br;
     int       num;
{
  int sval, first, numvis;

  /* if we know what path we have, remember last visible icon for this path */
  if (br->path)
    recIconVisible(br->path, num);

  /* if icon #i isn't visible, adjust scrollbar so it *is* */

  sval = br->scrl.val;
  first = sval * br->numWide;
  numvis = br->visHigh * br->numWide;

  while (num<first) { sval--;  first = sval * br->numWide; }
  if (num>= (first+numvis)) {
    /* get #num into top row, to reduce future scrolling */
    while (num>=(first+br->numWide) && sval<br->scrl.max) {
      sval++;  first = sval * br->numWide;
    }
  }
  SCSetVal(&br->scrl, sval);
}



/***************************************************************/
static void clickBrow(br, x,y)
     BROWINFO *br;
     int x,y;
{
  int   i;
  BUTT *bp;

  for (i=0; i<BR_NBUTTS; i++) {
    bp = &(br->but[i]);
    if (PTINRECT(x,y,bp->x,bp->y,bp->w,bp->h)) break;
  }

  if (i<BR_NBUTTS) {
    if (BTTrack(bp)) doCmd(br, i);
    return;
  }

  if (MBClick(&(br->dirMB), x,y)) {
    i = MBTrack(&(br->dirMB));
    if (i >= 0) changedBrDirMB(br, i);
    return;
  }

  if (MBClick(&(br->cmdMB), x,y)) {
    i = MBTrack(&(br->cmdMB));
    if (i >= 0) doCmd(br, i);
    return;
  }

  return;
}

/***************************************************************/
static int updateSel(br, sel, multi, mtime)
  BROWINFO *br;
  int sel, multi;
  unsigned long mtime;
{
  int i;
  BFIL *bf;

  if (sel == -1) {  /* clicked on nothing */
    if (!multi) {   /* deselect all */
      for (i=0; i<br->bfLen; i++) {
	if (br->bfList[i].lit) { br->bfList[i].lit = 0;  drawIcon(br,i); }
	br->numlit = 0;
      }
    }

    changedNumLit(br, sel, 0);
    br->lastIconClicked = -1;
  }


  else if (multi) {      /* clicked on something, while 'multi' key down */
    bf = &(br->bfList[sel]);

    bf->lit = !bf->lit;
    br->lastIconClicked = -1;

    if (!bf->lit) br->numlit--;
             else br->numlit++;

    drawIcon(br, sel);

    changedNumLit(br, sel, 0);
  }


  else {   /* clicked on something, and not in 'multi' mode */
    /* if there are some lit, and we *didn't* click one of them, turn
       all others off */
    if (br->numlit && !br->bfList[sel].lit) {
      for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
	if (bf->lit && i!=sel) {
	  bf->lit = 0;
	  drawIcon(br, i);
	  br->numlit--;
	}
      }
    }


    bf = &br->bfList[sel];

    if (!bf->lit) {
      bf->lit = 1;
      drawIcon(br, sel);
      br->numlit = 1;
    }


    changedNumLit(br, sel, 0);


    /* see if we've double-clicked something */
    if (mtime &&
        sel==br->lastIconClicked && mtime-br->lastClickTime < DBLCLICKTIME) {
      br->lastIconClicked = -1;    /* YES */

      doubleClick(br, sel);
      return -1;
    }

    else {
      br->lastIconClicked = sel;
      br->lastClickTime = mtime;
    }
  }

  changedNumLit(br, -1, 0);
  return 0;
}


/***************************************************************/
static int clickIconWin(br, mx, my, mtime, multi)
     BROWINFO *br;
     int mx,my,multi;
     unsigned long mtime;
{
  /* returns '-1' normally, returns an index into bfList[] if the user
     double-clicks an icon */

  int         i,j, sel, cpymode, dodel;
  BROWINFO   *destBr;
  BFIL       *bf;
  char        buf[256];
  const char *destFolderName;

  if (!br->bfList || !br->bfLen) return -1;

  destBr = br;  destFolderName = ".";

  sel = mouseInWhichIcon(br, mx, my);
  dodel = 0;

  recIconVisible(br->path, sel);

  if (updateSel(br, sel, multi, mtime))
    return -1;


  {    /* track mouse until button1 is released */

    Window       rW, win, cW;
    int          x, y, rootx, rooty, iwx, iwy, bwx, bwy;
    unsigned int mask;
    Cursor       curs;
    int          samepos, oldx, oldy, oldbrnum, destic, origsval, first;
    int          hasrect, rx, ry, rw, rh;

    rx = ry = rw = rh = 0;
    first = 1;  hasrect = 0;  cpymode = 0;
    origsval = br->scrl.val;

    if ( (sel>=0 && !multi) || sel==-1) {
      /* clicked on an icon, or clicked on nothing... */

      while (!XQueryPointer(theDisp, rootW, &rW, &cW, &rootx, &rooty,
			    &x,&y,&mask));
      if (mask & Button1Mask) {  /* still held down */

	if (sel == -1) curs = tcross;
	else if ((mask & ControlMask) || (mask & ShiftMask)) {
	  curs = copycurs;  cpymode = 1;
	}
	else curs = movecurs;

	/* change cursors */
	for (i=0; i<MAXBRWIN; i++)
	  XDefineCursor(theDisp,binfo[i].iconW, curs);

	samepos = oldx = oldy = oldbrnum = 0;

	while (1) {  /* wait for button 1 to be released */
	  while (!XQueryPointer(theDisp,rootW,&rW,&win,&rootx,&rooty,
				&x,&y,&mask));
	  if (!(mask & Button1Mask)) break;

	  if (sel>=0) {  /* see if changed copy/move status (and cursor) */
	    int cmod;

	    cmod = (mask&ControlMask || mask&ShiftMask) ? 1 : 0;

	    if (cmod != cpymode && !dodel) {
	      curs = (cmod) ? copycurs : movecurs;
	      	for (i=0; i<MAXBRWIN; i++)
		  XDefineCursor(theDisp,binfo[i].iconW, curs);
	    }
	    cpymode = cmod;


	    /* see if cursor is in any of the trash can areas */
	    for (i=0; i<MAXBRWIN; i++) {
	      if (binfo[i].vis) {
		XTranslateCoordinates(theDisp, rW, binfo[i].win, rootx,rooty,
				      &bwx,&bwy, &cW);
		if (inTrash(&binfo[i], bwx, bwy)) break;
	      }
	    }

	    if (dodel && i==MAXBRWIN) {        /* moved out */
	      dodel = 0;
	      curs = (cpymode) ? copycurs : movecurs;
	      for (i=0; i<MAXBRWIN; i++)
		XDefineCursor(theDisp,binfo[i].iconW, curs);
	    }

	    else if (!dodel && i<MAXBRWIN) {   /* moved in */
	      dodel = 1;
	      for (i=0; i<MAXBRWIN; i++)
		XDefineCursor(theDisp,binfo[i].iconW, delcurs);
	    }
	  }



	  XTranslateCoordinates(theDisp, rW, br->iconW, rootx,rooty,
				&iwx,&iwy, &cW);

	  /* find deepest child that the mouse is in */
	  while (win!=None) {
	    XTranslateCoordinates(theDisp, rW, win, rootx, rooty, &x, &y, &cW);
	    if (cW == None) break;
	    else win = cW;
	  }

	  for (i=0; i<MAXBRWIN && win!=binfo[i].iconW; i++);
	  if (i==MAXBRWIN) { destBr=(BROWINFO *) NULL; destFolderName = ""; }

	  /* if it's in any icon window, and we're doing icon-dragging
	     OR we're doing a rectangle-drag */

	  if (i<MAXBRWIN || sel == -1) {
	    if (i<MAXBRWIN) destBr = &binfo[i];
	    if (sel == -1)  destBr = br;

	    /* AUTO-SCROLLING:  scroll any icon window if we're doing an
	       icon-drag.  Only scroll the original window if we're doing
	       a rect drag */

	    if (sel>=0 && (oldx!=x || oldy!=y || oldbrnum!=i)) {  /* moved */
	      samepos = 0;  oldx = x;  oldy = y;  oldbrnum = i;
	    }
	    else {
	      int scamt = 0;
	      if (sel == -1) {   /* rectangle dragging */
		if (iwy < SCROLLVERT)                scamt = -1;
		if (iwy < -PAGEVERT)                 scamt = -destBr->visHigh;
		if (iwy > destBr->iwHigh-SCROLLVERT) scamt = 1;
		if (iwy > destBr->iwHigh+PAGEVERT)   scamt = destBr->visHigh;
	      }
	      else {             /* file dragging */
		if (y >= 0 && y < SCROLLVERT) scamt = -1;
		if (y <= destBr->iwHigh && y > destBr->iwHigh-SCROLLVERT)
		  scamt = 1;
	      }

	      if ((scamt < 0 && destBr->scrl.val > 0) ||
		  (scamt > 0 && destBr->scrl.val < destBr->scrl.max)) {

		if (hasrect) invertSelRect(br,rx,ry,rw,rh);
		hasrect = 0;

		SCSetVal(&(destBr->scrl), destBr->scrl.val + scamt);
		Timer(150);
	      }
	    }


	    /* if we clicked on an icon (originally), and therefore are
	       showing the 'move files' cursor, see if the cursor is within
	       the icon region of any folders.  If so, light up *the icon
	       only* of the selected folder, by setting it's 'lit' field to
	       ICON_ONLY, and clearing any other folders that might have
	       that set (only one dest folder can be lit at a time) */

	    if (sel>=0) {
	      destic = mouseInWhichIcon(destBr, x, y);

	      bf = (destic>=0) ? &(destBr->bfList[destic]) : (BFIL *) NULL;
	      if (!bf || (bf && bf->ftype == BF_DIR && bf->lit!=ICON_ONLY)) {
		for (i=0; i<destBr->bfLen; i++) {      /* clear prev dest */
		  if (destBr->bfList[i].lit == ICON_ONLY) {
		    destBr->bfList[i].lit = 0;
		    drawIcon(destBr, i);
		  }
		}
		destFolderName = ".";
	      }

	      if (bf && bf->ftype == BF_DIR && !bf->lit) {
		bf->lit = ICON_ONLY;
		drawIcon(destBr, destic);
		destFolderName = bf->name;
	      }
	    }

	    /* Dragging a selection rectangle. */

	    else {
	      static int prevx, prevy, prevcnt;
	      int        origy, cnt;

	      if (first) { prevx = mx;  prevy = my;  first=0;  prevcnt = -1; }

	      /* set x,y to iconW coordinate system, clipped... */
	      x = iwx;  y = iwy;
	      RANGE(x, 0, br->iwWide-1);
	      RANGE(y, 0, br->iwHigh-1);

	      if (x != prevx || y != prevy || !hasrect) {   /* cursor moved */
		origy = my - (br->scrl.val - origsval) * ISPACE_HIGH;

		if (hasrect) invertSelRect(br, rx, ry, rw, rh);  /* turn off */

		rx  = (mx    < x) ? mx    : x;
		ry  = (origy < y) ? origy : y;
		rw  = abs(mx - x);
		rh  = abs(origy - y);

		/* figure out which icons need to be lit/unlit.  Only
		   redraw those that have changed state */

		for (i=0,cnt=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
		  int ix, iy, isin;

		  ix = ((i%br->numWide) * ISPACE_WIDE)
		                  + ISPACE_WIDE/2 - bf->w/2;
		  iy = ((i/br->numWide) * ISPACE_HIGH)
		                  + ISPACE_TOP + ISIZE_HIGH - bf->h;

		  iy = iy - br->scrl.val * ISPACE_HIGH;

		  /* is the icon rectangle of this beastie inside the
		     dragging rectangle ? */

		  isin =  (ix+bf->w >= rx && ix < rx+rw &&
			   iy+bf->h >= ry && iy < ry+rh);

		  if (isin) {
		    if (bf->lit==0) {
		      bf->lit = TEMP_LIT;  drawIcon(br, i);
		    }
		    else if (bf->lit==1) {
		      bf->lit = TEMP_LIT1; drawIcon(br, i);
		    }
		  }
		  else {
		    if (bf->lit == TEMP_LIT) {
		      bf->lit = 0;  drawIcon(br, i);
		    }
		    else if (bf->lit == TEMP_LIT1) {
		      bf->lit = 1;  drawIcon(br, i);
		    }
		  }

		  if (bf->lit) cnt++;
		}

		invertSelRect(br, rx, ry, rw, rh);  /* turn on */
		hasrect = 1;

		if (cnt != prevcnt) {
		  if (cnt) sprintf(buf, "%d file%s selected", cnt,
				   (cnt>1) ? "s" : "");
		  else buf[0]='\0';
		  setBrowStr(br, buf);
		  prevcnt = cnt;
		}

		prevx = x;  prevy = y;
	      }
	    }
	  }

	  else {      /* NOT in an icon window... */
	    /* turn off ALL ICON_ONLY icons in ALL icon windows... */
	    for (i=0; i<MAXBRWIN; i++) {
	      for (j=0, bf=binfo[i].bfList; j<binfo[i].bfLen; j++,bf++) {
		if (bf->lit == ICON_ONLY) {
		  bf->lit = 0;
		  drawIcon(&binfo[i], j);
		}
	      }
	    }
	  }
	}

	/* RELEASED BUTTON:  back to normal arrow cursor */
	for (i=0; i<MAXBRWIN; i++)
	  XDefineCursor(theDisp, binfo[i].iconW, None);

	if (sel == -1) {  /* was dragging rectangle */
	  if (hasrect) invertSelRect(br, rx, ry, rw, rh);

	  br->numlit = 0;
	  for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
	    if (!multi && bf->lit == 1) { bf->lit = 0;  drawIcon(br, i); }

	    if (bf->lit == TEMP_LIT || bf->lit == TEMP_LIT1) {
	      bf->lit = 1;  drawIcon(br, i);
	    }

	    if (bf->lit) br->numlit++;
	  }

	  changedNumLit(br, -1, 0);
	}
      }
    }
  }


  /* if doing a copy or a move, do the thing to the files */
  if (sel >= 0) {

    if (DEBUG) {
      fprintf(stderr,"---------------\n");
      fprintf(stderr,"Source  Dir: '%s'\n", br->path);
      fprintf(stderr,"Dest    Dir: '%s'\n", destBr ? destBr->path : "<null>");
      fprintf(stderr,"Dest Folder: '%s'\n", destFolderName);
    }


    if (!br->numlit) {
      if (DEBUG) fprintf(stderr, "no selected files.  Nothing to do!\n");
    }

    else if (dodel) {
      doDeleteCmd(br);
    }

    else if (!destBr || strlen(destFolderName) == 0) {
      if (DEBUG) fprintf(stderr, "no destination.  Nothing to do!\n");
    }

    else if (strcmp(destFolderName,".")     == 0 &&
	     strcmp(br->path, destBr->path) == 0) {
      if (DEBUG) fprintf(stderr,"source == destination.  Nothing to do!\n");
    }

    else {    /* have to do some copying/moving */
      char **nlist;  int ncnt;

      if (DEBUG) fprintf(stderr,"Files to %s:  ", cpymode ? "copy" : "move");

      nlist = (char **) malloc(br->numlit * sizeof(char *));
      if (!nlist) FatalError("clickIconWin: couldn't malloc nlist");

      /* copy names to nlist */
      for (i=ncnt=0, bf=br->bfList;  i<br->bfLen;  i++,bf++) {
	if (bf->lit == 1 && ncnt < br->numlit) {
	  nlist[ncnt] = (char *) malloc(strlen(bf->name)+1);
	  if (!nlist[ncnt]) FatalError("out of memory building namelist");

	  strcpy(nlist[ncnt], bf->name);
	  ncnt++;
	  if (DEBUG) fprintf(stderr, "%s  ", bf->name);
	}
      }
      if (DEBUG) fprintf(stderr,"\n\n");

#ifdef VMS
      /*
       * For VMS, our directory file names are identifed by the
       * special filename extension, ".DIR".  Unfortunately, this
       * needs to be stripped before we ever actually use the name
       * in a copy command... :(     RLD 26-FEB-1993
       */

      *rindex ( destFolderName, '.' ) = '\0';  /* FIXME: potentially writing into static strings! */
#endif


      dragFiles(br, destBr, br->path, destBr->path, destFolderName, nlist,
		ncnt, cpymode);

      /* free namelist */
      for (i=0; i<ncnt; i++) if (nlist[i]) free(nlist[i]);
      free(nlist);
    }


    if (destBr) {   /* turn off any ICON_ONLY folders */
      for (i=0, bf=destBr->bfList;  i<destBr->bfLen;  i++,bf++) {
	if (bf->lit == ICON_ONLY) {
	  bf->lit = 0;
	  drawIcon(destBr, i);
	}
      }
    }
  }      /* end of 'tracking' sub-function */

  return -1;
}

/*******************************************/
static void doubleClick(br, sel)
     BROWINFO *br;
     int       sel;
{
  int i, j, k;
  char buf[512];
  BFIL *bf;

  /* called to 'open' icon #sel, which could be a file or a dir */

  br->lst = 0;

  /* if sel == -1, then called via RETURN key.  just use first lit item
     as thing that was double clicked on */

  if (sel < 0) {
    for (sel=0; sel<br->bfLen && br->bfList[sel].lit==0; sel++);
  }

  j = 0;
  k = numnames;
  if (sel < 0 || sel >= br->bfLen) return;   /* no selection */

  /* if this isn't a dir, copy it, and all other selected non-dir
     filenames into the ctrl list */
  if (ISLOADABLE(br->bfList[sel].ftype)) {
    AddFNameToCtrlList(br->path, br->bfList[sel].name);
    j++;
  }

  for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
    if (bf->lit && ISLOADABLE(bf->ftype) && i!=sel) {
      AddFNameToCtrlList(br->path, bf->name);
      j++;
    }
  }

  /* clear all 'other' selections */
  for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
    if (bf->lit && i!=sel) {
      bf->lit = 0;
      drawIcon(br, i);
    }
  }
  br->numlit = 1;
  changedNumLit(br, sel, 0);


  if (j && k<numnames) {   /* stuff was *actually* added to the list */
    curname = nList.selected = k;
    ChangedCtrlList();  /* added something to list */
  }



  /* double-clicked something.  We should do something about it */
  if (br->bfList[sel].ftype == BF_DIR) {  /* try to cd */
#ifndef VMS
    sprintf(buf, "%s%s", br->path, br->bfList[sel].name);
#else
    if (strcmp(br->bfList[sel].name,"..")==0) sprintf(buf,"[-]");
    else sprintf(buf, "%s%s", br->path, br->bfList[sel].name);
#endif

#ifdef AUTO_EXPAND
    if (Chvdir(buf)) {
#else
    if (chdir(buf)) {
#endif
      char str[512];
      sprintf(str,"Unable to cd to '%s'\n", br->bfList[sel].name);
      setBrowStr(br, str);
      XBell(theDisp, 50);
    }
    else {
#ifdef AUTO_EXPAND
      if (Isvdir(buf)) {
	BTSetActive(&br->but[BR_DELETE],  0);
	br->cmdMB.dim[BR_DELETE] = 1;

	BTSetActive(&br->but[BR_RENAME],  0);
	br->cmdMB.dim[BR_RENAME] = 1;

	BTSetActive(&br->but[BR_MKDIR],  0);
	br->cmdMB.dim[BR_MKDIR] = 1;
      }
      else {
	BTSetActive(&br->but[BR_MKDIR],  1);
	br->cmdMB.dim[BR_MKDIR] = 0;
      }
#endif
      scanDir(br);
      SCSetVal(&(br->scrl), 0);  /* reset to top on a chdir */
      restIconVisible(br);
    }
  }

  else {  /* not a directory.  Try to open it as a file */
    int j;

    sprintf(buf, "%s%s", br->path, br->bfList[sel].name);

    /* if name is already in namelist, make it the current selection */
    for (j=0;  j<numnames && strcmp(namelist[j],buf);  j++);

    if (j<numnames) {
      if (nList.selected != j) {
	curname = nList.selected = j;
	ChangedCtrlList();
      }
      *event_retP = THISNEXT;
    }
    else { *event_retP = LOADPIC;  SetDirFName(buf);  }

#ifdef VS_RESCMAP
    /* Change Colormap for browser */
    if (browPerfect && browCmap) {
      int i;
      XSetWindowAttributes  xswa;
      if(LocalCmap) {
	xswa.colormap = LocalCmap;
	_IfTempOut=2;
      }
      else {
	xswa.colormap = theCmap;
	_IfTempOut=2;
      }
      for(i=0;i<MAXBRWIN;i++)
	XChangeWindowAttributes(theDisp, binfo[i].win, CWColormap, &xswa);
      XFlush(theDisp);
    }
#endif

    *event_doneP = 1;     /* make MainLoop load image */
  }
}

/*******************************************/
static int mouseInWhichIcon(br, mx, my)
     BROWINFO *br;
     int       mx, my;
{
  /* mx,my are mouse position in iconW coordinates.  Returns '-1' if the
     mouse is not in any icon */

  int i, x, y, ix, iy, sel, base, num;
  BFIL *bf;

  /* figure out what was clicked... */
  base = br->scrl.val   * br->numWide;
  num = (1+br->visHigh) * br->numWide;

  for (i=0, sel=base; i<num; i++,sel++) {
    if (sel>=0 && sel<br->bfLen) {
      bf = &(br->bfList[sel]);

      x = (i%br->numWide) * ISPACE_WIDE;  /* x,y=top-left of icon region */
      y = (i/br->numWide) * ISPACE_HIGH;

      ix = x + ISPACE_WIDE/2 - bf->w/2;          /* center align */
      iy = y + ISPACE_TOP + ISIZE_HIGH - bf->h;  /* bottom align */

      if (PTINRECT(mx,my, ix, iy, bf->w, bf->h)) break;
    }
  }

  if (i==num) return -1;
  return sel;
}


/*******************************************/
static void invertSelRect(br, x, y, w, h)
     BROWINFO *br;
     int       x,y,w,h;
{
  if (w>1 && h>1) {
    XSetState(theDisp,theGC, browfg, browbg, GXinvert, browfg^browbg);
    XDrawRectangle(theDisp, br->iconW, theGC, x,y, (u_int) w, (u_int) h);
    XSetState(theDisp,theGC, browfg, browbg, GXcopy, AllPlanes);
  }
}




/***************************************************************/
static void keyIconWin(br, kevt)
     BROWINFO *br;
     XKeyEvent *kevt;
{
  char buf[128];
  KeySym ks;
  int stlen, shift, dealt, ck;

  stlen = XLookupString(kevt, buf, 128, &ks, (XComposeStatus *) NULL);
  shift = kevt->state & ShiftMask;
  ck    = CursorKey(ks, shift, 1);
  dealt = 1;

  RemapKeyCheck(ks, buf, &stlen);

  /* check for arrow keys, Home, End, PgUp, PgDown, etc. */
  if (ck!=CK_NONE)  browKey(br,ck);
  else dealt = 0;

  /* fake tab and backtab to be same as 'space' and 'backspace' */
  if (ks==XK_Tab &&  shift) { buf[0] = '\010';  stlen = 1; }
  if (ks==XK_Tab && !shift) { buf[0] = ' ';  stlen = 1; }

  if (dealt || !stlen) return;

  /* keyboard equivalents */
  switch (buf[0]) {
  case '\003': doCmd(br, BR_CHDIR);    break;      /* ^C = Chdir */
  case '\004': doCmd(br, BR_DELETE);   break;      /* ^D = Delete Files */
  case '\016': doCmd(br, BR_MKDIR);    break;      /* ^N = New Directory */
  case '\022': doCmd(br, BR_RENAME);   break;      /* ^R = Rename */
  case '\023': doCmd(br, BR_RESCAN);   break;      /* ^S = reScan */
  case '\025': doCmd(br, BR_UPDATE);   break;      /* ^U = Update icons */
  case '\027': doCmd(br, BR_NEWWIN);   break;      /* ^W = open new Window */
  case '\007': doCmd(br, BR_GENICON);  break;      /* ^G = Generate icons */
  case '\001': doCmd(br, BR_SELALL);   break;      /* ^A = select All */
  case '\024': doCmd(br, BR_TEXTVIEW); break;      /* ^T = Textview */
  case '\005': doCmd(br, BR_RECURSUP); break;      /* ^E = rEcursive update */
  case '\021': doCmd(br, BR_QUIT);     break;      /* ^Q = Quit xv */

  case '\006': doCmd(br, BR_SELFILES); break;      /* ^F = Select Files */
  case '\030': doCmd(br, BR_CLIPBRD);  break;      /* ^X = Copy to clipboard */


  /* case '\003': FakeButtonPress(&but[BCMTVIEW]); break; */    /* ^C */

  case '\033': doCmd(br, BR_CLOSE);   break;      /* ESC = Close window */

  case '\r':
  case '\n':   doubleClick(br, -1);   break;      /* RETURN = load selected */

  case ' ':
    if (br->lst && (time(NULL) <= br->lst + incrementalSearchTimeout))
      goto do_default;
    /* else fall through... */
  case '\010':
  case '\177':   /* SPACE = load next, BS/DEL = load prev */
    if (br->bfLen && br->numlit >= 1) {
      int i, j, viewsel;
      char fname[MAXPATHLEN];

      /* if 'shift-space' find last lit icon, select the next one after it,
	 and load it.  If 'space' do the same, but lose prior lit.  These
	 are the only cases where br->numlit >1 allowed */

      if (br->numlit>1  && buf[0] != ' ') return;

      if (buf[0]==' ' && (br->numlit>1 || (br->numlit==1 && shift))) {
	for (i=br->bfLen-1; i>=0 && !br->bfList[i].lit; i--);  /* i=last lit */
	if (i==br->bfLen-1) return;

	i++;
	if (!shift) {
	  for (j=0; j<br->bfLen; j++) {
	    if (br->bfList[j].lit && j!=i) {
	      br->bfList[j].lit = 0;
	      drawIcon(br, j);
	    }
	  }
	}

	br->bfList[i].lit = 1;

	for (j=0, br->numlit=0; j<br->bfLen; j++)
	  if (br->bfList[j].lit) br->numlit++;

	makeIconVisible(br, i);
	drawIcon(br, i);
	setSelInfoStr(br, i);

	/* load this file, stick it in ctrlList, etc. */

	if (ISLOADABLE(br->bfList[i].ftype)) {
	  char foo[256];

	  j = numnames;
	  AddFNameToCtrlList(br->path, br->bfList[i].name);
	  if (j<numnames) {  /* actually added it */
	    curname = nList.selected = j;
	    ChangedCtrlList();
	  }

	  /* try to open this file */
	  sprintf(foo, "%s%s", br->path, br->bfList[i].name);
#ifdef AUTO_EXPAND
	Dirtovd(foo);
#endif
	  for (j=0; j<numnames && strcmp(namelist[j],foo); j++);
	  if (j<numnames) {
	    curname = nList.selected = j;
	    ChangedCtrlList();
	    *event_retP = THISNEXT;
	  }
	  else { *event_retP = LOADPIC;  SetDirFName(foo); }

	  *event_doneP = 1;
	}
      }

      else {          /* not SPACE, or SPACE and lit=1 and not shift */
	for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);  /* find lit one */
	sprintf(fname, "%s%s", br->path, br->bfList[i].name);
#ifdef AUTO_EXPAND
	Dirtovd(fname);
#endif
	viewsel = !(strcmp(fname, fullfname));

	if (viewsel) {
	  if (buf[0]==' ') browKey(br, CK_RIGHT);
	              else browKey(br, CK_LEFT);
	}

	if (!br->bfList[i].lit || !viewsel) {   /* changed selection */
	  for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);  /* find it */
	  if (br->bfList[i].ftype != BF_DIR)
	    doubleClick(br, -1);
	}
      }
    }
    break;


  default:  /* unknown character.  Take it as an alpha accelerator */
  do_default:  /* (goto-label, not switch-label) */
    if (buf[0] >= 32) browAlpha(br, buf[0]);
                else XBell(theDisp, 0);
    break;
  }

}


/***************************************************/
static void browKey(br, key)
     BROWINFO *br;
     int key;
{
  int i,j;

  if (!br->bfLen) return;

  /* an arrow key (or something like that) was pressed in icon window.
     change selection/scrollbar accordingly */

  br->lst = 0;

  /* handle easy keys */
  if (key == CK_PAGEUP)   SCSetVal(&br->scrl, br->scrl.val - br->scrl.page);
  if (key == CK_PAGEDOWN) SCSetVal(&br->scrl, br->scrl.val + br->scrl.page);
  if (key == CK_HOME)     SCSetVal(&br->scrl, br->scrl.min);
  if (key == CK_END)      SCSetVal(&br->scrl, br->scrl.max);

  /* handle up/down/left/right keys
   *
   * if precisely *one* item is lit, than the up/down/left/right keys move
   * the selection.
   *
   * if NO items are lit, then left/right select the first/last fully-displayed
   * icon, and up/down simply scroll window up or down, without selecting
   * anything
   *
   * if more than one item is lit, up/down/left/right keys BEEP
   */

  if (key==CK_UP || key==CK_DOWN || key==CK_LEFT || key==CK_RIGHT) {

    if (br->numlit > 1) XBell(theDisp, 50);
    else if (br->numlit == 1) {
      /* find it */
      for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);

      /* if it's not visible, lose it */
      if ((i <   br->scrl.val * br->numWide) ||
	  (i >= (br->scrl.val + br->visHigh) * br->numWide)) {
	br->numlit = 0;
	br->bfList[i].lit = 0;
	drawIcon(br, i);
      }
      else {
	/* make it visible */
	makeIconVisible(br, i);

	j = i;

	if (key == CK_UP)    j = i - br->numWide;
	if (key == CK_DOWN)  j = i + br->numWide;
	if (key == CK_LEFT)  j = i - 1;
	if (key == CK_RIGHT) j = i + 1;

	if (j >= 0 && j < br->bfLen) {
	  br->bfList[i].lit = 0;
	  br->bfList[j].lit = 1;
	  makeIconVisible(br,j);
	  drawIcon(br,i);
	  drawIcon(br,j);
	  setSelInfoStr(br, j);
	}
      }
    }


    if (br->numlit == 0) {   /* no current selection */
      if (key == CK_UP)   SCSetVal(&br->scrl, br->scrl.val - 1);
      if (key == CK_DOWN) SCSetVal(&br->scrl, br->scrl.val + 1);
      if (key == CK_LEFT || key == CK_RIGHT) {
	if (key == CK_LEFT)  i = (br->scrl.val+br->visHigh) * br->numWide - 1;
	                else i = (br->scrl.val * br->numWide);
	RANGE(i, 0, br->bfLen-1);
	br->bfList[i].lit = 1;
	br->numlit = 1;
	changedNumLit(br, i, 0);
	drawIcon(br, i);
      }
    }
  }
}



/***************************************************/
static void browAlpha(br, ch)
     BROWINFO *br;
     int ch;
{
  /* find first 'plain' file that is lexically >= than the given ch */

  int i,j;
  time_t now = time(NULL);

  if (!br->bfLen) return;
  if (ch < ' ' || ch > '\177')  return;    /* ignore 'funny' keys */

  for (i=0; i<br->bfLen && br->bfList[i].ftype==BF_DIR; i++);
  if (i==br->bfLen) return;    /* only directories in this dir */

  if (!br->lst || (br->lst + incrementalSearchTimeout < now)) br->len = 0;
  br->lst = now;
  
  if (br->len + 2 > br->siz)
    if ((br->str = (char *)realloc(br->str, (br->siz = br->len + 32))) == NULL)
       br->siz = br->len = 0;
  
  if (br->len + 2 <= br->siz) {
    br->str[br->len++] = ch;
    br->str[br->len] = '\0';
  }

  for ( ; i<br->bfLen; i++) {
    if (strncmp(br->bfList[i].name, br->str, br->len) >= 0) break;
  }

  if (i==br->bfLen) i--;

  for (j=0; j<br->bfLen; j++) {
    if (br->bfList[j].lit) {
      br->bfList[j].lit = 0;
      drawIcon(br, j);
    }
  }

  br->bfList[i].lit = 1;
  drawIcon(br, i);
  br->numlit = 1;

  makeIconVisible(br, i);
  changedNumLit(br, i, 0);
}



/***************************************************/
static void changedBrDirMB(br, sel)
     BROWINFO *br;
     int sel;
{

  if (sel != 0) {   /* changed directories */
    char tmppath[MAXPATHLEN+1];
    int  i;

    /* build 'tmppath' */
    tmppath[0] = '\0';
    for (i = br->ndirs-1; i>=sel; i--)
      strcat(tmppath, br->mblist[i]);

    if (tmppath[0] == '\0') {
      /* special case:  if cd to '/', fix path (it's currently "") */
#ifdef apollo    /*** Apollo DomainOS uses // as the network root ***/
      strcpy(tmppath,"//");
#else
      strcpy(tmppath,"/");
#endif
    }
#ifdef VMS
    else {
      /*
       *  The VMS chdir always needs 2 components (device and directory),
       *  so convert "/device" to "/device/000000" and convert
       *  "/" to "/XV_Root_Device/000000" (XV_RootDevice will need to be
       *  a special concealed device setup to provide list of available
       *  disks).
       *
       *  End 'tmppath' by changing trailing '/' (of dir name) to a '\0'
       */
      *rindex ( tmppath, '/') = '\0';
      if ( ((br->ndirs-sel) == 2) && (strlen(tmppath) > 1) )
	strcat ( tmppath, "/000000" ); /* add root dir for device */
      else if  ((br->ndirs-sel) == 1 )
	strcpy ( tmppath, "/XV_Root_Device/000000" );  /* fake top level */
    }
#endif

#ifdef AUTO_EXPAND
    if (Chvdir(tmppath)) {
#else
    if (chdir(tmppath)) {
#endif
      char str[512];
      sprintf(str,"Unable to cd to '%s'\n", tmppath);
      MBRedraw(&(br->dirMB));
      setBrowStr(br,str);
      XBell(theDisp, 50);
    }
    else {
#ifdef AUTO_EXPAND
      if (Isvdir(tmppath)) {
	BTSetActive(&br->but[BR_DELETE],  0);
	br->cmdMB.dim[BR_DELETE] = 1;

	BTSetActive(&br->but[BR_RENAME],  0);
	br->cmdMB.dim[BR_RENAME] = 1;

	BTSetActive(&br->but[BR_MKDIR],  0);
	br->cmdMB.dim[BR_MKDIR] = 1;
      }
      else {
	BTSetActive(&br->but[BR_MKDIR],  1);
	br->cmdMB.dim[BR_MKDIR] = 0;
      }
#endif
      scanDir(br);
      SCSetVal(&br->scrl, 0);  /* reset to top of window on a chdir */
      restIconVisible(br);
    }
  }
}



/***************************************************************/
static int cdBrow(br)
     BROWINFO *br;
{
  /* returns non-zero on failure */

  int rv;

  /* temporarily excise trailing '/' char from br->path */
  if ((strlen(br->path) > (size_t) 2) && br->path[strlen(br->path)-1] == '/')
    br->path[strlen(br->path)-1] = '\0';

#ifdef AUTO_EXPAND
  rv = Chvdir(br->path);
#else
  rv = chdir(br->path);
#endif
  if (rv) {
    char str[512];
    sprintf(str, "Unable to cd to '%s'\n", br->path);
    setBrowStr(br, str);
    XBell(theDisp, 50);
  }

#ifdef AUTO_EXPAND
  if (Isvdir(br->path)) {
    BTSetActive(&br->but[BR_DELETE],  0);
    br->cmdMB.dim[BR_DELETE] = 1;

    BTSetActive(&br->but[BR_RENAME],  0);
    br->cmdMB.dim[BR_RENAME] = 1;

    BTSetActive(&br->but[BR_MKDIR],  0);
    br->cmdMB.dim[BR_MKDIR] = 1;
  }
  else {
    BTSetActive(&br->but[BR_MKDIR],  1);
    br->cmdMB.dim[BR_MKDIR] = 0;
  }
#endif

  restIconVisible(br);
  strcat(br->path, "/");   /* put trailing '/' back on */
  return rv;
}


/***************************************************************/
static void copyDirInfo(srcbr, dstbr)
     BROWINFO *srcbr, *dstbr;
{
  /* copies br info from an already existing browser window
     (ie, one that is already showing the same directory) */

  int i, oldnum, maxv, page;

  oldnum = dstbr->bfLen;
  dstbr->lastIconClicked = -1;
  setBrowStr(dstbr,"");

  /* copy mblist */
  dstbr->ndirs = srcbr->ndirs;
  for (i=0;  i<dstbr->ndirs;  i++) {
    dstbr->mblist[i] = strdup(srcbr->mblist[i]);
    if (!dstbr->mblist[i]) FatalError("unable to malloc brMBlist[]");
  }

#if 0
  dstbr->dirMB.list  = srcbr->mblist;  /* original bug..? */
  dstbr->dirMB.nlist = srcbr->ndirs;
#else
  dstbr->dirMB.list  = dstbr->mblist;  /* fixed by        */
  dstbr->dirMB.nlist = dstbr->ndirs;   /*   jp-extension. */
#endif

  XClearArea(theDisp, dstbr->dirMB.win, dstbr->dirMB.x, dstbr->dirMB.y,
	     dstbr->dirMB.w+3, dstbr->dirMB.h+3, False);

  i = StringWidth(dstbr->mblist[0]) + 10;
  dstbr->dirMB.x = dstbr->dirMB.x + dstbr->dirMB.w/2 - i/2;
  dstbr->dirMB.w = i;
  MBRedraw(&dstbr->dirMB);

  strcpy(dstbr->path, srcbr->path);

  WaitCursor();
  freeBfList(dstbr);     /* just to be safe */

  /* copy the bfList info */
  dstbr->numlit = 0;
  dstbr->bfLen = srcbr->bfLen;

  dstbr->bfList = (BFIL *) calloc((size_t) dstbr->bfLen, sizeof(BFIL));
  if (!dstbr->bfList) FatalError("can't create bfList!");

  for (i=0; i<dstbr->bfLen; i++) {
    BFIL *sbf, *dbf;

    if ((i&0x03) == 0) drawTemp(dstbr, i, dstbr->bfLen);
    if ((i & 0x3f) == 0) WaitCursor();

    sbf = &(srcbr->bfList[i]);
    dbf = &(dstbr->bfList[i]);

    if (sbf->name) {
      dbf->name = (char *) malloc(strlen(sbf->name) + 1);
      if (!dbf->name) FatalError("ran out of memory for dbf->name");
      strcpy(dbf->name, sbf->name);
    }
    else dbf->name = (char *) NULL;

    if (sbf->imginfo) {
      dbf->imginfo = (char *) malloc(strlen(sbf->imginfo) + 1);
      if (!dbf->imginfo) FatalError("ran out of memory for dbf->imginfo");
      strcpy(dbf->imginfo, sbf->imginfo);
    }
    else dbf->imginfo = (char *) NULL;

    dbf->ftype = sbf->ftype;
    dbf->lit = 0;
    dbf->w   = sbf->w;
    dbf->h   = sbf->h;

    if (sbf->pimage) {
      dbf->pimage = (byte *) malloc((size_t) dbf->w * dbf->h);
      if (!dbf->pimage) FatalError("ran out of memory for dbf->pimage");
      xvbcopy((char *) sbf->pimage, (char *) dbf->pimage,
	      (size_t) (dbf->w * dbf->h));
    }
    else dbf->pimage = (byte *) NULL;

    if (sbf->ximage) {
      dbf->ximage = (XImage *) malloc(sizeof(XImage));
      if (!dbf->ximage) FatalError("ran out of memory for dbf->ximage");
      xvbcopy((char *) sbf->ximage, (char *) dbf->ximage, sizeof(XImage));

      if (sbf->ximage->data) {
	dbf->ximage->data = (char *) malloc((size_t) dbf->ximage->height *
					    dbf->ximage->bytes_per_line);
	if (!dbf->ximage->data) FatalError("ran out of memory for ximg data");
	xvbcopy((char *) sbf->ximage->data, (char *) dbf->ximage->data,
		(size_t) dbf->ximage->height * dbf->ximage->bytes_per_line);
      }
    }
    else dbf->ximage = (XImage *) NULL;

  }

  clearTemp(dstbr);

  /* misc setup (similar to endScan(), but without unnecessary stuff) */
  eraseNumfiles(dstbr, oldnum);
  drawNumfiles(dstbr);
  drawTrash(dstbr);
  computeScrlVals(dstbr, &maxv, &page);
  if (dstbr->scrl.val > maxv) dstbr->scrl.val = maxv;

  XClearArea(theDisp, dstbr->iconW, 0, 0, (u_int) dstbr->iwWide,
	     (u_int) dstbr->iwHigh, True);
  SCSetRange(&dstbr->scrl, 0, maxv, dstbr->scrl.val, page);

  SetCursors(-1);
}




/***************************************************************/
static void scanDir(br)
     BROWINFO *br;
{
  /* loads contents of current working directory into BFIL structures...
   * and also loads up the MB list
   *
   * note:  when actually doing the code, in addition to stat'ing files, we
   * might want to try reading the first couple of bytes out of them, to see
   * what magicno they have, and putting up an appropriate icon for different
   * types of standard files.  Make this mechanism fairly clean and easily
   * extensible, as different machines will have different types of files,
   * and it's reasonable to expect folks to want to add their own bitmaps
   */

  int   i,j,oldbflen,vmsparent;
  BFIL *bf;

  DIR           *dirp;
  char          *dbeg, *dend;
  char          *dirnames[MAXDEEP];
  static char    path[MAXPATHLEN + 2] = { '\0' };

#ifdef NODIRENT
  struct direct *dp;
#else
  struct dirent *dp;
#endif


  br->lastIconClicked = -1;  /* turn off possibility of seeing a dblclick */
  setBrowStr(br,"");


  /********************************************************************/
  /*** LOAD UP the brdirMB information to reflect the new directory ***/
  /********************************************************************/


  xv_getwd(path, sizeof(path));
  if (path[strlen(path)-1] != '/') strcat(path,"/");   /* add trailing '/' */

  for (i=0; i<br->ndirs; i++) free((char *) br->mblist[i]);  /* clear old dir names */

  /* path will be something like: "/u3/bradley/src/weiner/whatever/" */

  dbeg = dend = path;
  for (i=0; i<MAXDEEP && dend; i++) {
    dend = (char *) index(dbeg,'/');  /* find next '/' char */

#ifdef apollo
    /** On Apollos the path will be something like //machine/users/foo/ **/
    /** handle the initial // **/
    if ((dend == dbeg ) && (dbeg[0] == '/') && (dbeg[1] == '/')) dend += 1;
#endif

    dirnames[i] = dbeg;
    dbeg = dend+1;
  }
  br->ndirs = i-1;


  /* build brMBlist */
  for (i = br->ndirs-1,j=0; i>=0; i--,j++) {
    size_t  stlen = (i<(br->ndirs-1)) ? dirnames[i+1] - dirnames[i]
                                  : strlen(dirnames[i]);
    char   *copy;

    copy = malloc(stlen+1);
    if (!copy) FatalError("unable to malloc brMBlist[]");

    strncpy(copy, dirnames[i], stlen);
    copy[stlen] = '\0';
    br->mblist[j] = copy;
  }


  /* refresh the brdirMB button */
  br->dirMB.list  = br->mblist;
  br->dirMB.nlist = br->ndirs;

  XClearArea(theDisp, br->dirMB.win, br->dirMB.x, br->dirMB.y,
	     br->dirMB.w+3, br->dirMB.h+3, False);

  i = StringWidth(br->mblist[0]) + 10;
  br->dirMB.x = br->dirMB.x + br->dirMB.w/2 - i/2;
  br->dirMB.w = i;
  MBRedraw(&br->dirMB);

  strcpy(br->path, path);   /* will have a trailing '/' character */


  /********************************************************************/
  /*** read the directory   (load up bfList)                        ***/
  /********************************************************************/


  WaitCursor();

  oldbflen = br->bfLen;

  freeBfList(br);   /* free all memory currently used by bfList structure */

  /* count how many files are in the list */

  dirp = opendir(".");
  if (!dirp) {
    endScan(br, oldbflen);
    setBrowStr(br, "Couldn't read current directory.");
    SetCursors(-1);
    return;
  }

#ifdef VMS
  br->bfLen = 1;   /* always have a parent directory */
#endif

  while ( (dp = readdir(dirp)) != NULL) {
    if (strcmp(dp->d_name, ".") &&
	strcmp(dp->d_name, THUMBDIR)) {
      if (!br->showhidden && dp->d_name[0] == '.' &&
	  strcmp(dp->d_name,"..")!=0) continue;
      else
	br->bfLen++;
    }
    if ((br->bfLen & 0x3f) == 0) WaitCursor();
  }


  if (br->bfLen) {
    int readcount, iconcount, statcount;

    br->bfList = (BFIL *) calloc((size_t) br->bfLen, sizeof(BFIL));

    if (!br->bfList) FatalError("can't create bfList! (malloc failed)\n");

    rewinddir(dirp);   /* back to beginning of directory */

    vmsparent = 0;
#ifdef VMS
    vmsparent = 1;
#endif

    /* get info for each file in directory */

    readcount = 0;  iconcount = 0;  statcount = 0;
    for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {

      drawTemp(br, i, br->bfLen);

      if ((i & 0x1f) == 0) WaitCursor();

      /* get next directory entry that isn't '.' or THUMBDIR or
	 '..' in root directory, or a hidden file if !showhidden */

      if (vmsparent) {     /* first time:  make bogus parent for VMS */
	bf->name  = (char *) malloc(strlen("..") + 1);
	if (!bf->name) FatalError("out of memory in scanDir()");
	strcpy(bf->name, "..");
	bf->ftype  = BF_DIR;
	bf->w      = br_dir_width;
	bf->h      = br_chr_width;
	bf->pimage = (byte *) NULL;
	bf->ximage = (XImage *) NULL;
	bf->lit    = 0;
      }
      else {
	do { dp = readdir(dirp); }
	while (dp && (strcmp(dp->d_name, ".")==0                    ||
		      strcmp(dp->d_name, THUMBDIR)==0               ||
		      strcmp(dp->d_name, THUMBDIRNAME)==0           ||
		      (br->ndirs==1 && strcmp(dp->d_name,"..")==0)  ||
		      (!br->showhidden && dp->d_name[0] == '.' &&
		       strcmp(dp->d_name,"..")!=0)));

	if (!dp) { br->bfLen = i;  break; }   /* dir got shorter... */
      }

      if (!vmsparent) scanFile(br, bf, dp->d_name);
      vmsparent = 0;

      statcount++;
      if (bf->ftype == BF_HAVEIMG) iconcount++;
      if (bf->ftype == BF_FILE)    readcount++;

      if ((statcount && (statcount%100)==0) ||
	  (iconcount && (iconcount% 20)==0) ||
	  (readcount && (readcount% 20)==0)) {   /* mention progress */

	char tmp[64];

	sprintf(tmp, "Processed %d out of %d...", i+1, br->bfLen);
	setBrowStr(br, tmp);
      }
    }

    clearTemp(br);
  }

  closedir(dirp);

  endScan(br, oldbflen);
}


/***************************************************************/
static void endScan(br, oldnum)
     BROWINFO *br;
     int       oldnum;
{
  /* called at end of scanDir() and rescanDir() */

  int maxv, page;
  int w,h;

  setBrowStr(br,"");
  sortBFList(br);

  eraseNumfiles(br, oldnum);
  drawNumfiles(br);
  drawTrash(br);

  computeScrlVals(br, &maxv, &page);
  if (br->scrl.val>maxv) br->scrl.val = maxv;

  /* have to clear window as # of icons may have changed */
  w = br->iwWide;  h = br->iwHigh;
  if (ctrlColor) { w -= 4;  h -= 4; }
  if (w<1) w = 1;
  if (h<1) h = 1;

  XClearArea(theDisp, br->iconW, (ctrlColor) ? 2 : 0, (ctrlColor) ? 2 : 0,
	     (u_int) w, (u_int) h, False);

  SCSetRange(&br->scrl, 0, maxv, br->scrl.val, page);

  SetCursors(-1);
}


/***************************************************************/
static void scanFile(br, bf, name)
     BROWINFO *br;
     BFIL *bf;
     char *name;
{
  /* given a pointer to an empty BFIL structure, and a filename,
     loads up the BFIL structure appropriately */

  struct stat    st;

  /* copy name */
  bf->name = (char *) malloc(strlen(name) + 1);

  if (!bf->name) FatalError("ran out of memory for bf->name");
  strcpy(bf->name, name);

  /* default icon values.  (in case 'stat' doesn't work) */
  bf->ftype  = BF_FILE;
  bf->w = br_file_width;  bf->h = br_file_height;
  bf->pimage = (byte *) NULL;
  bf->ximage = (XImage *) NULL;
  bf->lit    = 0;


  if (stat(bf->name, &st)==0) {
#ifdef AUTO_EXPAND
    bf->ftype = stat2bf((u_int) st.st_mode , bf->name);
#else
    bf->ftype = stat2bf((u_int) st.st_mode);
#endif
    if (bf->ftype == BF_FILE && (st.st_mode & 0111)) bf->ftype = BF_EXE;

    switch (bf->ftype) {
    case BF_DIR:  bf->w = br_dir_width;   bf->h = br_dir_height;   break;
    case BF_CHR:  bf->w = br_chr_width;   bf->h = br_chr_height;   break;
    case BF_BLK:  bf->w = br_blk_width;   bf->h = br_blk_height;   break;
    case BF_SOCK: bf->w = br_sock_width;  bf->h = br_sock_height;  break;
    case BF_FIFO: bf->w = br_fifo_width;  bf->h = br_fifo_height;  break;
    case BF_EXE:  bf->w = br_exe_width;   bf->h = br_exe_height;   break;
    }
  }


  loadThumbFile(br, bf);


  if (bf->ftype == BF_FILE || bf->ftype == BF_EXE) {
    /* if it's a regular file, with no thumbnail, try to determine what
       type of file it is */

    int filetype;

    filetype = ReadFileType(bf->name);

    switch (filetype) {
    case RFT_GIF:      bf->ftype = BF_GIF;      break;
    case RFT_PM:       bf->ftype = BF_PM;       break;
    case RFT_PBM:      bf->ftype = BF_PBM;      break;
    case RFT_XBM:      bf->ftype = BF_XBM;      break;
    case RFT_SUNRAS:   bf->ftype = BF_SUNRAS;   break;
    case RFT_BMP:      bf->ftype = BF_BMP;      break;
    case RFT_WBMP:     bf->ftype = BF_BMP;      break;
    case RFT_UTAHRLE:  bf->ftype = BF_UTAHRLE;  break;
    case RFT_IRIS:     bf->ftype = BF_IRIS;     break;
    case RFT_PCX:      bf->ftype = BF_PCX;      break;
    case RFT_JFIF:     bf->ftype = BF_JFIF;     break;
    case RFT_TIFF:     bf->ftype = BF_TIFF;     break;
    case RFT_PDSVICAR: bf->ftype = BF_PDS;      break;
    case RFT_COMPRESS: bf->ftype = BF_COMPRESS; break;
    case RFT_BZIP2:    bf->ftype = BF_BZIP2;    break;
    case RFT_PS:       bf->ftype = BF_PS;       break;
    case RFT_IFF:      bf->ftype = BF_IFF;      break;
    case RFT_TARGA:    bf->ftype = BF_TGA;      break;
    case RFT_XPM:      bf->ftype = BF_XPM;      break;
    case RFT_XWD:      bf->ftype = BF_XWD;      break;
    case RFT_FITS:     bf->ftype = BF_FITS;     break;
    case RFT_PNG:      bf->ftype = BF_PNG;      break;
    case RFT_ZX:       bf->ftype = BF_ZX;       break;	/* [JCE] */
    case RFT_PCD:      bf->ftype = BF_PCD;      break;
    case RFT_MAG:      bf->ftype = BF_MAG;      break;
    case RFT_MAKI:     bf->ftype = BF_MAKI;     break;
    case RFT_PIC:      bf->ftype = BF_PIC;      break;
    case RFT_PI:       bf->ftype = BF_PI;       break;
    case RFT_PIC2:     bf->ftype = BF_PIC2;     break;
    case RFT_MGCSFX:   bf->ftype = BF_MGCSFX;   break;
    }
  }
}



/***************************************************************/
static unsigned long bfcompares;

static void sortBFList(br)
     BROWINFO *br;
{
  bfcompares = 0;
  qsort((char *) br->bfList, (size_t) br->bfLen, sizeof(BFIL), bfnamCmp);
}


static int bfnamCmp(p1, p2)
     const void *p1, *p2;
{
  BFIL *b1, *b2;

  b1 = (BFIL *) p1;
  b2 = (BFIL *) p2;

  bfcompares++;
  if ((bfcompares & 0x7f)==0) WaitCursor();

  /* sort critera:  directories first, in alphabetical order,
     followed by everything else, in alphabetical order */

  if ((b1->ftype == BF_DIR && b2->ftype == BF_DIR) ||
      (b1->ftype != BF_DIR && b2->ftype != BF_DIR))
    return strcmp(b1->name, b2->name);

  else if (b1->ftype == BF_DIR && b2->ftype != BF_DIR) return -1;
  else return 1;
}


/***************************************************************/
static void rescanDir(br)
     BROWINFO *br;
{
  /* chdir to br->path
   * build two name-lists, one holding the names of all files in the bfList,
   * and the second holding the names of all files in this directory
   * (ignore . and .. and THUMBDIR in both)
   *
   * include directories in both lists, but filter files (in the second list)
   * by br->showhidden
   *
   * sort the two namelists in pure-alpha order.
   * for each item in the first list, see if it has an entry in the second
   *   list.  If it does, remove the entry from *both* lists
   *
   * once that's done, we'll have a list of files that have been deleted
   *     (are in bfList, but not in directory)
   * and a list of files that have been created
   *     (aren't in bfList, are in directory)
   *
   * malloc a new temp bfList that has room for (bfLen - #del'd + #created)
   * entries, copy all entries from the old bfList (that aren't on the
   * deleted list) into the new list.  for each entry on the created list,
   * copy it to the new bfList and load its icon (not that it's likely to
   * have one, in which case fall back to the generic file-type icon).
   *
   * free data used by any remaining (deleted) entries in the old bfList
   *
   * call sortBfList to put the new bflist into the right order
   */

  int    i, j, bflen, dirlen, dnum, cmpval, newlen, n, oldlen;
  char **bfnames, **dirnames;
  BFIL  *newbflist, *bf;

  if (cdBrow(br)) return;

  WaitCursor();

  /* build 'bfnames' array */
  bflen = oldlen = br->bfLen;   bfnames = (char **) NULL;
  if (bflen) {
    bfnames = (char **) malloc(bflen * sizeof(char *));
    if (!bfnames) FatalError("couldn't alloc bfnames in rescanDir()");
    for (i=0; i<bflen; i++) {
      bfnames[i] = (char *) malloc(strlen(br->bfList[i].name) + 1);
      if (!bfnames[i]) FatalError("couldn't alloc bfnames in rescanDir()");

      strcpy(bfnames[i], br->bfList[i].name);
    }
  }

  WaitCursor();

  dirnames = getDirEntries(".", &dirlen, br->showhidden);

  WaitCursor();

  /* note, either (or both) dirnames/bfnames can be NULL, in which case
     their respective 'len's will be zero */

  /* sort the two name lists */
  if (bflen)  qsort((char *) bfnames,  (size_t) bflen, sizeof(char *),namcmp);
  if (dirlen) qsort((char *) dirnames, (size_t) dirlen,sizeof(char *),namcmp);

  /* run through the bflist, and delete entries common to both lists */
  for (i=0, dnum=0; i<bflen && dnum<dirlen; i++) {
    cmpval = strcmp(bfnames[i], dirnames[dnum]);
    if      (cmpval < 0) continue;
    else if (cmpval > 0) {       /* advance dnum, and try again */
      dnum++;
      i--;
    }
    else /* cmpval == 0 */ {     /* remove from both lists */
      free(bfnames[i]);  free(dirnames[dnum]);
      bfnames[i] = dirnames[dnum] = (char *) NULL;
      dnum++;
    }
  }


  WaitCursor();

  /* compress the lists, removing NULL entries, . .. and THUMBDIR */

  for (i=j=0; i<bflen; i++) {
    if (bfnames[i] && strcmp(bfnames[i],".") && strcmp(bfnames[i],"..") &&
	strcmp(bfnames[i],THUMBDIR) && strcmp(bfnames[i],THUMBDIRNAME)) {
      bfnames[j++] = bfnames[i];
    }
  }
  bflen = j;


  for (i=j=0; i<dirlen; i++) {
    if (dirnames[i] && strcmp(dirnames[i],".") && strcmp(dirnames[i],"..") &&
	strcmp(dirnames[i],THUMBDIR) && strcmp(dirnames[i],THUMBDIRNAME)) {
      dirnames[j++] = dirnames[i];
    }
  }
  dirlen = j;


  if (DEBUG) {
    fprintf(stderr,"%d files seem to have gone away:  ", bflen);
    for (i=0; i<bflen; i++)
      fprintf(stderr,"%s ", bfnames[i]);
    fprintf(stderr,"\n\n");

    fprintf(stderr,"%d files seem to have appeared:  ", dirlen);
    for (i=0; i<dirlen; i++)
      fprintf(stderr,"%s ", dirnames[i]);
    fprintf(stderr,"\n\n");
  }


  /* create a new bfList */
  newlen = br->bfLen - bflen + dirlen;  /* oldlen - #del'd + #created */
  if (newlen>0) {
    newbflist = (BFIL *) calloc((size_t) newlen, sizeof(BFIL));
    if (!newbflist) FatalError("couldn't malloc newbflist in rescanDir()");

    /* copy all entries from old bflist that aren't on deleted list into new */
    for (i=n=0, bf=br->bfList;  i<br->bfLen && n<newlen; i++, bf++) {
      for (j=0; j<bflen; j++) {
	if (strcmp(bf->name, bfnames[j])==0) break;
      }
      if (j == bflen) {   /* not in del list.  copy to new list */
	xvbcopy((char *) bf, (char *) &(newbflist[n++]),  sizeof(BFIL));
      }
      else {              /* in deleted list.  free all data for this entry */
	if (bf->name)    free(bf->name);
	if (bf->imginfo) free(bf->imginfo);
	if (bf->pimage)  free(bf->pimage);
	if (bf->ximage)  xvDestroyImage(bf->ximage);
      }
    }


    /* add all entries in the 'created' list */
    for (i=0; i<dirlen && n<newlen; i++) {
      scanFile(br, &newbflist[n++], dirnames[i]);
    }


    if (br->bfList) free(br->bfList);

    br->bfList = newbflist;
    br->bfLen  = (n < newlen) ? n : newlen;
  }
  else freeBfList(br);      /* dir is now empty */

  WaitCursor();


  /* free memory still in use by bfnames and dirnames arrays */
  for (i=0; i<bflen; i++)  { if (bfnames[i])  free(bfnames[i]); }
  for (i=0; i<dirlen; i++) { if (dirnames[i]) free(dirnames[i]); }
  if (bfnames) free(bfnames);
  if (dirnames) free(dirnames);

  endScan(br, oldlen);
}

/***************************************************************/
static void freeBfList(br)
     BROWINFO *br;
{
  int   i;
  BFIL *bf;

  if (br->bfList) {
    for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
      if ((i & 0x3f) == 0) WaitCursor();

      if (bf->name)    free(bf->name);
      if (bf->imginfo) free(bf->imginfo);
      if (bf->pimage)  free(bf->pimage);
      if (bf->ximage)  xvDestroyImage(bf->ximage);
    }

    free(br->bfList);
  }

  br->bfList = (BFIL *) NULL;
  br->bfLen  = 0;
  br->numlit = 0;
}


static int namcmp(p1, p2)
     const void *p1, *p2;
{
  char **s1, **s2;
  s1 = (char **) p1;
  s2 = (char **) p2;

  return strcmp(*s1,*s2);
}

/***************************************************************/
static char **getDirEntries(dir, lenP, dohidden)
     const char *dir;
     int  *lenP;
     int   dohidden;
{
  /* loads up all directory entries into an array.  This *isn't* a great
     way to do it, but I can't count on 'scandir()' existing on
     every system.  Returns 'NULL' on failure, or pointer to array of
     'lenP' strings on success.  '.' and '..' ARE included in list
     if !dohidden, all '.*' files are skipped (except . and ..) */

  int    i, dirlen;
  DIR   *dirp;
  char **names;
#ifdef NODIRENT
  struct direct *dp;
#else
  struct dirent *dp;
#endif


  dirp = opendir(dir);
  if (!dirp) {
    SetISTR(ISTR_WARNING, "%s: %s", dir, ERRSTR(errno));
    *lenP = 0;
    return (char **) NULL;
  }


  /* count # of entries in dir (worst case) */
  for (dirlen=0;  (dp = readdir(dirp)) != NULL;  dirlen++);
  if (!dirlen) {
    closedir(dirp);
    *lenP = dirlen;
    return (char **) NULL;
  }


  /* load up the entries, now that we know how many to make */
  names = (char **) malloc(dirlen * sizeof(char *));
  if (!names) FatalError("malloc failure in getDirEntries()");


  rewinddir(dirp);
  for (i=0; i<dirlen; ) {
    dp = readdir(dirp);
    if (!dp) break;

    if (!dohidden) {
#ifndef VMS
      if (dp->d_name[0] == '.' &&
	  strcmp(dp->d_name,"." )!=0 &&
	  strcmp(dp->d_name,"..")!=0) continue;
#endif
    }

    names[i] = (char *) malloc(strlen(dp->d_name) + 1);
    if (!names[i]) FatalError("malloc failure in getDirEntries()");

    strcpy(names[i], dp->d_name);
    i++;
  }

  if (i<dirlen) dirlen = i;     /* dir got shorter... */

  closedir(dirp);

  *lenP = dirlen;
  return names;
}



/***************************************************************/
static void computeScrlVals(br, max, page)
     BROWINFO *br;
     int *max, *page;
{
  /* called whenever bfList or size of icon window has changed */

  if (br->numWide<1) br->numWide = 1;    /* safety */

  br->numHigh = (br->bfLen + br->numWide-1) / br->numWide;  /* # icons high */

  *page = br->visHigh;
  *max  = br->numHigh - *page;
}






/***************************************************************/
static void genSelectedIcons(br)
     BROWINFO *br;
{
  int i, cnt;

  setBrowStr(br, "");

  if (!br->bfList || !br->bfLen) return;

  if (cdBrow(br)) return;

  WaitCursor();

  for (i=cnt=0; i<br->bfLen; i++) {
    if (br->bfList[i].lit) {
      if (br->numlit) drawTemp(br, cnt, br->numlit);
      cnt++;
      makeIconVisible(br, i);
      eraseIcon(br, i);
      genIcon(br, &(br->bfList[i]));
      br->bfList[i].lit = 0;
      drawIcon(br, i);
    }
  }

  if (br->numlit) clearTemp(br);
  br->numlit = 0;
  changedNumLit(br, -1, 1);

  SetCursors(-1);
}


/***************************************************************/
static void genIcon(br, bf)
     BROWINFO *br;
     BFIL *bf;
{
  /* given a BFIL entry, load up the file.
   * if we succeeded in loading up the file,
   *      generate an aspect-correct 8-bit image using brow Cmap
   * otherwise
   *      replace this icon with the BF_UNKNOWN, or BF_ERR icons
   */

  PICINFO pinfo;
  int     i, filetype;
  double  wexpand,hexpand;
  int     iwide, ihigh;
  byte   *icon24, *icon8;
  char    str[256], str1[256], readname[128], uncompname[128];
  char    basefname[128], *uncName;


  if (!bf || !bf->name || bf->name[0] == '\0') return;   /* shouldn't happen */
  str[0] = '\0';
  basefname[0] = '\0';
  pinfo.pic = (byte *) NULL;
  pinfo.comment = (char *) NULL;
  strncpy(readname, bf->name, sizeof(readname) - 1);

  /* free any old info in 'bf' */
  if (bf->imginfo) free          (bf->imginfo);
  if (bf->pimage)  free          (bf->pimage);
  if (bf->ximage)  xvDestroyImage(bf->ximage);

  bf->imginfo = (char *)   NULL;
  bf->pimage  = (byte *)   NULL;
  bf->ximage  = (XImage *) NULL;


  /* skip all 'special' files */
  if (!ISLOADABLE(bf->ftype)) return;

  filetype = ReadFileType(bf->name);

  if ((filetype == RFT_COMPRESS) || (filetype == RFT_BZIP2)) {
#if (defined(VMS) && !defined(GUNZIP))
    /* VMS decompress doesn't like the file to have a trailing .Z in fname
       however, GUnZip is OK with it, which we are calling UnCompress */
    strcpy (basefname, bf->name);
    *rindex (basefname, '.') = '\0';
    uncName = basefname;
#else
    uncName = bf->name;
#endif

    if (UncompressFile(uncName, uncompname, filetype)) {
      filetype = ReadFileType(uncompname);
      strncpy(readname, uncompname, sizeof(readname) - 1);
    }
    else {
      sprintf(str, "Couldn't uncompress file '%s'", bf->name);
      setBrowStr(br, str);
      bf->ftype = BF_ERROR;
    }
  }

#ifdef MACBINARY
  if (handlemacb && macb_file == True && bf->ftype != BF_ERROR) {
    if (RemoveMacbinary(readname, uncompname)) {
      if (strcmp(readname, bf->name)!=0) unlink(readname);
      strncpy(readname, uncompname, sizeof(readname) - 1);
    }
    else {
      sprintf(str, "Unable to remove a InfoFile header form '%s'.", bf->name);
      setBrowStr(br, str);
      bf->ftype = BF_ERROR;
    }
  }
#endif

#ifdef HAVE_MGCSFX_AUTO
  if (bf->ftype != BF_ERROR) {
    if(filetype == RFT_MGCSFX){
      char tmpname[128];
      char *icom;

      if((icom = mgcsfx_auto_input_com(bf->name)) != NULL){
	sprintf(tmpname, "%s/xvmsautoXXXXXX", tmpdir);
#ifdef USE_MKSTEMP
	close(mkstemp(tmpname));
#else
	mktemp(tmpname);
#endif
	SetISTR(ISTR_INFO, "Converting to known format by MgcSfx auto...");
	sprintf(str,"%s >%s", icom, tmpname);
      }else goto ms_auto_no;

#ifndef VMS
      if (system(str))
#else
      if (!system(str))
#endif
      {
        sprintf(str, "Unable to convert '%s' by MgcSfx auto.", bf->name);
        setBrowStr(br, str);
        bf->ftype = BF_ERROR;
      }
      else {
        filetype = ReadFileType(tmpname);
        if (strcmp(readname, bf->name)!=0) unlink(readname);
        strncpy(readname, tmpname, sizeof(readname) - 1);
      }
    }
  }
ms_auto_no:
#endif /* HAVE_MGCSFX_AUTO */

  /* get rid of comments.  don't need 'em */
  if (pinfo.comment) free(pinfo.comment);  pinfo.comment = (char *) NULL;

  if (filetype == RFT_ERROR) {
    sprintf(str,"Couldn't open file '%s'", bf->name);
    setBrowStr(br, str);
    bf->ftype = BF_ERROR;
  }

  else if (filetype == RFT_UNKNOWN) {
    /* if it *was* an 'exe', leave it that way */
    if (bf->ftype != BF_EXE) bf->ftype = BF_UNKNOWN;
  }

  else {
    /* otherwise it's a known filetype... do the *hard* part now... */

#ifdef VS_ADJUST
    normaspect = defaspect;
#endif
    i = ReadPicFile(readname, filetype, &pinfo, 1);
    KillPageFiles(pinfo.pagebname, pinfo.numpages);

    if (!i) bf->ftype = BF_ERROR;

    if (i && (pinfo.w<=0 || pinfo.h<=0)) {        /* bogus size */
      bf->ftype = BF_ERROR;
      free(pinfo.pic);  pinfo.pic = (byte *) NULL;
    }

    if (bf->ftype==BF_ERROR && filetype==RFT_XBM) bf->ftype = BF_UNKNOWN;
  }

  /* get rid of comment, as we don't need it */
  if (pinfo.comment) {
    free(pinfo.comment);  pinfo.comment = (char *) NULL;
  }

  /* if we made an uncompressed file, we can rm it now */
  if (strcmp(readname, bf->name)!=0) unlink(readname);


  /* at this point either BF_ERROR, BF_UNKNOWN, BF_EXE or pic */

  if (!pinfo.pic) {
    if (bf->ftype == BF_EXE) return;  /* don't write thumbfiles for exe's */

    bf->w = br_file_width;  bf->h = br_file_height;
    writeThumbFile(br, bf, NULL, 0, 0, NULL);   /* BF_ERROR, BF_UNKNOWN */
    return;
  }

  /* at this point, we have a pic, so it must be an image file */


  /* compute size of icon  (iwide,ihigh) */

#ifdef VS_ADJUST
  if (!vsadjust) normaspect = 1;

  wexpand = (double) (pinfo.w * normaspect) / (double) ISIZE_WIDE;
#else
  wexpand = (double) pinfo.w / (double) ISIZE_WIDE;
#endif /* VS_ADJUST */
  hexpand = (double) pinfo.h / (double) ISIZE_HIGH;

  if (wexpand >= 1.0 || hexpand >= 1.0) {   /* don't expand small icons */
    if (wexpand>hexpand) {
#ifdef VS_ADJUST
      iwide = (int) ((pinfo.w * normaspect) / wexpand + 0.5);
#else
      iwide = (int) (pinfo.w / wexpand + 0.5);
#endif
      ihigh = (int) (pinfo.h / wexpand + 0.5);
    }
    else {
#ifdef VS_ADJUST
      iwide = (int) ((pinfo.w * normaspect) / hexpand + 0.5);
#else
      iwide = (int) (pinfo.w / hexpand + 0.5);
#endif
      ihigh = (int) (pinfo.h / hexpand + 0.5);
    }
  }
  else {  /* smaller than ISIZE.  Leave it that way. */
    iwide = pinfo.w;  ihigh = pinfo.h;
  }


  /* generate icon */
  icon24 = Smooth24(pinfo.pic, pinfo.type==PIC24, pinfo.w, pinfo.h,
		    iwide, ihigh, pinfo.r,pinfo.g,pinfo.b);
  if (!icon24) { bf->ftype = BF_FILE;  free(pinfo.pic); return; }

  sprintf(str, "%dx%d ", pinfo.normw, pinfo.normh);
  switch (filetype) {
  case RFT_GIF:      if (xv_strstr(pinfo.shrtInfo, "GIF89"))
                       strcat(str,"GIF89 file");
                     else
		       strcat(str,"GIF87 file");
                     break;

  case RFT_PM:       strcat(str,"PM file");               break;

  case RFT_PBM:      if (xv_strstr(pinfo.fullInfo, "raw")) strcat(str,"Raw ");
                     else strcat(str,"Ascii ");

                     for (i=0; i<3 && (strlen(pinfo.fullInfo)>(size_t)3); i++){
		       str1[0] = pinfo.fullInfo[i];  str1[1] = '\0';
		       strcat(str, str1);
		     }

                     strcat(str," file");
                     break;

  case RFT_XBM:      strcat(str,"X11 bitmap file");       break;
  case RFT_SUNRAS:   strcat(str,"Sun rasterfile");        break;
  case RFT_BMP:      strcat(str,"BMP file");              break;
  case RFT_UTAHRLE:  strcat(str,"Utah RLE file");         break;
  case RFT_IRIS:     strcat(str,"Iris RGB file");         break;
  case RFT_PCX:      strcat(str,"PCX file");              break;
  case RFT_JFIF:     strcat(str,"JPEG file");             break;
  case RFT_TIFF:     strcat(str,"TIFF file");             break;
  case RFT_PDSVICAR: strcat(str,"PDS/VICAR file");        break;
  case RFT_PS:       strcat(str,"PostScript file");       break;
  case RFT_IFF:      strcat(str,"ILBM file");             break;
  case RFT_TARGA:    strcat(str,"Targa file");            break;
  case RFT_XPM:      strcat(str,"XPM file");              break;
  case RFT_XWD:      strcat(str,"XWD file");              break;
  case RFT_FITS:     strcat(str,"FITS file");             break;
  case RFT_PNG:      strcat(str,"PNG file");              break;
  case RFT_ZX:       strcat(str,"Spectrum SCREEN$");      break; /* [JCE] */
  case RFT_PCD:      strcat(str,"PhotoCD file");          break;
  case RFT_MAG:      strcat(str,"MAG file");              break;
  case RFT_MAKI:     strcat(str,"MAKI file");             break;
  case RFT_PIC:      strcat(str,"PIC file");              break;
  case RFT_PI:       strcat(str,"PI file");               break;
  case RFT_PIC2:     strcat(str,"PIC2 file");             break;
  case RFT_MGCSFX:   strcat(str,"Magic Suffix file");     break;
  default:           strcat(str,"file of unknown type");  break;
  }


  /* find out length of original file */
  {  FILE *fp;
     long  filesize;
     char  buf[64];

     fp = fopen(bf->name, "r");
     if (fp) {
       fseek(fp, 0L, 2);
       filesize = ftell(fp);
       fclose(fp);

       sprintf(buf,"  (%ld bytes)", filesize);
       strcat(str, buf);
     }
   }


  sprintf(str1, "%s:  %s", bf->name, str);
  setBrowStr(br, str1);

  /* dither 24-bit icon into 8-bit icon (using 3/3/2 cmap) */
  icon8 = DoColorDither(icon24, NULL, iwide, ihigh, NULL, NULL, NULL,
			browR, browG, browB, 256);
  if (!icon8) { bf->ftype = BF_FILE;  free(icon24); free(pinfo.pic); return; }

  writeThumbFile(br, bf, icon8, iwide, ihigh, str);

  /* have to make a *copy* of str */
  if (strlen(str)) {
    bf->imginfo = (char *) malloc(strlen(str)+1);
    if (bf->imginfo) strcpy(bf->imginfo, str);
  }
  else bf->imginfo = (char *) NULL;

  bf->pimage  = icon8;
  bf->w       = iwide;
  bf->h       = ihigh;
  bf->ftype   = BF_HAVEIMG;

  bf->ximage = Pic8ToXImage(icon8, (u_int) iwide, (u_int) ihigh, browcols,
			    browR, browG, browB);

  free(icon24);
  free(pinfo.pic);
}






/*
 *  THUMBNAIL FILE FORMAT:
 *
 * <magic number 'P7 332' >
 * <comment identifying version of XV that wrote this file>
 * <comment identifying type & size of the full-size image>
 * <OPTIONAL comment identifying this as a BUILT-IN icon, in which case
 *    there is no width,height,maxval info, nor any 8-bit data>
 * <comment signifying end of comments>
 * <width, height, and maxval of this file >
 * <raw binary 8-bit data, in 3/3/2 Truecolor format>
 *
 * Example:
 *    P7 332
 *    #XVVERSION:Version 2.28  Rev: 9/26/92
 *    #IMGINFO:512x440 Color JPEG
 *    #END_OF_COMMENTS
 *    48 40 255
 *    <binary data>
 *
 * alternately:
 *    P7 332
 *    #XVVERSION:Version 2.28 Rev: 9/26/92
 *    #BUILTIN:UNKNOWN
 *    #IMGINFO:
 *    #END_OF_COMMENTS
 */



/***************************************************************/
static void loadThumbFile(br, bf)
     BROWINFO *br;
     BFIL *bf;
{
  /* determine if bf has an associated thumbnail file.  If so, load it up,
     and create the ximage, and such */

  FILE *fp;
  char  thFname[512];
  char  buf[256], *st, *info;
  int   w,h,mv,i,builtin;
  byte *icon8;

  info = NULL;  icon8 = NULL;  builtin = 0;

  sprintf(thFname, "%s%s/%s", br->path, THUMBDIR, bf->name);

#ifdef AUTO_EXPAND
  Dirtovd(thFname);
#endif

  fp = fopen(thFname, "r");
  if (!fp) return;            /* nope, it doesn't have one */

  /* read in the file */
  if (!fgets(buf, 256, fp)) goto errexit;

  if (strncmp(buf, "P7 332", (size_t) 6)) goto errexit;


  /* read comments until we see '#END_OF_COMMENTS', or hit EOF */
  while (1) {
    if (!fgets(buf, 256, fp)) goto errexit;

    if      (!strncmp(buf, "#END_OF_COMMENTS", strlen("#END_OF_COMMENTS")))
      break;

    else if (!strncmp(buf, "#XVVERSION:", strlen("#XVVERSION:"))) {
      /* probably should check for compatibility, or something... */
    }

    else if (!strncmp(buf, "#BUILTIN:", strlen("#BUILTIN:"))) {
      builtin = 1;
      st = (char *) index(buf, ':') + 1;
      if (strcmp(st, "ERROR")==0)   bf->ftype = BF_ERROR;
      else bf->ftype = BF_UNKNOWN;
    }

    else if (!strncmp(buf, "#IMGINFO:", strlen("#IMGINFO:"))) {
      st = (char *) index(buf, ':') + 1;
      info = (char *) malloc(strlen(st) + 1);
      if (info) strcpy(info, st);
    }
  }


  if (builtin) {
    bf->imginfo = info;
    fclose(fp);
    return;
  }



  /* read width, height, maxval */
  if (!fgets(buf, 256, fp) || sscanf(buf, "%d %d %d", &w, &h, &mv) != 3)
    goto errexit;


  if (w>ISIZE_WIDE || h>ISIZE_HIGH || w<1 || h<1 || mv != 255) {
    sprintf(buf,"Bogus thumbnail file for '%s'.  Skipping.", bf->name);
    setBrowStr(br, buf);
    goto errexit;
  }


  /* read binary data */
  icon8 = (byte *) malloc((size_t) w * h);
  if (!icon8) goto errexit;

  i = fread(icon8, (size_t) 1, (size_t) w*h, fp);
  if (i != w*h) goto errexit;

  if (icon8) {
    bf->pimage  = icon8;
    bf->w       = w;
    bf->h       = h;
    bf->ftype   = BF_HAVEIMG;
    bf->imginfo = info;

    bf->ximage = Pic8ToXImage(icon8, (u_int) w, (u_int) h, browcols,
			      browR, browG, browB);
  }
  else {
    if (info) free(info);
  }

  fclose(fp);
  return;


 errexit:
  fclose(fp);
  if (info) free(info);
  if (icon8) free(icon8);
}



/***************************************************************/
static void writeThumbFile(br, bf, icon8, w, h, info)
     BROWINFO *br;
     BFIL *bf;
     byte *icon8;
     int   w,h;
     char *info;
{
  FILE *fp;
  char  thFname[512], buf[256];
  int   i, perm;
  struct stat st;


  makeThumbDir(br);


  /* stat the original file, get permissions for thumbfile */
  sprintf(thFname, "%s%s", br->path, bf->name);
  i = stat(thFname, &st);
  if (!i) perm = st.st_mode & 07777;
     else perm = 0755;



  sprintf(thFname, "%s%s/%s", br->path, THUMBDIR, bf->name);

#ifdef AUTO_EXPAND
  Dirtovd(thFname);
#endif

  unlink(thFname);  /* just in case there's already an unwritable one */
  fp = fopen(thFname, "w");
  if (!fp) {
    sprintf(buf, "Can't create thumbnail file '%s':  %s", thFname,
	    ERRSTR(errno));
    setBrowStr(br, buf);
    return;            /* can't write... */
  }


  /* write the file */
  fprintf(fp, "P7 332\n");
  fprintf(fp, "#XVVERSION:%s\n", REVDATE);

  if (icon8) {
    fprintf(fp, "#IMGINFO:%s\n", (info) ? info : "");
  }
  else {
    fprintf(fp, "#BUILTIN:");
    switch (bf->ftype) {
    case BF_ERROR:   fprintf(fp,"ERROR\n");    break;
    case BF_UNKNOWN: fprintf(fp,"UNKNOWN\n");  break;
    default:         fprintf(fp,"UNKNOWN\n");  break;
    }

    fprintf(fp, "#IMGINFO:%s\n", (info) ? info : "");
  }

  fprintf(fp, "#END_OF_COMMENTS\n");

  if (icon8) {
    fprintf(fp, "%d %d %d\n", w, h, 255);

    /* write the raw data */
    fwrite(icon8, (size_t) 1, (size_t) w*h, fp);
  }

  if (ferror(fp)) {  /* error occurred */
    fclose(fp);
    unlink(thFname);  /* delete it */
    sprintf(buf, "Can't write thumbnail file '%s':  %s", thFname,
	    ERRSTR(errno));
    setBrowStr(br, buf);
    return;            /* can't write... */
  }

  fclose(fp);

  chmod(thFname, (mode_t) perm);
}


/***************************************************************/
static void makeThumbDir(br)
     BROWINFO *br;
{
  char  thFname[512];
  int i, perm;
  struct stat st;

  /* stat the THUMBDIR directory:  if it doesn't exist, and we are not
     already in a THUMBDIR, create it */

  sprintf(thFname, "%s%s", br->path, THUMBDIRNAME);

#ifdef AUTO_EXPAND
  Dirtovd(thFname);
#endif

  i = stat(thFname, &st);
  if (i) {                      /* failed, let's create it */
    sprintf(thFname, "%s.", br->path);
#ifdef AUTO_EXPAND
    Dirtovd(thFname);
#endif
    i = stat(thFname, &st);     /* get permissions of parent dir */
    if (!i) perm = st.st_mode & 07777;
       else perm = 0755;

    sprintf(thFname, "%s%s", br->path, THUMBDIRNAME);
#ifdef AUTO_EXPAND
    Dirtovd(thFname);
#endif
    i = mkdir(thFname, (mode_t) perm);
#ifdef VIRTUAL_TD
    if (i < 0)
      Mkvdir_force(thFname);
#endif
  }
}



/***************************************************************/
static void updateIcons(br)
     BROWINFO *br;
{
  /* for each file in the bfList, see if it has an icon file.
   *    if it doesn't, generate one
   *    if it does, check the dates.  If the pic file is newer, regen icon
   *
   * for each file in the current directory's thumbnail directory,
   *   see if there's a corresponding pic file.  If not, delete the
   *   icon file
   */

  int            i, iconsBuilt, iconsKilled, statcount;
  char           tmpstr[128];
  BFIL          *bf;
  DIR           *dirp;
#ifdef NODIRENT
  struct direct *dp;
#else
  struct dirent *dp;
#endif


  iconsBuilt = iconsKilled = statcount = 0;

  makeThumbDir(br);

  /* okay, we're in the right directory.  run through the bfList, and look
     for corresponding thumbnail files */

  WaitCursor();

  for (i=0, bf=br->bfList; i<br->bfLen; i++, bf++) {
    if (bf->ftype <= BF_FILE || bf->ftype >= BF_ERROR || bf->ftype==BF_EXE) {

      /* i.e., not a 'special' file */

      int  s1, s2;
      char thfname[256];
      struct stat filest, thumbst;

      drawTemp(br, i, br->bfLen);

      s1 = stat(bf->name, &filest);

      /* see if this file has an associated thumbnail file */
      sprintf(thfname, "%s/%s", THUMBDIR, bf->name);
      s2 = stat(thfname, &thumbst);

      if (s1 || s2 || filest.st_mtime > thumbst.st_mtime) {
	/* either stat'ing the file or the thumbfile failed, or
	   both stat's succeeded and the file has a newer mod
	   time than the thumbnail file */

	makeIconVisible(br, i);
	eraseIcon(br, i);
	genIcon(br, bf);
	drawIcon(br, i);

	if (bf->ftype != BF_EXE) {
	  iconsBuilt++;
	  if (DEBUG)
	    fprintf(stderr,"icon made:fname='%s' thfname='%s' %d,%d,%ld,%ld\n",
		    bf->name, thfname, s1, s2,
		    (long)filest.st_mtime, (long)thumbst.st_mtime);
	}
      }
      else if (filest.st_ctime > thumbst.st_ctime) {
        /* update protections */
        chmod(thfname, (mode_t) (filest.st_mode & 07777));
      }
    }
    statcount++;

    if ((statcount % 30)==0) WaitCursor();

    if ((statcount  && (statcount % 100)==0) ||
	(iconsBuilt && (iconsBuilt % 20)==0)) {

      sprintf(tmpstr, "Processed %d out of %d...", i+1, br->bfLen);
      setBrowStr(br, tmpstr);
    }
  }

  clearTemp(br);



  /* search the THUMBDIR directory, looking for thumbfiles that don't have
     corresponding pic files.  Delete those. */

  setBrowStr(br, "Scanning for excess icon files...");

  statcount = 0;
  dirp = opendir(THUMBDIR);
  if (dirp) {
    while ( (dp = readdir(dirp)) != NULL) {
      char thfname[256];
      struct stat filest, thumbst;

      /* stat this directory entry to make sure it's a plain file */
      sprintf(thfname, "%s/%s", THUMBDIR, dp->d_name);
      if (stat(thfname, &thumbst)==0) {  /* success */
	int tmp;
#ifdef AUTO_EXPAND
	tmp  = stat2bf((u_int) thumbst.st_mode , thfname);
#else
	tmp  = stat2bf((u_int) thumbst.st_mode);
#endif

	if (tmp == BF_FILE) {  /* a plain file */
	  /* see if this thumbfile has an associated pic file */
	  if (stat(dp->d_name, &filest)) {  /* failed!: guess it doesn't */
	    if (unlink(thfname)==0) iconsKilled++;
	  }
	}
      }
      statcount++;

      if ((statcount % 30)==0) WaitCursor();
    }
    closedir(dirp);
  }

  SetCursors(-1);

  sprintf(tmpstr, "Update finished:  %d icon%s created, %d icon%s deleted.",
	  iconsBuilt,  (iconsBuilt ==1) ? "" : "s",
	  iconsKilled, (iconsKilled==1) ? "" : "s");
  setBrowStr(br, tmpstr);

  drawIconWin(0, &(br->scrl));   /* redraw icon window */
}


/*******************************************/
static void drawTemp(br, cnt, maxcnt)
     BROWINFO *br;
     int       cnt, maxcnt;
{
  if (maxcnt<1) return;   /* none of that naughty ol' divide by zero stuff */

  DrawTempGauge(br->win, 5, br->dirMB.y,
		(int) br->dirMB.x-10, (int) br->dirMB.h,
		(double) cnt / (double) maxcnt,
		browfg, browbg, browhi, browlo, "");
}

static void clearTemp(br)
  BROWINFO *br;
{
  XClearArea(theDisp, br->win, 5, br->dirMB.y,
	     (u_int) br->dirMB.x-10+1, (u_int) br->dirMB.h + 1, True);
}




/*******************************************/
static void doTextCmd(br)
     BROWINFO *br;
{
  int i;

  if (!br->bfLen || !br->bfList || !br->numlit) return;

  for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);
  if (i==br->bfLen) return;    /* shouldn't happen */

  if (cdBrow(br)) return;
  TextView(br->bfList[i].name);
}


/*******************************************/
static void doRenameCmd(br)
     BROWINFO *br;
{
  /* called when one (and *only* one!) item is lit in the current br.
     pops up a 'what do you want to rename it to' box, and attempts to
     do the trick... */

  int                i, num;
  char               buf[128], txt[256], *origname, txt1[256];
  static const char *labels[] = { "\nOk", "\033Cancel" };
  struct stat        st;

#ifdef AUTO_EXPAND
  if (Isvdir(br->path)) {
    sprintf(buf,"Sorry, you can't rename file in the virtual directory, '%s'",
	    br->path);
    ErrPopUp(buf, "\nBummer!");
    return;
  }
#endif

  if (cdBrow(br)) return;

  /* find the selected file */
  for (i=0; i<br->bfLen && !br->bfList[i].lit; i++);
  if (i==br->bfLen) return;    /* shouldn't happen */

  origname = br->bfList[i].name;   num = i;

  if (strcmp(origname, "..")==0) {
    sprintf(buf,"Sorry, you can't rename the parent directory, %s",
	    "for semi-obvious reasons.");
    ErrPopUp(buf, "\nRight.");
    return;
  }

  sprintf(txt, "Enter a new name for the %s '%s':",
	  (br->bfList[i].ftype==BF_DIR) ? "directory" : "file",
	  origname);

  strcpy(buf, origname);
  i = GetStrPopUp(txt, labels, 2, buf, 128, "/ |\'\"<>,", 0);
  if (i) return;     /* cancelled */


  if (strcmp(origname, buf)==0) return;


  /* see if the desired file exists, and attempt to do the rename if
     it doesn't.  On success, free bfList[].name, realloc with the new name,
     and redraw that icon.  If any other br's are pointed to same dir,
     do rescan's in them */

  /* this will also pick up mucking around with '.' and '..' */
  if (stat(buf, &st) == 0) {   /* successful stat:  new name already exists */
    sprintf(txt,"Sorry, a file or directory named '%s' already exists.",buf);
    ErrPopUp(txt, "\nOh!");
    return;
  }

  /* try to rename the file */
  if (rename(origname, buf) < 0) {
    sprintf(txt, "Error renaming '%s' to '%s':  %s",
	    origname, buf, ERRSTR(errno));
    ErrPopUp(txt, "\nSo what!");
    return;
  }


  /* try to rename it's thumbnail file, if any.  Ignore errors */
  sprintf(txt,  "%s/%s", THUMBDIR, origname);
  sprintf(txt1, "%s/%s", THUMBDIR, buf);
  rename(txt, txt1);



  free(br->bfList[num].name);
  br->bfList[num].name = (char *) malloc(strlen(buf) + 1);
  if (br->bfList[num].name) strcpy(br->bfList[num].name, buf);
                       else FatalError("out of memory in doRenameCmd");

  eraseIconTitle(br, num);
  drawIcon(br, num);

  for (i=0; i<MAXBRWIN; i++) {
    if (&binfo[i] != br && strcmp(binfo[i].path, br->path)==0)
      rescanDir(&binfo[i]);
  }

  DIRCreatedFile(br->path);
}




/*******************************************/
static void doMkdirCmd(br)
     BROWINFO *br;
{
  /* called at any time (doesn't have anything to do with current selection)
     pops up a 'what do you want to call it' box, and attempts to
     do the trick... */

  int                i;
  char               buf[128], txt[256];
  static const char *labels[] = { "\nOk", "\033Cancel" };
  struct stat        st;

#ifdef AUTO_EXPAND
  if (Isvdir(br->path)) {
    sprintf(buf,"Sorry, you can't mkdir in the virtual directory, '%s'",
	    br->path);
    ErrPopUp(buf, "\nBummer!");
    return;
  }
#endif

  if (cdBrow(br)) return;

  buf[0] = '\0';
  i = GetStrPopUp("Enter name for new directory:", labels, 2,
		  buf, 128, "/ |\'\"<>,", 0);
  if (i) return;     /* cancelled */

  if (strlen(buf)==0) return;    /* no name entered */

  /* make sure they haven't tried to create '.' or '..' (can't be filtered) */
  /* see if the file exists already, complain and abort if it does */

  if (strcmp(buf,".")==0 || strcmp(buf,"..")==0 ||
      stat(buf, &st)==0) {
    sprintf(txt,"Sorry, a file or directory named '%s' already exists.",buf);
    ErrPopUp(txt, "\nZoinks!");
    return;
  }


  /* if it doesn't, do the mkdir().  On success, need to do a rescan of
     cwd, and any other br's pointing at same directory */
  if (mkdir(buf, 0755) < 0) {
    sprintf(txt, "Error creating directory '%s':  %s", buf, ERRSTR(errno));
    ErrPopUp(txt, "\nEat me!");
    return;
  }


  /* rescan current br, and all other br's pointing to same directory */
  for (i=0; i<MAXBRWIN; i++) {
    if (strcmp(binfo[i].path, br->path)==0)
      rescanDir(&binfo[i]);
  }

  DIRCreatedFile(br->path);
}





/*******************************************/
static void doChdirCmd(br)
     BROWINFO *br;
{
  int                i;
  static char        buf[MAXPATHLEN+100];
  static const char *labels[] = { "\nOk", "\033Cancel" };
  char               str[512];

  buf[0] = '\0';
  i = GetStrPopUp("Change to directory:", labels, 2, buf, MAXPATHLEN, " ", 0);
  if (i) return;		/* cancelled */

#ifndef VMS
  if (Globify(buf)) {		/* do ~ expansion if necessary */
    sprintf(str,"Unable to expand '%s' (unknown uid)\n", buf);
    setBrowStr(br, str);
    XBell(theDisp, 50);
    return;
  }
#endif

  if (buf[0] == '.') {		/* chdir to relative dir */
    if (cdBrow(br)) return;     /* prints its own error message */
  }

#ifdef AUTO_EXPAND
  if (Chvdir(buf)) {
#else
  if (chdir(buf)) {
#endif
    sprintf(str,"Unable to cd to '%s'\n", buf);
    setBrowStr(br, str);
    XBell(theDisp, 50);
  }
  else {
#ifdef AUTO_EXPAND
      if (Isvdir(buf)) {
	BTSetActive(&br->but[BR_DELETE],  0);
	br->cmdMB.dim[BR_DELETE] = 1;

	BTSetActive(&br->but[BR_RENAME],  0);
	br->cmdMB.dim[BR_RENAME] = 1;

	BTSetActive(&br->but[BR_MKDIR],  0);
	br->cmdMB.dim[BR_MKDIR] = 1;
      }
      else {
	BTSetActive(&br->but[BR_MKDIR],  1);
	br->cmdMB.dim[BR_MKDIR] = 0;
      }
#endif
    scanDir(br);
    SCSetVal(&(br->scrl), 0);	/* reset to top on a chdir */
    restIconVisible(br);
  }
}



/*******************************************/
static void doDeleteCmd(br)
     BROWINFO *br;
{
  /* if '..' is lit, turn it off and mention that you can't delete it.
   *
   * count # of lit files and lit directories.  Prompt with an
   * appropriate 'Are you sure?' box
   *
   * if we're proceeding, delete the non-dir files, and their thumbnail
   *    buddies, if any (by calling rm_file() )
   * call 'rm_dir()' for each of the directories
   */

  BFIL              *bf;
  int                i, numdirs, numfiles, slen, firstdel;
  char               buf[512];
  static const char *yesno[]  = { "\004Delete", "\033Cancel" };

#ifdef AUTO_EXPAND
  if (Isvdir(br->path)) {
    sprintf(buf,"Sorry, you can't delete file at the virtual directory, '%s'",
	    br->path);
    ErrPopUp(buf, "\nBummer!");
    return;
  }
#endif

  if (!br->bfLen || !br->bfList || !br->numlit) return;

  if (cdBrow(br)) return;     /* can't cd to this directory.  screw it! */

  if (br->bfList[0].lit && strcmp(br->bfList[0].name,"..")==0) {
    br->numlit--;
    br->bfList[0].lit = 0;
    changedNumLit(br, -1, 0);

    sprintf(buf,"Sorry, but you can't delete the parent directory, %s",
	    "for semi-obvious reasons.");
    ErrPopUp(buf, "\nRight.");
    if (!br->numlit) {               /* turned off only lit file.  return */
      drawIcon(br, 0);
      return;
    }
  }

  numdirs = numfiles = 0;  firstdel = -1;
  for (i=0, bf=br->bfList; i<br->bfLen; i++,bf++) {
    if (bf->lit) {
      if (firstdel == -1) firstdel = i;
      if (bf->ftype == BF_DIR
#ifdef AUTO_EXPAND
	  && (!Isarchive(bf->name))
#endif
			     ) numdirs++;
      else numfiles++;
    }
  }


  /* if any plain files are being toasted, bring up the low-key
     confirmation box */

  if (numfiles) {
    sprintf(buf,"Delete file%s:  ", numfiles>1 ? "s" : "");
    slen = strlen(buf);

    for (i=0, bf=br->bfList;  i<br->bfLen;  i++,bf++) {
#ifdef AUTO_EXPAND
      if (bf->lit && (bf->ftype != BF_DIR || Isarchive(bf->name))) {
#else
      if (bf->lit && bf->ftype != BF_DIR) {
#endif

	if ( (slen + strlen(bf->name) + 1) > 256) {
	  strcat(buf,"...");
	  break;
	}
	else {
	  strcat(buf, bf->name);   slen += strlen(bf->name);
	  strcat(buf, " ");        slen++;
	}
      }
    }

    i = PopUp(buf, yesno, COUNT(yesno));
    if (i) return;              /* cancelled */
  }


  /* if any directories are being toasted, bring up the are you REALLY sure
     confirmation box */

  if (numdirs) {
    sprintf(buf,"Recursively delete director%s:  ", numdirs>1 ? "ies" : "y");
    slen = strlen(buf);

    for (i=0, bf=br->bfList;  i<br->bfLen;  i++,bf++) {
#ifdef AUTO_EXPAND
      if (bf->lit && (bf->ftype == BF_DIR || !Isarchive(bf->name))) {
#else
      if (bf->lit && bf->ftype == BF_DIR) {
#endif
	if ( (slen + strlen(bf->name) + 1) > 256) {
	  strcat(buf,"...");
	  break;
	}
	else {
	  strcat(buf, bf->name);   slen += strlen(bf->name);
	  strcat(buf, " ");        slen++;
	}
      }
    }

    i = PopUp(buf, yesno, COUNT(yesno));
    if (i) return;              /* cancelled */
  }


  /* okay, at this point they've been warned.  do the deletion */

  for (i=0, bf=br->bfList;  i<br->bfLen;  i++,bf++) {
    if (bf->lit) {
      if (bf->ftype == BF_DIR
#ifdef AUTO_EXPAND
	  && !Isarchive(bf->name)
#endif
			     ) rm_dir (br, bf->name);
                          else rm_file(br, bf->name);
    }
  }

  /* rescan br, as it's probably changed */
  rescanDir(br);

  /* recompute br->numlit */
  for (i=br->numlit=0; i<br->bfLen; i++) {
    if (br->bfList[i].lit) br->numlit++;
  }
  changedNumLit(br, -1, 0);


  /* if deleted a single file, select the icon that's in the same position */
  if (numfiles + numdirs >= 1) {
    if (!br->bfList[firstdel].lit) {
      br->bfList[firstdel].lit = 1;
      br->numlit++;
      drawIcon(br, firstdel);
      makeIconVisible(br, firstdel);
      changedNumLit(br, firstdel, 0);
    }
  }

  /* rescan other br's that are looking at this directory */
  for (i=0; i<MAXBRWIN; i++) {
    if (&binfo[i] != br && strcmp(binfo[i].path, br->path)==0)
      rescanDir(&binfo[i]);
  }

  DIRDeletedFile(br->path);
}



/*******************************************/
static void doSelFilesCmd(br)
     BROWINFO *br;
{
  int                i;
  static char        buf[MAXPATHLEN+100];
  static const char *labels[] = { "\nOk", "\033Cancel" };
  char               str[512];

  buf[0] = '\0';
  strcpy(str,"Select file name(s).  Wildcard '*' is allowed.  ");
  strcat(str,"Previously selected files will remain selected.");

  i = GetStrPopUp(str, labels, 2, buf, MAXPATHLEN, "", 0);
  if (i) return;		/* cancelled */

  for (i=0; i<br->bfLen; i++) {
    if (strcmp(br->bfList[i].name, "..")==0) continue;    /* skip '..' */

    if (selmatch(br->bfList[i].name, buf)) {
      br->bfList[i].lit = 1;
      drawIcon(br,i);
    }
  }

  /* recount numlit */
  br->numlit = 0;
  for (i=0; i<br->bfLen; i++) {
    if (br->bfList[i].lit) br->numlit++;
  }
  changedNumLit(br, -1, 0);
}



/*******************************************/

static char *dirStack[128];
static int   dirStackLen;


/*******************************************/
static void doRecurseCmd(br)
     BROWINFO *br;
{
  int                i;
  static const char *labels[] = { "\nOk", "\033Cancel" };
  char               str[512];

  strcpy(str,"Recursive Update:  This could take *quite* a while.\n");
  strcat(str,"Are you sure?");

  i = PopUp(str, labels, 2);
  if (i) return;		/* cancelled */


  /* initialize dirname list */
  dirStackLen = 0;

  cdBrow(br);
  SCSetVal(&(br->scrl),0);
  recurseUpdate(br, ".");
}


/*******************************************/
static void recurseUpdate(br, subdir)
     BROWINFO   *br;
     const char *subdir;
{
  /* note:  'br->path + subdir' is the full path to recurse down from */

  /* save current directory, so we can restore upon exit
   *
   * build new dest directory, cd there, get working directory (which
   * shouldn't have symlink names in it).  If this dir is in dirstack,
   * we've looped:  cd to orig dir and return
   * otherwise:  load 'br' to reflect new dir, do an Update(),
   *      and for each subdir in this dir, recurse
   *
   * if cur dir != orig dir, cd back to orig dir and reload 'br'
   */

  int  i;
  char orgDir[MAXPATHLEN + 2];
  char curDir[MAXPATHLEN + 2];
  char *sp;
  BFIL *bf;

  xv_getwd(orgDir, sizeof(orgDir));

  sprintf(curDir, "%s%s", br->path, subdir);
#ifdef AUTO_EXPAND
  if (Chvdir(curDir)) {
#else
  if (chdir(curDir)) {
#endif
    char str[512];
    sprintf(str, "Unable to cd to '%s'\n", curDir);
    setBrowStr(br, str);
    return;
  }

  xv_getwd(curDir, sizeof(curDir));

  /* have we looped? */
  for (i=0; i<dirStackLen && strcmp(curDir, dirStack[i]); i++);
  if (i<dirStackLen) {   /* YES */
#ifdef AUTO_EXPAND
    Chvdir(orgDir);
#else
    chdir(orgDir);
#endif
    restIconVisible(br);
    return;
  }

  sp = (char *) malloc((size_t) strlen(curDir) + 1);
  if (!sp) {
    setBrowStr(br, "malloc() error in recurseUpdate()\n");
#ifdef AUTO_EXPAND
    Chvdir(orgDir);
#else
    chdir(orgDir);
#endif
    restIconVisible(br);
    return;
  }

  strcpy(sp, curDir);
  dirStack[dirStackLen++] = sp;

  if (DEBUG) {
    fprintf(stderr,"------\n");
    for (i=dirStackLen-1; i>=0; i--) fprintf(stderr,"  %s\n", dirStack[i]);
    fprintf(stderr,"------\n");
  }


  /* do this directory */
  scanDir(br);
  updateIcons(br);

  /* do subdirectories of this directory, not counting .  .. and .xvpics */
  for (i=0; i<br->bfLen; i++) {
    bf = &(br->bfList[i]);
    if (bf                     &&
	bf->ftype == BF_DIR    &&
	strcmp(bf->name, ".")  &&
	strcmp(bf->name, "..") &&
        strcmp(bf->name, THUMBDIRNAME) ) {
      recurseUpdate(br, bf->name);
    }
  }

  /* remove this directory from the stack */
  free(dirStack[--dirStackLen]);

  xv_getwd(curDir, sizeof(curDir));
  if (strcmp(orgDir, curDir)) {   /* change back to orgdir */
#ifdef AUTO_EXPAND
    Chvdir(orgDir);
#else
    chdir(orgDir);
#endif
    restIconVisible(br);
    scanDir(br);
  }
}


/*******************************************/
static void rm_file(br, name)
     BROWINFO *br;
     char *name;
{
  /* unlinks specified file.  br only needed to display potential err msg */

  int   i;
  char buf[512], buf1[512], *tmp;

  if (DEBUG) fprintf(stderr,"rm %s", name);

  i = unlink(name);
  if (i) {
    sprintf(buf, "rm %s: %s", name, ERRSTR(errno));
    setBrowStr(br, buf);
  }

#ifdef AUTO_EXPAND
  if (Rmvdir(name)) {
    sprintf(buf, "fail to remove virturl directory: %s", name);
    setBrowStr(br, buf);
  }
#endif

  /* try to delete a thumbnail file, as well.  ignore errors */
  strcpy(buf1, name);          /* tmp1 = leading path of name */
  tmp = (char *) rindex(buf1, '/');
  if (!tmp) strcpy(buf1,".");
  else *tmp = '\0';

  sprintf(buf, "%s/%s/%s", buf1, THUMBDIR, BaseName(name));
  if (DEBUG) fprintf(stderr,"   (%s)\n", buf);

  unlink(buf);
}

static char rmdirPath[MAXPATHLEN+1];

/*******************************************/
static void rm_dir(br, dname)
     BROWINFO *br;
     char *dname;
{
  /* called to remove top-level dir.  All subdirs are handled by rm_dir1() */

  strcpy(rmdirPath, dname);
  rm_dir1(br);
}

static void rm_dir1(br)
     BROWINFO *br;
{
  /* recursively delete this directory, and all things under it */

  int    i, dirlen, longpath, oldpathlen;
  char **names, *name, buf[512];
  struct stat st;

  if (DEBUG) fprintf(stderr,"rm %s\n", rmdirPath);

  longpath = 0;
  oldpathlen = strlen(rmdirPath);

  /* delete all plain files under this directory */
  names = getDirEntries(rmdirPath, &dirlen, 1);

  if (names && dirlen) {
    /* we've got the names of all files & dirs in this directory.  rm the
       non-subdirectories first */

    for (i=0; i<dirlen; i++) {
      name = names[i];

      /* skip . and .. (not that we should ever see them... */
      if (name[0] == '.' && (name[1]=='\0' ||
			     (name[1]=='.' && name[2]=='\0'))) goto done;

      if (strlen(name) + oldpathlen >= (MAXPATHLEN-3)) {
	longpath = 1;
	goto done;
      }

      strcat(rmdirPath, "/");
      strcat(rmdirPath, name);

      if (stat(rmdirPath, &st) < 0) {
	sprintf(buf, "%s: %s", name, ERRSTR(errno));
	setBrowStr(br, buf);
	rmdirPath[oldpathlen] = '\0';
	goto done;
      }

#ifdef AUTO_EXPAND
      if ((stat2bf((u_int) st.st_mode , rmdirPath) == BF_DIR)
	  && !Isarchive(rmdirPath))                /* skip, for now */
#else

      if (stat2bf((u_int) st.st_mode) == BF_DIR)   /* skip, for now */
#endif
      {
	rmdirPath[oldpathlen] = '\0';
	continue;   /* don't remove from list */
      }

      rm_file(br, rmdirPath);
      rmdirPath[oldpathlen] = '\0';

    done:     /* remove name from list */
      free(name);
      names[i] = (char *) NULL;
    }


    /* rm subdirectories, the only things left in the list */
    for (i=0; i<dirlen; i++) {
      if (!names[i]) continue;    /* loop until next name found */
      name = names[i];

      if (strlen(name) + oldpathlen >= (MAXPATHLEN-3)) {
	longpath = 1;
	continue;
      }

      strcat(rmdirPath, "/");
      strcat(rmdirPath, name);

      rm_dir1(br);     /* RECURSE! */

      rmdirPath[oldpathlen] = '\0';
    }

    /* empty rest of namelist */
    for (i=0; i<dirlen; i++) {
      if (names[i]) free(names[i]);
    }
    free(names);
  }

  if (longpath) setBrowStr(br, "Path too long error!");

  i = rmdir(rmdirPath);
  if (i < 0) {
    sprintf(buf, "rm %s: %s", rmdirPath, ERRSTR(errno));
    setBrowStr(br, buf);
  }
}




static int overwrite;
#define OWRT_ASK    0
#define OWRT_ALWAYS 1
#define OWRT_NEVER  2
#define OWRT_CANCEL 3

/*******************************************/
static void dragFiles(srcBr, dstBr, srcpath, dstpath, dstdir,
		      names, nlen, cpymode)
     BROWINFO   *srcBr, *dstBr;
     char       *srcpath, *dstpath, **names;
     const char *dstdir;
     int         nlen, cpymode;
{
  /* move or copy file(s) and their associated thumbnail files.
     srcpath and dstpath will have trailing '/'s.  dstdir is name of
     folder in dstpath (or "." or "..") to write to.  names is an nlen
     long array of strings (the simple filenames of the files to move)
     if 'cpymode' copy files, otherwise move them */

  int  i, j, dothumbs, fail;
  char dstp[MAXPATHLEN + 1];
  char src[MAXPATHLEN+1], dst[MAXPATHLEN+1];
  char buf[128];
  struct stat st;


  /* if the source directory is read-only, don't move files; copy them */
  if (!cpymode && (access(srcpath, W_OK) != 0))
    cpymode = 1;

  /* build real destination dir */
  strcpy(dstp, dstpath);

  /* note dstpath of "/" and dstdir of ".." will never happen, as there's
     no parent folder shown when in the root directory */

  if (strcmp(dstdir,"..")==0) {  /* lop off last pathname component */
    for (i=strlen(dstp)-2;  i>0 && dstp[i]!='/';  i--);
    i++;
    dstp[i] = '\0';
  }
  else if (strcmp(dstdir,".")!=0) sprintf(dstp, "%s%s/", dstpath, dstdir);

#ifdef AUTO_EXPAND
  if (Isvdir(dstp)) {
    sprintf(buf,"Sorry, you can't %s to the virtual directory, '%s'",
	    cpymode ? "copy" : "move", dstp);
    ErrPopUp(buf, "\nBummer!");
    SetCursors(-1);
    return;
  }
  if (Isvdir(srcpath))
      cpymode = 1;
#endif



  /* if there is a thumbnail directory in 'srcpath', make one for dstpath */
  sprintf(src,"%s%s", srcpath, THUMBDIR);
  dothumbs = 0;
#ifdef AUTO_EXPAND
  Dirtovd(src);
#endif
  if (stat(src, &st)==0) {
    sprintf(dst,"%s%s", dstp, THUMBDIR);
    mkdir(dst, st.st_mode & 07777);
    dothumbs = 1;
  }


  overwrite = OWRT_ASK;

  if (nlen>1) {
    if (cpymode) setBrowStr(srcBr, "Copying files...");
            else setBrowStr(srcBr, "Moving files...");
  }

  for (i=fail=0; i<nlen; i++) {
    WaitCursor();
    /* progress report */
    if (nlen>1) drawTemp(srcBr, i, nlen);

    if (strcmp(names[i], "..")==0) continue;  /* don't move parent */

    sprintf(src,"%s%s", srcpath, names[i]);
    sprintf(dst,"%s%s", dstp,    names[i]);

    if (cpymode) j = copyFile(src,dst);
            else j = moveFile(src,dst);

    if (overwrite == OWRT_CANCEL) break;         /* abort move */
    if (j==1) fail++;

#ifdef AUTO_EXPAND
    if (!cpymode && j==0)
      if (Movevdir(src,dst)) {
	sprintf(buf, "fail to move virturl directory: %s", names[i]);
	setBrowStr(srcBr, buf);
      }
#endif

    if (dothumbs && j==0) {
      sprintf(src,"%s%s/%s", srcpath, THUMBDIR, names[i]);
      sprintf(dst,"%s%s/%s", dstp,    THUMBDIR, names[i]);

      /* delete destination thumbfile to avoid 'overwrite' warnings */
      unlink(dst);

      if (cpymode) j = copyFile(src,dst);
              else j = moveFile(src,dst);
    }
  }

  if (nlen>1) clearTemp(srcBr);


  /* update icon windows appropriately */
  for (i=0; i<MAXBRWIN; i++) {
    if (strcmp(binfo[i].path, srcpath)==0 ||
	strcmp(binfo[i].path, dstp)==0) {
      rescanDir(&binfo[i]);
    }
  }

  /* update directory window (load/save) */
  sprintf(src, "%sbozo", srcpath);
  DIRDeletedFile(src);
  sprintf(src, "%sbozo", dstp);
  DIRCreatedFile(src);

  if (dothumbs) {
    sprintf(src, "%s%s/bozo", srcpath, THUMBDIR);
    DIRDeletedFile(src);
    sprintf(src, "%s%s/bozo", dstp, THUMBDIR);
    DIRCreatedFile(src);
  }


  if (!cpymode) {
    /* clear all lit files in the source folder (as they've been moved)
       note:  this won't be the optimal behavior if any files failed to
       move, but screw it, that's not going to happen too often... */
    for (i=0; i<srcBr->bfLen; i++) srcBr->bfList[i].lit = 0;
    srcBr->numlit = 0;
  }


  /* clear all files in the destination folder */
  for (i=0; i<dstBr->bfLen; i++) {
    dstBr->bfList[i].lit = 0;
  }
  dstBr->numlit = 0;


  /* light all named files in the destination folder */
  for (i=0; i<nlen; i++) {
    char *name;  BFIL *bf;
    name = names[i];
    for (j=0, bf=dstBr->bfList;
	 j<dstBr->bfLen && strcmp(name, bf->name)!=0; j++, bf++);
    if (j<dstBr->bfLen) {
      bf->lit = 1;  dstBr->numlit++;
    }
  }


  /* scroll so first lit file is visible */
  for (i=0; i<dstBr->bfLen && !dstBr->bfList[i].lit; i++);
  if (i<dstBr->bfLen) makeIconVisible(dstBr, i);

  drawIconWin(0, &(dstBr->scrl));    /* redraw dst window */
  changedNumLit(dstBr, -1, 0);


  /* adjust srcBr */
  for (i=srcBr->numlit=0; i<srcBr->bfLen; i++) {
    if (srcBr->bfList[i].lit) srcBr->numlit++;
  }
  changedNumLit(srcBr, -1, 0);


  if (fail) sprintf(buf, "Some files were not %s because of errors.",
		    cpymode ? "copied" : "moved");

  else if (nlen>1) sprintf(buf, "%d files %s", nlen,
			   (cpymode) ? "copied" : "moved");
  else buf[0] = '\0';
  setBrowStr(srcBr, buf);

  SetCursors(-1);
}

static int recursive_remove(dir)
     char *dir;
{
  DIR *dp = NULL;
  struct dirent *di;
  char name[MAXPATHLEN+1];

  strncpy(name, dir, MAXPATHLEN);
  name[MAXPATHLEN] = 0;

  if (name[strlen(name) - 1] == '/')
    name[strlen(name) - 1] = 0;

  if ((dp = opendir(name)) == NULL)
    goto err;

  while ((di = readdir(dp)) != NULL) {
    char buf[MAXPATHLEN+1];
    struct stat st;

    if (!strcmp(di->d_name, ".") || !strcmp(di->d_name, ".."))
      continue;

    snprintf(buf, MAXPATHLEN, "%s/%s", name, di->d_name);

    if (stat(buf, &st) < 0)
      continue;

    if (S_ISDIR(st.st_mode)) {
      if (recursive_remove(buf) < 0)
	goto err;
    } else
      unlink(buf);
  }

  if (rmdir(name) < 0)
    goto err;

  closedir(dp);
  return 0;

err:
  if (dp) closedir(dp);
  return -1;
}

/*************************************************/
static int moveFile(src,dst)
     char *src, *dst;
{
  /* essentially the same as the 'mv' command.  src and dst are full
     pathnames.  It's semi-quiet about errors.  a non-existant src file is
     *not* considered an error (as we don't check for thumbfiles to exist
     before calling this function).  Returns '1' on error, '0' if ok.
     Returns '-1' on 'skip this'

     One bit of noise:  if destination file exists, pop up a Overwrite?
     warning box.  */

  int                i, srcdir, dstdir;
  struct stat        st;
  char               buf[512];
  static const char *owbuts[]  = { "\nOk", "aAlways", "nNo", "NNever", "\033Cancel" };

  if (DEBUG) fprintf(stderr,"moveFile %s %s\n", src, dst);

#ifdef AUTO_EXPAND
  Dirtosubst(src);
#endif

  if (stat(src, &st)) return 0;    /* src doesn't exist, it would seem */
#ifdef AUTO_EXPAND
  srcdir = (stat2bf((u_int) st.st_mode , src) == BF_DIR);
#else
  srcdir = (stat2bf((u_int) st.st_mode) == BF_DIR);
#endif

  /* see if destination exists */

  if (stat(dst, &st)==0) {
    if (overwrite==OWRT_NEVER) return -1;
#ifdef AUTO_EXPAND
    dstdir = (stat2bf((u_int) st.st_mode , dst) == BF_DIR);
#else
    dstdir = (stat2bf((u_int) st.st_mode) == BF_DIR);
#endif

    if (overwrite==OWRT_ASK) {
      snprintf(buf, sizeof(buf), "%s '%s' exists.\n\nOverwrite?",
	      dstdir ? "Directory" : "File", dst);
      switch (PopUp(buf, owbuts, COUNT(owbuts))) {
	case 1: overwrite = OWRT_ALWAYS; break;
	case 2: return -1;
	case 3: overwrite = OWRT_NEVER; return -1;
	case 4: overwrite = OWRT_CANCEL;  return 1;
      }
    }

    if (dstdir) {
#ifndef VMS  /* we don't delete directories in VMS */
      if (recursive_remove(dst)) {   /* okay, so it's cheating... */
	SetISTR(ISTR_WARNING, "Unable to remove directory %s", dst);
	return 1;
      }
#endif /* VMS */
    }
    else if (unlink(dst)) {
      SetISTR(ISTR_WARNING, "unlink %s: %s", dst, ERRSTR(errno));
      return 1;
    }
  }


  if (!rename(src, dst)) return 0;   /* Ok */
  if (errno != EXDEV) return 1;      /* failure, of some sort */

  /* We're crossing filesystem boundaries.  Copy the file and rm the
     original */

  i = copyFile(src, dst);
  if (i == 0) {    /* copied okay, kill the original */
    if (srcdir) {
#ifndef VMS   /* we don't delete directories in VMS */
      if (recursive_remove(src)) {   /* okay, so it's cheating... */
	SetISTR(ISTR_WARNING, "Unable to remove directory %s", src);
	return 1;
      }
#endif /* VMS */
    }
    else if (unlink(src)) {
      SetISTR(ISTR_WARNING, "unlink %s: %s", src, ERRSTR(errno));
      return 1;
    }
  }

  return i;
}


/* needed to recursively copy directories */
static int  userMask, copyerr;
static char cpSrcPath[MAXPATHLEN], cpDstPath[MAXPATHLEN];


/*************************************************/
static int copyFile(src,dst)
     char *src, *dst;
{
  /* src and dst are full
     pathnames.  It's semi-quiet about errors.  a non-existant src file is
     *not* considered an error (as we don't check for thumbfiles to exist
     before calling this function).  Returns '1' on error, '0' if ok.

     Will be called with full filenames of source and destination, as in:
     src="/usr/bozo/foobie" and dst="/somewhere/else/foobie" */

  /* possible cases:  source is either a file or a directory, or doesn't exist,
     destination is either a file, a directory, or doesn't exist.

     if source doesn't exist, nothing to do.
     if source is a file:
        if dest is a file, popup 'overwriting' question, delete file if ok
	if dest is a dir,  popup 'overwriting dir' question, delete dir if ok
	   fall through:  if dest doesn't exist, copy the file
     if source is a dir:
        if dest is a file, popup 'overwriting' question, delete file if ok
	if dest is a dir,  popup 'overwriting dir' question, delete dir if ok
	   fall through:  if dest doesn't exist, copy the directory, recurs */


  int                dstExists, srcdir, dstdir;
  struct stat        srcSt, dstSt;
  char               buf[1024];
  static const char *owdiff[] = { "\nOk", "nNo", "\033Cancel" };
  static const char *owsame[] = { "\nOk", "aAlways", "nNo", "NNever", "\033Cancel" };

  if (DEBUG) fprintf(stderr,"copyFile %s %s\n", src, dst);

#ifdef AUTO_EXPAND
  Dirtosubst(src);
#endif

  if (stat(src,&srcSt)) return 0;  /* source doesn't exist, it would seem */

  dstExists = (stat(dst, &dstSt)==0);

  if (dstExists) {   /* ask about overwriting... */
#ifdef AUTO_EXPAND
  srcdir = (stat2bf((u_int) srcSt.st_mode , src) == BF_DIR);
  dstdir = (stat2bf((u_int) dstSt.st_mode , dst) == BF_DIR);
#else
  srcdir = (stat2bf((u_int) srcSt.st_mode) == BF_DIR);
  dstdir = (stat2bf((u_int) dstSt.st_mode) == BF_DIR);
#endif

    sprintf(buf, "%s '%s' already exists.  Replace it with %s '%s'?",
	    (dstdir) ? "Directory" : "File", dst,
	    (srcdir) ? "contents of directory" : "file", src);

    if (srcdir == dstdir) {
      if (overwrite==OWRT_NEVER) return -1;
      if (overwrite==OWRT_ASK) {
	switch (PopUp(buf, owsame, COUNT(owsame))) {
	  case 1: overwrite = OWRT_ALWAYS; break;
	  case 2: return -1;
	  case 3: overwrite = OWRT_NEVER; return -1;
	  case 4: overwrite = OWRT_CANCEL;  return 1;
	}
      }
    }
    else {     /* one's a dir, the other's a file.  *ALWAYS* ask! */
      switch (PopUp(buf, owdiff, COUNT(owdiff))) {
        case 1: return -1;
        case 2: overwrite = OWRT_CANCEL;  return 1;
      }
    }


    /* it's okay...  rm the destination */
    if (dstdir) {
      if (rmdir(dst)) return 1;   /* failed to remove */
    }
    else {
      if (unlink(dst)) return 1;  /* failed to remove */
    }

    dstExists = 0;
  }


  /* destination doesn't exist no more, if it ever did... */
  userMask = umask(0);  /* grab the umask */
  umask((mode_t) userMask);      /* put it back... */


  strcpy(cpSrcPath, src);
  strcpy(cpDstPath, dst);

  copyerr = 0;
  cp();

  return (copyerr>0) ? 1 : 0;
}


/* The following cp() and cp_* functions are derived from the source
   to 'cp' from the BSD sources */

/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/************************/
static void cp()
/************************/
{
  /* copies 'thing' at cpSrcPath to cpDstPath.  Since this function is
     called recursively by cp_dir, there are *no* guarantees that either file
     exists or not */

  int         havedst;
  struct stat srcSt, dstSt;

  if (stat(cpSrcPath, &srcSt)) {   /* src doesn't exist, usefully... */
    SetISTR(ISTR_WARNING, "%s: %s", cpSrcPath, ERRSTR(errno));
    copyerr++;
    return;
  }

  if (stat(cpDstPath, &dstSt)) {  /* dst doesn't exist.  not really a prob */
    havedst = 0;
  }
  else {
    if (srcSt.st_dev == dstSt.st_dev && srcSt.st_ino == dstSt.st_ino) {
      return;         /* identical files:  nothing to do */
    }
    havedst = 1;
  }

#ifdef AUTO_EXPAND
  switch(stat2bf((u_int) srcSt.st_mode , cpDstPath)) {
#else
  switch(stat2bf((u_int) srcSt.st_mode)) {
#endif
    /* determine how to copy, by filetype */

    /* NOTE:  There is no S_IFLNK case here, since we're using 'stat()' and
     * not lstat().  As such we'll never see symbolic links, only that which
     * they point to
     */

  case BF_DIR:  if (!havedst) {  /* create destination directory */
    if (mkdir(cpDstPath, srcSt.st_mode | 0700) < 0) {
      SetISTR(ISTR_WARNING,"%s: %s",cpDstPath,ERRSTR(errno));
      copyerr++;
      return;
    }
  }
  else {
#ifdef AUTO_EXPAND
    if (stat2bf((u_int) dstSt.st_mode , cpDstPath) != BF_DIR) {
#else
    if (stat2bf((u_int) dstSt.st_mode) != BF_DIR) {
#endif
      SetISTR(ISTR_WARNING,"%s: not a directory", cpDstPath);
      copyerr++;
      return;
    }
  }

    cp_dir();
    if (!havedst) chmod(cpDstPath, srcSt.st_mode);

    break;


  case BF_CHR:
  case BF_BLK:   cp_special(&srcSt, havedst);    break;

  case BF_FIFO:  cp_fifo(&srcSt, havedst);       break;

  case BF_SOCK:  SetISTR(ISTR_WARNING,"Socket file '%s' not copied.",
			 cpSrcPath);
                 copyerr++;
                 break;

  default:       cp_file(&srcSt, havedst);
  }

  return;
}


/********************/
static void cp_dir()
/********************/
{
  int    i, dirlen, oldsrclen, olddstlen, longpath;
  char **names, *name;
  struct stat  srcSt;


  /* src and dst directories both exists now.  copy entries */

  if (DEBUG) fprintf(stderr,"cp_dir:   src='%s',  dst='%s'\n",
		     cpSrcPath, cpDstPath);

  longpath  = 0;
  oldsrclen = strlen(cpSrcPath);
  olddstlen = strlen(cpDstPath);

  names = getDirEntries(cpSrcPath, &dirlen, 1);
  if (!names || !dirlen) return;      /* nothing to copy */


  /* now, we've got a list of entry names.  Copy the non-subdirs first, for
     performance reasons... */

  for (i=0; i<dirlen && overwrite!=OWRT_CANCEL; i++) {
    name = names[i];
    if (name[0] == '.' && (name[1]=='\0' ||
			   (name[1]=='.' && name[2]=='\0'))) goto done;

    /* add name to src and dst paths */
    if ((strlen(name) + oldsrclen >= (MAXPATHLEN-3)) ||
	(strlen(name) + olddstlen >= (MAXPATHLEN-3)))   {
      copyerr++;   /* path too long */
      longpath = 1;
      goto done;
    }

    strcat(cpSrcPath, "/");
    strcat(cpSrcPath, name);

    if (stat(cpSrcPath, &srcSt) < 0) {
      SetISTR(ISTR_WARNING,"%s: %s",cpSrcPath,ERRSTR(errno));
      copyerr++;
      cpSrcPath[oldsrclen] = '\0';
      goto done;
    }

#ifdef AUTO_EXPAND
    if (stat2bf((u_int) srcSt.st_mode , cpSrcPath) == BF_DIR)
#else
    if (stat2bf((u_int) srcSt.st_mode) == BF_DIR)
#endif
    {
      cpSrcPath[oldsrclen] = '\0';
      continue;                     /* don't remove from list, just skip */
    }

    strcat(cpDstPath, "/");
    strcat(cpDstPath, name);
    cp();                         /* RECURSE */

    cpSrcPath[oldsrclen] = '\0';
    cpDstPath[olddstlen] = '\0';

  done:                           /* remove name from list */
    free(name);
    names[i] = (char *) NULL;
  }


  /* copy subdirectories, which are the only things left in the list */
  for (i=0; i<dirlen && overwrite!=OWRT_CANCEL; i++) {
    if (!names[i]) continue;    /* loop until next name found */
    name = names[i];

    /* add name to src and dst paths */
    if ((strlen(name) + oldsrclen >= (MAXPATHLEN-3)) ||
	(strlen(name) + olddstlen >= (MAXPATHLEN-3)))   {
      copyerr++;
      longpath = 1;
      continue;
    }

    strcat(cpSrcPath, "/");
    strcat(cpSrcPath, name);

    strcat(cpDstPath, "/");
    strcat(cpDstPath, name);

    cp();                        /* RECURSE */

    cpSrcPath[oldsrclen] = '\0';
    cpDstPath[olddstlen] = '\0';
  }

  /* free all memory still in use */
  for (i=0; i<dirlen; i++) {
    if (names[i]) free(names[i]);
  }
  free(names);

  if (longpath) SetISTR(ISTR_WARNING, "Path too long error!");
}


/*****************************/
static void cp_file(st, exists)
     struct stat *st;
     int exists;
/*****************************/
{
  register int       srcFd, dstFd, rcount, wcount;
  char               buf[8192];
  static const char *owbuts[] = { "\nOk", "aAlways", "nNo", "NNever", "\033Cancel" };

  if (DEBUG) fprintf(stderr,"cp_file:  src='%s',  dst='%s'\n",
		     cpSrcPath, cpDstPath);

  if ((srcFd = open(cpSrcPath, O_RDONLY, 0)) == -1) {
    SetISTR(ISTR_WARNING, "%s: %s", cpSrcPath, ERRSTR(errno));
    copyerr++;
    return;
  }

  if (exists) {
    if (overwrite==OWRT_NEVER) return;
    if (overwrite==OWRT_ASK) {
      sprintf(buf, "File '%s' exists.\n\nOverwrite?", cpDstPath);
      switch (PopUp(buf, owbuts, 4)) {
        case 1: overwrite = OWRT_ALWAYS; break;
        case 2: return;
        case 3: overwrite = OWRT_NEVER; return;
        case 4: overwrite = OWRT_CANCEL;  return;
      }
    }
    dstFd = open(cpDstPath, O_WRONLY|O_TRUNC, 0);
  }
  else
    dstFd = open(cpDstPath, O_WRONLY|O_CREAT|O_TRUNC,
		 (st->st_mode & 0777) & (~userMask));

  if (dstFd == -1) {
    SetISTR(ISTR_WARNING, "%s: %s", cpDstPath, ERRSTR(errno));
    copyerr++;
    return;
  }

  WaitCursor();

  /* copy the file contents */
  while ((rcount = read(srcFd, buf, (size_t) 8192)) > 0) {
    wcount = write(dstFd, buf, (size_t) rcount);
    if (rcount != wcount || wcount == -1) {
      SetISTR(ISTR_WARNING, "%s: %s", cpDstPath, ERRSTR(errno));
      copyerr++;
      break;
    }
  }
  if (rcount < 0) {
    SetISTR(ISTR_WARNING, "%s: %s", cpSrcPath, ERRSTR(errno));
    copyerr++;
  }

  close(srcFd);
  if (close(dstFd)) {
    SetISTR(ISTR_WARNING, "%s: %s", cpDstPath, ERRSTR(errno));
    copyerr++;
  }
}



/*********************************/
static void cp_special(st, exists)
     struct stat *st;
     int exists;
/*********************************/
{
  if (DEBUG) fprintf(stderr,"cp_spec:  src='%s',  dst='%s'\n",
		     cpSrcPath, cpDstPath);

  if (exists && unlink(cpDstPath)) {
    SetISTR(ISTR_WARNING, "unlink %s: %s", cpDstPath, ERRSTR(errno));
    copyerr++;
    return;
  }

#ifndef VMS  /* VMS doesn't have a mknod command */
  if (mknod(cpDstPath, st->st_mode, st->st_rdev)) {
    SetISTR(ISTR_WARNING, "mknod %s: %s", cpDstPath, ERRSTR(errno));
    copyerr++;
    return;
  }
#endif

}


/*********************************/
static void cp_fifo(st, exists)
     struct stat *st;
     int exists;
/*********************************/
{
  if (DEBUG) fprintf(stderr,"cp_fifo:  src='%s',  dst='%s'\n",
		     cpSrcPath, cpDstPath);

#ifdef S_IFIFO
  if (exists && unlink(cpDstPath)) {
    SetISTR(ISTR_WARNING, "unlink %s: %s", cpDstPath, ERRSTR(errno));
    copyerr++;
    return;
  }

  if (mknod(cpDstPath, (st->st_mode & 07777) | S_IFIFO, 0)) {
               /* was:  mkfifo(cpDstPath, st->st_mode) */
    SetISTR(ISTR_WARNING, "mkfifo %s: %s", cpDstPath, ERRSTR(errno));
    copyerr++;
    return;
  }
#endif
}




/*********************************/
#ifdef AUTO_EXPAND
static int stat2bf(uistmode, path)
     u_int uistmode;
     char *path;
#else
static int stat2bf(uistmode)
     u_int uistmode;
#endif
{
  /* given the 'st.st_mode' field from a successful stat(), returns
     BF_FILE, BF_DIR, BF_BLK, BF_CHR, BF_FIFO, or BF_SOCK.  Does *NOT*
     return BF_EXE */

  int rv;
  mode_t stmode = (mode_t) uistmode;

  if      (S_ISDIR(stmode))  rv = BF_DIR;
  else if (S_ISCHR(stmode))  rv = BF_CHR;
  else if (S_ISBLK(stmode))  rv = BF_BLK;
  else if (S_ISFIFO(stmode)) rv = BF_FIFO;
  else if (S_ISSOCK(stmode)) rv = BF_SOCK;
#ifdef AUTO_EXPAND
  else if (Isarchive(path))  rv = BF_DIR;
#endif
  else                       rv = BF_FILE;

  return rv;
}



/*********************************/
static int selmatch(name, line)
     char *name, *line;
{
  /* returns non-zero if 'name' is found in 'line'.  Line can be sequence of
     words separated by whitespace, in which case 'name' is compared to each
     word in the line */

  char *arg;
  int   ch, rv;

  rv = 0;

  while (*line && !rv) {
    while (*line && isspace(*line)) line++;   /* to begin of next word */
    arg = line;
    while (*line && !isspace(*line)) line++;  /* end of this word */

    ch = *line;  *line = '\0';                /* null-terminate 'arg' */
    rv = selmatch1(name, arg);
    *line = ch;
  }

  return rv;
}


/*********************************/
static int selmatch1(name, arg)
     char *name, *arg;
{
  /* returns non-zero if 'name' matches 'arg'.  Any '*' chars found in arg
     are considered wildcards that match any number of characters,
     including zero. */

  char *sp, *oldnp;

  while (*arg && *name) {
    if (*arg != '*') {
      if (*arg != *name) return 0;
      arg++;  name++;
    }

    else {      /* hit a '*' ... */
      for (sp=arg+1;  *sp && *sp!='*';  sp++);   /* any other '*'s in arg? */
      if (!*sp) {
	/* this is the last '*'.  Advance name, arg to end of their strings,
	   and match backwards until we hit the '*' character */

	oldnp = name;
	while (*name) name++;
	while (*arg ) arg++;
	name--;  arg--;

	while (*arg != '*') {
	  if (*arg != *name || name<oldnp) return 0;
	  arg--;  name--;
	}
	return 1;   /* success! */
      }

      else {  /* there are more '*'s in arg... */
	/* find the first occurrence of the string between the two '*'s.
	   if the '*'s are next to each other, just throw away the first one */

	arg++;  /* points to char after  first  '*' */
	sp--;   /* points to char before second '*' */

	if (arg<=sp) {   /* find string arg..sp in name */
	  int i;
	  int sslen = (sp-arg) + 1;

	  while (*name) {
	    for (i=0; i<sslen && name[i] && name[i]==arg[i]; i++);
	    if (i==sslen) break;
	    else name++;
	  }
	  if (!*name) return 0;

	  /* found substring in name */
	  name += sslen;
	  arg = sp+1;
	}
      }
    }
  }

  if (!*arg && !*name) return 1;

  return 0;
}


static IVIS *icon_vis_list = NULL;

/***************************************************************/
static void recIconVisible(name, icon)
  char *name;
  int   icon;
{
  IVIS *ptr, *prev = NULL;

  for (ptr = icon_vis_list; ptr; prev = ptr, ptr = ptr->next) {
    if (!strcmp(ptr->name, name)) {
      ptr->icon = icon;
      return;
    }
  }

  ptr = calloc(sizeof(IVIS), 1);
  if (!ptr)
    return;

  ptr->name = strdup(name);

  if (!ptr->name) {
    free(ptr);
    return;
  }

  if (!prev) {
    icon_vis_list = ptr;
  } else {
    prev->next = ptr;
  }

  ptr->next = NULL;
  ptr->icon = icon;
}

/***************************************************************/
static void restIconVisible(br)
  BROWINFO *br;
{
  IVIS *ptr;

  for (ptr = icon_vis_list; ptr; ptr = ptr->next) {
    if (!strcmp(ptr->name, br->path)) {
      if (ptr->icon >= 0) {
        makeIconVisible(br, ptr->icon);
        updateSel(br, ptr->icon, 0, 0);
      }
      return;
    }
  }
}


/*********************************/
static void clipChanges(br)
     BROWINFO *br;
{
  /* called whenever schnauzer activity should place file names in
     the X11 clipboard, or change what it put there.

     Implementation is simple because the UI is non-standard
     (i.e., not like xterm(1)). The clipboard command causes the
     current browser to dump all its currently selected files'
     (if any) names to the clipboard, space-separated.
     No effort is made to shell-escape blanks and other 'odd'
     characters in the names. */

  char buf[4000]; /* too much or too little, whatever... */
  int n;
  int i;

  n = 0;
  strcpy(buf, "");

  for (i=0; i<br->bfLen; i++) {
    if(br->bfList[i].lit == 1) {
      int m;

      m = strlen(br->bfList[i].name) + 1;

      if(n+m+1 >= sizeof(buf)) return;  /* names probably won't fit in buf, abort */
      strcat(buf, br->bfList[i].name);
      strcat(buf, " ");
      n += m;
    }
  }

  if(n) {
    buf[n-1] = 0; /* trim last space */

    NewCutBuffer(buf);
  }
}
