/* 
 * xvctrl.c - Control box handling functions
 *
 * callable functions:
 *
 *   CreateCtrl(geom)       -  creates the ctrlW window.  Doesn't map it.
 *   CtrlBox(vis)           -  random processing based on value of 'vis'
 *                             maps/unmaps window, etc.
 *   RedrawCtrl(x,y,w,h)    -  called by 'expose' events
 *   ClickCtrl(x,y)
 *   DrawCtrlStr()          -  called to redraw 'ISTR_INFO' string in ctrlW
 *   ScrollToCurrent()      -  called when list selection is changed 
 *
 *   LSCreate()             -  creates a listbox
 *   LSRedraw()             -  redraws 'namelist' box
 *   LSClick()              -  operates list box
 *   LSChangeData()         -  like LSNewData(), but tries not to repos list
 *   LSNewData()            -  called when strings or number of them change
 *   LSKey()                -  called to handle page up/down, arrows
 *
 */

#include "copyright.h"

#include "xv.h"

#include "bits/gray25"
#include "bits/gray50"
#include "bits/i_fifo"
#include "bits/i_chr"
#include "bits/i_dir"
#include "bits/i_blk"
#include "bits/i_lnk"
#include "bits/i_sock"
#include "bits/i_exe"
#include "bits/i_reg"
#include "bits/h_rotl"
#include "bits/h_rotr"
#include "bits/fliph"
#include "bits/flipv"
#include "bits/p10"
#include "bits/m10"
#include "bits/cut"
#include "bits/copy"
#include "bits/clear"
#include "bits/paste"
#include "bits/padimg"
#include "bits/annot"
#include "bits/uicon"
#include "bits/oicon1"
#include "bits/oicon2"
#include "bits/icon"

#define CTRLWIDE 440               /* (fixed) size of control window */
#define CTRLHIGH 348 /* 379 */

#define DBLCLKTIME 500             /* double-click speed in milliseconds */

#define INACTIVE(lptr, item) ((lptr)->filetypes && (lptr)->dirsonly && \
			      (item) >= 0 && (item) < (lptr)->nstr && \
			      (lptr)->str[(item)][0] != C_DIR && \
			      (lptr)->str[(item)][0] != C_LNK)

#define NLINES  11                 /* # of lines in list control (keep odd) */

#define BUTTW   71                 /* keep odd for 'half' buttons to work   */
#define BUTTH   24
#define SBUTTH  21

static int    ptop;                /* y-coord of top of button area in ctrlW */

static Pixmap fifoPix, chrPix, dirPix, blkPix, lnkPix, sockPix, exePix, regPix;
static Pixmap rotlPix, rotrPix, fliphPix, flipvPix, p10Pix, m10Pix;
static Pixmap cutPix, copyPix, pastePix, clearPix, oiconPix, uiconPix;
static Pixmap padPix, annotPix;

static XRectangle butrect;

/* NOTE: make these string arrays match up with their respective #defines
   in xv.h */


static char *dispMList[] = { "Raw\tr", 
			     "Dithered\td",
			     "Smooth\ts",
			     MBSEP,
			     "Read/Write Colors",
			     MBSEP,
			     "Normal Colors",
			     "Perfect Colors",
			     "Use Own Colormap",
			     "Use Std. Colormap" };

static char *rootMList[] = { "Window", 
			     "Root: tiled",
			     "Root: integer tiled",
			     "Root: mirrored",
			     "Root: integer mirrored",
			     "Root: center tiled",
			     "Root: centered",
			     "Root: centered, warp",
			     "Root: centered, brick",
    		             "Root: symmetrical tiled",
			     "Root: symmetrical mirrored" };

static char *conv24MList[] = { "8-bit mode\t\2448",
			       "24-bit mode\t\2448",
			       MBSEP,
			       "Lock current mode",
			       MBSEP,
                               "Quick 24->8",
			       "Slow 24->8",
			       "Best 24->8" };

static char *algMList[]    = { "Undo All\t\244u",
			       MBSEP,
 			       "Blur...\t\244b",
			       "Sharpen...\t\244s",
			       "Edge Detect\t\244e",
			       "Emboss\t\244m",
			       "Oil Painting\t\244o",
			       "Blend\t\244B",
			       "Copy Rotate...\t\244t",
			       "Clear Rotate...\t\244T",
			       "Pixelize...\t\244p",
			       "Spread...\t\244S",
			       "DeSpeckle...\t\244k"};

static char *sizeMList[]   = { "Normal\tn",
			       "Max Size\tm",
			       "Maxpect\tM",
			       "Double Size\t>",
			       "Half Size\t<",
			       "10% Larger\t.",
			       "10% Smaller\t,",
			       MBSEP,
			       "Set Size\tS",
			       "Re-Aspect\ta",
			       "4x3\t4",
			       "Int. Expand\tI" };

static char *windowMList[] = { "Visual Schnauzer\t^v",
			       "Color Editor\te",
			       "Image Info\ti",
			       "Image Comments\t^c",
			       "Text View\t^t",
			       MBSEP,
			       "About XV\t^a",
			       "XV Keyboard Help"};



static void drawSel      PARM((LIST *, int));
static void RedrawNList  PARM((int, SCRL *));
static void ls3d         PARM((LIST *));


/***************************************************/
void CreateCtrl(geom)
     char *geom;
{
  int i, listh, topskip;
  double skip;
  XSetWindowAttributes xswa;
  Pixmap oicon1Pix, oicon2Pix;

  ctrlW = CreateWindow("xv controls", "XVcontrols", geom, 
		       CTRLWIDE, CTRLHIGH, infofg, infobg, 0);
  if (!ctrlW) FatalError("can't create controls window!");

#ifdef BACKING_STORE
  xswa.backing_store = WhenMapped;
  XChangeWindowAttributes(theDisp, ctrlW, CWBackingStore, &xswa);
#endif

  grayTile = XCreatePixmapFromBitmapData(theDisp, rootW, (char *) gray25_bits,
		   gray25_width, gray25_height, infofg, infobg, dispDEEP);

  dimStip  = MakePix1(ctrlW, gray50_bits, gray50_width, gray50_height);
  fifoPix  = MakePix1(ctrlW, i_fifo_bits, i_fifo_width, i_fifo_height);
  chrPix   = MakePix1(ctrlW, i_chr_bits,  i_chr_width,  i_chr_height);
  dirPix   = MakePix1(ctrlW, i_dir_bits,  i_dir_width,  i_dir_height);
  blkPix   = MakePix1(ctrlW, i_blk_bits,  i_blk_width,  i_blk_height);
  lnkPix   = MakePix1(ctrlW, i_lnk_bits,  i_lnk_width,  i_lnk_height);
  sockPix  = MakePix1(ctrlW, i_sock_bits, i_sock_width, i_sock_height);
  exePix   = MakePix1(ctrlW, i_exe_bits,  i_exe_width,  i_exe_height);
  regPix   = MakePix1(ctrlW, i_reg_bits,  i_reg_width,  i_reg_height);
  rotlPix  = MakePix1(ctrlW, h_rotl_bits, h_rotl_width, h_rotl_height);
  rotrPix  = MakePix1(ctrlW, h_rotr_bits, h_rotr_width, h_rotr_height);
  fliphPix = MakePix1(ctrlW, fliph_bits,  fliph_width,  fliph_height);
  flipvPix = MakePix1(ctrlW, flipv_bits,  flipv_width,  flipv_height);
  p10Pix   = MakePix1(ctrlW, p10_bits,    p10_width,    p10_height);
  m10Pix   = MakePix1(ctrlW, m10_bits,    m10_width,    m10_height);
  cutPix   = MakePix1(ctrlW, cut_bits,    cut_width,    cut_height);
  copyPix  = MakePix1(ctrlW, copy_bits,   copy_width,   copy_height);
  pastePix = MakePix1(ctrlW, paste_bits,  paste_width,  paste_height);
  clearPix = MakePix1(ctrlW, clear_bits,  clear_width,  clear_height);
  uiconPix = MakePix1(ctrlW, uicon_bits,  uicon_width,  uicon_height);
  padPix   = MakePix1(ctrlW, padimg_bits, padimg_width, padimg_height);
  annotPix = MakePix1(ctrlW, annot_bits,  annot_width,  annot_height);

  /* make multi-plane oiconPix pixmap */
  oiconPix = XCreatePixmap(theDisp,rootW,oicon1_width,oicon1_height,dispDEEP);
  oicon1Pix = MakePix1(ctrlW, oicon1_bits,  oicon1_width,  oicon1_height);
  oicon2Pix = MakePix1(ctrlW, oicon2_bits,  oicon2_width,  oicon2_height);

  if (!grayTile  || !dimStip  || !fifoPix   || !chrPix    || !dirPix    ||
      !blkPix    || !lnkPix   || !regPix    || !rotlPix   || !fliphPix  || 
      !flipvPix  || !p10Pix   || !m10Pix    || !cutPix    || !copyPix   ||
      !pastePix  || !clearPix || !uiconPix  || !oiconPix  || !oicon1Pix ||
      !oicon2Pix || !padPix   || !annotPix) 
    FatalError("unable to create all pixmaps in CreateCtrl()\n");


  /* build multi-color XV pixmap */
  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp, oiconPix, theGC, 0,0,oicon1_width,oicon1_height);
  XSetFillStyle(theDisp, theGC, FillStippled);
  XSetStipple(theDisp, theGC, oicon1Pix);
  XSetForeground(theDisp, theGC, (ctrlColor) ? locol : infofg);
  XFillRectangle(theDisp, oiconPix, theGC, 0,0,oicon1_width,oicon1_height);
  XSetStipple(theDisp, theGC, oicon2Pix);
  XSetForeground(theDisp, theGC, (ctrlColor) ? infofg : infofg);
  XFillRectangle(theDisp, oiconPix, theGC, 0,0,oicon1_width,oicon1_height);
  XSetFillStyle(theDisp, theGC, FillSolid);
  XFreePixmap(theDisp, oicon1Pix);
  XFreePixmap(theDisp, oicon2Pix);

  

  if (ctrlColor) XSetWindowBackground(theDisp, ctrlW, locol);
            else XSetWindowBackgroundPixmap(theDisp, ctrlW, grayTile);

  listh = LINEHIGH * NLINES;

  LSCreate(&nList, ctrlW, 5, 52, (CTRLWIDE-BUTTW-18),
	   LINEHIGH*NLINES, NLINES, dispnames, numnames, 
	   infofg, infobg, hicol, locol, RedrawNList, 0, 0);
  nList.selected = 0;  /* default to first name selected */


#define BCLS infofg, infobg, hicol, locol

  /* expressions for positioning right-side buttons */

  topskip = nList.y;
  skip =  ((double) (nList.h - (CHIGH+5))) / 6.0;
  if (skip > SBUTTH+8) {  
    skip = SBUTTH + 7;  
    topskip = nList.y + (nList.h - (6*skip + (CHIGH+5))) / 2;
  }

#define R_BW1 BUTTW
#define R_BX0 (CTRLWIDE - R_BW1 - 1 - 5)
#define R_BY0 (topskip)
#define R_BY1 (topskip + (int)(1*skip))
#define R_BY2 (topskip + (int)(2*skip))
#define R_BY3 (topskip + (int)(3*skip))
#define R_BY4 (topskip + (int)(4*skip))
#define R_BY5 (topskip + (int)(5*skip))
  
  BTCreate(&but[BNEXT],    ctrlW, R_BX0, R_BY0, R_BW1, SBUTTH, "Next",   BCLS);
  BTCreate(&but[BPREV],    ctrlW, R_BX0, R_BY1, R_BW1, SBUTTH, "Prev",   BCLS);
  BTCreate(&but[BLOAD],    ctrlW, R_BX0, R_BY2, R_BW1, SBUTTH, "Load",   BCLS);
  BTCreate(&but[BSAVE],    ctrlW, R_BX0, R_BY3, R_BW1, SBUTTH, "Save",   BCLS);
  BTCreate(&but[BPRINT],   ctrlW, R_BX0, R_BY4, R_BW1, SBUTTH, "Print",  BCLS);
  BTCreate(&but[BDELETE],  ctrlW, R_BX0, R_BY5, R_BW1, SBUTTH, "Delete", BCLS);


  /* expressions for positioning bottom buttons (6x2 array) */

#define BXSPACE (BUTTW+1)
#define BYSPACE (BUTTH+1)

  ptop = CTRLHIGH - (2*BYSPACE + 5 + 4);

#define BX0 ((CTRLWIDE - (BXSPACE*6))/2)
#define BX1 (BX0 + BXSPACE)
#define BX2 (BX0 + BXSPACE*2)
#define BX3 (BX0 + BXSPACE*3)
#define BX4 (BX0 + BXSPACE*4)
#define BX5 (BX0 + BXSPACE*5)
#define BY0 (ptop+5)
#define BY1 (BY0 + BYSPACE)

  butrect.x = BX0-1;  butrect.y = BY0-1;
  butrect.width = 6*BXSPACE + 1;
  butrect.height = 2*BYSPACE + 1;

  BTCreate(&but[BCOPY],  ctrlW,BX0,            BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BCUT],   ctrlW,BX0+BUTTW/2 + 1,BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BPASTE], ctrlW,BX1,            BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BCLEAR], ctrlW,BX1+BUTTW/2 + 1,BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BDN10],  ctrlW,BX2,            BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BUP10],  ctrlW,BX2+BUTTW/2 + 1,BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BROTL],  ctrlW,BX3,            BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BROTR],  ctrlW,BX3+BUTTW/2 + 1,BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BFLIPH], ctrlW,BX4,            BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BFLIPV], ctrlW,BX4+BUTTW/2 + 1,BY0,BUTTW/2,BUTTH, "",    BCLS);
  BTCreate(&but[BGRAB],  ctrlW,BX5,            BY0,BUTTW,  BUTTH, "Grab",BCLS);


  BTCreate(&but[BPAD],    ctrlW,BX0,          BY1,BUTTW/2,BUTTH,"",BCLS);
  BTCreate(&but[BANNOT],  ctrlW,BX0+BUTTW/2+1,BY1,BUTTW/2,BUTTH,"",BCLS);

  BTCreate(&but[BCROP],   ctrlW,BX1,  BY1,BUTTW,BUTTH,"Crop",    BCLS);
  BTCreate(&but[BUNCROP], ctrlW,BX2,  BY1,BUTTW,BUTTH,"UnCrop",  BCLS);
  BTCreate(&but[BACROP],  ctrlW,BX3,  BY1,BUTTW,BUTTH,"AutoCrop",BCLS);
  BTCreate(&but[BABOUT],  ctrlW,BX4,  BY1,BUTTW,BUTTH,"About XV",BCLS);
  BTCreate(&but[BQUIT],   ctrlW,BX5,  BY1,BUTTW,BUTTH,"Quit",    BCLS);

  BTCreate(&but[BXV],     ctrlW,5,5, 100, (u_int) nList.y - 5 - 2 - 5, 
	   "", BCLS);

  SetButtPix(&but[BCOPY],  copyPix,  copy_width,   copy_height);
  SetButtPix(&but[BCUT],   cutPix,   cut_width,    cut_height);
  SetButtPix(&but[BPASTE], pastePix, paste_width,  paste_height);
  SetButtPix(&but[BCLEAR], clearPix, clear_width,  clear_height);
  SetButtPix(&but[BUP10],  p10Pix,   p10_width,    p10_height);
  SetButtPix(&but[BDN10],  m10Pix,   m10_width,    m10_height);
  SetButtPix(&but[BROTL],  rotlPix,  h_rotl_width, h_rotl_height);
  SetButtPix(&but[BROTR],  rotrPix,  h_rotr_width, h_rotr_height);
  SetButtPix(&but[BFLIPH], fliphPix, fliph_width,  fliph_height);
  SetButtPix(&but[BFLIPV], flipvPix, flipv_width,  flipv_height);
  SetButtPix(&but[BPAD],   padPix,   padimg_width, padimg_height);
  SetButtPix(&but[BANNOT], annotPix, annot_width,  annot_height);

#ifdef REGSTR
  if (ctrlColor) {
    SetButtPix(&but[BXV], oiconPix, oicon1_width,  oicon1_height);
    but[BXV].colorpix = 1;
  } 
  else SetButtPix(&but[BXV], iconPix, icon_width,  icon_height);
#else
  SetButtPix(&but[BXV], uiconPix, uicon_width,  uicon_height);
#endif

  XMapSubwindows(theDisp, ctrlW);


  /* have to create menu buttons after XMapSubWindows, as we *don't* want 
     the popup menus mapped */

  MBCreate(&dispMB,   ctrlW, CTRLWIDE - 8 - 112 - 2*(112+2), 5,112,19, 
	   "Display",    dispMList,   DMB_MAX,    BCLS);
  MBCreate(&conv24MB, ctrlW, CTRLWIDE - 8 - 112 - (112+2),   5,112,19, 
	   "24/8 Bit",   conv24MList, CONV24_MAX, BCLS);
  MBCreate(&algMB,    ctrlW, CTRLWIDE - 8 - 112,             5,112,19, 
	   "Algorithms", algMList,    ALG_MAX,    BCLS);

  MBCreate(&rootMB,   ctrlW, CTRLWIDE - 8 - 112 - 2*(112+2), 5+21,112,19, 
	   "Root",       rootMList,   RMB_MAX,    BCLS);
  MBCreate(&windowMB, ctrlW, CTRLWIDE - 8 - 112 - (112+2),   5+21,112,19, 
	   "Windows",    windowMList, WMB_MAX,    BCLS);
  MBCreate(&sizeMB,   ctrlW, CTRLWIDE - 8 - 112,             5+21,112,19, 
	   "Image Size", sizeMList,   SZMB_MAX,   BCLS);



#undef BCLS


  /* set up initial state for various controls */

  but[BXV].w = dispMB.x - 5 - but[BXV].x;

  dispMB.flags[DMB_COLRW] = (allocMode == AM_READWRITE);
  dispMB.flags[colorMapMode + DMB_COLNORM - CM_NORMAL] = 1;

  conv24MB.flags[conv24] = 1;

  if (!useroot) dispMode = RMB_WINDOW;
           else dispMode = rootMode + (RMB_ROOT - RM_NORMAL);
  rootMB.flags[dispMode] = 1;

  windowMB.dim[WMB_TEXTVIEW] = (numnames<1);

  BTSetActive(&but[BDELETE], (numnames>=1));
}

/***************************************************/
void SetButtPix(bp, pix, w,h)
     BUTT *bp;
     Pixmap pix;
     int    w,h;
{
  if (!bp) return;
  bp->pix = pix;  bp->pw = w;  bp->ph = h;
}


/***************************************************/
Pixmap MakePix1(win, bits, w, h)
     Window win;
     byte *bits;
     int   w,h;
{
  return XCreatePixmapFromBitmapData(theDisp, win, (char *) bits, 
				     (u_int) w, (u_int) h, 1L,0L,1);
}


/***************************************************/
void CtrlBox(vis)
int vis;
{
  if (vis) XMapRaised(theDisp, ctrlW);  
  else     XUnmapWindow(theDisp, ctrlW);

  ctrlUp = vis;
}


/***************************************************/
void RedrawCtrl(x,y,w,h)
int x,y,w,h;
{
  int i;
  XRectangle xr;

  RANGE(w, 0, CTRLWIDE);
  RANGE(h, 0, CTRLHIGH);

#ifdef CLIPRECT
  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);
#endif

  DrawCtrlNumFiles();

  XSetForeground(theDisp,theGC,infofg);
  XDrawRectangles(theDisp, ctrlW, theGC, &butrect, 1);

  for (i=0; i<NBUTTS; i++)
    BTRedraw(&but[i]);

  MBRedraw(&dispMB);
  MBRedraw(&conv24MB);
  MBRedraw(&algMB);
  MBRedraw(&rootMB);
  MBRedraw(&windowMB);
  MBRedraw(&sizeMB);

  DrawCtrlStr();

#ifdef CLIPRECT
  XSetClipMask(theDisp, theGC, None);
#endif
}


/***************************************************/
void DrawCtrlNumFiles()
{
  int x,y,w,h;
  char foo[40];

  x  = but[BNEXT].x;
  y  = nList.y + nList.h - (CHIGH+5);
  w  = but[BNEXT].w;

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  sprintf(foo, "%d file%s", numnames, (numnames==1) ? "" : "s");
    
  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp,ctrlW, theGC, x+1,y+1, (u_int) w-1, (u_int) CHIGH+5);

  XSetForeground(theDisp,theGC,infofg);
  XDrawRectangle(theDisp,ctrlW, theGC, x,y,     (u_int) w,   (u_int) CHIGH+6);

  Draw3dRect(ctrlW, x+1,y+1,                    (u_int) w-2, (u_int) CHIGH+4, 
	     R3D_IN, 2, hicol, locol, infobg);

  XSetForeground(theDisp,theGC,infofg);
  CenterString(ctrlW, x+w/2, y+(CHIGH+6)/2, foo);
}


/***************************************************/
void DrawCtrlStr()
{
  int   y;
  char *st,*st1;

  y = ptop - (CHIGH + 4)*2 - 2;
  st  = GetISTR(ISTR_INFO);
  st1 = GetISTR(ISTR_WARNING);

  XSetForeground(theDisp, theGC, infobg);
  XFillRectangle(theDisp, ctrlW, theGC, 0, y+1, 
		 CTRLWIDE, (u_int)((CHIGH+4)*2+1));

  XSetForeground(theDisp, theGC, infofg);
  XDrawLine(theDisp, ctrlW, theGC, 0, y,   CTRLWIDE, y);
  XDrawLine(theDisp, ctrlW, theGC, 0, y+CHIGH+4, CTRLWIDE, y+CHIGH+4);
  XDrawLine(theDisp, ctrlW, theGC, 0, y+(CHIGH+4)*2, CTRLWIDE, y+(CHIGH+4)*2);

  if (ctrlColor) {
    XSetForeground(theDisp, theGC, locol);
    XDrawLine(theDisp, ctrlW, theGC, 0, y+1,   CTRLWIDE, y+1);
    XDrawLine(theDisp, ctrlW, theGC, 0, y+CHIGH+5, CTRLWIDE, y+CHIGH+5);
    XDrawLine(theDisp, ctrlW, theGC, 0, y+(CHIGH+4)*2+1, 
	      CTRLWIDE, y+(CHIGH+4)*2+1);
  }

  if (ctrlColor) XSetForeground(theDisp, theGC, hicol);
  XDrawLine(theDisp, ctrlW, theGC, 0, y+2, CTRLWIDE, y+2);
  XDrawLine(theDisp, ctrlW, theGC, 0, y+CHIGH+6, CTRLWIDE, y+CHIGH+6);
  if (ctrlColor) XSetForeground(theDisp, theGC, infobg);
  XDrawLine(theDisp, ctrlW, theGC, 0, ptop, CTRLWIDE, ptop);

  XSetForeground(theDisp, theGC, infofg);
  DrawString(ctrlW, 10, y+ASCENT+3,       st);
  DrawString(ctrlW, 10, y+ASCENT+CHIGH+7, st1);
}


/***************************************************/
int ClickCtrl(x,y)
int x,y;
{
  BUTT *bp;
  int   i;

  for (i=0; i<NBUTTS; i++) {
    bp = &but[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }

  if (i<NBUTTS) {                   /* found one */
    if (BTTrack(bp)) return (i);    /* and it was clicked */
  }

  return -1;
}



/***************************************************/
void ScrollToCurrent(lst)
LIST *lst;
{
  /* called when selected item on list is changed.  Makes the selected 
     item visible.  If it already is, nothing happens.  Otherwise, it
     attempts to scroll so that the selection appears in the middle of 
     the list window */

  int halfway;

  if (lst->selected < 0) return;  /* no selection, do nothing */

  if (lst->selected > lst->scrl.val && 
      lst->selected <  lst->scrl.val + lst->nlines-1) LSRedraw(lst, 0);
  else {
    halfway = (lst->nlines)/2;   /* offset to the halfway pt. of the list */
    if (!SCSetVal(&lst->scrl, lst->selected - halfway)) LSRedraw(lst, 0);
  }
}


/***************************************************/
static void RedrawNList(delta, sptr)
     int delta;
     SCRL *sptr;
{
  LSRedraw(&nList, delta);
}




/***************** LIST STUFF *********************/

/***************************************************/
void LSCreate(lp, win, x, y, w, h, nlines, strlist, nstr, fg, bg, hi, lo,
	      fptr, typ, donly)
LIST         *lp;
Window        win;
int           x,y,w,h,nlines,nstr,typ,donly;
unsigned long fg, bg, hi, lo;
char        **strlist;    /* a pointer to a list of strings */

void        (*fptr)PARM((int,SCRL *));

{
  if (ctrlColor) h += 4;

  lp->win = XCreateSimpleWindow(theDisp,win,x,y,(u_int) w, (u_int) h,1,fg,bg);
  if (!lp->win) FatalError("can't create list window!");

  lp->x = x;    lp->y = y;   
  lp->w = w;    lp->h = h;
  lp->fg = fg;  lp->bg = bg;
  lp->hi = hi;  lp->lo = lo;
  lp->str      = strlist;
  lp->nstr     = nstr;
  lp->selected = -1;   /* no initial selection */
  lp->nlines   = nlines;
  lp->filetypes= typ;
  lp->dirsonly = donly;

  XSelectInput(theDisp, lp->win, ExposureMask | ButtonPressMask);

  SCCreate(&lp->scrl, lp->win, w-20, -1, 1, h, 0, 
	   nstr-nlines, 0, nlines-1, fg, bg, hi, lo, fptr);

  XMapSubwindows(theDisp, lp->win);
}



/***************************************************/
void LSChangeData(lp, strlist, nstr)
LIST         *lp;
char        **strlist;
int           nstr;
{
  /* tries to keep list selection and scrollbar in same place, if possible */

  lp->str = strlist;
  lp->nstr = nstr;
  if (lp->selected >= nstr) lp->selected = -1;

  RANGE(lp->scrl.val, 0, nstr - lp->nlines);
  SCSetRange(&lp->scrl, 0, nstr - lp->nlines, lp->scrl.val, lp->nlines-1);
}


/***************************************************/
void LSNewData(lp, strlist, nstr)
LIST         *lp;
char        **strlist;
int           nstr;
{
  lp->str = strlist;
  lp->nstr = nstr;
  lp->selected = -1;   /* no initial selection */
  SCSetRange(&lp->scrl, 0, nstr - lp->nlines, 0, lp->nlines-1);
}


/***************************************************/
static void ls3d(lp)
LIST *lp;
{
  /* redraws lists 3d-effect, which can be trounced by drawSel() */
  Draw3dRect(lp->win, 0, 0, lp->w-1, lp->h-1, R3D_IN, 2, 
	     lp->hi, lp->lo, lp->bg);
}


/***************************************************/
static void drawSel(lp,j)
LIST *lp;
int j;
{
  int i, inactive, x0,y0,wide, selected;
  unsigned long fg, bg;

  x0 = 0;  y0 = 0;  wide = lp->w;
  if (ctrlColor) { x0 = y0 = 2;  wide -= 6; }

  inactive = INACTIVE(lp,j);

  i = j - lp->scrl.val;
  if (i<0 || i>=lp->nlines) return;  /* off screen */

  selected = (j == lp->selected && !inactive && j<lp->nstr);
  if (selected) {  /* inverse colors */
    if (ctrlColor) { fg = lp->fg;  bg = lp->lo; }
              else { fg = lp->bg;  bg = lp->fg; }
  }
  else { fg = lp->fg;  bg = lp->bg; }

  XSetForeground(theDisp, theGC, bg);
  XFillRectangle(theDisp, lp->win, theGC, x0, y0+i*LINEHIGH, 
		 (u_int) wide+1, (u_int) LINEHIGH);

  if (j>=0 && j<lp->nstr) {   /* only draw string if valid */
    XSetForeground(theDisp, theGC, fg);
    XSetBackground(theDisp, theGC, bg);

    if (!lp->filetypes) 
      DrawString(lp->win, x0+3, y0+i*LINEHIGH + ASCENT + 1, lp->str[j]);
    else {
      int ypos = y0 + i*LINEHIGH + (LINEHIGH - i_fifo_height)/2;

      if (lp->str[j][0] == C_FIFO) 
	XCopyPlane(theDisp, fifoPix, lp->win, theGC, 0, 0,
		   i_fifo_width, i_fifo_height, x0+3, ypos, 1L);

      else if (lp->str[j][0] == C_CHR) 
	XCopyPlane(theDisp, chrPix, lp->win, theGC, 0, 0,
		   i_chr_width, i_chr_height, x0+3, ypos, 1L);

      else if (lp->str[j][0] == C_DIR) 
	XCopyPlane(theDisp, dirPix, lp->win, theGC, 0, 0,
		   i_dir_width, i_dir_height, x0+3, ypos, 1L);

      else if (lp->str[j][0] == C_BLK) 
	XCopyPlane(theDisp, blkPix, lp->win, theGC, 0, 0,
		   i_blk_width, i_blk_height, x0+3, ypos, 1L);

      else if (lp->str[j][0] == C_LNK) 
	XCopyPlane(theDisp, lnkPix, lp->win, theGC, 0, 0,
		   i_lnk_width, i_lnk_height, x0+3, ypos, 1L);

      else if (lp->str[j][0] == C_SOCK) 
	XCopyPlane(theDisp, sockPix, lp->win, theGC, 0, 0,
		   i_sock_width, i_sock_height, x0+3, ypos, 1L);

      else if (lp->str[j][0] == C_EXE) 
	XCopyPlane(theDisp, exePix, lp->win, theGC, 0, 0,
		   i_exe_width, i_exe_height, x0+3, ypos, 1L);

      else  /* lp->str[j][0] == C_REG */
	XCopyPlane(theDisp, regPix, lp->win, theGC, 0, 0,
		   i_reg_width, i_reg_height, x0+3, ypos, 1L);


      DrawString(lp->win, x0+3 + i_fifo_width + 3, 
		  y0+i*LINEHIGH + ASCENT + 1, 
		  lp->str[j]+1);
    }
  }
}


/***************************************************/
void LSRedraw(lp, delta)
LIST *lp;
int   delta;
{
  int  i;

  for (i = lp->scrl.val; i < lp->scrl.val + lp->nlines; i++) 
    drawSel(lp,i);
  ls3d(lp);
}


/***************************************************/
int LSClick(lp,ev)
LIST *lp;
XButtonEvent *ev;
{
  /* returns '-1' normally.  returns 0 -> numnames-1 for a goto */

  Window       rW, cW;
  int          rx, ry, x, y, sel, oldsel, y0, high;
  unsigned int mask;
  static Time  lasttime=0;
  static int   lastsel = -1;

  y0   = (ctrlColor) ? 2 : 0;
  high = (ctrlColor) ? lp->h - 4 : lp->h;

  x = ev->x;  y = ev->y;
  sel = lp->scrl.val + (y-y0)/LINEHIGH;
  if (sel >= lp->nstr) sel = lp->selected;

  /* see if it's a double click */
  if (ev->time - lasttime < DBLCLKTIME && sel==lastsel 
      && (lp->scrl.val + (y-y0)/LINEHIGH) < lp->nstr
      && !INACTIVE(lp,sel)) {
    return (sel);
  }

  lasttime = ev->time;  lastsel = sel;

  /* if not clicked on selected, turn off selected and select new one */
  if (sel != lp->selected) {
    oldsel = lp->selected;
    lp->selected = sel;
    drawSel(lp,sel);  drawSel(lp,oldsel);
    ls3d(lp);
    XFlush(theDisp);
  }

  while (XQueryPointer(theDisp,lp->win,&rW,&cW,&rx,&ry,&x,&y,&mask)) {
    if (!(mask & Button1Mask)) break;    /* button released */

    if (y<y0) { /* scroll up in list */ 
      if (lp->scrl.val > lp->scrl.min) {
	lp->selected = lp->scrl.val - 1;
	SCSetVal(&lp->scrl, lp->scrl.val - 1);
	Timer(100);
      }
    }

    else if (y>high) { /* scroll down in list */
      if (lp->scrl.val < lp->scrl.max) {
	lp->selected = lp->scrl.val + lp->nlines;
	if (lp->selected >= lp->nstr) lp->selected = lp->nstr - 1;
	SCSetVal(&lp->scrl, lp->scrl.val + 1);
	Timer(100);
      }
    }

    else {
      sel = lp->scrl.val + (y-y0)/LINEHIGH;
      if (sel >= lp->nstr) sel = lp->nstr - 1;

      if (sel != lp->selected && sel >= lp->scrl.val &&
	  sel < lp->scrl.val + lp->nlines) {  
	/* dragged to another on current page */
	oldsel = lp->selected;
	lp->selected = sel;
	drawSel(lp, sel);  drawSel(lp, oldsel);
	ls3d(lp);
	XFlush(theDisp);
      }
    }
  }

  return(-1);
}



/***************************************************/
void LSKey(lp, key)
     LIST         *lp;
     int           key;
{
  if      (key==LS_PAGEUP)   SCSetVal(&lp->scrl,lp->scrl.val - (lp->nlines-1));
  else if (key==LS_PAGEDOWN) SCSetVal(&lp->scrl,lp->scrl.val + (lp->nlines-1));
  else if (key==LS_HOME)     SCSetVal(&lp->scrl,lp->scrl.min);
  else if (key==LS_END)      SCSetVal(&lp->scrl,lp->scrl.max);
  
  else if (key==LS_LINEUP)   {
    /* if the selected item visible, but not the top line */
    if (lp->selected > lp->scrl.val && 
	lp->selected <= lp->scrl.val + lp->nlines - 1) {
      /* then just move it */
      lp->selected--;
      drawSel(lp, lp->selected);  drawSel(lp, lp->selected+1);
      ls3d(lp);
    }
    
    /* if it's the top line... */
    else if (lp->selected == lp->scrl.val) {
      if (lp->selected > 0) {
	lp->selected--;
	SCSetVal(&lp->scrl, lp->selected);
      }
    }
    
    /* if it's not visible, put it on the bottom line */
    else {
      lp->selected = lp->scrl.val + lp->nlines - 1;
      if (lp->selected >= lp->nstr) lp->selected = lp->nstr - 1;
      drawSel(lp, lp->selected);
      ls3d(lp);
    }
  }
  
  else if (key==LS_LINEDOWN)   {
    /* if the selected item visible, but not the bottom line */
    if (lp->selected >= lp->scrl.val && 
	lp->selected < lp->scrl.val + lp->nlines - 1) {
      if (lp->selected < lp->nstr-1) {
	/* then just move it */
	lp->selected++;
	drawSel(lp, lp->selected);  drawSel(lp, lp->selected-1);
	ls3d(lp);
      }
    }
    
    /* if it's the bottom line... */
    else if (lp->selected == lp->scrl.val + lp->nlines - 1) {
      if (lp->selected < lp->nstr-1) {
	lp->selected++;
	SCSetVal(&lp->scrl, lp->scrl.val+1);
      }
    }
    
    /* if it's not visible, put it on the top line */
    else {
      lp->selected = lp->scrl.val;
      drawSel(lp, lp->selected);
      ls3d(lp);
    }
  }
}

