/* 
 * xvdir.c - Directory changin', file i/o dialog box
 *
 * callable functions:
 *
 *   CreateDirW(geom,bwidth)-  creates the dirW window.  Doesn't map it.
 *   DirBox(vis)            -  random processing based on value of 'vis'
 *                             maps/unmaps window, etc.
 *   ClickDirW()            -  handles mouse clicks in DirW
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


#define DIRWIDE  350               /* (fixed) size of directory window */
#define DIRHIGH  400

#define NLINES   15                /* # of lines in list control (keep odd) */
#define LISTWIDE 237               /* width of list window */
#define BUTTW    60                /* width of buttons */
#define BUTTH    24                /* height of buttons */
#define DDWIDE  (LISTWIDE-80+15)   /* max width of dirMB */
#define DNAMWIDE 252               /* width of 'file name' entry window */
#define MAXDEEP  30                /* max num of directories in cwd path */
#define MAXFNLEN 256               /* max len of filename being entered */

#define FMTLABEL "Format:"         /* label shown next to fmtMB */
#define COLLABEL "Colors:"         /* label shown next to colMB */
#define FMTWIDE  150               /* width of fmtMB */
#define COLWIDE  150               /* width of colMB */

/* NOTE: make sure these match up with F_* definitions in xv.h */
static char *saveColors[] = { "Full Color", 
			      "Greyscale",
			      "B/W Dithered",
			      "Reduced Color" };

static char *saveFormats[] = { "GIF",
#ifdef HAVE_JPEG
			       "JPEG",
#endif
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
			       MBSEP,
			       "Filename List"};


static void arrangeButts     PARM((int));
static void RedrawDList      PARM((int, SCRL *));
static void changedDirMB     PARM((int));
static int  dnamcmp          PARM((const void *, const void *));
static int  FNameCdable      PARM((void));
static void loadCWD          PARM((void));
static int  cd_able          PARM((char *));
static void scrollToFileName PARM((void));
static void setFName         PARM((char *));
static void showFName        PARM((void));
static void changeSuffix     PARM((void));
static int  autoComplete     PARM((void));

static byte *handleBWandReduced   PARM((byte *, int,int,int, int, int *, 
					byte **, byte **, byte **));
static byte *handleNormSel        PARM((int *, int *, int *, int *));


static char  *fnames[MAXNAMES];
static int    numfnames = 0, ndirs = 0;
static char   path[MAXPATHLEN+1];       /* '/' terminated */
static char   loadpath[MAXPATHLEN+1];   /* '/' terminated */
static char   savepath[MAXPATHLEN+1];   /* '/' terminated */
static char  *dirs[MAXDEEP];            /* list of directory names */
static char  *dirMBlist[MAXDEEP];       /* list of dir names in right order */
static char  *lastdir;                  /* name of the directory we're in */
static char   filename[MAXFNLEN+100];   /* filename being entered */
static char   deffname[MAXFNLEN+100];   /* default filename */

static int    savemode;                 /* if 0 'load box', if 1 'save box' */
static int    curPos, stPos, enPos;     /* filename textedit stuff */
static MBUTT  dirMB;                    /* popup path menu */
static MBUTT  fmtMB;                    /* 'format' menu button (Save only) */
static MBUTT  colMB;                    /* 'colors' menu button (Save only) */

static Pixmap d_loadPix, d_savePix;

static int  haveoldinfo = 0;
static int  oldformat, oldcolors;
static char oldfname[MAXFNLEN+100];

/* the name of the file actually opened.  (the temp file if we are piping) */
static char outFName[256];  
static int  dopipe;


/***************************************************/
void CreateDirW(geom)
     char *geom;
{
  int w, y;
  
  path[0] = '\0';

  xv_getwd(loadpath, sizeof(loadpath));
  xv_getwd(savepath, sizeof(savepath));

  
  dirW = CreateWindow("","XVdir", geom, DIRWIDE, DIRHIGH, infofg, infobg, 0);
  if (!dirW) FatalError("couldn't create 'directory' window!");

  LSCreate(&dList, dirW, 10, 5 + 3*(6+LINEHIGH) + 6, LISTWIDE, 
	   LINEHIGH*NLINES, NLINES, fnames, numfnames, infofg, infobg, 
	   hicol, locol, RedrawDList, 1, 0);

  dnamW = XCreateSimpleWindow(theDisp, dirW, 80, dList.y + (int) dList.h + 30, 
			      (u_int) DNAMWIDE+6, (u_int) LINEHIGH+5, 
			      1, infofg, infobg);
  if (!dnamW) FatalError("can't create name window");
  XSelectInput(theDisp, dnamW, ExposureMask);


  CBCreate(&browseCB,   dirW, DIRWIDE/2, dList.y + (int) dList.h + 6, 
	   "Browse", infofg, infobg, hicol,locol);

  CBCreate(&savenormCB, dirW, 220, dList.y + (int) dList.h + 6, 
	   "Normal Size", infofg, infobg,hicol,locol);

  CBCreate(&saveselCB,  dirW, 80,        dList.y + (int) dList.h + 6, 
           "Selected Area", infofg, infobg,hicol,locol);


  /* y-coordinates get filled in when window is opened */
  BTCreate(&dbut[S_BOK],     dirW, 259, 0, 80, BUTTH, 
	   "Ok",        infofg, infobg,hicol,locol);
  BTCreate(&dbut[S_BCANC],   dirW, 259, 0, 80, BUTTH, 
	   "Cancel",    infofg,infobg,hicol,locol);
  BTCreate(&dbut[S_BRESCAN], dirW, 259, 0, 80, BUTTH, 
	   "Rescan",    infofg,infobg,hicol,locol);
  BTCreate(&dbut[S_BOLDSET], dirW, 259, 0, 80, BUTTH, 
	   "Prev Set",  infofg,infobg,hicol,locol);
  BTCreate(&dbut[S_BOLDNAM], dirW, 259, 0, 80, BUTTH, 
	   "Prev Name", infofg,infobg,hicol,locol);

  SetDirFName("");
  XMapSubwindows(theDisp, dirW);
  numfnames = 0;


  /*
   * create MBUTTs *after* calling XMapSubWindows() to keep popup unmapped
   */

  MBCreate(&dirMB, dirW, 50, dList.y -(LINEHIGH+6), 
	   (u_int) DDWIDE, (u_int) LINEHIGH, NULL, NULL, 0,
	   infofg,infobg,hicol,locol);

  MBCreate(&fmtMB, dirW, DIRWIDE-FMTWIDE-10, 5,            
	   (u_int) FMTWIDE, (u_int) LINEHIGH, NULL, saveFormats, F_MAXFMTS, 
	   infofg,infobg,hicol,locol);
  fmtMB.hascheck = 1;
  MBSelect(&fmtMB, 0);

  MBCreate(&colMB, dirW, DIRWIDE-COLWIDE-10, 5+LINEHIGH+6, 
	   (u_int) COLWIDE, (u_int) LINEHIGH, NULL, saveColors, F_MAXCOLORS, 
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
void DirBox(mode)
     int mode;
{
  static int firstclose = 1;

  if (!mode) {
    if (savemode) strcpy(savepath, path);
             else strcpy(loadpath, path);

    if (firstclose) {
      strcpy(loadpath, path);
      strcpy(savepath, path);
      firstclose = 0;
    }

    XUnmapWindow(theDisp, dirW);  /* close */
  }

  else if (mode == BLOAD) {
    strcpy(path, loadpath);
    WaitCursor();  LoadCurrentDirectory();  SetCursors(-1);

    XStoreName(theDisp, dirW, "xv load");
    XSetIconName(theDisp, dirW, "xv load");

    dbut[S_BLOADALL].str = "Load All";
    BTSetActive(&dbut[S_BLOADALL], 1);

    arrangeButts(mode);

    MBSetActive(&fmtMB, 0);
    MBSetActive(&colMB, 0);

    CenterMapWindow(dirW, dbut[S_BOK].x+30, dbut[S_BOK].y + BUTTH/2,
		    DIRWIDE, DIRHIGH);

    savemode = 0;
  }

  else if (mode == BSAVE) {
    strcpy(path, savepath);
    WaitCursor();  LoadCurrentDirectory();  SetCursors(-1);

    XStoreName(theDisp, dirW, "xv save");
    XSetIconName(theDisp, dirW, "xv save");

    dbut[S_BOLDSET].str = "Prev Set";

    arrangeButts(mode);

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

    CenterMapWindow(dirW, dbut[S_BOK].x+30, dbut[S_BOK].y + BUTTH/2,
		    DIRWIDE, DIRHIGH);

    savemode = 1;
  }

  scrollToFileName();

  dirUp = mode;
  BTSetActive(&but[BLOAD], !dirUp);
  BTSetActive(&but[BSAVE], !dirUp);
}


/***************************************************/
static void arrangeButts(mode)
     int mode;
{
  int i, nbts, ngaps, szdiff, top, gap;

  nbts = (mode==BLOAD) ? S_LOAD_NBUTTS : S_NBUTTS;
  ngaps = nbts-1;

  szdiff = dList.h - (nbts * BUTTH);
  gap    = szdiff / ngaps;

  if (gap>16) {
    gap = 16;
    top = dList.y + (dList.h - (nbts*BUTTH) - (ngaps*gap))/2;
    
    for (i=0; i<nbts; i++) dbut[i].y = top + i*(BUTTH+gap);
  }
  else {
    for (i=0; i<nbts; i++) 
      dbut[i].y = dList.y + ((dList.h-BUTTH)*i) / ngaps;
  }
}
    


/***************************************************/
void RedrawDirW(x,y,w,h)
     int x,y,w,h;
{
  int        i, ypos, txtw;
  char       foo[30], *str;
  XRectangle xr;

  if (dList.nstr==1) strcpy(foo,"1 file");
                else sprintf(foo,"%d files",dList.nstr);

  ypos = dList.y + dList.h + 8 + ASCENT;
  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp, dirW, theGC, 10, ypos-ASCENT, 
		 (u_int) DIRWIDE, (u_int) CHIGH);
  XSetForeground(theDisp, theGC, infofg);
  DrawString(dirW, 10, ypos, foo);


  if (dirUp == BLOAD) str = "Load file:";  
                 else str = "Save file:";
  DrawString(dirW, 10, dList.y + (int) dList.h + 30 + 4 + ASCENT, str);
  
  /* draw dividing line */
  XSetForeground(theDisp,    theGC, infofg);
  XDrawLine(theDisp, dirW,   theGC, 0, dirMB.y-6, DIRWIDE, dirMB.y-6);
  if (ctrlColor) {
    XSetForeground(theDisp,  theGC, locol);
    XDrawLine(theDisp, dirW, theGC, 0, dirMB.y-5, DIRWIDE, dirMB.y-5);
    XSetForeground(theDisp,  theGC, hicol);
  }
  XDrawLine(theDisp, dirW,   theGC, 0, dirMB.y-4, DIRWIDE, dirMB.y-4);
  
  
  
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
	      10, (dirMB.y-6)/2 - d_load_height/2);

    XSetFillStyle(theDisp, theGC, FillStippled);
    XSetStipple(theDisp, theGC, dimStip);
    DrawString(dirW, fmtMB.x-6-txtw, 5+3+ASCENT, FMTLABEL);
    DrawString(dirW, fmtMB.x-6-txtw, 5+3+ASCENT + (LINEHIGH+6), COLLABEL);
    XSetFillStyle(theDisp,theGC,FillSolid);

    CBRedraw(&browseCB);
  }
  else {
    XCopyArea(theDisp, d_savePix, dirW, theGC, 0,0,d_save_width,d_save_height,
	      10, (dirMB.y-6)/2 - d_save_height/2);

    XSetForeground(theDisp, theGC, infofg);
    DrawString(dirW, fmtMB.x-6-txtw, 5+3+ASCENT, FMTLABEL);
    DrawString(dirW, fmtMB.x-6-txtw, 5+3+ASCENT + (LINEHIGH+6), COLLABEL);

    CBRedraw(&savenormCB);
    CBRedraw(&saveselCB);
  }
}


/***************************************************/
int ClickDirW(x,y)
int x,y;
{
  BUTT  *bp;
  int    bnum,i,maxbut,v;
  char   buf[1024];

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
	    if (StringWidth(dname) > (nList.w-10-16)) {   /* truncate */
	      char *tmp;
	      int   prelen = 0;

	      tmp = dname;
	      while (1) {
		tmp = (char *) index(tmp,'/'); /* find next '/' in buf */
		if (!tmp) break;

		tmp++;                   /* move to char following the '/' */
		prelen = tmp - dname;
		if (StringWidth(tmp) <= (nList.w-10-16)) break; /* cool now */
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
  }

  return -1;
}


/***************************************************/
void SelectDir(n)
int n;
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
static void changedDirMB(sel)
     int sel;
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

    if (chdir(tmppath)) {
      char str[512];
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
static void RedrawDList(delta, sptr)
     int   delta;
     SCRL *sptr;
{
  LSRedraw(&dList,delta);
}


/***************************************************/
static void loadCWD()
{
  /* loads up current-working-directory into load/save list */

  xv_getwd(path, sizeof(path));
  LoadCurrentDirectory();
}
  


/***************************************************/
void LoadCurrentDirectory()
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
  for (i=0; i<ndirs; i++) free(dirMBlist[i]);

#ifndef VMS
  if (strlen(path) == 0) xv_getwd(path, sizeof(path));  /* no dir, use cwd */
#else
  xv_getwd(path, sizeof(path));
#endif

  if (chdir(path)) {
    ErrPopUp("Current load/save directory seems to have gone away!",
	     "\nYikes!");
#ifdef apollo
    strcpy(path,"//");
#else
    strcpy(path,"/");
#endif
    chdir(path);
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
    dbeg = dend+1;
  }
  ndirs = i-1;


  /* build dirMBlist */
  for (i=ndirs-1,j=0; i>=0; i--,j++) {
    size_t stlen = (i<(ndirs-1)) ? dirs[i+1] - dirs[i] : strlen(dirs[i]);
    dirMBlist[j] = (char *) malloc(stlen+1);
    if (!dirMBlist[j]) FatalError("unable to malloc dirMBlist[]");

    strncpy(dirMBlist[j], dirs[i], stlen);
    dirMBlist[j][stlen] = '\0';
  }
    

  lastdir = dirs[ndirs-1];
  dirMB.list = dirMBlist;
  dirMB.nlist = ndirs;
  XClearArea(theDisp, dirMB.win, dirMB.x, dirMB.y, 
	     (u_int) dirMB.w+3, (u_int) dirMB.h+3, False);
  i = StringWidth(dirMBlist[0]) + 10;
  dirMB.x = dirMB.x + dirMB.w/2 - i/2;
  dirMB.w = i;
  MBRedraw(&dirMB);


  dirp = opendir(".");
  if (!dirp) {
    LSNewData(&dList, fnames, 0);
    RedrawDirW(0,0,DIRWIDE,DIRHIGH);
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
		"%s: too many directory entries.  Only using first %d.\n",
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
  RedrawDirW(0,0,DIRWIDE,DIRHIGH);
  SetCursors(-1);
}


/***************************************************/
void GetDirPath(buf)
     char *buf;
{
  /* returns current 'dirW' path.  buf should be MAXPATHLEN long */

  strcpy(buf, path);
}


/***************************************************/
static int cd_able(str)
char *str;
{
  return ((str[0] == C_DIR || str[0] == C_LNK));
}


/***************************************************/
static int dnamcmp(p1,p2)
     const void *p1, *p2;
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
int DirKey(c)
     int c;
{
  /* got keypress in dirW.  stick on end of filename */
  int len;

  len = strlen(filename);
  
  if (c>=' ' && c<'\177') {             /* printable characters */
    /* note: only allow 'piped commands' in savemode... */

    /* only allow spaces in 'piped commands', not filenames */
    if (c==' ' && (!ISPIPE(filename[0]) || curPos==0)) return (-1);

    /* only allow vertbars in 'piped commands', not filenames */
    if (c=='|' && curPos!=0 && !ISPIPE(filename[0])) return(-1);

    if (len >= MAXFNLEN-1) return(-1);  /* max length of string */
    xvbcopy(&filename[curPos], &filename[curPos+1], (size_t) (len-curPos+1));
    filename[curPos]=c;  curPos++;

    scrollToFileName();
  }

  else if (c=='\010' || c=='\177') {    /* BS or DEL */
    if (curPos==0) return(-1);          /* at beginning of str */
    xvbcopy(&filename[curPos], &filename[curPos-1], (size_t) (len-curPos+1));
    curPos--;

    if (strlen(filename) > (size_t) 0) scrollToFileName();
  }

  else if (c=='\025') {                 /* ^U: clear entire line */
    filename[0] = '\0';
    curPos = 0;
  }

  else if (c=='\013') {                 /* ^K: clear to end of line */
    filename[curPos] = '\0';
  }

  else if (c=='\001') {                 /* ^A: move to beginning */
    curPos = 0;
  }

  else if (c=='\005') {                 /* ^E: move to end */
    curPos = len;
  }

  else if (c=='\004') {                 /* ^D: delete character at curPos */
    if (curPos==len) return(-1);
    xvbcopy(&filename[curPos+1], &filename[curPos], (size_t) (len-curPos));
  }

  else if (c=='\002') {                 /* ^B: move backwards char */
    if (curPos==0) return(-1);
    curPos--;
  }

  else if (c=='\006') {                 /* ^F: move forwards char */
    if (curPos==len) return(-1);
    curPos++;
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
static int autoComplete()
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

  /* is filename a simple filename? */
  if (strlen(filename)==0  || 
      ISPIPE(filename[0])  ||
      index(filename, '/') ||
      filename[0]=='~'   ) return 0;

  slen = strlen(filename);
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
static void scrollToFileName()
{
  int i, hi, lo, pos, cmp;

  /* called when 'fname' changes.  Tries to scroll the directory list
     so that fname would be centered in it */

  /* nothing to do if scrlbar not enabled ( <= NLINES names in list) */
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
  i = pos - (NLINES/2);
  SCSetVal(&dList.scrl, i);
}
  

/***************************************************/
void RedrawDNamW()
{
  int cpos;

  /* draw substring filename[stPos:enPos] and cursor */

  Draw3dRect(dnamW, 0, 0, (u_int) DNAMWIDE+5, (u_int) LINEHIGH+4, R3D_IN, 2, 
	     hicol, locol, infobg);

  XSetForeground(theDisp, theGC, infofg);

  if (stPos>0) {  /* draw a "there's more over here" doowah */
    XDrawLine(theDisp, dnamW, theGC, 0,0,0,LINEHIGH+5);
    XDrawLine(theDisp, dnamW, theGC, 1,0,1,LINEHIGH+5);
    XDrawLine(theDisp, dnamW, theGC, 2,0,2,LINEHIGH+5);
  }

  if ((size_t) enPos < strlen(filename)) { 
    /* draw a "there's more over here" doowah */
    XDrawLine(theDisp, dnamW, theGC, DNAMWIDE+5,0,DNAMWIDE+5,LINEHIGH+5);
    XDrawLine(theDisp, dnamW, theGC, DNAMWIDE+4,0,DNAMWIDE+4,LINEHIGH+5);
    XDrawLine(theDisp, dnamW, theGC, DNAMWIDE+3,0,DNAMWIDE+3,LINEHIGH+5);
  }

  XDrawString(theDisp, dnamW, theGC,3,ASCENT+3,filename+stPos, enPos-stPos);

  cpos = XTextWidth(mfinfo, &filename[stPos], curPos-stPos);
  XDrawLine(theDisp, dnamW, theGC, 3+cpos, 2, 3+cpos, 2+CHIGH+1);
  XDrawLine(theDisp, dnamW, theGC, 3+cpos, 2+CHIGH+1, 5+cpos, 2+CHIGH+3);
  XDrawLine(theDisp, dnamW, theGC, 3+cpos, 2+CHIGH+1, 1+cpos, 2+CHIGH+3);
}


/***************************************************/
int DoSave()
{
  FILE *fp;
  byte *thepic, *rp, *gp, *bp;
  int   i, w, h, rv, fmt, col, nc, ptype, pfree;
  char *fullname;

  /* opens file, does appropriate color pre-processing, calls save routine
     based on chosen format.  Returns '0' if successful */

  dbut[S_BOK].lit = 1;  BTRedraw(&dbut[S_BOK]);

  fullname = GetDirFullName();

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
    JPEGDialog(1);                   /* open JPEGDialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif

#ifdef HAVE_TIFF
  else if (fmt == F_TIFF) {   /* TIFF */
    TIFFSaveParams(fullname, col);
    TIFFDialog(1);                   /* open TIFF Dialog box */
    dbut[S_BOK].lit = 0;  BTRedraw(&dbut[S_BOK]);
    return 0;                      /* always 'succeeds' */
  }
#endif




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
    rv = WriteXBM   (fp, thepic, w, h, rp, gp, bp, fullname);          break;

  case F_SUNRAS:
    rv = WriteSunRas(fp, thepic, ptype, w, h, rp, gp, bp, nc, col,0);  break;

  case F_BMP:
    rv = WriteBMP   (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);    break;

  case F_IRIS:
    rv = WriteIRIS  (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);    break;
    
  case F_TARGA:
    rv = WriteTarga (fp, thepic, ptype, w, h, rp, gp, bp, nc, col);    break;
    
  case F_XPM:
    rv = WriteXPM   (fp, thepic, ptype, w, h, rp, gp, bp, nc, col, 
		     fullname, picComments);    
  case F_FITS:
    rv = WriteFITS  (fp, thepic, ptype, w, h, rp, gp, bp, nc, col, 
		     picComments);    
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
void SetDirFName(st)
     char *st;
{
  strncpy(deffname, st, (size_t) MAXFNLEN-1);
  setFName(st);
}


/***************************************************/
static void setFName(st)
     char *st;
{
  strncpy(filename, st, (size_t) MAXFNLEN-1);
  filename[MAXFNLEN-1] = '\0';  /* make sure it's terminated */
  curPos = strlen(st);
  stPos = 0;  enPos = curPos;
  
  showFName();
}


/***************************************************/
static void showFName()
{
  int len;
  
  len = strlen(filename);
  
  if (curPos<stPos) stPos = curPos;
  if (curPos>enPos) enPos = curPos;
  
  if (stPos>len) stPos = (len>0) ? len-1 : 0;
  if (enPos>len) enPos = (len>0) ? len-1 : 0;
  
  /* while substring is shorter than window, inc enPos */
  
  while (XTextWidth(mfinfo, &filename[stPos], enPos-stPos) < DNAMWIDE
	 && enPos<len) { enPos++; }

  /* while substring is longer than window, dec enpos, unless enpos==curpos,
     in which case, inc stpos */

  while (XTextWidth(mfinfo, &filename[stPos], enPos-stPos) > DNAMWIDE) {
    if (enPos != curPos) enPos--;
    else stPos++;
  }


  if (ctrlColor) XClearArea(theDisp, dnamW, 2,2, (u_int) DNAMWIDE+5-3, 
			    (u_int) LINEHIGH+4-3, False);
  else XClearWindow(theDisp, dnamW);

  RedrawDNamW();
  BTSetActive(&dbut[S_BOK], strlen(filename)!=0);
}


/***************************************************/
char *GetDirFName()
{
  return (filename);
}


/***************************************************/
char *GetDirFullName()
{
  static char globname[MAXFNLEN+100];   /* the +100 is for ~ expansion */
  static char fullname[MAXPATHLEN+2];

  if (ISPIPE(filename[0])) strcpy(fullname, filename);
  else {
    strcpy(globname, filename);
    if (globname[0] == '~') Globify(globname);
    
    if (globname[0] != '/') sprintf(fullname, "%s%s", path, globname);
    else strcpy(fullname, globname);
  }

  return (fullname);
}


/***************************************************/
void SetDirSaveMode(group, bnum)
     int group, bnum;
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
    if (MBWhich(&fmtMB) == F_XBM) { /* turn off all but B/W */
      colMB.dim[F_FULLCOLOR] = 1;
      colMB.dim[F_GREYSCALE] = 1;
      colMB.dim[F_BWDITHER]  = 0;
      colMB.dim[F_REDUCED]   = 1;
      MBSelect(&colMB, F_BWDITHER);
    }

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
static void changeSuffix()
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
      (strcmp(lowsuf,"xpm" )==0) ||
      (strcmp(lowsuf,"fits")==0) ||
      (strcmp(lowsuf,"fts" )==0) ||
      (strcmp(lowsuf,"jpg" )==0) ||
      (strcmp(lowsuf,"jpeg")==0) ||
      (strcmp(lowsuf,"jfif")==0) ||
      (strcmp(lowsuf,"tif" )==0) ||
      (strcmp(lowsuf,"tiff")==0)) {

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
    case F_PS:       strcpy(lowsuf,"ps");   break;
    case F_IRIS:     strcpy(lowsuf,"rgb");  break;
    case F_TARGA:    strcpy(lowsuf,"tga");  break;
    case F_XPM:      strcpy(lowsuf,"xpm");  break;
    case F_FITS:     strcpy(lowsuf,"fts");  break;

#ifdef HAVE_JPEG
    case F_JPEG:     strcpy(lowsuf,"jpg");  break;
#endif

#ifdef HAVE_TIFF
    case F_TIFF:     strcpy(lowsuf,"tif");  break;
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
int DirCheckCD()
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
static int FNameCdable()
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
int Globify(fname)
  char *fname;
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
    char *homedir;
    homedir = (char *) getenv("HOME");  
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
FILE *OpenOutFile(filename)
     char *filename;
{
  /* opens file for output.  does various error handling bits.  Returns
     an open file pointer if success, NULL if failure */

  FILE *fp;
  struct stat st;

  if (!filename || filename[0] == '\0') return NULL;
  strcpy(outFName, filename);
  dopipe = 0;

  /* make sure we're in the correct directory */
  if (strlen(path)) chdir(path);

  if (ISPIPE(filename[0])) {   /* do piping */
    /* make up some bogus temp file to put this in */
#ifndef VMS
    sprintf(outFName, "%s/xvXXXXXX", tmpdir);
#else
    strcpy(outFName, "[]xvXXXXXX.lis");
#endif
    mktemp(outFName);
    dopipe = 1;
  }


  /* see if file exists (ie, we're overwriting) */
  if (stat(outFName, &st)==0) {   /* stat succeeded, file must exist */
    static char *foo[] = { "\nOk", "\033Cancel" };
    char str[512];

    sprintf(str,"Overwrite existing file '%s'?", outFName);
    if (PopUp(str, foo, 2)) return NULL;
  }
    

  /* Open file */
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
int CloseOutFile(fp, filename, failed)
     FILE *fp;
     char *filename;
     int   failed;
{
  char buf[64];

  /* close output file, and if piping, deal... Returns '0' if everything OK */

  if (failed) {    /* failure during format-specific output routine */
    char  str[512];
    sprintf(str,"Couldn't write file '%s'.", outFName);
    ErrPopUp(str, "\nBummer!");
    unlink(outFName);   /* couldn't properly write file:  delete it */
    return 1;
  }

    
  if (fclose(fp) == EOF) {
    static char *foo[] = { "\nWeird!" };
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
static byte *handleBWandReduced(pic, ptype, pw, ph, color, nc, rpp, gpp, bpp)
     byte  *pic;
     int    ptype, pw, ph, color, *nc;
     byte **rpp, **gpp, **bpp;
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
static byte *handleNormSel(pptype, pwide, phigh, pfree)
     int *pptype, *pwide, *phigh, *pfree;
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
byte *GenSavePic(ptypeP, wP, hP, freeP, ncP, rmapP, gmapP, bmapP)
     int  *ptypeP, *wP, *hP, *freeP, *ncP;
     byte **rmapP, **gmapP, **bmapP;
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
void GetSaveSize(wP, hP)
     int *wP, *hP;
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
void InitPoll()
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
      if (DEBUG) fprintf(stderr," origStat.size=%ld,  origStat.mtime=%d\n", 
			 origStat.st_size, origStat.st_mtime);
    }
  }
}


/****************************/
int CheckPoll(del)
     int del;
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
      if (DEBUG) fprintf(stderr," st.size=%ld,  st.mtime=%d\n", 
			 st.st_size, st.st_mtime);

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
void DIRDeletedFile(name)
     char *name;
{
  /* called when file 'name' has been deleted.  If any of the browsers
     were showing the directory that the file was in, does a rescan() */
  
  int  i;
  char buf[MAXPATHLEN + 2], *tmp;

  strcpy(buf, name);
  tmp = BaseName(buf);
  *tmp = '\0';     /* truncate after last '/' */
  
  if (strcmp(path, buf)==0) LoadCurrentDirectory();
}


/***************************************************************/
void DIRCreatedFile(name)
     char *name;
{
  DIRDeletedFile(name);
}


