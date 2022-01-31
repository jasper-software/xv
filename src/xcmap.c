/*
 * xcmap.c - shows the contents of the colormap on 4, 6, or 8-bit X11 displays
 *
 *  Author:    John Bradley, University of Pennsylvania
 *                (bradley@cis.upenn.edu)
 */

#define REVDATE   "Rev: 2/13/89"

/* include files */
#include <stdio.h>
#ifdef __STDC__
#  include <stdlib.h>   /* exit(), abs() */
#endif
#include <sys/types.h>
#include <ctype.h>

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#ifdef VMS
#  define index strchr
#endif

typedef unsigned char byte;

/* text centering macros for X11 */
#define CENTERX(f,x,str) ((x)-XTextWidth(f,str,strlen(str))/2)
#define CENTERY(f,y) ((y)-((f->ascent+f->descent)/2)+f->ascent)

#undef PARM
#ifdef __STDC__
#  define PARM(a) a
#else
#  define PARM(a) ()
#endif

#define FONT "8x13"


/* X stuff */
Display       *theDisp;
int           theScreen, dispcells;
Colormap      theCmap;
Window        rootW, mainW;
GC            theGC;
unsigned long fcol,bcol;
Font          mfont;
XFontStruct   *mfinfo;
Visual        *theVisual;


/* global vars */
int            WIDE,HIGH,cWIDE,cHIGH,nxcells,nycells, pvalup;
XColor         defs[256];
char          *cmd, tmpstr[128];


       int  main             PARM((int, char **));
static void HandleEvent      PARM((XEvent *));
static void Syntax           PARM((void));
static void FatalError       PARM((const char *));
static void Quit             PARM((void));
static void CreateMainWindow PARM((char *, char *, int, char **));
static void DrawWindow       PARM((int,int,int,int));
static void Resize           PARM((int,int));
static void TrackMouse       PARM((int,int));
static void DrawPixValue     PARM((int,int));

/*******************************************/
int main(argc, argv)
     int   argc;
     char *argv[];
/*******************************************/
{
  int        i;
  char      *display, *geom;
  XEvent     event;

  cmd = argv[0];
  display = geom = NULL;


  /*********************Options*********************/

  for (i = 1; i < argc; i++) {
    char *strind;

    if (!strncmp(argv[i],"-g", (size_t)2)) {	/* geometry */
      i++;
      geom = argv[i];
      continue;
    }

    if (argv[i][0] == '=') {		/* old-style geometry */
      geom = argv[i];
      continue;
    }

    if (!strncmp(argv[i],"-d",(size_t) 2)) {	/* display */
      i++;
      display = argv[i];
      continue;
    }

    strind = (char *) index(argv[i], ':');	/* old-style display */
    if(strind != NULL) {
      display = argv[i];
      continue;
    }

    Syntax();
  }


  /*****************************************************/

  /* Open up the display. */

  if ( (theDisp=XOpenDisplay(display)) == NULL)
    FatalError("can't open display");

  theScreen = DefaultScreen(theDisp);
  theCmap   = DefaultColormap(theDisp, theScreen);
  rootW     = RootWindow(theDisp,theScreen);
  theGC     = DefaultGC(theDisp,theScreen);
  fcol      = WhitePixel(theDisp,theScreen);
  bcol      = BlackPixel(theDisp,theScreen);
  theVisual = DefaultVisual(theDisp,theScreen);

  dispcells = DisplayCells(theDisp, theScreen);

  if (dispcells>256) {
    sprintf(tmpstr,"dispcells = %d.  %s",
	    dispcells, "This program can only deal with <= 8-bit displays.");
    FatalError(tmpstr);
  }
  else if (dispcells>64)
    nxcells = nycells = 16;
  else if (dispcells>16)
    nxcells = nycells = 8;
  else if (dispcells>4)
    nxcells = nycells = 4;
  else if (dispcells>2)
    nxcells = nycells = 2;
  else
  {
    nxcells = 2;
    nycells = 1;
  }

  /**************** Create/Open X Resources ***************/
  if ((mfinfo = XLoadQueryFont(theDisp,FONT))==NULL) {
    sprintf(tmpstr,"couldn't open '%s' font",FONT);
    FatalError(tmpstr);
  }

  mfont=mfinfo->fid;
  XSetFont(theDisp,theGC,mfont);
  XSetForeground(theDisp,theGC,fcol);
  XSetBackground(theDisp,theGC,bcol);

  CreateMainWindow(cmd,geom,argc,argv);
  Resize((int)WIDE,(int)HIGH);

  XSelectInput(theDisp, mainW, ExposureMask | KeyPressMask
	       | StructureNotifyMask | ButtonPressMask);
  XMapWindow(theDisp,mainW);

  /**************** Main loop *****************/
  while (1) {
    XNextEvent(theDisp, &event);
    HandleEvent(&event);
  }
}



/************************************************/
static void HandleEvent(event)
     XEvent *event;
{
  switch (event->type) {
  case Expose: {
    XExposeEvent *exp_event = (XExposeEvent *) event;

    if (exp_event->window==mainW)
      DrawWindow(exp_event->x,exp_event->y,
		 exp_event->width, exp_event->height);
  }
    break;

  case ButtonPress: {
    XButtonEvent *but_event = (XButtonEvent *) event;

    if (but_event->window == mainW && but_event->button == Button1)
      TrackMouse(but_event->x, but_event->y);
  }
    break;

  case KeyPress: {
    XKeyEvent *key_event = (XKeyEvent *) event;
    KeySym ks;
    XComposeStatus status;

    XLookupString(key_event,tmpstr,128,&ks,&status);
    if (tmpstr[0]=='q' || tmpstr[0]=='Q') Quit();
  }
    break;

  case ConfigureNotify: {
    XConfigureEvent *conf_event = (XConfigureEvent *) event;
    int w = conf_event->width, h = conf_event->height;

    if (conf_event->window == mainW && (w != WIDE || h != HIGH))
      Resize((int)(w ? w : WIDE), (int)(h ? h : HIGH));
  }
    break;


  case CirculateNotify:
  case MapNotify:
  case DestroyNotify:
  case GravityNotify:
  case ReparentNotify:
  case UnmapNotify:
  case MappingNotify:
  case ClientMessage:         break;

  default:		/* ignore unexpected events */
    break;
  }  /* end of switch */
}


/***********************************/
static void Syntax()
{
  printf("Usage: %s filename [=geom | -geometry geom] [ [-display] disp]\n",
	 cmd);
  exit(1);
}


/***********************************/
static void FatalError(identifier)
     const char *identifier;
{
  fprintf(stderr, "%s: %s\n", cmd, identifier);
  exit(-1);
}


/***********************************/
static void Quit()
{
  exit(0);
}


/***********************************/
static void CreateMainWindow(name,geom,argc,argv)
     char *name,*geom,**argv;
     int   argc;
{
  XSetWindowAttributes xswa;
  unsigned long xswamask;
  XSizeHints hints;
  int i,x,y;
  unsigned int w,h;

  WIDE = HIGH = 256;			/* default window size */

  x=y=w=h=1;
  hints.flags = 0;

  i=XParseGeometry(geom,&x,&y,&w,&h);
  if (i&WidthValue)
  {
    WIDE = (int) w;
    hints.flags |= USSize;
  }
  if (i&HeightValue)
  {
    HIGH = (int) h;
    hints.flags |= USSize;
  }

  if (i&XValue || i&YValue)
  {
    if (i&XNegative)
      x = XDisplayWidth(theDisp,theScreen)-WIDE-abs(x);
    if (i&YNegative)
      y = XDisplayHeight(theDisp,theScreen)-HIGH-abs(y);
    hints.flags |= USPosition;
  }

  hints.x=x;             hints.y=y;
  hints.width  = WIDE;   hints.height = HIGH;
  hints.max_width  = DisplayWidth(theDisp,theScreen);
  hints.max_height = DisplayHeight(theDisp,theScreen);
  hints.min_width  = 16;
  hints.min_height = 16;
  hints.width_inc = hints.height_inc = 16;
  hints.flags |= PMaxSize | PMinSize | PResizeInc;

  xswa.background_pixel = bcol;
  xswa.border_pixel     = fcol;
  xswa.cursor = XCreateFontCursor (theDisp, XC_top_left_arrow);
  xswamask = CWBackPixel | CWBorderPixel | CWCursor;

  mainW = XCreateWindow(theDisp,rootW,x,y,(unsigned int) WIDE,
			(unsigned int) HIGH, 2, 0,
			(unsigned int) CopyFromParent,
			CopyFromParent, xswamask, &xswa);

  XSetStandardProperties(theDisp,mainW,"xcmap","xcmap",None,
			 argv,argc,&hints);

  if (!mainW) FatalError("Can't open main window");

}


/***********************************/
static void DrawWindow(x,y,w,h)
     int x,y,w,h;
{
  int i,j,x1,y1,x2,y2;

  x1 = x / cWIDE;      y1 = y / cHIGH;	/* (x1,y1) (x2,y2): bounding */
  x2 = ((x+w) + cWIDE - 1) / cWIDE;		/*       rect in cell coords */
  y2 = ((y+h) + cHIGH - 1) / cHIGH;

  for (i=y1; i<y2; i++) {
    for (j=x1; j<x2; j++) {
      XSetForeground(theDisp,theGC,(unsigned long) (i*nycells+j) );
      XFillRectangle(theDisp,mainW,theGC,j*cWIDE,i*cHIGH,
		     (unsigned int) cWIDE, (unsigned int) cHIGH);
    }
  }
}


/***********************************/
static void Resize(w,h)
     int w,h;
{
  cWIDE = (w + nxcells - 1) / nxcells;
  cHIGH = (h + nycells - 1) / nycells;
  WIDE = w;  HIGH = h;
}


/***********************************/
static void TrackMouse(mx,my)
     int mx,my;
{
  /* called when there's a button press in the window.  draws the pixel
     value, and loops until button is released */

  Window        rootW,childW;
  int           rx,ry,x,y;
  unsigned int  mask;

  pvalup = 0;
  DrawPixValue(mx,my);

  while (1) {
    if (XQueryPointer(theDisp,mainW,&rootW,&childW,&rx,&ry,&x,&y,&mask)) {
      if (!(mask & Button1Mask)) break;    /* button released */

      DrawPixValue(x,y);
    }
  }
}


/***********************************/
static void DrawPixValue(x,y)
     int x,y;
{
  static unsigned long pix, lastpix;
  static int           pvaly;

  if (x<0) x=0;  if (x>=WIDE) x=WIDE-1;
  if (y<0) y=0;  if (y>=HIGH) y=HIGH-1;

  if (!pvalup) {	/* it's not up.  make it so */
    if (y >= HIGH/2) pvaly = 0;  else pvaly = HIGH - 12;
    pvalup = 1;
    lastpix = 0xffff;		/* kludge to force redraw on first */
    XClearArea(theDisp,mainW,0,pvaly,
	       (unsigned int) WIDE, (unsigned int) 13,True);
  }

  x /= cWIDE;  y /= cHIGH;

  pix = y * nxcells + x;

  if (pix != lastpix) {
    XColor def;
    char  *sp;

    XSetForeground(theDisp,theGC,fcol);
    lastpix = def.pixel = pix;
    if (pix<dispcells) {
      XQueryColor(theDisp, theCmap, &def);
      sprintf(tmpstr, "Pix %3ld = ($%04x, $%04x, $%04x)",
	      pix, def.red, def.green, def.blue);

      /* make the hex uppercase */
      for (sp=tmpstr+4; *sp; sp++)
	if (islower(*sp)) *sp = toupper(*sp);
    }
    else {
      sprintf(tmpstr, "Pix %3ld is out of legal range. ", pix);
    }

    XDrawImageString(theDisp,mainW,theGC,5,pvaly+10,tmpstr,
		     (int) strlen(tmpstr));
  }
}








