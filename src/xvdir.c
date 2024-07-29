/*
 * xvdir.c - Directory changin', file i/o dialog box
 *
 * callable functions:
 *
 *   CreateDirW(geom,bwidth)-  creates the dirW window.  Doesn't map it.
 *   ResizeDirW()	    -  change the size of the dirW window
 *   DirBox(vis)            -  random processing based on value of 'vis'
 *                             maps/unmaps window, etc.
 *   ClickDirW()            -  handles mouse clicks in DirW
 *   DbouleClickDirW()      -  handles mouse double-clicks in DirW
 *   DragDirW()             -  handles mouse movement with mouse down in DirW
 *   LoadCurrentDirectory() -  loads up current dir information for dirW
 *   GetDirPath()           -  returns path that 'dirW' is looking at
 *   DoSave()               -  calls appropriate save routines
 *   SetDirFName()          -  sets the 'load/save-as' filename and default
 *   GetDirFName()          -  gets the 'load/save-as' filename (no path)
 *   SetDirSaveMode()       -  sets default format/color settings
 *
 *   InitPoll()             -  called whenever a file is first loaded
 *   CheckPoll(int)         -  checks to see whether we should reload
 */

#include "copyright.h"

#define NEEDSTIME      /* for CheckPoll */
#define NEEDSDIR
#include "xv.h"

#include "bits/d_load"
#include "bits/d_save"

#ifndef VMS
#include <pwd.h>       /* for getpwnam() prototype and passwd struct */
#endif


#define DEF_DIRWIDE (350*dpiMult)  /* initial size of directory window */
#define DEF_DIRHIGH (400*dpiMult)
#define MIN_DIRWIDE (330*dpiMult)
#define MIN_DIRHIGH (300*dpiMult)

#define DLIST_X  (10*dpiMult)      /* left of directory list box */
#define DLIST_Y  (30*dpiMult)      /* top of directory list box */
#define DNAM_X   (80*dpiMult)      /* left of filename box */

#define NLINES   15                /* initial # of lines in list control (keep odd) */
#define BUTTW    (80*dpiMult)      /* width of buttons */
#define BUTTH    (24*dpiMult)      /* height of buttons */
#define MBDIRPAD (10*dpiMult)      /* space around directory name in menu button */
#define DNAMMORE (3*dpiMult)       /* width of "more off screen" markers */
#define MAXDEEP  30                /* max num of directories in cwd path */
#define MAXFNLEN 256               /* max len of filename being entered */

#define FMTLABEL "Format:"         /* label shown next to fmtMB */
#define COLLABEL "Colors:"         /* label shown next to colMB */
#define FMTWIDE  (150*dpiMult)     /* width of fmtMB */
#define COLWIDE  (150*dpiMult)     /* width of colMB */

/* NOTE: make sure these match up with F_* definitions in xv.h */
static const char *saveColors[] = { "Full Color",
				    "Greyscale",
				    "B/W Dithered",
				    "Reduced Color" };

static const char *saveFormats[] = {
#ifdef HAVE_PNG
					"PNG",
#endif
#ifdef HAVE_JPEG
					"JPEG",
#endif
#ifdef HAVE_JP2K
					"JPEG 2000",
					"JP2",
#endif 
					"GIF",
#ifdef HAVE_TIFF
					"TIFF",
#endif
					"PostScript",
					"PBM/PGM/PPM (raw)",
					"PBM/PGM/PPM (ascii)",
					"X11 Bitmap",
					"XPM",
					"BMP",
					"Sun Rasterfile",
					"IRIS RGB",
					"Targa (24-bit)",
					"FITS",
					"PM",
					"Spectrum SCREEN$",	/* [JCE] */
					"G3 fax",		/* [JPD] and [DEG] */
					"WBMP",
#ifdef HAVE_WEBP
					"WEBP",
#endif
#ifdef HAVE_MAG
					"MAG",
#endif
#ifdef HAVE_PIC
					"PIC",
#endif
#ifdef HAVE_MAKI
					"MAKI (640x400 only)",
#endif
#ifdef HAVE_PI
					"PI",
#endif
#ifdef HAVE_PIC2
					"PIC2",
#endif
#ifdef HAVE_MGCSFX
					"MgcSfx",
#endif
					MBSEP,
					"Filename List" };


static int  DirHigh             PARM((void));
static int  DirWide             PARM((void));
static int  roomForLines        PARM((int));
static int  DNamWide            PARM((void));
static int  DNamY               PARM((void));
static void arrangeElements     PARM((int));
static int  posOfCoordinate     PARM((int));
static void moveInsertionPoint  PARM((int));
static void unselect            PARM((void));
static void removeSelectedRange PARM((void));
static void pasteIntoBox        PARM((const char *text));
static void RedrawDList         PARM((int, SCRL *));
static void changedDirMB        PARM((int));
static int  updatePrimarySelection PARM((void));
static int  updateClipboardSelection PARM((void));
static int  dnamcmp             PARM((const void *, const void *));
static int  FNameCdable         PARM((void));
static void loadCWD             PARM((void));
#ifdef FOO
static int  cd_able             PARM((char *));
#endif
static void scrollToFileName    PARM((void));
static void setFName            PARM((const char *));
static void showFName           PARM((void));
static void changeSuffix        PARM((void));
static int  autoComplete        PARM((void));

static byte *handleBWandReduced PARM((byte *, int,int,int, int, int *,
					byte **, byte **, byte **));
static byte *handleNormSel      PARM((int *, int *, int *, int *));

static int   dir_h; /* current height of dir window */

static char       *fnames[MAXNAMES];
static int         numfnames = 0, ndirs = 0;
static char        path[MAXPATHLEN+1];       /* '/' terminated */
static char        loadpath[MAXPATHLEN+1];   /* '/' terminated */
static char        savepath[MAXPATHLEN+1];   /* '/' terminated */
static char       *dirs[MAXDEEP];            /* list of directory names */
static const char *dirMBlist[MAXDEEP];       /* list of dir names in right order */
static char       *lastdir;                  /* name of the directory we're in */
static char        filename[MAXFNLEN+100];   /* filename being entered */
static char        deffname[MAXFNLEN+100];   /* default filename */

static int   savemode;                 /* if 0 'load box', if 1 'save box' */
static int   curPos;                   /* insertion point in textedit filename */
static int   stPos, enPos;             /* start and end of visible textedit filename */
static int   selPos, selLen;           /* start and length of selected region of textedit filename */
static MBUTT dirMB;                    /* popup path menu */
static MBUTT fmtMB;                    /* 'format' menu button (Save only) */
static MBUTT colMB;                    /* 'colors' menu button (Save only) */

static Pixmap d_loadPix, d_savePix;

static int  haveoldinfo = 0;
static int  oldformat, oldcolors;
static char oldfname[MAXFNLEN+100];

/* the name of the file actually opened.  (the temp file if we are piping) */
static char outFName[256];
static int  dopipe;


/***************************************************/
static int DirHigh(void)
{
  return dir_h;
}


/***************************************************/
static int DirWide(void)
{
  return dList.w + 113*dpiMult;
}


/***************************************************/
static int roomForLines(int height)
{
  int num;

  num = (height - (dList.y + 66*dpiMult))/LINEHIGH;
  if (num < 1)
    num = 1;
  if (num > MAXNAMES)
    num = MAXNAMES;

  return num;
}


/***************************************************/
void ResizeDirW(int w, int h)
{
  int nlines;

  dir_h = h;

  nlines = roomForLines(h);
  LSResize(&dList, w - 113*dpiMult, LINEHIGH*nlines, nlines);
  arrangeElements(savemode);
  showFName();
}


/***************************************************/
static int DNamWide(void)
{
  return DirWide() - 100*dpiMult;
}


/***************************************************/
static int DNamY(void)
{
  return DirHigh() - (10*dpiMult + 2*dpiMult + LINEHIGH + 5*dpiMult);
}


/***************************************************/
void CreateDirW(void)
{
  int nlines;

  path[0] = '\0';

  xv_getwd(loadpath, sizeof(loadpath));
  xv_getwd(savepath, sizeof(savepath));

  dir_h = DEF_DIRHIGH;
  dirW = CreateFlexWindow("", "XVdir", NULL, DEF_DIRWIDE, DEF_DIRHIGH,
			  infofg, infobg, FALSE, FALSE, FALSE);
  if (!dirW) FatalError("couldn't create 'directory' window!");
  SetMinSizeWindow(dirW, MIN_DIRWIDE, MIN_DIRHIGH);

  nlines = roomForLines(dir_h);

  LSCreate(&dList, dirW, DLIST_X, 5*dpiMult + 3*(6*dpiMult + LINEHIGH) + 6*dpiMult, DEF_DIRWIDE - 113*dpiMult,
	   LINEHIGH*nlines, nlines, fnames, numfnames, infofg, infobg,
	   hicol, locol, RedrawDList, TRUE, FALSE);


  dnamW = XCreateSimpleWindow(theDisp, dirW, 0, 0, 1, 1,
			      1, infofg, infobg);
  if (!dnamW) FatalError("can't create name window");
  XSelectInput(theDisp, dnamW, ExposureMask);

  /* create checkboxes */

  CBCreate(&browseCB,   dirW, 0, 0,
	   "Browse", infofg, infobg, hicol,locol);

  CBCreate(&savenormCB, dirW, 0, 0,
	   "Normal Size", infofg, infobg, hicol,locol);

  CBCreate(&saveselCB,  dirW, 0, 0,
           "Selected Area", infofg, infobg, hicol,locol);

  /* y-coordinates get filled in when window is opened */
  BTCreate(&dbut[S_BOK],     dirW, 0, 0, BUTTW, BUTTH,
	   "Ok",        infofg, infobg,hicol,locol);
  BTCreate(&dbut[S_BCANC],   dirW, 0, 0, BUTTW, BUTTH,
	   "Cancel",    infofg,infobg,hicol,locol);
  BTCreate(&dbut[S_BRESCAN], dirW, 0, 0, BUTTW, BUTTH,
	   "Rescan",    infofg,infobg,hicol,locol);
  BTCreate(&dbut[S_BOLDSET], dirW, 0, 0, BUTTW, BUTTH,
	   "Prev Set",  infofg,infobg,hicol,locol);
  BTCreate(&dbut[S_BOLDNAM], dirW, 0, 0, BUTTW, BUTTH,
	   "Prev Name", infofg,infobg,hicol,locol);

  SetDirFName("");
  XMapSubwindows(theDisp, dirW);
  numfnames = 0;

  /*
   * create MBUTTs *after* calling XMapSubWindows() to keep popup unmapped
   */

  MBCreate(&dirMB, dirW, 0, 0, 1*dpiMult, 1*dpiMult,
	   NULL, NULL, 0,
	   infofg,infobg,hicol,locol);

  MBCreate(&fmtMB, dirW, 0, 0, 1*dpiMult, 1*dpiMult,
	   NULL, saveFormats, F_MAXFMTS,
	   infofg,infobg,hicol,locol);
  fmtMB.hascheck = 1;
  MBSelect(&fmtMB, 0);

  MBCreate(&colMB, dirW, 0, 0, 1*dpiMult, 1*dpiMult,
	   NULL, saveColors, F_MAXCOLORS,
	   infofg,infobg,hicol,locol);
  colMB.hascheck = 1;
  MBSelect(&colMB, 0);


  d_loadPix = XCreatePixmapFromBitmapData(theDisp, dirW,
                 (char *) d_load_bits, d_load_width, d_load_height,
					  infofg, infobg, dispDEEP);

  d_savePix = XCreatePixmapFromBitmapData(theDisp, dirW,
                 (char *) d_save_bits, d_save_width, d_save_height,
					  infofg, infobg, dispDEEP);
}


/***************************************************/
void DirBox(int mode)
{
  static int firstclose = 1;

  if (mode == 0) {
    if (savemode) strcpy(savepath, path);
             else strcpy(loadpath, path);

    if (firstclose) {
      strcpy(loadpath, path);
      strcpy(savepath, path);
      firstclose = 0;
    }

    XUnmapWindow(theDisp, dirW);  /* close */
  }
  else {

    if (mode == BLOAD) {
      savemode = FALSE;

      strcpy(path, loadpath);
      WaitCursor();  LoadCurrentDirectory();  SetCursors(-1);

      XStoreName(theDisp, dirW, "xv load");
      XSetIconName(theDisp, dirW, "xv load");

      dbut[S_BLOADALL].str = "Load All";
      BTSetActive(&dbut[S_BLOADALL], 1);

      arrangeElements(savemode);

      MBSetActive(&fmtMB, 0);
      MBSetActive(&colMB, 0);
    }

    else if (mode == BSAVE) {
      savemode = TRUE;

      strcpy(path, savepath);
      WaitCursor();  LoadCurrentDirectory();  SetCursors(-1);

      XStoreName(theDisp, dirW, "xv save");
      XSetIconName(theDisp, dirW, "xv save");

      dbut[S_BOLDSET].str = "Prev Set";

      arrangeElements(savemode);

      BTSetActive(&dbut[S_BOLDSET], haveoldinfo);
      BTSetActive(&dbut[S_BOLDNAM], haveoldinfo);

      CBSetActive(&saveselCB, HaveSelection());

      MBSetActive(&fmtMB, 1);
      if (MBWhich(&fmtMB) == F_FILELIST) {
        MBSetActive(&colMB,      0);
        CBSetActive(&savenormCB, 0);
      }
      else {
        MBSetActive(&colMB,      1);
        CBSetActive(&savenormCB, 1);
      }
    }

    CenterMapFlexWindow(dirW,
		        dbut[S_BOK].x + BUTTW/2, dbut[S_BOK].y + BUTTH/2,
		        DirWide(), DirHigh(), FALSE);

    scrollToFileName();
  }

  dirUp = mode;
  BTSetActive(&but[BLOAD], !dirUp);
  BTSetActive(&but[BSAVE], !dirUp);
}


/***************************************************/
static void arrangeElements(int savemode)
{
  int i, nbts, ngaps, szdiff, top, gap;

  /* buttons */

  nbts = !savemode ? S_LOAD_NBUTTS : S_NBUTTS;
  ngaps = nbts-1;

  szdiff = dList.h - (nbts * BUTTH);
  gap    = szdiff / ngaps;

  if (gap>16*dpiMult) {
    gap = 16*dpiMult;
    top = dList.y + (dList.h - (nbts*BUTTH) - (ngaps*gap))/2;

    for (i=0; i<nbts; i++)
      BTMove(&dbut[i],
             dList.x + dList.w + 12*dpiMult,
             top + i*(BUTTH+gap));
  }
  else {
    for (i=0; i<nbts; i++)
      BTMove(&dbut[i],
             dList.x + dList.w + 12*dpiMult,
             dList.y + ((dList.h-BUTTH)*i) / ngaps);
  }

  /* checkboxes */

  CBMove(&browseCB,   DirWide()/2, dList.y + (int) dList.h + 6*dpiMult);
  CBMove(&savenormCB, 220*dpiMult, dList.y + (int) dList.h + 6*dpiMult);
  CBMove(&saveselCB,   80*dpiMult, dList.y + (int) dList.h + 6*dpiMult);

  /* filename box */

  XMoveResizeWindow(theDisp, dnamW,
		    DNAM_X, DNamY(),
		    (u_int) DNamWide()+2*DNAMMORE, (u_int) LINEHIGH+5);

  /* menu buttons */

  MBChange(&dirMB,
           dList.x + dList.w/2 - dirMB.w/2,
	   dList.y - (LINEHIGH + 6*dpiMult),
	   (u_int) dirMB.w, (u_int) LINEHIGH);

  MBChange(&fmtMB,
           DirWide() - FMTWIDE - 10*dpiMult, 5*dpiMult,
	   (u_int) FMTWIDE, (u_int) LINEHIGH);

  MBChange(&colMB,
           DirWide() - COLWIDE - 10*dpiMult, 5*dpiMult + LINEHIGH + 6*dpiMult,
	   (u_int) COLWIDE, (u_int) LINEHIGH);
}



/***************************************************/
void RedrawDirW(int x, int y, int w, int h)
{
  int        i, ypos, txtw;
  char       foo[30];
  const char *str;

  XV_UNUSED(x); XV_UNUSED(y); XV_UNUSED(w); XV_UNUSED(h);

  if (dList.nstr==1)
    strcpy(foo, "1 file");
  else
    sprintf(foo, "%d files", dList.nstr);

  ypos = dList.y + dList.h + 8*dpiMult + ASCENT;
  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp, dirW, theGC, 10*dpiMult, ypos-ASCENT,
		 (u_int) DirWide(), (u_int) CHIGH);
  XSetForeground(theDisp, theGC, infofg);
  DrawString(dirW, 10*dpiMult, ypos, foo);


  if (dirUp == BLOAD)
    str = "Load file:";
  else
    str = "Save file:";
  DrawString(dirW, 10*dpiMult, DNamY() + 1*dpiMult + ASCENT + 3*dpiMult, str);

  /* draw dividing line */
  XSetForeground(theDisp,    theGC, infofg);
  XDrawLine(theDisp, dirW,   theGC, 0, dirMB.y - 6*dpiMult, DirWide(), dirMB.y - 6*dpiMult);
  if (ctrlColor) {
    XSetForeground(theDisp,  theGC, locol);
    XDrawLine(theDisp, dirW, theGC, 0, dirMB.y - 5*dpiMult, DirWide(), dirMB.y - 5*dpiMult);
    XSetForeground(theDisp,  theGC, hicol);
  }
  XDrawLine(theDisp, dirW,   theGC, 0, dirMB.y - 4*dpiMult, DirWide(), dirMB.y - 4*dpiMult);



  for (i=0; i<(savemode ? S_NBUTTS : S_LOAD_NBUTTS); i++) BTRedraw(&dbut[i]);

  MBRedraw(&dirMB);
  MBRedraw(&fmtMB);
  MBRedraw(&colMB);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  txtw = StringWidth(FMTLABEL);
  if (StringWidth(COLLABEL) > txtw) txtw = StringWidth(COLLABEL);

  if (!savemode) {
    XCopyArea(theDisp, d_loadPix, dirW, theGC, 0,0,d_load_width,d_load_height,
	      10, (dirMB.y - 6*dpiMult)/2 - d_load_height/2);

    XSetFillStyle(theDisp, theGC, FillStippled);
    XSetStipple(theDisp, theGC, dimStip);
    DrawString(dirW, fmtMB.x - 6*dpiMult - txtw, 5*dpiMult + 3*dpiMult + ASCENT, FMTLABEL);
    DrawString(dirW, fmtMB.x - 6*dpiMult - txtw, 5*dpiMult + 3*dpiMult + ASCENT + (LINEHIGH + 6*dpiMult), COLLABEL);
    XSetFillStyle(theDisp,theGC,FillSolid);

    CBRedraw(&browseCB);
  }
  else {
    XCopyArea(theDisp, d_savePix, dirW, theGC, 0,0,d_save_width,d_save_height,
	      10*dpiMult, (dirMB.y - 6*dpiMult)/2 - d_save_height/2);

    XSetForeground(theDisp, theGC, infofg);
    DrawString(dirW, fmtMB.x - 6*dpiMult - txtw, 5*dpiMult + 3*dpiMult + ASCENT, FMTLABEL);
    DrawString(dirW, fmtMB.x - 6*dpiMult - txtw, 5*dpiMult + 3*dpiMult + ASCENT + (LINEHIGH + 6*dpiMult), COLLABEL);

    CBRedraw(&savenormCB);
    CBRedraw(&saveselCB);
  }
}


/***************************************************/
int ClickDirW(int x, int y, int button)
{
  BUTT  *bp;
  int    bnum,i,maxbut,v;
  char   buf[MAXPATHLEN + 1024];

  switch (button) {
  case 1:
    if (savemode) {                           /* check format/colors MBUTTS */
      i = v = 0;
      if      (MBClick(&fmtMB, x,y) && (v=MBTrack(&fmtMB))>=0) i=1;
      else if (MBClick(&colMB, x,y) && (v=MBTrack(&colMB))>=0) i=2;

      if (i) {  /* changed one of them */
        if (i==1) SetDirSaveMode(F_FORMAT, v);
             else SetDirSaveMode(F_COLORS, v);
        changeSuffix();
      }
    }

    if (!savemode) {  /* LOAD */
      if (CBClick(&browseCB,x,y)) CBTrack(&browseCB);
    }
    else {            /* SAVE */
      if      (CBClick(&savenormCB,x,y)) CBTrack(&savenormCB);
      else if (CBClick(&saveselCB,x,y))  CBTrack(&saveselCB);
    }

    maxbut = (savemode) ? S_NBUTTS : S_LOAD_NBUTTS;

    for (bnum=0; bnum<maxbut; bnum++) {
      bp = &dbut[bnum];
      if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
    }

    if (bnum<maxbut && BTTrack(bp)) {   /* found one */
      if (bnum<S_BOLDSET) return bnum;  /* do Ok,Cancel,Rescan in xvevent.c */

      if (bnum == S_BOLDSET && savemode && haveoldinfo) {
        MBSelect(&fmtMB, oldformat);
        MBSelect(&colMB, oldcolors);
        changeSuffix();
      }

      else if (bnum == S_BOLDNAM && savemode && haveoldinfo) {
        setFName(oldfname);
      }

      else if (bnum == S_BLOADALL && !savemode) {
        int j, oldnumnames;
        char *dname;

        oldnumnames = numnames;

        for (i=0; i<numfnames && numnames<MAXNAMES; i++) {
         if (fnames[i][0] == C_REG || fnames[i][0] == C_EXE) {
           sprintf(buf,"%s%s", path, fnames[i]+1);

           /* check for dups.  Don't add it if it is. */
           for (j=0; j<numnames && strcmp(buf,namelist[j]); j++);

           if (j==numnames) {  /* add to list */
             namelist[numnames] = (char *) malloc(strlen(buf)+1);
             if (!namelist[numnames]) FatalError("out of memory!\n");
             strcpy(namelist[numnames],buf);

             dname = namelist[numnames];

             /* figure out how much of name can be shown */
             if (StringWidth(dname) > (nList.w - 10*dpiMult - 16*dpiMult)) {   /* truncate */
               char *tmp;
               int   prelen = 0;

               tmp = dname;
               while (1) {
                 tmp = (char *) index(tmp,'/'); /* find next '/' in buf */
                 if (!tmp) break;

                 tmp++;                   /* move to char following the '/' */
                 prelen = tmp - dname;
                 if (StringWidth(tmp) <= (nList.w - 10*dpiMult - 16*dpiMult)) break; /* cool now */
               }
               dispnames[numnames] = dname + prelen;
             }
             else dispnames[numnames] = dname;
             numnames++;
           }
         }
        }

        if (oldnumnames != numnames) {  /* added some */
         if (numnames>0) BTSetActive(&but[BDELETE],1);
         windowMB.dim[WMB_TEXTVIEW] = (numnames==0);

         LSNewData(&nList, dispnames, numnames);
         nList.selected = oldnumnames;
         curname = oldnumnames - 1;

         ActivePrevNext();

         ScrollToCurrent(&nList);
         DrawCtrlNumFiles();

         if (!browseCB.val) DirBox(0);
        }
      }
    }

    if (MBClick(&dirMB, x, y)) {
      i = MBTrack(&dirMB);
      if (i >= 0) changedDirMB(i);
      return -1;
    }

    /* handle clicks inside the filename box */
    if (x > DNAM_X &&
        x < DNAM_X + DNamWide()+2*DNAMMORE &&
        y > DNamY() &&
        y < DNamY() + LINEHIGH + 5*dpiMult) {
      Window dummy_root, dummy_child;
      int dummy_rx, dummy_ry;
      int wx, wy;
      unsigned int mask;
      int clkx_start, clkx_now;
      int pos_start, pos_now, pos_prev;

      /* make coordinates relative to dnamW */
      /* left side plus the border plus the space for the "more stuff" sign */
      clkx_start = x - (DNAM_X + 1*dpiMult + DNAMMORE);
      pos_start = posOfCoordinate(clkx_start);

      clkx_now = clkx_start;
      pos_prev = pos_now = -1;
      selPos=selLen = 0;
      while (XQueryPointer(theDisp, dirW,
                           &dummy_root, &dummy_child, &dummy_rx, &dummy_ry,
                           &wx, &wy, &mask)) {
        if (!(mask & Button1Mask)) break;    /* button released */

        clkx_now = wx - (DNAM_X + 1*dpiMult + DNAMMORE);
        pos_prev = pos_now;
        pos_now = posOfCoordinate(clkx_now);

        if (pos_prev != pos_now) {
          if (pos_start == pos_now) {
            selPos=selLen = 0;
            curPos = pos_start;
          } else if (pos_start < pos_now) {
            selPos = pos_start;
            selLen = pos_now - pos_start;
          } else {
            selPos = pos_now;
            selLen = pos_start - pos_now;
          }
          showFName();
        }
      }

      if (selLen > 0)
        updatePrimarySelection();
      else {
        unselect();
        moveInsertionPoint(clkx_start);
      }
     }
    break;

  case 2:
    /* handle clicks inside the filename box */
    if (x > DNAM_X &&
        x < DNAM_X + DNamWide()+2*DNAMMORE &&
        y > DNamY() &&
        y < DNamY() + LINEHIGH + 5*dpiMult) {
      int   clkx;
      char *text;

      /* make coordinates relative to dnamW */
      clkx = x - (DNAM_X + 1*dpiMult + DNAMMORE); /* left side plus the border plus the space for the "more stuff" sign */
      moveInsertionPoint(clkx);

      text = GetPrimaryText();

      if (text != NULL) {
       pasteIntoBox(text);
        free(text);
      }
    }
    break;
   }

  return -1;
}

/***************************************************/
int DoubleClickDirW(int x, int y, int button)
{
  /* handle double-clicks inside the filename box */
  if (button == 1 &&
      x > DNAM_X &&
      x < DNAM_X + DNamWide()+2*DNAMMORE &&
      y > DNamY() &&
      y < DNamY() + LINEHIGH + 5*dpiMult) {
    SelectAllDirW();
    return -1;
  }
  return ClickDirW(x, y, button);
}


static int posOfCoordinate(int clkx)
{
  int dx, pos;

  for (pos=stPos; pos < enPos; pos++) {
    if (XTextWidth(mfinfo, &filename[stPos], pos-stPos) + 1*dpiMult > clkx)
      break;
  }
  /* if we are more than halfway past this char, put the insertion point after it */
  if (pos < enPos) {
    dx = clkx - XTextWidth(mfinfo, &filename[stPos], pos-stPos);
    if (dx > XTextWidth(mfinfo, &filename[pos], 1)/2)
      pos++;
  }

  return pos;
}


/***************************************************/
static void moveInsertionPoint(int clkx)
{
  curPos = posOfCoordinate(clkx);
  selPos=selLen = 0;

  showFName();
}


/***************************************************/
static void unselect(void)
{
  selPos=selLen = 0;
  ReleaseSelection(XA_PRIMARY);
}


/***************************************************/
static void removeSelectedRange(void)
{
  if (selLen > 0) {
    int len;
    len = strlen(filename);
    xvbcopy(&filename[selPos + selLen], &filename[selPos], (size_t)(len - (selPos + selLen)));
    filename[len - selLen] = '\0';
    curPos = selPos;
  }
  unselect();
}


/***************************************************/
static void pasteIntoBox(const char *text)
{
  int len, cleanlen;
  int tpos, cpos;
  char ch;
  char *clean;

  removeSelectedRange();

  len = strlen(filename);

  clean = malloc(strlen(text)+1);
  if (!clean) FatalError("out of memory!\n");

  cpos = 0;
  for (tpos=0; text[tpos]!='\0'; tpos++) {
    ch = text[tpos];

    /* skip non-printable characters */
    if (ch<' ' || ch>='\177') continue;

    /* note: only allow 'piped commands' in savemode... */
    /* only allow vertbars in 'piped commands', not filenames */
    if (ch=='|' && curPos+cpos>0 && !ISPIPE(filename[0])) continue;

    /* stop if we fill up the filename array */
    if (len + cpos >= MAXFNLEN-1) break;

    clean[cpos] = ch;
    cpos++;
  }
  clean[cpos] = '\0';
  cleanlen = cpos;

  xvbcopy(&filename[curPos], &filename[curPos+cleanlen], (size_t) (len+1-curPos));
  xvbcopy(clean, &filename[curPos], cleanlen);
  curPos += cleanlen;

  free(clean);

  scrollToFileName();
  showFName();
}


/***************************************************/
void SelectDir(int n)
{
  /* called when entry #n in the dir list was selected/double-clicked */

  /* if n<0, nothing was double-clicked, but perhaps the selection
     has changed.  Copy the selection to the filename if a) we're in
     the 'load' box, and b) it's not a directory name */

  if (n<0) {
    if (dList.selected>=0)
      setFName(dList.str[dList.selected]+1);
    return;
  }

  /* can just pretend 'enter' was hit on a double click, as the original
     click would've copied the string to filename */

  if (!DirCheckCD()) FakeButtonPress(&dbut[S_BOK]);
}



/***************************************************/
static void changedDirMB(int sel)
{
  if (sel != 0) {   /* changed directories */
    char tmppath[MAXPATHLEN+1], *trunc_point;

    /* end 'path' by changing trailing '/' (of dir name) to a '\0' */
    trunc_point = (dirs[(ndirs-1)-sel + 1] - 1);
    *trunc_point = '\0';

    if (path[0] == '\0') {
      /* special case:  if cd to '/', fix path (it's currently "") */
#ifdef apollo    /*** Apollo DomainOS uses // as the network root ***/
      strcpy(tmppath,"//");
#else
      strcpy(tmppath,"/");
#endif
    }
    else strcpy(tmppath, path);

#ifdef VMS
    /*
     *  The VMS chdir always needs 2 components (device and directory),
     *  so convert "/device" to "/device/000000" and convert
     *  "/" to "/XV_Root_Device/000000" (XV_Root_Device will need to be
     *  a special concealed device setup to provide a list of available
     *  disks).
     */
    if ( ((ndirs-sel) == 2) && (strlen(tmppath) > 1) )
      strcat ( tmppath, "/000000" ); /* add root dir for device */
    else if  ((ndirs-sel) == 1 ) {
      strcpy ( tmppath, "/XV_Root_Device/000000" );  /* fake top level */
    }
#endif

#ifdef AUTO_EXPAND
    if (Chvdir(tmppath)) {
#else
    if (chdir(tmppath)) {
#endif
      char str[MAXPATHLEN + 128];
      sprintf(str,"Unable to cd to '%s'\n", tmppath);
      *trunc_point = '/';  /* restore the path */
      MBRedraw(&dirMB);
      ErrPopUp(str, "\nWhatever");
    }
    else {
      loadCWD();
    }
  }
}


/***************************************************/
static void RedrawDList(int delta, SCRL *sptr)
{
  XV_UNUSED(sptr);
  LSRedraw(&dList,delta);
}


/***************************************************/
static void loadCWD(void)
{
  /* loads up current-working-directory into load/save list */

  xv_getwd(path, sizeof(path));
  LoadCurrentDirectory();
}


/***************************************************/
void LoadCurrentDirectory(void)
{
  /* rescans current load/save directory */

  DIR           *dirp;
  int            i, j, ftype, mode, changedDir;
  struct stat    st;
  char          *dbeg, *dend;
  static char    oldpath[MAXPATHLEN + 2] = { '\0' };

#ifdef NODIRENT
  struct direct *dp;
#else
  struct dirent *dp;
#endif


  /* get rid of previous file names */
  for (i=0; i<numfnames; i++) free(fnames[i]);
  numfnames = 0;

  /* get rid of old dirMBlist */
  for (i=0; i<ndirs; i++) free((char *) dirMBlist[i]);

#ifndef VMS
  if (strlen(path) == 0) xv_getwd(path, sizeof(path));  /* no dir, use cwd */
#else
  xv_getwd(path, sizeof(path));
#endif

#ifdef AUTO_EXPAND
  if (Chvdir(path)) {
#else
  if (chdir(path)) {
#endif
    ErrPopUp("Current load/save directory seems to have gone away!",
	     "\nYikes!");
#ifdef apollo
    strcpy(path,"//");
#else
    strcpy(path,"/");
#endif
#ifdef AUTO_EXPAND
    Chvdir(path);
#else
    chdir(path);
#endif
  }

  changedDir = strcmp(path, oldpath);
  strcpy(oldpath, path);

  if ((strlen(path) > (size_t) 1) && path[strlen(path)-1] != '/')
    strcat(path,"/");   /* tack on a trailing '/' to make path consistent */

  /* path will be something like: "/u3/bradley/src/weiner/whatever/" */
  /* parse path into individual directory names */
  dbeg = dend = path;
  for (i=0; i<MAXDEEP && dend; i++) {
    dend = (char *) index(dbeg,'/');  /* find next '/' char */

#ifdef apollo
    /** On apollos the path will be something like //machine/users/foo/ **/
    /** handle the initial // **/
    if ((dend == dbeg ) && (dbeg[0] == '/') && (dbeg[1] == '/')) dend += 1;
#endif

    dirs[i] = dbeg;
    dbeg = ((dend == NULL)? NULL: (dend+1));
  }
  ndirs = i-1;


  /* build dirMBlist */
  for (i=ndirs-1,j=0; i>=0; i--,j++) {
    size_t  stlen = (i<(ndirs-1)) ? dirs[i+1] - dirs[i] : strlen(dirs[i]);
    char   *copy;

    copy = malloc(stlen+1);
    if (!copy) FatalError("unable to malloc dirMBlist[]");

    strncpy(copy, dirs[i], stlen);
    copy[stlen] = '\0';
    dirMBlist[j] = copy;
  }


  lastdir = dirs[ndirs-1];
  dirMB.list = dirMBlist;
  dirMB.nlist = ndirs;
  XClearArea(theDisp, dirMB.win, dirMB.x, dirMB.y,
	     (u_int) dirMB.w + 3*dpiMult, (u_int) dirMB.h + 3*dpiMult, False);
  dirMB.w = StringWidth(dirMBlist[0]) + 2*MBDIRPAD;
  arrangeElements(savemode);
  MBRedraw(&dirMB);


  dirp = opendir(".");
  if (!dirp) {
    LSNewData(&dList, fnames, 0);
    RedrawDirW(0, 0, DirWide(), DirHigh());
    return;
  }

  WaitCursor();

  i=0;
  while ( (dp = readdir(dirp)) != NULL) {
    if (strcmp(dp->d_name, ".")==0   ||
	(strcmp(dp->d_name, "..")==0 &&
	 (strcmp(path,"/")==0 || strcmp(path,"//")==0)) ||
	strcmp(dp->d_name, THUMBDIR)==0) {
      /* skip over '.' and '..' and THUMBDIR */
    }
    else {

      if (i == MAXNAMES) {
	fprintf(stderr,
		"%s: too many directory entries.  Using only first %d.\n",
		cmd, MAXNAMES);
	break;
      }

      if ((i&31)==0) WaitCursor();

      fnames[i] = (char *) malloc(strlen(dp->d_name)+2); /* +2=ftype + '\0' */

      if (!fnames[i]) FatalError("malloc error while reading directory");
      strcpy(fnames[i]+1, dp->d_name);

      /* figure out what type of file the beastie is */
      fnames[i][0] = C_REG;   /* default to normal file, if stat fails */

#ifdef VMS
      /* For VMS we will default all files EXCEPT directories to avoid
	 the high cost of the VAX C implementation of the stat function.
	 Suggested by Kevin Oberman (OBERMAN@icdc.llnl.gov) */

      if (xv_strstr (fnames[i]+1, ".DIR") != NULL) fnames[i][0] = C_DIR;
      if (xv_strstr (fnames[i]+1, ".EXE") != NULL) fnames[i][0] = C_EXE;
      if (xv_strstr (fnames[i]+1, ".OBJ") != NULL) fnames[i][0] = C_BLK;
#else
      if (!nostat && (stat(fnames[i]+1, &st)==0)) {
	mode  = st.st_mode & 0777;     /* rwx modes */

        ftype = st.st_mode;
        if      (S_ISDIR(ftype))  fnames[i][0] = C_DIR;
        else if (S_ISCHR(ftype))  fnames[i][0] = C_CHR;
        else if (S_ISBLK(ftype))  fnames[i][0] = C_BLK;
	else if (S_ISLINK(ftype)) fnames[i][0] = C_LNK;
	else if (S_ISFIFO(ftype)) fnames[i][0] = C_FIFO;
	else if (S_ISSOCK(ftype)) fnames[i][0] = C_SOCK;
        else if (fnames[i][0] == C_REG && (mode&0111)) fnames[i][0] = C_EXE;
#ifdef AUTO_EXPAND
	else if (Isarchive(fnames[i]+1)) fnames[i][0] = C_DIR;
#endif
      }
      else {
	/* fprintf(stderr,"problems 'stat-ing' files\n");*/
	fnames[i][0] = C_REG;
      }
#endif /* VMS */

      i++;
    }
  }

  closedir(dirp);

  numfnames = i;

  qsort((char *) fnames, (size_t) numfnames, sizeof(char *), dnamcmp);

  if (changedDir) LSNewData(&dList, fnames, numfnames);
             else LSChangeData(&dList, fnames, numfnames);
  RedrawDirW(0, 0, DirWide(), DirHigh());
  SetCursors(-1);
}


/***************************************************/
void GetDirPath(char *buf)
{
  /* returns current 'dirW' path.  buf should be MAXPATHLEN long */

  strcpy(buf, path);
}


/***************************************************/
#ifdef FOO
static int cd_able(str)
    char *str;
{
  return ((str[0] == C_DIR || str[0] == C_LNK));
}
#endif /* FOO */


/***************************************************/
static int dnamcmp(const void *p1, const void *p2)
{
  char **s1, **s2;

  s1 = (char **) p1;
  s2 = (char **) p2;

#ifdef FOO
  /* sort so that directories are at beginning of list */

  /* if both dir/lnk or both NOT dir/lnk, sort on name */

  if ( ( cd_able(*s1) &&  cd_able(*s2)) ||
       (!cd_able(*s1) && !cd_able(*s2)))
    return (strcmp((*s1)+1, (*s2)+1));

  else if (cd_able(*s1)) return -1;  /* s1 is first */
  else return 1;                     /* s2 is first */
#else
  /* sort in pure alpha order */
  return(strcmp((*s1)+1, (*s2)+1));
#endif
}


/***************************************************/
static int updatePrimarySelection(void)
{
  if (selLen > 0)
    if (SetPrimaryText(dirW, &filename[selPos], selLen) == 0) {
      if (DEBUG) fprintf(stderr, "unable to set PRIMARY selection\n");
      return 0;
    }

  return 1;
}


/***************************************************/
static int updateClipboardSelection(void)
{
  if (selLen > 0)
    if (SetClipboardText(dirW, &filename[selPos], selLen) == 0) {
      if (DEBUG) fprintf(stderr, "unable to set CLIPBOARD selection\n");
      return 0;
    }

  return 1;
}


/***************************************************/
void SelectAllDirW(void)
{
  selPos = 0;
  selLen = strlen(filename);
  showFName();
  updatePrimarySelection();
}


/***************************************************/
void InactivateDirW(void)
{
  /* FIXME: it would be nice to have two selection colors:
   * one for selected and PRIMARY and one for just selected
   */
  ReleaseSelection(XA_PRIMARY);
}


/***************************************************/
void CutDirW(void)
{
  updateClipboardSelection();

  removeSelectedRange();

  scrollToFileName();
  showFName();
}


/***************************************************/
void CopyDirW(void)
{
  updateClipboardSelection();
}


/***************************************************/
void PasteDirW(void)
{
  char *text = GetClipboardText();

  if (text != NULL) {
    pasteIntoBox(text);
    free(text);
  }
}


/***************************************************/
void ClearDirW(void)
{
  removeSelectedRange();

  scrollToFileName();
  showFName();
}


/***************************************************/
int DirKey(int c)
{
  /* got keypress in dirW.  stick on end of filename */
  int len;

  len = strlen(filename);

  if (c>=' ' && c<'\177') {             /* printable characters */
    /* note: only allow 'piped commands' in savemode... */

#undef PREVENT_SPACES /* Spaces are fine in filenames. */
#ifdef PREVENT_SPACES
    /* only allow spaces in 'piped commands', not filenames */
    if (c==' ' && (!ISPIPE(filename[0]) || curPos==0)) return (-1);
#endif

    /* only allow vertbars in 'piped commands', not filenames */
    if (c=='|' && curPos!=0 && !ISPIPE(filename[0])) return(-1);

    if (len >= MAXFNLEN-1) return(-1);  /* max length of string */

    removeSelectedRange();

    xvbcopy(&filename[curPos], &filename[curPos+1], (size_t) (len-curPos+1));
    filename[curPos]=c;  curPos++;

    scrollToFileName();
  }

  else if (c=='\010') {                 /* BS */
    if (selLen > 0) {
      removeSelectedRange();
    } else {
      if (curPos==0) return(-1);          /* at beginning of str */
      xvbcopy(&filename[curPos], &filename[curPos-1], (size_t) (len-curPos+1));
      curPos--;
    }

    if (strlen(filename) > (size_t) 0) scrollToFileName();
  }

  else if (c=='\025') {                 /* ^U: clear entire line */
    filename[0] = '\0';
    curPos = 0;
    unselect();
  }

  else if (c=='\013') {                 /* ^K: clear to end of line */
    filename[curPos] = '\0';
    unselect();
  }

  else if (c=='\001') {                 /* ^A: move to beginning */
    curPos = 0;
    unselect();
  }

  else if (c=='\005') {                 /* ^E: move to end */
    curPos = len;
    unselect();
  }

  else if (c=='\004' || c=='\177') {    /* ^D or DEL: delete character at curPos */
    if (selLen > 0) {
      removeSelectedRange();
    } else {
      if (curPos==len) return(-1);
      xvbcopy(&filename[curPos+1], &filename[curPos], (size_t) (len-curPos));
    }

    if (strlen(filename) > (size_t) 0) scrollToFileName();
  }

  else if (c=='\002') {                 /* ^B: move backwards char */
    if (selLen > 0) {
      curPos = selPos;
    } else {
      if (curPos<=0) { curPos = 0; unselect(); return(-1); }
      curPos--;
    }
    unselect();
  }

  else if (c=='\006') {                 /* ^F: move forwards char */
    if (selLen > 0) {
      curPos = selPos + selLen;
    } else {
      if (curPos>=len) { curPos = len; unselect(); return(-1); }
      curPos++;
    }
    unselect();
  }

  else if (c=='\012' || c=='\015') {    /* CR or LF */
    if (!DirCheckCD()) FakeButtonPress(&dbut[S_BOK]);
  }

  else if (c=='\033') {                  /* ESC = Cancel */
    FakeButtonPress(&dbut[S_BCANC]);
  }

  else if (c=='\011') {                  /* tab = filename expansion */
    if (!autoComplete()) XBell(theDisp, 0);
    else {
      curPos = strlen(filename);
      unselect();
      scrollToFileName();
    }
  }

  else return(-1);                      /* unhandled character */

  showFName();

  /* if we cleared out filename, clear out deffname as well */
  if (!filename[0]) deffname[0] = '\0';

  return(0);
}


/***************************************************/
static int autoComplete(void)
{
  /* called to 'auto complete' a filename being entered.  If the name that
     has been entered so far is anything but a simple filename (ie, has
     spaces, pipe char, '/', etc) fails.  If it is a simple filename,
     looks through the name list to find something that matches what's already
     been typed.  If nothing matches, it fails.  If more than one thing
     matches, it sets the name to the longest string that the multiple
     matches have in common, and succeeds (and beeps).
     If only one matches, sets the string to the match and succeeds.

     returns zero on failure, non-zero on success */

  int i, firstmatch, slen, nummatch, cnt;

  slen = strlen(filename);

  /* is filename a simple filename? */
  if (slen==0  ||
      ISPIPE(filename[0])  ||
      index(filename, '/') ||
      filename[0]=='~'   ) return 0;

  for (i=0; i<dList.nstr; i++) {
    if (strncmp(filename, dList.str[i]+1, (size_t) slen) <= 0) break;
  }
  if (i==dList.nstr) return 0;
  if (strncmp(filename, dList.str[i]+1, (size_t) slen) < 0) return 0;

  /* there's a match of some sort... */
  firstmatch = i;

  /* count # of matches */
  for (i=firstmatch, nummatch=0;
       i<dList.nstr && strncmp(filename, dList.str[i]+1, (size_t) slen)==0;
       i++, nummatch++);

  if (nummatch == 1) {       /* only one match */
    strcpy(filename, dList.str[firstmatch]+1);
    return 1;
  }


  /* compute longest common prefix among the matches */
  while (dList.str[firstmatch][slen+1]!='\0') {
    filename[slen] = dList.str[firstmatch][slen+1];
    slen++;  filename[slen] = '\0';

    for (i=firstmatch, cnt=0;
	 i<dList.nstr && strncmp(filename, dList.str[i]+1, (size_t) slen)==0;
	 i++, cnt++);

    if (cnt != nummatch) {  slen--;  filename[slen] = '\0';  break; }
  }

  XBell(theDisp, 0);

  return 1;
}


/***************************************************/
static void scrollToFileName(void)
{
  int i, hi, lo, pos, cmp;

  /* called when 'fname' changes.  Tries to scroll the directory list
     so that fname would be centered in it */

  /* nothing to do if scrlbar not enabled ( <= nlines names in list) */
  if (dList.scrl.max <= 0) return;

  /* find the position in the namelist that the current name should be at
     (binary search) */

  pos = 0;  lo = 0;  hi = dList.nstr-1;
  i = strlen(filename);
  if (!i) { SCSetVal(&dList.scrl, 0); return; }

  while ((hi-lo)>=0) {
    pos = lo + (hi-lo)/2;
    cmp = strcmp(filename, dList.str[pos]+1);
    if      (cmp<0) hi = pos-1;
    else if (cmp>0) lo = pos+1;
    else break;      /* found it! */
  }

  /* set scroll position so that 'pos' will be centered in the list */
  i = pos - (dList.nlines/2);
  SCSetVal(&dList.scrl, i);
}


/***************************************************/
void RedrawDNamW(void)
{
  int cpos, i;
  int xoff;

  /* draw substring filename[stPos:enPos] and cursor */

  Draw3dRect(dnamW, 0, 0, (u_int) DNamWide() + 2*DNAMMORE - 1*dpiMult, (u_int) LINEHIGH + 4*dpiMult, R3D_IN, 2,
	     hicol, locol, infobg);

  XSetForeground(theDisp, theGC, infofg);

  xoff = DNAMMORE;
  if (selLen == 0 ||
      selPos + selLen < stPos ||
      selPos > enPos) {
    XDrawString(theDisp, dnamW, theGC, xoff, ASCENT + 3*dpiMult, &filename[stPos], enPos-stPos);
  } else {
    if (selPos > stPos) {
      XDrawString(theDisp, dnamW, theGC, xoff, ASCENT + 3*dpiMult, &filename[stPos], selPos-stPos);
      xoff += XTextWidth(mfinfo, &filename[stPos], selPos-stPos);
    }
    if (selPos + selLen > enPos) {
      XSetForeground(theDisp, theGC, infobg);
      XSetBackground(theDisp, theGC, infofg);
      XDrawImageString(theDisp, dnamW, theGC, xoff, ASCENT + 3*dpiMult, &filename[selPos], enPos-selPos);
      XSetForeground(theDisp, theGC, infofg);
      XSetBackground(theDisp, theGC, infobg);
    } else {
      XSetForeground(theDisp, theGC, infobg);
      XSetBackground(theDisp, theGC, infofg);
      XDrawImageString(theDisp, dnamW, theGC, xoff, ASCENT + 3*dpiMult, &filename[selPos], selLen);
      xoff += XTextWidth(mfinfo, &filename[selPos], selLen);
      XSetForeground(theDisp, theGC, infofg);
      XSetBackground(theDisp, theGC, infobg);
      XDrawString(theDisp, dnamW, theGC, xoff, ASCENT + 3*dpiMult, &filename[selPos + selLen], enPos - (selPos + selLen));
    }
  }

  /* draw a "there's more over here" doowah on the left */
  if (stPos > 0)
    for (i=0; i<DNAMMORE; i++)
      XDrawLine(theDisp, dnamW, theGC, i*dpiMult, 0, i*dpiMult, LINEHIGH + 5*dpiMult);

  /* draw a "there's more over here" doowah on the right */
  if ((size_t) enPos < strlen(filename))
    for (i=0; i<DNAMMORE; i++)
      XDrawLine(theDisp, dnamW, theGC, DNamWide() + DNAMMORE + i*dpiMult, 0, DNamWide() + DNAMMORE + i*dpiMult, LINEHIGH + 5*dpiMult);

  /* draw insertion point */
  if (selLen == 0) {
    cpos = DNAMMORE + XTextWidth(mfinfo, &filename[stPos], curPos-stPos);
    XDrawLine(theDisp, dnamW, theGC, cpos, 2*dpiMult,                     cpos,             2*dpiMult + CHIGH + 1*dpiMult);
    XDrawLine(theDisp, dnamW, theGC, cpos, 2*dpiMult + CHIGH + 1*dpiMult, cpos + 2*dpiMult, 2*dpiMult + CHIGH + 3*dpiMult);
    XDrawLine(theDisp, dnamW, theGC, cpos, 2*dpiMult + CHIGH + 1*dpiMult, cpos - 2*dpiMult, 2*dpiMult + CHIGH + 3*dpiMult);
  }
}


/***************************************************/
int DoSave(void)
{
  FILE *fp;
  byte *thepic, *rp, *gp, *bp;
  int   i, w, h, rv, fmt, col, nc, ptype, pfree;
  char *fullname;

  /* opens file, does appropriate color pre-processing, calls save routine
     based on chosen format.  Returns '0' if successful */

  dbut[S_BOK].lit = 1;  BTRedraw(&dbut[S_BOK]);

  fullname = GetDirFullName();

#ifdef AUTO_EXPAND
  {
      char path[MAXPATHLEN];

      GetDirPath(path);
      Mkvdir(path);
      if ((i = Isvdir(fullname)) & 01) {
	  char buf[128];
	  sprintf(buf,
		  "Sorry, you can't save file in the virtual directory, '%s'",
		  path);
	  ErrPopUp(buf, "\nBummer!");
	  return -1;
      }
      if (i & 06)
	  Rmvdir(fullname);
  }
#endif

  fmt = MBWhich(&fmtMB);
  col = MBWhich(&colMB);

  if (fmt<0 || col<0)
    FatalError("xv: no 'checked' format or color.  shouldn't happen!\n");


  if (fmt == F_FILELIST) {       /* write filename list */
    fp = OpenOutFile(fullname);
    if (!fp) {
      SetCursors(-1);
      dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
      return -1;
    }

    for (i=0; i<numnames; i++) {
      if ((i&0x3f)==0) WaitCursor();
      if (namelist[i][0] != '/') fprintf(fp, "%s/%s\n", initdir, namelist[i]);
                            else fprintf(fp, "%s\n", namelist[i]);
    }

    i = (ferror(fp)) ? 1 : 0;
    if (CloseOutFile(fp, fullname, i) == 0) {
      DirBox(0);
      XVCreatedFile(fullname);
    }

    SetCursors(-1);
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return i;
  }  /* FILELIST */


  /* handle formats that pop up 'how do you want to save this' boxes */


  if (fmt == F_PS) {   /* PostScript */
    PSSaveParams(fullname, col);
    PSDialog(1);                   /* open PSDialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }

#ifdef HAVE_JPEG
  else if (fmt == F_JPEG) {   /* JPEG */
    JPEGSaveParams(fullname, col);
    JPEGDialog(1);                 /* open JPEGDialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif

#ifdef HAVE_JP2K
  else if (fmt == F_JPC || fmt == F_JP2) {   /* JPEG 2000 */
    JP2KSaveParams(fmt, fullname, col);
    JP2KDialog(1);                 /* open JP2KDialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif

#ifdef HAVE_TIFF
  else if (fmt == F_TIFF) {   /* TIFF */
    TIFFSaveParams(fullname, col);
    TIFFDialog(1);                 /* open TIFF Dialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif

#ifdef HAVE_PNG
  else if (fmt == F_PNG) {   /* PNG */
    PNGSaveParams(fullname, col);
    PNGDialog(1);                  /* open PNG Dialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif

#ifdef HAVE_WEBP
  else if (fmt == F_WEBP) {   /* WEBP */
    WEBPSaveParams(fullname, col);
    WEBPDialog(1);                  /* open WEBP Dialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif

#ifdef HAVE_PIC2
  else if (fmt == F_PIC2) {   /* PIC2 */
    if (PIC2SaveParams(fullname, col) < 0)
	return 0;
    PIC2Dialog(1);                   /* open PIC2 Dialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif /* HAVE_PIC2 */

#ifdef HAVE_MGCSFX
  else if (fmt == F_MGCSFX) {   /* MGCSFX */
    if (MGCSFXSaveParams(fullname, col) < 0)
	return 0;
    MGCSFXDialog(1);                   /* open MGCSFX Dialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif /* HAVE_MGCSFX */


  WaitCursor();

  thepic = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rp, &gp, &bp);

  fp = OpenOutFile(fullname);
  if (!fp) {
    if (pfree) free(thepic);
    SetCursors(-1);
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return -1;
  }


  if (col == F_REDUCED) col = F_FULLCOLOR;
  rv = 0;

  switch (fmt) {

#ifdef HAVE_WEBP
  case F_WEBP:
    rv = WriteWEBP  (fp, thepic, ptype, w, h, rp,gp,bp, nc,col);
    break;
#endif

  case F_GIF:
    rv = WriteGIF   (fp, thepic, ptype, w, h, rp,gp,bp, nc,col,picComments);
    break;

  case F_PM:
    rv = WritePM    (fp, thepic, ptype, w, h, rp,gp,bp, nc,col,picComments);
    break;

  case F_PBMRAW:
    rv = WritePBM   (fp, thepic, ptype, w, h, rp,gp,bp, nc,col,1,picComments);
    break;

  case F_PBMASCII:
    rv = WritePBM   (fp, thepic, ptype, w, h, rp,gp,bp, nc,col,0,picComments);
    break;

  case F_XBM:
    rv = WriteXBM   (fp, thepic, w, h, rp, gp, bp, fullname);
    break;

  case F_SUNRAS:
    rv = WriteSunRas(fp, thepic, ptype, w, h, rp, gp, bp, nc, col,0);
    break;

  case F_BMP:
    rv = WriteBMP   (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);
    break;

  case F_WBMP:
    rv = WriteWBMP  (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);
    break;

  case F_IRIS:
    rv = WriteIRIS  (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);
    break;

  case F_TARGA:
    rv = WriteTarga (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);
    break;

  case F_XPM:
    rv = WriteXPM   (fp, thepic, ptype, w, h, rp, gp, bp, nc, col,
		     fullname, picComments);
    break;

  case F_FITS:
    rv = WriteFITS  (fp, thepic, ptype, w, h, rp, gp, bp, nc, col,
		     picComments);
    break;

  case F_ZX:		/* [JCE] Spectrum SCREEN$ */
    rv = WriteZX    (fp, thepic, ptype, w, h, rp, gp, bp, nc, col,
		     picComments);
    break;
#ifdef HAVE_MAG
  case F_MAG:
    rv = WriteMAG   (fp, thepic, ptype, w, h, rp, gp, bp, nc, col,
		     picComments);
    break;
#endif /* HAVE_MAG */
#ifdef HAVE_PIC
  case F_PIC:
    rv = WritePIC   (fp, thepic, ptype, w, h, rp, gp, bp, nc, col,
		     picComments);
    break;
#endif /* HAVE_PIC */
#ifdef HAVE_MAKI
  case F_MAKI:
    rv = WriteMAKI  (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);
    break;
#endif /* HAVE_MAKI */

#ifdef HAVE_PI
  case F_PI:
    rv = WritePi    (fp, thepic, ptype, w, h, rp, gp, bp, nc, col,
		     picComments);
    break;
#endif /* HAVE_PI */
  default:
    {
      char str[256];
      sprintf(str, "Saving to '%s' is not supported.", saveFormats[ fmt ]);
      ErrPopUp(str, "\nOk");
    }
   break;
  }


  if (CloseOutFile(fp, fullname, rv) == 0) {
    DirBox(0);
    if (!dopipe) {
      XVCreatedFile(fullname);
      StickInCtrlList(0);
    }
  }


  if (pfree) free(thepic);

  SetCursors(-1);
  dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);

  return rv;
}


/***************************************************/
void SetDirFName(const char *st)
{
  strncpy(deffname, st, (size_t) MAXFNLEN-1);
  deffname[MAXFNLEN-1] = '\0';
  setFName(st);
}


/***************************************************/
static void setFName(const char *st)
{
  /* Prevent an ASan failure. */
  /* Why is this code being called with filename == st??? */
  if (filename != st) {
    strncpy(filename, st, (size_t) MAXFNLEN-1);
  }
  filename[MAXFNLEN-1] = '\0';  /* make sure it's terminated */
  curPos = strlen(st);
  stPos = 0;  enPos = curPos;
  unselect();

  showFName();
}


/***************************************************/
static void showFName(void)
{
  int len;

  len = strlen(filename);

  if (curPos<stPos) stPos = curPos;
  if (curPos>enPos) enPos = curPos;

  if (stPos>=len) stPos = (len > 0) ? len - 1 : 0;
  if (enPos>len) enPos = len;

  /* while substring is shorter than window, inc enPos,
     or if that is maxed out, then decrement stPos
     (and leave 1 pixel of room for the insertion point) */

  while (XTextWidth(mfinfo, &filename[stPos], enPos-stPos) + 1*dpiMult <= DNamWide()) {
    if (enPos < len) enPos++;
    else if (stPos > 0) stPos--;
    else break; /* string completely visible */
  }

  /* while substring is longer than window, dec enpos, unless enpos==curpos,
     in which case, inc stpos */

  while (XTextWidth(mfinfo, &filename[stPos], enPos-stPos) + 1*dpiMult > DNamWide()) {
    if (enPos > curPos) enPos--;
    else if (stPos < curPos) stPos++;
    else break; /* did our best ... maybe one really wide character? */
  }


  if (ctrlColor) XClearArea(theDisp, dnamW, 2*dpiMult, 2*dpiMult,
			    (u_int) DNamWide() + (2*DNAMMORE - 1*dpiMult) - 3*dpiMult,
			    (u_int) LINEHIGH + ((5-1)-3)*dpiMult, False);
  else XClearWindow(theDisp, dnamW);

  RedrawDNamW();
  BTSetActive(&dbut[S_BOK], len>0);
}


/***************************************************/
char *GetDirFName(void)
{
  return (filename);
}


/***************************************************/
char *GetDirFullName(void)
{
  static char globname[MAXFNLEN+100];   /* the +100 is for ~ expansion */
  static char fullname[MAXPATHLEN+2];

  if (ISPIPE(filename[0])) strcpy(fullname, filename);
  else {
    strcpy(globname, filename);
    if (globname[0] == '~') Globify(globname);

    if (globname[0] != '/') sprintf(fullname, "%.*s%s", MAXPATHLEN-(int)strlen(globname), path, globname);
    else strcpy(fullname, globname);
  }

  return (fullname);
}


/***************************************************/
void SetDirSaveMode(int group, int bnum)
{
  if (group == F_COLORS) {
    if (picType == PIC24) {   /* disable REDUCED COLOR */
      colMB.dim[F_REDUCED] = 1;
      if (MBWhich(&colMB) == F_REDUCED) MBSelect(&colMB, F_FULLCOLOR);
    }
    else {  /* PIC8 - turn on REDUCED COLOR, if not XBM */
      if (MBWhich(&fmtMB) != F_XBM) {
	colMB.dim[F_REDUCED] = 0;
	MBRedraw(&fmtMB);
      }
    }

    if (bnum>=0) MBSelect(&colMB, bnum);
  }


  else if (group == F_FORMAT) {
    MBSelect(&fmtMB, bnum);
    if (MBWhich(&fmtMB) == F_XBM ||
	MBWhich(&fmtMB) == F_WBMP) { /* turn off all but B/W */
      colMB.dim[F_FULLCOLOR] = 1;
      colMB.dim[F_GREYSCALE] = 1;
      colMB.dim[F_BWDITHER]  = 0;
      colMB.dim[F_REDUCED]   = 1;
      MBSelect(&colMB, F_BWDITHER);
    }

#ifdef HAVE_WEBP
    else if (MBWhich(&fmtMB) == F_WEBP) { /* turn off all but FULLCOLOR */
      colMB.dim[F_FULLCOLOR] = 0;
      colMB.dim[F_GREYSCALE] = 1;
      colMB.dim[F_BWDITHER]  = 1;
      colMB.dim[F_REDUCED]   = 1;
      MBSelect(&colMB, F_FULLCOLOR);
    }
#endif

    else if (MBWhich(&fmtMB) == F_FITS) { /* turn off 'color' modes */
      colMB.dim[F_FULLCOLOR] = 1;
      colMB.dim[F_GREYSCALE] = 0;
      colMB.dim[F_BWDITHER]  = 0;
      colMB.dim[F_REDUCED]   = 1;
      MBSelect(&colMB, F_GREYSCALE);
    }

    else {                       /* turn on all */
      colMB.dim[F_FULLCOLOR] = 0;
      colMB.dim[F_GREYSCALE] = 0;
      colMB.dim[F_BWDITHER]  = 0;
      colMB.dim[F_REDUCED]   = (picType==PIC8) ? 0 : 1;
      if (picType!=PIC8 && MBWhich(&colMB)==F_REDUCED)
	MBSelect(&colMB, F_FULLCOLOR);
    }

    if (MBWhich(&fmtMB) == F_FILELIST) {
      MBSetActive(&colMB,      0);
      CBSetActive(&savenormCB, 0);
    }
    else {
      MBSetActive(&colMB,      1);
      CBSetActive(&savenormCB, 1);
    }
  }
}



/***************************************/
static void changeSuffix(void)
{
  /* see if there's a common suffix at the end of the filename.
     if there is, remember what case it was (all caps or all lower), lop
     it off, and replace it with a new appropriate suffix, in the
     same case */

  int allcaps;
  char *suffix, *sp, *dp, lowsuf[512];

  /* find the last '.' in the filename */
  suffix = (char *) rindex(filename, '.');
  if (!suffix) return;
  suffix++;  /* point to first letter of the suffix */

  /* check for all-caposity */
  for (sp = suffix, allcaps=1; *sp; sp++)
    if (islower(*sp)) allcaps = 0;

  /* copy the suffix into an all-lower-case buffer */
  for (sp=suffix, dp=lowsuf; *sp; sp++, dp++) {
    *dp = (isupper(*sp)) ? tolower(*sp) : *sp;
  }
  *dp = '\0';

  /* compare for common suffixes */
  if ((strcmp(lowsuf,"gif" )==0) ||
      (strcmp(lowsuf,"pm"  )==0) ||
      (strcmp(lowsuf,"pbm" )==0) ||
      (strcmp(lowsuf,"pgm" )==0) ||
      (strcmp(lowsuf,"ppm" )==0) ||
      (strcmp(lowsuf,"pnm" )==0) ||
      (strcmp(lowsuf,"bm"  )==0) ||
      (strcmp(lowsuf,"xbm" )==0) ||
      (strcmp(lowsuf,"ras" )==0) ||
      (strcmp(lowsuf,"bmp" )==0) ||
      (strcmp(lowsuf,"ps"  )==0) ||
      (strcmp(lowsuf,"eps" )==0) ||
      (strcmp(lowsuf,"rgb" )==0) ||
      (strcmp(lowsuf,"tga" )==0) ||
      (strcmp(lowsuf,"fits")==0) ||
      (strcmp(lowsuf,"fts" )==0) ||
#ifdef HAVE_JPEG
      (strcmp(lowsuf,"jpg" )==0) ||
      (strcmp(lowsuf,"jpeg")==0) ||
      (strcmp(lowsuf,"jfif")==0) ||
#endif
#ifdef HAVE_JP2K
      (strcmp(lowsuf,"jpc" )==0) ||
      (strcmp(lowsuf,"jp2" )==0) ||
#endif
#ifdef HAVE_TIFF
      (strcmp(lowsuf,"tif" )==0) ||
      (strcmp(lowsuf,"tiff")==0) ||
#endif
#ifdef HAVE_PNG
      (strcmp(lowsuf,"png" )==0) ||
#endif
#ifdef HAVE_WEBP
      (strcmp(lowsuf,"webp" )==0) ||
#endif
      (strcmp(lowsuf,"wbmp")==0) ||
      (strcmp(lowsuf,"xpm" )==0) ||
      (strcmp(lowsuf,"tiff")==0) ||
      (strcmp(lowsuf,"mag" )==0) ||
      (strcmp(lowsuf,"pic" )==0) ||
      (strcmp(lowsuf,"mki" )==0) ||
      (strcmp(lowsuf,"pi"  )==0) ||
      (strcmp(lowsuf,"p2"  )==0) ||
      (strcmp(lowsuf,"pcd" )==0)) {

    /* found one.  set lowsuf = to the new suffix, and tack on to filename */

    int fmt, col;
    fmt = MBWhich(&fmtMB);
    col = MBWhich(&colMB);

    if (fmt<0 || col<0) return;    /* shouldn't happen */

    switch (fmt) {
    case F_GIF:      strcpy(lowsuf,"gif");  break;
    case F_PM:       strcpy(lowsuf,"pm");   break;
    case F_PBMRAW:
    case F_PBMASCII: if (col == F_FULLCOLOR || col == F_REDUCED)
                                                  strcpy(lowsuf,"ppm");
                     else if (col == F_GREYSCALE) strcpy(lowsuf,"pgm");
                     else if (col == F_BWDITHER)  strcpy(lowsuf,"pbm");
                     break;

    case F_XBM:      strcpy(lowsuf,"xbm");  break;
    case F_SUNRAS:   strcpy(lowsuf,"ras");  break;
    case F_BMP:      strcpy(lowsuf,"bmp");  break;
    case F_WBMP:     strcpy(lowsuf,"wbmp"); break;
    case F_PS:       strcpy(lowsuf,"ps");   break;
    case F_IRIS:     strcpy(lowsuf,"rgb");  break;
    case F_TARGA:    strcpy(lowsuf,"tga");  break;
    case F_XPM:      strcpy(lowsuf,"xpm");  break;
    case F_FITS:     strcpy(lowsuf,"fts");  break;

#ifdef HAVE_JPEG
    case F_JPEG:     strcpy(lowsuf,"jpg");  break;
#endif

#ifdef HAVE_JP2K
    case F_JPC:      strcpy(lowsuf,"jpc");  break;
    case F_JP2:      strcpy(lowsuf,"jp2");  break;
#endif

#ifdef HAVE_TIFF
    case F_TIFF:     strcpy(lowsuf,"tif");  break;
#endif

#ifdef HAVE_PNG
    case F_PNG:      strcpy(lowsuf,"png");  break;
#endif

#ifdef HAVE_WEBP
    case F_WEBP:     strcpy(lowsuf,"webp");  break;
#endif

#ifdef HAVE_MAG
    case F_MAG:      strcpy(lowsuf,"mag");  break;
#endif

#ifdef HAVE_PIC
    case F_PIC:      strcpy(lowsuf,"pic");  break;
#endif

#ifdef HAVE_MAKI
    case F_MAKI:     strcpy(lowsuf,"mki");  break;
#endif

#ifdef HAVE_PI
    case F_PI:       strcpy(lowsuf,"pi");   break;
#endif

#ifdef HAVE_PIC2
    case F_PIC2:     strcpy(lowsuf,"p2");   break;
#endif
    }


    if (allcaps) {  /* upper-caseify lowsuf */
      for (sp=lowsuf; *sp; sp++)
	*sp = (islower(*sp)) ? toupper(*sp) : *sp;
    }

    /* one other case:  if the original suffix started with a single
       capital letter, make the new suffix start with a single cap */
    if (isupper(suffix[0])) lowsuf[0] = toupper(lowsuf[0]);

    strcpy(suffix, lowsuf);   /* tack onto filename */
    SetDirFName(filename);
  }

}


/***************************************************/
int DirCheckCD(void)
{
  /* checks if the current filename is a directory.  If so,
     cd's there, resets the filename to 'deffname', and returns '1'

     otherwise, does nothing and returns '0' */

  if (FNameCdable()) {
    setFName(deffname);
    return 1;
  }

  return 0;
}


/***************************************************/
static int FNameCdable(void)
{
  /* returns '1' if filename is a directory, and goes there */

  char newpath[1024];
  struct stat st;
  int retval = 0;

  newpath[0] = '\0';   /* start out empty */

  if (ISPIPE(filename[0]) || strlen(filename)==0) return 0;

  if (filename[0] == '/' || filename[0] == '~') {  /* absolute path */
    strcpy(newpath, filename);
  }
  else {  /* not an absolute pathname */
    strcpy(newpath,path);
    strcat(newpath,filename);
  }

  if (newpath[0]=='~') {    /* handle globbing */
    Globify(newpath);
  }

#ifdef VMS
  /* Convert names of form "/device.dir" to "/device/000000.DIR"  */
  if ( rindex ( newpath, '/' ) == newpath ) {
    strcpy ( rindex ( newpath, '.' ), "/000000.DIR" );
  }
#endif

#ifdef AUTO_EXPAND
  Mkvdir(newpath);
  Dirtovd(newpath);
#endif

  if (stat(newpath, &st)==0) {
    int isdir;

    isdir = S_ISDIR(st.st_mode);

    if (isdir) {
#ifdef VMS
      /* remove .DIR from the path so that false 000000 directories work */
      char *dirext;
      dirext = rindex ( newpath, '/' );
      if ( dirext == NULL ) dirext = newpath; else dirext++;
      dirext = xv_strstr ( dirext, "." );
      *dirext = '\0';
#endif

      if (chdir(newpath)==0) {
	loadCWD();  /* success! */
      }

      else {
	char str[512];

	sprintf(str,"Can't chdir to '%s'.\n\n  %s.",filename, ERRSTR(errno));
	ErrPopUp(str, "\nPity");
      }
      retval = 1;
    }
  }

  return retval;
}


/**************************************************************************/
int Globify(char *fname)
{
  /* expands ~s in file names.  Returns the name inplace 'name'.
     returns 0 if okay, 1 if error occurred (user name not found) */

  struct passwd *entry;
  char *cp, *sp, *up, uname[64], tmp[MAXFNLEN+100];

#ifdef VMS
  return 1;
#else
  if (*fname != '~') return 0; /* doesn't start with a tilde, don't expand */

  /* look for the first '/' after the tilde */
  sp = (char *) index(fname,'/');
  if (sp == 0) {               /* no '/' after the tilde */
    sp = fname+strlen(fname);  /* sp = end of string */
  }

  /* uname equals the string between the ~ and the / */
  for (cp=fname+1,up=uname; cp<sp; *up++ = *cp++);
  *up='\0';

  if (*uname=='\0') { /* no name.  substitute ~ with $HOME */
    const char *homedir;
    homedir = (const char *) getenv("HOME");
    if (homedir == NULL) homedir = ".";
    strcpy(tmp,homedir);
    strcat(tmp,sp);
  }

  else {              /* get password entry for uname */
    entry = getpwnam(uname);
    if (entry==0) return 1;       /* name not found */
    strcpy(tmp,entry->pw_dir);
    strcat(tmp,sp);
    endpwent();
  }

  strcpy(fname,tmp);  /* return expanded file name */
  return 0;
#endif  /* !VMS */
}


/***************************************/
FILE *OpenOutFile(const char *filename)
{
  /* opens file for output.  does various error handling bits.  Returns
     an open file pointer if success, NULL if failure */

  FILE *fp = NULL;
  struct stat st;

  if (!filename || filename[0] == '\0') return NULL;
  strcpy(outFName, filename);
  dopipe = 0;

  /* make sure we're in the correct directory */
#ifdef AUTO_EXPAND
  if (strlen(path))
    if (Chvdir(path))
      return NULL;
#else
  if (strlen(path))
    if (chdir(path) == -1)
      return NULL;
#endif

  if (ISPIPE(filename[0])) {   /* do piping */
    /* make up some bogus temp file to put this in */
#ifndef VMS
    sprintf(outFName, "%s/xvXXXXXX", tmpdir);
#else
    strcpy(outFName, "[]xvXXXXXX.lis");
#endif
#ifdef USE_MKSTEMP
    fp = fdopen(mkstemp(outFName), "w");
#else
    mktemp(outFName);
#endif
    dopipe = 1;
  }


#ifdef USE_MKSTEMP  /* (prior) nonexistence of file is already guaranteed by */
  if (!dopipe)      /*  mkstemp(), but now mkstemp() itself has created it */
#endif
    /* see if file exists (i.e., we're overwriting) */
    if (stat(outFName, &st)==0) {   /* stat succeeded, file must exist */
      static const char *labels[] = { "\nOk", "\033Cancel" };
      char               str[512];

      sprintf(str,"Overwrite existing file '%s'?", outFName);
      if (PopUp(str, labels, 2)) return NULL;
    }


  /* Open file (if not already open via mkstemp()) */
#ifdef USE_MKSTEMP
  if (!dopipe)
#endif
    fp = fopen(outFName, "w");

  if (!fp) {
    char  str[512];
    sprintf(str,"Can't write file '%s'\n\n  %s.",outFName, ERRSTR(errno));
    ErrPopUp(str, "\nBummer");
    return NULL;
  }

  return fp;
}


/***************************************/
int CloseOutFileWhy(FILE *fp, const char *filename, int failed, const char *why)
{
  char buf[64];

  /* close output file, and if piping, deal... Returns '0' if everything OK */

  if (failed) {    /* failure during format-specific output routine */
    char  str[2048];
    if (why)
	    snprintf(str, 2048, "Couldn't write file '%s' (%s).", outFName, 
		why);
    else
	    snprintf(str, 2048, "Couldn't write file '%s'.", outFName);
    ErrPopUp(str, "\nBummer!");
    unlink(outFName);   /* couldn't properly write file:  delete it */
    return 1;
  }


  if (fclose(fp) == EOF) {
    char  str[512];
    sprintf(str,"Can't close file '%s'\n\n  %s.",outFName, ERRSTR(errno));
    ErrPopUp(str, "\nWeird!");
    return 1;
  }

  buf[0]= '\0';  /* empty buffer */
  { /* compute size of written file */
    FILE *fp;
    long  filesize;
    fp = fopen(outFName,"r");
    if (fp) {
      fseek(fp, 0L, 2);
      filesize = ftell(fp);
      fclose(fp);

      sprintf(buf,"  (%ld bytes)", filesize);
    }
  }

  SetISTR(ISTR_INFO,"Successfully wrote '%s'%s", outFName, buf);

  if (dopipe) {
    char cmd[512], str[1024];
    int  i;

#ifndef VMS
    sprintf(cmd, "cat %s |%s", outFName, filename+1);  /* lose pipe char */
#else
    sprintf(cmd, "Print /Queue = XV_Queue /Delete %s", outFName);
#endif
    sprintf(str,"Doing command: '%s'", cmd);
    OpenAlert(str);
    i = system(cmd);

#ifdef VMS
    i = !i;
#endif

    if (i) {
      sprintf(str, "Unable to complete command:\n  %s", cmd);
      CloseAlert();
      ErrPopUp(str, "\nThat Sucks!");
      unlink(outFName);
      return 1;
    }
    else {
      CloseAlert();
      SetISTR(ISTR_INFO,"Successfully completed command.");
#ifndef VMS
      unlink(outFName);
#endif
    }
  }

  /* save old info */
  haveoldinfo = 1;
  oldformat = MBWhich(&fmtMB);
  oldcolors = MBWhich(&colMB);
  strcpy(oldfname, filename);

  return 0;
}




static byte rBW[2], gBW[2], bBW[2];
static byte gray[256];

/***************************************/
static byte *handleBWandReduced(byte *pic, int ptype, int pw, int ph, int color, int *nc, byte **rpp, byte **gpp, byte **bpp)
{
  /* given 'color mode' (F_FULLCOLOR, etc.), we may have to dither
     and/or use different colormaps.  Returns 'nc', rpp, gpp, bpp (the
     colormap to use).  Also, if the function returns non-NULL, it generated
     a new (dithered) image to use. */

  int   i;
  byte *bwpic;

  bwpic = (byte *) NULL;
  *nc = numcols;  *rpp = rMap;  *gpp = gMap;  *bpp = bMap;

  /* quick check:  if we're saving a 24-bit image, then none of this
     complicated 'reduced'/dithered/smoothed business comes into play.
     'reduced' is disabled, for semi-obvious reasons, in 24-bit mode,
     as is 'dithered'.  If 'smoothed', and we're saving at current
     size, no problem.  Otherwise, if we're saving at original size,
     smoothing should have no effect, so there's no reason to smooth
     the original pic...

     In any event:  in 24-bit mode, all we have to do here is determine
     if we're saving B/W DITHERED, and deal accordingly */


  if (ptype == PIC24) {
    if (color != F_BWDITHER) return NULL;
    else {                                /* generate a bw-dithered version */
      byte *p24, *thepic;

      thepic = pic;
      p24 = GammifyPic24(thepic, pw, ph);
      if (p24) thepic = p24;

      /* generate a FSDithered 1-byte per pixel image */
      bwpic = FSDither(thepic, PIC24, pw, ph, NULL,NULL,NULL, 0, 1);
      if (!bwpic) FatalError("unable to malloc dithered picture (DoSave)");

      if (p24) free(p24);  /* won't need it any more */

      /* build a BW colormap */
      rBW[0] = gBW[0] = bBW[0] = 0;
      rBW[1] = gBW[1] = bBW[1] = 255;

      *rpp = rBW;  *gpp = gBW;  *bpp = bBW;
      *nc = 2;

      return bwpic;
    }
  }



  /* ptype == PIC8 ... */

  *nc = numcols;  *rpp = rMap;  *gpp = gMap;  *bpp = bMap;
  if (color==F_REDUCED) { *rpp = rdisp;  *gpp = gdisp;  *bpp = bdisp; }

  /* if DITHER or SMOOTH, and color==FULLCOLOR or GREY,
     make color=REDUCED, so it will be written with the correct colortable  */

  if ((epicMode == EM_DITH || epicMode == EM_SMOOTH) && color != F_REDUCED) {
    if (color == F_FULLCOLOR) {
      *rpp = rdisp;  *gpp = gdisp;  *bpp = bdisp;
    }
    else if (color == F_GREYSCALE) {
      for (i=0; i<256; i++) gray[i] = MONO(rdisp[i], gdisp[i], bdisp[i]);
      *rpp = gray;  *gpp = gray;  *bpp = gray;
    }
  }




  if (color==F_BWDITHER || (ncols==0 && color==F_REDUCED) ) {
    /* if we're saving as 'dithered', or we're viewing as dithered
       and we're saving 'reduced' then generate a dithered image */

    if (numcols==2) return NULL;     /* already dithered */

    /* generate a dithered 1-byte per pixel image */
    bwpic = FSDither(pic, PIC8, pw, ph, rMap,gMap,bMap, 0, 1);
    if (!bwpic) FatalError("unable to malloc dithered picture (DoSave)");

    /* put a BW colormap */
    rBW[0] = (blkRGB>>16)&0xff;     rBW[1] = (whtRGB>>16)&0xff;
    gBW[0] = (blkRGB>>8)&0xff;      gBW[1] = (whtRGB>>8)&0xff;
    bBW[0] = blkRGB&0xff;           bBW[1] =  whtRGB&0xff;
    *rpp = rBW;  *gpp = gBW;  *bpp = bBW;
    *nc = 2;
  }

  return bwpic;
}


/***************************************/
static byte *handleNormSel(int *pptype, int *pwide, int *phigh, int *pfree)
{
  /* called to return a pointer to a 'pic', its type, its width & height,
   * and whether or not it should be freed when we're done with it.  The 'pic'
   * returned is the desired portion of 'cpic' or 'epic' if there is a
   * selection, and the saveselCB is enabled, or alternately, it's the
   * whole cpic or epic.
   *
   * if selection does not intersect cpic/epic, returns cpic/epic
   * NEVER RETURNS NULL
   */

  byte *thepic;
  int   pw, ph, slx, sly, slw, slh;

  *pfree = 0;  *pptype = picType;

  if (savenormCB.val) { thepic = cpic;  pw = cWIDE;  ph = cHIGH; }
                 else { thepic = epic;  pw = eWIDE;  ph = eHIGH; }

  *pwide = pw;  *phigh = ph;


  if (saveselCB.active && saveselCB.val && HaveSelection()) {
    GetSelRCoords(&slx, &sly, &slw, &slh);    /* in 'pic' coords */

    if (savenormCB.val) {
      CropRect2Rect(&slx, &sly, &slw, &slh, 0,0,pWIDE,pHIGH);

      if (slw<1 || slh<1) { slx = sly = 0;  slw=pWIDE;  slh=pHIGH; }

      if (slx==0 && sly==0 && slw==pWIDE && slh==pHIGH) thepic = pic;
      else {
	thepic = XVGetSubImage(pic, *pptype, pWIDE,pHIGH, slx, sly, slw, slh);
	*pfree = 1;
      }
    }
    else {                                    /* convert sel -> epic coords */
      int x1,x2,y1,y2;
      x1 = slx;        y1 = sly;
      x2 = slx + slw;  y2 = sly + slh;
      CoordP2E(x1,y1, &x1, &y1);
      CoordP2E(x2,y2, &x2, &y2);
      slx = x1;  sly = y1;  slw = x2-x1;  slh = y2-y1;
      CropRect2Rect(&slx, &sly, &slw, &slh, 0,0,pw,ph);

      if (slw<1 || slh<1) { slx = sly = 0;  slw=pw;  slh=ph; }

      if (slx!=0 || sly!=0 || slw!=pw || slh!=ph) {
	thepic = XVGetSubImage(thepic, *pptype, pw, ph, slx, sly, slw, slh);
	*pfree = 1;
      }
    }

    *pwide = slw;  *phigh = slh;
  }

  return thepic;
}


/***************************************/
byte *GenSavePic(int *ptypeP, int *wP, int *hP, int *freeP, int *ncP, byte **rmapP, byte **gmapP, byte **bmapP)
{
  /* handles the whole ugly mess of the various save options.
   * returns an image, of type 'ptypeP', size 'wP,hP'.
   * if (*ptypeP == PIC8), also returns numcols 'ncP', and the r,g,b map
   * to use rmapP, gmapP, bmapP.
   *
   * if freeP is set, image can safely be freed after it is saved
   */

  byte *pic1, *pic2;
  int   ptype, w, h, pfree;

  pic1 = handleNormSel(&ptype, &w, &h, &pfree);

  pic2 = handleBWandReduced(pic1, ptype, w,h, MBWhich(&colMB),
			      ncP, rmapP, gmapP, bmapP);
  if (pic2) {
    if (pfree) free(pic1);
    pic1  = pic2;
    pfree = 1;
    ptype = PIC8;
  }


  if (ptype == PIC24) {
    pic2 = GammifyPic24(pic1, w, h);
    if (pic2) {
      if (pfree) free(pic1);
      pic1  = pic2;
      pfree = 1;
    }
  }

  *ptypeP = ptype;  *wP = w;  *hP = h;  *freeP = pfree;

  return pic1;
}


/***************************************/
void GetSaveSize(int *wP, int *hP)
{
  /* returns the size (in pixels) of the save image.  Takes 'normal size'
     and 'save selection' checkboxes into account */

  int slx,sly,slw,slh;

  if (savenormCB.val) { slw = cWIDE;  slh = cHIGH; }
                 else { slw = eWIDE;  slh = eHIGH; }

  if (saveselCB.active && saveselCB.val && HaveSelection()) {
    GetSelRCoords(&slx, &sly, &slw, &slh);                 /* pic coord     */
    if (savenormCB.val) {
      CropRect2Rect(&slx, &sly, &slw, &slh, 0,0,pWIDE,pHIGH);
      if (slw<1 || slh<1) { slx = sly = 0;  slw=pWIDE;  slh=pHIGH; }
    }
    else {                                                 /* -> epic coord */
      int x1,x2,y1,y2;
      x1 = slx;        y1 = sly;
      x2 = slx + slw;  y2 = sly + slh;
      CoordP2E(x1,y1, &x1, &y1);
      CoordP2E(x2,y2, &x2, &y2);
      slx = x1;  sly = y1;  slw = x2-x1;  slh = y2-y1;
      CropRect2Rect(&slx, &sly, &slw, &slh, 0,0,eWIDE,eHIGH);

      if (slw<1 || slh<1) { slx = sly = 0;  slw=eWIDE;  slh=eHIGH; }
    }
  }

  *wP = slw;  *hP = slh;
}




/*************************************************************/
/*       POLLING ROUTINES                                    */
/*************************************************************/


static struct stat origStat, lastStat;
static int haveStat = 0, haveLastStat = 0;
static time_t lastchgtime;


/****************************/
void InitPoll(void)
{
  /* called whenever a file is initially loaded.  stat's the file and puts
     the results in origStat */

  haveStat = haveLastStat = 0;
  lastchgtime = (time_t) 0;

  /* only do stat() if curname is a valid index, and it's not '<stdin>' */
  if (curname>=0 && curname<numnames &&
      (strcmp(namelist[curname], STDINSTR)!=0)) {

    if (stat(namelist[curname], &origStat)==0) {
      haveStat = 1;
      if (DEBUG) fprintf(stderr," origStat.size=%ld,  origStat.mtime=%ld\n",
			 (long)origStat.st_size, (long)origStat.st_mtime);
    }
  }
}


/****************************/
int CheckPoll(int del)
{
  /* returns '1' if the file has been modified, and either
      A) the file has stabilized (st = lastStat), or
      B) 'del' seconds have gone by since the file last changed size
   */

  struct stat st;
  time_t nowT;

  time(&nowT);

  if (haveStat && curname>=0 && curname<numnames &&
      (strcmp(namelist[curname], STDINSTR)!=0)) {

    if (stat(namelist[curname], &st)==0) {
      if (DEBUG) fprintf(stderr," st.size=%ld,  st.mtime=%ld\n",
			 (long)st.st_size, (long)st.st_mtime);

      if ((st.st_size  == origStat.st_size) &&
	  (st.st_mtime == origStat.st_mtime)) return 0;  /* no change */

      /* if it's changed since last looked ... */
      if (!haveLastStat ||
	  st.st_size  != lastStat.st_size  ||
	  st.st_mtime != lastStat.st_mtime)   {
	xvbcopy((char *) &st, (char *) &lastStat, sizeof(struct stat));
	haveLastStat = 1;
	lastchgtime = nowT;
	return 0;
      }

      /* if it hasn't changed in a while... */
      if (haveLastStat && st.st_size > 0 && (nowT - lastchgtime) > del) {
	xvbcopy((char *) &st, (char *) &origStat, sizeof(struct stat));
	haveLastStat = 0;  lastchgtime = 0;
	return 1;
      }
    }
  }

  return 0;
}


/***************************************************************/
void DIRDeletedFile(char *name)
{
  /* called when file 'name' has been deleted.  If any of the browsers
     were showing the directory that the file was in, does a rescan() */

  char buf[MAXPATHLEN + 2], *tmp;

  strcpy(buf, name);
  tmp = (char *) BaseName(buf);  /* intentionally losing constness */
  *tmp = '\0';     /* truncate after last '/' */

  if (strcmp(path, buf)==0) LoadCurrentDirectory();
}


/***************************************************************/
void DIRCreatedFile(char *name)
{
  DIRDeletedFile(name);
}


#ifdef HAVE_PIC2
/**** Stuff for PIC2Dialog box ****/
FILE *pic2_OpenOutFile(char *filename, int *append)
{
    /* opens file for output.  does various error handling bits.  Returns
       an open file pointer if success, NULL if failure */

    FILE *fp = NULL;
    struct stat st;

    if (!filename || filename[0] == '\0')
	return (NULL);
    strcpy(outFName, filename);
    dopipe = 0;

    /* make sure we're in the correct directory */
#ifdef AUTO_EXPAND
    if (strlen(path))
        if (Chvdir(path))
            return NULL;
#else
    if (strlen(path))
        if (chdir(path) == -1)
            return NULL;
#endif

    if (ISPIPE(filename[0])) {   /* do piping */
	/* make up some bogus temp file to put this in */
#ifndef VMS
	sprintf(outFName, "%s/xvXXXXXX", tmpdir);
#else
	strcpy(outFName, "[]xvXXXXXX.lis");
#endif
#ifdef USE_MKSTEMP
	fp = fdopen(mkstemp(outFName), "w");
#else
	mktemp(outFName);
#endif
	dopipe = 1;
    }


    /* see if file exists (i.e., we're overwriting) */
    *append = 0;
#ifdef USE_MKSTEMP
    if (!dopipe)
#endif
    if (stat(outFName, &st)==0) {    /* stat succeeded, file must exist */
	if (ReadFileType(outFName) != RFT_PIC2) {
	    static const char *labels[] = { "\nOk", "\033Cancel" };
	    char               str[512];

	    sprintf(str,"Overwrite existing file '%s'?", outFName);
	    if (PopUp(str, labels, 2))
		return (NULL);
	} else {
	    static const char *labels[] = { "\nOk", "\033Cancel" };
	    char               str[512];

	    sprintf(str,"Append to existing file '%s'?", outFName);
	    if (PopUp(str, labels, 2)) {
		sprintf(str,"Overwrite existing file '%s'?", outFName);
		if (PopUp(str, labels, 2))
		    return (NULL);
	    } else
		*append = 1;
	}
    }

    /* Open file */
#ifdef USE_MKSTEMP
    if (!dopipe)
#endif
    fp = *append ? fopen(outFName, "r+") : fopen(outFName, "w");
    if (!fp) {
	char  str[512];
	sprintf(str,"Can't write file '%s'\n\n  %s.",outFName, ERRSTR(errno));
	ErrPopUp(str, "\nBummer");
	return (NULL);
    }

    return (fp);
}


/***************************************/
void pic2_KillNullFile(FILE *fp)
{
    fseek(fp, (size_t) 0, SEEK_END);
    if (ftell(fp) > 0) {
	fclose(fp);
	return;
    } else {
	fclose(fp);
	unlink(outFName);
	return;
    }
}
#endif /* HAVE_PIC2 */


#ifdef HAVE_MGCSFX
/**** Stuff for MGCSFX Dialog box ****/
/***************************************/
int OpenOutFileDesc(filename)
    char *filename;
{
  /* opens file for output.  does various error handling bits.  Returns
     an open file pointer if success, NULL if failure */

  int         fd;
  struct stat st;

  if (!filename || filename[0] == '\0') return -1;
  strcpy(outFName, filename);
  dopipe = 0;

  /* make sure we're in the correct directory */
#ifdef AUTO_EXPAND
  if (strlen(path))
    if (Chvdir(path))
      return -1;
#else
  if (strlen(path))
    if (chdir(path) == -1)
      return -1;
#endif

  if (ISPIPE(filename[0])) {   /* do piping */
    /* make up some bogus temp file to put this in */
#ifndef VMS
    sprintf(outFName, "%s/xvXXXXXX", tmpdir);
#else
    strcpy(outFName, "[]xvXXXXXX.lis");
#endif
#ifdef USE_MKSTEMP
    close(mkstemp(outFName));
#else
    mktemp(outFName);
#endif
    dopipe = 1;
  }


  /* if didn't just create it, see if file exists (i.e., we're overwriting) */
  if (!dopipe && stat(outFName, &st)==0) {   /* stat succeeded, file exists */
    static const char *labels[] = { "\nOk", "\033Cancel" };
    char               str[512];

    sprintf(str,"Overwrite existing file '%s'?", outFName);
    if (PopUp(str, labels, 2)) return -1;
  }


  /* Open file */
  fd = open(outFName, O_WRONLY | O_CREAT | O_TRUNC, (0644));
  if (fd < 0) {
    char  str[512];
    sprintf(str,"Can't write file '%s'\n\n  %s.", outFName, ERRSTR(errno));
    ErrPopUp(str, "\nBummer");
    return -1;
  }

  return fd;
}
#endif /* HAVE_MGCSFX */
